using System;
using Boom;

namespace GameScripts
{
    public class PlayerMovement
    {
        // === Movement Settings ===
        private float _moveSpeed = 6.0f;
        private float _jumpSpeed = 8.0f;
        private bool _gateRmbToPauseMove = true;

        public ulong Entity;

        // === State Tracking ===
        private bool _wasSpaceDown = false;
        private string _currentAnimation = "";

        public void OnStart(string jsonParams)
        {
            if (!API.HasTransform(Entity))
            {
                API.Log("[PlayerMovement] Entity has no TransformComponent.");
                return;
            }

            // Verify animator exists
            if (!API.HasAnimator(Entity))
            {
                API.Log("[PlayerMovement] WARNING: Entity has no AnimatorComponent!");
                return;
            }

            // Start with walking animation (idle state)
            PlayAnimation("Walking");

            API.Log("[PlayerMovement] Initialized - Ready to animate!");
        }

        public void OnUpdate(float dt)
        {
            // Check if movement is paused by right mouse button
            if (_gateRmbToPauseMove && API.IsMouseDown(API.MOUSE_RIGHT))
                return;

            // === 1. GET MOVEMENT INPUT ===
            float dx = 0f, dz = 0f;
            if (API.IsKeyDown(API.KEY_A)) dx -= 1f;
            if (API.IsKeyDown(API.KEY_D)) dx += 1f;
            if (API.IsKeyDown(API.KEY_W)) dz -= 1f;
            if (API.IsKeyDown(API.KEY_S)) dz += 1f;

            // Normalize diagonal movement
            float inputLen = (float)Math.Sqrt(dx * dx + dz * dz);
            if (inputLen > 0f)
            {
                dx /= inputLen;
                dz /= inputLen;
            }

            // === 2. UPDATE PHYSICS VELOCITY ===
            var vel = API.GetLinearVelocity(Entity);

            // Set horizontal velocity based on input
            vel.X = dx * _moveSpeed;
            vel.Z = dz * _moveSpeed;

            // === 3. HANDLE JUMPING ===
            bool isGrounded = API.IsColliding(Entity);
            bool spaceDown = API.IsKeyDown(API.KEY_SPACE);

            // Jump on space press (only when grounded)
            if (spaceDown && !_wasSpaceDown && isGrounded)
            {
                vel.Y = _jumpSpeed;
                PlayAnimation("Jumping");
            }

            _wasSpaceDown = spaceDown;

            // Apply velocity
            API.SetLinearVelocity(Entity, vel);

            // === 4. PLAY ANIMATIONS BASED ON MOVEMENT ===
            if (API.HasAnimator(Entity))
            {
                // Only change animation if grounded (don't interrupt jump)
                if (isGrounded)
                {
                    if (inputLen > 0.1f)
                    {
                        // Moving - play run animation
                        PlayAnimation("run");
                    }
                    else
                    {
                        // Standing still - play walking animation as idle
                        PlayAnimation("Walking");
                    }
                }
            }

            // === 5. ROTATE CHARACTER TO FACE MOVEMENT DIRECTION ===
            if (inputLen > 0.1f && API.HasTransform(Entity))
            {
                // Calculate target rotation angle
                float targetAngle = (float)Math.Atan2(dx, dz);
                float targetDegrees = targetAngle * (180.0f / (float)Math.PI);

                // Get current transform
                var transform = API.GetTransform(Entity);

                // Instant rotation
                transform.RotationY = targetDegrees;

                // Apply rotation
                API.SetTransform(Entity, transform);
            }
        }

        public void OnDestroy()
        {
            // Cleanup if needed
        }

        // === HELPER METHOD ===
        private void PlayAnimation(string animationName)
        {
            // Only switch if it's a different animation
            if (_currentAnimation != animationName)
            {
                _currentAnimation = animationName;

                if (API.HasAnimator(Entity))
                {
                    API.AnimatorPlay(Entity, animationName);
                }
            }
        }

        // === PUBLIC HELPER METHODS ===

        // Adjust movement speed at runtime
        public void SetMoveSpeed(float speed)
        {
            _moveSpeed = speed;
        }

        // Check if character is currently moving
        public bool IsMoving()
        {
            var vel = API.GetLinearVelocity(Entity);
            float planarSpeed = (float)Math.Sqrt(vel.X * vel.X + vel.Z * vel.Z);
            return planarSpeed > 0.1f;
        }

        // Check if character is grounded
        public bool IsGrounded()
        {
            return API.IsColliding(Entity);
        }
    }
}