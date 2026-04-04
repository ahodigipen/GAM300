using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// FallingObject (Maze Blocker Edition):
    /// Uses Predictive Coordinate Sweeping to ensure no clipping.
    /// Includes robust safety checks for death animations and respawn timing.
    /// </summary>
    public class FallingObject : IEnemyController
    {
        public ulong Entity;

        [Boom.EditorExposed("Trigger Radius", "Horizontal distance to start the telegraph", 0.5f, 20f)]
        private float _triggerRadius = 6.0f;

        [Boom.EditorExposed("Trigger Height Limit", "How far below the rock the player can be to trigger it", 1f, 50f)]
        private float _triggerHeightLimit = 10.0f;

        [Boom.EditorExposed("Fall Speed", "How fast the rock slams down", 1f, 200f)]
        private float _fallSpeed = 80.0f;

        [Boom.EditorExposed("Telegraph Time", "Seconds to shake before falling", 0f, 2f)]
        private float _telegraphDuration = 0.5f;

        [Boom.EditorExposed("Shake Intensity", "How much the rock shakes during telegraph", 0f, 1f)]
        private float _shakeIntensity = 0.15f;

        [Boom.EditorExposed("Min Y (Safety Floor)", "Absolute lowest Y the rock can reach", -50f, 50f)]
        private float _minY = 9.0f;

        [Boom.EditorExposed("Pivot to Bottom Offset", "Distance to bottom if pivot is center (usually 1.0)", 0.0f, 10f)]
        private float _pivotToBottomOffset = 0.0f;

        [Boom.EditorExposed("Fall Sound Volume", "Volume of the falling sound (0.0 - 1.0)")]
        private float _fallSoundVolume = 0.3f;

        private bool _isFalling = false;
        private bool _isTelegraphing = false;
        private bool _hasHitGround = false;
        private float _telegraphTimer = 0.0f;
        private float _respawnCooldown = 0.0f;
        private bool _needsReset = false;

        private Vec3 _initialPosition;
        private Vec3 _initialRotation;
        private float _currentY;

        private static readonly string[] FALL_SOUND_PATHS = new string[]
        {
            "Resources/Audio/platformFall_1.wav",
            "Resources/Audio/platformFall_2.wav",
            "Resources/Audio/platformFall_3.wav"
        };
        private static Random _rng = new Random();

        public void OnStart(string json)
        {
            if (!API.HasTransform(Entity)) return;

            _initialPosition = API.GetPosition(Entity);
            _initialRotation = API.GetRotation(Entity);
            _currentY = _initialPosition.Y;
            
            _isFalling = false;
            _isTelegraphing = false;
            _hasHitGround = false;
            _telegraphTimer = 0.0f;
            _respawnCooldown = 0.0f;
            _needsReset = false;

            PlayerManager.RegisterEnemy(this);
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Pause logic if game is paused
            if (API.GetApplicationState() == API.APP_STATE_PAUSED) return;

            // Handle forced reset if OnPlayerRespawned was called
            if (_needsReset)
            {
                PerformReset();
                return;
            }

            // Handle respawn cooldown to prevent immediate re-triggering during animations/fades
            if (_respawnCooldown > 0)
            {
                _respawnCooldown -= dt;
                return;
            }

            if (_hasHitGround) return; 

            // STATE: WAITING
            if (!_isFalling && !_isTelegraphing)
            {
                API.SetPosition(Entity, _initialPosition);
                API.SetRotation(Entity, _initialRotation);

                // SAFETY: Don't trigger if player is dead, in death anim, or respawning
                if (HUD.HealthRatio <= 0 || PlayerMovement.IsRespawning) return;

                ulong pEntity = PlayerMovement.GetPlayerEntity();
                if (pEntity != 0)
                {
                    Vec3 pPos = API.GetPosition(pEntity);
                    float dx = pPos.X - _initialPosition.X;
                    float dz = pPos.Z - _initialPosition.Z;
                    float horizontalDist = (float)Math.Sqrt(dx * dx + dz * dz);

                    // Check horizontal distance
                    if (horizontalDist < _triggerRadius)
                    {
                        float verticalDist = _initialPosition.Y - pPos.Y;
                        // Check vertical distance (must be below but within the height limit)
                        if (verticalDist > 0 && verticalDist < _triggerHeightLimit)
                        {
                            _isTelegraphing = true;
                            _telegraphTimer = _telegraphDuration;
                        }
                    }
                }
            }
            // STATE: TELEGRAPHING
            else if (_isTelegraphing)
            {
                // If player dies/respawns during telegraph, reset the rock
                if (HUD.HealthRatio <= 0 || PlayerMovement.IsRespawning)
                {
                    OnPlayerRespawned();
                    return;
                }

                _telegraphTimer -= dt;
                float shakeX = ((float)_rng.NextDouble() * 2f - 1f) * _shakeIntensity;
                float shakeZ = ((float)_rng.NextDouble() * 2f - 1f) * _shakeIntensity;
                Vec3 shakePos = _initialPosition;
                shakePos.X += shakeX;
                shakePos.Z += shakeZ;
                API.SetPosition(Entity, shakePos);

                if (_telegraphTimer <= 0)
                {
                    _isTelegraphing = false;
                    _isFalling = true;
                    string randomFallSound = FALL_SOUND_PATHS[_rng.Next(FALL_SOUND_PATHS.Length)];
                    string soundName = "RockSlam_" + Entity + "_" + DateTime.Now.Ticks;
                    API.PlaySound(soundName, randomFallSound, false);
                    API.SetSoundVolume(soundName, _fallSoundVolume);
                }
            }
            // STATE: SLAMMING
            else
            {
                float prevY = _currentY;
                float frameMovement = _fallSpeed * dt;
                float targetY = prevY - frameMovement;

                if (targetY <= _minY)
                {
                    _currentY = _minY;
                    _hasHitGround = true;
                    _isFalling = false;
                }
                else
                {
                    bool hitThisFrame = CheckFloorAtY(targetY);
                    if (hitThisFrame)
                    {
                        float high = prevY;
                        float low = targetY;
                        for(int i = 0; i < 4; i++)
                        {
                            float mid = (high + low) * 0.5f;
                            if (CheckFloorAtY(mid)) high = mid;
                            else low = mid;
                        }
                        _currentY = high; 
                        _hasHitGround = true;
                        _isFalling = false;
                    }
                    else
                    {
                        _currentY = targetY;
                    }
                }

                Vec3 nextPos = _initialPosition;
                nextPos.Y = _currentY;
                API.SetPosition(Entity, nextPos);
                API.TeleportRigidBody(Entity, nextPos);
            }
        }

        private bool CheckFloorAtY(float y)
        {
            Vec3 testPoint = _initialPosition;
            testPoint.Y = y - _pivotToBottomOffset;
            testPoint.Y += 0.02f; 
            ulong hit = API.Raycast(testPoint, new Vec3(0, -1, 0), 0.05f);
            ulong pEntity = PlayerMovement.GetPlayerEntity();
            return (hit != 0 && hit != Entity && hit != pEntity);
        }

        public void OnPlayerRespawned()
        {
            _needsReset = true;
            API.Log($"[FallingObject] {Entity} marked for reset.");
        }

        private void PerformReset()
        {
            _isFalling = false;
            _isTelegraphing = false;
            _hasHitGround = false;
            _currentY = _initialPosition.Y;
            _telegraphTimer = 0.0f;
            _respawnCooldown = 2.0f; // Long cooldown to cover animations/fades
            _needsReset = false;

            if (API.HasTransform(Entity))
            {
                API.SetPosition(Entity, _initialPosition);
                API.SetRotation(Entity, _initialRotation);
                API.TeleportRigidBody(Entity, _initialPosition);
            }
        }

        public void OnDestroy()
        {
            PlayerManager.UnregisterEnemy(this);
        }
    }
}
