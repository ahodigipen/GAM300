#pragma once

#include "BehaviourTree.h"
#include "Actions.h"
#include "ECS/ECS.hpp"

namespace Boom {

    // -
    // Patrol + Seek (Auto mode)
    // 
    BOOM_INLINE BTNodePtr BuildPatrolSeekTree()
    {
        // Chase branch: See (detect/lose logic) -> Seek (follow player)
        auto chaseSeq = std::make_unique<Sequence>();
        chaseSeq->children.emplace_back(std::make_unique<SeePlayerCond>());
        chaseSeq->children.emplace_back(std::make_unique<SeekPlayerAction>());

        // Patrol branch: Patrol -> (Idle with cooldown)
        auto patrolSeq = std::make_unique<Sequence>();
        patrolSeq->children.emplace_back(std::make_unique<PatrolAction>());
        patrolSeq->children.emplace_back(
            std::make_unique<Cooldown>(std::make_unique<IdleAction>(), 0.0f));

        // Root selector: try chase, else patrol
        auto root = std::make_unique<Selector>();
        root->children.emplace_back(std::move(chaseSeq));
        root->children.emplace_back(std::move(patrolSeq));
        return root;
    }

    // 
    // Idle-only tree (AIMode::Idle)
    // 
    BOOM_INLINE BTNodePtr BuildIdleTree()
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.emplace_back(std::make_unique<IdleAction>());
        return seq;
    }

    // 
    // Patrol-only tree (AIMode::Patrol)
    // 
    BOOM_INLINE BTNodePtr BuildPatrolOnlyTree()
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.emplace_back(std::make_unique<PatrolAction>());
        seq->children.emplace_back(
            std::make_unique<Cooldown>(std::make_unique<IdleAction>(), 0.0f));
        return seq;
    }

    // 
    // Seek-only tree (AIMode::Seek)
    // 
    BOOM_INLINE BTNodePtr BuildSeekOnlyTree()
    {
        auto seq = std::make_unique<Sequence>();
        seq->children.emplace_back(std::make_unique<SeePlayerCond>());
        seq->children.emplace_back(std::make_unique<SeekPlayerAction>());
        seq->children.emplace_back(std::make_unique<StillChasingCond>());
        return seq;
    }

    BOOM_INLINE BTNodePtr BuildTreeForMode(AIComponent::AIMode mode)
    {
        switch (mode)
        {
        case AIComponent::AIMode::Idle:   return BuildIdleTree();
        case AIComponent::AIMode::Patrol: return BuildPatrolOnlyTree();
        case AIComponent::AIMode::Seek:   return BuildSeekOnlyTree();
        case AIComponent::AIMode::Auto:
        default:
            return BuildPatrolSeekTree();
        }
    }

} // namespace Boom
