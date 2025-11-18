// AnimationEventRegistry.h
// Auto-registration system for animation event handlers
// Solves the "handlers lost on scene load" problem

#pragma once
#include "BoomEngine.h"
#include "Graphics/Models/Animator.h"
#include <functional>
#include <unordered_map>

namespace Boom {

/**
 * @brief Centralized registry for animation event handlers
 *
 * This solves the serialization problem: event handlers (function pointers)
 * cannot be saved to disk, so they must be re-registered when scenes load.
 *
 * Usage:
 *   1. Implement RegisterXXXHandlers() for each entity type
 *   2. Call RegisterForEntity() when entities are created
 *   3. Call RegisterAllInScene() after scene loads
 *
 * This is similar to Unity's MonoBehaviour system where methods
 * are re-connected via reflection after scene load.
 */
class AnimationEventRegistry
{
public:
    using EventCallback = std::function<void(const AnimationEvent&)>;

    /**
     * @brief Register event handlers for a specific entity
     * Automatically detects entity type and registers appropriate handlers
     */
    static void RegisterForEntity(Entity entity)
    {
        if (!entity.Has<AnimatorComponent>()) return;

        auto& animComp = entity.Get<AnimatorComponent>();
        if (!animComp.animator) return;

        // Determine entity type and register handlers
        // Add your component types here
        if (entity.Has<PlayerComponent>()) {
            RegisterPlayerHandlers(animComp.animator, entity);
        }
        // else if (entity.Has<EnemyComponent>()) {
        //     RegisterEnemyHandlers(animComp.animator, entity);
        // }
        // else if (entity.Has<NPCComponent>()) {
        //     RegisterNPCHandlers(animComp.animator, entity);
        // }

        BOOM_INFO("[EventRegistry] Registered handlers for entity {}", (uint32_t)entity);
    }

    /**
     * @brief Register handlers for ALL entities in a scene
     * Call this after scene load to restore all event handlers
     */
    static void RegisterAllInScene(EntityRegistry& registry)
    {
        auto view = registry.view<AnimatorComponent>();
        int count = 0;

        for (auto entityID : view) {
            Entity entity(entityID, &registry);
            RegisterForEntity(entity);
            count++;
        }

        BOOM_INFO("[EventRegistry] Registered handlers for {} entities", count);
    }

private:
    // ========================================================================
    // ENTITY-SPECIFIC HANDLER REGISTRATION
    // Implement these for your game's entity types
    // ========================================================================

    static void RegisterPlayerHandlers(std::shared_ptr<Animator> animator, Entity entity)
    {
        // Locomotion events
        animator->RegisterEventHandler("OnFootstepLeft",
            [entity](const AnimationEvent& e) {
                // Play left footstep sound
                // Audio::PlaySound("player_footstep_left.wav");

                // Spawn dust particle
                // ParticleSystem::Spawn("dust", entity.GetPosition());

                BOOM_INFO("Player: Left footstep at {:.2f}s", e.time);
            });

        animator->RegisterEventHandler("OnFootstepRight",
            [entity](const AnimationEvent& e) {
                // Play right footstep sound
                // Audio::PlaySound("player_footstep_right.wav");

                BOOM_INFO("Player: Right footstep at {:.2f}s", e.time);
            });

        animator->RegisterEventHandler("OnJumpLand",
            [entity](const AnimationEvent& e) {
                // Play landing sound
                // Audio::PlaySound("player_land.wav");

                // Camera shake
                // Camera::Shake(0.3f);

                BOOM_INFO("Player: Landed at {:.2f}s", e.time);
            });

        // Combat events
        animator->RegisterEventHandler("OnAttackStart",
            [entity](const AnimationEvent& e) {
                // Begin attack animation
                BOOM_INFO("Player: Attack started at {:.2f}s", e.time);
            });

        animator->RegisterEventHandler("OnAttackHit",
            [entity](const AnimationEvent& e) {
                // Enable weapon hitbox and check for hits
                int damage = e.intParameter;

                // if (auto* combat = entity.TryGet<CombatComponent>()) {
                //     combat->EnableWeaponHitbox(damage);
                //     combat->CheckForHits();
                // }

                BOOM_INFO("Player: Attack hit window (damage: {})", damage);
            });

        animator->RegisterEventHandler("OnAttackEnd",
            [entity](const AnimationEvent& e) {
                // Disable weapon hitbox
                // if (auto* combat = entity.TryGet<CombatComponent>()) {
                //     combat->DisableWeaponHitbox();
                // }

                BOOM_INFO("Player: Attack ended at {:.2f}s", e.time);
            });

        // VFX events
        animator->RegisterEventHandler("SpawnVFX",
            [entity](const AnimationEvent& e) {
                std::string vfxName = e.stringParameter;
                float intensity = e.floatParameter;

                // VFXSystem::Spawn(vfxName, entity.GetPosition(), intensity);

                BOOM_INFO("Player: Spawn VFX '{}' (intensity: {:.2f})", vfxName, intensity);
            });

        BOOM_INFO("[EventRegistry] Registered player handlers");
    }

    static void RegisterEnemyHandlers(std::shared_ptr<Animator> animator, Entity entity)
    {
        // Similar to player but with enemy-specific behavior
        animator->RegisterEventHandler("OnFootstep",
            [entity](const AnimationEvent& e) {
                // Different sound for enemy
                // Audio::PlaySound("enemy_footstep.wav");

                BOOM_INFO("Enemy: Footstep at {:.2f}s", e.time);
            });

        animator->RegisterEventHandler("OnAttackTelegraph",
            [entity](const AnimationEvent& e) {
                // Show attack warning VFX
                BOOM_INFO("Enemy: Telegraph attack at {:.2f}s", e.time);
            });

        animator->RegisterEventHandler("OnAttackHit",
            [entity](const AnimationEvent& e) {
                // Enemy attack logic
                BOOM_INFO("Enemy: Attack hit at {:.2f}s", e.time);
            });

        BOOM_INFO("[EventRegistry] Registered enemy handlers");
    }

    static void RegisterNPCHandlers(std::shared_ptr<Animator> animator, Entity entity)
    {
        // NPC-specific events (dialogue, gestures, etc.)
        animator->RegisterEventHandler("OnTalk",
            [entity](const AnimationEvent& e) {
                std::string dialogue = e.stringParameter;

                // Show dialogue UI
                // DialogueSystem::Show(dialogue);

                BOOM_INFO("NPC: Talk '{}' at {:.2f}s", dialogue, e.time);
            });

        animator->RegisterEventHandler("OnGesture",
            [entity](const AnimationEvent& e) {
                std::string gestureType = e.stringParameter;

                BOOM_INFO("NPC: Gesture '{}' at {:.2f}s", gestureType, e.time);
            });

        BOOM_INFO("[EventRegistry] Registered NPC handlers");
    }
};

} // namespace Boom

// ============================================================================
// USAGE EXAMPLE
// ============================================================================

/*
// In your Application.cpp or SceneManager.cpp:

void Application::OnSceneLoaded()
{
    // ... existing scene load code ...

    // Re-register all animation event handlers
    Boom::AnimationEventRegistry::RegisterAllInScene(ctx->scene);
}

void Application::OnEntityCreated(Boom::Entity entity)
{
    // ... existing entity creation code ...

    // Register handlers for this new entity
    Boom::AnimationEventRegistry::RegisterForEntity(entity);
}

// That's it! Now handlers are automatically restored after scene load.
*/

// ============================================================================
// ADDING EVENTS TO CLIPS (Do this once, events are saved)
// ============================================================================

/*
// In your entity initialization or asset loading:

void SetupPlayerAnimationEvents(std::shared_ptr<Boom::Animator> animator)
{
    // Walk animation
    auto* walkClip = const_cast<Boom::AnimationClip*>(animator->GetClip("Walk"));
    if (walkClip) {
        walkClip->AddEvent(0.3f, "OnFootstepLeft");
        walkClip->AddEvent(0.8f, "OnFootstepRight");
    }

    // Attack animation
    auto* attackClip = const_cast<Boom::AnimationClip*>(animator->GetClip("Attack"));
    if (attackClip) {
        attackClip->AddEvent(0.2f, "OnAttackStart");

        Boom::AnimationEvent hitEvent(0.5f, "OnAttackHit");
        hitEvent.intParameter = 50;  // Damage amount
        attackClip->events.push_back(hitEvent);

        attackClip->AddEvent(0.8f, "OnAttackEnd");
        attackClip->SortEvents();
    }

    // Events are now saved with the scene!
}
*/

// ============================================================================
// EVENT NAMING CONVENTIONS
// ============================================================================

/*
Recommended event naming patterns:

Common Events:
  - OnFootstep, OnFootstepLeft, OnFootstepRight
  - OnJumpStart, OnJumpApex, OnJumpLand
  - OnAttackStart, OnAttackHit, OnAttackEnd
  - OnDamageReceived
  - OnDeath

VFX Events:
  - SpawnVFX (use stringParameter for VFX name)
  - SpawnDustVFX, SpawnBloodVFX, etc.

Audio Events:
  - PlaySound (use stringParameter for sound name)
  - PlayVoiceLine

Gameplay Events:
  - EnableHitbox, DisableHitbox
  - EnableInvincibility, DisableInvincibility
  - CheckForPickup

Use stringParameter for variations:
  - Event: "PlaySound", stringParameter: "sword_swing_01.wav"
  - Event: "SpawnVFX", stringParameter: "explosion_large"

Use floatParameter for intensity/scale:
  - Event: "CameraShake", floatParameter: 0.5f (intensity)
  - Event: "SpawnVFX", floatParameter: 1.5f (scale)

Use intParameter for counts/IDs:
  - Event: "OnAttackHit", intParameter: 50 (damage)
  - Event: "SpawnProjectiles", intParameter: 3 (count)
*/
