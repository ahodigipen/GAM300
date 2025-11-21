#include "Core.h"
#include "Scripting/ScriptingSystem.h"

namespace Boom {

    bool ScriptingSystem::Init(const std::string& scriptsDir, AppContext* ctx)
    {
        m_Ctx = ctx;
        m_ScriptsDir = scriptsDir;

        if (!m_Mono.Init("BoomDomain", scriptsDir.c_str()))
            return false;

        RegisterScriptInternalCalls(m_Ctx);

#ifdef DEBUG
        BOOM_INFO("[Scripting] Mono ready. {}", m_Mono.RuntimeInfo());
#endif

        m_Alive = true;
        return true;
    }

    void ScriptingSystem::Shutdown()
    {
        if (!m_Alive)
            return;

        BOOM_INFO("[Scripting] Starting shutdown...");

        // 1. Stop file watching
        m_FileWatcher.ClearAll();

        // 2. Mark as NOT alive first to prevent any new invocations
        m_Alive = false;

        // 3. Destroy all instances (safe GCHandle cleanup)
        for (auto& [id, inst] : m_Instances) {
            if (inst.gchandle) {
                MonoObject* obj = mono_gchandle_get_target(inst.gchandle);
                if (inst.onDestroy && obj) {
                    MonoObject* exc = nullptr;
                    mono_runtime_invoke(inst.onDestroy, obj, nullptr, &exc);
                    if (exc) m_Mono.LogException(exc, "[Scripting] OnDestroy (shutdown)");
                }
                mono_gchandle_free(inst.gchandle);
            }
        }
        m_Instances.clear();
        m_NextId = 1;

        // 4. Clear assembly reference before shutdown
        m_Scripts = nullptr;

        // 5. Shutdown Mono runtime
        m_Mono.Shutdown();

        BOOM_INFO("[Scripting] Shutdown complete");
    }

    bool ScriptingSystem::LoadScriptsDll(const std::string& dllPath)
    {
        if (!m_Alive)
            return false;

        m_Scripts = m_Mono.LoadAssembly(dllPath.c_str());
        if (!m_Scripts) {
            BOOM_ERROR("[Scripting] Failed to load assembly: {}", dllPath);
            return false;
        }

        m_DllPath = dllPath;

        if (m_AutoHotReload) {
            m_FileWatcher.AddWatch(dllPath,
                [this](const std::string& path) {
                    BOOM_INFO("[Scripting] DLL modified, triggering hot reload...");
                    this->ReloadScripts();
                },
                1000
            );
        }

#ifdef DEBUG
        BOOM_INFO("[Scripting] Loaded assembly: {}", dllPath);
        if (m_AutoHotReload) {
            BOOM_INFO("[Scripting] Auto hot-reload ENABLED for: {}", dllPath);
        }
#endif
        return true;
    }

    bool ScriptingSystem::CallStart()
    {
        if (!m_Alive || !m_Scripts)
            return false;

        return m_Mono.InvokeStatic("GameScripts.Entry:Start()");
    }

    bool ScriptingSystem::CallUpdate(float dt)
    {
        if (!m_Alive || !m_Scripts)
            return false;

        void* args[1] = { &dt };
        return m_Mono.InvokeStatic("GameScripts.Entry:Update(single)", args, 1);
    }

    bool ScriptingSystem::CreateInstance(const std::string& typeName,
        const nlohmann::json& params,
        uint64_t entityHandle,
        Instance& out)
    {
        if (!m_Alive) {
            BOOM_ERROR("[Scripting] Cannot create instance - system not alive");
            return false;
        }

        // Resolve class
        MonoClass* klass = m_Mono.FindClassByName(typeName.c_str());
        if (!klass) {
            BOOM_ERROR("[Scripting] Type not found: {}", typeName);
            return false;
        }

        MonoObject* obj = mono_object_new(mono_domain_get(), klass);
        if (!obj) {
            BOOM_ERROR("[Scripting] Failed to create object for type: {}", typeName);
            return false;
        }
        mono_runtime_object_init(obj);

        // Bind methods
        auto find = [&](const char* name, int argc)->MonoMethod* {
            return m_Mono.FindMethod(klass, name, argc);
            };

        MonoMethod* mStart = find("OnStart", 1);
        MonoMethod* mUpdate = find("OnUpdate", 1);
        MonoMethod* mDestroy = find("OnDestroy", 0);

        // Set entity handle field
        if (MonoClassField* f = m_Mono.FindField(klass, "Entity")) {
            uint64_t h = entityHandle;
            mono_field_set_value(obj, f, &h);
        }

        // Call OnStart with params
        if (mStart) {
            std::string js = params.dump();
            MonoString* s = mono_string_new(mono_domain_get(), js.c_str());
            void* a[1] = { s };
            MonoObject* exc = nullptr;
            mono_runtime_invoke(mStart, obj, a, &exc);
            if (exc) {
                m_Mono.LogException(exc, "[Scripting] OnStart");
            }
        }

        // Pin object and store
        out.gchandle = mono_gchandle_new(obj, false);
        out.onStart = mStart;
        out.onUpdate = mUpdate;
        out.onDestroy = mDestroy;

#ifdef DEBUG
        BOOM_INFO("[Scripting] Created instance of type: {}", typeName);
#endif

        return true;
    }

    void ScriptingSystem::DestroyForEntity(entt::entity e, ScriptComponent& sc)
    {
        if (!sc.InstanceId) return;

        auto it = m_Instances.find(sc.InstanceId);
        if (it != m_Instances.end()) {
            Instance& inst = it->second;
            if (inst.gchandle) {
                MonoObject* obj = mono_gchandle_get_target(inst.gchandle);
                if (inst.onDestroy && obj) {
                    MonoObject* exc = nullptr;
                    mono_runtime_invoke(inst.onDestroy, obj, nullptr, &exc);
                    if (exc) m_Mono.LogException(exc, "[Scripting] OnDestroy");
                }
                mono_gchandle_free(inst.gchandle);
            }
            m_Instances.erase(it);
        }
        sc.InstanceId = 0;
    }

    bool ScriptingSystem::RecreateForEntity(entt::entity e, ScriptComponent& sc)
    {
        // Destroy old instance first
        DestroyForEntity(e, sc);

        // If disabled or no type name, just return success (instance stays destroyed)
        if (!sc.Enabled || sc.TypeName.empty()) {
#ifdef DEBUG
            BOOM_INFO("[Scripting] Script disabled or empty for entity {}, skipping recreation",
                static_cast<uint32_t>(e));
#endif
            return true;
        }

        // Create new instance
        Instance inst{};
        if (!CreateInstance(sc.TypeName, sc.Params, (uint64_t)(uint32_t)e, inst)) {
            BOOM_ERROR("[Scripting] Failed to create instance of {} for entity {}",
                sc.TypeName, static_cast<uint32_t>(e));
            return false;
        }

        sc.InstanceId = m_NextId++;
        m_Instances.emplace(sc.InstanceId, std::move(inst));

#ifdef DEBUG
        BOOM_INFO("[Scripting] Recreated instance {} for entity {}",
            sc.InstanceId, static_cast<uint32_t>(e));
#endif

        return true;
    }

    bool ScriptingSystem::TickEntity(entt::entity e, ScriptComponent& sc, float dt)
    {
        if (!m_Alive || !sc.Enabled || !sc.InstanceId)
            return false;

        auto it = m_Instances.find(sc.InstanceId);
        if (it == m_Instances.end()) return false;

        Instance& inst = it->second;
        if (!inst.onUpdate || !inst.gchandle) return false;

        MonoObject* obj = mono_gchandle_get_target(inst.gchandle);
        if (!obj) return false;

        void* a[1] = { &dt };
        MonoObject* exc = nullptr;
        mono_runtime_invoke(inst.onUpdate, obj, a, &exc);
        if (exc) m_Mono.LogException(exc, "[Scripting] OnUpdate");
        return true;
    }

    bool ScriptingSystem::ReloadScripts()
    {
        if (!m_Alive) {
            BOOM_ERROR("[Scripting] Cannot reload - system not alive");
            return false;
        }

        BOOM_INFO("[Scripting] ========== HOT RELOAD START ==========");
        m_Reloading = true;

        // Temporarily disable file watching during reload
        bool wasAutoReloadEnabled = m_AutoHotReload;
        m_FileWatcher.ClearAll();

        // 1. Call OnDestroy on all existing instances
        BOOM_INFO("[Scripting] Destroying {} existing instances...", m_Instances.size());
        for (auto& [id, inst] : m_Instances) {
            if (inst.gchandle) {
                MonoObject* obj = mono_gchandle_get_target(inst.gchandle);
                if (inst.onDestroy && obj) {
                    MonoObject* exc = nullptr;
                    mono_runtime_invoke(inst.onDestroy, obj, nullptr, &exc);
                    if (exc) m_Mono.LogException(exc, "[Scripting] OnDestroy (reload)");
                }
                mono_gchandle_free(inst.gchandle);
            }
        }
        m_Instances.clear();
        m_NextId = 1;

        // 2. Clear assembly reference and function pointers BEFORE unloading domain
        m_Scripts = nullptr;
        BOOM_INFO("[Scripting] Cleared assembly references");

        // 3. Unload old domain
        BOOM_INFO("[Scripting] Unloading old app domain...");
        m_Mono.UnloadDomain();

        // 4. Re-initialize Mono with new domain
        BOOM_INFO("[Scripting] Initializing new app domain...");
        if (!m_Mono.Init("BoomDomain", m_ScriptsDir.c_str())) {
            BOOM_ERROR("[Scripting] Failed to reinitialize Mono runtime");
            m_Alive = false;
            m_Reloading = false;
            return false;
        }

        // 5. Re-register internal calls
        BOOM_INFO("[Scripting] Re-registering internal calls...");
        RegisterScriptInternalCalls(m_Ctx);

        // 6. Reload the DLL (this will also reload the function pointers)
        BOOM_INFO("[Scripting] Loading assembly: {}", m_DllPath);
        if (!LoadScriptsDll(m_DllPath)) {
            BOOM_ERROR("[Scripting] Failed to reload GameScripts.dll");
            m_Reloading = false;
            return false;
        }

        // Re-enable file watching if it was enabled
        if (wasAutoReloadEnabled) {
            m_FileWatcher.AddWatch(m_DllPath,
                [this](const std::string& path) {
                    BOOM_INFO("[Scripting] DLL modified, triggering hot reload...");
                    this->ReloadScripts();
                },
                1000
            );
        }

        // 7. Call Entry.Start() if it exists
        BOOM_INFO("[Scripting] Calling Entry.Start()...");
        if (!CallStart()) {
            BOOM_WARN("[Scripting] Entry.Start() failed or not found (this may be okay)");
        }

        // 8. Recreate instances from ScriptComponents
        auto& registry = m_Ctx->scene;
        auto view = registry.view<ScriptComponent>();
        int recreated = 0, failed = 0;

        BOOM_INFO("[Scripting] Recreating script instances for {} entities...", view.size());
        for (auto entity : view) {
            auto& sc = view.get<ScriptComponent>(entity);
            if (RecreateForEntity(entity, sc)) {
                recreated++;
            }
            else {
                failed++;
                BOOM_ERROR("[Scripting] Failed to recreate instance for entity {}", static_cast<uint32_t>(entity));
            }
        }

        m_Reloading = false;

        BOOM_INFO("[Scripting] Hot reload complete!");
        BOOM_INFO("[Scripting]   - Recreated: {}", recreated);
        BOOM_INFO("[Scripting]   - Failed: {}", failed);
        BOOM_INFO("[Scripting] ========== HOT RELOAD END ==========");

        return (failed == 0);
    }

    void ScriptingSystem::EnableAutoHotReload(bool enable)
    {
        if (m_AutoHotReload == enable) return;

        m_AutoHotReload = enable;

        if (enable && !m_DllPath.empty()) {
            m_FileWatcher.AddWatch(m_DllPath,
                [this](const std::string& path) {
                    BOOM_INFO("[Scripting] DLL modified, triggering hot reload...");
                    this->ReloadScripts();
                },
                1000
            );
            BOOM_INFO("[Scripting] Auto hot-reload ENABLED");
        }
        else {
            m_FileWatcher.ClearAll();
            BOOM_INFO("[Scripting] Auto hot-reload DISABLED");
        }
    }

    void ScriptingSystem::UpdateFileWatcher()
    {
        if (m_AutoHotReload && m_Alive && !m_Reloading) {
            m_FileWatcher.Update();
        }
    }

    std::vector<std::string> ScriptingSystem::GetAvailableScriptTypes() const
    {
        std::vector<std::string> types;

        if (!m_Alive || !m_Scripts)
            return types;

        MonoImage* image = mono_assembly_get_image(m_Scripts);
        if (!image)
            return types;

        // Find GameScripts.ScriptRegistry
        MonoClass* registryClass = mono_class_from_name(image, "GameScripts", "ScriptRegistry");
        if (!registryClass) {
            BOOM_WARN("[Scripting] ScriptRegistry class not found - dropdown will be empty");
            return types;
        }

        // Find static string[] GetAvailableScriptTypes()
        MonoMethod* getTypesMethod =
            mono_class_get_method_from_name(registryClass, "GetAvailableScriptTypes", 0);
        if (!getTypesMethod) {
            BOOM_WARN("[Scripting] GetAvailableScriptTypes() method not found - dropdown will be empty");
            return types;
        }

        MonoObject* exc = nullptr;
        MonoObject* result = mono_runtime_invoke(getTypesMethod, nullptr, nullptr, &exc);
        if (exc) {
            const_cast<MonoRuntime&>(m_Mono).LogException(exc, "[Scripting] GetAvailableScriptTypes");
            return types;
        }

        if (!result)
            return types;

        MonoArray* arr = reinterpret_cast<MonoArray*>(result);
        uint32_t len = mono_array_length(arr);
        types.reserve(len);

        for (uint32_t i = 0; i < len; ++i) {
            MonoString* ms = mono_array_get(arr, MonoString*, i);
            if (!ms) continue;

            char* utf8 = mono_string_to_utf8(ms);
            if (!utf8) continue;

            types.emplace_back(utf8);
            mono_free(utf8);
        }

        return types;
    }


} // namespace Boom