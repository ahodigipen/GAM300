using System;
using Boom;

namespace GameScripts
{
    public class VisionComponent
    {
        public ulong Entity;
        private bool _linecastAvailable = true;
        private bool _warnedNoLinecast = false;

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
            public string[] targetNames = { "Samurai", "Player", "SamuraiModel" };
            public bool requireLineOfSight = true;
            public bool debugLog = false;
            public bool debugReasons = false;
            public bool debugLOS = false;
            public float eyeOffsetY = 1.5f;
        }

        public enum VisionState { Idle, Suspicious, Alert, Searching }

        private VisionSettings _settings = new VisionSettings();
        private VisionState _currentState = VisionState.Idle;
        private ulong _currentTarget = 0;
        private float _lastUpdateTime;
        private float _alertTimer;
        private Vec3 _lastKnownTargetPosition;

        public void OnStart(string jsonParams)
        {
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

        public void Reset()
        {
            _currentTarget = 0;
            _currentState = VisionState.Idle;
            _alertTimer = 0f;
            _lastUpdateTime = 0f;
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
                        _currentState = VisionState.Idle;
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

            // Skip player if in stealth crouch invisibility
            if (target == PlayerMovement.GetPlayerEntity() && PlayerMovement.IsPlayerInvisibleToEnemies())
                return false;

            var enemyPos = API.GetPosition(Entity);
            var enemyRot = API.GetRotation(Entity);
            var targetPos = API.GetPosition(target);

            float verticalDiff = Math.Abs(targetPos.Y - enemyPos.Y);
            if (verticalDiff > _settings.verticalMaxDifference)
                return false;

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
                return false;

            float tx = dx / dist;
            float tz = dz / dist;

            float yawRad = enemyRot.Y * (float)Math.PI / 180f;
            float fx = (float)Math.Sin(yawRad);
            float fz = (float)Math.Cos(yawRad);

            float dot = tx * fx + tz * fz;
            float halfAngleRad = (_settings.detectionAngle * 0.5f) * (float)Math.PI / 180f;
            float cosHalf = (float)Math.Cos(halfAngleRad);

            if (dot < cosHalf)
                return false;

            if (_settings.requireLineOfSight && !HasLineOfSightToTarget(enemyPos, targetPos, targetHandle))
                return false;

            return true;
        }

        private bool HasLineOfSightToTarget(Vec3 from, Vec3 to, ulong targetHandle)
        {
            if (!_settings.requireLineOfSight) return true;
            if (!_linecastAvailable) return true;

            try
            {
                Vec3 fromEye = new Vec3(from.X, from.Y + _settings.eyeOffsetY, from.Z);
                Vec3 toEye = new Vec3(to.X, to.Y + 1.5f, to.Z);

                // First check: ignore self and target (most common case)
                bool clear = API.LinecastIgnoreBoth(fromEye, toEye, Entity, targetHandle);

                if (clear)
                {
                    if (_settings.debugLOS)
                        API.Log("[VisionComponent] LOS clear (no obstruction)");
                    return true;
                }

                if (_settings.debugLOS)
                    API.Log("[VisionComponent] LOS blocked");

                return false;
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
            OnTargetDetected?.Invoke(target, _lastKnownTargetPosition);
        }

        private void UpdateTargetTracking()
        {
            var targetPos = API.GetPosition(_currentTarget);
            _lastKnownTargetPosition = targetPos;
            OnTargetUpdated?.Invoke(_currentTarget, targetPos);
        }

        private void LoseTarget()
        {
            if (_currentTarget == 0) return;
            OnTargetLost?.Invoke(_currentTarget, _lastKnownTargetPosition);
            _currentTarget = 0;
            _currentState = VisionState.Searching;
            _alertTimer = 0f;
        }

        private bool ValidateEntity()
        {
            return API.HasTransform(Entity);
        }

        public VisionState GetState() => _currentState;
        public ulong GetCurrentTarget() => _currentTarget;
        public Vec3 GetLastKnownTargetPosition() => _lastKnownTargetPosition;
        public bool HasTarget() => _currentTarget != 0;
        public void SetDetectionRange(float r) => _settings.detectionRange = r;
        public void SetLoseTargetRange(float r) => _settings.loseTargetRange = r;
        public void SetDetectionAngle(float a) => _settings.detectionAngle = a;
        public void SetUpdateInterval(float i) => _settings.updateInterval = i;
        public void SetVerticalTolerance(float v) => _settings.verticalMaxDifference = v;
        public void SetAlertDuration(float d) => _settings.alertDuration = d;
        public void SetRequireLineOfSight(bool v) => _settings.requireLineOfSight = v;
        public void SetEyeOffsetY(float y) => _settings.eyeOffsetY = y;
        public void SetTargetNames(string[] names) => _settings.targetNames = names;
        public void EnableDebugReasons(bool v) => _settings.debugReasons = v;
        public void EnableDebugLOS(bool v) => _settings.debugLOS = v;

        public void OnDestroy() { }
    }
}
