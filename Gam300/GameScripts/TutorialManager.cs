using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Manages one-time tutorial popups for key and freeze pickups
    /// Both tutorials require manual dismissal with ESC/P
    /// </summary>
    public static class TutorialManager
    {
        // Track whether tutorials have been shown
        private static bool s_keyTutorialShown = false;
        private static bool s_freezeTutorialShown = false;

        // Tutorial scene names
        private const string KEY_TUTORIAL_SCENE = "TutorialKey";
        private const string FREEZE_TUTORIAL_SCENE = "TutorialFreeze";

        // Tutorial UI entity names (must match entity names in the tutorial scenes)
        private const string KEY_TUTORIAL_UI = "KeyTutorialPopup";
        private const string FREEZE_TUTORIAL_UI = "FreezeTutorialPopup";

        // GLFW key constants
        private const int KEY_ESCAPE = 256;
        private const int KEY_P = 80;

        // Active tutorial tracking
        private static bool s_isTutorialActive = false;
        private static string s_activeTutorialUI = "";
        private static bool s_justDismissed = false; // Track if tutorial was dismissed this frame

        // Key state tracking to prevent pause menu conflicts
        private static bool s_escapeKeyWasDown = false;
        private static bool s_pKeyWasDown = false;
        private static bool s_startButtonWasDown = false;

        /// <summary>
        /// Reset all tutorial states (call this on game restart)
        /// </summary>
        public static void Reset()
        {
            s_keyTutorialShown = false;
            s_freezeTutorialShown = false;
            s_isTutorialActive = false;
            s_activeTutorialUI = "";
            s_justDismissed = false;
            s_escapeKeyWasDown = false;
            s_pKeyWasDown = false;
            s_startButtonWasDown = false;
            API.Log("[TutorialManager] Reset");
        }

        /// <summary>
        /// Show the key pickup tutorial if it hasn't been shown yet
        /// This tutorial requires manual dismissal (ESC/P)
        /// </summary>
        public static void ShowKeyTutorial()
        {
            if (s_keyTutorialShown)
            {
                API.Log("[TutorialManager] Key tutorial already shown - skipping");
                return;
            }

            API.Log("[TutorialManager] Showing key tutorial (manual dismiss with ESC/P)");
            s_keyTutorialShown = true;
            ShowTutorial(KEY_TUTORIAL_SCENE, KEY_TUTORIAL_UI);
        }

        /// <summary>
        /// Show the freeze pickup tutorial if it hasn't been shown yet
        /// This tutorial requires manual dismissal (ESC/P)
        /// </summary>
        public static void ShowFreezeTutorial()
        {
            if (s_freezeTutorialShown)
            {
                API.Log("[TutorialManager] Freeze tutorial already shown - skipping");
                return;
            }

            API.Log("[TutorialManager] Showing freeze tutorial (manual dismiss with ESC/P)");
            s_freezeTutorialShown = true;
            ShowTutorial(FREEZE_TUTORIAL_SCENE, FREEZE_TUTORIAL_UI);
        }

        /// <summary>
        /// Internal method to load and display a tutorial scene
        /// </summary>
        private static void ShowTutorial(string sceneName, string uiEntityName)
        {
            if (s_isTutorialActive)
            {
                API.Log("[TutorialManager] Tutorial already active - skipping");
                return;
            }

            // Load the tutorial scene additively
            API.LoadSceneAdditive(sceneName);

            // Set active tutorial tracking
            s_isTutorialActive = true;
            s_activeTutorialUI = uiEntityName;

            // Reset key states to prevent immediate dismissal
            s_escapeKeyWasDown = true;
            s_pKeyWasDown = true;
            s_startButtonWasDown = true;

            API.Log($"[TutorialManager] Loaded tutorial scene '{sceneName}' - press ESC/P to dismiss");
        }

        /// <summary>
        /// Update method - handles manual dismissal input
        /// Should be called from Entry.Update() or similar
        /// </summary>
        public static void Update(float dt)
        {
            // Clear the "just dismissed" flag at the start of each frame
            s_justDismissed = false;

            if (!s_isTutorialActive) return;

            // Check input for manual dismissal
            bool escapeKeyDown = API.IsKeyDown(KEY_ESCAPE);
            bool pKeyDown = API.IsKeyDown(KEY_P);
            bool startButtonDown = API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_START);

            // Detect key press (edge detection: key is down now but wasn't down before)
            bool escapePressed = escapeKeyDown && !s_escapeKeyWasDown;
            bool pPressed = pKeyDown && !s_pKeyWasDown;
            bool startPressed = startButtonDown && !s_startButtonWasDown;

            if (escapePressed || pPressed || startPressed)
            {
                API.Log("[TutorialManager] Manual dismiss triggered by player input");
                DismissTutorial();
                
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

        /// <summary>
        /// Manually dismiss the active tutorial (if any)
        /// </summary>
        public static void DismissTutorial()
        {
            if (!s_isTutorialActive) return;

            API.Log($"[TutorialManager] Dismissing tutorial: {s_activeTutorialUI}");

            // Find and destroy the tutorial UI entity
            ulong tutorialEntity = API.FindEntity(s_activeTutorialUI);
            if (tutorialEntity != 0)
            {
                API.DestroyEntity(tutorialEntity);
                API.Log($"[TutorialManager] Destroyed tutorial UI entity: {s_activeTutorialUI}");
            }
            else
            {
                API.Log($"[TutorialManager] Warning: Could not find tutorial UI entity: {s_activeTutorialUI}");
            }

            // Reset active state
            s_isTutorialActive = false;
            s_activeTutorialUI = "";
        }

        /// <summary>
        /// Check if a tutorial is currently active
        /// </summary>
        public static bool IsTutorialActive()
        {
            return s_isTutorialActive;
        }

        /// <summary>
        /// Check if any tutorial is currently active (for Entry to know to skip pause handling)
        /// </summary>
        public static bool IsKeyTutorialActive()
        {
            // Now returns true for ANY active tutorial (key or freeze)
            return s_isTutorialActive;
        }

        /// <summary>
        /// Check if a tutorial was just dismissed this frame (to prevent pause menu from opening)
        /// </summary>
        public static bool WasJustDismissed()
        {
            return s_justDismissed;
        }

        /// <summary>
        /// Get the current escape key state (for Entry to consume the key press)
        /// </summary>
        public static bool GetEscapeKeyWasDown()
        {
            return s_escapeKeyWasDown;
        }

        /// <summary>
        /// Set the escape key state (for Entry to prevent pause menu conflicts)
        /// </summary>
        public static void SetEscapeKeyWasDown(bool state)
        {
            s_escapeKeyWasDown = state;
        }
    }
}
