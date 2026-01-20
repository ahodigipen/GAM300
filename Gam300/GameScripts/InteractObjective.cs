using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Objective type: Interact with specific objects.
    /// Listens for ObjectInteracted events with matching TargetId.
    ///
    /// Usage:
    /// 1. Add this script to an empty GameObject in your scene
    /// 2. Set the Interaction Tag to match your interactable object's identifier
    /// 3. Set Target Count if multiple interactions are required
    /// 4. Have interactable objects call: ObjectiveManager.BroadcastEvent("ObjectInteracted", "Lever1")
    /// </summary>
    public class InteractObjective : BaseObjective
    {
        [EditorExposed("Interaction Tag", "The identifier of the object(s) to interact with")]
        private string _interactionTag = "Lever";

        [EditorExposed("Target Count", "Number of interactions required", 1, 100)]
        private int _targetCount = 1;

        [EditorExposed("Unique Interactions", "If true, same object can only count once")]
        private bool _uniqueInteractions = true;

        [EditorExposed("Accept Door Events", "If true, also listens for DoorOpened events")]
        private bool _acceptDoorEvents = false;

        // Track unique interactions
        private System.Collections.Generic.HashSet<string> _interactedObjects =
            new System.Collections.Generic.HashSet<string>();

        public override ObjectiveType ObjectiveType => ObjectiveType.Interact;

        public string InteractionTag => _interactionTag;

        public override void OnStart(string jsonParams)
        {
            _targetProgress = _targetCount;
            _currentProgress = 0;
            _interactedObjects.Clear();

            // Update display name if using default
            if (_displayName == "Complete Objective")
            {
                _displayName = _targetCount > 1
                    ? $"Interact with {_targetCount} {_interactionTag}(s)"
                    : $"Interact with {_interactionTag}";
            }

            base.OnStart(jsonParams);
        }

        public override void HandleEvent(ObjectiveEventArgs args)
        {
            if (_state != ObjectiveState.Active) return;

            bool isValidEvent = args.EventType == ObjectiveEvents.ObjectInteracted ||
                               (_acceptDoorEvents && args.EventType == ObjectiveEvents.DoorOpened);

            if (!isValidEvent) return;

            // Check if the interaction tag matches
            if (string.Equals(args.TargetId, _interactionTag, StringComparison.OrdinalIgnoreCase))
            {
                // Handle unique interaction tracking
                string uniqueKey = args.CustomData ?? args.SourceEntity.ToString();

                if (_uniqueInteractions)
                {
                    if (_interactedObjects.Contains(uniqueKey))
                    {
                        API.Log($"[InteractObjective] '{_objectiveId}' already interacted with: {uniqueKey}");
                        return;
                    }
                    _interactedObjects.Add(uniqueKey);
                }

                AddProgress(args.Count);
                API.Log($"[InteractObjective] '{_objectiveId}' interacted: {_currentProgress}/{_targetProgress}");
            }
        }

        protected override void OnCompleted()
        {
            base.OnCompleted();
            API.Log($"[InteractObjective] '{_objectiveId}' completed! Interacted with all {_interactionTag}(s)");
        }
    }
}
