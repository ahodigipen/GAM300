using System;
using Boom;

namespace GameScripts
{
    public static class Entry
    {
        private const int KEY_W = 87, KEY_A = 65, KEY_S = 83, KEY_D = 68, KEY_SPACE = 32;
        private const int MOUSE_LEFT = 0; // GLFW_MOUSE_BUTTON_LEFT
        private const int MOUSE_RIGHT = 1;

        private static ulong _player;
        private static float _speed = 50f;

        // jump/gravity
        private static float _vy = 0f, _gravity = -20f, _jumpSpeed = 8f, _groundY = 0f;
        private static bool _grounded = true;

        public static void Start()
        {
            _player = API.FindEntity("Samurai");
            if (_player != 0) _groundY = API.GetPosition(_player).Y;
        }

        public static void Update(float dt)
        {
            API.Log("Update called!");

            if (_player == 0) return;

            var pos = API.GetPosition(_player);

            // Don't move if LMB is pressed
            bool allowMove = !API.IsMouseDown(MOUSE_RIGHT);

            float dx = 0f, dz = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(KEY_A)) dx -= 1f;
                if (API.IsKeyDown(KEY_D)) dx += 1f;
                if (API.IsKeyDown(KEY_W)) dz -= 1f; // forward = -Z
                if (API.IsKeyDown(KEY_S)) dz += 1f;

                if (dx != 0f || dz != 0f)
                {
                    float len = (float)Math.Sqrt(dx * dx + dz * dz);
                    dx /= len; dz /= len;
                    pos.X += dx * _speed * dt;
                    pos.Z += dz * _speed * dt;
                }

                // Jump only when allowed
                if (_grounded && API.IsKeyDown(KEY_SPACE))
                {
                    _vy = _jumpSpeed;
                    _grounded = false;
                }
            }

            // Gravity continues so you still fall back to groundY
            _vy += _gravity * dt;
            pos.Y += _vy * dt;

            if (pos.Y <= _groundY)
            {
                pos.Y = _groundY;
                _vy = 0f;
                _grounded = true;
            }

            API.SetPosition(_player, pos);
        }
    }
}
