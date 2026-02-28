using System;
using Boom;

namespace GameScripts
{
    // FallingObject: placed above the player path. Starts falling when player walks toward it.
    public class FallingObject
    {
        public ulong Entity;

        [Boom.EditorExposed("Trigger Radius", "Player distance to start checking for fall", 0.5f, 50f, true)]
        private float _triggerRadius = 6.0f;

        [Boom.EditorExposed("Fall Speed", "Initial downward velocity when triggered", 0.1f, 50f, true)]
        private float _fallSpeed = 6.0f;

        [Boom.EditorExposed("Gravity Multiplier", "Extra gravity applied while falling", 0f, 50f, true)]
        private float _gravity = 9.8f;

        // Only trigger once
        private bool _hasFallen = false;
        private bool _isFalling = false;
        private bool _hasHit = false;

        // Track previous horizontal distance to player to detect approach (distance decreasing)
        private float _lastHorizontalDistance = float.MaxValue;

        // Small delay before enabling physics (makes object stable atop a parent)
        private float _enableTimer = 0f;
        private const float ENABLE_DELAY = 0.05f;

        public void OnStart(string json)
        {
            _hasFallen = false;
            _lastHorizontalDistance = float.MaxValue;
            _enableTimer = 0f;

            // Make sure entity exists and has transform
            if (!API.HasTransform(Entity)) return;

            // Ensure the object is not affected by physics until triggered (engine-specific)
            // We rely on SetLinearVelocity to move it when falling.
        }

        public void OnUpdate(float dt)
        {
            if (_hasFallen) return;

            // If currently falling, handle collision detection
            if (_isFalling)
            {
                if (_hasHit) return;

                // Poll collision state
                if (API.IsColliding(Entity))
                {
                    _hasHit = true;

                    // Determine if player is under the platform
                    ulong playerEntity = PlayerMovement.GetPlayerEntity();
                    if (playerEntity != 0 && API.HasTransform(playerEntity))
                    {
                        Vec3 objPosNow = API.GetPosition(Entity);
                        Vec3 playerPosNow = API.GetPosition(playerEntity);
                        float dxp = playerPosNow.X - objPosNow.X;
                        float dzp = playerPosNow.Z - objPosNow.Z;
                        float horiz = (float)System.Math.Sqrt(dxp * dxp + dzp * dzp);

                        // If player is directly underneath (within small radius) and below Y, consider hit
                        if (horiz <= 1.5f && playerPosNow.Y < objPosNow.Y)
                        {
                            // Kill player
                            API.Log("[FallingObject] Platform hit player - triggering death");
                            Entry.TriggerPlayerDeath();
                            // Destroy platform
                            API.DestroyEntity(Entity);
                            return;
                        }
                    }

                    // Otherwise, break on ground
                    API.DestroyEntity(Entity);
                    return;
                }

                return;
            }

            _enableTimer += dt;
            if (_enableTimer < ENABLE_DELAY) return;

            ulong player = PlayerMovement.GetPlayerEntity();
            if (player == 0) return;

            Vec3 objPos = API.GetPosition(Entity);
            Vec3 playerPos = API.GetPosition(player);

            // Horizontal distance (X,Z) only
            float dx = playerPos.X - objPos.X;
            float dz = playerPos.Z - objPos.Z;
            float horizDist = (float)Math.Sqrt(dx * dx + dz * dz);

            // Use prediction similar to Unity version: compute vertical distance and predicted player travel
            Vec3 playerVel = API.GetLinearVelocity(player);

            float verticalDistance = objPos.Y - playerPos.Y;
            if (verticalDistance <= 0f)
            {
                // Player at or above platform - ignore
                _lastHorizontalDistance = horizDist;
                return;
            }

            // Estimate fall time using gravity constant from PlayerMovement (approx)
            // Use engine's gravity approximation (PlayerMovement uses GRAVITY constant)
            float playerGravity = 50f; // fallback
            try { playerGravity = 50f; } catch { }

            float tFall = (float)System.Math.Sqrt(2f * verticalDistance / playerGravity);

            // Player forward speed - query PlayerMovement
            float forwardSpeed = PlayerMovement.GetCurrentMoveSpeed();
            float predictedDistance = forwardSpeed * tFall;

            // Trigger if player is within predicted distance plus threshold
            if (horizDist <= predictedDistance + _triggerRadius - 0.1f)
            {
                // schedule fall with slight delay to 'smash' player
                _hasFallen = true;
                float delay = 0.1f;
                // start countdown to fall
                _enableTimer = -delay;
                return;
            }

            _lastHorizontalDistance = horizDist;
        }

        private void TriggerFall()
        {
            _hasFallen = true;
            _isFalling = true;
            FallNow();
        }

        private void FallNow()
        {
            // Give a downward velocity; if the engine uses gravity, velocity will be integrated.
            Vec3 vel = API.GetLinearVelocity(Entity);
            vel.Y = -Math.Abs(_fallSpeed);
            API.SetLinearVelocity(Entity, vel);

            // Ensure this object collides
            if (API.HasCollider(Entity))
            {
                API.SetTrigger(Entity, false);
            }

            API.PlaySoundAt("falling_obj_" + Entity, "Resources/Audio/rock_fall.wav", API.GetPosition(Entity), false);
            API.SetSoundVolume("falling_obj_" + Entity, 1.0f);
            API.Set3DMinMaxDistance("falling_obj_" + Entity, 1.0f, 40.0f);
        }

        public void OnDestroy()
        {
            // cleanup if needed
        }
    }
}
