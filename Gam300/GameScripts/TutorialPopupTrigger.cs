using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attach to a trigger zone where a tutorial popup should appear
    /// Shows a tutorial UI sprite when player enters
    /// Hides the tutorial UI when player exits
    /// </summary>
    public class TutorialPopupTrigger
    {
        public ulong Entity;

        private static readonly Dictionary<ulong, TutorialPopupTrigger> s_instances = new Dictionary<ulong, TutorialPopupTrigger>();

        private static bool _globalEnabled = false;

        // The sprite entity name to show/hide (e.g., "UI_L2_Tutorial")
        [Boom.EditorExposed("Tutorial Sprite", "Name of the sprite entity to show (e.g., UI_Tutorial)")]
        private string _tutorialSpriteName = "UI_Tutorial";

        [Boom.EditorExposed("Only Show Once", "Whether this tutorial should only show once")]
        private bool _onlyShowOnce = true;

        private bool _hasTriggered = false;
        private bool _isCurrentlyShowing = false;
        private bool _isPlayerInside = false;

        // Optional: Play a sound when entering the zone
        [Boom.EditorExposed("Play Sound On Enter", "Whether to play a sound when player enters the tutorial zone")]
        private bool _playSoundOnEnter = false;

        [Boom.EditorExposed("Enter Sound", "Sound played when player enters the tutorial zone")]
        private string _enterSound = "Resources/Audio/ambient_notification.wav";

        public static void EnableAllTutorials()
        {
            if (_globalEnabled) return; // Already enabled

            API.Log("[TutorialPopupTrigger] Global Tutorials ENABLED.");
            _globalEnabled = true;

            // Check if player is ALREADY inside any trigger waiting for this
            foreach (var kvp in s_instances)
            {
                TutorialPopupTrigger inst = kvp.Value;
                if (inst._isPlayerInside && !inst._isCurrentlyShowing)
                {
                    inst.ShowPopup(); // Force show immediately
                }
            }
        }

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;

            if (!API.HasCollider(Entity))
            {
                API.Log("[TutorialPopupTrigger] WARNING: Entity has no collider. Trigger will not work.");
                return;
            }

            // Ensure this collider is a trigger
            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log("[TutorialPopupTrigger] Collider set to IsTrigger = true.");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log("[TutorialPopupTrigger] Registered trigger callbacks.");
        }

        public void OnUpdate(float dt)
        {
            // No-op
        }

        public void OnDestroy()
        {
            // Cleanup
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private void ShowPopup()
        {
            if (_onlyShowOnce && _hasTriggered) return;

            Entry.TriggerModalPopup(_tutorialSpriteName);

            _hasTriggered = true;
            _isCurrentlyShowing = true;
            API.Log("[TutorialPopupTrigger] Showing UI popup.");
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            TutorialPopupTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player enters this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // Check if we should only show once
            if (inst._onlyShowOnce && inst._hasTriggered) return;

            // *** Show the tutorial UI prompt ***
            UIManager.ShowTutorialPopup(API.FindEntity(inst._tutorialSpriteName));
            inst._hasTriggered = true;
            inst._isCurrentlyShowing = true;

            // Optional: Play notification sound
            if (inst._playSoundOnEnter && API.HasTransform(inst.Entity))
            {
                var p = API.GetPosition(inst.Entity);
                API.PlaySoundAt("sfx_tutorial_enter", inst._enterSound, p, false);
                API.SetSoundVolume("sfx_tutorial_enter", 0.5f);
                API.Set3DMinMaxDistance("sfx_tutorial_enter", 1.0f, 12.0f);
            }

            API.Log("[TutorialPopupTrigger] Player entered tutorial zone - showing UI popup");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            TutorialPopupTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player exits this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // Only hide if we were the ones showing it
            if (!inst._isCurrentlyShowing) return;

            // *** Hide the tutorial UI prompt ***
            UIManager.HideTutorialPopup();
            inst._isCurrentlyShowing = false;

            API.Log("[TutorialPopupTrigger] Player exited tutorial zone - hiding UI popup");
        }
    }
}
