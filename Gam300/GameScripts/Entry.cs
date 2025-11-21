using System;
using Boom;

namespace GameScripts
{
    public static class Entry
    {
        // --- Player properties ---
        private static ulong _player;
        private static float _speed = 5f;
        private static float _jumpSpeed = 8f;

        // --- Scene Management ---
        private const string PAUSE_SCENE_NAME = "PauseMenu";
        private const string MAIN_MENU_SCENE_NAME = "MainMenu";
        private const string HOW_TO_PLAY_SCENE_NAME = "HowToPlay";
        private static string _currentSceneName;

        // Key state trackers
        private static bool _p_KeyWasDown = false;
        private static bool _r_KeyWasDown = false;
        private static bool _m_KeyWasDown = false;
        private static bool _q_KeyWasDown = false;

        public static void Start()
        {
            // Reset keys every time a scene loads
            _p_KeyWasDown = false;
            _r_KeyWasDown = false;
            _m_KeyWasDown = false;
            _q_KeyWasDown = false;

            _currentSceneName = API.GetCurrentSceneName();

            // Find the player
            _player = API.FindEntity("Samurai");
            if (_player == 0)
            {
                API.Log("Entry.Start: Player 'Samurai' not found (This is normal for menus).");
            }
        }

        public static void Update(float dt)
        {
            if (API.GetApplicationState() == API.APP_STATE_PAUSED)
            {
                if (API.IsPauseMenuLoaded())
                {
                    // Run pause menu logic
                    UpdatePauseMenu();
                }
            }
            else
            {
                // Run normal game logic
                UpdateGame(dt);
            }
        }

        // This function runs all your game logic
        private static void UpdateGame(float dt)
        {
            if (_currentSceneName == MAIN_MENU_SCENE_NAME || _currentSceneName == HOW_TO_PLAY_SCENE_NAME)
            {
                return;
            }

            // --- 1. Check for Pause Key ---
            bool p_KeyDown = API.IsKeyDown(API.KEY_P);
            bool ctrl_KeyDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            if (p_KeyDown && !_p_KeyWasDown && !ctrl_KeyDown)
            {
                API.Log("Pausing game...");

                API.TogglePause(); // 1. Freeze the game
                API.LoadSceneAdditive(PAUSE_SCENE_NAME); // 2. Load menu on top

                _p_KeyWasDown = p_KeyDown;
                return;
            }
            _p_KeyWasDown = p_KeyDown;

            // --- 2. Run Player Logic ---
            if (_player == 0) return;

            var vel = API.GetLinearVelocity(_player);
            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);
            bool isGrounded = API.IsColliding(_player);

            float dx = 0f, dz = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) dx -= 1f;
                if (API.IsKeyDown(API.KEY_D)) dx += 1f;
                if (API.IsKeyDown(API.KEY_W)) dz -= 1f;
                if (API.IsKeyDown(API.KEY_S)) dz += 1f;
            }

            if (dx != 0f || dz != 0f)
            {
                float len = (float)Math.Sqrt(dx * dx + dz * dz);
                vel.X = (dx / len) * _speed;
                vel.Z = (dz / len) * _speed;
            }
            else
            {
                vel.X = 0f;
                vel.Z = 0f;
            }

            if (allowMove && isGrounded && API.IsKeyDown(API.KEY_SPACE))
            {
                vel.Y = _jumpSpeed;
            }

            API.SetLinearVelocity(_player, vel);
        }

        // This function runs all your pause menu logic
        private static void UpdatePauseMenu()
        {
            // --- Resume Button (R) ---
            bool r_KeyDown = API.IsKeyDown(API.KEY_R);
            if (r_KeyDown && !_r_KeyWasDown)
            {
                API.Log("Resuming game...");

                // --- NEW RESUME LOGIC ---
                API.UnloadPauseMenu(); // 1. Destroy the menu objects
                API.TogglePause();     // 2. Un-freeze the game

                _r_KeyWasDown = r_KeyDown;
                return;
            }
            _r_KeyWasDown = r_KeyDown;

            // --- Main Menu Button (M) ---
            bool m_KeyDown = API.IsKeyDown(API.KEY_M);
            if (m_KeyDown && !_m_KeyWasDown)
            {
                API.Log("Returning to Main Menu...");

                API.TogglePause(); // 1. Un-pause the engine first...
                API.LoadScene(MAIN_MENU_SCENE_NAME); // 2. ...THEN load the new scene (this will clear everything)

                _m_KeyWasDown = m_KeyDown;
                return;
            }
            _m_KeyWasDown = m_KeyDown;

            // --- Quit Button (Q) ---
            bool q_KeyDown = API.IsKeyDown(API.KEY_Q);
            if (q_KeyDown && !_q_KeyWasDown)
            {
                API.Log("Quitting game...");
                API.QuitGame();
                _q_KeyWasDown = q_KeyDown;
                return;
            }
            _q_KeyWasDown = q_KeyDown;
        }
    }
}