using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Objective type: Collect N items of a specific type.
    /// Listens for ItemCollected events with matching TargetId.
    ///
    /// Usage:
    /// 1. Add this script to an empty GameObject in your scene
    /// 2. Set the Item Tag to match what your collectibles broadcast (e.g., "Key", "Coin", "Gem")
    /// 3. Set Target Count to the number of items required
    /// 4. Have your collectible scripts call: ObjectiveManager.BroadcastEvent("ItemCollected", "Key")
    /// </summary>
    public class CollectObjective : BaseObjective
    {
        [EditorExposed("Item Tag", "The tag/type of item to collect (e.g., 'Key', 'Coin')")]
        private string _itemTag = "Key";

        [EditorExposed("Target Count", "Number of items required to complete", 1, 100)]
        private int _targetCount = 1;

        [EditorExposed("Accept Any Item", "If true, counts any ItemCollected event regardless of tag")]
        private bool _acceptAnyItem = false;

        public override ObjectiveType ObjectiveType => ObjectiveType.Collect;

        public string ItemTag => _itemTag;

        public override void OnStart(string jsonParams)
        {
            _targetProgress = _targetCount;
            _currentProgress = 0;

            // Update display name if using default
            if (_displayName == "Complete Objective")
            {
                _displayName = $"Collect {_targetCount} {_itemTag}(s)";
            }

            base.OnStart(jsonParams);
        }

        public override void HandleEvent(ObjectiveEventArgs args)
        {
            if (_state != ObjectiveState.Active) return;

            // Check if this is a collection event
            if (args.EventType == ObjectiveEvents.ItemCollected ||
                args.EventType == ObjectiveEvents.KeyCollected)
            {
                // Check if the item tag matches (or accept any)
                if (_acceptAnyItem ||
                    string.Equals(args.TargetId, _itemTag, StringComparison.OrdinalIgnoreCase))
                {
                    AddProgress(args.Count);
                    API.Log($"[CollectObjective] '{_objectiveId}' collected {args.TargetId}: {_currentProgress}/{_targetProgress}");
                }
            }
        }

        protected override void OnCompleted()
        {
            base.OnCompleted();
            API.Log($"[CollectObjective] '{_objectiveId}' completed! Collected all {_targetCount} {_itemTag}(s)");
        }
    }
}
