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

        // --- Scene names ---
        private const string LEVEL_SCENE_NAME = "level";
        private const string PAUSE_SCENE_NAME = "PauseMenu";
        private const string MAIN_MENU_SCENE_NAME = "MainMenu";
        private const string HOW_TO_PLAY_SCENE_NAME = "HowToPlay";
        private static string _currentSceneName = MAIN_MENU_SCENE_NAME; // default at boot

        // Edge-trigger trackers
        private static bool _h_KeyWasDown = false;
        private static bool _p_KeyWasDown = false;
        private static bool _r_KeyWasDown = false;
        private static bool _y_KeyWasDown = false;
        private static bool _m_KeyWasDown = false;
        private static bool _q_KeyWasDown = false;

        // --- small helpers to keep _currentSceneName consistent ---
        private static void LoadScene(string name)
        {
            API.LoadScene(name);
            _currentSceneName = name;
        }
        private static void LoadSceneAdditive(string name)
        {
            API.LoadSceneAdditive(name);
            // keep _currentSceneName as the base scene; additive menu sits on top
        }
        private static void UnloadPauseMenu()
        {
            API.UnloadPauseMenu();
            // no change to _currentSceneName
        }

        public static void Start()
        {
            // reset edge-tracking each scene start
            _h_KeyWasDown = _p_KeyWasDown = _r_KeyWasDown = _y_KeyWasDown = _m_KeyWasDown = _q_KeyWasDown = false;

            // Find player now that the scene is loaded
            _player = API.FindEntity("Samurai");

            if (_player != 0)
            {
                if (!API.HasTransform(_player))
                {
                    API.Log("[C#] ERROR: Samurai entity does not have TransformComponent!");
                    _player = 0;
                    return;
                }

                // If your API doesn’t expose HasScript, remove this block.
                if (API.HasScript(_player) == false)
                {
                    API.Log("[C#] WARNING: Samurai has no ScriptComponent; movement disabled.");
                    _player = 0;
                    return;
                }

                API.Log("[C#] Samurai ready (Transform + Script present).");
            }
            else
            {
                API.Log("[C#] NOTE: Samurai not found (expected for menu scenes).");
            }
        }

        public static void Update(float dt)
        {
            int state = API.GetApplicationState();

            if (state == API.APP_STATE_RUNNING)
            {
                UpdateGame(dt);
            }
            else if (state == API.APP_STATE_PAUSED)
            {
                if (API.IsPauseMenuLoaded())
                    UpdatePauseMenu();
            }
        }

        // ================= GAME LOOP =================
        private static void UpdateGame(float dt)
        {
            // --- read inputs once ---
            bool p_KeyDown = API.IsKeyDown(API.KEY_P);
            bool h_KeyDown = API.IsKeyDown(API.KEY_H);
            bool r_KeyDown = API.IsKeyDown(API.KEY_R);
            bool q_KeyDown = API.IsKeyDown(API.KEY_Q);
            bool ctrl_Down = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            // ===== MENUS (Main / HowToPlay) =====
            if (_currentSceneName == MAIN_MENU_SCENE_NAME || _currentSceneName == HOW_TO_PLAY_SCENE_NAME)
            {
                if (p_KeyDown && !_p_KeyWasDown && !ctrl_Down)
                {
                    API.Log("Starting game... loading 'level'.");
                    LoadScene(LEVEL_SCENE_NAME);
                    _p_KeyWasDown = p_KeyDown;
                    return;
                }

                if (h_KeyDown && !_h_KeyWasDown && !ctrl_Down)
                {
                    API.Log("Loading 'HowToPlay'.");
                    LoadScene(HOW_TO_PLAY_SCENE_NAME);
                    _h_KeyWasDown = h_KeyDown;
                    return;
                }

                if (r_KeyDown && !_r_KeyWasDown && !ctrl_Down && _currentSceneName == HOW_TO_PLAY_SCENE_NAME)
                {
                    API.Log("Returning to Main Menu from HowToPlay.");
                    LoadScene(MAIN_MENU_SCENE_NAME);
                    _r_KeyWasDown = r_KeyDown;
                    return;
                }

                if (q_KeyDown && !_q_KeyWasDown && !ctrl_Down)
                {
                    API.Log("Quitting application from Main Menu.");
                    API.QuitGame();
                    _q_KeyWasDown = q_KeyDown;
                    return;
                }

                _p_KeyWasDown = p_KeyDown;
                _h_KeyWasDown = h_KeyDown;
                _r_KeyWasDown = r_KeyDown;
                _q_KeyWasDown = q_KeyDown;
                return;
            }

            // ===== IN-GAME =====

            // Pause with P (no Ctrl)
            if (p_KeyDown && !_p_KeyWasDown && !ctrl_Down)
            {
                API.Log("Pausing game (P).");
                API.TogglePause();
                LoadSceneAdditive(PAUSE_SCENE_NAME);
                _p_KeyWasDown = p_KeyDown;
                return;
            }
            _p_KeyWasDown = p_KeyDown;

            // Player might not exist in some levels
            if (_player == 0) return;

            if (!API.HasTransform(_player))
            {
                API.Log("[C#] ERROR: Player lost TransformComponent!");
                _player = 0;
                return;
            }
            // If you don’t have HasScript, remove this guard.
            if (API.HasScript(_player) == false)
            {
                API.Log("[C#] ERROR: Player lost ScriptComponent! Movement disabled.");
                _player = 0;
                return;
            }

            // -------- PhysX-driven character movement ----------
            var vel = API.GetLinearVelocity(_player);

            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);
            bool grounded = API.IsColliding(_player);

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

            if (allowMove && grounded && API.IsKeyDown(API.KEY_SPACE))
            {
                vel.Y = _jumpSpeed; // jump impulse
            }

            API.SetLinearVelocity(_player, vel);

            // -------- Animator parameters (INSIDE UpdateGame) ----------
            float horizSpeed = (float)Math.Sqrt(vel.X * vel.X + vel.Z * vel.Z);
            API.AnimatorSetFloat(_player, "Speed", horizSpeed);
            API.AnimatorSetBool(_player, "IsGrounded", grounded);
            if (grounded && API.IsKeyDown(API.KEY_SPACE))
                API.AnimatorSetTrigger(_player, "Jump");

            if (API.IsMouseDown(API.MOUSE_LEFT))
                API.AnimatorSetTrigger(_player, "Attack");
        }

        // ================= PAUSE MENU LOOP =================
        private static void UpdatePauseMenu()
        {
            // Resume (R)
            bool r_KeyDown = API.IsKeyDown(API.KEY_R);
            if (r_KeyDown && !_r_KeyWasDown)
            {
                API.Log("Resuming game...");
                UnloadPauseMenu();
                API.TogglePause();
                _r_KeyWasDown = r_KeyDown;
                return;
            }
            _r_KeyWasDown = r_KeyDown;

            // Main Menu (M)
            bool m_KeyDown = API.IsKeyDown(API.KEY_M);
            if (m_KeyDown && !_m_KeyWasDown)
            {
                API.Log("Returning to Main Menu...");
                API.TogglePause(); // unpause first
                LoadScene(MAIN_MENU_SCENE_NAME);
                _m_KeyWasDown = m_KeyDown;
                return;
            }
            _m_KeyWasDown = m_KeyDown;

            // Restart (Y)
            bool y_KeyDown = API.IsKeyDown(API.KEY_Y);
            if (y_KeyDown && !_y_KeyWasDown)
            {
                API.Log("Restarting scene...");
                API.TogglePause();
                LoadScene(_currentSceneName);
                _y_KeyWasDown = y_KeyDown;
                return;
            }
            _y_KeyWasDown = y_KeyDown;

            // Quit (Q)
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
