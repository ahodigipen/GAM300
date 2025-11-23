#include "Core.h"
#include "Scripting/ScriptBinding.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>

#include "ECS/ECS.hpp"
#include "Application/Interface.h"
#include "Auxiliaries/Assets.h"
#include <glm/vec3.hpp>
#include <GLFW/glfw3.h>

#include "AppWindow.h"
#include "Input/InputHandler.h"

#include "Application/Application.h"
#include "Audio/Audio.hpp"  // Add this include for sound engine

namespace Boom {

    static AppContext* s_Ctx = nullptr;

    static entt::entity FindEntityByName(const std::string& name) {
        auto& reg = s_Ctx->scene;
        auto view = reg.view<InfoComponent>();
        for (auto e : view) {
            if (view.get<InfoComponent>(e).name == name) return e;
        }
        return entt::null;
    }

    static void ICALL_API_Log(MonoString* msg) {
        if (!msg) return;
        char* s = mono_string_to_utf8(msg);
        if (s) { BOOM_INFO("[C#] {}", s); mono_free(s); }
    }

    static uint64_t ICALL_API_FindEntity(MonoString* name) {
        if (!name) return 0ull;
        char* s = mono_string_to_utf8(name);
        if (!s) return 0ull;
        entt::entity e = FindEntityByName(s);
        mono_free(s);
        if (e == entt::null) return 0ull;
        return static_cast<uint64_t>(static_cast<uint32_t>(e));
    }

    // NEW: Check if entity has TransformComponent
    static bool ICALL_API_HasTransform(uint64_t handle) {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null) return false;
        return s_Ctx->scene.any_of<TransformComponent>(e);
    }

    // NEW: Check if entity has ScriptComponent
    static bool ICALL_API_HasScript(uint64_t handle) {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null) return false;
        return s_Ctx->scene.any_of<ScriptComponent>(e);
    }

    static glm::vec3* ICALL_API_GetPosition(uint64_t handle, glm::vec3* outPos) {
        if (!s_Ctx) return nullptr;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        // Validate entity and component existence
        if (e == entt::null) {
            BOOM_WARN("[ScriptBinding] GetPosition: Invalid entity handle");
            return nullptr;
        }

        if (!s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] GetPosition: Entity no longer valid");
            return nullptr;
        }

        if (!s_Ctx->scene.any_of<TransformComponent>(e)) {
            BOOM_WARN("[ScriptBinding] GetPosition: Entity {} has no TransformComponent", static_cast<uint32_t>(e));
            return nullptr;
        }

        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;
        if (outPos) *outPos = t.translate;
        return outPos;
    }

    static void ICALL_API_SetPosition(uint64_t handle, glm::vec3* pos) {
        if (!pos || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        // Validate entity and component existence
        if (e == entt::null) {
            BOOM_WARN("[ScriptBinding] SetPosition: Invalid entity handle");
            return;
        }

        if (!s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] SetPosition: Entity no longer valid");
            return;
        }

        if (!s_Ctx->scene.any_of<TransformComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetPosition: Entity {} has no TransformComponent", static_cast<uint32_t>(e));
            return;
        }

        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;
        t.translate = *pos;
    }

    static glm::vec3* ICALL_API_GetRotation(uint64_t handle, glm::vec3* outRot) {
        if (!s_Ctx) return nullptr;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (e == entt::null) {
            BOOM_WARN("[ScriptBinding] GetRotation: Invalid entity handle");
            return nullptr;
        }

        if (!s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] GetRotation: Entity no longer valid");
            return nullptr;
        }

        if (!s_Ctx->scene.any_of<TransformComponent>(e)) {
            BOOM_WARN("[ScriptBinding] GetRotation: Entity {} has no TransformComponent", static_cast<uint32_t>(e));
            return nullptr;
        }

        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;
        if (outRot) *outRot = t.rotate;
        return outRot;
    }

    static void ICALL_API_SetRotation(uint64_t handle, glm::vec3* rot) {
        if (!rot || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (e == entt::null) {
            BOOM_WARN("[ScriptBinding] SetRotation: Invalid entity handle");
            return;
        }

        if (!s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] SetRotation: Entity no longer valid");
            return;
        }

        if (!s_Ctx->scene.any_of<TransformComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetRotation: Entity {} has no TransformComponent", static_cast<uint32_t>(e));
            return;
        }

        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;
        t.rotate = *rot;
    }

    static bool ICALL_API_IsKeyDown(int key)
    {
        if (!s_Ctx || !s_Ctx->window) return false;
        return s_Ctx->window->input.keyDown(key);
    }

    static bool ICALL_API_IsMouseDown(int button) {
        if (!s_Ctx || !s_Ctx->window) return false;
        return s_Ctx->window->input.mouseDown(button);
    }

    
    static glm::vec3* ICALL_API_GetLinearVelocity(uint64_t handle, glm::vec3* outVel)
    {
        if (!s_Ctx || !outVel)
            return nullptr;

        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e))
        {
            BOOM_WARN("[ScriptBinding] GetLinearVelocity: Invalid or dead entity");
            return nullptr;
        }

        if (!s_Ctx->scene.any_of<RigidBodyComponent>(e))
        {
            BOOM_WARN("[ScriptBinding] GetLinearVelocity: Entity {} has no RigidBodyComponent",
                static_cast<uint32_t>(e));
            return nullptr;
        }

        auto& rb = s_Ctx->scene.get<RigidBodyComponent>(e).RigidBody;

        glm::vec3 v(0.0f);
        if (rb.actor)
        {
            // PhysX: query velocity from PxRigidDynamic
            if (PxRigidDynamic* dyn = rb.actor->is<PxRigidDynamic>())
            {
                PxVec3 pv = dyn->getLinearVelocity();
                v = glm::vec3(pv.x, pv.y, pv.z);
            }
        }

        *outVel = v;
        return outVel;
    }

    static void ICALL_API_SetLinearVelocity(uint64_t handle, glm::vec3* vel)
    {
        if (!s_Ctx || !vel)
            return;

        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e))
        {
            BOOM_WARN("[ScriptBinding] SetLinearVelocity: Invalid or dead entity");
            return;
        }

        if (!s_Ctx->scene.any_of<RigidBodyComponent>(e))
        {
            BOOM_WARN("[ScriptBinding] SetLinearVelocity: Entity {} has no RigidBodyComponent",
                static_cast<uint32_t>(e));
            return;
        }

        auto& rb = s_Ctx->scene.get<RigidBodyComponent>(e).RigidBody;

        if (rb.actor)
        {
            if (PxRigidDynamic* dyn = rb.actor->is<PxRigidDynamic>())
            {
                dyn->setLinearVelocity(PxVec3(vel->x, vel->y, vel->z));
            }
        }
    }

    static bool ICALL_API_IsColliding(uint64_t handle)
    {
        if (!s_Ctx)
            return false;

        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e))
        {
            // Don�t spam too hard here, this might be called every frame
            // BOOM_WARN("[ScriptBinding] IsColliding: Invalid or dead entity");
            return false;
        }

        if (!s_Ctx->scene.any_of<RigidBodyComponent>(e))
        {
            // No rigidbody => we treat as not grounded
            return false;
        }

        auto& rb = s_Ctx->scene.get<RigidBodyComponent>(e).RigidBody;
        return rb.isColliding;
    }

    static void ICALL_API_LoadScene(MonoString* sceneName) {
        if (!sceneName || !s_Ctx || !s_Ctx->app) return;
        char* s = mono_string_to_utf8(sceneName);
        if (s) {
            std::string sceneNameStr(s);
            if (sceneNameStr == "PauseMenu") {
                std::string currentPath = s_Ctx->app->GetCurrentScenePath();
                s_Ctx->app->SetPreviousScenePath(currentPath);
            }

            s_Ctx->app->LoadScene(sceneNameStr);
            mono_free(s);
        }
    }

    static MonoString* ICALL_API_GetCurrentSceneName() {
        if (!s_Ctx || !s_Ctx->app) return mono_string_new(mono_domain_get(), "");

        std::string path = s_Ctx->app->GetCurrentScenePath();
        if (path.empty()) {
            return mono_string_new(mono_domain_get(), "");
        }

        std::filesystem::path p(path);
        std::string sceneName = p.stem().string();
        return mono_string_new(mono_domain_get(), sceneName.c_str());
    }

    static void ICALL_API_QuitGame() {
        if (s_Ctx && s_Ctx->app) {
            s_Ctx->app->Stop();
        }
    }

    static void ICALL_API_LoadSceneAdditive(MonoString* sceneName) {
        if (!sceneName || !s_Ctx || !s_Ctx->app) return;
        char* s = mono_string_to_utf8(sceneName);
        if (s) {
            s_Ctx->app->LoadSceneAdditive(s);
            mono_free(s);
        }
    }

    static void ICALL_API_UnloadPauseMenu() {
        if (!s_Ctx || !s_Ctx->app) return;
        s_Ctx->app->UnloadAdditiveScene<PauseMenuTagComponent>();
    }

    static void ICALL_API_TogglePause() {
        if (s_Ctx && s_Ctx->app) {
            s_Ctx->app->TogglePause();
        }
    }

    static int ICALL_API_GetApplicationState() {
        if (!s_Ctx || !s_Ctx->app) return (int)ApplicationState::STOPPED;
        return (int)s_Ctx->app->GetState();
    }

    static bool ICALL_API_IsPauseMenuLoaded() {
        if (!s_Ctx) return false;
        // Check if any entity in the scene has the pause menu tag
        auto view = s_Ctx->scene.view<PauseMenuTagComponent>();
        return !view.empty();
    }

    static float ICALL_API_GetThirdPersonCameraYaw() {
        if (!s_Ctx) return 0.0f;

        // Find the active third-person camera
        auto& registry = s_Ctx->scene;
        auto view = registry.view<ThirdPersonCameraComponent>();

        for (auto entity : view) {
            auto& cam = view.get<ThirdPersonCameraComponent>(entity);
            // For now, return the first third-person camera found
            // You might want to add logic to find the "active" one
            return cam.currentYaw;
        }

        return 0.0f; // No third-person camera found
    }

    // Add these new functions after the existing ICALL functions

    static bool ICALL_API_HasCollider(uint64_t handle) {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e)) return false;
        return s_Ctx->scene.any_of<ColliderComponent>(e);
    }

    static bool ICALL_API_IsTrigger(uint64_t handle) {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e)) return false;

        if (!s_Ctx->scene.any_of<ColliderComponent>(e)) return false;

        auto& collider = s_Ctx->scene.get<ColliderComponent>(e);
        return collider.Collider.isTrigger;
    }

    static void ICALL_API_SetTrigger(uint64_t handle, bool isTrigger) {
        if (!s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e)) return;

        if (!s_Ctx->scene.any_of<ColliderComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetTrigger: Entity {} has no ColliderComponent", static_cast<uint32_t>(e));
            return;
        }

        auto& collider = s_Ctx->scene.get<ColliderComponent>(e);
        collider.Collider.isTrigger = isTrigger;

        // Update the physics shape flags if the actor exists
        if (collider.Collider.Shape) {
            if (isTrigger) {
                collider.Collider.Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                collider.Collider.Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
            }
            else {
                collider.Collider.Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                collider.Collider.Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
            }
        }
    }

    // Global trigger callback storage
    static std::unordered_map<uint64_t, std::function<void(uint64_t, uint64_t)>> s_TriggerEnterCallbacks;
    static std::unordered_map<uint64_t, std::function<void(uint64_t, uint64_t)>> s_TriggerExitCallbacks;

    // Function pointer types for C# callbacks
    typedef void (*TriggerCallback)(uint64_t triggerEntity, uint64_t otherEntity);

    // REMOVE THESE DUPLICATE FUNCTIONS (lines ~376-387):
    /*
    static void ICALL_API_RegisterTriggerEnterCallback(uint64_t triggerHandle, TriggerCallback callback) {
        if (!callback) return;
        s_TriggerEnterCallbacks[triggerHandle] = [callback](uint64_t trigger, uint64_t other) {
            callback(trigger, other);
            };
    }

    static void ICALL_API_RegisterTriggerExitCallback(uint64_t triggerHandle, TriggerCallback callback) {
        if (!callback) return;
        s_TriggerExitCallbacks[triggerHandle] = [callback](uint64_t trigger, uint64_t other) {
            callback(trigger, other);
            };
    }
    */

    static void ICALL_API_UnregisterTriggerCallbacks(uint64_t triggerHandle) {
        s_TriggerEnterCallbacks.erase(triggerHandle);
        s_TriggerExitCallbacks.erase(triggerHandle);
    }

    // Function to call trigger callbacks (called from Application.h)
    void CallTriggerEnterCallbacks(uint64_t triggerEntity, uint64_t otherEntity) {
        // Add comprehensive safety checks
        if (!s_Ctx) {
            BOOM_WARN("[ScriptBinding] CallTriggerEnterCallbacks: No script context");
            return;
        }

        // Validate entities still exist
        entt::entity trigger = static_cast<entt::entity>(static_cast<uint32_t>(triggerEntity));
        entt::entity other = static_cast<entt::entity>(static_cast<uint32_t>(otherEntity));

        if (!s_Ctx->scene.valid(trigger) || !s_Ctx->scene.valid(other)) {
            BOOM_WARN("[ScriptBinding] CallTriggerEnterCallbacks: Invalid entities {} or {}",
                static_cast<uint32_t>(trigger), static_cast<uint32_t>(other));
            return;
        }

        auto it = s_TriggerEnterCallbacks.find(triggerEntity);
        if (it != s_TriggerEnterCallbacks.end()) {
            try {
                // Additional safety: check if the function is valid before calling
                if (it->second) {
                    it->second(triggerEntity, otherEntity);
                    BOOM_INFO("[ScriptBinding] Successfully called trigger enter callback for trigger {} and entity {}",
                        triggerEntity, otherEntity);
                }
                else {
                    BOOM_WARN("[ScriptBinding] Trigger enter callback for {} is null", triggerEntity);
                    s_TriggerEnterCallbacks.erase(it); // Remove invalid callback
                }
            }
            catch (const std::exception& e) {
                BOOM_ERROR("[ScriptBinding] Exception in trigger enter callback: {}", e.what());
                s_TriggerEnterCallbacks.erase(it); // Remove problematic callback
            }
            catch (...) {
                BOOM_ERROR("[ScriptBinding] Unknown exception in trigger enter callback for trigger {}", triggerEntity);
                s_TriggerEnterCallbacks.erase(it); // Remove problematic callback
            }
        }
        else {
            BOOM_INFO("[ScriptBinding] No trigger enter callback registered for entity {}", triggerEntity);
        }
    }

    void CallTriggerExitCallbacks(uint64_t triggerEntity, uint64_t otherEntity) {
        // Add comprehensive safety checks
        if (!s_Ctx) {
            BOOM_WARN("[ScriptBinding] CallTriggerExitCallbacks: No script context");
            return;
        }

        // Validate entities still exist
        entt::entity trigger = static_cast<entt::entity>(static_cast<uint32_t>(triggerEntity));
        entt::entity other = static_cast<entt::entity>(static_cast<uint32_t>(otherEntity));

        if (!s_Ctx->scene.valid(trigger) || !s_Ctx->scene.valid(other)) {
            BOOM_WARN("[ScriptBinding] CallTriggerExitCallbacks: Invalid entities {} or {}",
                static_cast<uint32_t>(trigger), static_cast<uint32_t>(other));
            return;
        }

        auto it = s_TriggerExitCallbacks.find(triggerEntity);
        if (it != s_TriggerExitCallbacks.end()) {
            try {
                // Additional safety: check if the function is valid before calling
                if (it->second) {
                    it->second(triggerEntity, otherEntity);
                    BOOM_INFO("[ScriptBinding] Successfully called trigger exit callback for trigger {} and entity {}",
                        triggerEntity, otherEntity);
                }
                else {
                    BOOM_WARN("[ScriptBinding] Trigger exit callback for {} is null", triggerEntity);
                    s_TriggerExitCallbacks.erase(it); // Remove invalid callback
                }
            }
            catch (const std::exception& e) {
                BOOM_ERROR("[ScriptBinding] Exception in trigger exit callback: {}", e.what());
                s_TriggerExitCallbacks.erase(it); // Remove problematic callback
            }
            catch (...) {
                BOOM_ERROR("[ScriptBinding] Unknown exception in trigger exit callback for trigger {}", triggerEntity);
                s_TriggerExitCallbacks.erase(it); // Remove problematic callback
            }
        }
        else {
            BOOM_INFO("[ScriptBinding] No trigger exit callback registered for entity {}", triggerEntity);
        }
    }

    // KEEP ONLY THESE ENHANCED VERSIONS:
    static void ICALL_API_RegisterTriggerEnterCallback(uint64_t triggerHandle, TriggerCallback callback) {
        // Add comprehensive safety checks
        if (!callback) {
            BOOM_WARN("[ScriptBinding] RegisterTriggerEnterCallback: Null callback provided");
            return;
        }

        if (!s_Ctx) {
            BOOM_WARN("[ScriptBinding] RegisterTriggerEnterCallback: No script context");
            return;
        }

        // Validate the trigger entity exists
        entt::entity trigger = static_cast<entt::entity>(static_cast<uint32_t>(triggerHandle));
        if (!s_Ctx->scene.valid(trigger)) {
            BOOM_WARN("[ScriptBinding] RegisterTriggerEnterCallback: Invalid trigger entity {}", triggerHandle);
            return;
        }

        // CRITICAL FIX: Store the callback with additional validation
        s_TriggerEnterCallbacks[triggerHandle] = [callback, triggerHandle](uint64_t trigger, uint64_t other) {
            // Additional runtime validation before calling the callback
            if (!s_Ctx) {
                BOOM_ERROR("[ScriptBinding] Callback invoked but no script context");
                return;
            }

            try {
                // Verify entities still exist before calling
                entt::entity triggerEntity = static_cast<entt::entity>(static_cast<uint32_t>(trigger));
                entt::entity otherEntity = static_cast<entt::entity>(static_cast<uint32_t>(other));

                if (!s_Ctx->scene.valid(triggerEntity) || !s_Ctx->scene.valid(otherEntity)) {
                    BOOM_WARN("[ScriptBinding] Callback entities became invalid");
                    return;
                }

                // Call the actual C# callback
                callback(trigger, other);
            }
            catch (...) {
                BOOM_ERROR("[ScriptBinding] Exception in callback wrapper for trigger {}", triggerHandle);
                // Remove the problematic callback
                s_TriggerEnterCallbacks.erase(triggerHandle);
            }
            };

        BOOM_INFO("[ScriptBinding] Registered trigger enter callback for entity {}", triggerHandle);
    }

    static void ICALL_API_RegisterTriggerExitCallback(uint64_t triggerHandle, TriggerCallback callback) {
        // Add comprehensive safety checks
        if (!callback) {
            BOOM_WARN("[ScriptBinding] RegisterTriggerExitCallback: Null callback provided");
            return;
        }

        if (!s_Ctx) {
            BOOM_WARN("[ScriptBinding] RegisterTriggerExitCallback: No script context");
            return;
        }

        // Validate the trigger entity exists
        entt::entity trigger = static_cast<entt::entity>(static_cast<uint32_t>(triggerHandle));
        if (!s_Ctx->scene.valid(trigger)) {
            BOOM_WARN("[ScriptBinding] RegisterTriggerExitCallback: Invalid trigger entity {}", triggerHandle);
            return;
        }

        // CRITICAL FIX: Store the callback with additional validation
        s_TriggerExitCallbacks[triggerHandle] = [callback, triggerHandle](uint64_t trigger, uint64_t other) {
            // Additional runtime validation before calling the callback
            if (!s_Ctx) {
                BOOM_ERROR("[ScriptBinding] Callback invoked but no script context");
                return;
            }

            try {
                // Verify entities still exist before calling
                entt::entity triggerEntity = static_cast<entt::entity>(static_cast<uint32_t>(trigger));
                entt::entity otherEntity = static_cast<entt::entity>(static_cast<uint32_t>(other));

                if (!s_Ctx->scene.valid(triggerEntity) || !s_Ctx->scene.valid(otherEntity)) {
                    BOOM_WARN("[ScriptBinding] Callback entities became invalid");
                    return;
                }

                // Call the actual C# callback
                callback(trigger, other);
            }
            catch (...) {
                BOOM_ERROR("[ScriptBinding] Exception in callback wrapper for trigger {}", triggerHandle);
                // Remove the problematic callback
                s_TriggerExitCallbacks.erase(triggerHandle);
            }
            };

        BOOM_INFO("[ScriptBinding] Registered trigger exit callback for entity {}", triggerHandle);
    }

    // ========= SOUND / AUDIO BINDINGS =========
    
    static void ICALL_API_PlaySound(MonoString* name, MonoString* filePath, bool loop) {
        if (!name || !filePath) return;
        
        char* nameStr = mono_string_to_utf8(name);
        char* pathStr = mono_string_to_utf8(filePath);
        
        if (nameStr && pathStr) {
            auto& soundEngine = SoundEngine::Instance();
            soundEngine.PlaySound(std::string(nameStr), std::string(pathStr), loop);
        }
        
        if (nameStr) mono_free(nameStr);
        if (pathStr) mono_free(pathStr);
    }
    
    static void ICALL_API_PlaySoundAt(MonoString* name, MonoString* filePath, glm::vec3* position, bool loop) {
        if (!name || !filePath || !position) return;
        
        char* nameStr = mono_string_to_utf8(name);
        char* pathStr = mono_string_to_utf8(filePath);
        
        if (nameStr && pathStr) {
            auto& soundEngine = SoundEngine::Instance();
            soundEngine.PlaySoundAt(std::string(nameStr), std::string(pathStr), *position, loop);
        }
        
        if (nameStr) mono_free(nameStr);
        if (pathStr) mono_free(pathStr);
    }
    
    static void ICALL_API_StopSound(MonoString* name) {
        if (!name) return;
        
        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& soundEngine = SoundEngine::Instance();
            soundEngine.StopSound(std::string(nameStr));
            mono_free(nameStr);
        }
    }
    
    static void ICALL_API_SetSoundVolume(MonoString* name, float volume) {
        if (!name) return;
        
        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& soundEngine = SoundEngine::Instance();
            soundEngine.SetVolume(std::string(nameStr), volume);
            mono_free(nameStr);
        }
    }
    
    static bool ICALL_API_IsSoundPlaying(MonoString* name) {
        if (!name) return false;
        
        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& soundEngine = SoundEngine::Instance();
            bool result = soundEngine.IsPlaying(std::string(nameStr));
            mono_free(nameStr);
            return result;
        }
        return false;
    }
    
    static void ICALL_API_PauseSound(MonoString* name, bool pause) {
        if (!name) return;
        
        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& soundEngine = SoundEngine::Instance();
            soundEngine.Pause(std::string(nameStr), pause);
            mono_free(nameStr);
        }
    }
    
    static void ICALL_API_PreloadSound(MonoString* name, MonoString* filePath, bool loop) {
        if (!name || !filePath) return;
        
        char* nameStr = mono_string_to_utf8(name);
        char* pathStr = mono_string_to_utf8(filePath);
        
        if (nameStr && pathStr) {
            auto& soundEngine = SoundEngine::Instance();
            soundEngine.PreloadSound(std::string(nameStr), std::string(pathStr), false, loop);
        }
        
        if (nameStr) mono_free(nameStr);
        if (pathStr) mono_free(pathStr);
    }
    
    static void ICALL_API_SetSoundPosition(MonoString* name, glm::vec3* position) {
        if (!name || !position) return;
        
        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& soundEngine = SoundEngine::Instance();
            soundEngine.SetSoundPosition(std::string(nameStr), *position);
            mono_free(nameStr);
        }
    }
    // Add this function to clean up all trigger callbacks when Mono domain is unloaded
    void ClearAllTriggerCallbacks() {
        s_TriggerEnterCallbacks.clear();
        s_TriggerExitCallbacks.clear();
        BOOM_INFO("[ScriptBinding] Cleared all trigger callbacks (domain unload)");
    }

    void RegisterScriptInternalCalls(AppContext* ctx)
    {
        s_Ctx = ctx;

        ClearAllTriggerCallbacks();

        // IMPORTANT: These namespaces MUST match the C# side (Boom.Native)
        mono_add_internal_call("Boom.Native::Boom_API_Log", (const void*)ICALL_API_Log);
        mono_add_internal_call("Boom.Native::Boom_API_FindEntity", (const void*)ICALL_API_FindEntity);
        mono_add_internal_call("Boom.Native::Boom_API_GetPosition", (const void*)ICALL_API_GetPosition);
        mono_add_internal_call("Boom.Native::Boom_API_SetPosition", (const void*)ICALL_API_SetPosition);
        mono_add_internal_call("Boom.Native::Boom_API_GetRotation", (const void*)ICALL_API_GetRotation);
        mono_add_internal_call("Boom.Native::Boom_API_SetRotation", (const void*)ICALL_API_SetRotation);
        mono_add_internal_call("Boom.Native::Boom_API_IsKeyDown", (const void*)ICALL_API_IsKeyDown);
        mono_add_internal_call("Boom.Native::Boom_API_IsMouseDown", (const void*)ICALL_API_IsMouseDown);

        mono_add_internal_call("Boom.Native::Boom_API_LoadScene", (const void*)ICALL_API_LoadScene);
        mono_add_internal_call("Boom.Native::Boom_API_GetCurrentSceneName", (const void*)ICALL_API_GetCurrentSceneName);
        mono_add_internal_call("Boom.Native::Boom_API_QuitGame", (const void*)ICALL_API_QuitGame);
        mono_add_internal_call("Boom.Native::Boom_API_LoadSceneAdditive", (const void*)ICALL_API_LoadSceneAdditive);
        mono_add_internal_call("Boom.Native::Boom_API_UnloadPauseMenu", (const void*)ICALL_API_UnloadPauseMenu);
        mono_add_internal_call("Boom.Native::Boom_API_TogglePause", (const void*)ICALL_API_TogglePause);
        mono_add_internal_call("Boom.Native::Boom_API_GetApplicationState", (const void*)ICALL_API_GetApplicationState);
        mono_add_internal_call("Boom.Native::Boom_API_IsPauseMenuLoaded", (const void*)ICALL_API_IsPauseMenuLoaded);
        // Component checking functions
        mono_add_internal_call("Boom.Native::Boom_API_HasTransform", (const void*)ICALL_API_HasTransform);
        mono_add_internal_call("Boom.Native::Boom_API_HasScript", (const void*)ICALL_API_HasScript);

        // Physics functions 
        mono_add_internal_call("Boom.Native::Boom_API_GetLinearVelocity",
            (const void*)ICALL_API_GetLinearVelocity);

        mono_add_internal_call("Boom.Native::Boom_API_SetLinearVelocity",
            (const void*)ICALL_API_SetLinearVelocity);

        mono_add_internal_call("Boom.Native::Boom_API_IsColliding",
            (const void*)ICALL_API_IsColliding);

        mono_add_internal_call("Boom.Native::Boom_API_GetThirdPersonCameraYaw", (const void*)ICALL_API_GetThirdPersonCameraYaw);

        mono_add_internal_call("Boom.Native::Boom_API_HasCollider", (const void*)ICALL_API_HasCollider);
        mono_add_internal_call("Boom.Native::Boom_API_IsTrigger", (const void*)ICALL_API_IsTrigger);
        mono_add_internal_call("Boom.Native::Boom_API_SetTrigger", (const void*)ICALL_API_SetTrigger);
        mono_add_internal_call("Boom.Native::Boom_API_RegisterTriggerEnterCallback", (const void*)ICALL_API_RegisterTriggerEnterCallback);
        mono_add_internal_call("Boom.Native::Boom_API_RegisterTriggerExitCallback", (const void*)ICALL_API_RegisterTriggerExitCallback);
        mono_add_internal_call("Boom.Native::Boom_API_UnregisterTriggerCallbacks", (const void*)ICALL_API_UnregisterTriggerCallbacks);

        // Sound/Audio internal calls
        mono_add_internal_call("Boom.Native::Boom_API_PlaySound", (const void*)ICALL_API_PlaySound);
        mono_add_internal_call("Boom.Native::Boom_API_PlaySoundAt", (const void*)ICALL_API_PlaySoundAt);
        mono_add_internal_call("Boom.Native::Boom_API_StopSound", (const void*)ICALL_API_StopSound);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundVolume", (const void*)ICALL_API_SetSoundVolume);
        mono_add_internal_call("Boom.Native::Boom_API_IsSoundPlaying", (const void*)ICALL_API_IsSoundPlaying);
        mono_add_internal_call("Boom.Native::Boom_API_PauseSound", (const void*)ICALL_API_PauseSound);
        mono_add_internal_call("Boom.Native::Boom_API_PreloadSound", (const void*)ICALL_API_PreloadSound);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundPosition", (const void*)ICALL_API_SetSoundPosition);
    }
}