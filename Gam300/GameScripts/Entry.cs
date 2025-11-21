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
        private const string LEVEL_SCENE_NAME = "level";
        private const string PAUSE_SCENE_NAME = "PauseMenu";
        private const string MAIN_MENU_SCENE_NAME = "MainMenu";
        private const string HOW_TO_PLAY_SCENE_NAME = "HowToPlay";
        private static string _currentSceneName;

        // Key state trackers
        private static bool _h_KeyWasDown = false;

        private static bool _p_KeyWasDown = false;
        private static bool _r_KeyWasDown = false;
        private static bool _y_KeyWasDown = false;
        private static bool _m_KeyWasDown = false;

        private static bool _q_KeyWasDown = false;

        public static void Start()
        {
            // Reset keys every time a scene loads
            _h_KeyWasDown = false;

            _p_KeyWasDown = false;
            _r_KeyWasDown = false;
            _y_KeyWasDown = false;
            _m_KeyWasDown = false;

            _q_KeyWasDown = false;

            _currentSceneName = API.GetCurrentSceneName();

            // Find the player
        private static ulong _player;
        private static float _speed = 10f;   // movement speed (units per second)
        private static float _jumpSpeed = 8f; // vertical jump velocity

        public static void Start()
        {
            API.Log("[C#] Entry.Start() called");

            _player = API.FindEntity("Samurai");
            API.Log("[C#] Samurai handle = " + _player);

            if (_player != 0)
            {
                // Check if entity has required components
                if (!API.HasTransform(_player))
                {
                    API.Log("[C#] ERROR: Samurai entity does not have TransformComponent!");
                    _player = 0; // Invalidate so Update won't try to use it
                    return;
                }

                if (!API.HasScript(_player))
                {
                    API.Log("[C#] ERROR: Samurai entity does not have ScriptComponent!");
                    API.Log("[C#] Player movement requires a ScriptComponent to be attached.");
                    _player = 0; // Invalidate so Update won't try to use it
                    return;
                }

                API.Log("[C#] Samurai has required components (Transform + Script) - OK");
            }
            else
            {
                API.Log("[C#] WARNING: Could not find Samurai entity");
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
            // Get key states once
            bool p_KeyDown = API.IsKeyDown(API.KEY_P); // Menu Play
            bool h_KeyDown = API.IsKeyDown(API.KEY_H); // Menu How to Play
            bool r_KeyDown = API.IsKeyDown(API.KEY_R); // How to Play return to Menu
            bool q_KeyDown = API.IsKeyDown(API.KEY_Q); // Quit
            bool ctrl_KeyDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            // --- LOGIC FOR MENUS ---
            if (_currentSceneName == MAIN_MENU_SCENE_NAME || _currentSceneName == HOW_TO_PLAY_SCENE_NAME)
            {
                // Check for 'P' (without Ctrl) to START the Level
                if (p_KeyDown && !_p_KeyWasDown && !ctrl_KeyDown)
                {
                    API.Log("Starting game... Loading 'level' scene.");
                    API.LoadScene(LEVEL_SCENE_NAME);
                    _p_KeyWasDown = p_KeyDown;
                    return;
                }

                // Check for 'H' (without Ctrl) to go to HOW TO PLAY
                if (h_KeyDown && !_h_KeyWasDown && !ctrl_KeyDown)
                {
                    API.Log("Loading 'HowToPlay' scene.");
                    API.LoadScene(HOW_TO_PLAY_SCENE_NAME);
                    _h_KeyWasDown = h_KeyDown; // Set tracker
                    return; // Exit after action
                }

                // Check for 'R' (without Ctrl) to RETURN TO MAIN MENU (from HowToPlay)
                if (r_KeyDown && !_r_KeyWasDown && !ctrl_KeyDown && _currentSceneName == HOW_TO_PLAY_SCENE_NAME)
                {
                    API.Log("Returning to Main Menu from HowToPlay.");
                    API.LoadScene(MAIN_MENU_SCENE_NAME);
                    _r_KeyWasDown = r_KeyDown; // Set tracker
                    return; // Exit after action
                }

                // Check for 'Q' (without Ctrl) to QUIT
                if (q_KeyDown && !_q_KeyWasDown && !ctrl_KeyDown)
                {
                    API.Log("Quitting application from main menu.");
                    API.QuitGame(); // This will close the application
                    _q_KeyWasDown = q_KeyDown; // Set tracker
                    return; // Exit after action
                }

                // Update all menu key trackers if no action was taken
                _p_KeyWasDown = p_KeyDown;
                _h_KeyWasDown = h_KeyDown;
                _r_KeyWasDown = r_KeyDown;
                _q_KeyWasDown = q_KeyDown;

                return;
            }

            // --- LOGIC FOR IN-GAME ---

            // 1. Check for 'P' (without Ctrl) to PAUSE the game
            if (p_KeyDown && !_p_KeyWasDown && !ctrl_KeyDown)
            {
                API.Log("Pausing game (P key)...");
                API.TogglePause();
                API.LoadSceneAdditive(PAUSE_SCENE_NAME);
                _p_KeyWasDown = p_KeyDown;
                return;
            }
            _p_KeyWasDown = p_KeyDown;

            // 2. Run Player Logic
            if (_player == 0) return;
            // Only process if we have a valid player with required components
            if (_player == 0)
                return;

            // Double-check components still exist (in case they were removed at runtime)
            if (!API.HasTransform(_player))
            {
                API.Log("[C#] ERROR: Player lost TransformComponent!");
                _player = 0;
                return;
            }

            if (!API.HasScript(_player))
            {
                API.Log("[C#] ERROR: Player lost ScriptComponent! Movement disabled.");
                _player = 0;
                return;
            }

            // =============== PHYSX-BASED MOVEMENT =================

            var vel = API.GetLinearVelocity(_player);

            // 2. Check if the player is allowed to move (RMB held disables movement)
            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);

            // 3. Check if the player is "grounded" via collision flag
            bool isGrounded = API.IsColliding(_player);

            // 4. Calculate horizontal movement input
            float dx = 0f, dz = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) dx -= 1f;
                if (API.IsKeyDown(API.KEY_D)) dx += 1f;
                if (API.IsKeyDown(API.KEY_W)) dz -= 1f; // forward = -Z
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

            // 6. Apply vertical velocity (Jumping)
            if (allowMove && isGrounded && API.IsKeyDown(API.KEY_SPACE))
            {
                vel.Y = _jumpSpeed;
            }
            // NOTE: We do NOT apply gravity here.
            // PhysX already applies gravity every simulation step.
            // We just modify vel.X / vel.Z + jump impulse, and let PhysX handle the rest.

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

            // --- Restart Button (Y) ---
            bool y_KeyDown = API.IsKeyDown(API.KEY_Y);
            if (y_KeyDown && !_y_KeyWasDown)
            {
                API.Log("Restarting scene...");

                // 1. Un-pause the engine state
                API.TogglePause();

                // 2. Reload the current game scene
                // (This replaces all scenes, including the pause menu)
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
                _q_KeyWasDown = q_KeyDown;
                return;
            }
            _q_KeyWasDown = q_KeyDown;
        }
    }
}
