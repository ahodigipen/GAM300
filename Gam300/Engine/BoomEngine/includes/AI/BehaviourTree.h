#pragma once
#include "Core.h"
#include <entt/entt.hpp>

namespace Boom {
    enum class BTState { Success, Failure, Running };

    struct BTNode {
        virtual ~BTNode() = default;
        virtual BTState tick(entt::registry& reg, entt::entity e, float dt) = 0;
    };

    using BTNodePtr = std::unique_ptr<BTNode>;

    struct Selector : BTNode {
        std::vector<BTNodePtr> children;
        size_t current{ 0 };

        Selector() = default; // allow default-construct
        explicit Selector(std::vector<BTNodePtr> ch) : children(std::move(ch)) {}

        BTState tick(entt::registry& reg, entt::entity e, float dt) override {
            for (; current < children.size(); ++current) {
                auto s = children[current]->tick(reg, e, dt);
                if (s == BTState::Running) return BTState::Running;
                if (s == BTState::Success) { current = 0; return BTState::Success; }
            }
            current = 0; return BTState::Failure;
        }
    };

    struct Sequence : BTNode {
        std::vector<BTNodePtr> children;
        size_t current{ 0 };

        Sequence() = default;
        explicit Sequence(std::vector<BTNodePtr> ch) : children(std::move(ch)) {}

        BTState tick(entt::registry& reg, entt::entity e, float dt) override {
            for (; current < children.size(); ++current) {
                auto s = children[current]->tick(reg, e, dt);
                if (s != BTState::Success) return s;
            }
            current = 0; return BTState::Success;
        }
    };

    struct Cooldown : BTNode {
        BTNodePtr child;
        float timeLeft{ 0.f };
        float cooldownS{ 1.f };

        explicit Cooldown(BTNodePtr c, float cd)
            : child(std::move(c)), cooldownS(cd) {
        }

        BTState tick(entt::registry& reg, entt::entity e, float dt) override {
            if (timeLeft > 0.f) { timeLeft -= dt; return BTState::Failure; }
            auto s = child->tick(reg, e, dt);
            if (s == BTState::Success) timeLeft = cooldownS;
            return s;
        }
    };

} // namespace Boom
