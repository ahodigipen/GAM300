# Objective System Documentation

## Overview

The Objective System provides a flexible, event-driven framework for creating and tracking game objectives. It supports multiple objective types, progress tracking, prerequisites, and UI integration.

## Quick Start

### 1. Add an Objective to Your Scene

1. Create an empty GameObject in your scene
2. Add a script component (e.g., `CollectObjective`, `ReachZoneObjective`)
3. Configure the objective parameters in the inspector:
   - **Objective ID**: Unique identifier (e.g., `"collect_keys"`)
   - **Display Name**: Text shown in UI (e.g., `"Collect all Keys"`)
   - **Target Count**: Number required to complete
   - **Is Required**: Whether this objective is mandatory

### 2. Trigger Objective Events

From your game scripts, broadcast events when relevant actions occur:

```csharp
// When player collects an item
ObjectiveManager.BroadcastEvent("ItemCollected", "Key", 1);

// When player enters a zone
ObjectiveManager.BroadcastEvent("ZoneEntered", "ExitZone");

// When enemy is defeated
ObjectiveManager.BroadcastEvent("EnemyDefeated", "Guard");
```

### 3. Use ObjectiveTrigger for Zones

For zone-based objectives, attach `ObjectiveTrigger` to a collider:

1. Create a GameObject with a Collider (mark as Trigger)
2. Add the `ObjectiveTrigger` script component
3. Set the **Zone Tag** to match your `ReachZoneObjective`

---

## Objective Types

### CollectObjective

Collect N items of a specific type.

**Parameters:**
| Parameter | Description | Default |
|-----------|-------------|---------|
| Item Tag | Type of item to collect (e.g., "Key", "Coin") | "Key" |
| Target Count | Number of items required | 1 |
| Accept Any Item | Count any item regardless of tag | false |

**Events Listened:**
- `ItemCollected` - When any item is collected
- `KeyCollected` - Specifically for keys

**Built-in Integration:**
The `KeyPickup` script automatically broadcasts `KeyCollected` events:
```csharp
// This happens automatically in KeyPickup.cs when a key is collected
PlayerInventory.AddKey(1);
ObjectiveManager.BroadcastEvent(ObjectiveEvents.KeyCollected, "Key", 1);
```

**Example Usage:**
```csharp
// In your collectible script
ObjectiveManager.BroadcastEvent(ObjectiveEvents.ItemCollected, "Gem", 1);
```

---

### ReachZoneObjective

Reach a specific location or trigger zone.

**Parameters:**
| Parameter | Description | Default |
|-----------|-------------|---------|
| Zone Tag | Identifier of the zone to reach | "ExitZone" |
| Stay Duration | Time player must stay in zone (0 = instant) | 0 |
| Reset On Exit | Reset progress timer when leaving zone | true |

**Events Listened:**
- `ZoneEntered` - When player enters the zone
- `ZoneExited` - When player exits the zone

**Built-in End Zone Integration:**
The `EndZoneTrigger` script automatically broadcasts a `ZoneEntered` event:
```csharp
// This happens automatically in EndZoneTrigger.cs
ObjectiveManager.BroadcastEvent(ObjectiveEvents.ZoneEntered, "EndZone", 1);
```

**Example Setup:**
1. Add `ReachZoneObjective` script to an empty GameObject
2. Set Zone Tag to "Checkpoint1"
3. Add `ObjectiveTrigger` to a trigger collider
4. Set the trigger's Zone Tag to "Checkpoint1"

**Using EndZoneTrigger for Level End:**
1. Add `EndZoneTrigger` script to a trigger collider at level end
2. When player enters, it broadcasts "EndZone" event and loads MainMenu
3. To track as objective, add `ReachZoneObjective` with Zone Tag = "EndZone"

---

### InteractObjective

Interact with specific objects.

**Parameters:**
| Parameter | Description | Default |
|-----------|-------------|---------|
| Interaction Tag | Identifier of objects to interact with | "Lever" |
| Target Count | Number of interactions required | 1 |
| Unique Interactions | Same object can only count once | true |
| Accept Door Events | Also listen for DoorOpened events | false |

**Events Listened:**
- `ObjectInteracted` - When player interacts with an object
- `DoorOpened` - When a door is opened (if enabled)

**Built-in Door Integration:**
The `DoorTriggerLeft` script automatically broadcasts `DoorOpened` events when a door opens:
```csharp
// This happens automatically in DoorTriggerLeft.cs when door opens
ObjectiveManager.BroadcastEvent(ObjectiveEvents.DoorOpened, _doorName, 1);
```

To track door opens:
1. Add an `InteractObjective` to your scene
2. Set **Interaction Tag** to the door's name (e.g., "MoveDoor")
3. Enable **Accept Door Events** = true

**Example Usage:**
```csharp
// In your interactable object
ObjectiveManager.BroadcastEvent(ObjectiveEvents.ObjectInteracted, "Lever", 1);
```

---

### DefeatEnemiesObjective

Defeat N enemies.

**Parameters:**
| Parameter | Description | Default |
|-----------|-------------|---------|
| Enemy Tag | Type of enemy (empty = any) | "" |
| Target Count | Number of enemies to defeat | 1 |
| Count Frozen | Frozen enemies count as defeated | false |

**Events Listened:**
- `EnemyDefeated` - When enemy is killed
- `EnemyFrozen` - When enemy is frozen (if enabled)

**Example Usage:**
```csharp
// In your enemy death handler
ObjectiveManager.BroadcastEvent(ObjectiveEvents.EnemyDefeated, "Guard");
```

---

### SurviveTimeObjective

Survive for X seconds.

**Parameters:**
| Parameter | Description | Default |
|-----------|-------------|---------|
| Duration | Time to survive in seconds | 30 |
| Fail On Damage | Any damage fails the objective | false |
| Pause In Safe Zones | Timer pauses in safe areas | false |
| Safe Zone Tag | Tag for safe zones | "SafeZone" |

**Events Listened:**
- `PlayerDied` - Instant fail
- `PlayerDamaged` - Fail if Fail On Damage enabled
- `ZoneEntered`/`ZoneExited` - For safe zone pausing

---

## Event Types Reference

All standard event types are defined in `ObjectiveEvents`:

```csharp
public static class ObjectiveEvents
{
    // Collection events
    public const string ItemCollected = "ItemCollected";
    public const string KeyCollected = "KeyCollected";

    // Zone events
    public const string ZoneEntered = "ZoneEntered";
    public const string ZoneExited = "ZoneExited";

    // Interaction events
    public const string ObjectInteracted = "ObjectInteracted";
    public const string DoorOpened = "DoorOpened";

    // Combat events
    public const string EnemyDefeated = "EnemyDefeated";
    public const string EnemyFrozen = "EnemyFrozen";

    // Survival events
    public const string PlayerDamaged = "PlayerDamaged";
    public const string PlayerDied = "PlayerDied";

    // Custom events
    public const string Custom = "Custom";
}
```

---

## ObjectiveManager API

### Core Methods

```csharp
// Reset all objectives (called automatically on scene start)
ObjectiveManager.Reset();

// Update objectives (called automatically from Entry.Update)
ObjectiveManager.Update(float dt);

// Broadcast an event to all active objectives
ObjectiveManager.BroadcastEvent(string eventType, string targetId, int count = 1);
ObjectiveManager.BroadcastEvent(ObjectiveEventArgs args);
```

### Query Methods

```csharp
// Get specific objective
BaseObjective obj = ObjectiveManager.GetObjective("collect_keys");

// Get all objectives
IReadOnlyList<BaseObjective> all = ObjectiveManager.GetAllObjectives();

// Get only active objectives
IReadOnlyList<BaseObjective> active = ObjectiveManager.GetActiveObjectives();

// Get completed objectives
IReadOnlyList<BaseObjective> completed = ObjectiveManager.GetCompletedObjectives();

// Check if all required objectives are complete
bool allDone = ObjectiveManager.AreAllRequiredComplete();

// Get progress counts
int completedCount = ObjectiveManager.GetCompletedCount();
int totalCount = ObjectiveManager.GetTotalCount();
```

### Debug Methods

```csharp
// Force complete an objective (for testing)
ObjectiveManager.ForceComplete("objective_id");

// Force fail an objective (for testing)
ObjectiveManager.ForceFail("objective_id");
```

### Events

```csharp
// Subscribe to objective state changes
ObjectiveManager.OnObjectiveStateChanged += (id, oldState, newState) => {
    API.Log($"Objective {id} changed from {oldState} to {newState}");
};

// Subscribe to progress updates
ObjectiveManager.OnObjectiveProgress += (id, current, target) => {
    API.Log($"Objective {id} progress: {current}/{target}");
};

// Subscribe to all-complete notification
ObjectiveManager.OnAllRequiredComplete += () => {
    API.Log("All required objectives complete!");
    // Trigger end game or next level
};
```

---

## Prerequisites System

Objectives can require other objectives to be completed first:

1. Set **Prerequisite ID** to the ID of the required objective
2. The objective will start in `Locked` state
3. When the prerequisite completes, this objective activates

**Example:**
```
Objective A (ID: "find_key")     -> Starts Active
Objective B (ID: "open_door")    -> Prerequisite: "find_key" -> Starts Locked
```

When "find_key" completes, "open_door" automatically becomes Active.

---

## UI Integration

### UIObjectiveController

The `UIObjectiveController` displays current objectives in the UI:

1. Add `UIObjectiveController` script to your UI entity
2. Configure child entity names for UI elements:
   - **Title Entity Name**: Shows objective name
   - **Progress Entity Name**: Shows progress (e.g., "2/5")
   - **Check Entity Name**: Checkmark shown on completion

### Static Helpers

```csharp
// Get formatted text of all active objectives
string text = UIObjectiveController.GetActiveObjectivesText();
// Output: "[ ] Collect 3 Keys (1/3)\n[ ] Reach Exit (0/1)"

// Get progress summary
string summary = UIObjectiveController.GetProgressSummary();
// Output: "2/5 objectives complete"
```

---

## Creating Custom Objectives

Extend `BaseObjective` for custom objective types:

```csharp
public class CustomObjective : BaseObjective
{
    [EditorExposed("Custom Parameter", "Description")]
    private string _customParam = "default";

    public override ObjectiveType ObjectiveType => ObjectiveType.Custom;

    public override void OnStart(string jsonParams)
    {
        _targetProgress = 1;
        base.OnStart(jsonParams);
    }

    public override void HandleEvent(ObjectiveEventArgs args)
    {
        if (_state != ObjectiveState.Active) return;

        if (args.EventType == "CustomEvent" &&
            args.TargetId == _customParam)
        {
            Complete();
        }
    }
}
```

---

## Best Practices

### 1. Use Meaningful Objective IDs
```csharp
// Good
_objectiveId = "collect_garden_keys";
_objectiveId = "reach_exit_door";

// Bad
_objectiveId = "obj1";
_objectiveId = "objective";
```

### 2. Broadcast Events at the Right Time
```csharp
// Broadcast AFTER the action completes successfully
PlayerInventory.AddKey(1);
ObjectiveManager.BroadcastEvent("KeyCollected", "Key");

// Not before
ObjectiveManager.BroadcastEvent("KeyCollected", "Key");
PlayerInventory.AddKey(1); // What if this fails?
```

### 3. Use Consistent Tags
Keep a constants file or use `ObjectiveEvents`:
```csharp
// Define tags in one place
public static class GameTags
{
    public const string Key = "Key";
    public const string ExitZone = "ExitZone";
    public const string GuardEnemy = "Guard";
}
```

### 4. Test with ForceComplete
During development, use debug methods:
```csharp
// Skip to test later objectives
ObjectiveManager.ForceComplete("collect_keys");
```

---

## Troubleshooting

### Objective Not Activating
- Check that **Prerequisite ID** is empty or the prerequisite is complete
- Verify **Start Active** is enabled
- Ensure the script is properly attached to an entity

### Events Not Being Received
- Verify the objective is in `Active` state
- Check that event type and target ID match exactly (case-insensitive)
- Ensure `ObjectiveManager.Reset()` is called in `Entry.Start()`

### UI Not Updating
- Verify `UIObjectiveController` is initialized in `UIManager`
- Check that **Show In UI** is enabled on the objective
- Ensure UI entity names match the configured names

---

## Scene Transition Handling

The objective system integrates with the game's scene transition system to ensure clean state management.

### Automatic Reset on Scene Load

When a new scene loads, `Entry.Start()` resets the scene transition flag:
```csharp
EndZoneTrigger.s_sceneTransitionInProgress = false;
```

### EndZoneTrigger Integration

The `EndZoneTrigger` component handles level completion and scene transitions:

1. When player enters the end zone, a scene load is queued
2. A **100ms delay** is applied before loading (allows physics to settle)
3. The `s_sceneTransitionInProgress` flag is set to block all trigger callbacks
4. After the delay, the scene loads

**Key Features:**
- Broadcasts `ZoneEntered` event with "EndZone" as target ID
- Uses deferred loading to prevent PhysX crashes
- All trigger callbacks check `s_sceneTransitionInProgress` and return early if true

### DoorTriggerLeft Integration

Doors now broadcast events to the objective system:

```csharp
// When door opens successfully
ObjectiveManager.BroadcastEvent(ObjectiveEvents.DoorOpened, doorName, 1);
```

**To track door opens with objectives:**
1. Add an `InteractObjective` to your scene
2. Set **Interaction Tag** to the door's name (e.g., "MoveDoor")
3. Enable **Accept Door Events** = true

### Trigger Callback Safety

All trigger-based scripts check the scene transition flag at the start of their callbacks:

```csharp
private static void OnTriggerEnterCallback(ulong triggerEntity, ulong otherEntity)
{
    // Absolute first check - if scene transition in progress, do nothing
    if (EndZoneTrigger.s_sceneTransitionInProgress) return;

    // ... rest of callback
}
```

This prevents crashes from stale entity access during scene cleanup.

### Scripts with Scene Transition Protection

The following scripts have scene transition checks in their trigger callbacks:
- `EndZoneTrigger.cs`
- `KeyPickup.cs`
- `DoorTriggerLeft.cs`
- `CrouchTriggerText.cs` (CrouchTriggerZone)
- `ObjectiveTrigger.cs`
- `PlayerMovement.cs`
- `MovementAnimator.cs`

---

## File Reference

| File | Purpose |
|------|---------|
| `ObjectiveData.cs` | Enums, event types, and data structures |
| `ObjectiveManager.cs` | Central manager singleton |
| `BaseObjective.cs` | Abstract base class |
| `CollectObjective.cs` | Collect items objective |
| `ReachZoneObjective.cs` | Reach location objective |
| `InteractObjective.cs` | Interact with objects objective |
| `DefeatEnemiesObjective.cs` | Defeat enemies objective |
| `SurviveTimeObjective.cs` | Survive time objective |
| `ObjectiveTrigger.cs` | Trigger zone component |
| `UIObjectiveController.cs` | UI display controller |
| `EndZoneTrigger.cs` | Level end zone with scene transition |
| `KeyPickup.cs` | Key collection with objective broadcast |
| `DoorTriggerLeft.cs` | Door trigger with objective broadcast |
