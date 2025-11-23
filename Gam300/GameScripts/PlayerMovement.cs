using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Handles player movement using PhysX linear velocity control.
    /// Supports WASD movement, jumping with Space, and mouse-look control.
    /// Movement is relative to the camera's facing direction.
    /// Attach this script to any entity with TransformComponent and ScriptComponent.
    /// </summary>
    public class PlayerMovement
    {
        // This field is automatically set by the scripting system
        public ulong Entity;

        // Movement parameters (configurable)
        private float _speed = 5f;
        private float _jumpSpeed = 8f;

        /// <summary>
        /// Called once when the script is first created.
        /// </summary>
        public void OnStart(string jsonParams)
        {
            API.Log($"[PlayerMovement] OnStart() - Entity: {Entity}");

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

            // Register trigger callbacks if this entity has a collider
            if (API.HasCollider(Entity))
            {
                API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
                API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
                API.Log("[PlayerMovement] Registered trigger callbacks for player");
            }

            API.Log($"[PlayerMovement] Using camera-relative PhysX movement: speed={_speed}, jumpSpeed={_jumpSpeed}");
        }

        /// <summary>
        /// Called every frame to update movement using PhysX linear velocity.
        /// </summary>
        public void OnUpdate(float dt)
        {
            // Safety check
            if (!API.HasTransform(Entity) || !API.HasScript(Entity))
                return;

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
                }
            }
            else
            {
                // No input - stop horizontal movement
                vel.X = 0f;
                vel.Z = 0f;
            }

            // Apply vertical velocity (Jumping)
            if (allowMove && isGrounded && API.IsKeyDown(API.KEY_SPACE))
            {
                vel.Y = _jumpSpeed;
            }
            // NOTE: We do NOT apply gravity here.
            // PhysX already applies gravity every simulation step.

            API.SetLinearVelocity(Entity, vel);
        }

        /// <summary>
        /// Trigger enter callback - called when the player enters a trigger volume
        /// </summary>
        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            API.Log($"[PlayerMovement] Player entered trigger! Trigger: {triggerEntity}, Other: {otherEntity}");

            // Find the trigger entity's name for more specific handling
            ulong checkpoint = API.FindEntity("Checkpoint");
            ulong damageZone = API.FindEntity("DamageZone");
            ulong powerup = API.FindEntity("PowerUp");
            ulong door = API.FindEntity("DoorTrigger");

            // Test different trigger types
            if (triggerEntity == checkpoint)
            {
                API.Log(">>> CHECKPOINT REACHED! <<<");
                // TODO: Save game state, play sound effect, show UI feedback
            }
            else if (triggerEntity == damageZone)
            {
                API.Log(">>> PLAYER TAKING DAMAGE! <<<");
                // TODO: Apply damage, screen effect, health reduction
            }
            else if (triggerEntity == powerup)
            {
                API.Log(">>> POWER-UP COLLECTED! <<<");
                // TODO: Apply power-up effect, destroy trigger entity, play sound
            }
            else if (triggerEntity == door)
            {
                API.Log(">>> DOOR ACTIVATED! <<<");
                // TODO: Open door, play animation, unlock path
            }
            else
            {
                API.Log($"[PlayerMovement] Unknown trigger type with entity ID: {triggerEntity}");
            }
        }

        /// <summary>
        /// Trigger exit callback - called when the player exits a trigger volume
        /// </summary>
        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            API.Log($"[PlayerMovement] Player exited trigger! Trigger: {triggerEntity}, Other: {otherEntity}");

            // Find the trigger entity's name for more specific handling
            ulong damageZone = API.FindEntity("DamageZone");
            ulong door = API.FindEntity("DoorTrigger");

            // Handle trigger exit events
            if (triggerEntity == damageZone)
            {
                API.Log(">>> PLAYER SAFE FROM DAMAGE! <<<");
                // TODO: Stop damage over time effects, remove screen effects
            }
            else if (triggerEntity == door)
            {
                API.Log(">>> PLAYER LEFT DOOR AREA <<<");
                // TODO: Close door after delay, or keep it open
            }
            else
            {
                API.Log($"[PlayerMovement] Exited unknown trigger with entity ID: {triggerEntity}");
            }
        }

        /// <summary>
        /// Called when the script is destroyed (optional cleanup).
        /// </summary>
        public void OnDestroy()
        {
            API.Log($"[PlayerMovement] OnDestroy() - Entity: {Entity}");

            // Unregister trigger callbacks to prevent memory leaks
            if (API.HasCollider(Entity))
            {
                API.UnregisterTriggerCallbacks(Entity);
                API.Log("[PlayerMovement] Unregistered trigger callbacks for player");
            }
        }
    }
}