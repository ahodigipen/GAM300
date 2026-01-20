using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Objective type: Survive for X seconds.
    /// Automatically tracks time and fails if player dies.
    ///
    /// Usage:
    /// 1. Add this script to an empty GameObject in your scene
    /// 2. Set Duration to the survival time in seconds
    /// 3. Optionally enable Fail On Damage for extra challenge
    /// 4. Ensure player death broadcasts: ObjectiveManager.BroadcastEvent("PlayerDied", "")
    /// </summary>
    public class SurviveTimeObjective : BaseObjective
    {
        [EditorExposed("Duration", "Time to survive in seconds", 1, 600)]
        private float _duration = 30f;

        [EditorExposed("Fail On Damage", "If true, taking any damage fails the objective")]
        private bool _failOnDamage = false;

        [EditorExposed("Pause In Safe Zones", "If true, timer pauses in safe zones")]
        private bool _pauseInSafeZones = false;

        [EditorExposed("Safe Zone Tag", "Tag for safe zones if Pause In Safe Zones is enabled")]
        private string _safeZoneTag = "SafeZone";

        private float _elapsedTime = 0f;
        private bool _isPaused = false;

        public override ObjectiveType ObjectiveType => ObjectiveType.SurviveTime;

        public float Duration => _duration;
        public float RemainingTime => Math.Max(0, _duration - _elapsedTime);
        public float ElapsedTime => _elapsedTime;

        public override void OnStart(string jsonParams)
        {
            _targetProgress = (int)_duration;
            _currentProgress = 0;
            _elapsedTime = 0f;
            _isPaused = false;

            // Update display name if using default
            if (_displayName == "Complete Objective")
            {
                _displayName = $"Survive for {_duration:F0} seconds";
            }

            base.OnStart(jsonParams);
        }

        internal override void Update(float dt)
        {
            if (_state != ObjectiveState.Active) return;
            if (_isPaused) return;

            _elapsedTime += dt;

            // Update progress (in seconds)
            int newProgress = (int)_elapsedTime;
            if (newProgress != _currentProgress)
            {
                _currentProgress = Math.Min(newProgress, _targetProgress);
                ObjectiveManager.NotifyProgress(_objectiveId, _currentProgress, _targetProgress);
            }

            // Check completion
            if (_elapsedTime >= _duration)
            {
                Complete();
            }
        }

        public override void HandleEvent(ObjectiveEventArgs args)
        {
            if (_state != ObjectiveState.Active) return;

            // Player death = instant fail
            if (args.EventType == ObjectiveEvents.PlayerDied)
            {
                Fail();
                return;
            }

            // Optional: fail on damage
            if (_failOnDamage && args.EventType == ObjectiveEvents.PlayerDamaged)
            {
                Fail();
                return;
            }

            // Handle safe zones
            if (_pauseInSafeZones)
            {
                if (args.EventType == ObjectiveEvents.ZoneEntered &&
                    string.Equals(args.TargetId, _safeZoneTag, StringComparison.OrdinalIgnoreCase))
                {
                    _isPaused = true;
                    API.Log($"[SurviveTimeObjective] Timer paused (safe zone)");
                }
                else if (args.EventType == ObjectiveEvents.ZoneExited &&
                         string.Equals(args.TargetId, _safeZoneTag, StringComparison.OrdinalIgnoreCase))
                {
                    _isPaused = false;
                    API.Log($"[SurviveTimeObjective] Timer resumed");
                }
            }
        }

        protected override void OnCompleted()
        {
            base.OnCompleted();
            API.Log($"[SurviveTimeObjective] '{_objectiveId}' completed! Survived {_duration:F0} seconds");
        }

        protected override void OnFailed()
        {
            base.OnFailed();
            API.Log($"[SurviveTimeObjective] '{_objectiveId}' failed! Survived only {_elapsedTime:F1} seconds");
        }

        /// <summary>
        /// Get formatted time remaining string (MM:SS)
        /// </summary>
        public string GetTimeRemainingFormatted()
        {
            float remaining = RemainingTime;
            int minutes = (int)(remaining / 60);
            int seconds = (int)(remaining % 60);
            return $"{minutes:D2}:{seconds:D2}";
        }
    }
}
