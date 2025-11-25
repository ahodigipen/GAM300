using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Camera-relative character controller that drives the Animator.
    /// - WASD move, Shift = Sprint, Ctrl = Sneak, Space = Jump
    /// - Uses PhysX linear velocity on XZ, custom gravity on Y
    /// - Rotates to face move direction
    /// - Sets Animator: Speed, IsMoving, Sprint, IsSneaking, Jump(trigger)
    /// </summary>
    public class CharacterControllerAnimator
    {
        public ulong Entity;

        // ===== Tunables =====
        private const float WALK_SPEED = 3.5f;   // m/s
        private const float RUN_SPEED = 6.0f;   // m/s
        private const float SNEAK_SPEED = 1.8f;   // m/s

        private const float ACCEL = 22f;    // m/s^2  (how fast we reach target speed)
        private const float AIR_ACCEL = 6f;     // weaker in air
        private const float FRICTION = 14f;    // slows to stop when no input (ground)
        private const float AIR_FRICTION = 1.0f;

        private const float JUMP_SPEED = 8.0f;   // initial upward velocity
        private const float GRAVITY = -22.0f; // m/s^2
        private const float ROTATE_SPEED = 12f;    // how fast we face move dir

        private const float MOVE_EPS = 0.10f;  // animator "moving" threshold
        private const float SPEED_DAMP = 10f;    // smoothing for animator Speed

        // ===== State =====
        private double _smoothedSpeed = 0.0;
        private bool _jumpPressedLast = false;

        public void OnStart(string _)
        {
            if (!API.HasAnimator(Entity))
            {
                API.Log("[CharacterControllerAnimator] Missing AnimatorComponent.");
                return;
            }

            API.AnimatorSetFloat(Entity, "Speed", 0f);
            API.AnimatorSetBool(Entity, "IsMoving", false);
            API.AnimatorSetBool(Entity, "Sprint", false);
            API.AnimatorSetBool(Entity, "IsSneaking", false);
        }

        public void OnUpdate(float dt)
        {
            if (Entity == 0) return;

            // --- Input ---
            bool w = API.IsKeyDown(API.KEY_W);
            bool a = API.IsKeyDown(API.KEY_A);
            bool s = API.IsKeyDown(API.KEY_S);
            bool d = API.IsKeyDown(API.KEY_D);

            bool sprintKey = API.IsKeyDown(API.KEY_LEFT_SHIFT);
            bool sneakKey = API.IsKeyDown(API.KEY_LEFT_CONTROL);
            bool jumpKey = API.IsKeyDown(API.KEY_SPACE);

            // --- Camera-relative move vector (XZ) ---
            float yawRad = (float)(Math.PI / 180.0) * API.GetThirdPersonCameraYaw();
            // Forward/right for -Z forward world (common OpenGL): forward=(sin,+0,-cos), right=(cos,+0,+sin)
            var fwd = new Vec3((float)Math.Sin(yawRad), 0f, (float)-Math.Cos(yawRad));
            var right = new Vec3((float)Math.Cos(yawRad), 0f, (float)Math.Sin(yawRad));

            float ix = (d ? 1f : 0f) - (a ? 1f : 0f);
            float iz = (w ? 1f : 0f) - (s ? 1f : 0f);

            // Desired direction in world space
            var desiredDir = Normalize(Add(Scale(fwd, iz), Scale(right, ix)));

            bool hasInput = (Math.Abs(ix) + Math.Abs(iz)) > 0.001f;

            // --- Target speed ---
            float targetSpeed = WALK_SPEED;
            if (sprintKey) targetSpeed = RUN_SPEED;
            if (sneakKey) targetSpeed = SNEAK_SPEED;

            // --- Current velocity ---
            var vel = API.GetLinearVelocity(Entity);
            bool grounded = API.IsGrounded(Entity);

            // --- Horizontal (XZ) velocity update ---
            var horizVel = new Vec3(vel.X, 0f, vel.Z);

            if (hasInput)
            {
                var desiredVel = Scale(desiredDir, targetSpeed);
                float accel = grounded ? ACCEL : AIR_ACCEL;

                // Accelerate toward desired velocity
                horizVel = MoveTowards(horizVel, desiredVel, accel * dt);
            }
            else
            {
                // Friction toward zero
                float friction = grounded ? FRICTION : AIR_FRICTION;
                horizVel = MoveTowards(horizVel, new Vec3(0, 0, 0), friction * dt);
            }

            // --- Vertical (Y) velocity update ---
            float vy = vel.Y;
            if (grounded)
            {
                // Jump
                if (jumpKey && !_jumpPressedLast)
                {
                    vy = JUMP_SPEED;
                    API.AnimatorSetTrigger(Entity, "Jump"); // animator trigger
                }
                else if (vy < 0f)
                {
                    vy = 0f; // stick to ground
                }
            }
            vy += GRAVITY * dt;

            _jumpPressedLast = jumpKey;

            // --- Apply final velocity ---
            var newVel = new Vec3(horizVel.X, vy, horizVel.Z);
            API.SetLinearVelocity(Entity, newVel);

            // --- Face move direction if moving ---
            if (hasInput)
            {
                // face desiredDir smoothly
                float targetYawDeg = (float)(Math.Atan2(desiredDir.X, -desiredDir.Z) * 180.0 / Math.PI); // consistent with forward=(sin,-cos)
                // simple critically damped interpolation (slerp-ish on yaw)
                // read current yaw if you have it; otherwise just step toward target
                var r = API.GetRotation(Entity);
                float currentYaw = r.Y;
                float newYaw = LerpAngle(currentYaw, targetYawDeg, Math.Min(1f, ROTATE_SPEED * dt));
                API.SetRotationY(Entity, newYaw);
            }

            // --- Animator params ---
            float speedXZ = Length(horizVel); // ignore vertical speed
            _smoothedSpeed += (speedXZ - _smoothedSpeed) * Math.Min(1.0, SPEED_DAMP * dt);

            API.AnimatorSetFloat(Entity, "Speed", (float)_smoothedSpeed);
            API.AnimatorSetBool(Entity, "IsMoving", _smoothedSpeed > MOVE_EPS || hasInput);
            API.AnimatorSetBool(Entity, "Sprint", sprintKey);
            API.AnimatorSetBool(Entity, "IsSneaking", sneakKey);
        }

        public void OnDestroy() { }

        // ========== small vec helpers (avoid allocating new structs everywhere) ==========
        private static Vec3 Add(Vec3 a, Vec3 b) => new Vec3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        private static Vec3 Scale(Vec3 a, float s) => new Vec3(a.X * s, a.Y * s, a.Z * s);
        private static float Dot(Vec3 a, Vec3 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z;
        private static float Length(Vec3 a) => (float)Math.Sqrt(Dot(a, a));
        private static Vec3 Normalize(Vec3 v)
        {
            float l = Length(v);
            return l > 1e-5f ? new Vec3(v.X / l, v.Y / l, v.Z / l) : new Vec3(0, 0, 0);
        }

        private static Vec3 MoveTowards(Vec3 current, Vec3 target, float maxDelta)
        {
            var delta = new Vec3(target.X - current.X, target.Y - current.Y, target.Z - current.Z);
            float dist = Length(delta);
            if (dist <= maxDelta || dist < 1e-6f) return target;
            float t = maxDelta / dist;
            return new Vec3(current.X + delta.X * t, current.Y + delta.Y * t, current.Z + delta.Z * t);
        }

        

        private static float LerpAngle(float a, float b, float t)
        {
            // shortest path
            float delta = Repeat((b - a) + 180f, 360f) - 180f;
            return a + delta * Clamp01(t);
        }
        private static float Repeat(float t, float length) => t - (float)Math.Floor(t / length) * length;
        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);
    }
}
