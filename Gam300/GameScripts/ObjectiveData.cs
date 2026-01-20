using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Defines the type of objective
    /// </summary>
    public enum ObjectiveType
    {
        Collect,        // Collect N items of a specific type
        ReachZone,      // Reach a specific location/trigger zone
        Interact,       // Interact with specific objects
        DefeatEnemies,  // Defeat N enemies
        SurviveTime     // Survive for X seconds
    }

    /// <summary>
    /// Current state of an objective
    /// </summary>
    public enum ObjectiveState
    {
        Locked,         // Not yet available (prerequisite not met)
        Active,         // Currently in progress
        Completed,      // Successfully completed
        Failed          // Failed (for time-limited objectives)
    }

    /// <summary>
    /// Event types that can be broadcast to the objective system
    /// </summary>
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

    /// <summary>
    /// Event arguments passed when an objective event is broadcast
    /// </summary>
    public class ObjectiveEventArgs
    {
        /// <summary>
        /// The type of event (e.g., "ItemCollected", "ZoneEntered")
        /// </summary>
        public string EventType { get; set; }

        /// <summary>
        /// The identifier/tag for the event target (e.g., "Key", "ExitZone", "Enemy_Guard")
        /// </summary>
        public string TargetId { get; set; }

        /// <summary>
        /// Optional count for the event (defaults to 1)
        /// </summary>
        public int Count { get; set; }

        /// <summary>
        /// Optional entity handle that triggered the event
        /// </summary>
        public ulong SourceEntity { get; set; }

        /// <summary>
        /// Optional world position where the event occurred
        /// </summary>
        public Vec3 Position { get; set; }

        /// <summary>
        /// Optional custom data string for specialized objectives
        /// </summary>
        public string CustomData { get; set; }

        public ObjectiveEventArgs()
        {
            EventType = "";
            TargetId = "";
            Count = 1;
            SourceEntity = 0;
            Position = new Vec3(0, 0, 0);
            CustomData = "";
        }

        public ObjectiveEventArgs(string eventType, string targetId, int count = 1)
        {
            EventType = eventType;
            TargetId = targetId;
            Count = count;
            SourceEntity = 0;
            Position = new Vec3(0, 0, 0);
            CustomData = "";
        }
    }

    /// <summary>
    /// Callback delegate for objective state changes
    /// </summary>
    public delegate void ObjectiveStateChangedHandler(string objectiveId, ObjectiveState oldState, ObjectiveState newState);

    /// <summary>
    /// Callback delegate for objective progress updates
    /// </summary>
    public delegate void ObjectiveProgressHandler(string objectiveId, int currentProgress, int targetProgress);

    /// <summary>
    /// Callback delegate when all required objectives are complete
    /// </summary>
    public delegate void AllObjectivesCompleteHandler();
}
