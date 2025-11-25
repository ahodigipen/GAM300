using Boom;

using System;

/// <summary>
/// Handles player movement using PhysX linear velocity control.
/// Supports WASD movement, jumping with Space, and mouse-look control.
/// Movement is relative to the camera's facing direction.
/// Attach this script to any entity with TransformComponent and ScriptComponent.
/// </summary>
namespace GameScripts
{
    public class PlayerMovement
    {
        // This field is automatically set by the scripting system
        public ulong Entity;

        // Movement parameters (configurable)
        private float _speed = 5f;
      

        // Footstep system
        private FootstepComponent _footstepComponent;

        // Jump state tracking
        private bool _wasSpacePressed = false;
        private bool _hasJumped = false;

        // Static reference to player entity for trigger callbacks
        private static ulong s_playerEntity = 0;

        // NEW: Rotation smoothing
        //private float _rotationSpeed = 10f; // degrees per frame to turn

        // NEW: Model forward direction offset (adjust if model faces wrong way)
        private float _modelForwardOffset = 180f; // 180° if model faces backwards, 0° if correct

        /// <summary>
        /// Called once when the script is first created.
        /// </summary>
        public void OnStart(string jsonParams)
        {
            API.Log($"[PlayerMovement] OnStart() - Entity: {Entity}");

            // Store player entity reference for static callbacks
            s_playerEntity = Entity;

            // Validate entity has required components
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

            // Initialize footstep component
            _footstepComponent = new FootstepComponent();
            _footstepComponent.Entity = Entity;
            _footstepComponent.OnStart("");

            // RE-ENABLE trigger callbacks (simple version)
            API.Log("[PlayerMovement] Registering trigger callbacks...");
            RegisterTriggerCallbacksOnAllTriggers();

            API.Log($"[PlayerMovement] Using camera-relative PhysX movement: speed={_speed}, jumpSpeed={_jumpSpeed}");
            API.Log($"[PlayerMovement] Model forward offset: {_modelForwardOffset} degrees");
        }

        private void RegisterTriggerCallbacksOnAllTriggers()
        {
            // Try to find common trigger entities and register callbacks on them
            string[] triggerNames = { "Checkpoint", "DamageZone", "PowerUp", "DoorTrigger", "TriggerVolume", "AreaTrigger" };

            int registeredCount = 0;
            foreach (string triggerName in triggerNames)
            {
                ulong triggerEntity = API.FindEntity(triggerName);
                API.Log($"[PlayerMovement] Looking for trigger: {triggerName}, found ID: {triggerEntity}");

                if (triggerEntity != 0)
                {
                    // Validate it has a collider and is a trigger
                    bool hasCollider = API.HasCollider(triggerEntity);
                    bool isTrigger = API.IsTrigger(triggerEntity);

                    API.Log($"[PlayerMovement] {triggerName} - HasCollider: {hasCollider}, IsTrigger: {isTrigger}");

                    if (hasCollider && isTrigger)
                    {
                        API.Log($"[PlayerMovement] Registering callbacks on {triggerName} (ID: {triggerEntity})");
                        API.RegisterTriggerEnterCallback(triggerEntity, OnTriggerEnter);
                        API.RegisterTriggerExitCallback(triggerEntity, OnTriggerExit);
                        registeredCount++;
                        API.Log($"[PlayerMovement] Successfully registered callbacks on {triggerName}");
                    }
                    else
                    {
                        API.Log($"[PlayerMovement] Skipping {triggerName} - not a proper trigger");
                    }
                }
                else
                {
                    API.Log($"[PlayerMovement] Trigger '{triggerName}' not found in scene");
                }
            }

            API.Log($"[PlayerMovement] Total callbacks registered: {registeredCount}");
        }

        /// <summary>
        /// Called every frame to update movement using PhysX linear velocity.
        /// </summary>
        public void OnUpdate(float dt)
        {
            // Safety check
            if (!API.HasTransform(Entity) || !API.HasScript(Entity))
                return;

            // Update footstep component
            _footstepComponent?.OnUpdate(dt);

            // =============== CAMERA-RELATIVE PHYSX MOVEMENT =================

            var vel = API.GetLinearVelocity(Entity);

            // Check if the player is allowed to move (RMB held disables movement)
            bool allowMove = !API.IsMouseDown(API.MOUSE_RIGHT);

            // Check if the player is "grounded" via collision flag
            bool isGrounded = API.IsColliding(Entity);

            // Calculate horizontal movement input
            float inputX = 0f, inputZ = 0f;
            if (allowMove)
            {
                if (API.IsKeyDown(API.KEY_A)) inputX += 1f; // Left
                if (API.IsKeyDown(API.KEY_D)) inputX -= 1f; // Right
                if (API.IsKeyDown(API.KEY_W)) inputZ += 1f; // Forward
                if (API.IsKeyDown(API.KEY_S)) inputZ -= 1f; // Backward
            }

            // Apply camera-relative movement
            if (inputX != 0f || inputZ != 0f)
            {
                // Get camera's yaw angle in degrees
                float cameraYawDegrees = API.GetThirdPersonCameraYaw();

                // Convert to radians for math calculations
                float cameraYawRadians = cameraYawDegrees * (float)Math.PI / 180f;

                // Calculate camera's forward and right vectors
                // Forward direction: where the camera is looking (negative Z in camera space becomes world space direction)
                Vec3 cameraForward = new Vec3(
                    (float)Math.Sin(cameraYawRadians),
                    0f,
                    (float)Math.Cos(cameraYawRadians)
                );

                // Right direction: 90 degrees to the right of forward
                Vec3 cameraRight = new Vec3(
                    (float)Math.Cos(cameraYawRadians),
                    0f,
                    -(float)Math.Sin(cameraYawRadians)
                );

                // Calculate world-space movement direction based on camera orientation
                Vec3 moveDirection = new Vec3(
                    cameraRight.X * inputX + cameraForward.X * inputZ,
                    0f, // Keep Y at 0 for ground movement
                    cameraRight.Z * inputX + cameraForward.Z * inputZ
                );

                // Normalize and apply speed
                float len = (float)Math.Sqrt(moveDirection.X * moveDirection.X + moveDirection.Z * moveDirection.Z);
                if (len > 0f)
                {
                    vel.X = (moveDirection.X / len) * _speed;
                    vel.Z = (moveDirection.Z / len) * _speed;

                    // NEW: Calculate rotation with model forward offset correction
                    float targetYaw = (float)(Math.Atan2(moveDirection.X, moveDirection.Z) * 180.0 / Math.PI);

                    // Apply the offset to correct for model's wrong forward direction
                    targetYaw += _modelForwardOffset;

                    // Normalize angle to [-180, 180] range
                    while (targetYaw > 180f) targetYaw -= 360f;
                    while (targetYaw < -180f) targetYaw += 360f;

                    API.SetRotationY(Entity, targetYaw);
                }
            }
            else
            {
                // No input - stop horizontal movement
                vel.X = 0f;
                vel.Z = 0f;
            }

            // =============== JUMPING LOGIC WITH PROPER STATE TRACKING =================

            bool spacePressed = API.IsKeyDown(API.KEY_SPACE);

            // Reset jump state when we land
            if (isGrounded && _hasJumped)
            {
                _hasJumped = false;
            }

            // Jump only on space key press (not hold) and when grounded
            if (allowMove && isGrounded && spacePressed && !_wasSpacePressed && !_hasJumped)
            {
                vel.Y = _jumpSpeed;
                _hasJumped = true;

                // Play jump sound only once
                Vec3 playerPos = API.GetPosition(Entity);
                API.PlaySoundAt("jump_sound", "Resources/Audio/playerPunch_1.wav", playerPos, false);
                API.SetSoundVolume("jump_sound", 0.9f);

                API.Log("[PlayerMovement] Jump executed!");
            }

            // Update space key state for next frame
            _wasSpacePressed = spacePressed;

            // NOTE: We do NOT apply gravity here.
            // PhysX already applies gravity every simulation step.

            API.SetLinearVelocity(Entity, vel);
        }

        /// <summary>
        /// Trigger enter callback - called when the player enters a trigger volume
        /// </summary>
        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                API.Log($"[PlayerMovement] Trigger Enter - Trigger: {triggerEntity}, Other: {otherEntity}, Player: {s_playerEntity}");

                // Check if the "other" entity is actually the player
                if (otherEntity != s_playerEntity)
                {
                    API.Log($"[PlayerMovement] Trigger not involving player - ignoring");
                    return;
                }

                API.Log($"[PlayerMovement] Player entered trigger! Trigger ID: {triggerEntity}");

                // Get the position for 3D sound
                Vec3 triggerPos = new Vec3(0, 0, 0);
                if (API.HasTransform(triggerEntity))
                {
                    triggerPos = API.GetPosition(triggerEntity);
                }

                // Try to identify trigger by name lookup
                // Note: This approach searches for known trigger names
                bool soundPlayed = false;

                // Try common trigger names
                ulong checkpoint = API.FindEntity("Checkpoint");
                ulong damageZone = API.FindEntity("DamageZone");
                ulong powerup = API.FindEntity("PowerUp");
                ulong door = API.FindEntity("DoorTrigger");

                if (triggerEntity == checkpoint && checkpoint != 0)
                {
                    API.Log(">>> CHECKPOINT REACHED! <<<");
                    API.PlaySoundAt("checkpoint", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("checkpoint", 0.95f);
                    soundPlayed = true;
                }
                else if (triggerEntity == damageZone && damageZone != 0)
                {
                    API.Log(">>> PLAYER TAKING DAMAGE! <<<");
                    API.PlaySoundAt("damage", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("damage", 0.8f);
                    soundPlayed = true;
                }
                else if (triggerEntity == powerup && powerup != 0)
                {
                    API.Log(">>> POWER-UP COLLECTED! <<<");
                    API.PlaySoundAt("powerup", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("powerup", 0.9f);
                    soundPlayed = true;
                }
                else if (triggerEntity == door && door != 0)
                {
                    API.Log(">>> DOOR ACTIVATED! <<<");
                    API.PlaySoundAt("door_open", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("door_open", 0.85f);
                    soundPlayed = true;
                }

                // If no specific trigger was found, play a generic trigger sound
                if (!soundPlayed)
                {
                    API.Log($"[PlayerMovement] Generic trigger entered with entity ID: {triggerEntity}");
                    API.PlaySoundAt("generic_trigger", "Resources/Audio/playerPunch_1.wav", triggerPos, false);
                    API.SetSoundVolume("generic_trigger", 0.7f);
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] ERROR in OnTriggerEnter: {ex.Message}");
            }
        }

        /// <summary>
        /// Trigger exit callback - called when the player exits a trigger volume
        /// </summary>
        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            try
            {
                API.Log($"[PlayerMovement] Trigger Exit - Trigger: {triggerEntity}, Other: {otherEntity}, Player: {s_playerEntity}");

                // Check if the "other" entity is actually the player
                if (otherEntity != s_playerEntity)
                {
                    API.Log($"[PlayerMovement] Trigger exit not involving player - ignoring");
                    return;
                }

                API.Log($"[PlayerMovement] Player exited trigger! Trigger ID: {triggerEntity}");

                // Find the trigger entity's name for more specific handling
                ulong damageZone = API.FindEntity("DamageZone");
                ulong door = API.FindEntity("DoorTrigger");

                // Handle trigger exit events
                if (triggerEntity == damageZone && damageZone != 0)
                {
                    API.Log(">>> PLAYER SAFE FROM DAMAGE! <<<");
                    API.StopSound("damage");
                }
                else if (triggerEntity == door && door != 0)
                {
                    API.Log(">>> PLAYER LEFT DOOR AREA <<<");
                    // Get door position for 3D sound
                    Vec3 doorPos = new Vec3(0, 0, 0);
                    if (API.HasTransform(triggerEntity))
                    {
                        doorPos = API.GetPosition(triggerEntity);
                    }
                    API.PlaySoundAt("door_close", "Resources/Audio/playerPunch_1.wav", doorPos, false);
                    API.SetSoundVolume("door_close", 0.75f);
                }
                else
                {
                    API.Log($"[PlayerMovement] Exited unknown trigger with entity ID: {triggerEntity}");
                }
            }
            catch (Exception ex)
            {
                API.Log($"[PlayerMovement] ERROR in OnTriggerExit: {ex.Message}");
            }
        }

        /// <summary>
        /// Called when the script is destroyed (optional cleanup).
        /// </summary>
        public void OnDestroy()
        {
            API.Log($"[PlayerMovement] OnDestroy() - Entity: {Entity}");

            // Clear static reference
            if (s_playerEntity == Entity)
            {
                s_playerEntity = 0;
            }

            // Cleanup footstep component
            _footstepComponent?.OnDestroy();

            // Unregister trigger callbacks to prevent memory leaks
            if (API.HasCollider(Entity))
            {
                API.UnregisterTriggerCallbacks(Entity);
                API.Log("[PlayerMovement] Unregistered trigger callbacks for player");
            }
        }

        // Public methods for customization
        public void SetFootstepVolume(float volume)
        {
            _footstepComponent?.SetFootstepVolume(volume);
        }

        public void SetFootstepInterval(float interval)
        {
            _footstepComponent?.SetFootstepInterval(interval);
        }

        /// <summary>
        /// Set the model forward direction offset if your model faces the wrong way
        /// Common values: 0° (correct), 180° (backwards), 90° (right), -90° (left)
        /// </summary>
        public void SetModelForwardOffset(float degrees)
        {
            _modelForwardOffset = degrees;
            API.Log($"[PlayerMovement] Model forward offset set to: {_modelForwardOffset} degrees");
        }
    }
}