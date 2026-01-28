using Boom;
using System;

namespace GameScripts
{
    public class PatrolEnemyController :IEnemyController
    {
        public ulong Entity;

        private const bool WORLD_FORWARD_IS_NEG_Z = false;
        private const bool LOCK_IN_PLACE = false;

        // Freeze
        private bool _isFrozen = false;
        private Vec3 _frozenPosition;

        private float _rotationSpeedDeg = 360f;
        private float _minSpeedToRotate = 0.15f;
        private float _yaw;

        private const bool DRIVE_SPEED_PARAM = true;
        private const float SPEED_SMOOTH = 10f;
        private double _smoothedSpeed = 0.0;

        private VisionComponent _vision;

        // NEW: Proximity detection
        private ProximityDetectionComponent _proximityDetection;

        private bool _isAlert;
        private bool _hasDealtDamage;

        private Vec3 _anchorPos;

        // ====== AUDIO ======
        [Boom.EditorExposed("Footstep Sound", "Sound played for enemy footsteps")]
        private string _footstepSoundPath = "Resources/Audio/playerRun_02.wav";

        [Boom.EditorExposed("Alert Sound", "Sound played when enemy detects player")]
        private string _alertSoundPath = "Resources/Audio/enemyHurt_2.wav";

        [Boom.EditorExposed("Detection", "For Enemy detection")]
        private bool EnemyDetection = true;

        private string _footBase;
        private string _alertName;

        // cadence settings
        private const float MOVE_START_SPEED = 0.25f;
        private const float MOVE_STOP_SPEED = 0.15f;
        private const float STEP_LENGTH_M = 0.7f;
        private const float MIN_INTERVAL_S = 0.5f;
        private const float MAX_INTERVAL_S = 2.0f;
        private const float VOL_BASE = 1.0f;
        private const float VOL_JITTER = 0.07f;

        private float _stepTimer = 0f;
        private bool _leftNext = true;
        private float _debugTimer;

        // detection damage reset
        private float _damageResetTimer = 0f;
        private const float DAMAGE_RESET_DELAY = 3.0f; // 3 seconds after damage

        public void OnStart(string json)
        {
            if (!API.HasTransform(Entity)) { API.Log("[PatrolEnemyController] Missing Transform."); return; }

            _yaw = API.GetRotation(Entity).Y;

            if (API.HasAnimator(Entity))
            {
                API.AnimatorPlay(Entity, "walking");
                if (DRIVE_SPEED_PARAM) API.AnimatorSetFloat(Entity, "Speed", 0f);
            }

            _anchorPos = API.GetPosition(Entity);

            // Initialize vision system
            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(json);

            // NEW: Initialize proximity detection
            _proximityDetection = new ProximityDetectionComponent { Entity = Entity };
            _proximityDetection.OnProximityDetected += OnProximityDetected;
            _proximityDetection.OnStart();
            // Optional: Configure
            // _proximityDetection.SetDetectionRadius(3.5f);
            // _proximityDetection.SetDetectionDuration(2.0f);

            _footBase = "foot_" + Entity.ToString();
            _alertName = "alert_" + Entity.ToString();

            // optional: preload step clip once (non-loop)
            API.PreloadSound(_footBase + "_L", _footstepSoundPath, loop: false);
            API.PreloadSound(_footBase + "_R", _footstepSoundPath, loop: false);
            API.PreloadSound(_alertName, _alertSoundPath, loop: false);

            // NEW: Register with PlayerManager
            PlayerManager.RegisterEnemy(this);

        }

        public void OnUpdate(float dt)
        {
            if (Entity == 0 || dt <= 0f) return;

            // --- FREEZE CHECK ---
            bool currentlyFrozen = FreezeManager.IsFrozen(API.GetPosition(Entity));

            if (currentlyFrozen)
            {
                if (!_isFrozen)
                {
                    _isFrozen = true;
                    _frozenPosition = API.GetPosition(Entity);

                    API.SetNavAgentActive(Entity, false);
                    API.SetLinearVelocity(Entity, new Vec3(0, 0, 0));

                    if (API.HasAnimator(Entity))
                    {
                        API.AnimatorSetFloat(Entity, "Speed", 0f);
                    }
                    _smoothedSpeed = 0.0;
                }

                API.TeleportRigidBody(Entity, _frozenPosition);
                API.SetLinearVelocity(Entity, new Vec3(0, 0, 0));

                return;
            }
            else
            {
                if (_isFrozen)
                {
                    _isFrozen = false;
                    API.SetNavAgentActive(Entity, true);
                }
            }

            // --- NORMAL LOGIC ---

            var v = API.GetLinearVelocity(Entity);
            float speedXZ = (float)Math.Sqrt(v.X * v.X + v.Z * v.Z);

            if (!_isAlert) FaceVelocity(dt, v.X, v.Z);

            if (API.HasAnimator(Entity) && DRIVE_SPEED_PARAM)
            {
                _smoothedSpeed += (speedXZ - _smoothedSpeed) * Min(1.0, SPEED_SMOOTH * dt);
                API.AnimatorSetFloat(Entity, "Speed", (float)_smoothedSpeed);
            }

            // ======= DISCRETE FOOTSTEPS =======
            bool grounded = API.IsColliding(Entity);
            bool moving = speedXZ >= MOVE_START_SPEED;

            if (grounded && moving)
            {
                float cadence = Math.Max(0.0001f, speedXZ / STEP_LENGTH_M);
                float interval = 1.0f / cadence;
                if (interval < MIN_INTERVAL_S) interval = MIN_INTERVAL_S;
                if (interval > MAX_INTERVAL_S) interval = MAX_INTERVAL_S;

                _stepTimer -= dt;
                if (_stepTimer <= 0f)
                {
                    var pos = API.GetPosition(Entity);

                    ulong playerEntity = PlayerMovement.GetPlayerEntity();
                    bool shouldPlayFootstep = true;

                    if (playerEntity != 0 && API.HasTransform(playerEntity))
                    {
                        var playerPos = API.GetPosition(playerEntity);
                        float verticalDistance = Math.Abs(pos.Y - playerPos.Y);
                        shouldPlayFootstep = verticalDistance < 10.0f;
                    }

                    if (shouldPlayFootstep)
                    {
                        string chName = _footBase + (_leftNext ? "_L" : "_R");
                        _leftNext = !_leftNext;

                        // play one-shot at position
                        API.PlaySoundAt(chName, _footstepSoundPath, pos, loop: false);

                        float jitter = (float)(Random01() * 2.0 - 1.0) * VOL_JITTER;
                        float vol = Clamp01(VOL_BASE + jitter);
                        API.SetSoundVolume(chName, vol);
                        API.Set3DMinMaxDistance(chName, 6.0f, 30.0f);
                    }

                    _stepTimer += interval;
                }
            }
            else
            {
                _stepTimer = 0f;
            }

            // Update vision (always active) and proximity (only if detection enabled)
            _vision?.OnUpdate(dt);
            if (EnemyDetection)
            {
                _proximityDetection?.OnUpdate(dt);
            }

            _debugTimer += dt;
            if (_debugTimer >= 1f)
            {
                _debugTimer = 0f;
                var r = API.GetRotation(Entity);
                API.Log($"[PatrolEnemyController] yaw={_yaw:F1}°, rotY={r.Y:F1}°, speed={speedXZ:F2} m/s");
            }

            if (_hasDealtDamage)
            {
                _damageResetTimer += dt;
                if (_damageResetTimer >= DAMAGE_RESET_DELAY)
                {
                    _hasDealtDamage = false;
                    _damageResetTimer = 0f;
                    API.Log("[PatrolEnemyController] Damage flag reset - can damage again");
                }
            }
        }

        private void FaceVelocity(float dt, float vx, float vz)
        {
            float speedXZ = (float)Math.Sqrt(vx * vx + vz * vz);
            if (speedXZ < _minSpeedToRotate) return;

            float baseYaw = ComputeYawFromVelocity(vx, vz);
            float targetYawDeg = baseYaw;

            float delta = Wrap180(targetYawDeg - _yaw);
            float maxStep = _rotationSpeedDeg * dt;

            _yaw = (Math.Abs(delta) <= maxStep) ? targetYawDeg : _yaw + Math.Sign(delta) * maxStep;
            _yaw = Wrap360(_yaw);
            API.SetRotationY(Entity, _yaw);
        }

        private float ComputeYawFromVelocity(float vx, float vz)
        {
            return WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(vx, -vz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(vx, vz) * 180.0 / Math.PI);
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

            API.PlaySoundAt(_alertName, _alertSoundPath, self, loop: false);
            API.SetSoundVolume(_alertName, 0.5f);
            API.Set3DMinMaxDistance(_alertName, 1.0f, 25.0f);

            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                _damageResetTimer = 0f;  // Start timer
                API.Log($"[PatrolEnemyController] Dealing damage (vision detection)!");
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }

        private void OnPlayerLost(ulong t, Vec3 lastPos)
        {
            _isAlert = false;
            _hasDealtDamage = false;

            // NEW: Reset proximity when player lost
            _proximityDetection?.ResetDetection();
        }

        private void OnPlayerTracking(ulong t, Vec3 pos)
        {
            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;
            float baseYaw = WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);
            _yaw = Wrap360(baseYaw);
            API.SetRotationY(Entity, _yaw);
        }

        // === NEW: PROXIMITY DETECTION HANDLER ===
        private void OnProximityDetected(ulong target, Vec3 pos)
        {
            // Check if detection is enabled
            if (!EnemyDetection)
            {
                API.Log("[PatrolEnemyController] Proximity detection disabled - ignoring detection event");
                return;
            }

            API.Log(">>> PATROL ENEMY ALERTED BY PROXIMITY! <<<");

            _isAlert = true;
            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;
            float baseYaw = WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);
            _yaw = Wrap360(baseYaw);
            API.SetRotationY(Entity, _yaw);

            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                _damageResetTimer = 0f;  // Start timer
                API.Log($"[PatrolEnemyController] Dealing damage (proximity detection)!");
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }

        // NEW: Implement interface method
        public void OnPlayerRespawned()
        {
            // Force reset all states
            _hasDealtDamage = false;
            _damageResetTimer = 0f;
            _isAlert = false;

            // Reset proximity
            _proximityDetection?.ResetDetection();

            API.Log("[PatrolEnemyController] Player respawned - all states reset");
        }

        public void OnDestroy()
        {
            _vision?.OnDestroy();
            _proximityDetection?.OnDestroy();

            // NEW: Unregister from PlayerManager
            PlayerManager.UnregisterEnemy(this);
        }

        // utils
        private static double Min(double a, double b) => (a < b) ? a : b;
        private static float Wrap360(float a) { while (a >= 360f) a -= 360f; while (a < 0f) a += 360f; return a; }
        private static float Wrap180(float a) { while (a > 180f) a -= 360f; while (a <= -180f) a += 360f; return a; }
        private static double Random01() => new Random().NextDouble();
        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);
    }
}