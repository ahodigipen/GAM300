using System;
using Boom;

namespace GameScripts
{
    // FallingObject: placed above the player path. Starts falling when player walks toward it.
    public class FallingObject
    {
        public ulong Entity;

        [Boom.EditorExposed("Trigger Radius", "Player distance to start checking for fall", 0.5f, 50f, true)]
        private float _triggerRadius = 2.0f;
        [Boom.EditorExposed("Debug: Force Fall In Radius", "If true, platform will fall immediately when player enters radius (for testing)")]
        private bool _debugForceFallInRadius = false;

        [Boom.EditorExposed("Fall Speed", "Initial downward velocity when triggered", 0.1f, 50f, true)]
        private float _fallSpeed = 3.0f;

        [Boom.EditorExposed("Gravity Multiplier", "Extra gravity applied while falling", 0f, 50f, true)]
        private float _gravity = 9.8f;

        // Only trigger once
        private bool _hasFallen = false;
        private bool _isFalling = false;
        private bool _hasHit = false;
        private float _fallVelocity = 0f;
        private Vec3 _stoppedPosition;

        // Track previous horizontal distance to player to detect approach (distance decreasing)
        private float _lastHorizontalDistance = float.MaxValue;

        // Small delay before enabling physics (makes object stable atop a parent)
        private float _enableTimer = 0f;
        private const float ENABLE_DELAY = 0.05f;
        // Keep initial position locked while armed so dynamic bodies don't fall prematurely
        private Vec3 _initialPosition;
        private bool _armed = true;

        public void OnStart(string json)
        {
            _hasFallen = false;
            _lastHorizontalDistance = float.MaxValue;
            _enableTimer = 0f;

            // Make sure entity exists and has transform
            if (!API.HasTransform(Entity)) return;

            API.Log($"[FallingObject] OnStart Entity={Entity}, triggerRadius={_triggerRadius}, fallSpeed={_fallSpeed}");

            // Keep the collider as a trigger initially so the dynamic rigidbody does not
            // immediately fall under gravity. We'll clear the trigger when the object
            // should start falling.
            if (API.HasCollider(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log($"[FallingObject] Collider set to trigger=true on start for Entity={Entity}");

                // Ensure the body has no initial velocity
                Vec3 zero = new Vec3(0f, 0f, 0f);
                API.SetLinearVelocity(Entity, zero);
            }

            // Record initial position so we can keep the object locked in place until triggered.
            if (API.HasTransform(Entity))
            {
                _initialPosition = API.GetPosition(Entity);
            }

            // Ensure the object is not affected by physics until triggered (engine-specific)
            // We rely on SetLinearVelocity to move it when falling.
        }

        public void OnUpdate(float dt)
        {
            API.Log($"[FallingObject] OnUpdate E={Entity} isFalling={_isFalling} hasFallen={_hasFallen} enableTimer={_enableTimer:F3}");

            // If currently falling, perform kinematic step-by-step teleport to avoid tunneling
            if (_isFalling)
            {
                if (_hasHit) return;

                // Use a scripted kinematic fall: increment velocity by gravity and teleport by small steps
                if (!API.HasTransform(Entity)) return;

                Vec3 pos = API.GetPosition(Entity);
                float prevY = pos.Y;

                // Initialize fall velocity if needed
                if (_fallVelocity == 0f)
                {
                    _fallVelocity = Math.Abs(_fallSpeed);
                }

                // Integrate simple gravity
                _fallVelocity += _gravity * dt;
                float deltaY = _fallVelocity * dt;

                // Move down by deltaY
                pos.Y -= deltaY;

                // Teleport actor to new position (updates PhysX actor directly)
                API.TeleportRigidBody(Entity, pos);

                // Additional immediate check: the teleported object may overlap the player's
                // controller without the rigidbody reporting a collision. Check player position
                // each step and trigger death if the player is directly underneath the object.
                ulong pEntity = PlayerMovement.GetPlayerEntity();
                if (pEntity != 0 && API.HasTransform(pEntity) && API.HasTransform(Entity))
                {
                    Vec3 objPosNow = API.GetPosition(Entity);
                    Vec3 playerPosNow = API.GetPosition(pEntity);
                    float dxp = playerPosNow.X - objPosNow.X;
                    float dzp = playerPosNow.Z - objPosNow.Z;
                    float horiz = (float)System.Math.Sqrt(dxp * dxp + dzp * dzp);

                    Vec3 scale = API.GetScale(Entity);
                    float killRadius = Math.Max(scale.X, scale.Z) * 0.5f + 0.8f; // platform half-width + player radius

                    // If player is directly underneath (within platform bounds) and below Y, consider hit
                    if (horiz <= killRadius && playerPosNow.Y < objPosNow.Y + (scale.Y * 0.5f) + 1.0f)
                    {
                        API.Log("[FallingObject] Platform directly hit player during fall - triggering death");
                        PlayerManager.GetPlayer()?.OnCaughtByEnemy(Entity);

                        // Stop motion so the platform stays on the floor / doesn't continue teleporting
                        Vec3 stop = new Vec3(0f, 0f, 0f);
                        API.SetLinearVelocity(Entity, stop);
                        _hasHit = true;
                        _isFalling = false;
                        return;
                    }
                }

                // Check collision after move
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

                        Vec3 scale = API.GetScale(Entity);
                        float killRadius = Math.Max(scale.X, scale.Z) * 0.5f + 0.8f; // platform half-width + player radius

                        // If player is directly underneath (within platform bounds) and below Y, consider hit
                        if (horiz <= killRadius && playerPosNow.Y < objPosNow.Y + (scale.Y * 0.5f) + 1.0f)
                        {
                            API.Log("[FallingObject] Platform hit player - triggering death (platform remains)");
                            PlayerManager.GetPlayer()?.OnCaughtByEnemy(Entity);

                            // Stop motion so the platform stays on the floor
                            Vec3 stop = new Vec3(0f, 0f, 0f);
                            API.SetLinearVelocity(Entity, stop);
                            _isFalling = false;
                            return;
                        }
                    }

                    // Hit ground or anything else - snap back to previous non-penetrating position and stop
                    API.Log("[FallingObject] Platform collided with environment - stopping and remaining");
                    Vec3 snap = API.GetPosition(Entity);
                    snap.Y = prevY; // place on last safe position
                    API.TeleportRigidBody(Entity, snap);
                    // Store stopped position to prevent later phasing when players interact
                    _stoppedPosition = snap;
                    Vec3 stop2 = new Vec3(0f, 0f, 0f);
                    API.SetLinearVelocity(Entity, stop2);
                    _isFalling = false;
                    return;
                }

                return;
            }

            // If we have a scheduled fall countdown (negative timer), count up to zero then fall
            if (_enableTimer < 0f)
            {
                _enableTimer += dt;
                if (_enableTimer >= 0f)
                {
                    _isFalling = true;
                    _armed = false;
                    FallNow();
                }
                return;
            }

            // While armed (not yet triggered to fall), lock the object to its initial position
            if (_armed && !_isFalling)
            {
                // Ensure collider is trigger and zero velocity
                if (API.HasCollider(Entity) && !API.IsTrigger(Entity))
                {
                    API.SetTrigger(Entity, true);
                }
                // Zero velocity
                Vec3 v = API.GetLinearVelocity(Entity);
                if (v.X != 0f || v.Y != 0f || v.Z != 0f)
                {
                    Vec3 zero = new Vec3(0f, 0f, 0f);
                    API.SetLinearVelocity(Entity, zero);
                }
                // Snap back to initial position in case physics moved it
                if (API.HasTransform(Entity))
                {
                    API.SetPosition(Entity, _initialPosition);
                }
            }

            // If the platform has already come to rest on the ground, keep it locked to the stopped position
            if (_hasHit && !_isFalling)
            {
                // Ensure collider remains a simulation shape and zero velocity
                if (API.HasCollider(Entity) && API.IsTrigger(Entity))
                    API.SetTrigger(Entity, false);

                Vec3 zero = new Vec3(0f, 0f, 0f);
                API.SetLinearVelocity(Entity, zero);
                if (API.HasTransform(Entity))
                {
                    API.SetPosition(Entity, _stoppedPosition);
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

            // Player forward speed - prefer actual physics velocity, fallback to PlayerMovement reported speed
            float forwardSpeed = (float)System.Math.Sqrt(playerVel.X * playerVel.X + playerVel.Z * playerVel.Z);
            if (forwardSpeed < 0.01f)
            {
                forwardSpeed = PlayerMovement.GetCurrentMoveSpeed();
            }

            float predictedDistance = forwardSpeed * tFall;

            API.Log($"[FallingObject] horizDist={horizDist:F2} forwardSpeed={forwardSpeed:F2} tFall={tFall:F2} predictedDistance={predictedDistance:F2} lastHoriz={_lastHorizontalDistance:F2}");

            // Debug: force fall if within radius
            if (_debugForceFallInRadius && horizDist <= _triggerRadius)
            {
                API.Log("[FallingObject] Debug force-trigger: player within radius, scheduling fall");
                _hasFallen = true;
                _enableTimer = -0.05f;
                return;
            }

            // Trigger if player is within predicted distance plus threshold OR simple proximity approach
            if (horizDist <= predictedDistance + _triggerRadius - 0.1f || (horizDist <= _triggerRadius && horizDist < _lastHorizontalDistance))
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
            // If we've already hit the ground or already fell, ignore further triggers
            if (_hasHit || _hasFallen) return;

            _hasFallen = true;
            _isFalling = true;
            FallNow();
        }

        private void FallNow()
        {
            // Ensure this object collides (only toggle if currently a trigger).
            if (API.HasCollider(Entity) && API.IsTrigger(Entity))
            {
                API.Log($"[FallingObject] Clearing trigger on Entity={Entity} before falling");
                API.SetTrigger(Entity, false);
            }
            // After enabling simulation, teleport the actor to its current transform to
            // ensure the physics actor is aligned with the visual transform and avoid
            // initial tunneling when applying a downward velocity.
            if (API.HasTransform(Entity))
            {
                Vec3 pos = API.GetPosition(Entity);
                // Larger upward nudge to avoid starting embedded in geometry (tunable)
                pos.Y += 0.05f;
                // Clear any existing velocity before teleport to avoid momentum carry-over
                Vec3 zero = new Vec3(0f, 0f, 0f);
                API.SetLinearVelocity(Entity, zero);
                API.TeleportRigidBody(Entity, pos);
            }

            // Let the physics engine apply gravity naturally. We avoid forcing a constant
            // downward velocity here so PhysX gravity and CCD (enabled engine-side)
            // can handle the fall and collision properly.
            API.Log($"[FallingObject] FallNow: letting physics gravity handle fall for Entity={Entity}");
        }

        public void OnDestroy()
        {
            // cleanup if needed
        }
    }
}
