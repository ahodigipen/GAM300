using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Detects player presence within close range (360 degrees) after a sustained duration.
    /// Used for rear/peripheral awareness when player lingers too close to an enemy.
    /// </summary>
    public class ProximityDetectionComponent
    {
        public ulong Entity;

        // Detection settings
        [Boom.EditorExposed("Detection Radius", "How close player must be", 0.1f, 20f, true)]
        public float DetectionRadius = 3.0f;           // How close player must be

        [Boom.EditorExposed("Detection Duration", "How long player must stay close", 0.1f, 20f, true)]
        public float DetectionDuration = 5.0f;         // How long player must stay close

        [Boom.EditorExposed("Vertical Tolerance", "Vertical range (same floor)", 0.1f, 10f, true)]
        public float VerticalTolerance = 2.5f;         // Vertical range (same floor)

        // State tracking
        private float _proximityTimer = 0f;
        private bool _isPlayerInRange = false;
        private bool _hasTriggeredDetection = false;

        // Events
        public delegate void ProximityEventHandler(ulong target, Vec3 position);
        public event ProximityEventHandler OnProximityDetected;

        // Debug
        [Boom.EditorExposed("Debug Log", "Enable proximity detection debug logging")]
        public bool DebugLog = true;  // Enable by default for testing
        private float _debugTimer = 0f;

        /// <summary>
        /// Initialize the proximity detection system
        /// </summary>
        public void OnStart()
        {
            if (!API.HasTransform(Entity))
            {
                API.Log("[ProximityDetection] ERROR: Entity missing TransformComponent!");
                return;
            }

            API.Log($"[ProximityDetection] Initialized - Range: {DetectionRadius}m, Duration: {DetectionDuration}s");
        }

        /// <summary>
        /// Update proximity detection each frame
        /// </summary>
        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Get player entity
            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            if (playerEntity == 0 || !API.HasTransform(playerEntity))
            {
                ResetProximityTimer();
                return;
            }

            // Skip if player is invisible (crouching stealth)
            if (PlayerMovement.IsPlayerInvisibleToEnemies())
            {
                ResetProximityTimer();
                return;
            }

            Vec3 enemyPos = API.GetPosition(Entity);
            Vec3 playerPos = API.GetPosition(playerEntity);

            // Check if player is on same floor (vertical check)
            float verticalDistance = Math.Abs(playerPos.Y - enemyPos.Y);
            if (verticalDistance > VerticalTolerance)
            {
                ResetProximityTimer();
                return;
            }

            // Calculate horizontal distance
            float dx = playerPos.X - enemyPos.X;
            float dz = playerPos.Z - enemyPos.Z;
            float horizontalDistance = (float)Math.Sqrt(dx * dx + dz * dz);

            // Check if player is within detection radius
            bool playerCurrentlyInRange = horizontalDistance <= DetectionRadius;

            if (playerCurrentlyInRange)
            {
                if (!_isPlayerInRange)
                {
                    // Player just entered range
                    _isPlayerInRange = true;
                    _proximityTimer = 0f;

                    if (DebugLog)
                    {
                        API.Log($"[ProximityDetection] Player entered proximity range ({horizontalDistance:F2}m)");
                    }
                }

                // Accumulate time while player is in range
                _proximityTimer += dt;

                // Check if detection threshold reached
                if (!_hasTriggeredDetection && _proximityTimer >= DetectionDuration)
                {
                    TriggerProximityDetection(playerEntity, playerPos);
                }

                // Debug logging
                if (DebugLog)
                {
                    _debugTimer += dt;
                    if (_debugTimer >= 0.5f)
                    {
                        _debugTimer = 0f;
                        float remaining = DetectionDuration - _proximityTimer;
                        API.Log($"[ProximityDetection] Timer: {_proximityTimer:F2}s / {DetectionDuration}s (Remaining: {remaining:F2}s)");
                    }
                }
            }
            else
            {
                // Player left range - reset timer
                if (_isPlayerInRange)
                {
                    if (DebugLog)
                    {
                        API.Log($"[ProximityDetection] Player left proximity range - timer reset");
                    }
                }

                ResetProximityTimer();
            }
        }

        /// <summary>
        /// Trigger proximity detection event
        /// </summary>
        private void TriggerProximityDetection(ulong playerEntity, Vec3 playerPos)
        {
            _hasTriggeredDetection = true;

            API.Log($"[ProximityDetection] PROXIMITY ALERT! Player lingered for {DetectionDuration}s");

            // Play proximity alert sound (different from vision alert)
            Vec3 enemyPos = API.GetPosition(Entity);
            API.PlaySoundAt("proximity_alert_" + Entity, "Resources/Audio/enemyHurt_2.wav", enemyPos, false);
            API.SetSoundVolume("proximity_alert_" + Entity, 0.7f);
            API.Set3DMinMaxDistance("proximity_alert_" + Entity, 1.0f, 20.0f);

            // Invoke event for parent controller to handle
            OnProximityDetected?.Invoke(playerEntity, playerPos);
        }

        /// <summary>
        /// Reset proximity timer and state
        /// </summary>
        private void ResetProximityTimer()
        {
            if (_isPlayerInRange || _proximityTimer > 0f)
            {
                _isPlayerInRange = false;
                _proximityTimer = 0f;
                _hasTriggeredDetection = false;
            }
        }

        /// <summary>
        /// Force reset detection state (e.g., when enemy loses target)
        /// </summary>
        public void ResetDetection()
        {
            ResetProximityTimer();
            if (DebugLog)
            {
                API.Log("[ProximityDetection] Detection state forcefully reset");
            }
        }

        // Configuration methods
        public void SetDetectionRadius(float radius)
        {
            DetectionRadius = radius;
            API.Log($"[ProximityDetection] Detection radius set to {DetectionRadius}m");
        }

        public void SetDetectionDuration(float duration)
        {
            DetectionDuration = duration;
            API.Log($"[ProximityDetection] Detection duration set to {DetectionDuration}s");
        }

        public void SetVerticalTolerance(float tolerance)
        {
            VerticalTolerance = tolerance;
            API.Log($"[ProximityDetection] Vertical tolerance set to {VerticalTolerance}m");
        }

        public void EnableDebugLog(bool enable)
        {
            DebugLog = enable;
        }

        public float GetProximityTimer() => _proximityTimer;
        public bool IsPlayerInProximity() => _isPlayerInRange;
        public bool HasTriggered() => _hasTriggeredDetection;

        public void OnDestroy()
        {
            // Cleanup if needed
        }
    }
}