using System;
using Boom;

namespace GameScripts
{
    public static class Entry
    {
        // --- Scene Management ---
        public const string LEVEL_SCENE_NAME = "M2_Redesign_scaled";
        public const string PAUSE_SCENE_NAME = "PauseMenu";
        public const string MAIN_MENU_SCENE_NAME = "MainMenu";
        public const string HOW_TO_PLAY_SCENE_NAME = "HowToPlay";
        public static string _currentSceneName;
        public static bool IsGamePaused = false;

        // Key state trackers
        private static bool _h_KeyWasDown = false;
        private static bool _p_KeyWasDown = false;
        private static bool _r_KeyWasDown = false;
        private static bool _y_KeyWasDown = false;
        private static bool _m_KeyWasDown = false;
        private static bool _q_KeyWasDown = false;

        public static PauseMenu s_ActivePauseMenuInstance = null;


        public static void Start()
        {
            // Reset keys every time a scene loads
            _h_KeyWasDown = false;
            _p_KeyWasDown = false;
            _r_KeyWasDown = false;
            _y_KeyWasDown = false;
            _m_KeyWasDown = false;
            _q_KeyWasDown = false;
            IsGamePaused = false;

            _currentSceneName = API.GetCurrentSceneName();
            API.EnableFileWatcher(true);
            API.SetGameLogicPaused(false);
            s_ActivePauseMenuInstance = null;

            API.Log("[C#] Entry.Start() called for scene: " + _currentSceneName);

            if (_currentSceneName == LEVEL_SCENE_NAME)
            {
                API.Log("Pre-loading pause menu additively...");
                API.LoadSceneAdditive(PAUSE_SCENE_NAME);
            }
        }

        public static void Update(float dt)
        {
            int state = API.GetApplicationState();

            if (state == API.APP_STATE_RUNNING)
            {
                // 1. Run the correct C# logic block
                if (IsGamePaused)
                {
                    if (API.IsPauseMenuLoaded())
                    {
                        UpdatePauseMenu(dt);
                    }
                }
                else
                {
                    UpdateGame(dt);
                }

                // 2. Set the C++ pause state *after* all logic is done.
                // This reflects the *final* state for the frame and fixes Bug 1.
                API.SetGameLogicPaused(IsGamePaused);
            }

        }

        // This function runs all your game logic
        private static void UpdateGame(float dt)
        {
            // Get key states once
            bool p_KeyDown = API.IsKeyDown(API.KEY_P); // Menu Play
            bool h_KeyDown = API.IsKeyDown(API.KEY_H); // Menu How to Play
            bool r_KeyDown = API.IsKeyDown(API.KEY_R); // How to Play return to Menu
            bool q_KeyDown = API.IsKeyDown(API.KEY_Q); // Quit
            bool ctrl_KeyDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            // --- LOGIC FOR IN-GAME ---

            // Check for 'P' (without Ctrl) to PAUSE the game
            if (p_KeyDown && !_p_KeyWasDown && !ctrl_KeyDown)
            {
                API.Log("Pausing game (P key)...");
                IsGamePaused = true;
                //API.LoadSceneAdditive(PAUSE_SCENE_NAME); // C++ Caching logic runs

                API.ShowPauseMenu();
                // Disable hot-reload to prevent auto-resume
                API.EnableFileWatcher(false);

                // API.SetGameLogicPaused(true); // <-- This is now handled in Update()

                s_ActivePauseMenuInstance = null;

                _p_KeyWasDown = p_KeyDown;
                return;
            }
            _p_KeyWasDown = p_KeyDown;

            // NOTE: Player movement is now handled by PlayerMovement.cs script
            // The PlayerMovement script should be attached to the player entity
        }

        // This function runs all your pause menu logic
        private static void UpdatePauseMenu(float dt)
        {
            // --- Resume Button (R) ---
            bool r_KeyDown = API.IsKeyDown(API.KEY_R);
            if (r_KeyDown && !_r_KeyWasDown)
            {
                API.Log("Resuming game...");

                API.UnloadPauseMenu(); // C++ Caching logic runs
                IsGamePaused = false;

                // Re-enable hot-reload
                API.EnableFileWatcher(true);

                // API.SetGameLogicPaused(false); // <-- Handled in Update()

                _r_KeyWasDown = r_KeyDown;
                return;
            }
            _r_KeyWasDown = r_KeyDown;

            // --- Main Menu Button (M) ---
            bool m_KeyDown = API.IsKeyDown(API.KEY_M);
            if (m_KeyDown && !_m_KeyWasDown)
            {
                API.Log("Returning to Main Menu...");
                IsGamePaused = false;
                // API.TogglePause();
                API.EnableFileWatcher(true);
                API.LoadScene(MAIN_MENU_SCENE_NAME); // 2. ...THEN load the new scene (this will clear everything)
                _m_KeyWasDown = m_KeyDown;
                return;
            }
            _m_KeyWasDown = m_KeyDown;

            // --- Restart Button (Y) ---
            bool y_KeyDown = API.IsKeyDown(API.KEY_Y);
            if (y_KeyDown && !_y_KeyWasDown)
            {
                API.Log("Restarting scene...");
                IsGamePaused = false;

                // API.SetGameLogicPaused(false); // <-- Handled in Update()

                // --- NEW (Fixes Bug 2) ---
                // Re-enable hot-reload
                API.EnableFileWatcher(true);

                API.LoadScene(_currentSceneName);

                _y_KeyWasDown = y_KeyDown;
                return;
            }
            _y_KeyWasDown = y_KeyDown;

            // --- Quit Button (Q) ---
            bool q_KeyDown = API.IsKeyDown(API.KEY_Q);
            if (q_KeyDown && !_q_KeyWasDown)
            {
                API.Log("Quitting game...");
                API.QuitGame();
                //API.Log("Quitting scene (temporary)...");
                //IsGamePaused = false;
                //API.LoadScene(_currentSceneName);

                _q_KeyWasDown = q_KeyDown;
                return;
            }
            _q_KeyWasDown = q_KeyDown;

            if (s_ActivePauseMenuInstance != null)
            {
                s_ActivePauseMenuInstance.OnUpdate(dt);
            }
        }

        public static void ResumeGame()
        {
            API.Log("Resuming game (Button click)...");


            API.UnloadPauseMenu(); // 1. Destroy the menu objects
            IsGamePaused = false;
            //API.SetGameLogicPaused(false);
            API.EnableFileWatcher(true);
            s_ActivePauseMenuInstance = null;
        }
    }
}