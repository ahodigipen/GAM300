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

        // Static registry for spotlight lookup
        private static System.Collections.Generic.Dictionary<string, PatrolEnemyController> s_PatrolEnemies
            = new System.Collections.Generic.Dictionary<string, PatrolEnemyController>();

        /// <summary>
        /// Get current yaw rotation in degrees
        /// </summary>
        public float GetYaw() => _yaw;

        private const bool DRIVE_SPEED_PARAM = true;
        private const float SPEED_SMOOTH = 10f;
        private double _smoothedSpeed = 0.0;

        private VisionComponent _vision;

        // NEW: Proximity detection
        private ProximityDetectionComponent _proximityDetection;

        private bool _isAlert;
        private bool _hasDealtDamage;

        private Vec3 _anchorPos;

        // Animation position tracking - prevents animation from drifting outside collider
        private Vec3 _lastPhysicsPosition;
        private bool _positionInitialized = false;
        // ====== ALERT VIDEO UI ======
        [Boom.EditorExposed("Alert Video Name", "Name of the entity with VideoComponent to show on alert")]
        private string _alertVideoName = "AlertVideo";

        [Boom.EditorExposed("Alert Video Offset Y", "Vertical offset for the alert video")]
        private float _alertVideoOffsetY = 5.0f;

        // Shared static video entity
        private static ulong _sharedVideoEntity = 0;
        private static ulong _videoOwnerID = 0;
        private static Vec3 _sharedVideoBaseScale = new Vec3(1, 1, 1);
        
        // Local state
        private bool _isMyVideoVisible = false;

        // ====== AUDIO ======
        [Boom.EditorExposed("Footstep Sound", "Sound played for enemy footsteps")]
        private string _footstepSoundPath = "Resources/Audio/footstep_stone_1.wav";
        private string _footstepSoundPath2 = "Resources/Audio/footstep_stone_2.wav";
        private string _footstepSoundPath3 = "Resources/Audio/footstep_stone_3.wav";
        private string _footstepSoundPath4 = "Resources/Audio/footstep_stone_4.wav";
        private string _footstepSoundPath5 = "Resources/Audio/footstep_stone_5.wav";
        private string _footstepSoundPath6 = "Resources/Audio/footstep_stone_6.wav";
        private string _footstepSoundPath7 = "Resources/Audio/footstep_stone_7.wav";
        private string _footstepSoundPath8 = "Resources/Audio/footstep_stone_8.wav";
        private string _footstepSoundPath9 = "Resources/Audio/footstep_stone_9.wav";
        private string _footstepSoundPath10 = "Resources/Audio/footstep_stone_10.wav";


        [Boom.EditorExposed("Alert Sound", "Sound played when enemy detects player")]
        private string _alertSoundPath = "Resources/Audio/VO_Patrol_Alert_020.wav";
        private string _alertSoundPath2 = "Resources/Audio/VO_Patrol_Alert_030.wav";
        private string _alertSoundPath3 = "Resources/Audio/VO_Patrol_Alert_021.wav";
        private string _alertSoundPath4 = "Resources/Audio/VO_Patrol_Alert_024.wav";
        private string _alertSoundPath5 = "Resources/Audio/VO_Patrol_Alert_027.wav";

        [Boom.EditorExposed("Grunts", "Grunt Sounds for real experience")]
        private string _gruntSoundPath = "Resources/Audio/VO_Patrol_001.wav";
        private string _gruntSoundPath2 = "Resources/Audio/VO_Patrol_002.wav";
        private string _gruntSoundPath3 = "Resources/Audio/VO_Patrol_003.wav";
        private string _gruntSoundPath4 = "Resources/Audio/VO_Patrol_004.wav";
        private string _gruntSoundPath5 = "Resources/Audio/VO_Patrol_005.wav";
        private string _gruntSoundPath6 = "Resources/Audio/VO_Patrol_006.wav";
        private string _gruntSoundPath7 = "Resources/Audio/VO_Patrol_008.wav";
        private string _gruntSoundPath8 = "Resources/Audio/VO_Patrol_009.wav";
        private string _gruntSoundPath9 = "Resources/Audio/VO_Patrol_012.wav";
        private string _gruntSoundPath10 = "Resources/Audio/VO_Patrol_015.wav";
        private string _gruntSoundPath11 = "Resources/Audio/VO_Patrol_016.wav";
        private string _gruntSoundPath12 = "Resources/Audio/VO_Patrol_017.wav";
        private string _gruntSoundPath13 = "Resources/Audio/VO_Patrol_018.wav";
        private string _gruntSoundPath14 = "Resources/Audio/VO_Patrol_019.wav";
        private string _gruntSoundPath15 = "Resources/Audio/VO_Patrol_031.wav";

        [Boom.EditorExposed("Detection", "For Enemy detection")]
        private bool EnemyDetection = true;

        [Boom.EditorExposed("Proximity Radius", "How close player must be to trigger proximity detection", 0.5f, 10f, true)]
        private float _proximityRadius = 5.0f;

        [Boom.EditorExposed("Proximity Duration", "How long player must stay close to trigger detection", 0.1f, 10f, true)]
        private float _proximityDuration = 5.0f;

        [Boom.EditorExposed("Proximity Vertical Tolerance", "Vertical range for proximity detection", 0.5f, 5f, true)]
        private float _proximityVerticalTolerance = 2.5f;

        [Boom.EditorExposed("Proximity Debug Log", "Enable debug logging for proximity detection")]
        private bool _proximityDebugLog = false;

        private string _footBase;
        private string _alertName;
        private string _gruntName;
        private bool _alertSoundPlayed = false; // Track if alert sound was already played this detection

        private Random _random = new Random();

        // Grunt timing settings
        private const float GRUNT_MIN_INTERVAL = 4.0f;  // Minimum seconds between grunts
        private const float GRUNT_MAX_INTERVAL = 10.0f; // Maximum seconds between grunts
        private float _gruntTimer = 0f;
        private float _nextGruntTime = 0f;

        // cadence settings
        private const float MOVE_START_SPEED = 0.25f;
        private const float MOVE_STOP_SPEED = 0.15f;
        private const float STEP_LENGTH_M = 0.7f;
        private const float MIN_INTERVAL_S = 0.5f;
        private const float MAX_INTERVAL_S = 2.0f;
        private const float VOL_BASE = 1.0f;
        private const float VOL_JITTER = 0.07f;

        private float _stepTimer = 0f;
        private float _debugTimer;

        // detection damage reset
        private float _damageResetTimer = 0f;
        private const float DAMAGE_RESET_DELAY = 3.0f; // 3 seconds after damage

        // Entity name for spotlight lookup (matches SpotlightFollower's targetName)
        [Boom.EditorExposed("Entity Name", "Name of this patrol enemy for spotlight lookup")]
        private string _entityName = "Patrol_1";

        [Boom.EditorExposed("Spotlight Name", "Name of the spotlight entity to control")]
        private string _spotlightName = "Patrol_1_Spotlight";

        // Gravity
        private float _verticalVelocity = 0f;
        private const float GRAVITY = 50f;
        private const float GROUND_STICK = -5.0f;

        // Anti-oscillation for slopes
        private float _lastVelocityX = 0f;
        private float _lastVelocityZ = 0f;
        private float _directionStableTimer = 0f;
        private const float DIRECTION_CHANGE_COOLDOWN = 0.3f; // Minimum time before allowing direction change
        private const float VELOCITY_CHANGE_THRESHOLD = 0.5f; // How much velocity can change before we consider it a direction change

#pragma warning disable CS0414 // Field is assigned but its value is never used (editor-exposed)
        [Boom.EditorExposed("Spotlight Offset Y", "Height offset for spotlight above entity", 0f, 10f, true)]
        private float _spotlightOffsetY = 2.0f;
#pragma warning restore CS0414

        // Cached spotlight entity ID
        private ulong _spotlightEntity = 0;

        public void OnStart(string json)
        {
            // Detect stale registry from previous play session by checking if our key exists with different instance
            if (s_PatrolEnemies.TryGetValue(_entityName, out PatrolEnemyController existing) && existing != this)
            {
                API.Log("[PatrolEnemyController] Detected stale registry - clearing for new session");
                s_PatrolEnemies.Clear();
                // Also reset other static variables
                _sharedVideoEntity = 0;
                _videoOwnerID = 0;
                _sharedVideoBaseScale = new Vec3(1, 1, 1);
            }

            if (!API.HasTransform(Entity)) { //("[PatrolEnemyController] Missing Transform."); return;
                                             }

            _yaw = API.GetRotation(Entity).Y;

            if (API.HasAnimator(Entity))
            {
                // IMPORTANT: Disable root motion FIRST before playing any animation
                // This prevents the character from drifting outside the collider
                API.AnimatorSetBool(Entity, "ApplyRootMotion", false);

                API.AnimatorPlay(Entity, "walking");
                if (DRIVE_SPEED_PARAM) API.AnimatorSetFloat(Entity, "Speed", 0f);
            }

            _anchorPos = API.GetPosition(Entity);
            _lastPhysicsPosition = _anchorPos;
            _positionInitialized = true;

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
            
            // Configure settings from editor-exposed fields
            _proximityDetection.DetectionRadius = _proximityRadius;
            _proximityDetection.DetectionDuration = _proximityDuration;
            _proximityDetection.VerticalTolerance = _proximityVerticalTolerance;
            _proximityDetection.DebugLog = _proximityDebugLog;

            _footBase = "foot_" + Entity.ToString();
            _alertName = "alert_" + Entity.ToString();
            _gruntName = "grunt_" + Entity.ToString();

            // Find spotlight entity by name
            if (!string.IsNullOrEmpty(_spotlightName))
            {
                _spotlightEntity = API.FindEntity(_spotlightName);
                if (_spotlightEntity != 0)
                {
                    API.Log($"[PatrolEnemyController] Found spotlight entity: '{_spotlightName}' (ID: {_spotlightEntity})");
                }
                else
                {
                    API.Log($"[PatrolEnemyController] WARNING: Could not find spotlight entity: '{_spotlightName}'");
                }
            }

            // Preload all footstep sound variants
            PreloadFootstepSounds();
            // Preload all alert sound variants
            PreloadAlertSounds();
            // Preload all grunt sound variants
            PreloadGruntSounds();

            // Initialize first grunt timer with random delay
            _nextGruntTime = GRUNT_MIN_INTERVAL + (float)(_random.NextDouble() * (GRUNT_MAX_INTERVAL - GRUNT_MIN_INTERVAL));

            // NEW: Register with PlayerManager
            PlayerManager.RegisterEnemy(this);

            // Register for spotlight lookup
            s_PatrolEnemies[_entityName] = this;

            // Initialize Alert Video (Shared)
            if (_sharedVideoEntity == 0 && !string.IsNullOrEmpty(_alertVideoName))
            {
                _sharedVideoEntity = API.FindEntity(_alertVideoName);
                if (_sharedVideoEntity != 0)
                {
                    API.Log($"[PatrolEnemyController] Found shared alert video entity: '{_alertVideoName}' (ID: {_sharedVideoEntity})");
                    _sharedVideoBaseScale = API.GetScale(_sharedVideoEntity);
                    
                    // Improved scale check - if it's too small/hidden, default to 1s
                    if (Math.Abs(_sharedVideoBaseScale.X) < 0.01f || Math.Abs(_sharedVideoBaseScale.Y) < 0.01f) 
                    {
                        API.Log($"[PatrolEnemyController] Video entity scale was near zero ({_sharedVideoBaseScale.X:F2}, {_sharedVideoBaseScale.Y:F2}). Defaulting to (1,1,1).");
                        _sharedVideoBaseScale = new Vec3(1, 1, 1);
                    }
                    else
                    {
                         API.Log($"[PatrolEnemyController] Captured base scale: {_sharedVideoBaseScale.X:F2}, {_sharedVideoBaseScale.Y:F2}, {_sharedVideoBaseScale.Z:F2}");
                    }
                        
                    // Default State: Hidden
                    API.SetScale(_sharedVideoEntity, new Vec3(0, 0, 0));
                }
                else
                {
                    API.Log($"[PatrolEnemyController] ERROR: Could not find entity named '{_alertVideoName}'!");
                }
            }

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
                    _verticalVelocity = 0f;

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

            // Store position at start of frame for drift detection
            Vec3 frameStartPos = API.GetPosition(Entity);

            var v = API.GetLinearVelocity(Entity);
            float speedXZ = (float)Math.Sqrt(v.X * v.X + v.Z * v.Z);

            // --- Improved Gravity Handling for Slopes ---
            // Use a larger probe distance for better slope detection
            bool isGrounded = API.IsGrounded(Entity, 1.5f);

            // Additional slope check - raycast downward to detect ground beneath
            bool onSlope = false;
            Vec3 currentPos = API.GetPosition(Entity);
            if (!isGrounded)
            {
                // Check if we're just slightly above ground (on a slope)
                Vec3 slopeProbeFrom = new Vec3(currentPos.X, currentPos.Y + 0.2f, currentPos.Z);
                Vec3 slopeProbeTo = new Vec3(currentPos.X, currentPos.Y - 0.8f, currentPos.Z);
                onSlope = API.Linecast(slopeProbeFrom, slopeProbeTo, Entity);
            }

            if (!isGrounded && !onSlope)
            {
                // Truly airborne: Physics engine handles gravity automatically because it's a DYNAMIC body.
                // We just ensure our internal tracker doesn't interfere.
                _verticalVelocity = v.Y; 
            }
            else
            {
                // Grounded or on slope: Apply a small downward velocity to stay glued to the floor/slopes.
                // This is safer than manual teleporting which causes phasing at low FPS.
                if (v.Y > 0.1f) {
                     // If something (like a bump) pushed us up, let it happen
                } else {
                    Vec3 stickVel = new Vec3(v.X, -5.0f, v.Z);
                    API.SetLinearVelocity(Entity, stickVel);
                }
                _verticalVelocity = 0f;
            }

            // Only face velocity if moving fast enough AND not rapidly changing direction
            if (!_isAlert)
            {
                FaceVelocitySmooth(dt, v.X, v.Z, speedXZ);
            }

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
                        PlayRandomFootstep(pos);
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
                if (_proximityDetection != null)
                {
                    _proximityDetection.DetectionRadius = _proximityRadius;
                    _proximityDetection.DetectionDuration = _proximityDuration;
                    _proximityDetection.VerticalTolerance = _proximityVerticalTolerance;
                    _proximityDetection.DebugLog = _proximityDebugLog;
                }
                _proximityDetection?.OnUpdate(dt);
            }

            // Update Alert Video (Shared)
            if (_sharedVideoEntity != 0)
            {
                // Visibility Update
                bool isInProximity = (_proximityDetection != null && _proximityDetection.IsPlayerInProximity());
                // Only show alert video for proximity detection, not for direct vision alert
                bool isVisible = isInProximity && !Entry.IsPlayerDead;

                if (isVisible)
                {
                    // Claim ownership
                    _videoOwnerID = Entity;

                    // 1. Position Update (Follow)
                    Vec3 myPos = API.GetPosition(Entity);
                    API.SetPosition(_sharedVideoEntity, new Vec3(myPos.X, myPos.Y + _alertVideoOffsetY, myPos.Z));

                    // 2. Rotation Update (Face Camera)
                    float camYaw = API.GetThirdPersonCameraYaw();
                    float billboardYaw = camYaw + 180.0f;
                    API.SetRotationY(_sharedVideoEntity, billboardYaw);

                    if (!_isMyVideoVisible)
                    {
                        API.Log($"[PatrolEnemyController] Enemy {Entity} showing shared video.");
                        API.SetScale(_sharedVideoEntity, _sharedVideoBaseScale);
                        API.PlayVideo(_sharedVideoEntity);
                        _isMyVideoVisible = true;
                    }
                }
                else
                {
                    if (_isMyVideoVisible)
                    {
                        // Only hide if I am the owner
                        if (_videoOwnerID == Entity)
                        {
                            API.Log($"[PatrolEnemyController] Enemy {Entity} hiding shared video.");
                            API.StopVideo(_sharedVideoEntity);
                            API.SetScale(_sharedVideoEntity, new Vec3(0, 0, 0));
                            _videoOwnerID = 0;
                        }
                        _isMyVideoVisible = false;
                    }
                    // ======= OCCASIONAL GRUNT SOUNDS =======
                    // Only grunt while patrolling (not alert) and moving
                    if (!_isAlert && moving)
                    {
                        _gruntTimer += dt;
                        if (_gruntTimer >= _nextGruntTime)
                        {
                            var pos = API.GetPosition(Entity);
                            PlayRandomGrunt(pos);

                            // Reset timer with new random interval
                            _gruntTimer = 0f;
                            _nextGruntTime = GRUNT_MIN_INTERVAL + (float)(_random.NextDouble() * (GRUNT_MAX_INTERVAL - GRUNT_MIN_INTERVAL));
                        }
                    }

                    _debugTimer += dt;
                    if (_debugTimer >= 1f)
                    {
                        _debugTimer = 0f;
                        var r = API.GetRotation(Entity);
                        //($"[PatrolEnemyController] yaw={_yaw:F1}°, rotY={r.Y:F1}°, speed={speedXZ:F2} m/s");
                    }

                    if (_hasDealtDamage)
                    {
                        _damageResetTimer += dt;
                        if (_damageResetTimer >= DAMAGE_RESET_DELAY)
                        {
                            _hasDealtDamage = false;
                            _damageResetTimer = 0f;
                            //("[PatrolEnemyController] Damage flag reset - can damage again");
                        }
                    }
                }
            }

            // === ANIMATION DRIFT CORRECTION ===
            // Some animations have root motion baked into non-root bones (like hips/pelvis)
            // which doesn't get stripped by ApplyRootMotion=false. This code detects and
            // corrects any unexpected drift from animation.
            // Animation drift correction - with slope-friendly threshold
            // Animation drift correction - with slope-friendly threshold
            if (_positionInitialized)
            {
                Vec3 driftCheckPos = API.GetPosition(Entity);  // RENAMED from currentPos

                // Calculate expected movement from NavAgent
                Vec3 expectedDelta = new Vec3(
                    driftCheckPos.X - frameStartPos.X,
                    0f, // Ignore Y for drift calculation
                    driftCheckPos.Z - frameStartPos.Z
                );

                // Calculate actual physics movement  
                float actualDeltaX = driftCheckPos.X - _lastPhysicsPosition.X;
                float actualDeltaZ = driftCheckPos.Z - _lastPhysicsPosition.Z;

                float expectedDeltaX = driftCheckPos.X - frameStartPos.X;
                float expectedDeltaZ = driftCheckPos.Z - frameStartPos.Z;

                // Drift = actual movement minus expected (NavAgent) movement
                float driftX = actualDeltaX - expectedDeltaX;
                float driftZ = actualDeltaZ - expectedDeltaZ;
                float driftMagnitude = (float)Math.Sqrt(driftX * driftX + driftZ * driftZ);

                // INCREASED threshold - more forgiving on slopes
                const float DRIFT_THRESHOLD = 0.1f;

                // Only correct drift if grounded and not on slope, and drift is significant
                bool shouldCorrectDrift = driftMagnitude > DRIFT_THRESHOLD &&
                                           isGrounded &&
                                           !onSlope &&
                                           _verticalVelocity >= -0.5f;

                if (shouldCorrectDrift)
                {
                    Vec3 correctedPos = new Vec3(
                        driftCheckPos.X - driftX,
                        driftCheckPos.Y,
                        driftCheckPos.Z - driftZ
                    );
                    API.TeleportRigidBody(Entity, correctedPos);
                }

                // Update tracking for next frame
                _lastPhysicsPosition = API.GetPosition(Entity);
            }
            else
            {
                _lastPhysicsPosition = API.GetPosition(Entity);
                _positionInitialized = true;
            }
        }
        private void FaceVelocitySmooth(float dt, float vx, float vz, float speedXZ)
        {
            // Don't rotate if moving too slowly
            if (speedXZ < _minSpeedToRotate)
            {
                _directionStableTimer = 0f;
                return;
            }

            // Check for rapid direction changes (oscillation detection)
            float velocityChangeX = Math.Abs(vx - _lastVelocityX);
            float velocityChangeZ = Math.Abs(vz - _lastVelocityZ);
            float totalVelocityChange = velocityChangeX + velocityChangeZ;

            // If velocity changed significantly, start cooldown
            if (totalVelocityChange > VELOCITY_CHANGE_THRESHOLD)
            {
                _directionStableTimer = DIRECTION_CHANGE_COOLDOWN;
            }

            // Update last velocity
            _lastVelocityX = vx;
            _lastVelocityZ = vz;

            // If in cooldown, don't change facing direction
            if (_directionStableTimer > 0f)
            {
                _directionStableTimer -= dt;
                return;
            }

            float targetYawDeg = ComputeYawFromVelocity(vx, vz);
            
            // Frame-rate independent smoothing
            // We use a lerp-like approach: current = current + (target - current) * (1 - exp(-speed * dt))
            float delta = Wrap180(targetYawDeg - _yaw);
            
            // Adjust rotation speed based on the size of the turn
            float turnSpeedMultiplier = 1.0f;
            if (Math.Abs(delta) > 90f) turnSpeedMultiplier = 1.5f; // Turn faster for large angles
            
            float step = delta * (1f - (float)Math.Exp(-10f * turnSpeedMultiplier * dt));
            
            // Clamp step to max rotation speed to prevent teleporting rotation
            float maxStep = _rotationSpeedDeg * dt;
            if (Math.Abs(step) > maxStep) step = Math.Sign(step) * maxStep;

            _yaw = Wrap360(_yaw + step);
            SetRotationAndSpotlight(_yaw);
        }

        // Helper to set entity rotation
        private void SetRotationAndSpotlight(float yaw)
        {
            API.SetRotationY(Entity, yaw);
            // Spotlight rotation is handled by SpotlightFollower with isPatrolEnemy=true
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

            // Set spotlight to alert (red) color
            var spotlight = SpotlightFollower.GetByTargetName(_entityName);
            if (spotlight != null)
            {
                spotlight.SetAlert(true);
            }

            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;
            float baseYaw = WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);
            _yaw = Wrap360(baseYaw);
            SetRotationAndSpotlight(_yaw);

            PlayRandomAlertSound(self);

            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                _damageResetTimer = 0f;  // Start timer
                //($"[PatrolEnemyController] Dealing damage (vision detection)!");
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }

        private void OnPlayerLost(ulong t, Vec3 lastPos)
        {
            _isAlert = false;
            _hasDealtDamage = false;
            _alertSoundPlayed = false; // Reset so alert can play again next detection

            // Reset spotlight to original color
            var spotlight = SpotlightFollower.GetByTargetName(_entityName);
            if (spotlight != null)
            {
                spotlight.SetAlert(false);
            }

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
            SetRotationAndSpotlight(_yaw);
        }

        // === NEW: PROXIMITY DETECTION HANDLER ===
        private void OnProximityDetected(ulong target, Vec3 pos)
        {
            // Check if detection is enabled
            if (!EnemyDetection)
            {
                //("[PatrolEnemyController] Proximity detection disabled - ignoring detection event");
                return;
            }

            //(">>> PATROL ENEMY ALERTED BY PROXIMITY! <<<");

            _isAlert = true;
            var self = API.GetPosition(Entity);
            float dx = pos.X - self.X, dz = pos.Z - self.Z;
            float baseYaw = WORLD_FORWARD_IS_NEG_Z
                ? (float)(Math.Atan2(dx, -dz) * 180.0 / Math.PI)
                : (float)(Math.Atan2(dx, dz) * 180.0 / Math.PI);
            _yaw = Wrap360(baseYaw);
            SetRotationAndSpotlight(_yaw);

            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                _damageResetTimer = 0f;  // Start timer
                //($"[PatrolEnemyController] Dealing damage (proximity detection)!");
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
            _alertSoundPlayed = false; // Reset so alert can play again

            // Reset proximity
            _proximityDetection?.ResetDetection();
            _verticalVelocity = 0f;

            // Hide alert video if active
            if (_isMyVideoVisible)
            {
                if (_sharedVideoEntity != 0 && _videoOwnerID == Entity)
                {
                    API.StopVideo(_sharedVideoEntity);
                    API.SetScale(_sharedVideoEntity, new Vec3(0, 0, 0));
                    _videoOwnerID = 0;
                }
                _isMyVideoVisible = false;
            }

            //("[PatrolEnemyController] Player respawned - all states reset");
            // Reset position to anchor
            API.SetNavAgentPosition(Entity, _anchorPos);

            //("[PatrolEnemyController] Player respawned - all states reset and teleported to anchor");
        }

        public void OnDestroy()
        {
            _vision?.OnDestroy();
            _proximityDetection?.OnDestroy();

            // NEW: Unregister from PlayerManager
            PlayerManager.UnregisterEnemy(this);

            // Unregister from spotlight lookup
            if (s_PatrolEnemies.ContainsKey(_entityName))
            {
                s_PatrolEnemies.Remove(_entityName);
            }
        }

        /// <summary>
        /// Get PatrolEnemyController by entity name (for spotlight lookup)
        /// </summary>
        public static PatrolEnemyController GetByName(string name)
        {
            if (s_PatrolEnemies.TryGetValue(name, out PatrolEnemyController controller))
            {
                return controller;
            }
            return null;
        }

        /// <summary>
        /// Clear the registry for a new play session. Call this when stopping play mode.
        /// </summary>
        public static void ClearRegistry()
        {
            s_PatrolEnemies.Clear();
            _sharedVideoEntity = 0;
            _videoOwnerID = 0;
            _sharedVideoBaseScale = new Vec3(1, 1, 1);
            API.Log("[PatrolEnemyController] Registry cleared");
        }

        // ====== AUDIO HELPER METHODS ======
        private string[] GetFootstepSounds()
        {
            return new string[] {
                _footstepSoundPath, _footstepSoundPath2, _footstepSoundPath3, _footstepSoundPath4, _footstepSoundPath5,
                _footstepSoundPath6, _footstepSoundPath7, _footstepSoundPath8, _footstepSoundPath9, _footstepSoundPath10
            };
        }

        private string[] GetAlertSounds()
        {
            return new string[] {
                _alertSoundPath, _alertSoundPath2, _alertSoundPath3, _alertSoundPath4, _alertSoundPath5
            };
        }

        private void PreloadFootstepSounds()
        {
            string[] sounds = GetFootstepSounds();
            for (int i = 0; i < sounds.Length; i++)
            {
                string soundName = _footBase + "_" + i;
                API.PreloadSound(soundName, sounds[i], loop: false);
            }
        }

        private void PreloadAlertSounds()
        {
            string[] sounds = GetAlertSounds();
            for (int i = 0; i < sounds.Length; i++)
            {
                string soundName = _alertName + "_" + i;
                API.PreloadSound(soundName, sounds[i], loop: false);
            }
        }

        private void PlayRandomFootstep(Vec3 position)
        {
            string[] sounds = GetFootstepSounds();
            int index = _random.Next(sounds.Length);
            string soundName = _footBase + "_" + index;

            API.PlaySoundAt(soundName, sounds[index], position, loop: false);

            float jitter = (float)(_random.NextDouble() * 2.0 - 1.0) * VOL_JITTER;
            float vol = Clamp01(VOL_BASE + jitter);
            API.SetSoundVolume(soundName, vol);
            API.Set3DMinMaxDistance(soundName, 6.0f, 30.0f);
        }

        private void PlayRandomAlertSound(Vec3 position)
        {
            if (_alertSoundPlayed) return; // Only play once per detection

            string[] sounds = GetAlertSounds();
            int index = _random.Next(sounds.Length);
            string soundName = _alertName + "_" + index;

            API.PlaySoundAt(soundName, sounds[index], position, loop: false);
            API.SetSoundVolume(soundName, 0.5f);
            API.Set3DMinMaxDistance(soundName, 1.0f, 25.0f);

            _alertSoundPlayed = true;
        }

        private string[] GetGruntSounds()
        {
            return new string[] {
                _gruntSoundPath, _gruntSoundPath2, _gruntSoundPath3, _gruntSoundPath4, _gruntSoundPath5,
                _gruntSoundPath6, _gruntSoundPath7, _gruntSoundPath8, _gruntSoundPath9, _gruntSoundPath10,
                _gruntSoundPath11, _gruntSoundPath12, _gruntSoundPath13, _gruntSoundPath14, _gruntSoundPath15
            };
        }

        private void PreloadGruntSounds()
        {
            string[] sounds = GetGruntSounds();
            for (int i = 0; i < sounds.Length; i++)
            {
                string soundName = _gruntName + "_" + i;
                API.PreloadSound(soundName, sounds[i], loop: false);
            }
        }

        private void PlayRandomGrunt(Vec3 position)
        {
            string[] sounds = GetGruntSounds();
            int index = _random.Next(sounds.Length);
            string soundName = _gruntName + "_" + index;

            API.PlaySoundAt(soundName, sounds[index], position, loop: false);
            API.SetSoundVolume(soundName, 0.6f);
            API.Set3DMinMaxDistance(soundName, 3.0f, 20.0f);
        }

        // Use a raycast probe for better ground detection on slopes
        private bool IsGroundedOnSlope(Vec3 pos)
        {
            float probeDistance = 0.5f; // Adjust based on your enemy height
            Vec3 from = new Vec3(pos.X, pos.Y + 0.1f, pos.Z);
            Vec3 to = new Vec3(pos.X, pos.Y - probeDistance, pos.Z);
            return API.Linecast(from, to, Entity);
        }


        // utils
        private static double Min(double a, double b) => (a < b) ? a : b;
        private static float Wrap360(float a) { while (a >= 360f) a -= 360f; while (a < 0f) a += 360f; return a; }
        private static float Wrap180(float a) { while (a > 180f) a -= 360f; while (a <= -180f) a += 360f; return a; }
        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);
    }
}