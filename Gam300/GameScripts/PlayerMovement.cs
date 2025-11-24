using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Camera-relative WASD using PhysX linear velocity.
    /// Space = jump (edge; only when grounded). Left Shift = sprint.
    /// Animator is driven by parameters: Speed (float), IsGrounded (bool), Jump (trigger).
    /// Requires TransformComponent; AnimatorComponent is optional (guarded).
    /// </summary>
    public class PlayerMovement
    {
        // ====== TUNABLES ======
        private float _walkSpeed = 3.5f;     // speed when not sprinting
        private float _runSpeed = 6.0f;     // speed when sprinting (Left Shift)
        private float _jumpSpeed = 8.0f;     // upward velocity applied on jump
        private bool _gateRmbToPauseMove = true; // if true, holding RMB pauses movement (useful in editor)

        // Animator parameter names (match your graph!)
        private const string PARAM_SPEED = "Speed";
        private const string PARAM_GROUNDED = "IsGrounded";
        private const string PARAM_JUMP = "Jump";

        // ====== ENGINE ======
        public ulong Entity;

        // ====== STATE ======
        private bool _prevSpaceDown = false;   // for edge-detect
        private bool _hasJumped = false;       // prevents multiple jumps in one airtime
        private float _currentMoveSpeed;        // cached per-frame (walk vs run)

        private FootstepComponent _footstep;    // optional helper
        private static ulong s_playerEntity = 0; // used to identify "the player" in static trigger callbacks

        // ---------------------------------------------------------
        // Lifecycle
        // ---------------------------------------------------------
        public void OnStart(string jsonParams)
        {
            API.Log($"[PlayerMovement] OnStart - Entity={Entity}");
            s_playerEntity = Entity;

            if (!API.HasTransform(Entity))
            {
                API.Log("[PlayerMovement] ERROR: Missing TransformComponent.");
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
                API.Log("[PlayerMovement] WARN: No AnimatorComponent detected.");
            }

            // Optional footstep helper
            try
            {
                _footstep = new FootstepComponent { Entity = Entity };
                _footstep.OnStart("");
            }
            catch { _footstep = null; }

            RegisterTriggerCallbacksOnAllTriggers();
            API.Log("[PlayerMovement] Init complete.");
        }

        public void OnDestroy()
        {
            if (s_playerEntity == Entity) s_playerEntity = 0;
            try { _footstep?.OnDestroy(); } catch { }
            // If engine exposes UnregisterTrigger* APIs, call them here to clean up.
        }

        // ---------------------------------------------------------
        // Update
        // ---------------------------------------------------------
        public void OnUpdate(float dt)
        {
            // RMB gate (e.g., when orbiting camera in editor)
            if (_gateRmbToPauseMove && API.IsMouseDown(API.MOUSE_RIGHT))
                return;

            try { _footstep?.OnUpdate(dt); } catch { }

            // ---- Input: WASD ----
            float rawX = 0f, rawZ = 0f;
            if (API.IsKeyDown(API.KEY_A)) rawX -= 1f;
            if (API.IsKeyDown(API.KEY_D)) rawX += 1f;
            if (API.IsKeyDown(API.KEY_W)) rawZ += 1f;
            if (API.IsKeyDown(API.KEY_S)) rawZ -= 1f;

            // Normalize input
            float mag = (float)Math.Sqrt(rawX * rawX + rawZ * rawZ);
            if (mag > 1e-4f) { rawX /= mag; rawZ /= mag; }

            // ---- Sprint ----
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

            // ---- Write horizontal velocity from input ----
            var vel = API.GetLinearVelocity(Entity);
            if (mag > 0f)
            {
                vel.X = moveDir.X * _currentMoveSpeed;
                vel.Z = moveDir.Z * _currentMoveSpeed;
            }
            else
            {
                vel.X = 0f;
                vel.Z = 0f;
            }

            // ---- Jump (edge on press, only when grounded) ----
            bool isGrounded = API.IsColliding(Entity);
            bool spaceDown = API.IsKeyDown(API.KEY_SPACE);

            if (isGrounded && _hasJumped)
                _hasJumped = false; // landed → can jump again

            if (isGrounded && spaceDown && !_prevSpaceDown && !_hasJumped)
            {
                vel.Y = _jumpSpeed;
                _hasJumped = true;

                // Optional: jump SFX
                try
                {
                    Vec3 pos = API.GetPosition(Entity);
                    API.PlaySoundAt("jump_sound", "Resources/Audio/playerPunch_1.wav", pos, false);
                    API.SetSoundVolume("jump_sound", 0.9f);
                }
                catch { }

                // Drive jump via Trigger (Animator graph handles the pose)
                if (API.HasAnimator(Entity))
                    API.AnimatorSetTrigger(Entity, PARAM_JUMP);

                API.Log("[PlayerMovement] Jump!");
            }

            _prevSpaceDown = spaceDown;

            // PhysX applies gravity; we just set velocity.
            API.SetLinearVelocity(Entity, vel);

            // ---- Animator Parameters ----
            if (API.HasAnimator(Entity))
            {
                // Planar speed for 1D blend (Idle/Walk/Run)
                float planarSpeed = (float)Math.Sqrt(vel.X * vel.X + vel.Z * vel.Z);
                API.AnimatorSetFloat(Entity, PARAM_SPEED, planarSpeed);

                // Grounded
                API.AnimatorSetBool(Entity, PARAM_GROUNDED, isGrounded);
            }

            // ---- Face movement direction (instant) ----
            if (mag > 0.1f && API.HasTransform(Entity))
            {
                float angleRad = (float)Math.Atan2(moveDir.X, moveDir.Z); // Atan2(x, z)
                float angleDeg = angleRad * 180.0f / (float)Math.PI;

                var t = API.GetTransform(Entity);
                t.RotationY = angleDeg;
                API.SetTransform(Entity, t);
            }
        }

        // ---------------------------------------------------------
        // Triggers
        // ---------------------------------------------------------
        private void RegisterTriggerCallbacksOnAllTriggers()
        {
            string[] names = { "Checkpoint", "DamageZone", "PowerUp", "DoorTrigger", "TriggerVolume", "AreaTrigger" };
            int count = 0;

            foreach (var name in names)
            {
                ulong trig = API.FindEntity(name);
                API.Log($"[PlayerMovement] Lookup trigger '{name}' -> {trig}");
                if (trig == 0) continue;

                bool hasCol = API.HasCollider(trig);
                bool isTrig = API.IsTrigger(trig);
                API.Log($"[PlayerMovement]   hasCol={hasCol}, isTrigger={isTrig}");

                if (hasCol && isTrig)
                {
                    API.RegisterTriggerEnterCallback(trig, OnTriggerEnter);
                    API.RegisterTriggerExitCallback(trig, OnTriggerExit);
                    count++;
                }
            }

            API.Log($"[PlayerMovement] Registered trigger callbacks on {count} entities.");
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                if (otherEntity != s_playerEntity) return;

                Vec3 pos = new Vec3(0, 0, 0);
                if (API.HasTransform(triggerEntity)) pos = API.GetPosition(triggerEntity);

                ulong checkpoint = API.FindEntity("Checkpoint");
                ulong damageZone = API.FindEntity("DamageZone");
                ulong powerup = API.FindEntity("PowerUp");
                ulong door = API.FindEntity("DoorTrigger");

                bool played = false;

                if (triggerEntity == checkpoint && checkpoint != 0)
                {
                    API.Log("[PlayerMovement] >>> CHECKPOINT <<<");
                    API.PlaySoundAt("checkpoint", "Resources/Audio/playerPunch_1.wav", pos, false);
                    API.SetSoundVolume("checkpoint", 0.95f);
                    played = true;
                }
                else if (triggerEntity == damageZone && damageZone != 0)
                {
                    API.Log("[PlayerMovement] >>> DAMAGE ZONE <<<");
                    API.PlaySoundAt("damage", "Resources/Audio/playerPunch_1.wav", pos, false);
                    API.SetSoundVolume("damage", 0.8f);
                    played = true;
                }
                else if (triggerEntity == powerup && powerup != 0)
                {
                    API.Log("[PlayerMovement] >>> POWER-UP <<<");
                    API.PlaySoundAt("powerup", "Resources/Audio/playerPunch_1.wav", pos, false);
                    API.SetSoundVolume("powerup", 0.9f);
                    played = true;
                }
                else if (triggerEntity == door && door != 0)
                {
                    API.Log("[PlayerMovement] >>> DOOR OPEN <<<");
                    API.PlaySoundAt("door_open", "Resources/Audio/playerPunch_1.wav", pos, false);
                    API.SetSoundVolume("door_open", 0.85f);
                    played = true;
                }

                if (!played)
                {
                    API.PlaySoundAt("generic_trigger", "Resources/Audio/playerPunch_1.wav", pos, false);
                    API.SetSoundVolume("generic_trigger", 0.7f);
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] OnTriggerEnter ERROR: {ex.Message}");
            }
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                if (otherEntity != s_playerEntity) return;

                ulong damageZone = API.FindEntity("DamageZone");
                ulong door = API.FindEntity("DoorTrigger");

                if (triggerEntity == damageZone && damageZone != 0)
                {
                    API.Log("[PlayerMovement] >>> LEAVE DAMAGE ZONE <<<");
                    API.StopSound("damage");
                }
                else if (triggerEntity == door && door != 0)
                {
                    API.Log("[PlayerMovement] >>> DOOR CLOSE <<<");
                    Vec3 pos = new Vec3(0, 0, 0);
                    if (API.HasTransform(triggerEntity)) pos = API.GetPosition(triggerEntity);
                    API.PlaySoundAt("door_close", "Resources/Audio/playerPunch_1.wav", pos, false);
                    API.SetSoundVolume("door_close", 0.75f);
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] OnTriggerExit ERROR: {ex.Message}");
            }
        }

        // ---------------------------------------------------------
        // Public knobs
        // ---------------------------------------------------------
        public void SetWalkSpeed(float s) => _walkSpeed = Math.Max(0f, s);
        public void SetRunSpeed(float s) => _runSpeed = Math.Max(0f, s);
        public void SetJumpSpeed(float j) => _jumpSpeed = Math.Max(0f, j);

        public bool IsMoving()
        {
            var v = API.GetLinearVelocity(Entity);
            float planar = (float)Math.Sqrt(v.X * v.X + v.Z * v.Z);
            return planar > 0.1f;
        }

        public bool IsGrounded() => API.IsColliding(Entity);

        public void SetFootstepVolume(float volume) { try { _footstep?.SetFootstepVolume(volume); } catch { } }
        public void SetFootstepInterval(float interval) { try { _footstep?.SetFootstepInterval(interval); } catch { } }
    }
}
