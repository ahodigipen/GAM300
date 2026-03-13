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

        public static bool CutsceneMode = false;

        // ==== DEBUG: Enable verbose crouch zone logging ====
        private const bool DEBUG_CROUCH = true;
        private float _debugLogTimer = 0f;
        private const float DEBUG_LOG_INTERVAL = 1.0f; // Log state every 1 second

        private float _walkSpeed = 3f;
        private float _sprintSpeed = 8f;
        private float _sneakSpeed = 1.5f;

        [Boom.EditorExposed("Level Start Pos", "Specific coordinates for the 'Teleport to Start' action")]
        private Vec3 _levelStartPos = new Vec3(0.914043128f, 1.8f, 13.9171219f);
        [Boom.EditorExposed("Level 2 Pos", "Specific coordinates for the 'Teleport to Level 2' action")]
        private Vec3 _level2Pos = new Vec3(-0.349f, 18.699f, 32.891f);
        [Boom.EditorExposed("Controller Offset Y", "Vertical offset for the character controller (matches YAML LocalOffset)")]
        private float _controllerOffsetY = 1.47f;

        private static int s_persistedHealth = 5;
        public static void ResetPersistedHealth() { s_persistedHealth = 5; }

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
        private const int CROUCH_KEY = API.KEY_LEFT_CONTROL;
        private bool _inCrouchZone = false;
        private bool _isCrouching = false;
        private static bool s_isStealthInvisible = false;
        private bool _prevStealthInvisible = false;
        private const float STEALTH_OPACITY = 0.30f;

        // NEW: Track all crouch zone entity IDs
        private HashSet<ulong> _crouchZoneIDs = new HashSet<ulong>();

        // ==== God Mode (No Detection Cheat) ====
        private static bool s_godMode = false;
        private bool _wasGodModePressed = false;
        private ulong _godModeTextEntity = 0;
        private float _godModeTextTimer = 0f;
        private const float GOD_MODE_TEXT_DISPLAY = 2.0f;

        // ==== Freeze Ability Fields ====
        private const int USE_FREEZE = API.KEY_E;
        private bool _wasUseFreezeDown = false;

        [Boom.EditorExposed("Freeze Radius", "Range of the freeze effect")]
        private float _freezeRadius = 6.0f;
        [Boom.EditorExposed("Freeze Duration", "How long the freeze lasts")]
        private float _freezeDuration = 3.0f;

#pragma warning disable CS0414 // Field is assigned but its value is never read
        private bool _canPickupFreeze = false;
#pragma warning restore CS0414
        private ulong _currentPickupEntity = 0;

        private HashSet<ulong> _freezePickupIDs = new HashSet<ulong>();
        private const int MAX_PICKUPS_TO_CHECK = 10;

        // ==== Gravity constant ====
        private const float GRAVITY = 50f;
        private const float GROUND_STICK = -5.0f;

        // ==== CRITICAL: Manual vertical velocity tracking for Character Controller ====
        private float _verticalVelocity = 0f;
        // Track current forward movement speed for other systems (falling platforms prediction)
        private float _currentMoveSpeed = 0f;

        // ==== Stealth Opacity Helper ====
        private void ApplyStealthOpacity(bool invisible)
        {
            if (!API.HasModelComponent(Entity)) return;
            API.SetModelOpacity(Entity, invisible ? STEALTH_OPACITY : 1.0f);
        }

        // ==== DEBUG: Helper to log crouch state ====
        private static void DebugCrouch(string message)
        {
            //if (DEBUG_CROUCH)
            //{
            //    API.Log($"[CROUCH_DEBUG] {message}");
            //}
        }

        private void DebugLogCrouchState(string context)
        {
            //if (DEBUG_CROUCH)
            //{
            //    API.Log($"[CROUCH_DEBUG] === STATE ({context}) ===");
            //    API.Log($"[CROUCH_DEBUG]   _inCrouchZone:        {_inCrouchZone}");
            //    API.Log($"[CROUCH_DEBUG]   _isCrouching:         {_isCrouching}");
            //    API.Log($"[CROUCH_DEBUG]   s_isStealthInvisible: {s_isStealthInvisible}");
            //    API.Log($"[CROUCH_DEBUG]   _isInvulnerable:      {_isInvulnerable}");
            //    API.Log($"[CROUCH_DEBUG]   IsPlayerInvisibleToEnemies(): {IsPlayerInvisibleToEnemies()}");
            //    API.Log($"[CROUCH_DEBUG]   Registered zones: {_crouchZoneIDs.Count}");
            //}
        }

        public void OnStart(string jsonParams)
        {
            API.Log($"[PlayerMovement] OnStart() - Entity: {Entity}");
            API.SetScreenFadeAlpha(0f);
            s_playerEntity = Entity;
            s_instance = this;

            // Reset god mode (static field persists across scene loads)
            s_godMode = false;

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

            _health = s_persistedHealth;

            _footstepComponent = new FootstepComponent { Entity = Entity };
            _footstepComponent.OnStart("");

            try
            {
                API.CreateController(Entity, 0.8f, 4.8f);
                API.Log("[PlayerMovement] Character controller created successfully");
                
                // NEW: Search for a scene-specific spawn point entity named "PlayerSpawn"
                // This allows the user to set a different start point for each scene visually
                ulong spawnPointID = API.FindEntity("PlayerSpawn");
                if (spawnPointID != 0)
                {
                    _levelStartPos = API.GetPosition(spawnPointID);
                    API.Log($"[PlayerMovement] Found scene-specific spawn point: {_levelStartPos}");
                }

                // Teleport to the designated level start position
                TeleportToStart();
                
                // Set initial spawn point to the teleported location
                _spawnPoint = API.GetPosition(Entity);
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] CreateController failed: {ex.Message}");
                _spawnPoint = API.GetPosition(Entity);
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
            _prevStealthInvisible = false;
            ApplyStealthOpacity(false);

            // Initialize current move speed
            _currentMoveSpeed = 0f;

            // Find god mode text entity (optional - add a TextComponent entity named "UI_GodMode" to scene)
            _godModeTextEntity = API.FindEntity("UI_GodMode");
            if (_godModeTextEntity != 0 && API.HasText(_godModeTextEntity))
            {
                API.SetText(_godModeTextEntity, "");
                API.SetTextColor(_godModeTextEntity, new Vec4(0, 0, 0, 0));
            }

            DebugCrouch($"OnStart complete. Player Entity: {Entity}");
            DebugLogCrouchState("OnStart");
        }

        public void OnCaughtByEnemy(ulong enemyEntity)
        {
            DebugCrouch($"OnCaughtByEnemy called! Enemy: {enemyEntity}");
            DebugLogCrouchState("OnCaughtByEnemy");

            if (_isInvulnerable || _isRespawning)
            {
                DebugCrouch($"  -> BLOCKED: _isInvulnerable={_isInvulnerable}, _isRespawning={_isRespawning}");
                return;
            }

            _health--;
            s_persistedHealth = _health;
            HUD.SetHealth(_health, _maxHealth);

            Vec3 playerPos = API.GetPosition(Entity);
            API.PlaySoundAt("player_damage", "Resources/Audio/playerPunch_1.wav", playerPos, false);
            API.Set3DMinMaxDistance("player_damage", 1.0f, 20.0f);
            API.SetSoundVolume("player_damage", 1.0f);

            if (_health <= 0)
                Entry.TriggerPlayerDeath();
            else
                StartRespawn();
        }

        private void StartRespawn()
        {
            _isRespawning = true;
            _verticalVelocity = 0f;
            // REMOVED: Don't overwrite _spawnPoint here - it should keep the checkpoint value
            // _spawnPoint = new Vec3(0.914043128f, 1.5f, 13.9171219f);
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

            // Apply manual offset to the teleport position since API.TeleportController 
            // ignores the localOffset in CharacterControllerComponent
            Vec3 teleportPos = new Vec3(_spawnPoint.X, _spawnPoint.Y + _controllerOffsetY, _spawnPoint.Z);
            API.TeleportController(Entity, teleportPos);
            
            API.SetPosition(Entity, _spawnPoint);

            _isInvulnerable = true;
            _invulnerabilityTimer = 0f;
            HUD.SetHealth(_health, _maxHealth);
            _isRespawning = false;

            PlayerManager.NotifyPlayerRespawned();
            API.Log("[PlayerMovement] Player respawned - enemies notified");
            SpotlightFollower.ResetAllSpotlights();

            API.Log($"[PlayerMovement] Respawned at ({_spawnPoint.X}, {_spawnPoint.Y}, {_spawnPoint.Z}) with Offset Y: {_controllerOffsetY}");
        }

        public void UpdateCheckpoint(Vec3 newCheckpoint)
        {
            _spawnPoint = newCheckpoint;
            API.PlaySoundAt("checkpoint_save", "Resources/Audio/playerPunch_1.wav", newCheckpoint, false);
            API.Set3DMinMaxDistance("checkpoint_save", 1.0f, 15.0f);
            API.SetSoundVolume("checkpoint_save", 0.8f);
        }

        public void RestoreHealth(int amount)
        {
            _health = Math.Min(_health + amount, _maxHealth);
            s_persistedHealth = _health;
            HUD.SetHealth(_health, _maxHealth);
            API.Log($"[PlayerMovement] Health restored by {amount}. Current health: {_health}/{_maxHealth}");
        }

        public void TeleportTo(Vec3 position)
        {
            _verticalVelocity = 0f;

            // Apply manual offset to the teleport position since API.TeleportController 
            // ignores the localOffset in CharacterControllerComponent
            Vec3 teleportPos = new Vec3(position.X, position.Y + _controllerOffsetY, position.Z);
            API.TeleportController(Entity, teleportPos);

            API.SetPosition(Entity, position);
            API.Log($"[PlayerMovement] Teleported to ({position.X:F2}, {position.Y:F2}, {position.Z:F2}) with Offset Y: {_controllerOffsetY}");
        }

        public void TeleportToStart()
        {
            TeleportTo(_levelStartPos);
        }

        public void TeleportToLastCheckpoint()
        {
            RespawnAtCheckpoint();
        }

        public void TeleportToLevel2()
        {
            TeleportTo(_level2Pos);
        }

        private void RegisterTriggerCallbacksOnAllTriggers()
        {
            _freezePickupIDs.Clear();
            _crouchZoneIDs.Clear();

            DebugCrouch("=== Registering Trigger Callbacks ===");

            string[] standardTriggers = { "Checkpoint", "DamageZone", "PowerUp", "DoorTrigger", "TriggerVolume", "AreaTrigger", "CrouchTriggerZone" };
            foreach (string name in standardTriggers)
            {
                ulong id = API.FindEntity(name);
                if (id != 0 && API.HasCollider(id) && API.IsTrigger(id))
                {
                    API.RegisterTriggerEnterCallback(id, OnTriggerEnter);
                    API.RegisterTriggerExitCallback(id, OnTriggerExit);
                    DebugCrouch($"  Registered standard trigger: {name} (ID: {id})");
                }
            }

            // Register crouch zones - search for multiple possible names
            RegisterCrouchZone("CrouchTriggerZone");
            RegisterCrouchZone("CrouchZone");
            RegisterCrouchZone("StealthZone");
            for (int i = 1; i <= 10; i++)
            {
                RegisterCrouchZone($"CrouchTriggerZone_{i}");
                RegisterCrouchZone($"CrouchZone_{i}");
                RegisterCrouchZone($"StealthZone_{i}");
            }

            RegisterFreezePickup("FreezePowerUp");
            for (int i = 1; i <= MAX_PICKUPS_TO_CHECK; i++)
            {
                RegisterFreezePickup($"FreezePowerUp_{i}");
            }

            DebugCrouch($"=== Registration Complete: {_crouchZoneIDs.Count} crouch zones ===");
        }

        private void RegisterCrouchZone(string name)
        {
            ulong id = API.FindEntity(name);
            if (id != 0 && API.HasCollider(id) && API.IsTrigger(id))
            {
                DebugCrouch($"  Found Crouch Zone: {name} (ID: {id})");
                API.RegisterTriggerEnterCallback(id, OnTriggerEnter);
                API.RegisterTriggerExitCallback(id, OnTriggerExit);
                _crouchZoneIDs.Add(id);
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
            if (CutsceneMode)
            {
                // Ensure we don't drift
                if (API.HasTransform(Entity)) API.SetLinearVelocity(Entity, new Vec3(0, 0, 0));
                return;
            }

            UpdateFade(dt);
            if (!API.HasTransform(Entity) || !API.HasScript(Entity)) return;

            FreezeManager.Update(dt);

            // ==== DEBUG: Periodic state logging ====
            if (DEBUG_CROUCH)
            {
                _debugLogTimer += dt;
                if (_debugLogTimer >= DEBUG_LOG_INTERVAL)
                {
                    _debugLogTimer = 0f;
                    DebugLogCrouchState("Periodic Update");
                }
            }

            // Cheat: God Mode toggle (F8 on keyboard, LB+RB on controller)
            bool isGodModeInput = API.IsKeyDown(API.KEY_F8)
                || (API.IsGamepadConnected()
                    && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_LEFT_BUMPER)
                    && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_RIGHT_BUMPER));
            bool godModePressed = isGodModeInput && !_wasGodModePressed;
            _wasGodModePressed = isGodModeInput;
            if (godModePressed)
            {
                s_godMode = !s_godMode;
                API.Log($"[CHEAT] God Mode (No Detection): {(s_godMode ? "ON" : "OFF")}");

                if (_godModeTextEntity != 0 && API.HasText(_godModeTextEntity))
                {
                    if (s_godMode)
                    {
                        API.SetText(_godModeTextEntity, "GOD MODE: ON");
                        API.SetTextColor(_godModeTextEntity, new Vec4(0f, 1f, 0f, 1f));
                        _godModeTextTimer = -1f; // stay on screen
                    }
                    else
                    {
                        API.SetText(_godModeTextEntity, "GOD MODE: OFF");
                        API.SetTextColor(_godModeTextEntity, new Vec4(1f, 0f, 0f, 1f));
                        _godModeTextTimer = GOD_MODE_TEXT_DISPLAY; // fade after 2s
                    }
                }
            }

            // Hide "OFF" text after timer
            if (_godModeTextTimer > 0f)
            {
                _godModeTextTimer -= dt;
                if (_godModeTextTimer <= 0f && _godModeTextEntity != 0 && API.HasText(_godModeTextEntity))
                {
                    API.SetText(_godModeTextEntity, "");
                    API.SetTextColor(_godModeTextEntity, new Vec4(0, 0, 0, 0));
                }
            }

            bool isUseFreeze = API.IsKeyDown(USE_FREEZE) ||
                               (API.IsGamepadConnected() &&
                               (API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_X) ||
                               API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_Y)));
            bool isFreezePressed = isUseFreeze && !_wasUseFreezeDown;
            _wasUseFreezeDown = isUseFreeze;

            if (API.GetApplicationState() != API.APP_STATE_PAUSED)
            {
                if (isFreezePressed)
                {
                    if (PlayerInventory.HasFreezePower())
                    {
                        if (!FreezeManager.IsFrozen(API.GetPosition(Entity)))
                        {
                            PlayerInventory.ConsumeFreezeCharge();
                            Vec3 playerPos = API.GetPosition(Entity);
                            FreezeManager.TriggerFreeze(playerPos, _freezeRadius, _freezeDuration);
                            API.Log("[PlayerMovement] Freeze activated! (Input: E or Gamepad X/Y)");
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

            // Crouch logic - CTRL key works anywhere, stealth invisibility only in crouch zones
            bool crouchDown = API.IsKeyDown(CROUCH_KEY) || (API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_B));
            if (crouchDown && !_isCrouching)
            {
                _isCrouching = true;
                DebugCrouch($"Crouch START - _inCrouchZone={_inCrouchZone}");

                // Only become invisible to enemies when in a crouch zone
                if (_inCrouchZone)
                {
                    s_isStealthInvisible = true;
                    DebugCrouch("  -> INVISIBLE (crouching in zone)");
                }
                else
                {
                    DebugCrouch("  -> VISIBLE (not in zone)");
                }

                if (_hasAnimator) API.AnimatorSetBool(Entity, "IsCrouching", true);
            }
            else if (!crouchDown && _isCrouching)
            {
                _isCrouching = false;
                s_isStealthInvisible = false;
                DebugCrouch("Crouch END - now VISIBLE");
                if (_hasAnimator) API.AnimatorSetBool(Entity, "IsCrouching", false);
            }

            // Update stealth visibility based on crouch zone while crouching
            if (_isCrouching)
            {
                bool wasInvisible = s_isStealthInvisible;
                s_isStealthInvisible = _inCrouchZone;

                // Log state changes
                if (s_isStealthInvisible != wasInvisible)
                {
                    if (s_isStealthInvisible)
                        DebugCrouch("Stealth CHANGED: INVISIBLE (entered zone while crouching)");
                    else
                        DebugCrouch("Stealth CHANGED: VISIBLE (left zone while crouching)");
                }
            }

            // Apply/remove translucency when stealth state changes
            if (s_isStealthInvisible != _prevStealthInvisible)
            {
                _prevStealthInvisible = s_isStealthInvisible;
                ApplyStealthOpacity(s_isStealthInvisible);
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

            // --- Gravity Handling ---
            if (!isGrounded)
            {
                // Airborne: apply gravity acceleration
                _verticalVelocity -= GRAVITY * dt;
            }
            else
            {
                // Grounded: use small stick force to maintain ground contact
                // Only reset if falling (preserves upward velocity for jumps if added later)
                if (_verticalVelocity < 0f)
                {
                    _verticalVelocity = GROUND_STICK;
                }
            }

            // Clamp terminal velocity
            _verticalVelocity = Math.Max(_verticalVelocity, -50f);
            _verticalVelocity = Math.Min(_verticalVelocity, 15f);

            float inputX = 0f, inputZ = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) inputX += 1f;
                if (API.IsKeyDown(API.KEY_D)) inputX -= 1f;
                if (API.IsKeyDown(API.KEY_W)) inputZ += 1f;
                if (API.IsKeyDown(API.KEY_S)) inputZ -= 1f;

                // Add Gamepad support for movement
                if (API.IsGamepadConnected())
                {
                    float gpX = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_X);
                    float gpY = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_Y);

                    // Deadzone handling (simple)
                    if (Math.Abs(gpX) > 0.15f) inputX -= gpX; // GLFW X is positive right, our logic was A=+1, D=-1
                    if (Math.Abs(gpY) > 0.15f) inputZ -= gpY; // GLFW Y is positive down, our logic was W=+1, S=-1
                }
            }

            bool hasInput = (inputX != 0f || inputZ != 0f);
            
            // Sprint: Left Trigger on gamepad
            // With XInput integration in InputHandler.h, triggers are normalized from 0.0 (released) to 1.0 (fully pressed)
            // Use a threshold of 0.3 to detect when the trigger is pressed
            bool sprintKey = API.IsKeyDown(API.KEY_LEFT_SHIFT);
            if (API.IsGamepadConnected())
            {
                float leftTrigger = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_TRIGGER);
                // XInput: normalized 0.0 to 1.0 after deadzone, threshold at 0.3 means pressed
                sprintKey = sprintKey || (leftTrigger > 0.3f);
            }
            
            bool sneakKey = API.IsKeyDown(API.KEY_LEFT_CONTROL) || (API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_B));

            float currentSpeed = _walkSpeed;
            if (sneakKey) currentSpeed = _sneakSpeed;
            else if (sprintKey) currentSpeed = _sprintSpeed;

            // Use analog stick magnitude for speed if using gamepad
            if (API.IsGamepadConnected() && hasInput)
            {
                float gpX = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_X);
                float gpY = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_Y);
                float mag = (float)Math.Sqrt(gpX * gpX + gpY * gpY);
                if (mag > 0.15f)
                {
                    currentSpeed *= Math.Min(1.0f, mag);
                }
            }

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

            bool ctrlDown = API.IsKeyDown(API.KEY_LEFT_CONTROL) || (API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_B));
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
                // expose current move speed for external scripts
                _currentMoveSpeed = (float)Math.Sqrt(velX * velX + velZ * velZ);
                _isInvulnerable = true;
                _wasCtrlPressed = ctrlDown;

                Vec3 displacement = new Vec3(velX * dt, _verticalVelocity * dt, velZ * dt);
                API.MoveController(Entity, displacement, 0.001f, dt);
                return;
            }

            if (_isRolling)
            {
                _rollTimer -= dt;
                velX = _rollDir.X * _rollSpeed;
                velZ = _rollDir.Z * _rollSpeed;
                _currentMoveSpeed = (float)Math.Sqrt(velX * velX + velZ * velZ);
                if (_rollTimer <= 0f)
                {
                    _isRolling = false;
                    _rollCooldownTimer = _rollCooldown;
                    _isInvulnerable = false;
                    if (_hasAnimator) API.AnimatorSetBool(Entity, "IsRolling", false);
                }
                _wasCtrlPressed = ctrlDown;

                Vec3 displacement = new Vec3(velX * dt, _verticalVelocity * dt, velZ * dt);
                API.MoveController(Entity, displacement, 0.001f, dt);
                return;
            }

            if (_rollCooldownTimer > 0f)
                _rollCooldownTimer = Math.Max(0f, _rollCooldownTimer - dt);
            _wasCtrlPressed = ctrlDown;

            float speedXZ = (float)Math.Sqrt(velX * velX + velZ * velZ);
            _currentMoveSpeed = speedXZ;
            Vec3 finalDisplacement = new Vec3(velX * dt, _verticalVelocity * dt, velZ * dt);
            API.MoveController(Entity, finalDisplacement, 0.001f, dt);

            if (_hasAnimator)
            {
                _smoothedSpeed += (speedXZ - _smoothedSpeed) * Math.Min(1.0, SPEED_DAMP * dt);
                API.AnimatorSetFloat(Entity, "Speed", (float)_smoothedSpeed);
                API.AnimatorSetBool(Entity, "IsMoving", _smoothedSpeed > MOVE_EPS || hasInput);
                API.AnimatorSetBool(Entity, "Sprint", sprintKey && hasInput);
                API.AnimatorSetBool(Entity, "IsSneaking", sneakKey && hasInput);
            }
        }

        private bool IsPlayerGrounded()
        {
            // Use the engine's native controller grounded check.
            // The previous raycast with 0.4f distance was causing false positives while airborne,
            // resulting in "sticky" gravity (constant fall speed) instead of proper acceleration.
            return API.IsControllerGrounded(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                DebugCrouch($"OnTriggerEnter: trigger={triggerEntity}, other={otherEntity}, player={s_playerEntity}");

                if (otherEntity != s_playerEntity || s_instance == null)
                {
                    DebugCrouch($"  -> IGNORED (not player or no instance)");
                    return;
                }

                // Check if this is a crouch zone using our cached IDs
                if (s_instance._crouchZoneIDs.Contains(triggerEntity))
                {
                    s_instance._inCrouchZone = true;
                    DebugCrouch($"  -> CROUCH ZONE ENTERED (ID: {triggerEntity})");
                    DebugCrouch($"     _isCrouching={s_instance._isCrouching}");

                    // If already crouching, immediately become invisible
                    if (s_instance._isCrouching)
                    {
                        s_isStealthInvisible = true;
                        DebugCrouch($"     Already crouching - now INVISIBLE");
                    }
                    return;
                }
                else
                {
                    DebugCrouch($"  -> NOT a registered crouch zone");
                }

                // Check freeze pickups
                if (s_instance._freezePickupIDs.Contains(triggerEntity))
                {
                    // Don't allow pickup if any popup/tutorial is active (prevents UI overlap)
                    if (TutorialPopupTrigger.IsPopupActive() || TutorialManager.IsTutorialActive())
                    {
                        API.Log("[PlayerMovement] Cannot pickup freeze - another popup/tutorial is active");
                        return;
                    }

                    if (PlayerInventory.TryAddFreezeCharge())
                    {
                        API.Log($"[PlayerMovement] Instant Pickup: Freeze Powerup (ID: {triggerEntity})");

                        // Show pickup tutorial (first-time or repeat) for Talisman
                        TutorialManager.ShowPickupTutorial(
                            TutorialManager.ItemType.Talisman,
                            PlayerInventory.GetTalismanPickupCount()
                        );
                        
                        API.DestroyEntity(triggerEntity);
                    }
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
                DebugCrouch($"OnTriggerExit: trigger={triggerEntity}, other={otherEntity}, player={s_playerEntity}");

                if (otherEntity != s_playerEntity || s_instance == null)
                {
                    DebugCrouch($"  -> IGNORED (not player or no instance)");
                    return;
                }

                // Check if this is a crouch zone using our cached IDs
                if (s_instance._crouchZoneIDs.Contains(triggerEntity))
                {
                    s_instance._inCrouchZone = false;
                    DebugCrouch($"  -> CROUCH ZONE EXITED (ID: {triggerEntity})");

                    // Immediately become visible when leaving crouch zone
                    s_isStealthInvisible = false;
                    DebugCrouch($"     Now VISIBLE to enemies");
                    return;
                }

                // Check freeze pickups
                if (s_instance._freezePickupIDs.Contains(triggerEntity))
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

        // PUBLIC: Allow external scripts (like CrouchTriggerZone) to notify us
        public static void SetInCrouchZone(bool inZone)
        {
            DebugCrouch($"SetInCrouchZone called externally: inZone={inZone}");

            if (s_instance == null)
            {
                DebugCrouch("  -> IGNORED (no instance)");
                return;
            }

            bool wasInZone = s_instance._inCrouchZone;
            s_instance._inCrouchZone = inZone;

            if (inZone && !wasInZone)
            {
                DebugCrouch("  External: Entered crouch zone");
                if (s_instance._isCrouching)
                {
                    s_isStealthInvisible = true;
                    DebugCrouch("  Already crouching - now INVISIBLE");
                }
            }
            else if (!inZone && wasInZone)
            {
                DebugCrouch("  External: Exited crouch zone");
                s_isStealthInvisible = false;
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
            ApplyStealthOpacity(false);
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
        public static bool IsPlayerInvisibleToEnemies()
        {
            if (s_instance == null) return false;

            // Cheat: God mode bypass
            if (s_godMode) return true;

            // Check crouch stealth
            if (s_isStealthInvisible) return true;

            // Check invulnerability (respawn protection)
            if (s_instance._isInvulnerable) return true;

            return false;
        }

        // Debug helper
        public static bool IsInCrouchZone()
        {
            return s_instance != null && s_instance._inCrouchZone;
        }

        public static bool IsCrouching()
        {
            return s_instance != null && s_instance._isCrouching;
        }

        // Provide current forward speed (approx) for external systems
        public static float GetCurrentMoveSpeed()
        {
            return s_instance != null ? s_instance._currentMoveSpeed : 0f;
        }
    }
}