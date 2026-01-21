using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Trigger component that broadcasts events to the ObjectiveManager when the player enters/exits.
    /// Attach to any entity with a collider marked as IsTrigger.
    ///
    /// Usage:
    /// 1. Create a GameObject with a Collider (marked as Trigger)
    /// 2. Attach this script component
    /// 3. Configure the Event Type and Zone Tag
    /// 4. Add matching objective components that listen for these events
    /// </summary>
    public class ObjectiveTrigger
    {
        public ulong Entity;

        // Static instances dictionary (following KeyPickup pattern)
        private static readonly Dictionary<ulong, ObjectiveTrigger> s_instances = new Dictionary<ulong, ObjectiveTrigger>();

        /// <summary>
        /// Clear all static instances (call on scene change to prevent stale entity access)
        /// </summary>
        public static void ClearInstances()
        {
            foreach (var kvp in s_instances)
            {
                API.UnregisterTriggerCallbacks(kvp.Key);
            }
            s_instances.Clear();
            API.Log("[ObjectiveTrigger] Cleared all instances");
        }

        [EditorExposed("Zone Tag", "Identifier for this trigger zone (e.g., 'ExitZone', 'Checkpoint1')")]
        private string _zoneTag = "Zone";

        [EditorExposed("Enter Event Type", "Event type to broadcast on enter")]
        private string _enterEventType = ObjectiveEvents.ZoneEntered;

        [EditorExposed("Exit Event Type", "Event type to broadcast on exit (empty = no exit event)")]
        private string _exitEventType = ObjectiveEvents.ZoneExited;

        [EditorExposed("Player Only", "If true, only triggers for the player entity")]
        private bool _playerOnly = true;

        [EditorExposed("One Shot", "If true, only triggers once then disables")]
        private bool _oneShot = false;

        [EditorExposed("Broadcast Count", "Count value to include in the event", 1, 100)]
        private int _broadcastCount = 1;

        [EditorExposed("Custom Data", "Optional custom data string to include in event")]
        private string _customData = "";

        private bool _hasTriggered = false;
        private bool _playerInside = false;

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;

            if (!API.HasCollider(Entity))
            {
                API.Log("[ObjectiveTrigger] WARNING: Entity has no collider. Trigger will not work.");
                return;
            }

            // Ensure this collider is a trigger
            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log("[ObjectiveTrigger] Collider set to IsTrigger = true.");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log($"[ObjectiveTrigger] Registered: {_zoneTag}");
        }

        public void OnUpdate(float dt)
        {
            // No-op
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity))
            {
                s_instances.Remove(Entity);
            }
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            // Skip if scene transition is in progress
            if (EndZoneTrigger.s_sceneTransitionInProgress) return;

            if (!s_instances.TryGetValue(triggerEntity, out var inst)) return;

            // Check one-shot
            if (inst._oneShot && inst._hasTriggered) return;

            // Check player-only
            if (inst._playerOnly && otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInside = true;
            inst._hasTriggered = true;

            // Broadcast enter event
            if (!string.IsNullOrEmpty(inst._enterEventType))
            {
                var args = new ObjectiveEventArgs
                {
                    EventType = inst._enterEventType,
                    TargetId = inst._zoneTag,
                    Count = inst._broadcastCount,
                    SourceEntity = triggerEntity,
                    Position = API.GetPosition(otherEntity),
                    CustomData = inst._customData
                };

                ObjectiveManager.BroadcastEvent(args);
                API.Log($"[ObjectiveTrigger] Broadcast: {inst._enterEventType} ({inst._zoneTag})");
            }
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            // Skip if scene transition is in progress
            if (EndZoneTrigger.s_sceneTransitionInProgress) return;

            if (!s_instances.TryGetValue(triggerEntity, out var inst)) return;

            // Check player-only
            if (inst._playerOnly && otherEntity != PlayerMovement.GetPlayerEntity()) return;

            if (!inst._playerInside) return;
            inst._playerInside = false;

            // Broadcast exit event
            if (!string.IsNullOrEmpty(inst._exitEventType))
            {
                var args = new ObjectiveEventArgs
                {
                    EventType = inst._exitEventType,
                    TargetId = inst._zoneTag,
                    Count = inst._broadcastCount,
                    SourceEntity = triggerEntity,
                    Position = API.GetPosition(otherEntity),
                    CustomData = inst._customData
                };

                ObjectiveManager.BroadcastEvent(args);
                API.Log($"[ObjectiveTrigger] Broadcast: {inst._exitEventType} ({inst._zoneTag})");
            }
        }

        /// <summary>
        /// Manually trigger this zone (for scripted events)
        /// </summary>
        public void ManualTrigger()
        {
            if (_oneShot && _hasTriggered) return;

            _hasTriggered = true;

            if (!string.IsNullOrEmpty(_enterEventType))
            {
                var args = new ObjectiveEventArgs
                {
                    EventType = _enterEventType,
                    TargetId = _zoneTag,
                    Count = _broadcastCount,
                    SourceEntity = Entity,
                    CustomData = _customData
                };

                ObjectiveManager.BroadcastEvent(args);
                API.Log($"[ObjectiveTrigger] Manual broadcast: {_enterEventType} ({_zoneTag})");
            }
        }

        /// <summary>
        /// Reset the trigger (for one-shot triggers)
        /// </summary>
        public void ResetTrigger()
        {
            _hasTriggered = false;
            _playerInside = false;
        }

        /// <summary>
        /// Get a trigger instance by entity ID
        /// </summary>
        public static ObjectiveTrigger GetInstance(ulong entity)
        {
            return s_instances.TryGetValue(entity, out var inst) ? inst : null;
        }
    }
}
