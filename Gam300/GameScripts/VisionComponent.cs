using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Industry-standard vision cone component that can be attached to any enemy entity.
    /// Handles detection, line of sight, and provides events for other systems.
    /// </summary>
    public class VisionComponent
    {
        public ulong Entity;

        // === CONFIGURABLE PARAMETERS ===
        [System.Serializable]
        public class VisionSettings
        {
            public float detectionRange = 15f;           // Max detection distance
            public float detectionAngle = 60f;           // Total cone angle (not half)
            public float loseTargetRange = 18f;          // Lose target beyond this range (hysteresis)
            public float loseTargetAngle = 75f;          // Lose target beyond this angle (hysteresis)
            public float updateInterval = 0.1f;          // How often to check (optimization)
            public float alertDuration = 5f;             // How long to stay alert after losing target
            public string[] targetTags = { "Samurai" };  // What entities to detect
            public bool requireLineOfSight = false;      // Enable raycast validation (future)
            public bool debugLog = true;                 // Enable debug logging
        }

        // === VISION STATE ===
        public enum VisionState
        {
            Idle,        // No target detected
            Suspicious,  // Target detected but not confirmed
            Alert,       // Target confirmed and tracking
            Searching    // Lost target, searching area
        }

        // === PRIVATE FIELDS ===
        private VisionSettings _settings = new VisionSettings();
        private VisionState _currentState = VisionState.Idle;
        private ulong _currentTarget = 0;
        private float _lastUpdateTime = 0f;
        private float _alertTimer = 0f;
        private Vec3 _lastKnownTargetPosition;

        // === EVENTS (for other systems to listen to) ===
        public delegate void VisionEventHandler(ulong target, Vec3 position);
        public event VisionEventHandler OnTargetDetected;
        public event VisionEventHandler OnTargetLost;
        public event VisionEventHandler OnTargetUpdated;

        public void OnStart(string jsonParams)
        {
            if (_settings.debugLog)
                API.Log($"[VisionComponent] Initialized on Entity: {Entity}");

            // TODO: Parse jsonParams for custom settings in the future
            ValidateEntity();
        }

        public void OnUpdate(float dt)
        {
            if (!ValidateEntity()) return;

            // Update timers
            _lastUpdateTime += dt;
            _alertTimer += dt;

            // Staggered updates for optimization
            if (_lastUpdateTime < _settings.updateInterval)
                return;

            _lastUpdateTime = 0f;

            // State machine update
            UpdateVisionState(dt);
        }

        private void UpdateVisionState(float dt)
        {
            switch (_currentState)
            {
                case VisionState.Idle:
                    CheckForTargets();
                    break;

                case VisionState.Suspicious:
                case VisionState.Alert:
                    if (_currentTarget != 0)
                    {
                        if (ValidateTarget(_currentTarget))
                        {
                            UpdateTargetTracking();
                        }
                        else
                        {
                            LoseTarget();
                        }
                    }
                    else
                    {
                        CheckForTargets();
                    }
                    break;

                case VisionState.Searching:
                    if (_alertTimer >= _settings.alertDuration)
                    {
                        _currentState = VisionState.Idle;
                        if (_settings.debugLog)
                            API.Log("[VisionComponent] Stopped searching, returning to idle");
                    }
                    else
                    {
                        CheckForTargets(); // Might spot target again
                    }
                    break;
            }
        }

        private void CheckForTargets()
        {
            // Check each potential target type
            foreach (string targetTag in _settings.targetTags)
            {
                ulong target = API.FindEntity(targetTag);
                if (target != 0 && ValidateTarget(target))
                {
                    DetectTarget(target);
                    return; // Only track one target at a time
                }
            }
        }

        private bool ValidateTarget(ulong target)
        {
            if (target == 0) return false;

            var enemyPos = API.GetPosition(Entity);
            var enemyRot = API.GetRotation(Entity);
            var targetPos = API.GetPosition(target);

            return IsTargetInVisionCone(enemyPos, enemyRot, targetPos);
        }

        private bool IsTargetInVisionCone(Vec3 enemyPos, Vec3 enemyRot, Vec3 targetPos)
        {
            // Calculate direction to target
            var dirToTarget = new Vec3(
                targetPos.X - enemyPos.X,
                0f,
                targetPos.Z - enemyPos.Z
            );

            // Calculate distance
            float distance = (float)Math.Sqrt(dirToTarget.X * dirToTarget.X + dirToTarget.Z * dirToTarget.Z);

            // Use different thresholds based on current state (hysteresis)
            float maxRange = (_currentState == VisionState.Alert) ? _settings.loseTargetRange : _settings.detectionRange;
            float maxAngle = (_currentState == VisionState.Alert) ? _settings.loseTargetAngle : _settings.detectionAngle;

            // Range check
            if (distance > maxRange) return false;
            if (distance < 0.1f) return true; // Very close always detected

            // Normalize direction
            dirToTarget.X /= distance;
            dirToTarget.Z /= distance;

            // FIXED: Calculate enemy forward direction correctly
            // The enemy's rotation.Y directly represents the yaw angle
            // 0° = facing +Z, 90° = facing +X, 180° = facing -Z, 270° = facing -X
            float yawRadians = enemyRot.Y * (float)Math.PI / 180f;

            var forward = new Vec3(
                (float)Math.Sin(yawRadians),
                0f,
                (float)Math.Cos(yawRadians)
            );

            // Calculate angle between forward direction and direction to target
            float dotProduct = dirToTarget.X * forward.X + dirToTarget.Z * forward.Z;
            float angle = (float)Math.Acos(Math.Max(-1f, Math.Min(1f, dotProduct))) * 180f / (float)Math.PI;

            if (_settings.debugLog && _currentState == VisionState.Alert)
            {
                API.Log($"[VisionComponent] Enemy rot: {enemyRot.Y}°, Forward: ({forward.X:F2}, {forward.Z:F2}), " +
                       $"DirToTarget: ({dirToTarget.X:F2}, {dirToTarget.Z:F2}), Angle: {angle:F1}°, MaxAngle: {maxAngle * 0.5f:F1}°");
            }

            return angle <= (maxAngle * 0.5f); // maxAngle is total cone, so half for each side
        }

        private void DetectTarget(ulong target)
        {
            if (_currentTarget != target)
            {
                _currentTarget = target;
                _lastKnownTargetPosition = API.GetPosition(target);
                _alertTimer = 0f;

                _currentState = VisionState.Alert;

                if (_settings.debugLog)
                    API.Log(">>> TARGET DETECTED! <<<");

                // Trigger event for other systems
                OnTargetDetected?.Invoke(target, _lastKnownTargetPosition);
            }
        }

        private void UpdateTargetTracking()
        {
            var targetPos = API.GetPosition(_currentTarget);
            _lastKnownTargetPosition = targetPos;

            if (_settings.debugLog)
            {
                var enemyPos = API.GetPosition(Entity);
                float distance = (float)Math.Sqrt(
                    (targetPos.X - enemyPos.X) * (targetPos.X - enemyPos.X) +
                    (targetPos.Z - enemyPos.Z) * (targetPos.Z - enemyPos.Z)
                );
                API.Log($"[VisionComponent] Tracking target - Distance: {distance:F1}");
            }

            // Trigger update event
            OnTargetUpdated?.Invoke(_currentTarget, targetPos);
        }

        private void LoseTarget()
        {
            if (_currentTarget != 0)
            {
                if (_settings.debugLog)
                    API.Log("[VisionComponent] Target lost, entering search mode");

                // Trigger event
                OnTargetLost?.Invoke(_currentTarget, _lastKnownTargetPosition);

                _currentTarget = 0;
                _currentState = VisionState.Searching;
                _alertTimer = 0f;
            }
        }

        private bool ValidateEntity()
        {
            if (!API.HasTransform(Entity))
            {
                if (_settings.debugLog)
                    API.Log("[VisionComponent] ERROR: Entity missing TransformComponent!");
                return false;
            }
            return true;
        }

        // === PUBLIC INTERFACE ===
        public VisionState GetState() => _currentState;
        public ulong GetCurrentTarget() => _currentTarget;
        public Vec3 GetLastKnownTargetPosition() => _lastKnownTargetPosition;
        public bool HasTarget() => _currentTarget != 0;

        // Allow runtime parameter adjustment
        public void SetDetectionRange(float range) => _settings.detectionRange = range;
        public void SetDetectionAngle(float angle) => _settings.detectionAngle = angle;
        public void SetUpdateInterval(float interval) => _settings.updateInterval = interval;

        public void OnDestroy()
        {
            if (_settings.debugLog)
                API.Log($"[VisionComponent] Destroyed on Entity: {Entity}");
        }
    }
}