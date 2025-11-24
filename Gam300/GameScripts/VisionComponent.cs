using System;
using Boom;

namespace GameScripts
{
    public class VisionComponent
    {
        public ulong Entity;
        // Tracks whether native linecast succeeded at least once. If it fails we stop trying.
        private static bool _linecastAvailable = true;
        private static bool _warnedNoLinecast = false;

        public delegate void VisionEventHandler(ulong target, Vec3 position);
        public event VisionEventHandler OnTargetDetected;
        public event VisionEventHandler OnTargetUpdated;
        public event VisionEventHandler OnTargetLost;

        [Serializable]
        public class VisionSettings
        {
            public float detectionRange = 12f;
            public float loseTargetRange = 15f;
            public float detectionAngle = 60f;
            public float updateInterval = 0.1f;
            public float alertDuration = 4f;
            public float verticalMaxDifference = 2.5f;
            public string[] targetNames = { "Samurai", "Player" };
            public bool requireLineOfSight = true;
            public bool debugLog = true;
            public bool debugReasons = false;
            public bool debugLOS = false;
        }

        public enum VisionState { Idle, Suspicious, Alert, Searching }

        private VisionSettings _settings = new VisionSettings();
        private VisionState _currentState = VisionState.Idle;
        private ulong _currentTarget = 0;
        private float _lastUpdateTime;
        private float _alertTimer;
        private Vec3 _lastKnownTargetPosition;
        private bool _hasValidTarget = false;


        private string _lastLosReason = "";

        public void OnStart(string jsonParams)
        {
            if (_settings.debugLog)
                API.Log($"[VisionComponent] Start on {Entity}");
            ValidateEntity();
        }

        public void OnUpdate(float dt)
        {
            if (!ValidateEntity()) return;
            _lastUpdateTime += dt;
            _alertTimer += dt;
            if (_lastUpdateTime < _settings.updateInterval) return;
            _lastUpdateTime = 0f;
            UpdateVisionState(dt);
        }

        private void UpdateVisionState(float dt)
        {
            switch (_currentState)
            {
                case VisionState.Idle:
                    CheckForTargets();
                    break;
                case VisionState.Alert:
                    if (_currentTarget != 0)
                    {
                        if (!ValidateTarget(_currentTarget))
                            LoseTarget();
                        else
                            UpdateTargetTracking();
                    }
                    else
                        CheckForTargets();
                    break;
                case VisionState.Searching:
                    if (_alertTimer >= _settings.alertDuration)
                    {
                        _currentState = VisionState.Idle;
                        if (_settings.debugLog)
                            API.Log("[VisionComponent] Search expired -> Idle");
                    }
                    else
                        CheckForTargets();
                    break;
                case VisionState.Suspicious:
                    CheckForTargets();
                    break;
            }
        }

        private void CheckForTargets()
        {
            ulong best = 0;
            float bestDist2 = float.MaxValue;
            foreach (string name in _settings.targetNames)
            {
                ulong e = API.FindEntity(name);
                if (e == 0) continue;
                if (!ValidateTarget(e)) continue;

                var enemyPos = API.GetPosition(Entity);
                var targetPos = API.GetPosition(e);
                float d2 = (targetPos.X - enemyPos.X) * (targetPos.X - enemyPos.X) +
                           (targetPos.Z - enemyPos.Z) * (targetPos.Z - enemyPos.Z);
                if (d2 < bestDist2) { best = e; bestDist2 = d2; }
            }
            if (best != 0)
                DetectTarget(best);
        }

        private bool ValidateTarget(ulong target)
        {
            if (target == 0) return false;
            var enemyPos = API.GetPosition(Entity);
            var enemyRot = API.GetRotation(Entity);
            var targetPos = API.GetPosition(target);

            float verticalDiff = Math.Abs(targetPos.Y - enemyPos.Y);
            if (verticalDiff > _settings.verticalMaxDifference)
            {
                if (_settings.debugReasons)
                    API.Log($"[VisionComponent] Reject vertical diff {verticalDiff:F2} > {_settings.verticalMaxDifference:F2}");
                return false;
            }

            return IsTargetInVisionConeAndLOS(enemyPos, enemyRot, targetPos, target);
        }

        private bool IsTargetInVisionConeAndLOS(Vec3 enemyPos, Vec3 enemyRot, Vec3 targetPos, ulong targetHandle)
        {
            float dx = targetPos.X - enemyPos.X;
            float dz = targetPos.Z - enemyPos.Z;
            float dist2 = dx * dx + dz * dz;
            float dist = (float)Math.Sqrt(Math.Max(1e-6f, dist2));

            float maxRange = (_currentState == VisionState.Alert) ? _settings.loseTargetRange : _settings.detectionRange;
            if (dist > maxRange)
            {
                if (_settings.debugReasons) API.Log($"[VisionComponent] Reject dist {dist:F2} > {maxRange:F2}");
                return false;
            }

            float tx = dx / dist;
            float tz = dz / dist;

            float yawRad = enemyRot.Y * (float)Math.PI / 180f;
            float fx = (float)Math.Sin(yawRad);
            float fz = (float)Math.Cos(yawRad);

            float dot = tx * fx + tz * fz;
            float halfAngleRad = (_settings.detectionAngle * 0.5f) * (float)Math.PI / 180f;
            float cosHalf = (float)Math.Cos(halfAngleRad);

            if (dot < cosHalf)
            {
                if (_settings.debugReasons)
                    API.Log($"[VisionComponent] Reject angle cos={dot:F3} < {cosHalf:F3} (yaw={enemyRot.Y:F1})");
                return false;
            }

            if (_settings.requireLineOfSight && !HasLineOfSight(enemyPos, targetPos))
            {
                if (_settings.debugReasons)
                    API.Log($"[VisionComponent] Reject LOS blocked ({_lastLosReason})");
                return false;
            }

            return true;
        }

        private bool HasLineOfSight(Vec3 from, Vec3 to)
        {
            if (!_settings.requireLineOfSight) return true;
            if (!_linecastAvailable) return true;

            try
            {
                // Pass Entity so native code can ignore enemy's own collider
                bool clear = API.Linecast(from, to, Entity);
                _lastLosReason = clear ? "clear" : "ray blocked before target";
                if (_settings.debugLOS)
                    API.Log($"[VisionComponent] LOS {(clear ? "CLEAR" : "BLOCKED")}");
                return clear;
            }
            catch
            {
                _linecastAvailable = false;
                _settings.requireLineOfSight = false;
                if (!_warnedNoLinecast)
                {
                    _warnedNoLinecast = true;
                    API.Log("[VisionComponent] WARN: Native Linecast missing -> LOS disabled.");
                }
                return true;
            }
        }

        private void DetectTarget(ulong target)
        {
            if (_currentTarget == target) return;
            _currentTarget = target;
            _lastKnownTargetPosition = API.GetPosition(target);
            _alertTimer = 0f;
            _currentState = VisionState.Alert;
            if (_settings.debugLog)
                API.Log($"[VisionComponent] >>> TARGET DETECTED ({target}) <<<");
            OnTargetDetected?.Invoke(target, _lastKnownTargetPosition);
        }

        private void UpdateTargetTracking()
        {
            var targetPos = API.GetPosition(_currentTarget);
            _lastKnownTargetPosition = targetPos;

            if (_settings.debugLog)
            {
                var enemyPos = API.GetPosition(Entity);
                float d = (float)Math.Sqrt(
                    (targetPos.X - enemyPos.X) * (targetPos.X - enemyPos.X) +
                    (targetPos.Z - enemyPos.Z) * (targetPos.Z - enemyPos.Z));
                API.Log($"[VisionComponent] Tracking target {_currentTarget} dist={d:F1}");
            }

            OnTargetUpdated?.Invoke(_currentTarget, targetPos);
        }

        private void LoseTarget()
        {
            if (_currentTarget == 0) return;
            if (_settings.debugLog) API.Log("[VisionComponent] Target lost -> Searching");
            OnTargetLost?.Invoke(_currentTarget, _lastKnownTargetPosition);
            _currentTarget = 0;
            _currentState = VisionState.Searching;
            _alertTimer = 0f;
        }

        private bool ValidateEntity()
        {
            if (!API.HasTransform(Entity))
            {
                if (_settings.debugLog) API.Log("[VisionComponent] ERROR: Missing TransformComponent");
                return false;
            }
            return true;
        }

        public VisionState GetState() => _currentState;
        public ulong GetCurrentTarget() => _currentTarget;
        public Vec3 GetLastKnownTargetPosition() => _lastKnownTargetPosition;
        public bool HasTarget() => _currentTarget != 0;

        public void SetDetectionRange(float r) => _settings.detectionRange = r;
        public void SetDetectionAngle(float a) => _settings.detectionAngle = a;
        public void SetUpdateInterval(float i) => _settings.updateInterval = i;
        public void SetVerticalTolerance(float v) => _settings.verticalMaxDifference = v;
        public void SetRequireLineOfSight(bool v) => _settings.requireLineOfSight = v;
        public void EnableDebugReasons(bool v) => _settings.debugReasons = v;
        public void EnableDebugLOS(bool v) => _settings.debugLOS = v;

        public void OnDestroy()
        {
            if (_settings.debugLog)
                API.Log($"[VisionComponent] Destroyed ({Entity})");
        }
    }
}