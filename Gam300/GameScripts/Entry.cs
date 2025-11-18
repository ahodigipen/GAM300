using System;
using Boom;

namespace GameScripts
{
    public static class Entry
    {
        // --- Key definitions (unchanged) ---
        private const int KEY_W = 87, KEY_A = 65, KEY_S = 83, KEY_D = 68, KEY_SPACE = 32;
        private const int MOUSE_LEFT = 0;
        private const int MOUSE_RIGHT = 1;

        // --- Player properties ---
        private static ulong _player;
        private static float _speed = 5f;
        private static float _jumpSpeed = 8f; // How much vertical speed to apply on jump

        // --- REMOVED ---
        // All manual gravity, _vy, and _groundY variables are gone.
        // PhysX now handles gravity.

        public static void Start()
        {
            _player = API.FindEntity("Samurai");
            if (_player == 0)
            {
                API.Log("Entry.Start: Player 'Samurai' not found!");
            }
        }

        public static void Update(float dt)
        {
            if (_player == 0) return;

            // 1. Get the player's CURRENT velocity from PhysX
            var vel = API.GetLinearVelocity(_player);

            // 2. Check if the player is allowed to move
            bool allowMove = !API.IsMouseDown(MOUSE_RIGHT);

            // 3. Check if the player is "grounded"
            // We ask the C++ engine if the rigid body's "isColliding" flag is true.
            bool isGrounded = API.IsColliding(_player);

            // 4. Calculate horizontal movement
            float dx = 0f, dz = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(KEY_A)) dx -= 1f;
                if (API.IsKeyDown(KEY_D)) dx += 1f;
                if (API.IsKeyDown(KEY_W)) dz -= 1f; // forward = -Z
                if (API.IsKeyDown(KEY_S)) dz += 1f;
            }

            // 5. Apply horizontal velocity
            if (dx != 0f || dz != 0f)
            {
                // Normalize to prevent faster diagonal movement
                float len = (float)Math.Sqrt(dx * dx + dz * dz);
                vel.X = (dx / len) * _speed;
                vel.Z = (dz / len) * _speed;
            }
            else
            {
                // No input, so stop horizontal movement
                vel.X = 0f;
                vel.Z = 0f;
            }

            // 6. Apply vertical velocity (Jumping)
            if (allowMove && isGrounded && API.IsKeyDown(KEY_SPACE))
            {
                // Apply a one-time upward velocity.
                vel.Y = _jumpSpeed;
            }
            // NOTE: We no longer apply gravity (vel.Y += gravity * dt).
            // PhysX is already doing this on every simulation step.
            // We just let vel.Y keep whatever value it has (falling, rising, etc.)

            // 7. Set the FINAL velocity back into PhysX
            API.SetLinearVelocity(_player, vel);

            // --- REMOVED ---
            // All API.GetPosition and API.SetPosition calls are gone.
            // The engine now updates the TransformComponent from the PhysX actor.
        }
    }
}