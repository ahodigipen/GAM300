using Boom;
using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace GameScripts
{
    public static class HUD
    {
        public static float HealthRatio = 1f;
        public static void SetHealth(int hp, int max)
        {
            if (max <= 0) { HealthRatio = 0f; return; }
            float r = (float)hp / (float)max;
            if (r < 0f) r = 0f;
            if (r > 1f) r = 1f;
            HealthRatio = r;
        }
    }

    public class PlayerMovement
    {
        public ulong Entity;

        private float _walkSpeed = 3f;
        private float _sprintSpeed = 8f;
        private float _sneakSpeed = 1.5f;

        private int _health = 5;
        private int _maxHealth = 5;
        private Vec3 _spawnPoint;
        private bool _isRespawning = false;
        private bool _isInvulnerable = false;
        private float _invulnerabilityDuration = 2.0f;
        private float _invulnerabilityTimer = 0f;

        private enum FadeState { None, FadingOut, BlackHold, FadingIn }
        private FadeState _fadeState = FadeState.None;
        private float _fadeTimer = 0f;
        private float _fadeOutDuration = 0.5f;
        private float _blackHoldDuration = 0.15f;
        private float _fadeInDuration = 0.75f;

        private FootstepComponent _footstepComponent;

        private static ulong s_playerEntity = 0;
        private static PlayerMovement s_instance = null;

        private float _modelForwardOffset = 0;

        private const float MOVE_EPS = 0.10f;
        private const float SPEED_DAMP = 10f;
        private double _smoothedSpeed = 0.0;
        private bool _hasAnimator = false;

        private float _rollSpeed = 14.0f;
        private float _rollDuration = 0.9f;
        private float _rollCooldown = 0.35f;

        private bool _isRolling = false;
        private float _rollTimer = 0f;
        private float _rollCooldownTimer = 0f;
        private bool _wasCtrlPressed = false;
        private Vec3 _rollDir = new Vec3(0, 0, 0);

        // ==== Crouch / Stealth Fields ====
        private const int CROUCH_KEY = API.KEY_Q;
        private bool _inCrouchZone = false;
        private bool _isCrouching = false;
        private static bool s_isStealthInvisible = false;

        // ==== Freeze Ability Fields ====
        private const int PICKUP_KEY = API.KEY_F;
        private const int USE_KEY = API.KEY_G;

        private bool _canPickupFreeze = false;
        private ulong _currentPickupEntity = 0;

        private bool _wasPickupKeyDown = false;
        private bool _wasUseKeyDown = false;

        private HashSet<ulong> _freezePickupIDs = new HashSet<ulong>();
        private const int MAX_PICKUPS_TO_CHECK = 10;

        // ==== Gravity constant ====
        private const float GRAVITY = 20f;
        private const float GROUND_STICK = -5.0f;

        // ==== CRITICAL: Manual vertical velocity tracking for Character Controller ====
        private float _verticalVelocity = 0f;

        public void OnStart(string jsonParams)
        {
            API.Log($"[PlayerMovement] OnStart() - Entity: {Entity}");
            API.SetScreenFadeAlpha(0f);
            s_playerEntity = Entity;
            s_instance = this;
            Vec3 rot = API.GetRotation(Entity);
            API.SetRotation(Entity, new Vec3(0, rot.Y, 0));
            PlayerManager.RegisterPlayer(this);

            if (!API.HasTransform(Entity))
            {
                API.Log("[PlayerMovement] ERROR: Entity missing TransformComponent!");
                return;
            }
            if (!API.HasScript(Entity))
            {
                API.Log("[PlayerMovement] ERROR: Entity missing ScriptComponent!");
                return;
            }

            _spawnPoint = API.GetPosition(Entity);
            _health = _maxHealth;

            _footstepComponent = new FootstepComponent { Entity = Entity };
            _footstepComponent.OnStart("");

            try
            {
                API.CreateController(Entity, 0.8f, 4.8f);
                API.Log("[PlayerMovement] Character controller created successfully");
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] CreateController failed: {ex.Message}");
            }

            if (API.HasAnimator(Entity))
            {
                _hasAnimator = true;
                API.AnimatorSetFloat(Entity, "Speed", 0f);
                API.AnimatorSetBool(Entity, "IsMoving", false);
                API.AnimatorSetBool(Entity, "Sprint", false);
                API.AnimatorSetBool(Entity, "IsSneaking", false);
                API.AnimatorSetBool(Entity, "IsRolling", false);
                API.AnimatorSetBool(Entity, "IsCrouching", false);
            }

            RegisterTriggerCallbacksOnAllTriggers();
            HUD.SetHealth(_health, _maxHealth);
        }

        public void OnCaughtByEnemy(ulong enemyEntity)
        {
            if (_isInvulnerable || _isRespawning) return;

            _health--;
            HUD.SetHealth(_health, _maxHealth);

            Vec3 playerPos = API.GetPosition(Entity);
            API.PlaySoundAt("player_damage", "Resources/Audio/playerPunch_1.wav", playerPos, false);
            API.Set3DMinMaxDistance("player_damage", 1.0f, 20.0f);
            API.SetSoundVolume("player_damage", 1.0f);

            if (_health <= 0)
                //RestartLevel();
                Entry.TriggerPlayerDeath();
            else
                StartRespawn();
        }

        private void StartRespawn()
        {
            _isRespawning = true;
            _verticalVelocity = 0f;
            // Set spawn point to the desired respawn location
            _spawnPoint = new Vec3(0.914043128f, 1.5f, 13.9171219f);
            // Stop movement by applying zero displacement
            API.MoveController(Entity, new Vec3(0, 0, 0), 0.001f, 0.016f);
            _fadeState = FadeState.FadingOut;
            _fadeTimer = 0f;
            API.SetScreenFadeAlpha(0f);
        }
        private void RestartLevel()
        {
            Vec3 playerPos = API.GetPosition(Entity);
            API.PlaySoundAt("player_death", "Resources/Audio/playerPunch_1.wav", playerPos, false);
            API.Set3DMinMaxDistance("player_death", 2.0f, 30.0f);
            API.SetSoundVolume("player_death", 1.0f);
            API.LoadScene(API.GetCurrentSceneName());
        }

        private void RespawnAtCheckpoint()
        {
            _verticalVelocity = 0f;

            // This will now work since the binding is fixed
            API.TeleportController(Entity, _spawnPoint);
            API.SetPosition(Entity, _spawnPoint);

            _isInvulnerable = true;
            _invulnerabilityTimer = 0f;
            HUD.SetHealth(_health, _maxHealth);
            _isRespawning = false;

            SpotlightFollower.ResetAllSpotlights();

            API.Log($"[PlayerMovement] Respawned at ({_spawnPoint.X}, {_spawnPoint.Y}, {_spawnPoint.Z})");
        }

        public void UpdateCheckpoint(Vec3 newCheckpoint)
        {
            _spawnPoint = newCheckpoint;
            API.PlaySoundAt("checkpoint_save", "Resources/Audio/playerPunch_1.wav", newCheckpoint, false);
            API.Set3DMinMaxDistance("checkpoint_save", 1.0f, 15.0f);
            API.SetSoundVolume("checkpoint_save", 0.8f);
        }

        private void RegisterTriggerCallbacksOnAllTriggers()
        {
            _freezePickupIDs.Clear();

            string[] standardTriggers = { "Checkpoint", "DamageZone", "PowerUp", "DoorTrigger", "TriggerVolume", "AreaTrigger" };
            foreach (string name in standardTriggers)
            {
                ulong id = API.FindEntity(name);
                if (id != 0 && API.HasCollider(id) && API.IsTrigger(id))
                {
                    API.RegisterTriggerEnterCallback(id, OnTriggerEnter);
                    API.RegisterTriggerExitCallback(id, OnTriggerExit);
                }
            }

            ulong crouchZone = API.FindEntity("CrouchTriggerZone");
            API.Log($"[PlayerMovement] CrouchTriggerZone entity ID: {crouchZone}");

            RegisterFreezePickup("FreezePowerUp");
            for (int i = 1; i <= MAX_PICKUPS_TO_CHECK; i++)
            {
                RegisterFreezePickup($"FreezePowerUp_{i}");
            }

            if (crouchZone != 0)
            {
                API.RegisterTriggerEnterCallback(crouchZone, OnTriggerEnter);
                API.RegisterTriggerExitCallback(crouchZone, OnTriggerExit);
            }
        }

        private void RegisterFreezePickup(string name)
        {
            ulong id = API.FindEntity(name);
            if (id != 0 && API.HasCollider(id) && API.IsTrigger(id))
            {
                API.Log($"[PlayerMovement] Found Freeze Pickup: {name} (ID: {id})");
                API.RegisterTriggerEnterCallback(id, OnTriggerEnter);
                API.RegisterTriggerExitCallback(id, OnTriggerExit);
                _freezePickupIDs.Add(id);
            }
        }

        private void UpdateFade(float dt)
        {
            switch (_fadeState)
            {
                case FadeState.FadingOut:
                    _fadeTimer += dt;
                    API.SetScreenFadeAlpha(Clamp01(_fadeTimer / Math.Max(0.0001f, _fadeOutDuration)));
                    if (_fadeTimer >= _fadeOutDuration)
                    {
                        API.SetScreenFadeAlpha(1f);
                        _fadeState = FadeState.BlackHold;
                        _fadeTimer = 0f;
                        RespawnAtCheckpoint();
                    }
                    break;
                case FadeState.BlackHold:
                    _fadeTimer += dt;
                    if (_fadeTimer >= _blackHoldDuration)
                    {
                        _fadeState = FadeState.FadingIn;
                        _fadeTimer = 0f;
                    }
                    break;
                case FadeState.FadingIn:
                    _fadeTimer += dt;
                    float a = 1f - Clamp01(_fadeTimer / Math.Max(0.0001f, _fadeInDuration));
                    API.SetScreenFadeAlpha(a);
                    if (_fadeTimer >= _fadeInDuration)
                    {
                        _fadeState = FadeState.None;
                        API.SetScreenFadeAlpha(0f);
                    }
                    break;
            }
        }

        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);

        public void OnUpdate(float dt)
        {
            UpdateFade(dt);
            if (!API.HasTransform(Entity) || !API.HasScript(Entity)) return;

            FreezeManager.Update(dt);

            bool isPickupDown = API.IsKeyDown(PICKUP_KEY);
            bool isPickupPressed = isPickupDown && !_wasPickupKeyDown;
            _wasPickupKeyDown = isPickupDown;

            bool isUseDown = API.IsKeyDown(USE_KEY);
            bool isUsePressed = isUseDown && !_wasUseKeyDown;
            _wasUseKeyDown = isUseDown;

            if (API.GetApplicationState() != API.APP_STATE_PAUSED)
            {
                if (isPickupPressed && _canPickupFreeze)
                {
                    if (!PlayerInventory.HasFreezePower())
                    {
                        if (PlayerInventory.TryAddFreezeCharge())
                        {
                            API.Log("[PlayerMovement] Picked up Freeze Ability!");

                            if (_currentPickupEntity != 0)
                            {
                                API.SetPosition(_currentPickupEntity, new Vec3(0, -5000, 0));
                                _canPickupFreeze = false;
                                _currentPickupEntity = 0;
                            }
                        }
                    }
                    else
                    {
                        API.Log("[PlayerMovement] Cannot pickup: You already have a Freeze Charge!");
                    }
                }

                if (isUsePressed)
                {
                    if (PlayerInventory.HasFreezePower())
                    {
                        if (!FreezeManager.IsFrozen(API.GetPosition(Entity)))
                        {
                            PlayerInventory.ConsumeFreezeCharge();
                            float radius = 6.0f;
                            float duration = 3.0f;
                            Vec3 playerPos = API.GetPosition(Entity);
                            FreezeManager.TriggerFreeze(playerPos, radius, duration);
                        }
                        else
                        {
                            API.Log("[PlayerMovement] Cannot use ability: Freeze already active!");
                        }
                    }
                    else
                    {
                        API.Log("[PlayerMovement] No Freeze Charge available!");
                    }
                }
            }

            _footstepComponent?.OnUpdate(dt);
            if (_isRespawning) return;

            if (_isInvulnerable)
            {
                _invulnerabilityTimer += dt;
                if (_invulnerabilityTimer >= _invulnerabilityDuration)
                    _isInvulnerable = false;
            }

            bool isGrounded = IsPlayerGrounded();

            // Crouch logic
            if (_inCrouchZone)
            {
                bool crouchDown = API.IsKeyDown(CROUCH_KEY);
                if (crouchDown && !_isCrouching)
                {
                    _isCrouching = true;
                    s_isStealthInvisible = true;
                    if (_hasAnimator) API.AnimatorSetBool(Entity, "IsCrouching", true);
                    API.Log("[PlayerMovement] Player entered crouch state");
                }
                else if (!crouchDown && _isCrouching)
                {
                    _isCrouching = false;
                    s_isStealthInvisible = false;
                    if (_hasAnimator) API.AnimatorSetBool(Entity, "IsCrouching", false);
                    API.Log("[PlayerMovement] Player exited crouch state");
                }
            }
            else if (_isCrouching)
            {
                _isCrouching = false;
                s_isStealthInvisible = false;
                if (_hasAnimator) API.AnimatorSetBool(Entity, "IsCrouching", false);
            }

            if (_isCrouching)
            {
                // Apply gravity while crouching
                if (!isGrounded)
                {
                    _verticalVelocity -= GRAVITY * dt;
                }
                else
                {
                    _verticalVelocity = -0.5f;
                }

                // Use MoveController with displacement (velocity * dt)
                Vec3 displacement = new Vec3(0, _verticalVelocity * dt, 0);
                API.MoveController(Entity, displacement, 0.001f, dt);

                if (_hasAnimator)
                {
                    API.AnimatorSetFloat(Entity, "Speed", 0f);
                    API.AnimatorSetBool(Entity, "IsMoving", false);
                    API.AnimatorSetBool(Entity, "Sprint", false);
                    API.AnimatorSetBool(Entity, "IsSneaking", false);
                }
                return;
            }

            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);

            // Update vertical velocity with gravity
            if (!isGrounded)
            {
                _verticalVelocity -= GRAVITY * dt;
            }
            else
            {
                // Only reset if we were actually falling
                if (_verticalVelocity < GROUND_STICK)
                {
                    _verticalVelocity = GROUND_STICK;
                }
                else
                {
                    _verticalVelocity = GROUND_STICK;
                }
            }

            // Clamp terminal velocity
            if (_verticalVelocity < -50f) _verticalVelocity = -50f;
            if (_verticalVelocity > 15f) _verticalVelocity = 15f;

            // Horizontal movement input
            float inputX = 0f, inputZ = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) inputX += 1f;
                if (API.IsKeyDown(API.KEY_D)) inputX -= 1f;
                if (API.IsKeyDown(API.KEY_W)) inputZ += 1f;
                if (API.IsKeyDown(API.KEY_S)) inputZ -= 1f;
            }

            bool hasInput = (inputX != 0f || inputZ != 0f);
            bool sprintKey = API.IsKeyDown(API.KEY_LEFT_SHIFT);
            bool sneakKey = API.IsKeyDown(API.KEY_LEFT_CONTROL);

            float currentSpeed = _walkSpeed;
            if (sneakKey) currentSpeed = _sneakSpeed;
            else if (sprintKey) currentSpeed = _sprintSpeed;

            // Calculate horizontal velocity
            float velX = 0f, velZ = 0f;
            if (hasInput)
            {
                float camYawDeg = API.GetThirdPersonCameraYaw();
                float yawRad = camYawDeg * (float)Math.PI / 180f;
                Vec3 forward = new Vec3((float)Math.Sin(yawRad), 0f, (float)Math.Cos(yawRad));
                Vec3 right = new Vec3((float)Math.Cos(yawRad), 0f, -(float)Math.Sin(yawRad));
                Vec3 moveDir = new Vec3(
                    right.X * inputX + forward.X * inputZ,
                    0f,
                    right.Z * inputX + forward.Z * inputZ
                );
                float len = (float)Math.Sqrt(moveDir.X * moveDir.X + moveDir.Z * moveDir.Z);
                if (len > 0f)
                {
                    velX = (moveDir.X / len) * currentSpeed;
                    velZ = (moveDir.Z / len) * currentSpeed;
                    float targetYaw = (float)(Math.Atan2(moveDir.X, moveDir.Z) * 180.0 / Math.PI) + _modelForwardOffset;
                    while (targetYaw > 180f) targetYaw -= 360f;
                    while (targetYaw < -180f) targetYaw += 360f;
                    API.SetRotationY(Entity, targetYaw);
                }
            }

            // Roll logic
            bool ctrlDown = API.IsKeyDown(API.KEY_LEFT_CONTROL);
            Vec3 desiredMoveDir = new Vec3(0, 0, 0);
            if (hasInput)
            {
                float camYawDeg = API.GetThirdPersonCameraYaw();
                float camYawRad = camYawDeg * (float)Math.PI / 180f;
                Vec3 fwd = new Vec3((float)Math.Sin(camYawRad), 0f, (float)Math.Cos(camYawRad));
                Vec3 right = new Vec3((float)Math.Cos(camYawRad), 0f, -(float)Math.Sin(camYawRad));
                desiredMoveDir = new Vec3(
                    right.X * inputX + fwd.X * inputZ,
                    0f,
                    right.Z * inputX + fwd.Z * inputZ
                );
            }

            if (!_isRolling && _rollCooldownTimer <= 0f && isGrounded && ctrlDown && !_wasCtrlPressed && hasInput)
            {
                float len = (float)Math.Sqrt(desiredMoveDir.X * desiredMoveDir.X + desiredMoveDir.Z * desiredMoveDir.Z);
                _rollDir = (len > 0f)
                    ? new Vec3(desiredMoveDir.X / len, 0f, desiredMoveDir.Z / len)
                    : new Vec3((float)Math.Sin(API.GetRotationY(Entity) * Math.PI / 180f), 0f,
                               (float)Math.Cos(API.GetRotationY(Entity) * Math.PI / 180f));
                _isRolling = true;
                _rollTimer = _rollDuration;
                if (_hasAnimator)
                {
                    API.AnimatorSetTrigger(Entity, "Roll");
                    API.AnimatorSetBool(Entity, "IsRolling", true);
                    API.AnimatorSetBool(Entity, "IsSneaking", false);
                    API.AnimatorSetBool(Entity, "Sprint", false);
                }
                velX = _rollDir.X * _rollSpeed;
                velZ = _rollDir.Z * _rollSpeed;
                _isInvulnerable = true;
                _wasCtrlPressed = ctrlDown;

                // Use MoveController with displacement (velocity * dt)
                Vec3 displacement = new Vec3(velX * dt, _verticalVelocity * dt, velZ * dt);
                API.MoveController(Entity, displacement, 0.001f, dt);
                return;
            }

            if (_isRolling)
            {
                _rollTimer -= dt;
                velX = _rollDir.X * _rollSpeed;
                velZ = _rollDir.Z * _rollSpeed;
                if (_rollTimer <= 0f)
                {
                    _isRolling = false;
                    _rollCooldownTimer = _rollCooldown;
                    _isInvulnerable = false;
                    if (_hasAnimator) API.AnimatorSetBool(Entity, "IsRolling", false);
                }
                _wasCtrlPressed = ctrlDown;

                // Use MoveController with displacement (velocity * dt)
                Vec3 displacement = new Vec3(velX * dt, _verticalVelocity * dt, velZ * dt);
                API.MoveController(Entity, displacement, 0.001f, dt);
                return;
            }

            if (_rollCooldownTimer > 0f)
                _rollCooldownTimer = Math.Max(0f, _rollCooldownTimer - dt);
            _wasCtrlPressed = ctrlDown;

            // Apply final movement using MoveController with displacement (velocity * dt)
            Vec3 finalDisplacement = new Vec3(velX * dt, _verticalVelocity * dt, velZ * dt);
            API.MoveController(Entity, finalDisplacement, 0.001f, dt);

            if (_hasAnimator)
            {
                float speedXZ = (float)Math.Sqrt(velX * velX + velZ * velZ);
                _smoothedSpeed += (speedXZ - _smoothedSpeed) * Math.Min(1.0, SPEED_DAMP * dt);
                API.AnimatorSetFloat(Entity, "Speed", (float)_smoothedSpeed);
                API.AnimatorSetBool(Entity, "IsMoving", _smoothedSpeed > MOVE_EPS || hasInput);
                API.AnimatorSetBool(Entity, "Sprint", sprintKey && hasInput);
                API.AnimatorSetBool(Entity, "IsSneaking", sneakKey && hasInput);
            }
        }

        private bool IsPlayerGrounded()
        {
            return API.IsControllerGrounded(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                if (otherEntity != s_playerEntity) return;

                ulong crouchZone = API.FindEntity("CrouchTriggerZone");
                if (triggerEntity == crouchZone && crouchZone != 0 && s_instance != null)
                {
                    s_instance._inCrouchZone = true;
                    API.Log("[PlayerMovement] Player ENTERED CrouchTriggerZone");
                    return;
                }

                if (s_instance != null && s_instance._freezePickupIDs.Contains(triggerEntity))
                {
                    s_instance._canPickupFreeze = true;
                    s_instance._currentPickupEntity = triggerEntity;
                    API.Log($"[PlayerMovement] Standing on Freeze Powerup (ID: {triggerEntity}). Press F to pickup.");
                    return;
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] ERROR in OnTriggerEnter: {ex.Message}");
            }
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                if (otherEntity != s_playerEntity) return;

                ulong crouchZone = API.FindEntity("CrouchTriggerZone");
                if (triggerEntity == crouchZone && crouchZone != 0 && s_instance != null)
                {
                    s_instance._inCrouchZone = false;
                    API.Log("[PlayerMovement] Player EXITED CrouchTriggerZone");

                    if (s_instance._isCrouching)
                    {
                        s_instance._isCrouching = false;
                        s_isStealthInvisible = false;
                        if (s_instance._hasAnimator)
                            API.AnimatorSetBool(s_instance.Entity, "IsCrouching", false);
                    }
                    return;
                }

                if (s_instance != null && s_instance._freezePickupIDs.Contains(triggerEntity))
                {
                    s_instance._canPickupFreeze = false;
                    if (s_instance._currentPickupEntity == triggerEntity)
                    {
                        s_instance._currentPickupEntity = 0;
                    }
                    API.Log("[PlayerMovement] Left Freeze Powerup zone.");
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] ERROR in OnTriggerExit: {ex.Message}");
            }
        }

        public void OnDestroy()
        {
            PlayerManager.UnregisterPlayer();
            if (s_playerEntity == Entity)
            {
                s_playerEntity = 0;
                s_instance = null;
            }
            _footstepComponent?.OnDestroy();
            if (API.HasCollider(Entity))
                API.UnregisterTriggerCallbacks(Entity);
            API.SetScreenFadeAlpha(0f);
        }

        public void SetFootstepVolume(float volume) => _footstepComponent?.SetFootstepVolume(volume);
        public void SetFootstepInterval(float interval) => _footstepComponent?.SetFootstepInterval(interval);
        public void SetModelForwardOffset(float degrees) => _modelForwardOffset = degrees;
        public void SetMovementSpeeds(float walk, float sprint, float sneak)
        {
            _walkSpeed = walk; _sprintSpeed = sprint; _sneakSpeed = sneak;
        }
        public int GetHealth() => _health;
        public static ulong GetPlayerEntity() => s_playerEntity;
        public static bool IsPlayerInvisibleToEnemies() => s_isStealthInvisible;
    }
}