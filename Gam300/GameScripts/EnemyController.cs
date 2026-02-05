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

        private float _currentYRotation = 0f;
        private float _targetYRotation = 0f;

        [Boom.EditorExposed("Rotation Speed", "Degrees per second for smooth rotation", 10f, 360f, true)]
        private float _rotationSpeed = 90f; // Degrees per second for smooth rotation

        [Boom.EditorExposed("Rotation Angle", "Degrees to rotate each turn", 10f, 360f, true)]
        private float _rotationAngle = 90f; // Degrees to rotate per turn (configurable per sentry)

        private bool _isRotating = false;
        private const string TURN_SOUND_ID = "enemy_turn";

        // ===== Audio Settings =====
        [Boom.EditorExposed("Turn Sound 1", "First rotation sound variant")]
        private string _turnSoundPath1 = "Resources/Audio/StatueTurn_01.wav";

        [Boom.EditorExposed("Turn Sound 2", "Second rotation sound variant")]
        private string _turnSoundPath2 = "Resources/Audio/StatueTurn_02.wav";

        [Boom.EditorExposed("Turn Sound 3", "Third rotation sound variant")]
        private string _turnSoundPath3 = "Resources/Audio/StatueTurn_03.wav";

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

        [Boom.EditorExposed("Rotation: Clockwise/Anti-clockwise", "Change Rotation")]
        private bool _rotation  = true;

        // Vision system
        private VisionComponent _vision;

        // NEW: Proximity detection system
        private ProximityDetectionComponent _proximityDetection;

        // Detection tracking (prevent multiple damage per detection)
        private bool _hasDealtDamage = false;

        // NEW: Auto-reset timer for damage flag
        private float _damageResetTimer = 0f;
        private const float DAMAGE_RESET_DELAY = 3.0f;

        // Entity name for spotlight lookup (matches SpotlightFollower's targetName)
        [Boom.EditorExposed("Entity Name", "Name of this sentry for spotlight lookup")]
        private string _entityName = "Sentry_1";

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

            // NEW: Initialize proximity detection
            _proximityDetection = new ProximityDetectionComponent { Entity = Entity };
            _proximityDetection.OnProximityDetected += OnProximityDetected;
            _proximityDetection.OnStart();
            // Optional: Configure settings
            // _proximityDetection.SetDetectionRadius(3.5f);
            // _proximityDetection.SetDetectionDuration(2.0f);
            // _proximityDetection.EnableDebugLog(true);

            // Initialize rotation from current entity rotation
            _currentYRotation = API.GetRotation(Entity).Y;
            _targetYRotation = _currentYRotation;

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

            // --- FREEZE CHECK ---
            if (FreezeManager.IsFrozen(API.GetPosition(Entity)))
            {
                // When frozen, still allow proximity detection (player can sneak close)
                // but disable rotation and vision
                if (EnemyDetection)
                {
                    _proximityDetection?.OnUpdate(dt);
                }
                return;
            }

            // Update vision system (always active)
            _vision?.OnUpdate(dt);

            // Update proximity detection (only if enabled)
            if (EnemyDetection)
            {
                _proximityDetection?.OnUpdate(dt);
            }

            // Handle rotation (only when not alert)
            if (_vision?.GetState() != VisionComponent.VisionState.Alert)
            {
                UpdateRotation(dt);
            }

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
                    if (_rotation)
                    {
                        // Clockwise
                        _targetYRotation += _rotationAngle;
                    }
                    else
                    {
                        // Anti-clockwise
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
                        API.SetSoundVolume(soundId, 0.5f);
                        API.Set3DMinMaxDistance(soundId, 1.0f, 25.0f);
                    }
                    catch (Exception ex)
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

                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
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

                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);

                //($"[EnemyController] Snapped to face player at {_currentYRotation:F1}°");
            }

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
            //("[EnemyController] Lost sight of player, searching...");

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

                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
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

                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);

                //($"[EnemyController] Turned to face player (proximity) at {_currentYRotation:F1}°");
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
            _proximityDetection?.SetDetectionRadius(radius);
        }

        public void SetProximityDuration(float duration)
        {
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

        public void OnDestroy()
        {
            _vision?.OnDestroy();
            _proximityDetection?.OnDestroy();

            // NEW: Unregister from PlayerManager
            PlayerManager.UnregisterEnemy(this);

            //($"[EnemyController] OnDestroy() - Entity: {Entity}");
        }
    }
}