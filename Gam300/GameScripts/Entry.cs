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

        public const string POPUP_SCENE_NAME = "PopUpMenu";
        public const string LEVEL_1_UI = "Level1PopUp";

        public static string _currentSceneName;
        public static bool IsGamePaused = false;
        public static bool IsPlayerDead = false;
        public static bool IsGameEnded = false;

        public static bool IsStartPopupActive = false;
        private static float _sceneInputDebounceTimer = 0.0f;
        public static bool CanProcessInput => _sceneInputDebounceTimer <= 0.0f;

        // GLFW key constants
        public const int KEY_ESCAPE = 256;

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

        public static PauseMenuAction s_RequestedPauseAction = PauseMenuAction.None;
        public static DeathMenuAction s_RequestedDeathAction = DeathMenuAction.None;
        public static EndMenuAction s_RequestedEndAction = EndMenuAction.None;

        private static bool _p_KeyWasDown = false;
        private static bool _escape_KeyWasDown = false;

        public static PauseMenu s_ActivePauseMenuInstance = null;
        public static DeathMenu s_ActiveDeathMenuInstance = null;
        public static EndMenu s_ActiveEndMenuInstance = null;

        public static void Start()
        {
            _p_KeyWasDown = false;
            _escape_KeyWasDown = false;

            IsGamePaused = false;
            IsPlayerDead = false;
            IsGameEnded = false;

            IsStartPopupActive = false;
            _sceneInputDebounceTimer = 0.5f;

            s_RequestedPauseAction = PauseMenuAction.None;
            s_RequestedDeathAction = DeathMenuAction.None;
            s_RequestedEndAction = EndMenuAction.None;

            _currentSceneName = API.GetCurrentSceneName();
            API.EnableFileWatcher(true);
            API.SetGameLogicPaused(false);
            API.SetGameEnd(false);

            s_ActivePauseMenuInstance = null;
            s_ActiveDeathMenuInstance = null;
            s_ActiveEndMenuInstance = null;

            SettingsManager.LoadSettings();

            API.Log("[C#] Entry.Start() called for scene: " + _currentSceneName);

            // Only pre-load menus for gameplay scene, not for cutscene
            if (_currentSceneName == GAMEPLAY_SCENE_NAME)
            {
                API.Log("Pre-loading pause menu additively...");
                API.LoadSceneAdditive(PAUSE_SCENE_NAME);
                API.LoadSceneAdditive(DEATH_SCENE_NAME);
                API.LoadSceneAdditive(END_SCENE_NAME);
            }
        }

        public static void OnCutsceneCompleted()
        {
            if (_currentSceneName == GAMEPLAY_SCENE_NAME && !IsStartPopupActive)
            {
                API.Log("[Entry] Cutscene Finished. Loading Level 1 Pop-up...");
                API.LoadSceneAdditive(POPUP_SCENE_NAME);
                ulong camEntity = API.FindEntity("Pop Up Camera");
                if (camEntity != 0) API.DestroyEntity(camEntity);
                IsStartPopupActive = true;
                API.SetGameLogicPaused(true);
            }
        }

        public static void Update(float dt)
        {
            if (_sceneInputDebounceTimer > 0.0f)
            {
                _sceneInputDebounceTimer -= dt;
            }

            // Update game logic pause state
            // If the popup is active, we force the game to stay paused
            API.SetGameLogicPaused(IsGamePaused || IsStartPopupActive);
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
            bool escape_KeyDown = API.IsKeyDown(KEY_ESCAPE);
            bool ctrl_KeyDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            // Handle Start Pop-up Interaction
            if (IsStartPopupActive)
            {
                // Trigger close on ESC (Primary) OR P (Fallback)
                bool closeTriggered = (escape_KeyDown && !_escape_KeyWasDown) ||
                                      (p_KeyDown && !_p_KeyWasDown);

                if (closeTriggered)
                {
                    API.Log("Closing Level 1 Pop-up...");

                    // 1. Find the UI entity by name and destroy it
                    ulong popupEntity = API.FindEntity(LEVEL_1_UI);
                    if (popupEntity != 0) API.DestroyEntity(popupEntity);
                    else API.Log("[Warning] Could not find Pop-up Entity to destroy: " + POPUP_SCENE_NAME);

                    // 2. Unpause the game and update state
                    IsStartPopupActive = false;
                    API.SetGameLogicPaused(false);

                    // 3. Consume the key press so it doesn't trigger Pause Menu in the very next frame
                    _escape_KeyWasDown = true;
                    _p_KeyWasDown = true;
                    return;
                }

                // Keep tracking key state while popup is active to prevent bleed-through
                _escape_KeyWasDown = escape_KeyDown;
                _p_KeyWasDown = p_KeyDown;
                return;
            }

            if (_currentSceneName == GAMEPLAY_SCENE_NAME)
            {
                // Handle Escape key to pause
                if (escape_KeyDown && !_escape_KeyWasDown)
                {
                    API.Log("Pausing game (Escape key)...");
                    IsGamePaused = true;
                    API.ShowPauseMenu();
                    API.EnableFileWatcher(false);

                    _escape_KeyWasDown = escape_KeyDown;
                    return;
                }
                _escape_KeyWasDown = escape_KeyDown;

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
                    API.LoadScene(MAIN_MENU_SCENE_NAME);
                    return;

                case EndMenuAction.Restart:
                    API.Log("EndMenu: Restarting scene...");
                    IsGameEnded = false;
                    API.SetGameEnd(false);
                    PlayerInventory.Reset();
                    API.EnableFileWatcher(true);
                    s_RequestedEndAction = EndMenuAction.None;
                    API.LoadScene(_currentSceneName);
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
                    API.LoadScene(MAIN_MENU_SCENE_NAME);
                    return;
                case DeathMenuAction.Restart:
                    API.Log("DeathMenu: Restarting scene...");
                    IsPlayerDead = false;
                    PlayerInventory.Reset();
                    API.SetPlayerDead(false);
                    API.EnableFileWatcher(true);
                    s_RequestedDeathAction = DeathMenuAction.None;
                    API.LoadScene(_currentSceneName);
                    return;
            }
        }

        private static void UpdatePauseMenu(float dt)
        {
            bool escape_KeyDown = API.IsKeyDown(KEY_ESCAPE);

            // Handle Escape key to resume
            if (escape_KeyDown && !_escape_KeyWasDown)
            {
                API.Log("Resuming game (Escape key)...");
                s_RequestedPauseAction = PauseMenuAction.Resume;
                _escape_KeyWasDown = escape_KeyDown;
                return;
            }
            _escape_KeyWasDown = escape_KeyDown;

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
                    API.LoadScene(MAIN_MENU_SCENE_NAME);
                    return;

                case PauseMenuAction.Restart:
                    API.Log("Restarting scene (Button Click)...");
                    IsGamePaused = false;
                    PlayerInventory.Reset();
                    API.EnableFileWatcher(true);
                    s_RequestedPauseAction = PauseMenuAction.None;
                    API.LoadScene(_currentSceneName);
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