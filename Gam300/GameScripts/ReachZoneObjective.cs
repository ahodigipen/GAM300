using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Objective type: Reach a specific location or trigger zone.
    /// Listens for ZoneEntered events with matching TargetId.
    ///
    /// Usage:
    /// 1. Add this script to an empty GameObject in your scene
    /// 2. Set the Zone Tag to match your trigger zone's identifier
    /// 3. Add an ObjectiveTrigger component to your zone collider
    /// 4. Configure the ObjectiveTrigger to broadcast "ZoneEntered" with the same zone tag
    /// </summary>
    public class ReachZoneObjective : BaseObjective
    {
        [EditorExposed("Zone Tag", "The identifier of the zone to reach (e.g., 'ExitZone', 'Checkpoint1')")]
        private string _zoneTag = "ExitZone";

        [EditorExposed("Stay Duration", "Time (seconds) player must stay in zone to complete (0 = instant)", 0, 60)]
        private float _stayDuration = 0f;

        [EditorExposed("Reset On Exit", "If true and using stay duration, progress resets when leaving zone")]
        private bool _resetOnExit = true;

        private bool _playerInZone = false;
        private float _timeInZone = 0f;

        public override ObjectiveType ObjectiveType => ObjectiveType.ReachZone;

        public string ZoneTag => _zoneTag;

        public override void OnStart(string jsonParams)
        {
            _targetProgress = 1;
            _currentProgress = 0;

            // Update display name if using default
            if (_displayName == "Complete Objective")
            {
                _displayName = _stayDuration > 0
                    ? $"Stay in {_zoneTag} for {_stayDuration:F1}s"
                    : $"Reach {_zoneTag}";
            }

            base.OnStart(jsonParams);
        }

        internal override void Update(float dt)
        {
            if (_state != ObjectiveState.Active) return;

            // Handle stay duration logic
            if (_playerInZone && _stayDuration > 0)
            {
                _timeInZone += dt;

                if (_timeInZone >= _stayDuration)
                {
                    Complete();
                }
            }
        }

        public override void HandleEvent(ObjectiveEventArgs args)
        {
            if (_state != ObjectiveState.Active) return;

            // Zone entered event
            if (args.EventType == ObjectiveEvents.ZoneEntered)
            {
                if (string.Equals(args.TargetId, _zoneTag, StringComparison.OrdinalIgnoreCase))
                {
                    _playerInZone = true;
                    API.Log($"[ReachZoneObjective] '{_objectiveId}' player entered zone: {_zoneTag}");

                    // Instant completion if no stay duration
                    if (_stayDuration <= 0)
                    {
                        Complete();
                    }
                }
            }
            // Zone exited event
            else if (args.EventType == ObjectiveEvents.ZoneExited)
            {
                if (string.Equals(args.TargetId, _zoneTag, StringComparison.OrdinalIgnoreCase))
                {
                    _playerInZone = false;
                    API.Log($"[ReachZoneObjective] '{_objectiveId}' player exited zone: {_zoneTag}");

                    // Reset time if configured
                    if (_resetOnExit)
                    {
                        _timeInZone = 0f;
                    }
                }
            }
        }

        protected override void OnCompleted()
        {
            base.OnCompleted();
            API.Log($"[ReachZoneObjective] '{_objectiveId}' completed! Reached zone: {_zoneTag}");
        }
    }
}
