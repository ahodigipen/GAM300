using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// EndZoneTrigger handles player collision with the end zone.
    /// When the player enters this trigger, it loads the main menu scene.
    /// </summary>
    public class EndZoneTrigger
    {
        public ulong Entity;

        private bool _hasTriggered = false;

        // Deferred scene loading to avoid PhysX crash during trigger callback
        private static bool s_pendingSceneLoad = false;
        private static string s_pendingSceneName = "";

        // Global flag to disable ALL trigger processing during scene transitions
        public static bool s_sceneTransitionInProgress = false;

        // Static instance tracking like DoorTriggerLeft
        private static readonly Dictionary<ulong, EndZoneTrigger> s_instances = new Dictionary<ulong, EndZoneTrigger>();

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
            s_pendingSceneLoad = false;
            s_pendingSceneName = "";
            API.Log("[EndZoneTrigger] Cleared all instances");
        }

        public void OnStart(string jsonParams)
        {
            // Reset static state on scene start
            s_pendingSceneLoad = false;
            s_pendingSceneName = "";

            // Register this instance
            s_instances[Entity] = this;

            // Ensure trigger is configured
            if (!API.HasCollider(Entity))
            {
                API.Log("[EndZoneTrigger] WARNING: Trigger entity has no collider!");
            }
            else if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
            }

            // Register static callbacks
            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnterCallback);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExitCallback);
            API.Log("[EndZoneTrigger] Registered trigger callbacks.");
        }

        public void OnUpdate(float dt)
        {
            // Handle deferred scene loading (outside of physics callback)
            if (s_pendingSceneLoad)
            {
                s_pendingSceneLoad = false;
                string sceneName = s_pendingSceneName;
                s_pendingSceneName = "";

                // Set global flag to disable ALL trigger processing during scene transition
                s_sceneTransitionInProgress = true;

                // CRITICAL: Clear ALL trigger instances BEFORE loading new scene
                // to prevent callbacks firing during scene cleanup
                API.Log("[EndZoneTrigger] Clearing all trigger instances before scene load...");
                KeyPickup.ClearInstances();
                DoorTriggerLeft.ClearInstances();
                CrouchTriggerZone.ClearInstances();
                ObjectiveTrigger.ClearInstances();
                PlayerMovement.ResetStatic();
                MovementAnimator.ResetStatic();
                PlayerManager.Reset();

                // Clear our own instances last
                foreach (var kvp in s_instances)
                {
                    API.UnregisterTriggerCallbacks(kvp.Key);
                }
                s_instances.Clear();

                API.Log($"[EndZoneTrigger] Deferred loading scene: {sceneName}");
                API.LoadScene(sceneName);
                return; // Don't process anything else after scene load
            }
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        // Static callback for trigger enter
        private static void OnTriggerEnterCallback(ulong triggerEntity, ulong otherEntity)
        {
            // Early exit if scene load is pending (prevents stale entity access)
            if (s_pendingSceneLoad) return;

            EndZoneTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Prevent multiple triggers
            if (inst._hasTriggered) return;

            // Only player triggers this
            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            if (playerEntity == 0 || otherEntity != playerEntity) return;

            API.Log("[EndZoneTrigger] Player entered end zone! Queueing MainMenu load...");
            inst._hasTriggered = true;

            // Broadcast zone event for objective system
            ObjectiveManager.BroadcastEvent(ObjectiveEvents.ZoneEntered, "EndZone", 1);

            // Defer scene loading to next frame to avoid PhysX crash
            s_pendingSceneLoad = true;
            s_pendingSceneName = "MainMenu";
        }

        // Static callback for trigger exit
        private static void OnTriggerExitCallback(ulong triggerEntity, ulong otherEntity)
        {
            // Early exit if scene load is pending (prevents stale entity access)
            if (s_pendingSceneLoad) return;

            EndZoneTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react to player exiting
            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            if (playerEntity == 0 || otherEntity != playerEntity) return;

            API.Log("[EndZoneTrigger] Player left end zone");
        }
    }
}