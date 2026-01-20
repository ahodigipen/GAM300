using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// UI controller for displaying objective status.
    /// Add this to a UI entity with child sprite entities for displaying objectives.
    ///
    /// This controller:
    /// - Subscribes to objective state/progress events
    /// - Updates UI elements to show current objectives
    /// - Handles objective completion animations
    ///
    /// Expected child entity naming convention:
    /// - "Objective_Title" - Text/sprite for objective title
    /// - "Objective_Progress" - Text/sprite for progress display
    /// - "Objective_Check" - Checkmark sprite for completed objectives
    /// </summary>
    public class UIObjectiveController
    {
        public ulong Entity;

        [EditorExposed("Title Entity Name", "Name of child entity for objective title display")]
        private string _titleEntityName = "Objective_Title";

        [EditorExposed("Progress Entity Name", "Name of child entity for progress display")]
        private string _progressEntityName = "Objective_Progress";

        [EditorExposed("Check Entity Name", "Name of child entity for completion checkmark")]
        private string _checkEntityName = "Objective_Check";

        [EditorExposed("Show Completed Duration", "Time to show completed objectives before hiding", 0, 10)]
        private float _showCompletedDuration = 3f;

        [EditorExposed("Fade Duration", "Duration of fade in/out animations", 0, 2)]
        private float _fadeDuration = 0.5f;

        [EditorExposed("Max Displayed", "Maximum number of objectives to display at once", 1, 10)]
        private int _maxDisplayed = 3;

        // Entity handles for UI elements
        private ulong _titleEntity = 0;
        private ulong _progressEntity = 0;
        private ulong _checkEntity = 0;

        // Current display state
        private string _currentObjectiveId = "";
        private float _completedTimer = 0f;
        private bool _isShowingCompleted = false;
        private float _currentAlpha = 0f;
        private bool _isVisible = false;

        // Cache last displayed info to avoid redundant updates
        private string _lastDisplayedTitle = "";
        private string _lastDisplayedProgress = "";

        public void OnStart(string jsonParams)
        {
            // Find child entities
            _titleEntity = API.FindEntity(_titleEntityName);
            _progressEntity = API.FindEntity(_progressEntityName);
            _checkEntity = API.FindEntity(_checkEntityName);

            // Subscribe to objective events
            ObjectiveManager.OnObjectiveStateChanged += HandleStateChanged;
            ObjectiveManager.OnObjectiveProgress += HandleProgress;
            ObjectiveManager.OnAllRequiredComplete += HandleAllComplete;

            // Initially hide the check mark
            if (_checkEntity != 0 && API.HasSprite(_checkEntity))
            {
                API.SetSpriteAlpha(_checkEntity, 0f);
            }

            // Start hidden
            SetVisibility(false);

            API.Log("[UIObjectiveController] Initialized");
        }

        public void OnUpdate(float dt)
        {
            // Handle completed objective display timer
            if (_isShowingCompleted)
            {
                _completedTimer -= dt;
                if (_completedTimer <= 0)
                {
                    _isShowingCompleted = false;
                    _currentObjectiveId = "";
                    UpdateDisplay();
                }
            }

            // Handle fade animation
            UpdateFade(dt);

            // Periodically refresh display to catch new objectives
            RefreshActiveObjective();
        }

        public void OnDestroy()
        {
            // Unsubscribe from events
            ObjectiveManager.OnObjectiveStateChanged -= HandleStateChanged;
            ObjectiveManager.OnObjectiveProgress -= HandleProgress;
            ObjectiveManager.OnAllRequiredComplete -= HandleAllComplete;
        }

        private void HandleStateChanged(string objectiveId, ObjectiveState oldState, ObjectiveState newState)
        {
            var objective = ObjectiveManager.GetObjective(objectiveId);
            if (objective == null || !objective.ShowInUI) return;

            if (newState == ObjectiveState.Active)
            {
                // New objective activated - show it
                _currentObjectiveId = objectiveId;
                _isShowingCompleted = false;
                UpdateDisplay();
            }
            else if (newState == ObjectiveState.Completed)
            {
                // Objective completed - show completion state
                _currentObjectiveId = objectiveId;
                _isShowingCompleted = true;
                _completedTimer = _showCompletedDuration;
                ShowCompleted(objective);
            }
            else if (newState == ObjectiveState.Failed)
            {
                // Objective failed - could show failure state
                if (_currentObjectiveId == objectiveId)
                {
                    _currentObjectiveId = "";
                    UpdateDisplay();
                }
            }
        }

        private void HandleProgress(string objectiveId, int current, int target)
        {
            if (objectiveId == _currentObjectiveId)
            {
                UpdateProgressDisplay(current, target);
            }
        }

        private void HandleAllComplete()
        {
            API.Log("[UIObjectiveController] All objectives complete!");
            // Could trigger special UI effect here
        }

        private void RefreshActiveObjective()
        {
            // If nothing is showing and we're not in completed state, find next active objective
            if (string.IsNullOrEmpty(_currentObjectiveId) && !_isShowingCompleted)
            {
                var activeObjectives = ObjectiveManager.GetActiveObjectives();
                foreach (var obj in activeObjectives)
                {
                    if (obj.ShowInUI)
                    {
                        _currentObjectiveId = obj.ObjectiveId;
                        UpdateDisplay();
                        break;
                    }
                }
            }
        }

        private void UpdateDisplay()
        {
            if (string.IsNullOrEmpty(_currentObjectiveId))
            {
                SetVisibility(false);
                return;
            }

            var objective = ObjectiveManager.GetObjective(_currentObjectiveId);
            if (objective == null)
            {
                SetVisibility(false);
                return;
            }

            SetVisibility(true);

            // Update title
            string title = objective.DisplayName;
            if (title != _lastDisplayedTitle)
            {
                _lastDisplayedTitle = title;
                // Would set text on title entity if text rendering is supported
            }

            // Update progress
            UpdateProgressDisplay(objective.CurrentProgress, objective.TargetProgress);

            // Hide checkmark for active objectives
            if (_checkEntity != 0 && API.HasSprite(_checkEntity))
            {
                API.SetSpriteAlpha(_checkEntity, 0f);
            }
        }

        private void UpdateProgressDisplay(int current, int target)
        {
            string progress = $"{current}/{target}";
            if (progress != _lastDisplayedProgress)
            {
                _lastDisplayedProgress = progress;
                // Would set text on progress entity if text rendering is supported
            }
        }

        private void ShowCompleted(BaseObjective objective)
        {
            SetVisibility(true);

            // Update title with completion message
            _lastDisplayedTitle = objective.DisplayName + " - Complete!";

            // Show checkmark
            if (_checkEntity != 0 && API.HasSprite(_checkEntity))
            {
                API.SetSpriteAlpha(_checkEntity, 1f);
            }

            // Update progress to show completion
            _lastDisplayedProgress = $"{objective.TargetProgress}/{objective.TargetProgress}";

            API.Log($"[UIObjectiveController] Showing completed: {objective.DisplayName}");
        }

        private void SetVisibility(bool visible)
        {
            _isVisible = visible;
            // Target alpha will be handled by UpdateFade
        }

        private void UpdateFade(float dt)
        {
            float targetAlpha = _isVisible ? 1f : 0f;

            if (Math.Abs(_currentAlpha - targetAlpha) > 0.01f)
            {
                float fadeSpeed = 1f / Math.Max(0.01f, _fadeDuration);

                if (_currentAlpha < targetAlpha)
                {
                    _currentAlpha = Math.Min(_currentAlpha + fadeSpeed * dt, targetAlpha);
                }
                else
                {
                    _currentAlpha = Math.Max(_currentAlpha - fadeSpeed * dt, targetAlpha);
                }

                // Apply alpha to UI elements
                if (_titleEntity != 0 && API.HasSprite(_titleEntity))
                {
                    API.SetSpriteAlpha(_titleEntity, _currentAlpha);
                }

                if (_progressEntity != 0 && API.HasSprite(_progressEntity))
                {
                    API.SetSpriteAlpha(_progressEntity, _currentAlpha);
                }
            }
        }

        // ===== Static Helper Methods for UIManager integration =====

        /// <summary>
        /// Get formatted string of all active objectives
        /// </summary>
        public static string GetActiveObjectivesText()
        {
            var objectives = ObjectiveManager.GetActiveObjectives();
            if (objectives.Count == 0) return "";

            var sb = new System.Text.StringBuilder();
            foreach (var obj in objectives)
            {
                if (obj.ShowInUI)
                {
                    sb.AppendLine($"[ ] {obj.DisplayName} ({obj.ProgressString})");
                }
            }
            return sb.ToString();
        }

        /// <summary>
        /// Get progress summary (e.g., "2/5 objectives complete")
        /// </summary>
        public static string GetProgressSummary()
        {
            int completed = ObjectiveManager.GetCompletedCount();
            int total = ObjectiveManager.GetTotalCount();
            return $"{completed}/{total} objectives complete";
        }
    }
}
