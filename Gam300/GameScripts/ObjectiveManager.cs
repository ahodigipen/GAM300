using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Global singleton manager for all objectives in the game.
    /// Handles registration, event broadcasting, and progress tracking.
    /// </summary>
    public static class ObjectiveManager
    {
        // Registered objectives by their unique ID
        private static Dictionary<string, BaseObjective> s_objectives = new Dictionary<string, BaseObjective>();

        // Objectives in order of registration (for display ordering)
        private static List<string> s_objectiveOrder = new List<string>();

        // Event callbacks
        public static event ObjectiveStateChangedHandler OnObjectiveStateChanged;
        public static event ObjectiveProgressHandler OnObjectiveProgress;
        public static event AllObjectivesCompleteHandler OnAllRequiredComplete;

        // Track if all required objectives were completed (to avoid firing event multiple times)
        private static bool s_allRequiredCompleteNotified = false;

        /// <summary>
        /// Reset the objective manager (call on scene start/restart)
        /// </summary>
        public static void Reset()
        {
            s_objectives.Clear();
            s_objectiveOrder.Clear();
            s_allRequiredCompleteNotified = false;
            OnObjectiveStateChanged = null;
            OnObjectiveProgress = null;
            OnAllRequiredComplete = null;
            API.Log("[ObjectiveManager] Reset");
        }

        /// <summary>
        /// Register an objective with the manager
        /// </summary>
        public static void RegisterObjective(BaseObjective objective)
        {
            if (objective == null || string.IsNullOrEmpty(objective.ObjectiveId))
            {
                API.Log("[ObjectiveManager] Cannot register objective: null or empty ID");
                return;
            }

            if (s_objectives.ContainsKey(objective.ObjectiveId))
            {
                API.Log($"[ObjectiveManager] Objective '{objective.ObjectiveId}' already registered, updating");
                s_objectives[objective.ObjectiveId] = objective;
            }
            else
            {
                s_objectives[objective.ObjectiveId] = objective;
                s_objectiveOrder.Add(objective.ObjectiveId);
                API.Log($"[ObjectiveManager] Registered objective: {objective.ObjectiveId} ({objective.ObjectiveType})");
            }
        }

        /// <summary>
        /// Unregister an objective from the manager
        /// </summary>
        public static void UnregisterObjective(string objectiveId)
        {
            if (s_objectives.ContainsKey(objectiveId))
            {
                s_objectives.Remove(objectiveId);
                s_objectiveOrder.Remove(objectiveId);
                API.Log($"[ObjectiveManager] Unregistered objective: {objectiveId}");
            }
        }

        /// <summary>
        /// Broadcast an event to all active objectives
        /// </summary>
        public static void BroadcastEvent(string eventType, string targetId, int count = 1)
        {
            var args = new ObjectiveEventArgs(eventType, targetId, count);
            BroadcastEvent(args);
        }

        /// <summary>
        /// Broadcast an event with full event args to all active objectives
        /// </summary>
        public static void BroadcastEvent(ObjectiveEventArgs args)
        {
            if (args == null) return;

            API.Log($"[ObjectiveManager] Broadcasting event: {args.EventType} (target: {args.TargetId}, count: {args.Count})");

            foreach (var kvp in s_objectives)
            {
                var objective = kvp.Value;
                if (objective.State == ObjectiveState.Active)
                {
                    objective.HandleEvent(args);
                }
            }

            CheckAllRequiredComplete();
        }

        /// <summary>
        /// Update all objectives (call from Entry.Update)
        /// </summary>
        public static void Update(float dt)
        {
            // Check for prerequisites and activate locked objectives if ready
            foreach (var kvp in s_objectives)
            {
                var objective = kvp.Value;

                if (objective.State == ObjectiveState.Locked)
                {
                    if (CheckPrerequisites(objective))
                    {
                        objective.Activate();
                    }
                }
                else if (objective.State == ObjectiveState.Active)
                {
                    objective.Update(dt);
                }
            }
        }

        /// <summary>
        /// Check if an objective's prerequisites are met
        /// </summary>
        private static bool CheckPrerequisites(BaseObjective objective)
        {
            if (string.IsNullOrEmpty(objective.PrerequisiteId))
            {
                return true; // No prerequisite
            }

            if (s_objectives.TryGetValue(objective.PrerequisiteId, out var prereq))
            {
                return prereq.State == ObjectiveState.Completed;
            }

            // Prerequisite objective not found, assume met
            return true;
        }

        /// <summary>
        /// Check if all required objectives are complete
        /// </summary>
        private static void CheckAllRequiredComplete()
        {
            if (s_allRequiredCompleteNotified) return;

            bool allComplete = true;
            bool hasRequired = false;

            foreach (var kvp in s_objectives)
            {
                if (kvp.Value.IsRequired)
                {
                    hasRequired = true;
                    if (kvp.Value.State != ObjectiveState.Completed)
                    {
                        allComplete = false;
                        break;
                    }
                }
            }

            if (hasRequired && allComplete)
            {
                s_allRequiredCompleteNotified = true;
                API.Log("[ObjectiveManager] All required objectives complete!");
                OnAllRequiredComplete?.Invoke();
            }
        }

        /// <summary>
        /// Notify that an objective's state has changed
        /// </summary>
        internal static void NotifyStateChanged(string objectiveId, ObjectiveState oldState, ObjectiveState newState)
        {
            API.Log($"[ObjectiveManager] Objective '{objectiveId}' state: {oldState} -> {newState}");
            OnObjectiveStateChanged?.Invoke(objectiveId, oldState, newState);

            if (newState == ObjectiveState.Completed)
            {
                CheckAllRequiredComplete();
            }
        }

        /// <summary>
        /// Notify that an objective's progress has updated
        /// </summary>
        internal static void NotifyProgress(string objectiveId, int current, int target)
        {
            OnObjectiveProgress?.Invoke(objectiveId, current, target);
        }

        // ===== Query Methods =====

        /// <summary>
        /// Get an objective by its ID
        /// </summary>
        public static BaseObjective GetObjective(string objectiveId)
        {
            if (s_objectives.TryGetValue(objectiveId, out var objective))
            {
                return objective;
            }
            return null;
        }

        /// <summary>
        /// Get all registered objectives
        /// </summary>
        public static IReadOnlyList<BaseObjective> GetAllObjectives()
        {
            var result = new List<BaseObjective>();
            foreach (var id in s_objectiveOrder)
            {
                if (s_objectives.TryGetValue(id, out var obj))
                {
                    result.Add(obj);
                }
            }
            return result;
        }

        /// <summary>
        /// Get all active objectives
        /// </summary>
        public static IReadOnlyList<BaseObjective> GetActiveObjectives()
        {
            var result = new List<BaseObjective>();
            foreach (var id in s_objectiveOrder)
            {
                if (s_objectives.TryGetValue(id, out var obj) && obj.State == ObjectiveState.Active)
                {
                    result.Add(obj);
                }
            }
            return result;
        }

        /// <summary>
        /// Get all completed objectives
        /// </summary>
        public static IReadOnlyList<BaseObjective> GetCompletedObjectives()
        {
            var result = new List<BaseObjective>();
            foreach (var id in s_objectiveOrder)
            {
                if (s_objectives.TryGetValue(id, out var obj) && obj.State == ObjectiveState.Completed)
                {
                    result.Add(obj);
                }
            }
            return result;
        }

        /// <summary>
        /// Check if all required objectives are complete
        /// </summary>
        public static bool AreAllRequiredComplete()
        {
            foreach (var kvp in s_objectives)
            {
                if (kvp.Value.IsRequired && kvp.Value.State != ObjectiveState.Completed)
                {
                    return false;
                }
            }
            return true;
        }

        /// <summary>
        /// Get the number of completed objectives
        /// </summary>
        public static int GetCompletedCount()
        {
            int count = 0;
            foreach (var kvp in s_objectives)
            {
                if (kvp.Value.State == ObjectiveState.Completed)
                {
                    count++;
                }
            }
            return count;
        }

        /// <summary>
        /// Get the total number of objectives
        /// </summary>
        public static int GetTotalCount()
        {
            return s_objectives.Count;
        }

        /// <summary>
        /// Force complete an objective (for debugging/testing)
        /// </summary>
        public static void ForceComplete(string objectiveId)
        {
            if (s_objectives.TryGetValue(objectiveId, out var objective))
            {
                objective.ForceComplete();
            }
        }

        /// <summary>
        /// Force fail an objective (for debugging/testing)
        /// </summary>
        public static void ForceFail(string objectiveId)
        {
            if (s_objectives.TryGetValue(objectiveId, out var objective))
            {
                objective.ForceFail();
            }
        }
    }
}
