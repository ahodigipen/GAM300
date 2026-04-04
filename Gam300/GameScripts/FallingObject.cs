using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// FallingObject (Maze Blocker Edition):
    /// Uses Predictive Coordinate Sweeping to ensure no clipping.
    /// </summary>
    public class FallingObject : IEnemyController
    {
        public ulong Entity;

        [Boom.EditorExposed("Trigger Radius", "Horizontal distance to start the telegraph", 0.5f, 20f)]
        private float _triggerRadius = 6.0f;

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

            PlayerManager.RegisterEnemy(this);
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;
            if (_hasHitGround) return; 

            // STATE: WAITING
            if (!_isFalling && !_isTelegraphing)
            {
                API.SetPosition(Entity, _initialPosition);
                API.SetRotation(Entity, _initialRotation);

                ulong pEntity = PlayerMovement.GetPlayerEntity();
                if (pEntity != 0)
                {
                    Vec3 pPos = API.GetPosition(pEntity);
                    float dx = pPos.X - _initialPosition.X;
                    float dz = pPos.Z - _initialPosition.Z;
                    float horizontalDist = (float)Math.Sqrt(dx * dx + dz * dz);

                    if (horizontalDist < _triggerRadius && pPos.Y < _initialPosition.Y)
                    {
                        _isTelegraphing = true;
                        _telegraphTimer = _telegraphDuration;
                    }
                }
            }
            // STATE: TELEGRAPHING
            else if (_isTelegraphing)
            {
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

                // 1. Min Y Safety
                if (targetY <= _minY)
                {
                    _currentY = _minY;
                    _hasHitGround = true;
                    _isFalling = false;
                }
                else
                {
                    // 2. COORDINATE-BASED COLLISION CHECK
                    // We check if the rock is about to pass through a floor this frame.
                    // Since we don't have hit distance, we use binary refinement to find the floor.
                    
                    bool hitThisFrame = CheckFloorAtY(targetY);

                    if (hitThisFrame)
                    {
                        // The floor is somewhere between prevY and targetY.
                        // Let's find the exact coordinate by subdividing the frame.
                        float high = prevY;
                        float low = targetY;
                        
                        // 4 iterations of binary search is usually enough for visual accuracy
                        for(int i = 0; i < 4; i++)
                        {
                            float mid = (high + low) * 0.5f;
                            if (CheckFloorAtY(mid))
                                high = mid; // Floor is higher
                            else
                                low = mid;  // Rock is still in the air
                        }
                        
                        _currentY = high; // Snap to the refined "highest point of collision"
                        _hasHitGround = true;
                        _isFalling = false;
                        API.Log($"[FallingObject] {Entity} landed at refined Y: {_currentY}");
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

        /// <summary>
        /// Checks if the bottom of the rock would be inside a floor at the given Y coordinate.
        /// </summary>
        private bool CheckFloorAtY(float y)
        {
            // We use a very short raycast (0.05 units) downwards from the point we want to test.
            // If it hits anything immediately, it means that point is "on or inside" the floor.
            Vec3 testPoint = _initialPosition;
            testPoint.Y = y - _pivotToBottomOffset;

            // Start slightly above the point to catch the floor exactly
            testPoint.Y += 0.02f; 
            
            ulong hit = API.Raycast(testPoint, new Vec3(0, -1, 0), 0.05f);
            
            ulong pEntity = PlayerMovement.GetPlayerEntity();
            return (hit != 0 && hit != Entity && hit != pEntity);
        }

        public void OnPlayerRespawned()
        {
            _isFalling = false;
            _isTelegraphing = false;
            _hasHitGround = false;
            _currentY = _initialPosition.Y;
            _telegraphTimer = 0.0f;
            
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
