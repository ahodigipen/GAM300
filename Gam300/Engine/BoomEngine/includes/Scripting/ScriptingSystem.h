#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "Scripting/MonoRuntime.h"
#include "Scripting/ScriptBinding.h"
#include "ECS/ECS.hpp"
#include "FileWatcher.h"

namespace Boom {
    // Forward declarations only - no includes that might cause circular deps
    struct AppContext;
    struct ScriptComponent;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#pragma warning(disable:4275)
#endif

    class BOOM_API ScriptingSystem {
    public:
        // call from Editor on startup with the folder that contains GameScripts.dll
        bool Init(const std::string& scriptsDir, AppContext* ctx);
        void Shutdown();

        bool LoadScriptsDll(const std::string& dllPath);
        bool CallStart();               // calls GameScripts.Entry:Start()
        bool CallSessionStart();        // calls GameScripts.Entry:OnSessionStart()
        bool CallUpdate(float dt);      // calls GameScripts.Entry:Update(float)

        // ---- Runtime instance metadata for a single C# object ----
        struct Instance {
            uint64_t    gchandle = 0;
            MonoMethod* onStart = nullptr;
            MonoMethod* onUpdate = nullptr;
            MonoMethod* onDestroy = nullptr;
        };

        // ---- Per-entity lifecycle (used by Inspector) ----
        bool RecreateForEntity(entt::entity e, ScriptComponent& sc);
        void DestroyForEntity(entt::entity e, ScriptComponent& sc);
        bool TickEntity(entt::entity e, ScriptComponent& sc, float dt);

        // ---- Whole-DLL hot reload (Inspector calls this) ----
        bool ReloadScripts();
        bool IsAlive() const { return m_Alive; }
        bool IsReloading() const { return m_Reloading; }

        // Hot reload control
        void EnableAutoHotReload(bool enable);
        bool IsAutoHotReloadEnabled() const { return m_AutoHotReload; }
        void UpdateFileWatcher();

        // Get available script types for dropdown
        std::vector<std::string> GetAvailableScriptTypes() const;

        // ---- Editor-exposed field support ----
        struct ExposedFieldInfo {
            std::string fieldName;
            std::string displayName;
            std::string typeName;
            std::string tooltip;
            float minValue = -FLT_MAX;
            float maxValue = FLT_MAX;
            bool useSlider = false;
            std::vector<std::string> options; // Non-empty = Dropdown list
        };

        // Get exposed fields for a script type (calls C# ScriptRegistry)
        std::vector<ExposedFieldInfo> GetExposedFields(const std::string& typeName) const;

        // Get/set field values on a live script instance
        std::string GetFieldValue(uint64_t instanceId, const std::string& fieldName) const;
        bool SetFieldValue(uint64_t instanceId, const std::string& fieldName, const std::string& valueJson);

        // Get the GC handle for an instance (for direct Mono operations if needed)
        uint64_t GetInstanceGCHandle(uint64_t instanceId) const;

    private:
        MonoRuntime    m_Mono;
        MonoAssembly* m_Scripts = nullptr;
        std::string    m_ScriptsDir;
        AppContext* m_Ctx = nullptr;  // Remove Boom:: prefix - already in namespace
        bool           m_Alive = false;
        bool           m_Reloading = false;  
        FileWatcher m_FileWatcher;
        bool m_AutoHotReload = true;
        std::string m_DllPath;


        bool CreateInstance(const std::string& typeName,
            const nlohmann::json& params,
            uint64_t entityHandle,
            Instance& out);

        std::unordered_map<uint64_t, Instance> m_Instances;
        uint64_t    m_NextId = 1;      // to mint InstanceId values

        std::unique_ptr<FileWatcher> m_ScriptFileWatcher;
        std::string m_ScriptsDirectory = "GameScripts/"; 
        bool m_needsRecompile = false;

    };

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace Boom