# Animation Events - Usage Guide

Animation events allow you to trigger code at specific points during animation playback. This is essential for syncing gameplay events with animations (footsteps, VFX spawning, sound effects, etc.).

## Features

- ✅ Events fire at specific timestamps in animations
- ✅ Support for string, float, and int parameters
- ✅ Automatic handling of looping animations
- ✅ Events fire during blend trees (from both blended clips)
- ✅ Events fire during state transitions
- ✅ Full serialization support (saved/loaded with scenes)

## Basic Usage

### 1. Add Events to Animation Clips

```cpp
// Get the animator
auto& animatorComp = registry.get<AnimatorComponent>(entity);
auto& animator = animatorComp.animator;

// Get a specific clip (assuming clip index 0 is "Walk")
auto* clip = const_cast<AnimationClip*>(animator->GetClip(0));

// Add events at specific times (in seconds)
clip->AddEvent(0.3f, "OnFootstepLeft");   // Left foot touches ground at 0.3s
clip->AddEvent(0.8f, "OnFootstepRight");  // Right foot touches ground at 0.8s
clip->AddEvent(0.5f, "OnSwingWeapon");    // Weapon swing midpoint

// Events with parameters
AnimationEvent dustEvent(0.3f, "SpawnDustVFX");
dustEvent.stringParameter = "DustParticle01";
dustEvent.floatParameter = 0.5f;  // Intensity
dustEvent.intParameter = 3;       // Particle count
clip->events.push_back(dustEvent);
clip->SortEvents();
```

### 2. Register Event Handlers

```cpp
// Register callbacks for your event functions
animator->RegisterEventHandler("OnFootstepLeft",
    [](const AnimationEvent& event) {
        // Play footstep sound
        Audio::PlaySound("footstep_left.wav");

        // Spawn dust particles
        SpawnParticles("dust", playerPosition);
    });

animator->RegisterEventHandler("OnFootstepRight",
    [](const AnimationEvent& event) {
        Audio::PlaySound("footstep_right.wav");
        SpawnParticles("dust", playerPosition);
    });

animator->RegisterEventHandler("OnSwingWeapon",
    [](const AnimationEvent& event) {
        // Enable weapon hitbox
        EnableWeaponDamage();
    });

animator->RegisterEventHandler("SpawnDustVFX",
    [entity](const AnimationEvent& event) {
        // Use event parameters
        std::string particleType = event.stringParameter;  // "DustParticle01"
        float intensity = event.floatParameter;            // 0.5
        int count = event.intParameter;                    // 3

        SpawnVFX(entity, particleType, intensity, count);
    });
```

### 3. Events Fire Automatically

Events are automatically processed during `Animate()`:
- During normal playback
- When looping (events fire on wrap-around)
- During blend trees (events from both blended clips)
- During state transitions (events from target animation)

## Advanced Examples

### Character Controller Integration

```cpp
class CharacterController {
    void Initialize() {
        // Register event handlers
        animator->RegisterEventHandler("OnLand", [this](const AnimationEvent& e) {
            OnLandEvent();
        });

        animator->RegisterEventHandler("OnJumpApex", [this](const AnimationEvent& e) {
            canDoubleJump = true;
        });
    }

    void OnLandEvent() {
        PlayLandSound();
        SpawnLandParticles();
        CameraShake(0.2f);
    }
};
```

### Combat System

```cpp
// Setup attack animation events
auto* attackClip = GetClip("Sword_Attack");

// Damage window
AnimationEvent damageStart(0.3f, "EnableDamage");
damageStart.intParameter = 50;  // Damage amount
attackClip->events.push_back(damageStart);

AnimationEvent damageEnd(0.5f, "DisableDamage");
attackClip->events.push_back(damageEnd);

// Visual effects
attackClip->AddEvent(0.35f, "SpawnSlashVFX");
attackClip->AddEvent(0.3f, "PlaySwingSound");

attackClip->SortEvents();

// Register handlers
animator->RegisterEventHandler("EnableDamage", [this](const AnimationEvent& e) {
    weaponCollider->SetEnabled(true);
    weaponDamage = e.intParameter;
});

animator->RegisterEventHandler("DisableDamage", [this](const AnimationEvent& e) {
    weaponCollider->SetEnabled(false);
});
```

### Dialogue/Cutscene System

```cpp
// Animation: "Character_Talk"
auto* talkClip = GetClip("Character_Talk");

AnimationEvent dialogueEvent1(0.5f, "ShowDialogue");
dialogueEvent1.stringParameter = "Hello, traveler!";
talkClip->events.push_back(dialogueEvent1);

AnimationEvent dialogueEvent2(2.0f, "ShowDialogue");
dialogueEvent2.stringParameter = "Welcome to our village.";
talkClip->events.push_back(dialogueEvent2);

talkClip->SortEvents();

// Handler
animator->RegisterEventHandler("ShowDialogue", [](const AnimationEvent& e) {
    UI::ShowDialogueBox(e.stringParameter);
});
```

## API Reference

### AnimationEvent Structure
```cpp
struct AnimationEvent {
    float time;                    // Time in seconds (absolute, not normalized)
    std::string functionName;      // Name of the callback function
    std::string stringParameter;   // Optional string data
    float floatParameter;          // Optional float data
    int intParameter;              // Optional int data
};
```

### AnimationClip Methods
```cpp
void AddEvent(float time, const std::string& functionName);
void SortEvents();  // Call after manually adding events
```

### Animator Methods
```cpp
void RegisterEventHandler(const std::string& functionName, EventCallback callback);
void UnregisterEventHandler(const std::string& functionName);
void ClearEventHandlers();
bool HasEventHandler(const std::string& functionName) const;
```

## Best Practices

### 1. Event Naming
Use clear, descriptive names:
- ✅ `OnFootstepLeft`, `OnWeaponImpact`, `SpawnBloodVFX`
- ❌ `Event1`, `Trigger`, `Do`

### 2. Event Timing
- Add events at the exact moment the action occurs visually
- Test with different animation speeds
- Account for blend transitions

### 3. Parameter Usage
```cpp
// Good: Use parameters for variations
event.stringParameter = "ParticleType";  // Reusable handler
event.floatParameter = intensity;        // Data-driven

// Avoid: Creating separate events for each variation
// BAD: "SpawnDust1", "SpawnDust2", "SpawnDust3" (use parameters instead)
```

### 4. Memory Management
```cpp
// Register handlers once during initialization
void Initialize() {
    animator->RegisterEventHandler("OnDamage", ...);
}

// Clean up when entity is destroyed
void Cleanup() {
    animator->ClearEventHandlers();
}
```

### 5. Debugging
```cpp
// Log events for debugging
animator->RegisterEventHandler("OnFootstep", [](const AnimationEvent& e) {
    BOOM_INFO("Footstep event fired at time: {}", e.time);
    PlayFootstepSound();
});
```

## Serialization

Events are automatically saved and loaded with your scenes:

```yaml
AnimatorComponent:
  Clips:
    - name: "Walk"
      filePath: "Assets/Animations/walk.fbx"
      events:
        - time: 0.3
          functionName: "OnFootstepLeft"
          stringParameter: ""
          floatParameter: 0
          intParameter: 0
        - time: 0.8
          functionName: "OnFootstepRight"
          stringParameter: ""
          floatParameter: 0
          intParameter: 0
```

**Important:** Event handlers are NOT serialized. You must re-register them when loading a scene.

## Common Use Cases

| Use Case | Event Name | Parameters |
|----------|-----------|------------|
| Footsteps | `OnFootstep` | `stringParameter`: surface type |
| Weapon Hit | `OnDamageFrame` | `intParameter`: damage amount |
| Spawn VFX | `SpawnVFX` | `stringParameter`: VFX name, `floatParameter`: scale |
| Camera Shake | `CameraShake` | `floatParameter`: intensity |
| Dialogue | `ShowDialogue` | `stringParameter`: dialogue text |
| Sound Effect | `PlaySound` | `stringParameter`: sound file |
| Enable Hitbox | `EnableHitbox` | `floatParameter`: damage multiplier |

## Troubleshooting

### Events Not Firing
1. Check event is registered: `animator->HasEventHandler("MyEvent")`
2. Verify event time is within clip duration
3. Ensure animation is actually playing
4. Check if events were cleared accidentally

### Events Firing Multiple Times
- Looping animations will fire events every loop (this is intentional)
- Blend trees fire events from BOTH blended clips
- Use state tracking to prevent duplicate actions if needed

### Events Firing at Wrong Time
- Remember: time is in seconds, not normalized (0-1)
- Call `clip->SortEvents()` after manually adding events
- Check `clip->duration` to verify timing

## Performance Notes

- Event processing is O(n) where n = number of events in clip
- Events are pre-sorted for fast range checking
- No overhead when no events are registered
- Blend trees may fire events from 2 clips simultaneously

## What's Next?

After mastering animation events, explore:
- **2D Blend Spaces** - Blend on two parameters (coming soon)
- **Animation Layers** - Separate upper/lower body animations (coming soon)
- **Root Motion** - Drive character movement from animation (coming soon)
