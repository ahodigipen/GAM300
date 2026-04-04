using System;
using System.Threading;
using Boom;

namespace GameScripts
{
    public class EnemyController : IEnemyController
    {
        public ulong Entity;

        // Rotation parameters
        private float _rotationTimer = 0f;

        [Boom.EditorExposed("Rotation Interval", "Time between random rotations in seconds", 0.5f, 10f, true)]
        private float _rotationInterval = 2f;

        private float _initialYRotation = 0f;
        private float _currentYRotation = 0f;
        private float _targetYRotation = 0f;

        [Boom.EditorExposed("Rotation Speed", "Degrees per second for smooth rotation", 10f, 360f, true)]
        private float _rotationSpeed = 90f; // Degrees per second for smooth rotation

        [Boom.EditorExposed("Rotation Angle", "Degrees to rotate each turn")]
        private float _rotationAngle = 90f; // Degrees to rotate per turn (configurable per sentry)

        private bool _isRotating = false;
        private const string TURN_SOUND_ID = "enemy_turn";

        // ===== Audio Settings =====
        [Boom.EditorExposed("Turn Sound 1", "First rotation sound variant")]
        private string _turnSoundPath1 = "Resources/Audio/StatueTurn01.wav";

        [Boom.EditorExposed("Turn Sound 2", "Second rotation sound variant")]
        private string _turnSoundPath2 = "Resources/Audio/StatueTurn02.wav";

        [Boom.EditorExposed("Turn Sound 3", "Third rotation sound variant")]
        private string _turnSoundPath3 = "Resources/Audio/StatueTurn03.wav";

        [Boom.EditorExposed("Alert Sound", "Sound played when enemy detects player")]
        private string _alertSoundPath = "Resources/Audio/VO_Statue_003.wav";
        private string _alertSoundPath2 = "Resources/Audio/VO_Statue_004.wav";
        private string _alertSoundPath3 = "Resources/Audio/VO_Statue_007.wav";
        private string _alertSoundPath4 = "Resources/Audio/VO_Statue_008.wav";
        private string _alertSoundPath5 = "Resources/Audio/VO_Statue_009.wav";
        private string _alertSoundPath6 = "Resources/Audio/VO_Statue_010.wav";
        private string _alertSoundPath7 = "Resources/Audio/VO_Statue_011.wav";

        [Boom.EditorExposed("Proximity Detection", "Enable/Disable proximity detection")]
        private bool EnemyDetection = true;

        [Boom.EditorExposed("Proximity Radius", "How close player must be to trigger proximity detection", 0.5f, 10f, true)]
        private float _proximityRadius = 2.5f;

        [Boom.EditorExposed("Proximity Duration", "How long player must stay close to trigger detection", 0.1f, 10f, true)]
        private float _proximityDuration = 5.0f;

        [Boom.EditorExposed("Proximity Vertical Tolerance", "Vertical range for proximity detection", 0.5f, 5f, true)]
        private float _proximityVerticalTolerance = 2.5f;

        [Boom.EditorExposed("Proximity Debug Log", "Enable debug logging for proximity detection")]
        private bool _proximityDebugLog = false;

        [Boom.EditorExposed("Rotation: Clockwise/Anti-clockwise", "Change Rotation (ignored when Ping Pong is enabled)")]
        private bool _rotation = true;

        [Boom.EditorExposed("Ping Pong Rotation", "Rotate by the angle in one direction, then sweep back the other way")]
        private bool _pingPong = false;

        private bool _pingPongForward = true;
        private bool _currentRotationIsClockwise = true;

        // Vision settings
        [Boom.EditorExposed("Detection Range", "How far the enemy can see", 1f, 30f, true)]
        private float _visionDetectionRange = 12f;

        [Boom.EditorExposed("Lose Target Range", "Range at which the enemy loses track of the target", 1f, 40f, true)]
        private float _visionLoseTargetRange = 15f;

        [Boom.EditorExposed("Detection Angle", "Field of view cone angle in degrees", 10f, 360f, true)]
        private float _visionDetectionAngle = 60f;

        [Boom.EditorExposed("Vertical Tolerance", "Max vertical height difference to detect target", 0.5f, 10f, true)]
        private float _visionVerticalTolerance = 2.5f;

        [Boom.EditorExposed("Require Line of Sight", "Whether the enemy needs clear line of sight to detect the player")]
        private bool _visionRequireLineOfSight = true;

        [Boom.EditorExposed("Show Vision Cone", "Draw the vision cone in-scene (visible without debug mode)")]
        private bool _showVisionCone = false;

        // Vision system
        private VisionComponent _vision;

        // NEW: Proximity detection system
        private ProximityDetectionComponent _proximityDetection;

        // Detection tracking (prevent multiple damage per detection)
        private bool _hasDealtDamage = false;
        private bool _coneInitialized = false;

        // NEW: Auto-reset timer for damage flag
        private float _damageResetTimer = 0f;
        private const float DAMAGE_RESET_DELAY = 3.0f;

        // Entity name for spotlight lookup (matches SpotlightFollower's targetName)
        [Boom.EditorExposed("Entity Name", "Name of this sentry for spotlight lookup")]
        private string _entityName = "Sentry_1";

        // ==== Vision Cone Particles ====
        [Boom.EditorExposed("Enable Cone Particles", "Show particle effect in the vision cone direction")]
        private bool _enableConeParticles = true;

        [Boom.EditorExposed("Cone Particle Rate", "Particles per second in the cone", 5f, 500f, true)]
        private float _coneParticleRate = 120f;

        [Boom.EditorExposed("Cone Particle Speed Min", "", 0.0f, 5f, true)]
        private float _coneSpeedMin = 0.05f;

        [Boom.EditorExposed("Cone Particle Speed Max", "", 0.0f, 5f, true)]
        private float _coneSpeedMax = 0.2f;

        [Boom.EditorExposed("Cone Particle Gravity", "", -5f, 5f, true)]
        private float _coneGravity = 0.0f;

        [Boom.EditorExposed("Cone Particle Lifetime Min", "", 0.1f, 5f, true)]
        private float _coneLifetimeMin = 0.3f;

        [Boom.EditorExposed("Cone Particle Lifetime Max", "", 0.1f, 5f, true)]
        private float _coneLifetimeMax = 0.7f;

        private bool _coneParticlesInitialized = false;

        // Alert sound tracking
        private bool _alertSoundPlayed = false;
        private Random _random = new Random();
        private string _alertSoundName;

        public void OnStart(string jsonParams)
        {
            //($"[EnemyController] OnStart() - Entity: {Entity}");

            if (!API.HasTransform(Entity))
            {
                //("[EnemyController] ERROR: Entity missing TransformComponent!");
                return;
            }

            // _entityName is now set via EditorExposed attribute in the Inspector

            // Initialize vision system
            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(jsonParams);
            _vision.SetDetectionRange(_visionDetectionRange);
            _vision.SetLoseTargetRange(_visionLoseTargetRange);
            _vision.SetDetectionAngle(_visionDetectionAngle);
            _vision.SetVerticalTolerance(_visionVerticalTolerance);
            _vision.SetRequireLineOfSight(_visionRequireLineOfSight);

            // NEW: Initialize proximity detection
            _proximityDetection = new ProximityDetectionComponent { Entity = Entity };
            _proximityDetection.OnProximityDetected += OnProximityDetected;
            _proximityDetection.OnStart();
            
            // Configure settings from editor-exposed fields
            _proximityDetection.SetDetectionRadius(_proximityRadius);
            _proximityDetection.SetDetectionDuration(_proximityDuration);
            _proximityDetection.SetVerticalTolerance(_proximityVerticalTolerance);
            _proximityDetection.EnableDebugLog(_proximityDebugLog);

            // Initialize rotation from current entity rotation
            _initialYRotation = API.GetRotation(Entity).Y;
            _currentYRotation = _initialYRotation;
            _targetYRotation = _initialYRotation;

            // NOTE: Visual cone is initialized in the first OnUpdate frame, after physics has
            // finished initializing, otherwise the rigidbody invalidates the cone render state.

            // Add particle emitter to this entity at runtime for the vision cone effect
            if (_enableConeParticles)
            {
                API.AddParticleEmitter(Entity);
            }

            // NEW: Register with PlayerManager
            PlayerManager.RegisterEnemy(this);

            //("[EnemyController] Controller initialized with vision and proximity systems");

            _vision.EnableDebugReasons(true);
            _vision.EnableDebugLOS(true);

            // Preload alert sounds
            _alertSoundName = "alert_" + Entity.ToString();
            PreloadAlertSounds();
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Defer visual cone setup to first update so physics/collider has finished initializing
            if (!_coneInitialized)
            {
                _coneInitialized = true;
                API.SetVisualConeParams(Entity, _visionDetectionRange, _visionDetectionAngle * 0.5f);
                API.SetAIFacingYaw(Entity, _initialYRotation);
                API.SetVisualConeFacing(Entity, _initialYRotation);
            }

            // Deferred: configure and start cone particles after first frame
            if (_enableConeParticles && !_coneParticlesInitialized && API.HasParticleEmitter(Entity))
            {
                _coneParticlesInitialized = true;
                ConfigureConeParticles();
                API.PlayParticleEmitter(Entity);
            }

            // --- FREEZE CHECK ---
            if (FreezeManager.IsFrozen(API.GetPosition(Entity)))
            {
                // When frozen, still allow proximity detection (player can sneak close)
                // but disable rotation and vision
                if (EnemyDetection)
                {
                    _proximityDetection?.OnUpdate(dt);
                }
                // Stop cone particles while frozen
                if (_enableConeParticles && API.HasParticleEmitter(Entity) && API.IsParticleEmitterPlaying(Entity))
                    API.StopParticleEmitter(Entity);
                return;
            }

            // Resume cone particles after unfreeze
            if (_enableConeParticles && _coneParticlesInitialized && API.HasParticleEmitter(Entity) && !API.IsParticleEmitterPlaying(Entity))
            {
                ConfigureConeParticles();
                API.PlayParticleEmitter(Entity);
            }

            // Update vision system (always active)
            _vision?.OnUpdate(dt);

            // Draw vision cone in-scene (no debug mode needed)
            if (_showVisionCone)
            {
                bool alert = _vision?.GetState() == VisionComponent.VisionState.Alert;
                Vec4 color = alert ? new Vec4(1f, 0.1f, 0.1f, 0.35f) : new Vec4(0f, 1f, 0.2f, 0.25f);
                API.DrawDebugVisionCone(Entity, _visionDetectionRange, _visionDetectionAngle * 0.5f, color);
            }

            // Update proximity detection (only if enabled)
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

            // Handle rotation (only when not alert)
            if (_vision?.GetState() != VisionComponent.VisionState.Alert)
            {
                UpdateRotation(dt);
            }

            // Cone particle direction is local-space (0,0,1); the entity's world matrix
            // rotation (set by ApplyYaw) automatically orients it to match the vision cone.

            // NEW: Auto-reset damage flag after delay
            if (_hasDealtDamage)
            {
                _damageResetTimer += dt;
                if (_damageResetTimer >= DAMAGE_RESET_DELAY)
                {
                    _hasDealtDamage = false;
                    _damageResetTimer = 0f;
                    //("[EnemyController] Damage flag auto-reset - can damage again");
                }
            }
        }

        private void UpdateRotation(float dt)
        {
            // Check if it's time to pick a new target rotation
            if (!_isRotating)
            {
                _rotationTimer += dt;

                if (_rotationTimer >= _rotationInterval)
                {
                    _rotationTimer = 0f;

                    // Apply rotation based on direction
                    if (_pingPong)
                    {
                        // _rotation sets the initial sweep direction; _pingPongForward alternates each sweep
                        bool goClockwise = (_rotation == _pingPongForward);
                        _currentRotationIsClockwise = goClockwise;
                        _targetYRotation += goClockwise ? _rotationAngle : -_rotationAngle;
                        _pingPongForward = !_pingPongForward;
                    }
                    else if (_rotation)
                    {
                        // Clockwise
                        _currentRotationIsClockwise = true;
                        _targetYRotation += _rotationAngle;
                    }
                    else
                    {
                        // Anti-clockwise
                        _currentRotationIsClockwise = false;
                        _targetYRotation -= _rotationAngle;
                    }

                    // Normalize angle
                    if (_targetYRotation >= 360f)
                        _targetYRotation -= 360f;
                    if (_targetYRotation < 0f)
                        _targetYRotation += 360f;

                    _isRotating = true;
                    //($"[EnemyController] Starting rotation to {_targetYRotation}°");

                    try
                    {
                        long ticks = DateTime.UtcNow.Ticks;
                        int index = (int)(ticks % 3);

                        string clipPath;
                        string soundId;

                        switch (index)
                        {
                            case 0:
                                clipPath = _turnSoundPath1;
                                soundId = TURN_SOUND_ID + "_0";
                                break;
                            case 1:
                                clipPath = _turnSoundPath2;
                                soundId = TURN_SOUND_ID + "_1";
                                break;
                            case 2:
                            default:
                                clipPath = _turnSoundPath3;
                                soundId = TURN_SOUND_ID + "_2";
                                break;
                        }

                        Vec3 enemyPos = API.GetPosition(Entity);
                        //($"[EnemyController] Playing turn sound {index} at {enemyPos} ({clipPath})");

                        API.PlaySoundAt(soundId, clipPath, enemyPos, false);
                        API.SetSoundVolume(soundId, 0.25f);
                        API.Set3DMinMaxDistance(soundId, 1.0f, 20.0f);
                    }
                    catch (Exception)
                    {
                        //($"[EnemyController] ERROR while playing rotation sound: {ex.Message}");
                    }
                }
            }

            // Smoothly interpolate toward target rotation
            if (_isRotating)
            {
                float angleDifference = _targetYRotation - _currentYRotation;

                while (angleDifference > 180f) angleDifference -= 360f;
                while (angleDifference < -180f) angleDifference += 360f;

                // When difference is exactly ±180°, shortest-path is ambiguous; use intended direction
                if (Math.Abs(Math.Abs(angleDifference) - 180f) < 0.01f)
                    angleDifference = _currentRotationIsClockwise ? 180f : -180f;

                float rotationStep = _rotationSpeed * dt;

                if (Math.Abs(angleDifference) <= rotationStep)
                {
                    _currentYRotation = _targetYRotation;
                    _isRotating = false;
                    //($"[EnemyController] Completed rotation at {_currentYRotation}°");
                }
                else
                {
                    _currentYRotation += Math.Sign(angleDifference) * rotationStep;

                    while (_currentYRotation >= 360f) _currentYRotation -= 360f;
                    while (_currentYRotation < 0f) _currentYRotation += 360f;
                }

                ApplyYaw(_currentYRotation);
            }
        }

        // === VISION EVENT HANDLERS ===
        private void OnPlayerDetected(ulong target, Vec3 position)
        {
            //(">>> ENEMY ALERTED BY VISION! STOPPING PATROL! <<<");

            // Set spotlight to alert (red) color
            var spotlight = SpotlightFollower.GetByTargetName(_entityName);
            if (spotlight != null)
            {
                spotlight.SetAlert(true);
            }

            // Instantly rotate to face the player
            Vec3 enemyPos = API.GetPosition(Entity);
            Vec3 directionToPlayer = new Vec3(
                position.X - enemyPos.X,
                0f,
                position.Z - enemyPos.Z
            );

            float distToPlayer = (float)Math.Sqrt(
                directionToPlayer.X * directionToPlayer.X +
                directionToPlayer.Z * directionToPlayer.Z
            );

            if (distToPlayer > 0f)
            {
                float lookAtYaw = (float)(Math.Atan2(directionToPlayer.X, directionToPlayer.Z) * 180.0 / Math.PI);
                _targetYRotation = lookAtYaw;
                _currentYRotation = lookAtYaw;
                _isRotating = false;
                ApplyYaw(_currentYRotation);
            }

            API.SetVisualConeAlert(Entity, true);

            // Play random alert sound (only once per detection)
            PlayRandomAlertSound(enemyPos);

            // Damage player (only once per detection)
            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                _damageResetTimer = 0f;  // Start timer
                //($"[EnemyController] Dealing damage to player (vision detection)!");
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }

        private void OnPlayerLost(ulong target, Vec3 lastKnownPosition)
        {
            API.SetVisualConeAlert(Entity, false);

            // Reset spotlight to original color
            var spotlight = SpotlightFollower.GetByTargetName(_entityName);
            if (spotlight != null)
            {
                spotlight.SetAlert(false);
            }

            // Reset damage flag so player can be caught again
            _hasDealtDamage = false;
            _alertSoundPlayed = false; // Reset so alert can play again next detection

            // NEW: Reset proximity detection when player is lost
            _proximityDetection?.ResetDetection();

            // Resume rotation patrol
            _rotationTimer = 0f;
            _isRotating = false;
        }

        private void OnPlayerTracking(ulong target, Vec3 position)
        {
            // Smoothly track the player while they're visible
            Vec3 enemyPos = API.GetPosition(Entity);
            Vec3 directionToPlayer = new Vec3(
                position.X - enemyPos.X,
                0f,
                position.Z - enemyPos.Z
            );

            float distToPlayer = (float)Math.Sqrt(
                directionToPlayer.X * directionToPlayer.X +
                directionToPlayer.Z * directionToPlayer.Z
            );

            if (distToPlayer > 0f)
            {
                float lookAtYaw = (float)(Math.Atan2(directionToPlayer.X, directionToPlayer.Z) * 180.0 / Math.PI);
                _targetYRotation = lookAtYaw;
                _currentYRotation = lookAtYaw;
                ApplyYaw(_currentYRotation);
            }
        }

        // === NEW: PROXIMITY DETECTION HANDLER ===
        private void OnProximityDetected(ulong target, Vec3 position)
        {
            // Check if proximity detection is enabled
            if (!EnemyDetection)
            {
                //("[EnemyController] Proximity detection disabled - ignoring detection event");
                return;
            }

            //(">>> ENEMY ALERTED BY PROXIMITY! PLAYER TOO CLOSE! <<<");

            // Similar to vision detection, but don't rotate immediately
            // Enemy "senses" player behind them and turns to attack

            Vec3 enemyPos = API.GetPosition(Entity);
            Vec3 directionToPlayer = new Vec3(
                position.X - enemyPos.X,
                0f,
                position.Z - enemyPos.Z
            );

            float distToPlayer = (float)Math.Sqrt(
                directionToPlayer.X * directionToPlayer.X +
                directionToPlayer.Z * directionToPlayer.Z
            );

            if (distToPlayer > 0f)
            {
                float lookAtYaw = (float)(Math.Atan2(directionToPlayer.X, directionToPlayer.Z) * 180.0 / Math.PI);
                _targetYRotation = lookAtYaw;
                _currentYRotation = lookAtYaw;
                _isRotating = false;
                ApplyYaw(_currentYRotation);
            }

            // Damage player (only once per detection)
            if (!_hasDealtDamage)
            {
                _hasDealtDamage = true;
                _damageResetTimer = 0f;  // Start timer
                //($"[EnemyController] Dealing damage to player (proximity detection)!");
                PlayerManager.NotifyPlayerCaught(Entity);
            }
        }

        // Apply a yaw rotation to the entity and keep the visual cone in sync.
        private void ApplyYaw(float yaw)
        {
            API.SetRotationY(Entity, yaw);
            API.SetAIFacingYaw(Entity, yaw);
            API.SetVisualConeFacing(Entity, yaw);
        }

        // === PUBLIC CONFIGURATION ===
        public void SetRotationSpeed(float degreesPerSecond)
        {
            _rotationSpeed = degreesPerSecond;
            //($"[EnemyController] Rotation speed set to {_rotationSpeed}°/s");
        }

        public void SetRotationInterval(float seconds)
        {
            _rotationInterval = seconds;
            //($"[EnemyController] Rotation interval set to {_rotationInterval}s");
        }

        public void SetRotationAngle(float degrees)
        {
            _rotationAngle = degrees;
            //($"[EnemyController] Rotation angle set to {_rotationAngle}°");
        }

        // NEW: Proximity configuration
        public void SetProximityRadius(float radius)
        {
            _proximityRadius = radius;
            _proximityDetection?.SetDetectionRadius(radius);
        }

        public void SetProximityDuration(float duration)
        {
            _proximityDuration = duration;
            _proximityDetection?.SetDetectionDuration(duration);
        }

        // NEW: Implement interface method
        public void OnPlayerRespawned()
        {
            // Force reset all detection states
            _hasDealtDamage = false;
            _damageResetTimer = 0f;
            _alertSoundPlayed = false; // Reset so alert can play again

            // Reset proximity detection
            _proximityDetection?.ResetDetection();

            // Reset vision/spotlight state
            var spotlight = SpotlightFollower.GetByTargetName(_entityName);
            if (spotlight != null)
                spotlight.SetAlert(false);

            // Restore rotation to original spawn rotation
            _currentYRotation = _initialYRotation;
            _targetYRotation = _initialYRotation;
            _isRotating = false;
            _rotationTimer = 0f;
            _pingPongForward = true;

            ApplyYaw(_initialYRotation);
            API.SetVisualConeAlert(Entity, false);
            //("[EnemyController] Player respawned - all detection states reset");
        }

        // ====== AUDIO HELPER METHODS ======
        private string[] GetAlertSounds()
        {
            return new string[] {
                _alertSoundPath, _alertSoundPath2, _alertSoundPath3, _alertSoundPath4,
                _alertSoundPath5, _alertSoundPath6, _alertSoundPath7
            };
        }

        private void PreloadAlertSounds()
        {
            string[] sounds = GetAlertSounds();
            for (int i = 0; i < sounds.Length; i++)
            {
                string soundName = _alertSoundName + "_" + i;
                API.PreloadSound(soundName, sounds[i], loop: false);
            }
        }

        private void PlayRandomAlertSound(Vec3 position)
        {
            if (_alertSoundPlayed) return; // Only play once per detection

            string[] sounds = GetAlertSounds();
            int index = _random.Next(sounds.Length);
            string soundName = _alertSoundName + "_" + index;

            API.PlaySoundAt(soundName, sounds[index], position, loop: false);
            API.SetSoundVolume(soundName, 1.0f);
            API.Set3DMinMaxDistance(soundName, 1.0f, 25.0f);

            _alertSoundPlayed = true;
        }

        private void ConfigureConeParticles()
        {
            // Shape: spotlight volume — particles spawn throughout the cone, not from origin
            API.SetParticleShapeType(Entity, 4); // 4 = spotlight volume
            API.SetParticleShapeAngle(Entity, _visionDetectionAngle * 0.5f);
            API.SetParticleShapeRange(Entity, _visionDetectionRange);

            // Direction: local-space forward — the entity's world rotation handles orientation
            API.SetParticleDirection(Entity, 0f, 0f, 1f);

            // Dust motes in light: soft glow that fades in then out
            API.SetParticleStartColor(Entity, 1.0f, 0.97f, 0.85f, 0.4f);
            API.SetParticleEndColor(Entity, 1.0f, 0.9f, 0.7f, 0.0f);

            API.SetParticleEmissionRate(Entity, _coneParticleRate);
            API.SetParticleSpeed(Entity, _coneSpeedMin, _coneSpeedMax);
            API.SetParticleGravity(Entity, _coneGravity);
            // Tiny floating dust specks
            API.SetParticleSize(Entity, 0.02f, 0.05f, 0.03f);
            API.SetParticleLifetime(Entity, _coneLifetimeMin, _coneLifetimeMax);
        }

        public void OnDestroy()
        {
            _vision?.OnDestroy();
            _proximityDetection?.OnDestroy();

            if (_enableConeParticles && API.HasParticleEmitter(Entity))
                API.StopParticleEmitter(Entity);

            // NEW: Unregister from PlayerManager
            PlayerManager.UnregisterEnemy(this);

            //($"[EnemyController] OnDestroy() - Entity: {Entity}");
        }
    }
}