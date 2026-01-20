using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Abstract base class for all objective types.
    /// Provides common properties, state management, and lifecycle methods.
    /// </summary>
    public abstract class BaseObjective
    {
        // Required Entity field for script system
        public ulong Entity;

        // ===== Core Properties (exposed to editor) =====

        [EditorExposed("Objective ID", "Unique identifier for this objective")]
        protected string _objectiveId = "objective_1";

        [EditorExposed("Display Name", "Name shown in the UI")]
        protected string _displayName = "Complete Objective";

        [EditorExposed("Description", "Detailed description shown in UI")]
        protected string _description = "Complete this objective to progress.";

        [EditorExposed("Is Required", "If true, must be completed to finish level")]
        protected bool _isRequired = true;

        [EditorExposed("Start Active", "If true, objective starts in Active state")]
        protected bool _startActive = true;

        [EditorExposed("Prerequisite ID", "ID of objective that must be completed first (empty = no prerequisite)")]
        protected string _prerequisiteId = "";

        [EditorExposed("Show In UI", "If true, this objective appears in the objective list UI")]
        protected bool _showInUI = true;

        // ===== Internal State =====

        protected ObjectiveState _state = ObjectiveState.Locked;
        protected int _currentProgress = 0;
        protected int _targetProgress = 1;

        // ===== Public Properties =====

        public string ObjectiveId => _objectiveId;
        public string DisplayName => _displayName;
        public string Description => _description;
        public bool IsRequired => _isRequired;
        public bool ShowInUI => _showInUI;
        public string PrerequisiteId => _prerequisiteId;
        public ObjectiveState State => _state;
        public int CurrentProgress => _currentProgress;
        public int TargetProgress => _targetProgress;

        /// <summary>
        /// The type of this objective (implemented by derived classes)
        /// </summary>
        public abstract ObjectiveType ObjectiveType { get; }

        /// <summary>
        /// Progress as a float from 0 to 1
        /// </summary>
        public float ProgressPercent => _targetProgress > 0 ? (float)_currentProgress / _targetProgress : 0f;

        /// <summary>
        /// Formatted progress string (e.g., "2/5")
        /// </summary>
        public string ProgressString => $"{_currentProgress}/{_targetProgress}";

        // ===== Lifecycle Methods =====

        /// <summary>
        /// Called when the script component starts
        /// </summary>
        public virtual void OnStart(string jsonParams)
        {
            // Register with the objective manager
            ObjectiveManager.RegisterObjective(this);

            // Set initial state
            if (_startActive && string.IsNullOrEmpty(_prerequisiteId))
            {
                _state = ObjectiveState.Active;
            }
            else
            {
                _state = ObjectiveState.Locked;
            }

            API.Log($"[{GetType().Name}] OnStart: {_objectiveId} (State: {_state})");
        }

        /// <summary>
        /// Called every frame
        /// </summary>
        public virtual void OnUpdate(float dt)
        {
            // Override in derived classes if needed
        }

        /// <summary>
        /// Called when the script component is destroyed
        /// </summary>
        public virtual void OnDestroy()
        {
            ObjectiveManager.UnregisterObjective(_objectiveId);
            API.Log($"[{GetType().Name}] OnDestroy: {_objectiveId}");
        }

        // ===== Internal Update (called by ObjectiveManager) =====

        /// <summary>
        /// Update called by ObjectiveManager for time-based objectives
        /// </summary>
        internal virtual void Update(float dt)
        {
            // Override in derived classes (e.g., SurviveTimeObjective)
        }

        // ===== State Management =====

        /// <summary>
        /// Activate the objective (transition from Locked to Active)
        /// </summary>
        public virtual void Activate()
        {
            if (_state == ObjectiveState.Locked)
            {
                var oldState = _state;
                _state = ObjectiveState.Active;
                OnActivated();
                ObjectiveManager.NotifyStateChanged(_objectiveId, oldState, _state);
            }
        }

        /// <summary>
        /// Complete the objective
        /// </summary>
        protected virtual void Complete()
        {
            if (_state == ObjectiveState.Active)
            {
                var oldState = _state;
                _state = ObjectiveState.Completed;
                _currentProgress = _targetProgress;
                OnCompleted();
                ObjectiveManager.NotifyStateChanged(_objectiveId, oldState, _state);
            }
        }

        /// <summary>
        /// Fail the objective
        /// </summary>
        protected virtual void Fail()
        {
            if (_state == ObjectiveState.Active)
            {
                var oldState = _state;
                _state = ObjectiveState.Failed;
                OnFailed();
                ObjectiveManager.NotifyStateChanged(_objectiveId, oldState, _state);
            }
        }

        /// <summary>
        /// Update progress and check for completion
        /// </summary>
        protected virtual void SetProgress(int progress)
        {
            int oldProgress = _currentProgress;
            _currentProgress = Math.Min(progress, _targetProgress);

            if (_currentProgress != oldProgress)
            {
                ObjectiveManager.NotifyProgress(_objectiveId, _currentProgress, _targetProgress);
                OnProgressChanged(oldProgress, _currentProgress);

                if (_currentProgress >= _targetProgress)
                {
                    Complete();
                }
            }
        }

        /// <summary>
        /// Add to the current progress
        /// </summary>
        protected virtual void AddProgress(int amount = 1)
        {
            SetProgress(_currentProgress + amount);
        }

        // ===== Event Handling =====

        /// <summary>
        /// Handle an event broadcast from ObjectiveManager
        /// Override in derived classes to respond to specific events
        /// </summary>
        public abstract void HandleEvent(ObjectiveEventArgs args);

        // ===== Lifecycle Hooks (override in derived classes) =====

        /// <summary>
        /// Called when the objective becomes active
        /// </summary>
        protected virtual void OnActivated()
        {
            API.Log($"[{GetType().Name}] Objective activated: {_displayName}");
        }

        /// <summary>
        /// Called when the objective is completed
        /// </summary>
        protected virtual void OnCompleted()
        {
            API.Log($"[{GetType().Name}] Objective completed: {_displayName}");
        }

        /// <summary>
        /// Called when the objective is failed
        /// </summary>
        protected virtual void OnFailed()
        {
            API.Log($"[{GetType().Name}] Objective failed: {_displayName}");
        }

        /// <summary>
        /// Called when progress changes
        /// </summary>
        protected virtual void OnProgressChanged(int oldProgress, int newProgress)
        {
            API.Log($"[{GetType().Name}] Progress: {newProgress}/{_targetProgress}");
        }

        // ===== Debug/Testing Methods =====

        /// <summary>
        /// Force complete the objective (for debugging)
        /// </summary>
        public void ForceComplete()
        {
            if (_state != ObjectiveState.Completed)
            {
                var oldState = _state;
                _state = ObjectiveState.Completed;
                _currentProgress = _targetProgress;
                ObjectiveManager.NotifyStateChanged(_objectiveId, oldState, _state);
                API.Log($"[{GetType().Name}] Force completed: {_objectiveId}");
            }
        }

        /// <summary>
        /// Force fail the objective (for debugging)
        /// </summary>
        public void ForceFail()
        {
            if (_state != ObjectiveState.Failed)
            {
                var oldState = _state;
                _state = ObjectiveState.Failed;
                ObjectiveManager.NotifyStateChanged(_objectiveId, oldState, _state);
                API.Log($"[{GetType().Name}] Force failed: {_objectiveId}");
            }
        }
    }
}
