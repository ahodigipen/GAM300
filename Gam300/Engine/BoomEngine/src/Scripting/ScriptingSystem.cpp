#include "Core.h"
#include "Scripting/ScriptingSystem.h"

namespace Boom {

    bool ScriptingSystem::Init(const std::string& scriptsDir, AppContext* ctx)
    {
        m_Ctx = ctx;
        m_ScriptsDir = scriptsDir;
        if (!m_Mono.Init("BoomDomain", scriptsDir.c_str())) return false;

        // IMPORTANT: register internal calls for THIS domain
        RegisterScriptInternalCalls(m_Ctx);

#ifdef DEBUG
        BOOM_INFO("[Scripting] Mono ready. {}", m_Mono.RuntimeInfo());
#endif
        return true;
    }

    void ScriptingSystem::Shutdown()
    {
        m_Scripts = nullptr;
        m_Mono.Shutdown();
    }

    bool ScriptingSystem::LoadScriptsDll(const std::string& dllPath)
    {
        m_Scripts = m_Mono.LoadAssembly(dllPath.c_str());
        return (m_Scripts != nullptr);
    }

    bool ScriptingSystem::CallStart()
    {
        // Fully-qualified static method: Namespace.Type:Method(signature)
        return m_Mono.InvokeStatic("GameScripts.Entry:Start()");
    }

    bool ScriptingSystem::CallUpdate(float dt)
    {
        void* args[1];
        args[0] = &dt; // Mono expects float* for single-precision
        return m_Mono.InvokeStatic("GameScripts.Entry:Update(single)", args, 1);
    }

    bool ScriptingSystem::CreateInstance(const std::string& typeName,
        const nlohmann::json& params,
        uint64_t entityHandle,
        Instance& out)
    {
        // Resolve class (Namespace.Type from sc.TypeName)
        MonoClass* klass = m_Mono.FindClassByName(typeName.c_str()); // implement: search images in m_LoadedAssemblies
        if (!klass) { BOOM_ERROR("[Scripting] Type not found: {}", typeName); return false; }

        MonoObject* obj = mono_object_new(mono_domain_get(), klass);
        if (!obj) return false;
        mono_runtime_object_init(obj); // default ctor

        // Bind methods (optional names; pick your convention)
        auto find = [&](const char* name, int argc)->MonoMethod* {
            return m_Mono.FindMethod(klass, name, argc); // implement: iterate mono_class_get_methods
            };
        MonoMethod* mStart = find("OnStart", 1); // (string json)
        MonoMethod* mUpdate = find("OnUpdate", 1); // (single dt)
        MonoMethod* mDestroy = find("OnDestroy", 0);

        // Provide entity handle into script if you want (e.g., public ulong Entity;)
        if (MonoClassField* f = m_Mono.FindField(klass, "Entity")) {
            uint64_t h = entityHandle;
            mono_field_set_value(obj, f, &h);
        }

        // Optional params injection on construct-time
        if (mStart) {
            std::string js = params.dump();
            MonoString* s = mono_string_new(mono_domain_get(), js.c_str());
            void* a[1] = { s };
            MonoObject* exc = nullptr;
            mono_runtime_invoke(mStart, obj, a, &exc);
            if (exc) { m_Mono.LogException(exc, "[Scripting] OnStart"); /* still keep instance */ }
        }

        // Pin object and store
        out.gchandle = mono_gchandle_new(obj, /*pinned*/ false);
        out.onStart = mStart; out.onUpdate = mUpdate; out.onDestroy = mDestroy;
        return true;
    }

    void ScriptingSystem::DestroyForEntity(entt::entity, ScriptComponent& sc)
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
        // destroy old
        DestroyForEntity(e, sc);
        if (!sc.Enabled) return true; // allowed to be disabled

        Instance inst{};
        if (!CreateInstance(sc.TypeName, sc.Params, (uint64_t)(uint32_t)e, inst))
            return false;

        sc.InstanceId = m_NextId++;
        m_Instances.emplace(sc.InstanceId, std::move(inst));
        return true;
    }

    static inline std::string JoinPath(const std::string& dir, const char* file) {
        if (dir.empty()) return file;
        if (dir.back() == '/' || dir.back() == '\\') return dir + file;
#ifdef _WIN32
        return dir + "\\" + file;
#else
        return dir + "/" + file;
#endif
    }

    bool ScriptingSystem::TickEntity(entt::entity, ScriptComponent& sc, float dt)
    {
        if (!sc.Enabled || !sc.InstanceId) return false;
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
    BOOM_INFO("[Scripting] Starting hot reload...");
    
    // 1. Destroy all existing instances
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
    
    // 2. Unload the old domain
    m_Mono.UnloadDomain();
    
    // 3. Create fresh app domain
    if (!m_Mono.Init("BoomDomain", m_ScriptsDir.c_str())) {
        BOOM_ERROR("[Scripting] Failed to reinitialize Mono runtime");
        return false;
    }
    
    // 4. Re-register internal calls (CRITICAL!)
    RegisterScriptInternalCalls(m_Ctx);
    
    // 5. Reload the DLL
    std::string dllPath = m_ScriptsDir + "/GameScripts.dll";
    if (!LoadScriptsDll(dllPath)) {
        BOOM_ERROR("[Scripting] Failed to reload GameScripts.dll");
        return false;
    }
    
    // 6. Recreate all script component instances
    auto& registry = m_Ctx->scene;
    auto view = registry.view<Boom::ScriptComponent>();
    for (auto entity : view) {
        auto& sc = view.get<Boom::ScriptComponent>(entity);
        if (!RecreateForEntity(entity, sc)) {
            BOOM_ERROR("[Scripting] Failed to recreate instance for entity");
        }
    }
    
    BOOM_INFO("[Scripting] Hot reload complete!");
    return true;
}



} // namespace Boom
