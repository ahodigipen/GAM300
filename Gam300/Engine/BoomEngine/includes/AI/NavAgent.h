#pragma once

#include <vector>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include "ECS/ECS.hpp"
#include "BoomProperties.h"
namespace Boom {




    class DetourNavSystem; // fwd

    class BOOM_API NavAgentSystem {
    public:
        void requestPath(entt::registry& reg, entt::entity e, DetourNavSystem& nav);

        void update(entt::registry& reg, float dt, DetourNavSystem& nav)
        {
            auto view = reg.view<TransformComponent, NavAgentComponent>();
            for (auto e : view) {
                auto& tr = view.get<TransformComponent>(e);
                auto& ag = view.get<NavAgentComponent>(e);

                if (!ag.active) {
                    ag.velocity = glm::vec3(0.f);
                    continue;
                }

                // Resolve followName to entity
                if (ag.follow == entt::null && !ag.followName.empty()) {
                    auto infoView = reg.view<InfoComponent>();
                    for (auto fe : infoView) {
                        const auto& info = infoView.get<InfoComponent>(fe);
                        if (info.name == ag.followName) {
                            ag.follow = fe;
                            ag.dirty = true;
                            ag.repathTimer = 0.f;
                            BOOM_INFO("[NavAgent] Found follow target: {}", ag.followName);
                            break;
                        }
                    }
                    if (ag.follow == entt::null) {
                        BOOM_WARN("[NavAgent] Could not find follow target: {}", ag.followName);
                    }
                }

                // FOLLOW mode: keep target synced to the followed entity
                if (ag.follow != entt::null &&
                    reg.valid(ag.follow) &&
                    reg.all_of<TransformComponent>(ag.follow))
                {
                    ag.repathTimer -= dt;
                    const glm::vec3 desired =
                        reg.get<TransformComponent>(ag.follow).transform.translate;

                    if (ag.repathTimer <= 0.f &&
                        glm::distance2(desired, ag.target) > ag.retargetDist * ag.retargetDist)
                    {
                        ag.target = desired;
                        ag.dirty = true;
                        ag.repathTimer = ag.repathCooldown;
                        BOOM_INFO("[NavAgent] Repathing to: ({}, {}, {})", desired.x, desired.y, desired.z);
                    }
                }

                if (ag.dirty) {
                    requestPath(reg, e, nav);
                    BOOM_INFO("[NavAgent] Path has {} waypoints", ag.path.size());
                }

                if (ag.path.empty() || ag.waypoint >= (int)ag.path.size()) {
                    ag.velocity = glm::vec3(0.f);
                    continue;
                }

                const glm::vec3 pos = tr.transform.translate;
                const glm::vec3 goal = ag.path[ag.waypoint];

                //  FIX: Use only XZ plane distance (ignore Y)
                const glm::vec3 posXZ = glm::vec3(pos.x, 0.0f, pos.z);
                const glm::vec3 goalXZ = glm::vec3(goal.x, 0.0f, goal.z);
                const glm::vec3 toXZ = goalXZ - posXZ;
                const float dXZ = glm::length(toXZ);

                // Check if we've reached the waypoint (horizontal distance only)
                if (dXZ <= ag.arrive) {
                    ++ag.waypoint;
                    if (ag.waypoint >= (int)ag.path.size()) {
                        ag.path.clear();
                        ag.velocity = glm::vec3(0.f);
                    }
                    continue;
                }

                //  Calculate direction in XZ plane only, keep Y = 0
                const glm::vec3 dirXZ = (dXZ > 0.f) ? (toXZ / dXZ) : glm::vec3(0);
                ag.velocity = dirXZ * ag.speed;

                BOOM_INFO("[NavAgent] Pos: ({:.2f}, {:.2f}, {:.2f}), Goal: ({:.2f}, {:.2f}, {:.2f}), Velocity: ({:.2f}, {:.2f}, {:.2f}), Dist: {:.2f}, Waypoint {}/{}",
                    pos.x, pos.y, pos.z, goal.x, goal.y, goal.z,
                    ag.velocity.x, ag.velocity.y, ag.velocity.z, dXZ, ag.waypoint, ag.path.size());
            }
        }
    };

} // namespace Boom