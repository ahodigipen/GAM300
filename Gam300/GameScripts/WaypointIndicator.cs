using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Waypoint indicator that uses the DetectionRing sprite entity.
    /// Moves the ring to follow the player and rotates it on the Y-axis
    /// to point toward the nearest uncollected key.
    /// When all keys are collected, the ring is hidden off-screen.
    /// </summary>
    public class WaypointIndicator
    {
        public ulong Entity;

        // All key entity names in the M3 GAMEPLAY scene
        private static readonly string[] KEY_NAMES = {
            "Key1_1", "Key1_2", "Key1_3",
            "Key2_1", "Key2_2", "Key2_3",
            "Key2_sr1", "Key2_sr2", "Key1_sr1"
        };

        // Key tracking
        private ulong[] _keyEntities;
        private Vec3[] _keyOriginalPositions;
        private bool[] _keyCollected;
        private int _currentTargetIndex = -1;

        // DetectionRing sprite entity
        private ulong _ringEntity;
        private Vec3 _ringScale;

        // Height offset above the player's feet
        private float _heightOffset = 0.3f;

        // Smooth rotation
        private float _currentYawDeg = 0f;
        private float _rotationSmoothSpeed = 6.0f;

        // Pulse animation
        private float _pulseTimer = 0f;
        private float _pulseSpeed = 2.5f;

        // Re-evaluation timer (find nearest key periodically)
        private float _retargetTimer = 0f;
        private float _retargetInterval = 1.0f;

        // Hidden position (far below the map)
        private static readonly Vec3 HIDDEN_POS = new Vec3(0f, -9999f, 0f);

        public void OnStart(string jsonParams)
        {
            _keyEntities = new ulong[KEY_NAMES.Length];
            _keyOriginalPositions = new Vec3[KEY_NAMES.Length];
            _keyCollected = new bool[KEY_NAMES.Length];

            int foundCount = 0;
            for (int i = 0; i < KEY_NAMES.Length; i++)
            {
                _keyEntities[i] = API.FindEntity(KEY_NAMES[i]);
                if (_keyEntities[i] != 0 && API.HasTransform(_keyEntities[i]))
                {
                    _keyOriginalPositions[i] = API.GetPosition(_keyEntities[i]);
                    _keyCollected[i] = false;
                    foundCount++;
                }
                else
                {
                    _keyCollected[i] = true;
                }
            }

            // Find the DetectionRing sprite entity in the scene
            _ringEntity = API.FindEntity("DetectionRing");
            if (_ringEntity != 0 && API.HasTransform(_ringEntity))
            {
                _ringScale = API.GetScale(_ringEntity);
                // Start hidden until we have a valid target
                API.SetPosition(_ringEntity, HIDDEN_POS);
                API.Log($"[WaypointIndicator] Found DetectionRing entity. Scale: ({_ringScale.X:F1}, {_ringScale.Y:F1}, {_ringScale.Z:F1})");
            }
            else
            {
                API.Log("[WaypointIndicator] WARNING: DetectionRing entity not found in scene!");
            }

            FindNearestTarget();
            API.Log($"[WaypointIndicator] Initialized with DetectionRing sprite. Tracking {foundCount} keys.");
        }

        public void OnUpdate(float dt)
        {
            if (_ringEntity == 0) return;

            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            if (playerEntity == 0) return;

            // Check which keys have been collected
            CheckCollectedKeys();

            // If current target was collected, find next immediately
            if (_currentTargetIndex >= 0 && _keyCollected[_currentTargetIndex])
                FindNearestTarget();

            // Periodically re-evaluate nearest target
            _retargetTimer += dt;
            if (_retargetTimer >= _retargetInterval)
            {
                _retargetTimer = 0f;
                FindNearestTarget();
            }

            // No target — hide the ring
            if (_currentTargetIndex < 0)
            {
                API.SetPosition(_ringEntity, HIDDEN_POS);
                return;
            }

            // Get player position
            Vec3 playerPos = API.GetPosition(playerEntity);
            Vec3 keyPos = _keyOriginalPositions[_currentTargetIndex];

            // Calculate direction from player to key on XZ plane
            float dx = keyPos.X - playerPos.X;
            float dz = keyPos.Z - playerPos.Z;

            // Target yaw in degrees (Y-axis rotation)
            float targetYawRad = (float)Math.Atan2(dx, dz);
            float targetYawDeg = targetYawRad * (180f / (float)Math.PI);

            // Smoothly interpolate current yaw toward target
            _currentYawDeg = LerpAngleDeg(_currentYawDeg, targetYawDeg, _rotationSmoothSpeed * dt);

            // Pulse animation for scale
            _pulseTimer += dt * _pulseSpeed;
            float pulse = 1.0f + 0.08f * (float)Math.Sin(_pulseTimer);

            // Position the ring at the player's feet + height offset
            Vec3 ringPos = new Vec3(playerPos.X, playerPos.Y + _heightOffset, playerPos.Z);
            API.SetPosition(_ringEntity, ringPos);

            // Rotate the ring to point toward the target key (Y-axis only)
            Vec3 rot = API.GetRotation(_ringEntity);
            API.SetRotation(_ringEntity, new Vec3(rot.X, _currentYawDeg, rot.Z));

            // Apply pulse to scale
            Vec3 pulsedScale = new Vec3(
                _ringScale.X * pulse,
                _ringScale.Y * pulse,
                _ringScale.Z * pulse
            );
            API.SetScale(_ringEntity, pulsedScale);
        }

        private void CheckCollectedKeys()
        {
            for (int i = 0; i < _keyEntities.Length; i++)
            {
                if (_keyCollected[i]) continue;
                if (_keyEntities[i] == 0) { _keyCollected[i] = true; continue; }

                Vec3 pos = API.GetPosition(_keyEntities[i]);
                if (pos.Y < -50f)
                {
                    _keyCollected[i] = true;
                    API.Log($"[WaypointIndicator] Key '{KEY_NAMES[i]}' collected.");
                }
            }
        }

        private void FindNearestTarget()
        {
            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            if (playerEntity == 0) { _currentTargetIndex = -1; return; }

            Vec3 playerPos = API.GetPosition(playerEntity);
            float bestDistSq = float.MaxValue;
            int bestIdx = -1;

            for (int i = 0; i < _keyEntities.Length; i++)
            {
                if (_keyCollected[i] || _keyEntities[i] == 0) continue;

                float dx = _keyOriginalPositions[i].X - playerPos.X;
                float dz = _keyOriginalPositions[i].Z - playerPos.Z;
                float distSq = dx * dx + dz * dz;

                if (distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    bestIdx = i;
                }
            }

            if (bestIdx != _currentTargetIndex)
            {
                _currentTargetIndex = bestIdx;
                if (bestIdx >= 0)
                    API.Log($"[WaypointIndicator] Targeting: {KEY_NAMES[bestIdx]} (dist: {Math.Sqrt(bestDistSq):F1})");
                else
                    API.Log("[WaypointIndicator] All keys collected! Indicator hidden.");
            }
        }

        private float LerpAngleDeg(float current, float target, float t)
        {
            float diff = target - current;

            // Normalize to -180..180
            while (diff > 180f) diff -= 360f;
            while (diff < -180f) diff += 360f;

            t = Math.Max(0f, Math.Min(1f, t));
            return current + diff * t;
        }
    }
}
