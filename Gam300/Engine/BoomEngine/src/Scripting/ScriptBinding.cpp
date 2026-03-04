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
#include "../includes/Graphics/Models/Animator.h"

#include "AppWindow.h"
#include "Input/InputHandler.h"

#include "Application/Application.h"
#include "Audio/Audio.hpp"  // Add this include for sound engine
#include <glm/gtc/matrix_transform.hpp>

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

    static bool ICALL_API_IsGamepadButtonDown(int button) {
        if (!s_Ctx || !s_Ctx->window) return false;
        return s_Ctx->window->input.gamepadButtonDown(button);
    }

    static float ICALL_API_GetGamepadAxis(int axis) {
        if (!s_Ctx || !s_Ctx->window) return 0.0f;
        return s_Ctx->window->input.gamepadAxis(axis);
    }

    static bool ICALL_API_IsGamepadConnected() {
        if (!s_Ctx || !s_Ctx->window) return false;
        return s_Ctx->window->input.isGamepadConnected();
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
        Entity ent{ &s_Ctx->scene, e };
        if (e == entt::null || !s_Ctx->scene.valid(e))
        {
            BOOM_WARN("[ScriptBinding] SetLinearVelocity: Invalid or dead entity");
            return;
        }



        // If entity uses a character controller, drive it using the PxController API.
        // Move controller by displacement = velocity * deltaTime so scripts can continue
        // to call SetLinearVelocity with a desired velocity vector.
        if (s_Ctx->physics && s_Ctx->physics->HasController(ent)) {
            // Compute world-space displacement for this frame
            float dt = static_cast<float>(s_Ctx->DeltaTime);
            glm::vec3 displacement = (*vel) * dt;
            // Use a small minDist for controller movement to avoid tunneling; pass elapsedTime as dt.
            s_Ctx->physics->MoveController(ent, displacement, 0.001f, dt);
            return;
        }

        // Fallback: existing rigidbody velocity path (dynamic bodies)
        if (!s_Ctx->scene.any_of<RigidBodyComponent>(e))
        {
            BOOM_WARN("[ScriptBinding] SetLinearVelocity: Entity {} has no RigidBodyComponent", static_cast<uint32_t>(e));
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

    static void ICALL_API_CreateController(uint64_t handle, float radius, float height)
    {
        if (!s_Ctx) return;

        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] CreateController: invalid entity handle {}", static_cast<uint32_t>(e));
            return;
        }

        if (!s_Ctx->physics) {
            BOOM_WARN("[ScriptBinding] CreateController: no physics context available");
            return;
        }

        Entity entity{ &s_Ctx->scene, e };

        if (s_Ctx->physics->HasController(entity)) {
            BOOM_INFO("[ScriptBinding] CreateController: controller already exists for entity {}", static_cast<uint32_t>(e));
            return;
        }

        // Ensure CharacterControllerComponent exists and is synced with the values
        if (!s_Ctx->scene.any_of<CharacterControllerComponent>(e)) {
            s_Ctx->scene.emplace<CharacterControllerComponent>(e);
        }

        // Update the component with the values being used to create the controller
        auto& cc = s_Ctx->scene.get<CharacterControllerComponent>(e);
        cc.radius = radius;
        cc.height = height;

        if (s_Ctx->physics->CreateCapsuleController(entity, radius, height)) {
            cc.isCreated = true;
            BOOM_INFO("[ScriptBinding] Created capsule controller for entity {} (r={}, h={})", static_cast<uint32_t>(e), radius, height);
        }
        else {
            BOOM_WARN("[ScriptBinding] Failed to create capsule controller for entity {}", static_cast<uint32_t>(e));
        }
    }

    static void ICALL_API_TeleportController(uint64_t handle, glm::vec3* pos) {
        if (!s_Ctx || !pos) return;
        uint32_t entityID = static_cast<uint32_t>(handle);

        entt::entity e = static_cast<entt::entity>(entityID);
        if (e == entt::null || !s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] TeleportController: Invalid entity {}", entityID);
            return;
        }

        // Create a named Entity lvalue and pass it by reference (required by HasController).
        Entity entity{ &s_Ctx->scene, e };

        if (s_Ctx->physics && s_Ctx->physics->HasController(entity)) {
            s_Ctx->physics->SetControllerPosition(entityID, *pos);
        }
        else {
            BOOM_WARN("[ScriptBinding] TeleportController: Entity {} has no controller", entityID);
        }
    }

    static MonoArray* ICALL_API_GetControllerTriggerOverlaps(uint64_t handle) {
        if (!s_Ctx || !s_Ctx->physics) return nullptr;

        uint32_t entityID = static_cast<uint32_t>(handle);
        std::vector<EntityID> overlaps = s_Ctx->physics->GetControllerTriggerOverlaps(entityID);

        // Create a C# array to return
        MonoArray* result = mono_array_new(mono_domain_get(), mono_get_uint64_class(), overlaps.size());
        for (size_t i = 0; i < overlaps.size(); i++) {
            mono_array_set(result, uint64_t, i, static_cast<uint64_t>(static_cast<uint32_t>(overlaps[i])));
        }

        return result;
    }


    static bool ICALL_API_IsColliding(uint64_t handle)
    {
        if (!s_Ctx)
            return false;

        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e))
        {
            // Don't spam too hard here, this might be called every frame
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

    // Addition of Animator
    static Animator* GetAnimator(entt::entity e) {
        if (!s_Ctx || e == entt::null || !s_Ctx->scene.valid(e)) return nullptr;
        if (!s_Ctx->scene.any_of<AnimatorComponent>(e)) return nullptr;
        return s_Ctx->scene.get<AnimatorComponent>(e).animator.get();
    }
    static void ICALL_API_AnimatorSetFloat(uint64_t h, MonoString* name, float v) {
        auto* anim = GetAnimator((entt::entity)(uint32_t)h); if (!anim) return;
        char* n = mono_string_to_utf8(name); if (!n) return; anim->SetFloat(n, v); mono_free(n);
    }
    static void ICALL_API_AnimatorSetBool(uint64_t h, MonoString* name, bool v) {
        auto* anim = GetAnimator((entt::entity)(uint32_t)h); if (!anim) return;
        char* n = mono_string_to_utf8(name); if (!n) return; anim->SetBool(n, v); mono_free(n);
    }
    static void ICALL_API_AnimatorSetTrigger(uint64_t h, MonoString* name) {
        auto* anim = GetAnimator((entt::entity)(uint32_t)h); if (!anim) return;
        char* n = mono_string_to_utf8(name); if (!n) return; anim->SetTrigger(n); mono_free(n);
    }
    static void ICALL_API_AnimatorPlay(uint64_t h, MonoString* stateName) {
        auto* anim = GetAnimator((entt::entity)(uint32_t)h); if (!anim) return;
        char* n = mono_string_to_utf8(stateName); if (!n) return; anim->PlayClip(n); mono_free(n);
    }
    static void ICALL_API_AnimatorSetStateMachineEnabled(uint64_t h, bool enabled) {
        auto* anim = GetAnimator((entt::entity)(uint32_t)h); if (!anim) return;
        anim->SetStateMachineEnabled(enabled);
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

    // Pause Menu
    static void ICALL_API_UnloadPauseMenu() {
        if (!s_Ctx || !s_Ctx->app) return;
        s_Ctx->app->UnloadAdditiveScene(Boom::MenuType::Pause);
    }

    static void ICALL_API_ShowPauseMenu() {
        if (!s_Ctx || !s_Ctx->app) return;
        s_Ctx->app->ShowAdditiveScene(Boom::MenuType::Pause);
    }

    static bool ICALL_API_IsPauseMenuLoaded() {
        if (!s_Ctx) return false;

        auto view = s_Ctx->scene.view<Boom::MenuComponent>();
        for (auto e : view) {
            if (view.get<Boom::MenuComponent>(e).menuType == Boom::MenuType::Pause)
                return true;
        }
        return false;
    }

    static void ICALL_API_SetGameLogicPaused(bool isPaused) {
        if (s_Ctx && s_Ctx->app) {
            s_Ctx->app->SetGameLogicPaused(isPaused);
        }
    }

    static void ICALL_API_SetGroupVolume(MonoString* groupName, float volume) {
        if (!groupName) return;
        char* nameStr = mono_string_to_utf8(groupName);
        if (nameStr) {
            // Clamp volume 0.0 to 1.0
            float v = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
            SoundEngine::Instance().SetGroupVolume(std::string(nameStr), v);
            mono_free(nameStr);
        }
    }

    static float ICALL_API_GetGroupVolume(MonoString* groupName) {
        if (!groupName) return 1.0f;
        char* nameStr = mono_string_to_utf8(groupName);
        float vol = 1.0f;
        if (nameStr) {
            vol = SoundEngine::Instance().GetGroupVolume(std::string(nameStr));
            mono_free(nameStr);
        }
        return vol;
    }
    // End Pause Menu

    // Death Menu
    static void ICALL_API_UnloadDeathMenu() {
        if (!s_Ctx || !s_Ctx->app) return;
        s_Ctx->app->UnloadAdditiveScene(Boom::MenuType::Death);
    }

    static void ICALL_API_ShowDeathMenu() {
        if (!s_Ctx || !s_Ctx->app) return;
        s_Ctx->app->ShowAdditiveScene(Boom::MenuType::Death);
    }

    static bool ICALL_API_IsDeathMenuLoaded() {
        if (!s_Ctx) return false;

        auto view = s_Ctx->scene.view<Boom::MenuComponent>();
        for (auto e : view) {
            if (view.get<Boom::MenuComponent>(e).menuType == Boom::MenuType::Death)
                return true;
        }
        return false;
    }

    static void ICALL_API_SetPlayerDead(bool isDead) {
        if (s_Ctx && s_Ctx->app) {
            s_Ctx->app->SetPlayerDead(isDead);
        }
    }

    // End Menu
    static void ICALL_API_UnloadEndMenu() {
        if (!s_Ctx || !s_Ctx->app) return;
        s_Ctx->app->UnloadAdditiveScene(Boom::MenuType::End);
    }

    static void ICALL_API_ShowEndMenu() {
        if (!s_Ctx || !s_Ctx->app) return;
        s_Ctx->app->ShowAdditiveScene(Boom::MenuType::End);
    }

    static bool ICALL_API_IsEndMenuLoaded() {
        if (!s_Ctx) return false;

        auto view = s_Ctx->scene.view<Boom::MenuComponent>();
        for (auto e : view) {
            if (view.get<Boom::MenuComponent>(e).menuType == Boom::MenuType::End)
                return true;
        }
        return false;
    }

    static void ICALL_API_SetGameEnd(bool isEnd) {
        if (s_Ctx && s_Ctx->app) {
            s_Ctx->app->SetGameEnd(isEnd);
        }
    }

    // For freeze
    static void ICALL_API_DestroyEntity(uint64_t entityID)
    {
        if (!s_Ctx || !s_Ctx->scene.valid((entt::entity)entityID)) return;
        s_Ctx->scene.destroy((entt::entity)entityID);
    }



    static void ICALL_API_TogglePause() {
        if (s_Ctx && s_Ctx->app) {
            s_Ctx->app->TogglePause();
        }
    }

    static void ICALL_API_EnableFileWatcher(bool enable)
    {
        if (!s_Ctx || !s_Ctx->scriptingSystem) return;

        // This function should exist in your ScriptingSystem.
        // It's the same one used in Application.cpp
        s_Ctx->scriptingSystem->EnableAutoHotReload(enable);

        if (enable) {
            BOOM_INFO("[ScriptBinding] Enabled auto hot-reload");
        }
        else {
            BOOM_INFO("[ScriptBinding] Disabled auto hot-reload");
        }
    }

    static int ICALL_API_GetApplicationState() {
        if (!s_Ctx || !s_Ctx->app) return (int)ApplicationState::STOPPED;
        return (int)s_Ctx->app->GetState();
    }

	//AI Component functions
    static int ICALL_API_AI_GetPatrolPointCount(uint64_t entityHandle)
    {
        auto e = static_cast<entt::entity>(entityHandle);
        auto& reg = s_Ctx->scene;
        if (!reg.valid(e) || !reg.all_of<Boom::AIComponent>(e))
            return 0;

        auto& ai = reg.get<Boom::AIComponent>(e);
        return static_cast<int>(ai.patrolPoints.size());
    }

    static void ICALL_API_AI_GetPatrolPoint(uint64_t entityHandle, int index, glm::vec3* outPos)
    {
        auto e = static_cast<entt::entity>(entityHandle);
        auto& reg = s_Ctx->scene;
        if (!outPos || !reg.valid(e) || !reg.all_of<Boom::AIComponent>(e))
            return;

        auto& ai = reg.get<Boom::AIComponent>(e);
        if (index < 0 || index >= static_cast<int>(ai.patrolPoints.size()))
            return;

        *outPos = ai.patrolPoints[static_cast<size_t>(index)];
    }

    static int ICALL_API_AI_GetMode(uint64_t entityHandle)
    {
        auto e = static_cast<entt::entity>(entityHandle);
        auto& reg = s_Ctx->scene;
        if (!reg.valid(e) || !reg.all_of<Boom::AIComponent>(e))
            return static_cast<int>(Boom::AIComponent::AIMode::Auto);

        auto& ai = reg.get<Boom::AIComponent>(e);
        return static_cast<int>(ai.mode);
    }

    static void ICALL_API_SetNavAgentActive(uint64_t handle, bool active)
    {
        if (!s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (e == entt::null || !s_Ctx->scene.valid(e)) return;

        // 1. Toggle AI Brain
        if (s_Ctx->scene.any_of<Boom::AIComponent>(e)) {
            auto& ai = s_Ctx->scene.get<Boom::AIComponent>(e);
            ai.active = active;
        }

        // 2. Toggle Nav Agent
        if (s_Ctx->scene.any_of<Boom::NavAgentComponent>(e)) {
            auto& ag = s_Ctx->scene.get<Boom::NavAgentComponent>(e);
            ag.active = active;

            if (!active) {
                // FREEZE
                ag.velocity = glm::vec3(0.0f);
            }
            else {
                // UNFREEZE
            }
        }
    }

    static void ICALL_API_SetNavAgentPosition(uint64_t handle, glm::vec3* pos)
    {
        if (!s_Ctx || !pos) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (e == entt::null || !s_Ctx->scene.valid(e)) return;

        // 1. Teleport Transform
        if (s_Ctx->scene.any_of<TransformComponent>(e)) {
            auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;
            t.translate = *pos;
        }

        // 2. Teleport RigidBody if present
        if (s_Ctx->scene.any_of<RigidBodyComponent>(e)) {
            auto& rb = s_Ctx->scene.get<RigidBodyComponent>(e).RigidBody;
            if (rb.actor) {
                PxTransform pose = rb.actor->getGlobalPose();
                pose.p = PxVec3(pos->x, pos->y, pos->z);
                
                if (PxRigidDynamic* dyn = rb.actor->is<PxRigidDynamic>()) {
                    dyn->setLinearVelocity(PxVec3(0, 0, 0));
                    dyn->setAngularVelocity(PxVec3(0, 0, 0));
                    dyn->setGlobalPose(pose);
                    dyn->wakeUp();
                } else {
                    rb.actor->setGlobalPose(pose);
                }
            }
        }

        // 3. Reset Nav Agent internal state
        if (s_Ctx->scene.any_of<Boom::NavAgentComponent>(e)) {
            auto& ag = s_Ctx->scene.get<Boom::NavAgentComponent>(e);
            ag.path.clear();
            ag.waypoint = 0;
            ag.dirty = true; // Force it to recalculate path from new position
            ag.velocity = glm::vec3(0, 0, 0);
        }
    }

    // Add this internal call function
    static void Boom_API_MoveController(uint64_t handle, glm::vec3* displacement, float minDist, float dt) {
        if (!s_Ctx || !s_Ctx->physics) return;

        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e)) return;

        Entity entity{ &s_Ctx->scene, e };
        glm::vec3 disp(displacement->x, displacement->y, displacement->z);
        s_Ctx->physics->MoveController(entity, disp, minDist, dt);
    }

    static bool Boom_API_IsControllerGrounded(uint64_t handle) {
        if (!s_Ctx || !s_Ctx->physics) return false;

        uint32_t entityID = static_cast<uint32_t>(handle);
        return s_Ctx->physics->IsControllerGrounded(entityID);
    }

    static float ICALL_API_GetThirdPersonCameraYaw() {
        if (!s_Ctx) return 0.0f;

        // Find the active third-person camera
        auto& registry = s_Ctx->scene;
        auto view = registry.view<ThirdPersonCameraComponent>();

        // Check if there are any third-person cameras
        if (!view.empty()) {
            // Return the first third-person camera found
            auto entity = *view.begin();
            auto& cam = view.get<ThirdPersonCameraComponent>(entity);
            return cam.currentYaw;
        }

        return 0.0f; // No third-person camera found
    }

    // Add near other ICALL functions (after ICALL_API_SetSoundPosition)

    // Physics line-of-sight raycast
    static bool ICALL_API_Linecast(glm::vec3* from, glm::vec3* to, uint64_t ignoreEntityHandle)
    {
        if (!s_Ctx || !from || !to) return true;
        if (!s_Ctx->physics) return true;

        PxScene* scene = s_Ctx->physics->GetPxScene();
        if (!scene) return true;

        glm::vec3 delta = *to - *from;
        float len2 = glm::dot(delta, delta);
        if (len2 < 1e-6f) return true;

        float len = std::sqrt(len2);

        // Start ray slightly above ground and from entity origin
        PxVec3 origin(from->x, from->y + 0.15f, from->z);
        PxVec3 dir(delta.x / len, delta.y / len, delta.z / len);

        PxQueryFilterData filter;
        filter.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;

        // Convert C# entity handle to entt::entity
        entt::entity ignoreEntity = static_cast<entt::entity>(static_cast<uint32_t>(ignoreEntityHandle));

        // Pre-filter to skip triggers AND the entity doing the looking
        struct PreFilter : PxQueryFilterCallback {
            entt::entity entityToIgnore;

            PreFilter(entt::entity ignore) : entityToIgnore(ignore) {}

            PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* shape,
                const PxRigidActor* actor, PxHitFlags&) override
            {
                // Skip triggers
                if (shape->getFlags() & PxShapeFlag::eTRIGGER_SHAPE)
                    return PxQueryHitType::eNONE;

                // Skip the entity doing the raycast (the enemy itself)
                if (actor && actor->userData)
                {
                    EntityID* entityID = static_cast<EntityID*>(actor->userData);
                    if (entityID && *entityID == entityToIgnore)
                        return PxQueryHitType::eNONE;
                }

                return PxQueryHitType::eBLOCK;
            }

            PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
            {
                return PxQueryHitType::eBLOCK;
            }
        } preFilter(ignoreEntity);

        PxRaycastBuffer hit;
        bool blocked = scene->raycast(origin, dir, len, hit, PxHitFlag::eDEFAULT, filter, &preFilter);

        // If nothing was hit, line is clear
        if (!blocked) return true;

        // If we hit something, check if it's beyond the target (we hit the target itself)
        // Use a smaller epsilon to be more strict
        const float epsilon = 0.05f;
        if (hit.block.distance >= len - epsilon)
            return true;

        // Something blocked the ray before reaching the target
        return false;
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

    static uint64_t ICALL_API_GetControllerStandingOn(uint64_t handle) {
        if (!s_Ctx || !s_Ctx->physics) return 0;

        uint32_t entityID = static_cast<uint32_t>(handle);
        return static_cast<uint64_t>(s_Ctx->physics->GetControllerStandingOn(entityID));
    }

    static int ICALL_API_GetSurfaceType(uint64_t handle) {
        if (!s_Ctx) return 0; // DEFAULT
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e)) return 0;

        if (!s_Ctx->scene.any_of<ColliderComponent>(e)) return 0;

        auto& collider = s_Ctx->scene.get<ColliderComponent>(e);
        return static_cast<int>(collider.Collider.surfaceType);
    }

    // Store GC handles to delegate objects instead of raw function pointers
    static std::unordered_map<uint64_t, uint32_t> s_TriggerEnterCallbackHandles;
    static std::unordered_map<uint64_t, uint32_t> s_TriggerExitCallbackHandles;

    // Function pointer types for C# callbacks (kept for backward compatibility)
    typedef void (*TriggerCallback)(uint64_t triggerEntity, uint64_t otherEntity);


    static void ICALL_API_UnregisterTriggerCallbacks(uint64_t triggerHandle) {
        // Free GC handles before removing
        auto enterIt = s_TriggerEnterCallbackHandles.find(triggerHandle);
        if (enterIt != s_TriggerEnterCallbackHandles.end()) {
            mono_gchandle_free(enterIt->second);
            s_TriggerEnterCallbackHandles.erase(enterIt);
        }

        auto exitIt = s_TriggerExitCallbackHandles.find(triggerHandle);
        if (exitIt != s_TriggerExitCallbackHandles.end()) {
            mono_gchandle_free(exitIt->second);
            s_TriggerExitCallbackHandles.erase(exitIt);
        }
    }

    static bool ICALL_API_HasAnimator(uint64_t handle) {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e)) return false;
        return s_Ctx->scene.any_of<AnimatorComponent>(e);
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

        auto it = s_TriggerEnterCallbackHandles.find(triggerEntity);
        if (it != s_TriggerEnterCallbackHandles.end()) {
            // Get the delegate object from GC handle
            MonoObject* delegateObj = mono_gchandle_get_target(it->second);
            if (!delegateObj) {
                BOOM_WARN("[ScriptBinding] Trigger enter callback GC handle is invalid for {}", triggerEntity);
                mono_gchandle_free(it->second);
                s_TriggerEnterCallbackHandles.erase(it);
                return;
            }

            // Prepare arguments for delegate invocation
            void* args[2];
            args[0] = &triggerEntity;
            args[1] = &otherEntity;

            // Invoke the delegate safely using Mono runtime
            MonoObject* exc = nullptr;
            mono_runtime_delegate_invoke(delegateObj, args, &exc);

            if (exc) {
                BOOM_ERROR("[ScriptBinding] Exception in trigger enter callback for trigger {}", triggerEntity);
                // Log the exception details if possible
                MonoClass* excClass = mono_object_get_class(exc);
                if (excClass) {
                    const char* excName = mono_class_get_name(excClass);
                    BOOM_ERROR("[ScriptBinding] Exception type: {}", excName);
                }
            }
            else {
                BOOM_INFO("[ScriptBinding] Successfully called trigger enter callback for trigger {} and entity {}",
                    triggerEntity, otherEntity);
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

        auto it = s_TriggerExitCallbackHandles.find(triggerEntity);
        if (it != s_TriggerExitCallbackHandles.end()) {
            // Get the delegate object from GC handle
            MonoObject* delegateObj = mono_gchandle_get_target(it->second);
            if (!delegateObj) {
                BOOM_WARN("[ScriptBinding] Trigger exit callback GC handle is invalid for {}", triggerEntity);
                mono_gchandle_free(it->second);
                s_TriggerExitCallbackHandles.erase(it);
                return;
            }

            // Prepare arguments for delegate invocation
            void* args[2];
            args[0] = &triggerEntity;
            args[1] = &otherEntity;

            // Invoke the delegate safely using Mono runtime
            MonoObject* exc = nullptr;
            mono_runtime_delegate_invoke(delegateObj, args, &exc);

            if (exc) {
                BOOM_ERROR("[ScriptBinding] Exception in trigger exit callback for trigger {}", triggerEntity);
                // Log the exception details if possible
                MonoClass* excClass = mono_object_get_class(exc);
                if (excClass) {
                    const char* excName = mono_class_get_name(excClass);
                    BOOM_ERROR("[ScriptBinding] Exception type: {}", excName);
                }
            }
            else {
                BOOM_INFO("[ScriptBinding] Successfully called trigger exit callback for trigger {} and entity {}",
                    triggerEntity, otherEntity);
            }
        }
        else {
            BOOM_INFO("[ScriptBinding] No trigger exit callback registered for entity {}", triggerEntity);
        }
    }

    // ========= SPRITE COMPONENT INTERNAL CALLS =========
    static bool ICALL_API_HasSprite(uint64_t handle)
    {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        return (e != entt::null && s_Ctx->scene.valid(e) && s_Ctx->scene.any_of<SpriteComponent>(e));
    }

    static void ICALL_API_GetSpriteColor(uint64_t handle, glm::vec4* outColor)
    {
        if (!outColor || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<SpriteComponent>(e)) {
            *outColor = glm::vec4(1.0f);
            return;
        }
        *outColor = s_Ctx->scene.get<SpriteComponent>(e).color;
    }

    static void ICALL_API_SetSpriteColor(uint64_t handle, glm::vec4* color)
    {
        if (!color || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<SpriteComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetSpriteColor: Entity doesn't have SpriteComponent");
            return;
        }
        s_Ctx->scene.get<SpriteComponent>(e).color = *color;
    }

    static float ICALL_API_GetSpriteAlpha(uint64_t handle)
    {
        if (!s_Ctx) return 1.0f;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<SpriteComponent>(e)) {
            return 1.0f;
        }
        return s_Ctx->scene.get<SpriteComponent>(e).color.a;
    }

    static void ICALL_API_SetSpriteAlpha(uint64_t handle, float alpha)
    {
        if (!s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<SpriteComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetSpriteAlpha: Entity doesn't have SpriteComponent");
            return;
        }
        s_Ctx->scene.get<SpriteComponent>(e).color.a = glm::clamp(alpha, 0.0f, 1.0f);
    }

    // ========= TEXT COMPONENT INTERNAL CALLS =========
    static bool ICALL_API_HasText(uint64_t handle)
    {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        return (e != entt::null && s_Ctx->scene.valid(e) && s_Ctx->scene.any_of<TextComponent>(e));
    }

    static void ICALL_API_GetText(uint64_t handle, MonoString** outText)
    {
        if (!outText || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
            *outText = mono_string_new(mono_domain_get(), "");
            return;
        }
        const std::string& text = s_Ctx->scene.get<TextComponent>(e).text;
        *outText = mono_string_new(mono_domain_get(), text.c_str());
    }

    static void ICALL_API_SetText(uint64_t handle, MonoString* newText)
    {
        if (!newText || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetText: Entity doesn't have TextComponent");
            return;
        }
        char* cStr = mono_string_to_utf8(newText);
        s_Ctx->scene.get<TextComponent>(e).text = std::string(cStr);
        mono_free(cStr);
    }

    static void ICALL_API_GetTextColor(uint64_t handle, glm::vec4* outColor)
    {
        if (!outColor || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
            *outColor = glm::vec4(1.0f);
            return;
        }
        *outColor = s_Ctx->scene.get<TextComponent>(e).color;
    }

    static void ICALL_API_SetTextColor(uint64_t handle, glm::vec4* color)
    {
        if (!color || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetTextColor: Entity doesn't have TextComponent");
            return;
        }
        s_Ctx->scene.get<TextComponent>(e).color = *color;
    }

    static float ICALL_API_GetTextScale(uint64_t handle)
    {
        if (!s_Ctx) return 1.0f;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
            return 1.0f;
        }
        return s_Ctx->scene.get<TextComponent>(e).scale;
    }

    static void ICALL_API_SetTextScale(uint64_t handle, float scale)
    {
        if (!s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetTextScale: Entity doesn't have TextComponent");
            return;
        }
        s_Ctx->scene.get<TextComponent>(e).scale = scale;
    }

    static void ICALL_API_GetTextPosition(uint64_t handle, glm::vec2* outPos)
    {
        if (!outPos || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
            *outPos = glm::vec2(0.0f);
            return;
        }
        *outPos = s_Ctx->scene.get<TextComponent>(e).screenPosition;
    }

    static void ICALL_API_SetTextPosition(uint64_t handle, glm::vec2* pos)
    {
        if (!pos || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetTextPosition: Entity doesn't have TextComponent");
            return;
        }
        s_Ctx->scene.get<TextComponent>(e).screenPosition = *pos;
    }

    // ========= SPOTLIGHT COMPONENT INTERNAL CALLS =========
    static bool ICALL_API_HasSpotLight(uint64_t handle)
    {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        return (e != entt::null && s_Ctx->scene.valid(e) && s_Ctx->scene.any_of<SpotLightComponent>(e));
    }

    static void ICALL_API_GetSpotLightColor(uint64_t handle, glm::vec3* outColor)
    {
        if (!outColor || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<SpotLightComponent>(e)) {
            *outColor = glm::vec3(1.0f);
            return;
        }
        *outColor = s_Ctx->scene.get<SpotLightComponent>(e).light.radiance;
    }

    static void ICALL_API_SetSpotLightColor(uint64_t handle, glm::vec3* color)
    {
        if (!color || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<SpotLightComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetSpotLightColor: Entity doesn't have SpotLightComponent");
            return;
        }
        s_Ctx->scene.get<SpotLightComponent>(e).light.radiance = *color;
    }

    static float ICALL_API_GetSpotLightIntensity(uint64_t handle)
    {
        if (!s_Ctx) return 1.0f;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<SpotLightComponent>(e)) {
            return 1.0f;
        }
        return s_Ctx->scene.get<SpotLightComponent>(e).light.intensity;
    }

    static void ICALL_API_SetSpotLightIntensity(uint64_t handle, float intensity)
    {
        if (!s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<SpotLightComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetSpotLightIntensity: Entity doesn't have SpotLightComponent");
            return;
        }
        s_Ctx->scene.get<SpotLightComponent>(e).light.intensity = intensity;
    }

    // ========= VIDEO COMPONENT INTERNAL CALLS =========
    static bool ICALL_API_HasVideoComponent(uint64_t handle)
    {
        if (!s_Ctx) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        return (e != entt::null && s_Ctx->scene.valid(e) && s_Ctx->scene.any_of<VideoComponent>(e));
    }

    static bool ICALL_API_IsVideoPlaying(uint64_t handle)
    {
        if (!s_Ctx || !s_Ctx->videoSystem) return false;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<VideoComponent>(e)) {
            return false;
        }
        auto* player = s_Ctx->videoSystem->GetPlayer(e);
        return player && player->IsPlaying();
    }

    static bool ICALL_API_HasVideoEnded(uint64_t handle)
    {
        if (!s_Ctx || !s_Ctx->videoSystem) return true;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<VideoComponent>(e)) {
            return true;
        }
        auto* player = s_Ctx->videoSystem->GetPlayer(e);
        return player ? player->HasEnded() : true;
    }

    static void ICALL_API_PlayVideo(uint64_t handle)
    {
        if (!s_Ctx || !s_Ctx->videoSystem) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<VideoComponent>(e)) {
            BOOM_WARN("[ScriptBinding] PlayVideo: Entity doesn't have VideoComponent");
            return;
        }
        s_Ctx->videoSystem->Play(e);
    }

    static void ICALL_API_StopVideo(uint64_t handle)
    {
        if (!s_Ctx || !s_Ctx->videoSystem) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<VideoComponent>(e)) {
            BOOM_WARN("[ScriptBinding] StopVideo: Entity doesn't have VideoComponent");
            return;
        }
        s_Ctx->videoSystem->Stop(e);
    }

    static double ICALL_API_GetVideoDuration(uint64_t handle)
    {
        if (!s_Ctx || !s_Ctx->videoSystem) return 0.0;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<VideoComponent>(e)) {
            return 0.0;
        }
        auto* player = s_Ctx->videoSystem->GetPlayer(e);
        return player ? player->GetDuration() : 0.0;
    }

    static double ICALL_API_GetVideoCurrentTime(uint64_t handle)
    {
        if (!s_Ctx || !s_Ctx->videoSystem) return 0.0;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<VideoComponent>(e)) {
            return 0.0;
        }
        auto* player = s_Ctx->videoSystem->GetPlayer(e);
        return player ? player->GetTickCount() : 0.0;
    }

    struct ScriptTransform {
        float posX, posY, posZ;
        float rotX, rotY, rotZ;
        float scaleX, scaleY, scaleZ;
    };

    static ScriptTransform* ICALL_API_GetTransform(uint64_t handle, ScriptTransform* outTransform) {
        if (!s_Ctx || !outTransform) return nullptr;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (e == entt::null || !s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] GetTransform: Invalid entity");
            return nullptr;
        }

        if (!s_Ctx->scene.any_of<TransformComponent>(e)) {
            BOOM_WARN("[ScriptBinding] GetTransform: Entity {} has no TransformComponent",
                static_cast<uint32_t>(e));
            return nullptr;
        }

        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;

        outTransform->posX = t.translate.x;
        outTransform->posY = t.translate.y;
        outTransform->posZ = t.translate.z;

        outTransform->rotX = t.rotate.x;
        outTransform->rotY = t.rotate.y;
        outTransform->rotZ = t.rotate.z;

        outTransform->scaleX = t.scale.x;
        outTransform->scaleY = t.scale.y;
        outTransform->scaleZ = t.scale.z;

        return outTransform;
    }

    static void ICALL_API_SetTransform(uint64_t handle, ScriptTransform* transform) {
        if (!s_Ctx || !transform) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (e == entt::null || !s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] SetTransform: Invalid entity");
            return;
        }

        if (!s_Ctx->scene.any_of<TransformComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetTransform: Entity {} has no TransformComponent",
                static_cast<uint32_t>(e));
            return;
        }

        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;

        t.translate.x = transform->posX;
        t.translate.y = transform->posY;
        t.translate.z = transform->posZ;

        t.rotate.x = transform->rotX;
        t.rotate.y = transform->rotY;
        t.rotate.z = transform->rotZ;

        t.scale.x = transform->scaleX;
        t.scale.y = transform->scaleY;
        t.scale.z = transform->scaleZ;
    }

    // NEW GCHANDLE-BASED VERSIONS:
    static void ICALL_API_RegisterTriggerEnterCallback(uint64_t triggerHandle, MonoObject* delegateObj) {
        // Add comprehensive safety checks
        if (!delegateObj) {
            BOOM_WARN("[ScriptBinding] RegisterTriggerEnterCallback: Null delegate provided");
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

        // Free existing handle if one exists
        auto existingIt = s_TriggerEnterCallbackHandles.find(triggerHandle);
        if (existingIt != s_TriggerEnterCallbackHandles.end()) {
            mono_gchandle_free(existingIt->second);
            BOOM_INFO("[ScriptBinding] Freed existing trigger enter callback for entity {}", triggerHandle);
        }

        // Create a GC handle to prevent the delegate from being collected
        uint32_t gcHandle = mono_gchandle_new(delegateObj, false);
        s_TriggerEnterCallbackHandles[triggerHandle] = gcHandle;

        BOOM_INFO("[ScriptBinding] Registered trigger enter callback (GCHandle: {}) for entity {}", gcHandle, triggerHandle);
    }

    static void ICALL_API_RegisterTriggerExitCallback(uint64_t triggerHandle, MonoObject* delegateObj) {
        // Add comprehensive safety checks
        if (!delegateObj) {
            BOOM_WARN("[ScriptBinding] RegisterTriggerExitCallback: Null delegate provided");
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

        // Free existing handle if one exists
        auto existingIt = s_TriggerExitCallbackHandles.find(triggerHandle);
        if (existingIt != s_TriggerExitCallbackHandles.end()) {
            mono_gchandle_free(existingIt->second);
            BOOM_INFO("[ScriptBinding] Freed existing trigger exit callback for entity {}", triggerHandle);
        }

        // Create a GC handle to prevent the delegate from being collected
        uint32_t gcHandle = mono_gchandle_new(delegateObj, false);
        s_TriggerExitCallbackHandles[triggerHandle] = gcHandle;

        BOOM_INFO("[ScriptBinding] Registered trigger exit callback (GCHandle: {}) for entity {}", gcHandle, triggerHandle);
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

    static void ICALL_API_Set3DMinMaxDistance(MonoString* name, float minDist, float maxDist) {
        if (!name) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& soundEngine = SoundEngine::Instance();
            soundEngine.Set3DMinMaxDistance(std::string(nameStr), minDist, maxDist);
            mono_free(nameStr);
        }
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

    // ========= SOUND COMPONENT MANIPULATION BINDINGS =========

    // Helper to find entry by name in SoundComponent
    static SoundComponent::Entry* FindSoundEntry(SoundComponent& sc, const std::string& name) {
        for (auto& entry : sc.entries) {
            if (entry.name == name) return &entry;
        }
        return nullptr;
    }

    static bool ICALL_API_HasSoundComponent(uint64_t handle) {
        if (!s_Ctx) return false;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return false;
        return s_Ctx->scene.any_of<SoundComponent>(eid);
    }

    static void ICALL_API_AddSoundComponent(uint64_t handle) {
        if (!s_Ctx) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) {
            s_Ctx->scene.emplace<SoundComponent>(eid);
        }
    }

    static int ICALL_API_GetSoundEntryCount(uint64_t handle) {
        if (!s_Ctx) return 0;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return 0;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return 0;
        auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
        return static_cast<int>(sc.entries.size());
    }

    static void ICALL_API_AddSoundEntry(uint64_t handle, MonoString* name, MonoString* filePath) {
        if (!s_Ctx || !name || !filePath) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;

        // Ensure component exists
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) {
            s_Ctx->scene.emplace<SoundComponent>(eid);
        }

        char* nameStr = mono_string_to_utf8(name);
        char* pathStr = mono_string_to_utf8(filePath);

        if (nameStr && pathStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);

            // Check if entry already exists
            auto* existing = FindSoundEntry(sc, nameStr);
            if (existing) {
                // Update existing entry
                existing->filePath = pathStr;
                existing->filePaths.clear();
                existing->filePaths.push_back(pathStr);
            } else {
                // Add new entry
                SoundComponent::Entry entry;
                entry.name = nameStr;
                entry.filePath = pathStr;
                entry.filePaths.push_back(pathStr);
                sc.entries.push_back(std::move(entry));
            }
        }

        if (nameStr) mono_free(nameStr);
        if (pathStr) mono_free(pathStr);
    }

    static void ICALL_API_RemoveSoundEntry(uint64_t handle, MonoString* name) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            sc.entries.erase(
                std::remove_if(sc.entries.begin(), sc.entries.end(),
                    [&](const SoundComponent::Entry& e) { return e.name == nameStr; }),
                sc.entries.end()
            );
            mono_free(nameStr);
        }
    }

    static void ICALL_API_SetSoundEntryVolume(uint64_t handle, MonoString* name, float volume) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->volume = volume;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_SetSoundEntryPitch(uint64_t handle, MonoString* name, float pitch) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->pitch = pitch;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_SetSoundEntryLoop(uint64_t handle, MonoString* name, bool loop) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->loop = loop;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_SetSoundEntryMute(uint64_t handle, MonoString* name, bool mute) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->mute = mute;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_SetSoundEntryPan(uint64_t handle, MonoString* name, float pan) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->stereoPan = pan;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_SetSoundEntrySpatialBlend(uint64_t handle, MonoString* name, float blend) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->spatialBlend = blend;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_SetSoundEntryPriority(uint64_t handle, MonoString* name, int priority) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->priority = priority;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_SetSoundEntry3DDistance(uint64_t handle, MonoString* name, float minDist, float maxDist) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->minDistance = minDist;
                entry->maxDistance = maxDist;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_SetSoundEntryPlayOnStart(uint64_t handle, MonoString* name, bool playOnStart) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->playOnStart = playOnStart;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_PlaySoundEntry(uint64_t handle, MonoString* name) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                // Set playOnStart to true - SoundSystem will pick it up
                entry->playOnStart = true;
            }
            mono_free(nameStr);
        }
    }

    static void ICALL_API_StopSoundEntry(uint64_t handle, MonoString* name) {
        if (!s_Ctx || !name) return;
        auto eid = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (!s_Ctx->scene.valid(eid)) return;
        if (!s_Ctx->scene.any_of<SoundComponent>(eid)) return;

        char* nameStr = mono_string_to_utf8(name);
        if (nameStr) {
            auto& sc = s_Ctx->scene.get<SoundComponent>(eid);
            if (auto* entry = FindSoundEntry(sc, nameStr)) {
                entry->playOnStart = false;
                // Also stop the actual sound instance
                uint64_t uid = static_cast<uint64_t>(static_cast<uint32_t>(eid));
                size_t idx = 0;
                for (size_t i = 0; i < sc.entries.size(); ++i) {
                    if (sc.entries[i].name == nameStr) { idx = i; break; }
                }
                std::string instanceName = "ent_" + std::to_string(uid) + "_" + std::to_string(idx) + "_" + nameStr;
                SoundEngine::Instance().StopSound(instanceName);
            }
            mono_free(nameStr);
        }
    }

    // main menu
    static uint64_t ICALL_API_PickGameEntity() {
        if (!s_Ctx || !s_Ctx->app) return 0;

        auto* app = static_cast<Application*>(s_Ctx->app);

        // Get the raw result (which might be entt::null / 4294967295)
        entt::entity hit = static_cast<entt::entity>(app->CastRayFromGameCamera());

        // FIX: Convert entt::null to 0 so C# logic works
        if (hit == entt::null) {
            return 0;
        }

        return static_cast<uint64_t>(static_cast<uint32_t>(hit));
    }

    static bool ICALL_API_GetMousePosInViewport(glm::vec2* outPos)
    {
        if (!s_Ctx || !s_Ctx->app || !outPos) return false;
        auto* app = static_cast<Application*>(s_Ctx->app);

        float x, y;
        if (app->GetMousePosInViewport(x, y)) {
            outPos->x = x;
            outPos->y = y;
            return true;
        }
        return false;
    }

    // Projects a 3D world point to 2D viewport pixel coordinates
    static bool ICALL_API_ProjectWorldToViewport(glm::vec3* worldPos, glm::vec2* outViewportPos)
    {
        if (!s_Ctx || !worldPos || !outViewportPos) return false;

        // 1. Find the main camera
        Camera3D* mainCam = nullptr;
        Transform3D* mainCamTrans = nullptr;
        auto view = s_Ctx->scene.view<CameraComponent>();
        for (auto e : view) {
            auto& cc = view.get<CameraComponent>(e);
            if (cc.camera.cameraType == Camera3D::CameraType::Main) {
                mainCam = &cc.camera;
                mainCamTrans = &s_Ctx->scene.get<TransformComponent>(e).transform;
                break;
            }
        }
        if (!mainCam || !mainCamTrans) return false;

        // 2. Get viewport dimensions
        auto* win = s_Ctx->window.get();
        float vX = win->GetViewportX();
        float vY = win->GetViewportY();
        float vW = win->GetViewportW();
        float vH = win->GetViewportH();
        if (vW <= 1.0f || vH <= 1.0f) { // Standalone build fix
            vX = 0.0f; vY = 0.0f;
            vW = (float)win->getWidth(); vH = (float)win->getHeight();
        }
        glm::vec4 viewportRect = glm::vec4(vX, vY, vW, vH);

        // 3. Get View and Projection matrices
        glm::mat4 viewMat = mainCam->View(*mainCamTrans);
        glm::mat4 projMat = mainCam->Projection(vW / vH);

        // 4. Project the point
        glm::vec3 screenPos = glm::project(*worldPos, viewMat, projMat, viewportRect);

        // 5. Check if it's behind the camera
        if (screenPos.z > 1.0f || screenPos.z < 0.0f) {
            return false;
        }

        // 6. Return Local Viewport Coordinates (subtract the offset)
        // This makes (0,0) the bottom-left of the GAME VIEW, not the window.
        outViewportPos->x = screenPos.x - vX;
        outViewportPos->y = screenPos.y - vY;

        return true;
    }

    static bool ICALL_API_Check2DViewportClick(uint64_t handle, float mouseX, float mouseY)
    {
        if (!s_Ctx) return false;
        entt::entity entity = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (entity == entt::null || !s_Ctx->scene.valid(entity)) return false;

        const auto* transformComp = s_Ctx->scene.try_get<Boom::TransformComponent>(entity);
        if (!transformComp) return false;

        auto* win = s_Ctx->window.get();
        float vW = win->GetViewportW();
        float vH = win->GetViewportH();

        // convert transform from ndc to screen pos
        glm::vec2 pos{ transformComp->transform.translate };
        pos.x = ((pos.x + 1.f) * 0.5f) * vW;
        pos.y = vH - ((pos.y + 1.f) * 0.5f) * vH;
        glm::vec2 scale{ transformComp->transform.scale.x * vW,  transformComp->transform.scale.y * vH };

        //get corners of box
        glm::vec2 min{ pos - scale * 0.5f };
        glm::vec2 max{ pos + scale * 0.5f };
        glm::vec2 mouse{ mouseX, mouseY };

        if (mouse.x < min.x || mouse.x > max.x || mouse.y < min.y || mouse.y > max.y) {
            return false;
        }

        return true;
    }

    // Add this function to clean up all trigger callbacks when Mono domain is unloaded
    void ClearAllTriggerCallbacks() {
        // Free all GC handles before clearing
        for (auto& pair : s_TriggerEnterCallbackHandles) {
            mono_gchandle_free(pair.second);
        }
        for (auto& pair : s_TriggerExitCallbackHandles) {
            mono_gchandle_free(pair.second);
        }

        s_TriggerEnterCallbackHandles.clear();
        s_TriggerExitCallbackHandles.clear();
        BOOM_INFO("[ScriptBinding] Cleared all trigger callbacks and freed GC handles (domain unload)");
    }

    static void ICALL_API_SetRotationY(uint64_t handle, float yawDegrees)
    {
        if (!s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (e == entt::null || !s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] SetRotationY: Invalid entity");
            return;
        }

        // 1. ALWAYS update the TransformComponent (This drives the visual model)
        if (s_Ctx->scene.any_of<TransformComponent>(e)) {
            auto& transform = s_Ctx->scene.get<TransformComponent>(e).transform;
            transform.rotate.y = yawDegrees;
        }
        else {
            BOOM_WARN("[ScriptBinding] SetRotationY: Entity has no TransformComponent");
            return;
        }

        // 2. Update RigidBody Actor if it exists
        // We no longer 'return' if this is missing, allowing Controllers to work.
        if (s_Ctx->scene.any_of<RigidBodyComponent>(e)) {
            auto& rb = s_Ctx->scene.get<RigidBodyComponent>(e).RigidBody;
            if (rb.actor) {
                PxTransform currentPose = rb.actor->getGlobalPose();

                // Convert yaw to quaternion (assuming Y-up world)
                float yawRadians = glm::radians(yawDegrees);
                PxQuat newRotation = PxQuat(yawRadians, PxVec3(0, 1, 0));
                PxTransform newPose(currentPose.p, newRotation);

                if (PxRigidDynamic* dyn = rb.actor->is<PxRigidDynamic>()) {
                    if (dyn->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) {
                        dyn->setKinematicTarget(newPose);
                    }
                    else {
                        dyn->setGlobalPose(newPose);
                    }
                }
                else {
                    rb.actor->setGlobalPose(newPose);
                }
            }
        }
    }

    // Add new overload that ignores two entities
    static bool ICALL_API_LinecastIgnoreBoth(glm::vec3* from, glm::vec3* to, uint64_t ignoreEntityHandle1, uint64_t ignoreEntityHandle2)
    {
        if (!s_Ctx || !from || !to) return true;
        if (!s_Ctx->physics) return true;

        PxScene* scene = s_Ctx->physics->GetPxScene();
        if (!scene) return true;

        glm::vec3 delta = *to - *from;
        float len2 = glm::dot(delta, delta);
        if (len2 < 1e-6f) return true;

        float len = std::sqrt(len2);

        PxVec3 origin(from->x, from->y + 0.15f, from->z);
        PxVec3 dir(delta.x / len, delta.y / len, delta.z / len);

        PxQueryFilterData filter;
        filter.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;

        entt::entity ignoreEntity1 = static_cast<entt::entity>(static_cast<uint32_t>(ignoreEntityHandle1));
        entt::entity ignoreEntity2 = static_cast<entt::entity>(static_cast<uint32_t>(ignoreEntityHandle2));

        struct PreFilter : PxQueryFilterCallback {
            entt::entity entityToIgnore1;
            entt::entity entityToIgnore2;

            PreFilter(entt::entity ignore1, entt::entity ignore2)
                : entityToIgnore1(ignore1), entityToIgnore2(ignore2) {
            }

            PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* shape,
                const PxRigidActor* actor, PxHitFlags&) override
            {
                if (shape->getFlags() & PxShapeFlag::eTRIGGER_SHAPE)
                    return PxQueryHitType::eNONE;

                if (actor && actor->userData)
                {
                    EntityID* entityID = static_cast<EntityID*>(actor->userData);
                    if (entityID && (*entityID == entityToIgnore1 || *entityID == entityToIgnore2))
                        return PxQueryHitType::eNONE;
                }

                return PxQueryHitType::eBLOCK;
            }

            PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
            {
                return PxQueryHitType::eBLOCK;
            }
        } preFilter(ignoreEntity1, ignoreEntity2);

        PxRaycastBuffer hit;
        bool blocked = scene->raycast(origin, dir, len, hit, PxHitFlag::eDEFAULT, filter, &preFilter);

        // If nothing was hit, line is clear
        if (!blocked) return true;

        // If we hit something, check if it's beyond the target (we hit the target itself)
        // Use a smaller epsilon to be more strict
        const float epsilon = 0.05f;
        if (hit.block.distance >= len - epsilon)
            return true;

        // Something blocked the ray before reaching the target
        return false;
    }

    static uint64_t ICALL_API_Raycast(glm::vec3* from, glm::vec3* dir, float maxDist)
    {
        if (!s_Ctx || !from || !dir) return 0;
        if (!s_Ctx->physics) return 0;

        auto result = s_Ctx->physics->Raycast(*from, *dir, maxDist);
        if (result.hitFound && result.hitEntity != entt::null) {
            return static_cast<uint64_t>(static_cast<uint32_t>(result.hitEntity));
        }
        return 0;
    }

    static void ICALL_API_TeleportRigidBody(uint64_t handle, glm::vec3* pos) {
        if (!pos || !s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (e == entt::null || !s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] TeleportRigidBody: Invalid entity");
            return;
        }

        if (!s_Ctx->scene.any_of<TransformComponent>(e)) {
            BOOM_WARN("[ScriptBinding] TeleportRigidBody: Entity has no TransformComponent");
            return;
        }

        // Update transform component
        auto& transform = s_Ctx->scene.get<TransformComponent>(e).transform;
        transform.translate = *pos;

        // If entity has rigid body, update PhysX actor position
        if (s_Ctx->scene.any_of<RigidBodyComponent>(e)) {
            auto& rb = s_Ctx->scene.get<RigidBodyComponent>(e).RigidBody;

            if (rb.actor) {
                // Get current pose to preserve rotation
                PxTransform currentPose = rb.actor->getGlobalPose();

                // Create new pose with updated position
                PxTransform newPose(PxVec3(pos->x, pos->y, pos->z), currentPose.q);

                // Set the global pose
                if (PxRigidDynamic* dyn = rb.actor->is<PxRigidDynamic>()) {
                    // Clear velocities to prevent momentum carry-over
                    dyn->setLinearVelocity(PxVec3(0.0f, 0.0f, 0.0f));
                    dyn->setAngularVelocity(PxVec3(0.0f, 0.0f, 0.0f));

                    // Teleport the actor
                    dyn->setGlobalPose(newPose);

                    // Wake the actor up in case it was sleeping
                    dyn->wakeUp();

                    BOOM_INFO("[ScriptBinding] Teleported rigid body to ({}, {}, {})", pos->x, pos->y, pos->z);
                }
                else {
                    // Static or kinematic body
                    rb.actor->setGlobalPose(newPose);
                }
            }
        }
    }

    static void ICALL_API_SetSpriteTexture(uint64_t handle, MonoString* texturePathMono)
    {
        if (!s_Ctx || !s_Ctx->app || !texturePathMono) return;

        // 1. Get the AppInterface (which has the asset system)
        // This is based on the m_App field in your DirectoryPanel.
        auto* app = static_cast<Boom::AppInterface*>(s_Ctx->app);
        if (!app) {
            BOOM_WARN("[ScriptBinding] SetSpriteTexture: AppInterface is null.");
            return;
        }

        // 2. Get the entity and check for SpriteComponent
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.valid(e)) {
            BOOM_WARN("[ScriptBinding] SetSpriteTexture: Invalid entity handle");
            return;
        }
        if (!s_Ctx->scene.any_of<SpriteComponent>(e)) {
            BOOM_WARN("[ScriptBinding] SetSpriteTexture: Entity has no SpriteComponent");
            return;
        }

        // 3. Convert path from MonoString to std::filesystem::path
        char* texturePathUTF8 = mono_string_to_utf8(texturePathMono);
        std::string texturePathStr(texturePathUTF8);
        mono_free(texturePathUTF8);
        std::filesystem::path texturePath(texturePathStr);

        // 4. Get the AssetID (hash) from the path
        // This matches the logic in DirectoryPanel::RegisterAsset
        AssetID textureAssetID = app->AssetIDFromPath(texturePath);
        if (textureAssetID == EMPTY_ASSET) {
            BOOM_WARN("[ScriptBinding] SetSpriteTexture: Could not generate valid AssetID for path '{}'", texturePathStr);
            return;
        }

        // 5. Check if the asset is already in the registry. If not, add it.
        // This logic is copied from your DirectoryPanel::RegisterAsset
        auto& registry = app->GetAssetRegistry();
        if (registry.Get<TextureAsset>(textureAssetID).uid == EMPTY_ASSET)
        {
            BOOM_INFO("[ScriptBinding] Texture '{}' not in registry. Adding it now...", texturePathStr);

            // Call AddTexture, just like in your DirectoryPanel
            registry.AddTexture(textureAssetID, texturePath.generic_string());

            // Check if it was added successfully
            if (registry.Get<TextureAsset>(textureAssetID).uid == EMPTY_ASSET) {
                BOOM_WARN("[ScriptBinding] SetSpriteTexture: FAILED to load and add texture at path '{}'", texturePathStr);
                return;
            }
        }

        // 6. Get the component and set its textureID
        // We assume SpriteComponent.textureID stores the AssetID (the hash)
        auto& sprite = s_Ctx->scene.get<SpriteComponent>(e);
        sprite.textureID = textureAssetID;
    }

    // Video
    static void ICALL_API_PlayVideoComponent(uint64_t handle) {
        if (!s_Ctx || !s_Ctx->videoSystem) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        // Call Play on the video system
        s_Ctx->videoSystem->Play(e);
    }

    static void ICALL_API_StopVideoComponent(uint64_t handle) {
        if (!s_Ctx || !s_Ctx->videoSystem) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        // Call Stop on the video system
        s_Ctx->videoSystem->Stop(e);
    }

    // Get the current viewport size (handles both Editor and Standalone)
    static void ICALL_API_GetViewportSize(float* width, float* height) {
        if (!s_Ctx || !s_Ctx->window) {
            *width = 1.0f; *height = 1.0f;
            return;
        }

        auto* win = s_Ctx->window.get();
        float vW = win->GetViewportW();
        float vH = win->GetViewportH();

        // Standalone fallback (if viewport is not set by editor)
        if (vW <= 1.0f || vH <= 1.0f) {
            vW = (float)win->getWidth();
            vH = (float)win->getHeight();
        }

        *width = vW;
        *height = vH;
    }

    static void ICALL_API_SetVideoRemoveBlack(uint64_t handle, bool enabled) {
        if (!s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (s_Ctx->scene.any_of<VideoComponent>(e)) {
            auto& vc = s_Ctx->scene.get<VideoComponent>(e);
            vc.removeBlackBackground = enabled;
        }
    }

    static void ICALL_API_SetVideoBrightness(uint64_t handle, float value) {
        if (!s_Ctx) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (s_Ctx->scene.any_of<VideoComponent>(e)) {
            auto& vc = s_Ctx->scene.get<VideoComponent>(e);
            vc.brightness = glm::clamp(value, 0.0f, 10.0f);
        }
    }

    static float ICALL_API_GetVideoBrightness(uint64_t handle) {
        if (!s_Ctx) return 1.0f;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));

        if (s_Ctx->scene.any_of<VideoComponent>(e)) {
            auto& vc = s_Ctx->scene.get<VideoComponent>(e);
            return vc.brightness;
        }
        return 1.0f;
    }

    // Cutscene
    static void ICALL_API_SetCutsceneMode(bool active) {
        if (s_Ctx && s_Ctx->app) {
            s_Ctx->app->SetCutsceneMode(active);
            BOOM_INFO("[ScriptBinding] SetCutsceneMode called: {}", active ? "TRUE" : "FALSE");
        }
        else {
            BOOM_ERROR("[ScriptBinding] SetCutsceneMode FAILED: Context or App is null!");
        }
    }

    static void ICALL_API_DrawDebugLine(Boom::Vec3 start, Boom::Vec3 end, Boom::Vec3 color) {
        if (s_Ctx && s_Ctx->app) {
            s_Ctx->app->DrawScriptLine(
                glm::vec3(start.x, start.y, start.z),
                glm::vec3(end.x, end.y, end.z),
                glm::vec3(color.x, color.y, color.z)
            );
        }
    }

    static void ICALL_API_ShutdownApplication()
    {
        if (!s_Ctx || !s_Ctx->window)
        {
            BOOM_WARN("[ScriptBinding] ShutdownApplication: No context or window!");
            return;
        }

        BOOM_INFO("[ScriptBinding] ShutdownApplication called. Closing main window.");

        // Get the native GLFW window handle
        GLFWwindow* glfwWindow = s_Ctx->window->Handle().get();

        // Tell GLFW to close the window. The main loop will catch this and exit.
        glfwSetWindowShouldClose(glfwWindow, GLFW_TRUE);
    }

    // Global fade alpha (0 = transparent, 1 = black)
    BOOM_API float g_ScreenFadeAlpha = 0.0f;

    // C# -> C++ internal call (sets global fade)
    static void ICALL_API_SetScreenFadeAlpha(float alpha)
    {
        g_ScreenFadeAlpha = glm::clamp(alpha, 0.0f, 1.0f);
    }

    static void ICALL_API_Set30FPSLimit(bool enabled)
    {
        if (s_Ctx && s_Ctx->app) {
            static_cast<Application*>(s_Ctx->app)->m_Is30FPSLimit = enabled;
            BOOM_INFO("[Script] 30 FPS Limit: {}", enabled ? "ENABLED" : "DISABLED");
        }
    }

    static bool ICALL_API_Get30FPSLimit()
    {
        if (s_Ctx && s_Ctx->app) {
            return static_cast<Application*>(s_Ctx->app)->m_Is30FPSLimit;
        }
        return false;
    }

    void RegisterScriptInternalCalls(AppContext* ctx)
    {
        s_Ctx = ctx;

        ClearAllTriggerCallbacks();

        // IMPORTANT: These namespaces MUST match the C# side (Boom.Native)
        mono_add_internal_call("Boom.Native::Boom_API_Log", (const void*)ICALL_API_Log);
        mono_add_internal_call("Boom.Native::Boom_API_Set30FPSLimit", (const void*)ICALL_API_Set30FPSLimit);
        mono_add_internal_call("Boom.Native::Boom_API_Get30FPSLimit", (const void*)ICALL_API_Get30FPSLimit);
        mono_add_internal_call("Boom.Native::Boom_API_FindEntity", (const void*)ICALL_API_FindEntity);
        mono_add_internal_call("Boom.Native::Boom_API_GetPosition", (const void*)ICALL_API_GetPosition);
        mono_add_internal_call("Boom.Native::Boom_API_SetPosition", (const void*)ICALL_API_SetPosition);
        mono_add_internal_call("Boom.Native::Boom_API_GetRotation", (const void*)ICALL_API_GetRotation);
        mono_add_internal_call("Boom.Native::Boom_API_SetRotation", (const void*)ICALL_API_SetRotation);
        mono_add_internal_call("Boom.Native::Boom_API_IsKeyDown", (const void*)ICALL_API_IsKeyDown);
        mono_add_internal_call("Boom.Native::Boom_API_IsMouseDown", (const void*)ICALL_API_IsMouseDown);
        mono_add_internal_call("Boom.Native::Boom_API_IsGamepadButtonDown", (const void*)ICALL_API_IsGamepadButtonDown);
        mono_add_internal_call("Boom.Native::Boom_API_GetGamepadAxis", (const void*)ICALL_API_GetGamepadAxis);
        mono_add_internal_call("Boom.Native::Boom_API_IsGamepadConnected", (const void*)ICALL_API_IsGamepadConnected);

        mono_add_internal_call("Boom.Native::Boom_API_LoadScene", (const void*)ICALL_API_LoadScene);
        mono_add_internal_call("Boom.Native::Boom_API_GetCurrentSceneName", (const void*)ICALL_API_GetCurrentSceneName);
        mono_add_internal_call("Boom.Native::Boom_API_QuitGame", (const void*)ICALL_API_QuitGame);
        mono_add_internal_call("Boom.Native::Boom_API_ShutdownApplication", (const void*)ICALL_API_ShutdownApplication); // CORRECT QUIT
        
        mono_add_internal_call("Boom.Native::Boom_API_LoadSceneAdditive", (const void*)ICALL_API_LoadSceneAdditive);
        // Pause Menu
        mono_add_internal_call("Boom.Native::Boom_API_UnloadPauseMenu", (const void*)ICALL_API_UnloadPauseMenu);
        mono_add_internal_call("Boom.Native::Boom_API_ShowPauseMenu", (const void*)ICALL_API_ShowPauseMenu);
        mono_add_internal_call("Boom.Native::Boom_API_IsPauseMenuLoaded", (const void*)ICALL_API_IsPauseMenuLoaded);
        mono_add_internal_call("Boom.Native::Boom_API_SetGameLogicPaused", (const void*)ICALL_API_SetGameLogicPaused);
        mono_add_internal_call("Boom.Native::Boom_API_SetGroupVolume", (const void*)ICALL_API_SetGroupVolume);
        mono_add_internal_call("Boom.Native::Boom_API_GetGroupVolume", (const void*)ICALL_API_GetGroupVolume);

        // Death Menu
        mono_add_internal_call("Boom.Native::Boom_API_UnloadDeathMenu", (const void*)ICALL_API_UnloadDeathMenu);
        mono_add_internal_call("Boom.Native::Boom_API_ShowDeathMenu", (const void*)ICALL_API_ShowDeathMenu);
        mono_add_internal_call("Boom.Native::Boom_API_IsDeathMenuLoaded", (const void*)ICALL_API_IsDeathMenuLoaded);
        mono_add_internal_call("Boom.Native::Boom_API_SetPlayerDead", (const void*)ICALL_API_SetPlayerDead);

        // End Menu
        mono_add_internal_call("Boom.Native::Boom_API_UnloadEndMenu", (const void*)ICALL_API_UnloadEndMenu);
        mono_add_internal_call("Boom.Native::Boom_API_ShowEndMenu", (const void*)ICALL_API_ShowEndMenu);
        mono_add_internal_call("Boom.Native::Boom_API_IsEndMenuLoaded", (const void*)ICALL_API_IsEndMenuLoaded);
        mono_add_internal_call("Boom.Native::Boom_API_SetGameEnd", (const void*)ICALL_API_SetGameEnd);

        // Freeze
        mono_add_internal_call("Boom.Native::Boom_API_DestroyEntity", (const void*)ICALL_API_DestroyEntity);

        mono_add_internal_call("Boom.Native::Boom_API_TogglePause", (const void*)ICALL_API_TogglePause);
        mono_add_internal_call("Boom.Native::Boom_API_GetApplicationState", (const void*)ICALL_API_GetApplicationState);
        
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

        mono_add_internal_call("Boom.Native::Boom_API_MoveController", (void*)Boom_API_MoveController);
        mono_add_internal_call("Boom.Native::Boom_API_IsControllerGrounded", (void*)Boom_API_IsControllerGrounded);
        // AI Component functions
        mono_add_internal_call("Boom.Native::Boom_API_AI_GetPatrolPointCount",
            (const void*)ICALL_API_AI_GetPatrolPointCount);
        mono_add_internal_call("Boom.Native::Boom_API_AI_GetPatrolPoint",
            (const void*)ICALL_API_AI_GetPatrolPoint);
        mono_add_internal_call("Boom.Native::Boom_API_AI_GetMode",
            (const void*)ICALL_API_AI_GetMode);
        mono_add_internal_call("Boom.Native::Boom_API_SetNavAgentActive",
            (const void*)ICALL_API_SetNavAgentActive);
        mono_add_internal_call("Boom.Native::Boom_API_SetNavAgentPosition",
            (const void*)ICALL_API_SetNavAgentPosition);


        // Animator function
        mono_add_internal_call("Boom.Native::Boom_API_AnimatorSetFloat", (const void*)ICALL_API_AnimatorSetFloat);
        mono_add_internal_call("Boom.Native::Boom_API_AnimatorSetBool", (const void*)ICALL_API_AnimatorSetBool);
        mono_add_internal_call("Boom.Native::Boom_API_AnimatorSetTrigger", (const void*)ICALL_API_AnimatorSetTrigger);
        mono_add_internal_call("Boom.Native::Boom_API_AnimatorPlay", (const void*)ICALL_API_AnimatorPlay);
        mono_add_internal_call("Boom.Native::Boom_API_AnimatorSetStateMachineEnabled", (const void*)ICALL_API_AnimatorSetStateMachineEnabled);
        mono_add_internal_call("Boom.Native::Boom_API_GetThirdPersonCameraYaw", (const void*)ICALL_API_GetThirdPersonCameraYaw);

        mono_add_internal_call("Boom.Native::Boom_API_HasCollider", (const void*)ICALL_API_HasCollider);
        mono_add_internal_call("Boom.Native::Boom_API_IsTrigger", (const void*)ICALL_API_IsTrigger);
        mono_add_internal_call("Boom.Native::Boom_API_SetTrigger", (const void*)ICALL_API_SetTrigger);
        mono_add_internal_call("Boom.Native::Boom_API_GetSurfaceType", (const void*)ICALL_API_GetSurfaceType);
        mono_add_internal_call("Boom.Native::Boom_API_RegisterTriggerEnterCallback", (const void*)ICALL_API_RegisterTriggerEnterCallback);
        mono_add_internal_call("Boom.Native::Boom_API_RegisterTriggerExitCallback", (const void*)ICALL_API_RegisterTriggerExitCallback);
        mono_add_internal_call("Boom.Native::Boom_API_UnregisterTriggerCallbacks", (const void*)ICALL_API_UnregisterTriggerCallbacks);
        mono_add_internal_call("Boom.Native::Boom_API_HasAnimator",
            (const void*)ICALL_API_HasAnimator);
        mono_add_internal_call("Boom.Native::Boom_API_GetTransform",
            (const void*)ICALL_API_GetTransform);
        mono_add_internal_call("Boom.Native::Boom_API_SetTransform",
            (const void*)ICALL_API_SetTransform);

        // Sound/Audio internal calls
        mono_add_internal_call("Boom.Native::Boom_API_PlaySound", (const void*)ICALL_API_PlaySound);
        mono_add_internal_call("Boom.Native::Boom_API_PlaySoundAt", (const void*)ICALL_API_PlaySoundAt);
        mono_add_internal_call("Boom.Native::Boom_API_StopSound", (const void*)ICALL_API_StopSound);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundVolume", (const void*)ICALL_API_SetSoundVolume);
        mono_add_internal_call("Boom.Native::Boom_API_IsSoundPlaying", (const void*)ICALL_API_IsSoundPlaying);
        mono_add_internal_call("Boom.Native::Boom_API_PauseSound", (const void*)ICALL_API_PauseSound);
        mono_add_internal_call("Boom.Native::Boom_API_PreloadSound", (const void*)ICALL_API_PreloadSound);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundPosition", (const void*)ICALL_API_SetSoundPosition);
        mono_add_internal_call("Boom.Native::Boom_API_Set3DMinMaxDistance", (const void*)ICALL_API_Set3DMinMaxDistance);

        // Sound Component manipulation internal calls
        mono_add_internal_call("Boom.Native::Boom_API_HasSoundComponent", (const void*)ICALL_API_HasSoundComponent);
        mono_add_internal_call("Boom.Native::Boom_API_AddSoundComponent", (const void*)ICALL_API_AddSoundComponent);
        mono_add_internal_call("Boom.Native::Boom_API_GetSoundEntryCount", (const void*)ICALL_API_GetSoundEntryCount);
        mono_add_internal_call("Boom.Native::Boom_API_AddSoundEntry", (const void*)ICALL_API_AddSoundEntry);
        mono_add_internal_call("Boom.Native::Boom_API_RemoveSoundEntry", (const void*)ICALL_API_RemoveSoundEntry);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundEntryVolume", (const void*)ICALL_API_SetSoundEntryVolume);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundEntryPitch", (const void*)ICALL_API_SetSoundEntryPitch);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundEntryLoop", (const void*)ICALL_API_SetSoundEntryLoop);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundEntryMute", (const void*)ICALL_API_SetSoundEntryMute);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundEntryPan", (const void*)ICALL_API_SetSoundEntryPan);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundEntrySpatialBlend", (const void*)ICALL_API_SetSoundEntrySpatialBlend);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundEntryPriority", (const void*)ICALL_API_SetSoundEntryPriority);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundEntry3DDistance", (const void*)ICALL_API_SetSoundEntry3DDistance);
        mono_add_internal_call("Boom.Native::Boom_API_SetSoundEntryPlayOnStart", (const void*)ICALL_API_SetSoundEntryPlayOnStart);
        mono_add_internal_call("Boom.Native::Boom_API_PlaySoundEntry", (const void*)ICALL_API_PlaySoundEntry);
        mono_add_internal_call("Boom.Native::Boom_API_StopSoundEntry", (const void*)ICALL_API_StopSoundEntry);

		//Raycasting internal call
        mono_add_internal_call("Boom.Native::Boom_API_PickGameEntity", (const void*)ICALL_API_PickGameEntity);
        mono_add_internal_call("Boom.Native::Boom_API_GetMousePosInViewport", (const void*)ICALL_API_GetMousePosInViewport);
        mono_add_internal_call("Boom.Native::Boom_API_ProjectWorldToViewport", (const void*)ICALL_API_ProjectWorldToViewport);
        mono_add_internal_call("Boom.Native::Boom_API_Check2DViewportClick", (const void*)ICALL_API_Check2DViewportClick);

        mono_add_internal_call("Boom.Native::Boom_API_Raycast", (const void*)ICALL_API_Raycast);
        mono_add_internal_call("Boom.Native::Boom_API_Linecast", (const void*)ICALL_API_Linecast);

        mono_add_internal_call("Boom.Native::Boom_API_SetRotationY", (const void*)ICALL_API_SetRotationY);
    
        mono_add_internal_call("Boom.Native::Boom_API_LinecastIgnoreBoth",(const void*)ICALL_API_LinecastIgnoreBoth);

        mono_add_internal_call("Boom.Native::Boom_API_EnableFileWatcher", (const void*)ICALL_API_EnableFileWatcher);

        mono_add_internal_call("Boom.Native::Boom_API_TeleportRigidBody", (void*)ICALL_API_TeleportRigidBody);

        mono_add_internal_call("Boom.Native::Boom_API_SetScreenFadeAlpha", (const void*)ICALL_API_SetScreenFadeAlpha);

		// Physics Controller internal calls
        mono_add_internal_call("Boom.Native::Boom_API_CreateController", (const void*)ICALL_API_CreateController);
        mono_add_internal_call("Boom.Native::Boom_API_TeleportController", (void*)ICALL_API_TeleportController);
        mono_add_internal_call("Boom.Native::Boom_API_GetControllerTriggerOverlaps", (const void*)ICALL_API_GetControllerTriggerOverlaps);
        mono_add_internal_call("Boom.Native::Boom_API_GetControllerStandingOn", (const void*)ICALL_API_GetControllerStandingOn);

        // Sprite component internal calls
        mono_add_internal_call("Boom.Native::Boom_API_HasSprite", (const void*)ICALL_API_HasSprite);
        mono_add_internal_call("Boom.Native::Boom_API_GetSpriteColor", (const void*)ICALL_API_GetSpriteColor);
        mono_add_internal_call("Boom.Native::Boom_API_SetSpriteColor", (const void*)ICALL_API_SetSpriteColor);
        mono_add_internal_call("Boom.Native::Boom_API_GetSpriteAlpha", (const void*)ICALL_API_GetSpriteAlpha);
        mono_add_internal_call("Boom.Native::Boom_API_SetSpriteAlpha", (const void*)ICALL_API_SetSpriteAlpha);
        mono_add_internal_call("Boom.Native::Boom_API_SetSpriteTexture", (const void*)ICALL_API_SetSpriteTexture);

        // Text Component functions
        mono_add_internal_call("Boom.Native::Boom_API_HasText", (const void*)ICALL_API_HasText);
        mono_add_internal_call("Boom.Native::Boom_API_GetText", (const void*)ICALL_API_GetText);
        mono_add_internal_call("Boom.Native::Boom_API_SetText", (const void*)ICALL_API_SetText);
        mono_add_internal_call("Boom.Native::Boom_API_GetTextColor", (const void*)ICALL_API_GetTextColor);
        mono_add_internal_call("Boom.Native::Boom_API_SetTextColor", (const void*)ICALL_API_SetTextColor);
        mono_add_internal_call("Boom.Native::Boom_API_GetTextScale", (const void*)ICALL_API_GetTextScale);
        mono_add_internal_call("Boom.Native::Boom_API_SetTextScale", (const void*)ICALL_API_SetTextScale);
        mono_add_internal_call("Boom.Native::Boom_API_GetTextPosition", (const void*)ICALL_API_GetTextPosition);
        mono_add_internal_call("Boom.Native::Boom_API_SetTextPosition", (const void*)ICALL_API_SetTextPosition);

        // SpotLight component internal calls
        mono_add_internal_call("Boom.Native::Boom_API_HasSpotLight", (const void*)ICALL_API_HasSpotLight);
        mono_add_internal_call("Boom.Native::Boom_API_GetSpotLightColor", (const void*)ICALL_API_GetSpotLightColor);
        mono_add_internal_call("Boom.Native::Boom_API_SetSpotLightColor", (const void*)ICALL_API_SetSpotLightColor);
        mono_add_internal_call("Boom.Native::Boom_API_GetSpotLightIntensity", (const void*)ICALL_API_GetSpotLightIntensity);
        mono_add_internal_call("Boom.Native::Boom_API_SetSpotLightIntensity", (const void*)ICALL_API_SetSpotLightIntensity);

        // Video component internal calls
        mono_add_internal_call("Boom.Native::Boom_API_HasVideoComponent", (const void*)ICALL_API_HasVideoComponent);
        mono_add_internal_call("Boom.Native::Boom_API_IsVideoPlaying", (const void*)ICALL_API_IsVideoPlaying);
        mono_add_internal_call("Boom.Native::Boom_API_HasVideoEnded", (const void*)ICALL_API_HasVideoEnded);
        mono_add_internal_call("Boom.Native::Boom_API_PlayVideo", (const void*)ICALL_API_PlayVideo);
        mono_add_internal_call("Boom.Native::Boom_API_StopVideo", (const void*)ICALL_API_StopVideo);
        mono_add_internal_call("Boom.Native::Boom_API_GetVideoDuration", (const void*)ICALL_API_GetVideoDuration);
        mono_add_internal_call("Boom.Native::Boom_API_GetVideoCurrentTime", (const void*)ICALL_API_GetVideoCurrentTime);
        // Video
        mono_add_internal_call("Boom.Native::Boom_API_PlayVideoComponent", (void*)ICALL_API_PlayVideoComponent);
        mono_add_internal_call("Boom.Native::Boom_API_StopVideoComponent", (void*)ICALL_API_StopVideoComponent);
        mono_add_internal_call("Boom.Native::Boom_API_GetViewportSize", (void*)ICALL_API_GetViewportSize);
        mono_add_internal_call("Boom.Native::Boom_API_SetVideoRemoveBlack", (void*)ICALL_API_SetVideoRemoveBlack);
        mono_add_internal_call("Boom.Native::Boom_API_SetVideoBrightness", (void*)ICALL_API_SetVideoBrightness);
        mono_add_internal_call("Boom.Native::Boom_API_GetVideoBrightness", (void*)ICALL_API_GetVideoBrightness);

        // Cutscene
        mono_add_internal_call("Boom.Native::Boom_API_SetCutsceneMode", (const void*)ICALL_API_SetCutsceneMode);
        mono_add_internal_call("Boom.Native::Boom_API_DrawDebugLine", (const void*)ICALL_API_DrawDebugLine);

    }
}