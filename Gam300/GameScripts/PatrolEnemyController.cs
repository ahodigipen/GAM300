using Boom;
using System;

namespace GameScripts
{
    public class PatrolEnemyController
    {
        public ulong Entity;

        // ===== World convention (pick ONE and keep it everywhere) =====
        // If your models face -Z when "forward", leave this true.
        // If your models face +Z when "forward", set to false.
        private const bool WORLD_FORWARD_IS_NEG_Z = true;

        // ===== Options =====
        private const bool LOCK_IN_PLACE = false;

        // ===== Rotation tuning =====
        private float _rotationSpeedDeg = 360f;
        private float _minSpeedToRotate = 0.15f;   // m/s
        private float _yaw;                        // current yaw degrees

        // ===== Animator =====
        private const bool DRIVE_SPEED_PARAM = true;
        private const float SPEED_SMOOTH = 10f;
        private double _smoothedSpeed = 0.0;

        // ===== Vision =====
        private VisionComponent _vision;
        private bool _isAlert;
        private bool _hasDealtDamage;

        // ===== Anchor to cancel root motion (optional) =====
        private Vec3 _anchorPos;

        // ===== Audio =====
        private const string SFX_FOOTSTEP_LOOP_PATH = "Resources/Audio/playerRun_02.wav";
        private const string SFX_ALERT_PATH = "Resources/Audio/enemyHurt_2.wav";
        private string _footLoopName;     // unique per entity
        private string _alertName;        // unique per entity
        private bool _footLoopPlaying = false;

        // Hysteresis so we don’t spam start/stop when speed hovers near the threshold
        private const float MOVE_START_SPEED = 0.25f;
        private const float MOVE_STOP_SPEED = 0.15f;

        // Simple restart cooldown (avoid quick re-triggers if multiple scripts touch audio)
        private float _footRestartCooldown = 0f;   // seconds
        private const float FOOT_RESTART_MIN_INTERVAL = 0.25f;

        private float _debugTimer;

        public void OnStart(string json)
        {
            if (!API.HasTransform(Entity))
            {
                API.Log("[PatrolEnemyController] Missing Transform.");
                return;
            }

            _yaw = API.GetRotation(Entity).Y;

            if (API.HasAnimator(Entity))
            {
                API.AnimatorPlay(Entity, "walking");
                if (DRIVE_SPEED_PARAM) API.AnimatorSetFloat(Entity, "Speed", 0f);
            }

            _anchorPos = API.GetPosition(Entity);

            // Vision hooks
            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(json);

            // Unique sound names per-entity so we can control each instance
            _footLoopName = "foot_" + Entity.ToString();
            _alertName = "alert_" + Entity.ToString();

            // Preload for snappy playback
            API.PreloadSound(_footLoopName, SFX_FOOTSTEP_LOOP_PATH, loop: true);
            API.PreloadSound(_alertName, SFX_ALERT_PATH, loop: false);
        }

        public void OnUpdate(float dt)
        {
            if (Entity == 0 || dt <= 0f) return;

            if (_footRestartCooldown > 0f)
                _footRestartCooldown -= dt;

            var v = API.GetLinearVelocity(Entity);
            float speedXZ = (float)Math.Sqrt(v.X * v.X + v.Z * v.Z);

            // Face velocity whenever not in alert tracking
            if (!_isAlert)
                FaceVelocity(dt, v.X, v.Z);

            // Drive animator speed
            if (API.HasAnimator(Entity) && DRIVE_SPEED_PARAM)
            {
                _smoothedSpeed += (speedXZ - _smoothedSpeed) * Min(1.0, SPEED_SMOOTH * dt);
                API.AnimatorSetFloat(Entity, "Speed", (float)_smoothedSpeed);
            }

            // Kill root motion drift + keep anchored
            if (LOCK_IN_PLACE)
            {
                API.SetLinearVelocity(Entity, new Vec3(0f, v.Y, 0f));
                var p = API.GetPosition(Entity);
                API.SetPosition(Entity, new Vec3(_anchorPos.X, p.Y, _anchorPos.Z));
            }

            // Footstep loop (3D positional)
            var pos = API.GetPosition(Entity);

            if (!_footLoopPlaying && speedXZ >= MOVE_START_SPEED && _footRestartCooldown <= 0f)
            {
                _footLoopPlaying = true;

                if (!API.IsSoundPlaying(_footLoopName))
                {
                    API.PlaySoundAt(_footLoopName, SFX_FOOTSTEP_LOOP_PATH, pos, loop: true);
                    API.SetSoundVolume(_footLoopName, 0.0f);
                }
                else
                {
                    // If some other system already started it, just sync position
                    API.SetSoundPosition(_footLoopName, pos);
                }
            }
            else if (_footLoopPlaying && speedXZ <= MOVE_STOP_SPEED)
            {
                _footLoopPlaying = false;
                if (API.IsSoundPlaying(_footLoopName))
                    API.StopSound(_footLoopName);

                _footRestartCooldown = FOOT_RESTART_MIN_INTERVAL;
            }

            if (_footLoopPlaying)
                API.SetSoundPosition(_footLoopName, pos);

            _vision?.OnUpdate(dt);

            // Debug
            _debugTimer += dt;
            if (_debugTimer >= 1f)
            {
                _debugTimer = 0f;
                var r = API.GetRotation(Entity);
                API.Log($"[PatrolEnemyController] yaw={_yaw:F1}°, rotY={r.Y:F1}°");
            }
        }

        // ----- Helpers -----

        private void FaceVelocity(float dt, float vx, float vz)
        {
            float speedXZ = (float)Math.Sqrt(vx * vx + vz * vz);
            if (speedXZ < _minSpeedToRotate) return;

            float baseYaw = ComputeYawFromVelocity(vx, vz); // single convention
            float targetYawDeg = baseYaw;                   // no auto-correction (prevents flips)

            float delta = Wrap180(targetYawDeg - _yaw);
            float maxStep = _rotationSpeedDeg * dt;

            _yaw = (Math.Abs(delta) <= maxStep) ? targetYawDeg : _yaw + Math.Sign(delta) * maxStep;
            _yaw = Wrap360(_yaw);
            API.SetRotationY(Entity, _yaw);
        }

        private float ComputeYawFromVelocity(float vx, float vz)
        {
            // Lock one convention to avoid “sometimes inverted”
            return WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(vx, -vz) * 180.0 / Math.PI)   // -Z forward
                : (float)(Math.Atan2(vx, vz) * 180.0 / Math.PI);  // +Z forward
        }

        private void OnPlayerDetected(ulong target, Vec3 pos)
        {
            _isAlert = true;

            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;

            float baseYaw = WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);

            _yaw = Wrap360(baseYaw);
            API.SetRotationY(Entity, _yaw);

            // Alert one-shot (positional)
            API.PlaySoundAt(_alertName, SFX_ALERT_PATH, self, loop: false);
            API.SetSoundVolume(_alertName, 0.3f);

            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }

        private void OnPlayerLost(ulong target, Vec3 lastPos)
        {
            _isAlert = false;
            _hasDealtDamage = false;
        }

        private void OnPlayerTracking(ulong target, Vec3 pos)
        {
            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;

            float baseYaw = WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);

            _yaw = Wrap360(baseYaw);
            API.SetRotationY(Entity, _yaw);
        }

        public void OnDestroy()
        {
            if (_footLoopPlaying && API.IsSoundPlaying(_footLoopName))
                API.StopSound(_footLoopName);

            _footLoopPlaying = false;
            _vision?.OnDestroy();
        }

        // ---- small utility methods (C# 7.3 safe) ----
        private static double Min(double a, double b) => (a < b) ? a : b;
        private static float Wrap360(float a) { while (a >= 360f) a -= 360f; while (a < 0f) a += 360f; return a; }
        private static float Wrap180(float a) { while (a > 180f) a -= 360f; while (a <= -180f) a += 360f; return a; }
    }
}
