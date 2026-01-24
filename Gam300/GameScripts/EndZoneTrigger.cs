using System;
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
        private static bool s_pendingBroadcast = false;
        private static string s_pendingSceneName = "";
        private static float s_sceneLoadDelay = 0f;
        private const float SCENE_LOAD_DELAY_TIME = 0.1f; // 100ms delay like pause menu

        // Global flag to disable ALL trigger processing during scene transitions
        public static bool s_sceneTransitionInProgress = false;

        // Static instance tracking like DoorTriggerLeft
        private static readonly Dictionary<ulong, EndZoneTrigger> s_instances = new Dictionary<ulong, EndZoneTrigger>();

        /// <summary>
        /// Clear all static instances (call on scene change to prevent stale entity access)
        /// </summary>
        public static void ClearInstances()
        {
            s_instances.Clear();
            s_pendingSceneLoad = false;
            s_pendingBroadcast = false;
            s_pendingSceneName = "";
            API.Log("[EndZoneTrigger] Cleared all instances");
        }

        public void OnStart(string jsonParams)
        {
            // Reset static state on scene start
            s_pendingSceneLoad = false;
            s_pendingBroadcast = false;
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
            // Handle deferred objective broadcast
            if (s_pendingBroadcast)
            {
                s_pendingBroadcast = false;
                ObjectiveManager.BroadcastEvent(ObjectiveEvents.ZoneEntered, "EndZone", 1);
            }

            // Handle deferred scene loading with delay (like pause menu)
            if (s_pendingSceneLoad)
            {
                // Set transition flag immediately to block all trigger callbacks
                if (!s_sceneTransitionInProgress)
                {
                    s_sceneTransitionInProgress = true;
                    s_sceneLoadDelay = SCENE_LOAD_DELAY_TIME;
                    API.Log("[EndZoneTrigger] Scene transition started, waiting for delay...");
                }

                // Count down the delay
                s_sceneLoadDelay -= dt;
                if (s_sceneLoadDelay > 0f)
                {
                    return; // Wait for delay to complete
                }

                // Delay complete - now load the scene
                s_pendingSceneLoad = false;
                string sceneName = s_pendingSceneName;
                s_pendingSceneName = "";

                API.Log($"[EndZoneTrigger] Delay complete, loading scene: {sceneName}");
                API.LoadScene(sceneName);
                return;
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
            EndZoneTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only player triggers this
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;
            // Prevent multiple triggers
            if (inst._hasTriggered) return;

            API.Log("[EndZoneTrigger] Player entered end zone! Loading MainMenu...");
            inst._hasTriggered = true;
            Entry.TriggerGameEnd();
        }

        // Static callback for trigger exit
        private static void OnTriggerExitCallback(ulong triggerEntity, ulong otherEntity)
        {
            // Absolute first check - if scene transition in progress, do nothing
            if (s_sceneTransitionInProgress || s_pendingSceneLoad) return;

            try
            {
                EndZoneTrigger inst;
                if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

                // Only react to player exiting
                ulong playerEntity = PlayerMovement.GetPlayerEntity();
                if (playerEntity == 0 || otherEntity != playerEntity) return;

                API.Log("[EndZoneTrigger] Player left end zone");
            }
            catch (Exception ex)
            {
                API.Log($"[EndZoneTrigger] OnTriggerExitCallback error: {ex.Message}");
            }
        }
    }
}