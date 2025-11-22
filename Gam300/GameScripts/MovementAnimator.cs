using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Feed Animator parameters based on the entity's motion and inputs.
    /// Works with an Animator graph that has:
    ///   - float  "Speed"
    ///   - bool   "IsGrounded"
    ///   - trigger "Jump"
    ///   - trigger "Attack"  (optional)
    ///
    /// This script will automatically animate WHATEVER entity it's attached to.
    /// Just add this script component to any entity with an AnimatorComponent!
    /// </summary>
    public class MovementAnimator
    {
        // This field is automatically set by the scripting system
        public ulong Entity;

        // --- CONFIG ---
        // Speed thresholds that match your blend tree (Idle@0, Walk@1, Run@3 by default)
        private const float WALK_THRESHOLD = 0.1f;
        private const float RUN_THRESHOLD = 2.5f;

        // Parameter names in the Animator (must match your graph)
        private const string PARAM_SPEED = "Speed";
        private const string PARAM_IS_GROUNDED = "IsGrounded";
        private const string TRIG_JUMP = "Jump";
        private const string TRIG_ATTACK = "Attack";

        // Keys (adapt if your engine exposes different constants)
        private const int KEY_SPACE = 32; // jump
        private const int MOUSE_LEFT = 0;  // attack

        // If your engine exposes API.GetVelocity, set this true; otherwise we'll use position delta.
        private const bool USE_PHYSICS_VELOCITY = true;

        // --- State (instance variables) ---
        private Boom.Vec3 _prevPos;
        private bool _hasPrev;

        // simple edge detectors so triggers don't spam every frame
        private bool _jumpHeldLast;
        private bool _attackHeldLast;

        /// <summary>
        /// Called once when the script is first created.
        /// </summary>
        public void OnStart(string paramsJson)
        {
            // Use the entity this script is attached to
            if (Entity == 0)
            {
                API.Log("[MovementAnimator] ERROR: Entity field not set by engine!");
                return;
            }

            // Validate the entity has required components
            if (!API.HasTransform(Entity))
            {
                API.Log("[MovementAnimator] ERROR: Entity missing TransformComponent!");
                return;
            }

            _prevPos = API.GetPosition(Entity);
            _hasPrev = true;

            API.Log($"[MovementAnimator] Started on entity: {Entity}");
        }

        /// <summary>
        /// Called every frame to update animator parameters.
        /// </summary>
        public void OnUpdate(float dt)
        {
            if (Entity == 0 || dt <= 0f)
                return;

            // --- 1) Measure horizontal speed (m/s) ---
            float horizSpeed = 0f;

            if (USE_PHYSICS_VELOCITY)
            {
                var v = API.GetLinearVelocity(Entity);
                horizSpeed = (float)Math.Sqrt(v.X * v.X + v.Z * v.Z);
            }
            else
            {
                var pos = API.GetPosition(Entity);
                if (_hasPrev)
                {
                    float dx = pos.X - _prevPos.X;
                    float dz = pos.Z - _prevPos.Z;
                    horizSpeed = (float)Math.Sqrt(dx * dx + dz * dz) / dt;
                }
                _prevPos = pos;
                _hasPrev = true;
            }

            // --- 2) Ground & inputs ---
            bool grounded = API.IsColliding(Entity);
            bool jumpHeld = API.IsKeyDown(KEY_SPACE);
            bool attackHeld = API.IsMouseDown(MOUSE_LEFT);

            // --- 3) SIMPLIFIED: Directly play animation clips based on speed ---
            // Comment out the parameter-based approach since you don't have a blend tree yet

            /*
            API.AnimatorSetFloat(Entity, PARAM_SPEED, horizSpeed);
            API.AnimatorSetBool(Entity, PARAM_IS_GROUNDED, grounded);

            if (jumpHeld && !_jumpHeldLast && grounded)
                API.AnimatorSetTrigger(Entity, TRIG_JUMP);
            _jumpHeldLast = jumpHeld;

            if (attackHeld && !_attackHeldLast)
                API.AnimatorSetTrigger(Entity, TRIG_ATTACK);
            _attackHeldLast = attackHeld;
            */

            // INSTEAD: Play clips directly based on speed and input
            if (attackHeld && !_attackHeldLast)
            {
                // Play attack animation
                API.AnimatorPlay(Entity, "Zombie Attack");
            }
            else if (horizSpeed < 0.1f)
            {
                // Idle - you'll need to add an idle animation
                API.AnimatorPlay(Entity, "idle.fbx"); // Replace with your idle animation name
            }
            else if (horizSpeed < 2.5f)
            {
                // Walking
                API.AnimatorPlay(Entity, "walking.fbx"); // Replace with your walk animation name
            }
            else
            {
                // Running
                API.AnimatorPlay(Entity, "Great Sword Walk.fbx"); // Replace with your run animation name
            }

            _attackHeldLast = attackHeld;
        }

        /// <summary>
        /// Called when the script is destroyed (optional cleanup).
        /// </summary>
        public void OnDestroy()
        {
            API.Log($"[MovementAnimator] Destroyed for entity: {Entity}");
        }
    }

    // Small helpers to keep the script resilient if GetVelocity isn't wired yet.
    internal static class APIExtensions
    {
        public static bool HasMethod_GetVelocity()
        {
            try
            {
                var dummy = API.GetLinearVelocity(0); // your native will likely return zeros
                return true;
            }
            catch { return false; }
        }
    }
}