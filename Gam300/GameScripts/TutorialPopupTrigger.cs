using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attach to a trigger zone where a tutorial popup should appear
    /// When player enters, pauses game logic and shows popup
    /// Player must press ESC/P to dismiss (like Level 1 popup)
    /// </summary>
    public class TutorialPopupTrigger
    {
        public ulong Entity;

        private static readonly Dictionary<ulong, TutorialPopupTrigger> s_instances = new Dictionary<ulong, TutorialPopupTrigger>();

        // The sprite entity name to show (e.g., "UI_Tutorial")
        [Boom.EditorExposed("Tutorial Sprite", "Name of the sprite entity to show (e.g., UI_Tutorial)")]
        private string _tutorialSpriteName = "UI_Tutorial";

        [Boom.EditorExposed("Only Show Once", "Whether this tutorial should only show once")]
        private bool _onlyShowOnce = true;

        private bool _hasTriggered = false;

        // Optional: Play a sound when entering the zone
        [Boom.EditorExposed("Play Sound On Enter", "Whether to play a sound when player enters the tutorial zone")]
        private bool _playSoundOnEnter = false;

        [Boom.EditorExposed("Enter Sound", "Sound played when player enters the tutorial zone")]
        private string _enterSound = "Resources/Audio/ambient_notification.wav";

        // Static tracking for active popup
        private static bool s_isPopupActive = false;
        private static string s_activeSpriteName = "";
        private static bool s_justDismissed = false;

        // Key state tracking to prevent pause menu conflicts
        private static bool s_escapeKeyWasDown = false;
        private static bool s_pKeyWasDown = false;
        private static bool s_startButtonWasDown = false;

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
            API.Log("[TutorialPopupTrigger] Registered trigger callbacks.");
        }

        public void OnUpdate(float dt)
        {
            // Clear just dismissed flag at start of each frame
            s_justDismissed = false;

            // Handle input for dismissal if popup is active
            if (!s_isPopupActive) return;

            // Check input for manual dismissal
            bool escapeKeyDown = API.IsKeyDown(API.KEY_ESCAPE);
            bool pKeyDown = API.IsKeyDown(API.KEY_P);
            bool startButtonDown = API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_START);

            // Detect key press (edge detection)
            bool escapePressed = escapeKeyDown && !s_escapeKeyWasDown;
            bool pPressed = pKeyDown && !s_pKeyWasDown;
            bool startPressed = startButtonDown && !s_startButtonWasDown;

            if (escapePressed || pPressed || startPressed)
            {
                API.Log("[TutorialPopupTrigger] Dismissing popup via player input");
                DismissPopup();
                
                // Mark that we just dismissed this frame
                s_justDismissed = true;
                
                // Keep key states as "down" to prevent Entry from detecting this as a new press
                s_escapeKeyWasDown = true;
                s_pKeyWasDown = true;
                s_startButtonWasDown = true;
                return;
            }

            // Update key states
            s_escapeKeyWasDown = escapeKeyDown;
            s_pKeyWasDown = pKeyDown;
            s_startButtonWasDown = startButtonDown;
        }

        public void OnDestroy()
        {
            // Cleanup
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            TutorialPopupTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player enters this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // Check if we should only show once
            if (inst._onlyShowOnce && inst._hasTriggered) return;

            // Check if another popup is already active
            if (s_isPopupActive)
            {
                API.Log("[TutorialPopupTrigger] Another popup is already active - skipping");
                return;
            }

            // Show the popup and pause game logic
            ShowPopup(inst);
        }

        private static void ShowPopup(TutorialPopupTrigger inst)
        {
            // Find the sprite entity
            ulong spriteEntity = API.FindEntity(inst._tutorialSpriteName);
            if (spriteEntity == 0)
            {
                API.Log($"[TutorialPopupTrigger] ERROR: Could not find sprite entity '{inst._tutorialSpriteName}'");
                return;
            }

            // Show the sprite
            UIManager.ShowTutorialPopup(spriteEntity);

            // Mark as triggered and active
            inst._hasTriggered = true;
            s_isPopupActive = true;
            s_activeSpriteName = inst._tutorialSpriteName;

            // Reset key states to prevent immediate dismissal
            s_escapeKeyWasDown = true;
            s_pKeyWasDown = true;
            s_startButtonWasDown = true;

            // Pause game logic (but not the pause menu)
            API.SetGameLogicPaused(true);

            // Optional: Play notification sound
            if (inst._playSoundOnEnter && API.HasTransform(inst.Entity))
            {
                var p = API.GetPosition(inst.Entity);
                API.PlaySoundAt("sfx_tutorial_enter", inst._enterSound, p, false);
                API.SetSoundVolume("sfx_tutorial_enter", 0.5f);
                API.Set3DMinMaxDistance("sfx_tutorial_enter", 1.0f, 12.0f);
            }

            API.Log($"[TutorialPopupTrigger] Showing popup '{inst._tutorialSpriteName}' - press ESC/P to dismiss");
        }

        private static void DismissPopup()
        {
            if (!s_isPopupActive) return;

            API.Log($"[TutorialPopupTrigger] Dismissing popup '{s_activeSpriteName}'");

            // Hide the tutorial UI
            UIManager.HideTutorialPopup();

            // Unpause game logic
            API.SetGameLogicPaused(false);

            // Reset state
            s_isPopupActive = false;
            s_activeSpriteName = "";
        }

        /// <summary>
        /// Check if a popup is currently active
        /// </summary>
        public static bool IsPopupActive()
        {
            return s_isPopupActive;
        }

        /// <summary>
        /// Check if a popup was just dismissed this frame
        /// </summary>
        public static bool WasJustDismissed()
        {
            return s_justDismissed;
        }
    }
}
