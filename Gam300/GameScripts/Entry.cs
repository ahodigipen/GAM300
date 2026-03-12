using System;
using Boom;

namespace GameScripts
{
    public static class Entry
    {
        // Scene flow: MainMenu -> Cutscene -> Gameplay
        public const string CUTSCENE_SCENE_NAME = "START CUTSCENE";
        public const string GAMEPLAY_SCENE_NAME = "M3 GAMEPLAY";
        public const string LEVEL_SCENE_NAME = GAMEPLAY_SCENE_NAME; // Alias for compatibility

        public const string PAUSE_SCENE_NAME = "PauseMenu";
        public const string MAIN_MENU_SCENE_NAME = "MainMenu";
        public const string HOW_TO_PLAY_SCENE_NAME = "HowToPlay";
        public const string DEATH_SCENE_NAME = "DeathMenu";
        public const string END_SCENE_NAME = "EndMenu";
        public const string INVENTORY_SCENE_NAME = "InventoryMenu";

        public const string OUTRO_SCENE_NAME = "OUTRO SCENE";
        public const string POPUP_SCENE_NAME = "PopUpMenu";
        public const string LEVEL_1_UI = "Level1PopUp";
        public static string _activePopupName = "";

        public static string _currentSceneName;
        public static bool IsGamePaused = false;
        public static bool IsPlayerDead = false;
        public static bool IsGameEnded = false;
        public static bool IsInventoryOpen = false;

        public static bool IsStartPopupActive = false;
        private static float _sceneInputDebounceTimer = 0.0f;
        private static float _startSequenceDelay = -1f; // Delay timer for Start Sequence

        public static bool CanProcessInput => _sceneInputDebounceTimer <= 0.0f;

        public enum PauseMenuAction
        {
            None,
            Resume,
            Restart,
            MainMenu,
            Quit
        }

        public enum DeathMenuAction
        {
            None,
            Restart,
            MainMenu
        }

        public enum EndMenuAction
        {
            None,
            Restart,
            MainMenu
        }

        public enum InventoryMenuAction
        {
            None,
            Close
        }

        public static PauseMenuAction s_RequestedPauseAction = PauseMenuAction.None;
        public static DeathMenuAction s_RequestedDeathAction = DeathMenuAction.None;
        public static EndMenuAction s_RequestedEndAction = EndMenuAction.None;
        public static InventoryMenuAction s_RequestedInventoryAction = InventoryMenuAction.None;

        private static bool _p_KeyWasDown = false;
        private static bool _escape_KeyWasDown = false;
        private static bool _i_KeyWasDown = false;
        private static bool _start_ButtonWasDown = false;
        private static bool _rightBracket_KeyWasDown = false;

        public static PauseMenu s_ActivePauseMenuInstance = null;
        public static DeathMenu s_ActiveDeathMenuInstance = null;
        public static EndMenu s_ActiveEndMenuInstance = null;
        public static InventoryMenu s_ActiveInventoryMenuInstance = null;

        public static void Start()
        {
            _p_KeyWasDown = false;
            _escape_KeyWasDown = false;
            _start_ButtonWasDown = false;
            _rightBracket_KeyWasDown = false;

            IsGamePaused = false;
            IsPlayerDead = false;
            IsGameEnded = false;
            IsInventoryOpen = false;

            IsStartPopupActive = false;
            _sceneInputDebounceTimer = 0.5f;
            _startSequenceDelay = 0.2f; // No delay — start dialogue fades in immediately

            s_RequestedPauseAction = PauseMenuAction.None;
            s_RequestedDeathAction = DeathMenuAction.None;
            s_RequestedEndAction = EndMenuAction.None;
            s_RequestedInventoryAction = InventoryMenuAction.None;

            _currentSceneName = API.GetCurrentSceneName();
            API.EnableFileWatcher(true);
            API.SetGameLogicPaused(false);
            API.SetGameEnd(false);

            s_ActivePauseMenuInstance = null;
            s_ActiveDeathMenuInstance = null;
            s_ActiveEndMenuInstance = null;
            s_ActiveInventoryMenuInstance = null;

            SettingsManager.LoadSettings();

            // Clear stale static registries from previous play session
            SpotlightFollower.ClearRegistry();
            TutorialManager.Reset();
            CrouchTutorialManager.Reset();
            StoryDialogueManager.Reset();

            API.Log("[C#] Entry.Start() called for scene: " + _currentSceneName);

            // Fade in from black at every scene start
            SceneFader.StartFadeIn(0.6f);

            _activePopupName = LEVEL_1_UI;

            // Only pre-load menus for gameplay scene, not for cutscene
            if (_currentSceneName == GAMEPLAY_SCENE_NAME)
            {
                API.Log("Pre-loading pause menu additively...");
                API.LoadSceneAdditive(PAUSE_SCENE_NAME);
                API.LoadSceneAdditive(DEATH_SCENE_NAME);
                API.LoadSceneAdditive(END_SCENE_NAME);
                API.LoadSceneAdditive(INVENTORY_SCENE_NAME);
            }
        }

        public static void TriggerModalPopup(string uiEntityName)
        {
            if (IsStartPopupActive) return; // Don't overlap popups

            API.Log($"[Entry] Triggering Modal Popup: {uiEntityName}");

            _activePopupName = uiEntityName; // Remember what to close

            // Find the UI and make sure it's visible (reset scale if needed)
            ulong uiID = API.FindEntity(_activePopupName);
            if (uiID != 0)
            {
                // Ensure it's visible if you used scale 0 to hide it previously
                API.SetScale(uiID, new Vec3(1, 1, 1));
            }
            else
            {
                API.Log($"[Entry] WARNING: Could not find UI Entity '{_activePopupName}'");
            }

            IsStartPopupActive = true;
            API.SetGameLogicPaused(true);
        }

        public static void OnCutsceneCompleted()
        {
            if (_currentSceneName == GAMEPLAY_SCENE_NAME && !StoryDialogueManager.IsSequenceActive())
            {
                API.Log("[Entry] Cutscene Finished. Triggering Start Dialogue Sequence...");
                StoryDialogueManager.PlayStartSequence();
            }
        }

        public static void Update(float dt)
        {
            // Always advance the screen fader first
            SceneFader.Update(dt);

            // Block all game logic while fading out to a new scene
            if (SceneFader.IsFadingOut) return;

            if (_sceneInputDebounceTimer > 0.0f)
            {
                _sceneInputDebounceTimer -= dt;
            }

            if (_currentSceneName == GAMEPLAY_SCENE_NAME && _startSequenceDelay > 0.0f)
            {
                _startSequenceDelay -= dt;
                if (_startSequenceDelay <= 0.0f && !StoryDialogueManager.IsSequenceActive())
                {
                    API.Log("[Entry] Delay finished. Triggering Start Dialogue Sequence...");
                    StoryDialogueManager.PlayStartSequence();
                }
            }

            // Update tutorial manager (handles key/freeze tutorial input even when paused)
            TutorialManager.Update(dt);

            // Update tutorial popup trigger (handles Level 2 popup input even when paused)
            TutorialPopupTrigger.Update(dt);

            // Update crouch dialogue input
            CrouchTutorialManager.Update(dt);
            
            // Update global UI manager (handles letterbox, hearts, etc.)
            UIManager.Update(dt);

            // Update door dialogue input
            MultiKeyDoor.UpdateDialogue(dt);
            
            // Update story dialogue sequences
            StoryDialogueManager.Update(dt);

            // Update game logic pause state
            // If any popup/tutorial/dialogue is active, we force the game to stay paused
            API.SetGameLogicPaused(IsGamePaused || IsStartPopupActive || TutorialManager.IsTutorialActive() || TutorialPopupTrigger.IsPopupActive() || MultiKeyDoor.IsDialogueActive() || CrouchTutorialManager.IsTutorialActive() || StoryDialogueManager.IsSequenceActive());
            API.SetPlayerDead(IsPlayerDead);
            API.SetGameEnd(IsGameEnded);

            // End Game
            if (s_RequestedEndAction != EndMenuAction.None)
            {
                UpdateEndMenu(dt);
                return;
            }
            if (IsGameEnded)
            {
                if (API.IsEndMenuLoaded()) UpdateEndMenu(dt);
                return;
            }

            // Death
            if (s_RequestedDeathAction != DeathMenuAction.None)
            {
                UpdateDeathMenu(dt);
                return;
            }
            if (IsPlayerDead)
            {
                if (API.IsDeathMenuLoaded()) UpdateDeathMenu(dt);
                return;
            }

            // Inventory
            if (s_RequestedInventoryAction != InventoryMenuAction.None || IsInventoryOpen)
            {
                if (API.IsInventoryMenuLoaded()) UpdateInventoryMenu(dt);
            }

            // Pause
            if (s_RequestedPauseAction == PauseMenuAction.MainMenu ||
                s_RequestedPauseAction == PauseMenuAction.Restart ||
                s_RequestedPauseAction == PauseMenuAction.Quit)
            {
                UpdatePauseMenu(dt);
                return;
            }
            else if (IsGamePaused)
            {
                if (API.IsPauseMenuLoaded()) UpdatePauseMenu(dt);
            }
            else
            {
                UpdateGame(dt);
            }
        }
        public static void TriggerGameEnd()
        {
            if (IsGameEnded) return;

            API.Log("Level Complete! Triggering End Menu...");
            IsGameEnded = true;
            IsGamePaused = false;
            IsPlayerDead = false;
            IsStartPopupActive = false;

            API.SetGameEnd(true);
            API.ShowEndMenu();
            API.EnableFileWatcher(false);
        }

        public static void TriggerPlayerDeath()
        {
            if (IsPlayerDead) return;

            API.Log("Player has died!");
            IsPlayerDead = true;
            IsGamePaused = false; // Ensure we aren't in 'pause' state

            API.ShowDeathMenu();
            API.EnableFileWatcher(false);
        }

        private static void UpdateGame(float dt)
        {
            if (IsPlayerDead) return;

            bool p_KeyDown = API.IsKeyDown(API.KEY_P);
            bool escape_KeyDown = API.IsKeyDown(API.KEY_ESCAPE);
            bool i_KeyDown = API.IsKeyDown(API.KEY_I);
            bool ctrl_KeyDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);
            bool start_ButtonDown = API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_START);
            bool rightBracket_KeyDown = API.IsKeyDown(API.KEY_RIGHT_BRACKET);

            // Handle Tutorial/Dialogue - Skip all input if any tutorial active OR just dismissed this frame
            if (TutorialManager.IsKeyTutorialActive() || TutorialManager.WasJustDismissed() ||
                TutorialPopupTrigger.IsPopupActive() || TutorialPopupTrigger.WasJustDismissed() ||
                MultiKeyDoor.IsDialogueActive() || CrouchTutorialManager.IsTutorialActive() || CrouchTutorialManager.WasJustDismissed() ||
                StoryDialogueManager.IsSequenceActive() || StoryDialogueManager.WasJustDismissed())
            {
                // Keep tracking key states to prevent bleed-through after tutorial dismissal
                _escape_KeyWasDown = escape_KeyDown;
                _p_KeyWasDown = p_KeyDown;
                _i_KeyWasDown = i_KeyDown;
                _start_ButtonWasDown = start_ButtonDown;
                _rightBracket_KeyWasDown = rightBracket_KeyDown;
                return;
            }

            // Handle Start Pop-up Interaction
            if (IsStartPopupActive)
            {
                // Trigger close on ESC (Primary) OR P (Fallback) OR Start (Gamepad)
                bool closeTriggered = (escape_KeyDown && !_escape_KeyWasDown) ||
                                      (p_KeyDown && !_p_KeyWasDown) ||
                                      (start_ButtonDown && !_start_ButtonWasDown);

                if (closeTriggered)
                {
                    API.Log("Closing Level 1 Pop-up...");

                    // 1. Find the UI entity by name and destroy it
                    ulong popupEntity = API.FindEntity(_activePopupName);
                    if (popupEntity != 0) API.DestroyEntity(popupEntity);
                    else API.Log("[Warning] Could not find Pop-up Entity to destroy: " + POPUP_SCENE_NAME);

                    // 2. Unpause the game and update state
                    IsStartPopupActive = false;
                    API.SetGameLogicPaused(false);

                    // 3. Consume the key press so it doesn't trigger Pause Menu in the very next frame
                    _escape_KeyWasDown = true;
                    _p_KeyWasDown = true;
                    _i_KeyWasDown = true;
                    _start_ButtonWasDown = true;
                    return;
                }

                // Keep tracking key state while popup is active to prevent bleed-through
                _escape_KeyWasDown = escape_KeyDown;
                _p_KeyWasDown = p_KeyDown;
                _i_KeyWasDown = i_KeyDown;
                _start_ButtonWasDown = start_ButtonDown;
                _rightBracket_KeyWasDown = rightBracket_KeyDown;
                return;
            }

            if (_currentSceneName == GAMEPLAY_SCENE_NAME)
            {
                // Handle Escape key or Start button to pause
                if ((escape_KeyDown && !_escape_KeyWasDown) || (start_ButtonDown && !_start_ButtonWasDown))
                {
                    API.Log("Pausing game...");
                    IsGamePaused = true;
                    API.ShowPauseMenu();
                    API.EnableFileWatcher(false);

                    _escape_KeyWasDown = escape_KeyDown;
                    _start_ButtonWasDown = start_ButtonDown;
                    return;
                }
                _escape_KeyWasDown = escape_KeyDown;
                _start_ButtonWasDown = start_ButtonDown;

                // Handle P key to pause (legacy support)
                if (p_KeyDown && !_p_KeyWasDown && !ctrl_KeyDown)
                {
                    API.Log("Pausing game (P key)...");
                    IsGamePaused = true;
                    API.ShowPauseMenu();
                    API.EnableFileWatcher(false);

                    _p_KeyWasDown = p_KeyDown;
                    return;
                }
                _p_KeyWasDown = p_KeyDown;

                // Handle I key to open inventory
                if (!IsInventoryOpen && i_KeyDown && !_i_KeyWasDown && !ctrl_KeyDown)
                {
                    API.Log("Opening inventory...");
                    IsInventoryOpen = true;
                    API.ShowInventoryMenu();
                    API.EnableFileWatcher(false);

                    _i_KeyWasDown = i_KeyDown;
                    return;
                }
                _i_KeyWasDown = i_KeyDown;

                // Handle ] key to transition to Outro Scene
                if (rightBracket_KeyDown && !_rightBracket_KeyWasDown)
                {
                    API.Log("[Entry] ']' pressed - transitioning to Outro Scene...");
                    _rightBracket_KeyWasDown = rightBracket_KeyDown;
                    SceneFader.FadeToScene(OUTRO_SCENE_NAME);
                    return;
                }
                _rightBracket_KeyWasDown = rightBracket_KeyDown;
            }
        }

        private static void UpdateInventoryMenu(float dt)
        {
            bool i_KeyDown = API.IsKeyDown(API.KEY_I);

            // Block closing inventory if a tutorial or door dialogue is active
            if (TutorialManager.IsKeyTutorialActive() || TutorialManager.WasJustDismissed() ||
                TutorialPopupTrigger.IsPopupActive() || TutorialPopupTrigger.WasJustDismissed() ||
                MultiKeyDoor.IsDialogueActive() || CrouchTutorialManager.IsTutorialActive() || CrouchTutorialManager.WasJustDismissed() ||
                StoryDialogueManager.IsSequenceActive() || StoryDialogueManager.WasJustDismissed())
            {
                _i_KeyWasDown = i_KeyDown;
                // We don't return entirely, just bypass the close logic so the menu keeps rendering/updating
                // But wait, the menu rendering is inside this function!
                // Actually, the rest of this function processes the s_RequestedInventoryAction
                // So bypassing the input check is enough.
            }
            else
            {
                // I key closes the inventory
                if (i_KeyDown && !_i_KeyWasDown)
                {
                    s_RequestedInventoryAction = InventoryMenuAction.Close;
                    _i_KeyWasDown = i_KeyDown;
                }
                _i_KeyWasDown = i_KeyDown;
            }

            switch (s_RequestedInventoryAction)
            {
                case InventoryMenuAction.Close:
                    API.Log("Closing inventory...");
                    s_RequestedInventoryAction = InventoryMenuAction.None;
                    IsInventoryOpen = false;
                    // API.UnloadInventoryMenu();
                    API.EnableFileWatcher(true);
                    return;
            }
        }

        private static void UpdateEndMenu(float dt)
        {
            switch (s_RequestedEndAction)
            {
                case EndMenuAction.MainMenu:
                    API.Log("EndMenu: Returning to Main Menu...");
                    IsGameEnded = false;
                    API.SetGameEnd(false);
                    API.EnableFileWatcher(true);
                    s_RequestedEndAction = EndMenuAction.None;
                    SceneFader.FadeToScene(MAIN_MENU_SCENE_NAME);
                    return;

                case EndMenuAction.Restart:
                    API.Log("EndMenu: Restarting scene...");
                    IsGameEnded = false;
                    API.SetGameEnd(false);
                    PlayerInventory.Reset();
                    API.EnableFileWatcher(true);
                    s_RequestedEndAction = EndMenuAction.None;
                    SceneFader.FadeToScene(_currentSceneName);
                    return;
            }
        }

        private static void UpdateDeathMenu(float dt)
        {
            switch (s_RequestedDeathAction)
            {
                case DeathMenuAction.MainMenu:
                    API.Log("DeathMenu: Returning to Main Menu...");
                    IsPlayerDead = false;
                    API.SetPlayerDead(false);
                    API.EnableFileWatcher(true);
                    s_RequestedDeathAction = DeathMenuAction.None;
                    SceneFader.FadeToScene(MAIN_MENU_SCENE_NAME);
                    return;
                case DeathMenuAction.Restart:
                    API.Log("DeathMenu: Restarting scene...");
                    IsPlayerDead = false;
                    PlayerInventory.Reset();
                    API.SetPlayerDead(false);
                    API.EnableFileWatcher(true);
                    s_RequestedDeathAction = DeathMenuAction.None;
                    SceneFader.FadeToScene(_currentSceneName);
                    return;
            }
        }

        private static void UpdatePauseMenu(float dt)
        {
            bool escape_KeyDown = API.IsKeyDown(API.KEY_ESCAPE);
            bool start_ButtonDown = API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_START);

            // Handle Escape key or Start button to resume
            if ((escape_KeyDown && !_escape_KeyWasDown) || (start_ButtonDown && !_start_ButtonWasDown))
            {
                API.Log("Resuming game...");
                s_RequestedPauseAction = PauseMenuAction.Resume;
                _escape_KeyWasDown = escape_KeyDown;
                _start_ButtonWasDown = start_ButtonDown;
                return;
            }
            _escape_KeyWasDown = escape_KeyDown;
            _start_ButtonWasDown = start_ButtonDown;

            switch (s_RequestedPauseAction)
            {
                case PauseMenuAction.Resume:
                    s_RequestedPauseAction = PauseMenuAction.None;
                    ResumeGame();
                    return;

                case PauseMenuAction.MainMenu:
                    API.Log("Returning to Main Menu (Button Click)...");
                    IsGamePaused = false;
                    API.EnableFileWatcher(true);
                    s_RequestedPauseAction = PauseMenuAction.None;
                    SceneFader.FadeToScene(MAIN_MENU_SCENE_NAME);
                    return;

                case PauseMenuAction.Restart:
                    API.Log("Restarting scene (Button Click)...");
                    IsGamePaused = false;
                    PlayerInventory.Reset();
                    API.EnableFileWatcher(true);
                    s_RequestedPauseAction = PauseMenuAction.None;
                    SceneFader.FadeToScene(_currentSceneName);
                    return;

                case PauseMenuAction.Quit:
                    API.Log("Quitting game (Button Click)...");
                    s_RequestedPauseAction = PauseMenuAction.None;
                    API.ShutdownApplication();
                    return;
            }
        }
        public static void ResumeGame()
        {
            API.Log("Resuming game (Button click)...");

            if (s_ActivePauseMenuInstance != null)
            {
                s_ActivePauseMenuInstance.ResetButtonState();
            }

            API.UnloadPauseMenu();
            IsGamePaused = false;
            API.EnableFileWatcher(true);
        }
    }
}