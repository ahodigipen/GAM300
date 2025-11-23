using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Two-state animator driver: plays either Idle or Move clip.
    /// - If the entity is moving (or there is WASD input), plays MOVE_CLIP.
    /// - If there is no input and speed is ~0, plays IDLE_CLIP.
    /// Works with AnimatorComponent; safe no-ops if animator is missing.
    /// </summary>
    public class MovementAnimator 
    {
        // Set these to your actual clip/state names
        private const string IDLE_CLIP = "Fast Run";   // e.g., "idle.fbx" or "Idle"
        private const string MOVE_CLIP = "Fast Run";   // e.g., "walking.fbx" or "Walk"

        // Movement detection threshold
        private const float SPEED_EPS = 0.10f;     // m/s to count as "moving"

        public ulong Entity;                       // set by native side

        // cache to avoid spamming AnimatorPlay every frame
        private bool _wasMoving = false;
        private bool _initialized = false;

        public void OnStart(string _)
        {
            if (Entity == 0)
            {
                API.Log("[MovementAnimator] Entity handle not set.");
                return;
            }
            if (!API.HasTransform(Entity))
            {
                API.Log("[MovementAnimator] Missing TransformComponent.");
                return;
            }

            // Start in Idle
            API.AnimatorPlay(Entity, IDLE_CLIP);
            _wasMoving = false;
            _initialized = true;
        }

        public void OnUpdate(float dt)
        {
            if (!_initialized || Entity == 0) return;

            // 1) Intent from input
            bool input =
                API.IsKeyDown(API.KEY_W) ||
                API.IsKeyDown(API.KEY_A) ||
                API.IsKeyDown(API.KEY_S) ||
                API.IsKeyDown(API.KEY_D);

            // 2) Actual movement from physics velocity (XZ)
            var v = API.GetLinearVelocity(Entity);
            float planarSpeed = (float)Math.Sqrt(v.X * v.X + v.Z * v.Z);

            bool moving = input || (planarSpeed > SPEED_EPS);

            // 3) Switch only on change
            if (moving != _wasMoving)
            {
                if (moving)
                    API.AnimatorPlay(Entity, MOVE_CLIP);
                else
                    API.AnimatorPlay(Entity, IDLE_CLIP);

                _wasMoving = moving;
            }
        }

        public void OnDestroy() { }
    }
}
