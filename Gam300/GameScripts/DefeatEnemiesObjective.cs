using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Objective type: Defeat N enemies.
    /// Listens for EnemyDefeated events with optionally matching TargetId.
    ///
    /// Usage:
    /// 1. Add this script to an empty GameObject in your scene
    /// 2. Set Enemy Tag to filter specific enemy types (or leave empty for any enemy)
    /// 3. Set Target Count to the number of enemies required
    /// 4. Have enemy scripts call: ObjectiveManager.BroadcastEvent("EnemyDefeated", "Guard")
    /// </summary>
    public class DefeatEnemiesObjective : BaseObjective
    {
        [EditorExposed("Enemy Tag", "Type of enemy to defeat (empty = any enemy)")]
        private string _enemyTag = "";

        [EditorExposed("Target Count", "Number of enemies to defeat", 1, 100)]
        private int _targetCount = 1;

        [EditorExposed("Count Frozen", "If true, frozen enemies also count as defeated")]
        private bool _countFrozen = false;

        public override ObjectiveType ObjectiveType => ObjectiveType.DefeatEnemies;

        public string EnemyTag => _enemyTag;

        public override void OnStart(string jsonParams)
        {
            _targetProgress = _targetCount;
            _currentProgress = 0;

            // Update display name if using default
            if (_displayName == "Complete Objective")
            {
                string enemyName = string.IsNullOrEmpty(_enemyTag) ? "enemies" : _enemyTag + "(s)";
                _displayName = $"Defeat {_targetCount} {enemyName}";
            }

            base.OnStart(jsonParams);
        }

        public override void HandleEvent(ObjectiveEventArgs args)
        {
            if (_state != ObjectiveState.Active) return;

            bool isDefeatEvent = args.EventType == ObjectiveEvents.EnemyDefeated ||
                                (_countFrozen && args.EventType == ObjectiveEvents.EnemyFrozen);

            if (!isDefeatEvent) return;

            // Check if enemy tag matches (or accept any if empty)
            if (string.IsNullOrEmpty(_enemyTag) ||
                string.Equals(args.TargetId, _enemyTag, StringComparison.OrdinalIgnoreCase))
            {
                AddProgress(args.Count);
                API.Log($"[DefeatEnemiesObjective] '{_objectiveId}' enemy defeated: {_currentProgress}/{_targetProgress}");
            }
        }

        protected override void OnCompleted()
        {
            base.OnCompleted();
            API.Log($"[DefeatEnemiesObjective] '{_objectiveId}' completed! Defeated {_targetCount} enemies");
        }
    }
}
