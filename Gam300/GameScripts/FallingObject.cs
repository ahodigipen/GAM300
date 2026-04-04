using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Bulletproof FallingObject:
    /// 1. Rock is STATIC in the editor.
    /// 2. Uses "Frame-to-Frame" sweeping to prevent phasing.
    /// 3. If any part of the path between frames intersects the floor, it STOPS.
    /// 4. No PhysX conflicts or spinning.
    /// </summary>
    public class FallingObject : IEnemyController
    {
        public ulong Entity;

        [Boom.EditorExposed("Trigger Radius", "Horizontal distance to start the fall", 0.5f, 20f)]
        private float _triggerRadius = 4.0f;

        [Boom.EditorExposed("Fall Speed", "How fast the rock falls", 1f, 100f)]
        private float _fallSpeed = 25.0f;

        [Boom.EditorExposed("Kill Radius", "Distance to crush the player", 0.5f, 5f)]
        private float _killRadius = 2.0f;

        [Boom.EditorExposed("Min Y (Safety Floor)", "Absolute lowest Y the rock can reach", -50f, 50f)]
        private float _minY = 9.0f;

        [Boom.EditorExposed("Floor Detect Offset", "How far below center to check for floor", 0.1f, 10f)]
        private float _floorDetectOffset = 1.0f;

        [Boom.EditorExposed("Fall Sound Volume", "Volume of the falling sound (0.0 - 1.0)")]
        private float _fallSoundVolume = 0.3f;

        private bool _isFalling = false;
        private bool _hasHitGround = false;

        private Vec3 _initialPosition;
        private Vec3 _initialRotation;
        private float _currentY;

        // Fall sound
        private static readonly string[] FALL_SOUND_PATHS = new string[]
        {
            "Resources/Audio/platformFall_1.wav",
            "Resources/Audio/platformFall_2.wav",
            "Resources/Audio/platformFall_3.wav"
        };
        private static Random _fallRandom = new Random();

        public void OnStart(string json)
        {
            if (!API.HasTransform(Entity)) return;

            _initialPosition = API.GetPosition(Entity);
            _initialRotation = API.GetRotation(Entity);
            _currentY = _initialPosition.Y;
            
            _isFalling = false;
            _hasHitGround = false;

            PlayerManager.RegisterEnemy(this);
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;
            if (_hasHitGround) return; 

            // STATE: WAITING
            if (!_isFalling)
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
                        _isFalling = true;

                        // Play random fall sound
                        string randomFallSound = FALL_SOUND_PATHS[_fallRandom.Next(FALL_SOUND_PATHS.Length)];
                        string soundName = "RockFall_" + Entity + "_" + DateTime.Now.Ticks;
                        API.PlaySound(soundName, randomFallSound, false);
                        API.SetSoundVolume(soundName, _fallSoundVolume);

                        API.Log($"[FallingObject] Bulletproof Fall Started for {Entity}");
                    }
                }
            }
            // STATE: FALLING
            else
            {
                float prevY = _currentY;
                _currentY -= _fallSpeed * dt;
                
                // 1. Hard Floor Safety (MinY)
                if (_currentY <= _minY)
                {
                    _currentY = _minY;
                    _hasHitGround = true;
                    _isFalling = false;
                    API.Log($"[FallingObject] {Entity} hit Safety Floor at {_minY}.");
                }

                Vec3 nextPos = _initialPosition;
                nextPos.Y = _currentY;

                // 2. Continuous Raycast Sweep
                // We check from the center to the bottom of the movement path
                Vec3 rayStart = _initialPosition;
                rayStart.Y = prevY;
                Vec3 rayDir = new Vec3(0, -1, 0);
                
                // The distance to check is the frame movement + the offset to the rock's "feet"
                float frameMovement = prevY - _currentY;
                float sweepDist = frameMovement + _floorDetectOffset;

                ulong hitEntity = API.Raycast(rayStart, rayDir, sweepDist);
                
                // If we hit anything that isn't ourselves or the player
                ulong pEntity = PlayerMovement.GetPlayerEntity();
                if (hitEntity != 0 && hitEntity != Entity && hitEntity != pEntity)
                {
                    _hasHitGround = true;
                    _isFalling = false;
                    // Snap to where it was last frame to avoid clipping
                    nextPos.Y = prevY; 
                    API.Log($"[FallingObject] {Entity} detected floor via Raycast. Stopping.");
                }

                // 3. Kill Check
                if (pEntity != 0)
                {
                    Vec3 pPos = API.GetPosition(pEntity);
                    float dx = pPos.X - nextPos.X;
                    float dz = pPos.Z - nextPos.Z;
                    float horizDist = (float)Math.Sqrt(dx * dx + dz * dz);

                    // Crush detection: horizontally close and player is "between" the rock's movement path
                    if (horizDist < _killRadius && pPos.Y < prevY + 1.0f && pPos.Y > nextPos.Y - 1.0f)
                    {
                        API.Log($"[FallingObject] {Entity} crushed the player!");
                        PlayerManager.GetPlayer()?.OnCaughtByEnemy(Entity);
                    }
                }

                // 4. Update Final Position
                API.SetPosition(Entity, nextPos);
                API.TeleportRigidBody(Entity, nextPos);
            }
        }

        public void OnPlayerRespawned()
        {
            _isFalling = false;
            _hasHitGround = false;
            _currentY = _initialPosition.Y;
            
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
