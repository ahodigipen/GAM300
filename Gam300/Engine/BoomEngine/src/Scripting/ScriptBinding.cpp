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

    static glm::vec3* ICALL_API_GetPosition(uint64_t handle, glm::vec3* outPos) {
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.any_of<TransformComponent>(e)) return nullptr;
        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;
        if (outPos) *outPos = t.translate;
        return outPos;
    }

    static void ICALL_API_SetPosition(uint64_t handle, glm::vec3* pos) {
        if (!pos) return;
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.any_of<TransformComponent>(e)) return;
        auto& t = s_Ctx->scene.get<TransformComponent>(e).transform;
        t.translate = *pos;
    }

    // Helper to get the PxRigidDynamic actor
    static physx::PxRigidDynamic* GetDynamicActor(uint64_t handle) {
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.any_of<RigidBodyComponent>(e)) return nullptr; 
        auto& rb = s_Ctx->scene.get<RigidBodyComponent>(e);
        if (!rb.RigidBody.actor) return nullptr;
        return rb.RigidBody.actor->is<physx::PxRigidDynamic>();
    }

    static glm::vec3* ICALL_API_GetLinearVelocity(uint64_t handle, glm::vec3* outVel) {
        auto* dyn = GetDynamicActor(handle);
        if (dyn && outVel) {
            physx::PxVec3 vel = dyn->getLinearVelocity();
            *outVel = glm::vec3(vel.x, vel.y, vel.z);
            return outVel;
        }
        return nullptr;
    }

    static void ICALL_API_SetLinearVelocity(uint64_t handle, glm::vec3* vel) {
        if (!vel) return;
        auto* dyn = GetDynamicActor(handle);
        if (dyn) {
            dyn->setLinearVelocity(physx::PxVec3(vel->x, vel->y, vel->z));
        }
    }

    static bool ICALL_API_IsColliding(uint64_t handle) {
        entt::entity e = static_cast<entt::entity>(static_cast<uint32_t>(handle));
        if (e == entt::null || !s_Ctx->scene.any_of<RigidBodyComponent>(e)) return false;
        // This flag is set to true by your physics event callback
        return s_Ctx->scene.get<RigidBodyComponent>(e).RigidBody.isColliding;
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



    void RegisterScriptInternalCalls(AppContext* ctx)
    {
        s_Ctx = ctx;

        // These names must match your C# InternalCall signatures in API.cs (namespace + class + method):
        //   Boom.Native::Boom_API_Log, Boom_API_FindEntity, Boom_API_GetPosition, Boom_API_SetPosition
        mono_add_internal_call("GameScripts.Native::Boom_API_Log", (const void*)ICALL_API_Log);
        mono_add_internal_call("GameScripts.Native::Boom_API_FindEntity", (const void*)ICALL_API_FindEntity);
        mono_add_internal_call("GameScripts.Native::Boom_API_GetPosition", (const void*)ICALL_API_GetPosition);
        mono_add_internal_call("GameScripts.Native::Boom_API_SetPosition", (const void*)ICALL_API_SetPosition);
        mono_add_internal_call("GameScripts.Native::Boom_API_GetLinearVelocity", (const void*)ICALL_API_GetLinearVelocity);
        mono_add_internal_call("GameScripts.Native::Boom_API_SetLinearVelocity", (const void*)ICALL_API_SetLinearVelocity);
        mono_add_internal_call("GameScripts.Native::Boom_API_IsColliding", (const void*)ICALL_API_IsColliding);
        mono_add_internal_call("GameScripts.Native::Boom_API_IsKeyDown", (const void*)ICALL_API_IsKeyDown);
        mono_add_internal_call("GameScripts.Native::Boom_API_IsMouseDown", (const void*)ICALL_API_IsMouseDown);

    }
}
