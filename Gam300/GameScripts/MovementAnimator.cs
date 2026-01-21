using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Movement + Animator in one script.
    /// - WASD camera-relative movement (PhysX linear velocity)
    /// - Left Shift = sprint
    /// - Space = jump (edge, only when grounded)
    /// - Animator parameters: Speed(float), IsGrounded(bool), Jump(trigger)
    /// - Optional footstep helper
    /// </summary>
    public class MovementAnimator
    {
        // ===== Tunables =====
        [Boom.EditorExposed("Walk Speed", "Walking speed in m/s", 0.5f, 10f, true)]
        private float _walkSpeed = 3.5f;

        [Boom.EditorExposed("Run Speed", "Running/sprint speed in m/s", 1f, 20f, true)]
        private float _runSpeed = 6.0f;

        [Boom.EditorExposed("Jump Speed", "Vertical jump velocity", 1f, 20f, true)]
        private float _jumpSpeed = 8.0f;

        private bool _gateRmbToPauseMove = true; // hold RMB to pause movement (useful in editor)

        // ===== Audio =====
        [Boom.EditorExposed("Jump Sound", "Sound played when jumping")]
        private string _jumpSoundPath = "Resources/Audio/playerPunch_1.wav";

        [Boom.EditorExposed("Trigger Sound", "Generic sound for trigger interactions")]
        private string _triggerSoundPath = "Resources/Audio/playerPunch_1.wav";

        [Boom.EditorExposed("Door Close Sound", "Sound for door closing")]
        private string _doorCloseSoundPath = "Resources/Audio/playerPunch_1.wav";

        // Animator param names (MUST match your Animator graph)
        private const string PARAM_SPEED = "Speed";
        private const string PARAM_GROUNDED = "IsGrounded";
        private const string PARAM_JUMP = "Jump";

        // Engine-provided
        public ulong Entity;

        // ===== State =====
        private float _currentMoveSpeed;
        private bool _prevSpaceDown = false;
        private bool _hasJumped = false;

        private FootstepComponent _footstep;   // optional
        private static ulong s_playerEntity = 0; // for static trigger callbacks
        private static MovementAnimator s_instance = null; // for accessing instance fields from static callbacks

        // -----------------------------
        // Lifecycle
        // -----------------------------
        public void OnStart(string jsonParams)
        {
            API.Log($"[MovementAnimator] OnStart - Entity={Entity}");
            s_playerEntity = Entity;
            s_instance = this;

            if (!API.HasTransform(Entity))
            {
                API.Log("[MovementAnimator] ERROR: Missing TransformComponent.");
                return;
            }

            // Initialize animator defaults (if present)
            if (API.HasAnimator(Entity))
            {
                API.AnimatorSetFloat(Entity, PARAM_SPEED, 0f);
                API.AnimatorSetBool(Entity, PARAM_GROUNDED, true);
            }
            else
            {
                API.Log("[MovementAnimator] WARN: No AnimatorComponent detected.");
            }

            // Optional footsteps
            try
            {
                _footstep = new FootstepComponent { Entity = Entity };
                _footstep.OnStart("");
            }
            catch { _footstep = null; }

            RegisterTriggerCallbacks();
            API.Log("[MovementAnimator] Init complete.");
        }

        public void OnDestroy()
        {
            if (s_playerEntity == Entity) s_playerEntity = 0;
            try { _footstep?.OnDestroy(); } catch { }
            // If your engine exposes UnregisterTrigger* APIs, call them here.
        }

        // -----------------------------
        // Update
        // -----------------------------
        public void OnUpdate(float dt)
        {
            // Gate movement while RMB is held (e.g., when orbiting camera)
            if (_gateRmbToPauseMove && API.IsMouseDown(API.MOUSE_RIGHT))
                return;

            try { _footstep?.OnUpdate(dt); } catch { }

            // ---- Input (WASD) ----
            float rawX = 0f, rawZ = 0f;
            if (API.IsKeyDown(API.KEY_A)) rawX -= 1f;
            if (API.IsKeyDown(API.KEY_D)) rawX += 1f;
            if (API.IsKeyDown(API.KEY_W)) rawZ += 1f;
            if (API.IsKeyDown(API.KEY_S)) rawZ -= 1f;

            float mag = (float)Math.Sqrt(rawX * rawX + rawZ * rawZ);
            if (mag > 1e-4f) { rawX /= mag; rawZ /= mag; }

            // ---- Sprint (Left Shift) ----
            bool sprint = API.IsKeyDown(API.KEY_LEFT_CONTROL);
            _currentMoveSpeed = sprint ? _runSpeed : _walkSpeed;

            // ---- Camera-relative direction (XZ) ----
            Vec3 moveDir = new Vec3(0, 0, 0);
            if (mag > 0f)
            {
                float yawDeg = API.GetThirdPersonCameraYaw();
                float yawRad = yawDeg * (float)Math.PI / 180f;

                Vec3 fwd = new Vec3((float)Math.Sin(yawRad), 0f, (float)Math.Cos(yawRad));
                Vec3 right = new Vec3((float)Math.Cos(yawRad), 0f, -(float)Math.Sin(yawRad));

                moveDir = new Vec3(
                    right.X * rawX + fwd.X * rawZ,
                    0f,
                    right.Z * rawX + fwd.Z * rawZ
                );

                // re-normalize
                float len = (float)Math.Sqrt(moveDir.X * moveDir.X + moveDir.Z * moveDir.Z);
                if (len > 1e-4f) { moveDir.X /= len; moveDir.Z /= len; }
            }

            // ---- Apply horizontal velocity ----
            var vel = API.GetLinearVelocity(Entity);
            vel.X = (mag > 0f) ? moveDir.X * _currentMoveSpeed : 0f;
            vel.Z = (mag > 0f) ? moveDir.Z * _currentMoveSpeed : 0f;

            // ---- Jump (edge on press, only when grounded) ----
            bool grounded = API.IsColliding(Entity);
            bool spaceDown = API.IsKeyDown(API.KEY_SPACE);

            if (grounded && _hasJumped)
                _hasJumped = false; // landed → can jump again

            if (grounded && spaceDown && !_prevSpaceDown && !_hasJumped)
            {
                vel.Y = _jumpSpeed;
                _hasJumped = true;

                if (API.HasAnimator(Entity))
                    API.AnimatorSetTrigger(Entity, PARAM_JUMP);

                // Optional SFX
                try
                {
                    Vec3 pos = API.GetPosition(Entity);
                    API.PlaySoundAt("jump_sound", _jumpSoundPath, pos, false);
                    API.SetSoundVolume("jump_sound", 0.9f);
                }
                catch { }

                API.Log("[MovementAnimator] Jump!");
            }

            _prevSpaceDown = spaceDown;

            // PhysX applies gravity; we just set desired velocity.
            API.SetLinearVelocity(Entity, vel);

            // ---- Animator parameters ----
            if (API.HasAnimator(Entity))
            {
                float planarSpeed = (float)Math.Sqrt(vel.X * vel.X + vel.Z * vel.Z);
                API.AnimatorSetFloat(Entity, PARAM_SPEED, planarSpeed);
                API.AnimatorSetBool(Entity, PARAM_GROUNDED, grounded);
            }

            // ---- Face movement direction (instant) ----
            if (mag > 0.1f && API.HasTransform(Entity))
            {
                float angleRad = (float)Math.Atan2(moveDir.X, moveDir.Z); // Atan2(x,z)
                float angleDeg = angleRad * 180.0f / (float)Math.PI;

                var t = API.GetTransform(Entity);
                t.RotationY = angleDeg;
                API.SetTransform(Entity, t);
            }
        }

        // -----------------------------
        // Triggers (optional SFX)
        // -----------------------------
        private void RegisterTriggerCallbacks()
        {
            string[] names = { "Checkpoint", "DamageZone", "PowerUp", "DoorTrigger" };
            int count = 0;

            foreach (string name in names)
            {
                ulong e = API.FindEntity(name);
                if (e == 0) continue;

                if (API.HasCollider(e) && API.IsTrigger(e))
                {
                    API.RegisterTriggerEnterCallback(e, OnTriggerEnter);
                    API.RegisterTriggerExitCallback(e, OnTriggerExit);
                    count++;
                }
            }

            API.Log($"[MovementAnimator] Registered trigger callbacks: {count}");
        }

        /// <summary>
        /// Reset static state (call on scene change to prevent stale entity access)
        /// </summary>
        public static void ResetStatic()
        {
            s_playerEntity = 0;
            s_instance = null;
            API.Log("[MovementAnimator] Reset static state");
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            // Skip if scene transition is in progress
            if (EndZoneTrigger.s_sceneTransitionInProgress) return;

            try
            {
                if (otherEntity != s_playerEntity) return;

                Vec3 pos = API.HasTransform(triggerEntity) ? API.GetPosition(triggerEntity) : new Vec3(0, 0, 0);

                // Pick SFX based on trigger name (best-effort)
                string soundId = "generic_trigger";
                if (triggerEntity == API.FindEntity("Checkpoint")) soundId = "checkpoint";
                else if (triggerEntity == API.FindEntity("DamageZone")) soundId = "damage";
                else if (triggerEntity == API.FindEntity("PowerUp")) soundId = "powerup";
                else if (triggerEntity == API.FindEntity("DoorTrigger")) soundId = "door_open";

                string soundPath = s_instance != null ? s_instance._triggerSoundPath : "Resources/Audio/playerPunch_1.wav";
                API.PlaySoundAt(soundId, soundPath, pos, false);
            }
            catch (Exception ex)
            {
                API.Log($"[MovementAnimator] OnTriggerEnter ERROR: {ex.Message}");
            }
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            // Skip if scene transition is in progress
            if (EndZoneTrigger.s_sceneTransitionInProgress) return;

            try
            {
                if (otherEntity != s_playerEntity) return;

                if (triggerEntity == API.FindEntity("DamageZone"))
                    API.StopSound("damage");

                if (triggerEntity == API.FindEntity("DoorTrigger"))
                {
                    Vec3 pos = API.HasTransform(triggerEntity) ? API.GetPosition(triggerEntity) : new Vec3(0, 0, 0);
                    string doorSoundPath = s_instance != null ? s_instance._doorCloseSoundPath : "Resources/Audio/playerPunch_1.wav";
                    API.PlaySoundAt("door_close", doorSoundPath, pos, false);
                }
            }
            catch (Exception ex)
            {
                API.Log($"[MovementAnimator] OnTriggerExit ERROR: {ex.Message}");
            }
        }

        // -----------------------------
        // Public knobs (optional)
        // -----------------------------
        public void SetWalkSpeed(float s) => _walkSpeed = Math.Max(0f, s);
        public void SetRunSpeed(float s) => _runSpeed = Math.Max(0f, s);
        public void SetJumpSpeed(float j) => _jumpSpeed = Math.Max(0f, j);

        public bool IsMoving()
        {
            var v = API.GetLinearVelocity(Entity);
            return Math.Sqrt(v.X * v.X + v.Z * v.Z) > 0.1f;
        }

        public bool IsGrounded() => API.IsColliding(Entity);

        public void SetFootstepVolume(float volume) { try { _footstep?.SetFootstepVolume(volume); } catch { } }
        public void SetFootstepInterval(float interval) { try { _footstep?.SetFootstepInterval(interval); } catch { } }
    }
}
