#pragma once

#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include "Context/Context.h"
#include "ECS/ECS.hpp"

namespace EditorUI {

    class RayCast {
    public:
        RayCast(Boom::AppContext* context);

        // Perform ray casting from screen coordinates
        entt::entity CastRayFromScreen(float screenX, float screenY,
            const glm::mat4& viewMatrix,
            const glm::mat4& projectionMatrix,
            const glm::vec3& cameraPosition,
            const glm::vec2& viewportSize);

        // Get the intersection point and distance
        struct RayHit {
            entt::entity entity{ entt::null };
            float distance{ FLT_MAX };
            glm::vec3 point{ 0.0f };
            bool hit{ false };
        };

        RayHit GetClosestHit(const glm::vec3& rayOrigin,
            const glm::vec3& rayDirection);

    private:
        Boom::AppContext* m_Context;

        // Convert screen coordinates to world space ray
        glm::vec3 ScreenToWorldRay(float screenX, float screenY,
            const glm::mat4& viewMatrix,
            const glm::mat4& projectionMatrix,
            const glm::vec2& viewportSize);

        // Check if ray intersects with a specific entity (now returns hit distance)
        bool RayIntersectsEntity(entt::entity entity,
            const glm::vec3& rayOrigin,
            const glm::vec3& rayDirection,
            float& hitDistance);

        // Ray-AABB intersection test
        bool RayAABBIntersection(const glm::vec3& rayOrigin,
            const glm::vec3& rayDirection,
            const glm::vec3& aabbMin,
            const glm::vec3& aabbMax,
            float& t);

        // Convert Transform3D to world matrix
        glm::mat4 TransformToMatrix(const Boom::Transform3D& transform);

        // Get entity's bounding box in world space
        void GetEntityAABB(entt::entity entity, glm::vec3& aabbMin, glm::vec3& aabbMax);

        // Get model's bounding box from ModelComponent
        bool GetModelBounds(const Boom::ModelComponent& modelComp, glm::vec3& min, glm::vec3& max);
    };

} // namespace EditorUI