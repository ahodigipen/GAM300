using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Boss behavior: Rotates 180 degrees every 5 seconds.
    /// If the boss is facing the player and the player is not "hiding" (crouching in a crouch zone),
    /// the player takes damage.
    /// </summary>
    public class BossHideSeekController
    {
        public ulong Entity;

        [Boom.EditorExposed("Rotation Interval", "Time in seconds between 180-degree turns")]
        private float _rotationInterval = 5.0f;

        [Boom.EditorExposed("Rotation Speed", "Degrees per second for the turn animation")]
        private float _rotationSpeed = 360.0f;

        [Boom.EditorExposed("Detection Angle", "FOV angle where the boss can 'see' the player")]
        private float _detectionAngle = 45.0f;

        [Boom.EditorExposed("Detection Range", "How far the boss can see")]
        private float _detectionRange = 50.0f;

        [Boom.EditorExposed("Invert Forward", "Check this if the boss detects you from behind instead of the front")]
        private bool _invertForward = false;

        private float _timer = 0f;
        private bool _isTurning = false;
        private float _targetYRotation = 0f;
        private float _currentYRotation = 0f;

        private bool _hasDealtDamageThisTurn = false;

        public void OnStart(string jsonParams)
        {
            if (!API.HasTransform(Entity))
            {
                API.Log("[BossHideSeek] ERROR: Entity missing TransformComponent!");
                return;
            }

            _currentYRotation = API.GetRotation(Entity).Y;
            _targetYRotation = _currentYRotation;
            _timer = 0f;

            API.Log("[BossHideSeek] Initialized.");
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Handle the turn timer
            if (!_isTurning)
            {
                _timer += dt;
                if (_timer >= _rotationInterval)
                {
                    _timer = 0f;
                    _isTurning = true;
                    _targetYRotation += 180f;
                    
                    // Normalize target
                    while (_targetYRotation >= 360f) _targetYRotation -= 360f;
                    
                    _hasDealtDamageThisTurn = false;
                    API.Log("[BossHideSeek] Starting 180-degree turn...");
                }
            }

            // Smooth rotation
            if (_isTurning)
            {
                float angleDiff = _targetYRotation - _currentYRotation;
                while (angleDiff > 180f) angleDiff -= 360f;
                while (angleDiff < -180f) angleDiff += 360f;

                float step = _rotationSpeed * dt;
                if (Math.Abs(angleDiff) <= step)
                {
                    _currentYRotation = _targetYRotation;
                    _isTurning = false;
                }
                else
                {
                    _currentYRotation += Math.Sign(angleDiff) * step;
                }

                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
            }
            else
            {
                // Boss is stationary and "watching"
                CheckPlayerDetection();
            }
        }

        private void CheckPlayerDetection()
        {
            if (_hasDealtDamageThisTurn) return;

            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            if (playerEntity == 0) return;

            // Use the established stealth logic: if the player is invisible to enemies, they are safe.
            // This covers crouching in crouch zones (the lion statues).
            if (PlayerMovement.IsPlayerInvisibleToEnemies()) return;

            Vec3 bossPos = API.GetPosition(Entity);
            Vec3 playerPos = API.GetPosition(playerEntity);

            // Distance check
            Vec3 toPlayer = new Vec3(playerPos.X - bossPos.X, 0, playerPos.Z - bossPos.Z);
            float distSq = toPlayer.X * toPlayer.X + toPlayer.Z * toPlayer.Z;
            
            if (distSq > _detectionRange * _detectionRange) return;

            // Angle check
            float dist = (float)Math.Sqrt(distSq);
            if (dist < 0.001f) return;

            // Boss forward vector (assuming Y rotation 0 faces Z+)
            float rad = _currentYRotation * (float)Math.PI / 180f;
            float multiplier = _invertForward ? -1f : 1f;
            Vec3 forward = new Vec3((float)Math.Sin(rad) * multiplier, 0, (float)Math.Cos(rad) * multiplier);

            // Dot product for angle
            float dot = (toPlayer.X * forward.X + toPlayer.Z * forward.Z) / dist;
            float angleToPlayer = (float)(Math.Acos(Math.Max(-1f, Math.Min(1f, dot))) * 180f / Math.PI);

            if (angleToPlayer <= _detectionAngle * 0.5f)
            {
                // LINE OF SIGHT CHECK (Optional but recommended)
                // If your engine supports raycasting, you could add it here.
                // For now, we rely on the crouch/stealth logic.

                API.Log("[BossHideSeek] Player detected! Dealing damage.");
                _hasDealtDamageThisTurn = true;
                
                // Trigger standard player damage
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }
    }
}
