// AI/Actions.h
#pragma once
#include "NavAgent.h"
#include "BehaviourTree.h"
#include "AIComponent.h"
#include "ECS/ECS.hpp"
#include "glm/glm.hpp"

namespace Boom {

    struct SeePlayerCond : BTNode
    {
        BTState tick(entt::registry& reg, entt::entity e, float) override
        {
            if (!reg.all_of<AIComponent, TransformComponent>(e))
                return BTState::Failure;

            auto& ai = reg.get<AIComponent>(e);
            auto& tc = reg.get<TransformComponent>(e);

            // Resolve player by name if needed
            if (ai.player == entt::null && !ai.playerName.empty())
            {
                auto view = reg.view<InfoComponent>();
                for (auto ent : view)
                {
                    const auto& info = view.get<InfoComponent>(ent);
                    if (info.name == ai.playerName)
                    {
                        ai.player = ent;
                        break;
                    }
                }
            }

            if (ai.player == entt::null)
                return BTState::Failure;

            if (!reg.all_of<TransformComponent>(ai.player))
                return BTState::Failure;

            auto& ptc = reg.get<TransformComponent>(ai.player);

            glm::vec3 pos = tc.transform.translate;
            glm::vec3 target = ptc.transform.translate;
            float d2 = glm::distance2(pos, target);

            // Check if we are already chasing via NavAgent
            bool chasingNow = false;
            if (reg.all_of<NavAgentComponent>(e))
            {
                auto& ag = reg.get<NavAgentComponent>(e);
                chasingNow = (ag.follow == ai.player && ag.active);
            }

            // If not chasing yet -> use detectRadius
            // If already chasing -> use loseRadius (hysteresis)
            float radius = chasingNow ? ai.loseRadius : ai.detectRadius;
            float r2 = radius * radius;

            if (d2 <= r2)
            {
                // Still allowed to see/keep chasing
                return BTState::Success;
            }
            else
            {
                // If we were chasing and we are now outside loseRadius -> stop chasing
                if (chasingNow && reg.all_of<NavAgentComponent>(e))
                {
                    auto& ag = reg.get<NavAgentComponent>(e);
                    ag.follow = entt::null;

                    ag.dirty = true;
                    ag.repathTimer = 0.f;
                    ag.path.clear();
                    ag.waypoint = 0;
                }
                return BTState::Failure;
            }
        }
    };


    // When chasing, stop if we lost the player far enough
    struct StillChasingCond : BTNode {
        BTState tick(entt::registry& reg, entt::entity e, float) override {
            auto& tr = reg.get<TransformComponent>(e);
            auto& ai = reg.get<AIComponent>(e);

            if (ai.player == entt::null || !reg.valid(ai.player) || !reg.all_of<TransformComponent>(ai.player))
                return BTState::Failure;

            const glm::vec3 me = tr.transform.translate;
            const glm::vec3 pp = reg.get<TransformComponent>(ai.player).transform.translate;
            const float d2 = glm::distance2(me, pp);
            return (d2 <= ai.loseRadius * ai.loseRadius) ? BTState::Success : BTState::Failure;
        }
    };

    struct IdleAction : BTNode {
        BTState tick(entt::registry& reg, entt::entity e, float dt) override {
            auto& ai = reg.get<AIComponent>(e);
            ai.idleTimer -= dt;
            if (ai.idleTimer <= 0.f) return BTState::Success;
            return BTState::Running;
        }
    };

 
    // Drives NavAgent to walk patrol loop. 
// - Ensures there's always a path to the current patrol point.
// - Succeeds (and advances to next point) when we're within ArriveRadius in XZ.
    struct PatrolAction : BTNode {
        BTState tick(entt::registry& reg, entt::entity e, float) override {
            if (!reg.all_of<AIComponent, NavAgentComponent, TransformComponent>(e))
                return BTState::Failure;

            auto& ai = reg.get<AIComponent>(e);
            auto& ag = reg.get<NavAgentComponent>(e);
            auto& tr = reg.get<TransformComponent>(e);

            if (ai.patrolPoints.empty())
                return BTState::Failure;

            // Clamp patrol index
            if (ai.patrolIndex < 0 ||
                ai.patrolIndex >= static_cast<int>(ai.patrolPoints.size()))
            {
                ai.patrolIndex = 0;
            }

            const glm::vec3 patrolGoal = ai.patrolPoints[ai.patrolIndex];

            // 1) Ensure nav agent is going to THIS patrol point.
            //    Only mark dirty when target actually changes.
            if (ag.target != patrolGoal) {
                ag.follow = entt::null;      // patrol ignores follow
                ag.target = patrolGoal;
                ag.dirty = true;            // NavAgentSystem will rebuild path once
            }

            // 2) Check if we've reached the patrol point (XZ only, like NavAgent).
            const glm::vec3 pos = tr.transform.translate;
            const glm::vec3 posXZ = { pos.x, 0.0f, pos.z };
            const glm::vec3 goalXZ = { patrolGoal.x, 0.0f, patrolGoal.z };
            const float dXZ = glm::length(goalXZ - posXZ);

            if (dXZ <= ag.arrive) {
                // Reached this point: advance to next and start idle.
                ai.patrolIndex = (ai.patrolIndex + 1) %
                    static_cast<int>(ai.patrolPoints.size());
                ai.idleTimer = ai.idleWait;
				ag.dirty = true;  // force path rebuild to next point
                // Clean up current path so next leg gets a fresh one.
                ag.path.clear();
                ag.waypoint = 0;
                ag.velocity = glm::vec3(0.0f);

                return BTState::Success; // Sequence will move to IdleAction
            }

            // Still walking towards patrolGoal.
            return BTState::Running;
        }
    };


    // Make agent chase the player by wiring NavAgent.follow
    struct SeekPlayerAction : BTNode
    {
        BTState tick(entt::registry& reg, entt::entity e, float) override
        {
            if (!reg.all_of<AIComponent, TransformComponent>(e))
                return BTState::Failure;

            auto& ai = reg.get<AIComponent>(e);
            if (ai.player == entt::null)
                return BTState::Failure;

            if (!reg.all_of<NavAgentComponent>(e))
                return BTState::Failure;

            auto& ag = reg.get<NavAgentComponent>(e);

            // Follow that player using NavAgent
            if (ag.follow != ai.player || !ag.active)
            {
                ag.follow = ai.player;
                ag.active = true;
                ag.dirty = true;
                ag.repathTimer = 0.0f;
            }

            // Movement is handled by NavAgentSystem, so the BT action itself
            // can be considered "instantly done" this frame.
            return BTState::Success;
        }
    };


} // namespace Boom
