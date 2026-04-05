using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    public class BossHideSeekController : IEnemyController
    {
        public ulong Entity;
        private static Dictionary<ulong, BossHideSeekController> s_instances = new Dictionary<ulong, BossHideSeekController>();
        private static Dictionary<string, BossHideSeekController> s_instancesByName = new Dictionary<string, BossHideSeekController>();

        public static void SetCinematicTilt(string bossName, float tiltAmount)
        {
            if (s_instancesByName.TryGetValue(bossName, out var boss))
            {
                boss._targetXRotation = boss._baseXRotation + tiltAmount;
            }
        }

        public static void TriggerBossLookDown()
        {
            foreach (var boss in s_instances.Values)
                boss._targetXRotation = boss._baseXRotation + boss._cinematicTiltAmount;
        }

        public static void TriggerBossLookReset()
        {
            foreach (var boss in s_instances.Values)
                boss._targetXRotation = boss._baseXRotation;
        }

        [Boom.EditorExposed("Is Active", "Whether the boss is currently active")]
        private bool _isActive = false;

        [Boom.EditorExposed("Boss Name", "Unique name for this boss (used by cutscenes)")]
        private string _bossName = "GiantDaruma";

        [Boom.EditorExposed("Watching Yaw", "Yaw angle when looking at player")]
        private float _watchingYaw = 0f;

        [Boom.EditorExposed("Resting Yaw", "Yaw angle when facing away")]
        private float _restingYaw = 180f;

        [Boom.EditorExposed("Initial Yaw", "Yaw angle during cutscene (before activation)")]
        private float _initialYaw = 180f;

        [Boom.EditorExposed("Inverse Forward", "Check this if the boss sees behind itself")]
        private bool _inverseForward = false;

        [Boom.EditorExposed("Activation Delay", "Seconds of safety after activation")]
        private float _activationDelay = 1.0f;

        [Boom.EditorExposed("Min Rotation Interval", "Minimum time between turns")]
        private float _minRotationInterval = 5.0f;

        [Boom.EditorExposed("Max Rotation Interval", "Maximum time between turns")]
        private float _maxRotationInterval = 7.0f;

        [Boom.EditorExposed("Scan Tilt", "Degrees to tilt down while searching")]
        private float _scanTiltAmount = 15.0f;

        [Boom.EditorExposed("Scan Yaw", "Degrees to scan left/right while searching")]
        private float _scanYawAmount = 30.0f;

        [Boom.EditorExposed("Scan Speed", "How fast to scan left/right")]
        private float _scanSpeed = 2.0f;

        [Boom.EditorExposed("Scan Forward Time", "Seconds to stare forward before scanning")]
        private float _scanForwardDuration = 2.0f;

        [Boom.EditorExposed("Scan Phase Time", "Seconds to scan left/right")]
        private float _scanCycleDuration = 4.0f;

        [Boom.EditorExposed("Cinematic Tilt", "Degrees to tilt down when TriggerBossLookDown is called")]
        private float _cinematicTiltAmount = 25.0f;

        [Boom.EditorExposed("Shake Intensity", "Camera shake when turning (0 for none)")]
        private float _shakeIntensity = 0.05f;

        [Boom.EditorExposed("Pulse Intensity", "Light pulse strength (0 for none)")]
        private float _pulseIntensity = 0.5f;

        [Boom.EditorExposed("Pulse Speed", "Speed of light pulsing")]
        private float _pulseSpeed = 8.0f;

        [Boom.EditorExposed("Rotation Interval", "Legacy - now uses Min/Max range")]
        private float _rotationInterval = 8.0f;

        [Boom.EditorExposed("Rotation Speed", "Degrees per second")]
        private float _rotationSpeed = 360.0f;

        [Boom.EditorExposed("Catch Delay", "Seconds until player is caught")]
        private float _catchDelay = 5.0f;

        [Boom.EditorExposed("Detection Range", "Vision distance")]
        private float _detectionRange = 30f;

        [Boom.EditorExposed("Detection Angle", "Vision cone width")]
        private float _detectionAngle = 90f;

        [Boom.EditorExposed("Vertical Tolerance", "Max vertical distance")]
        private float _verticalTolerance = 10.0f;

        [Boom.EditorExposed("Light Entities (CSV)", "Names of lights")]
        private string _lightNamesCSV = "BossLight,LevelLight1,LevelLight2";

        [Boom.EditorExposed("Red Light Color (R,G,B)", "Color when watching")]
        private string _redColorCSV = "1.0,0.0,0.0";

        [Boom.EditorExposed("Green Light Color (R,G,B)", "Color when resting")]
        private string _greenColorCSV = "0.0,1.0,0.0";

        [Boom.EditorExposed("Red Intensities (CSV)", "Intensity for each light when watching")]
        private string _redIntensitiesCSV = "5.0,5.0,5.0";

        [Boom.EditorExposed("Green Intensities (CSV)", "Intensity for each light when resting")]
        private string _greenIntensitiesCSV = "2.0,2.0,2.0";

        [Boom.EditorExposed("Warning Text Entity", "UI text name")]
        private string _warningTextEntityName = "UI_WarningText";

        [Boom.EditorExposed("Enable Debug Logs", "Show info in console")]
        private bool _showDebugLogs = true;

        [Boom.EditorExposed("Draw Vision Debug", "Draw a red line in scene")]
        private bool _drawVisionDebug = true;

        private List<ulong> _lightEntities = new List<ulong>();
        private List<float> _redIntensities = new List<float>();
        private List<float> _greenIntensities = new List<float>();

        private ulong _warningText = 0;
        private float _timer = 0f;
        private bool _isTurning = false;
        private bool _isWatching = false;
        private float _targetYRotation = 0f;
        private float _currentYRotation = 0f;
        private float _currentXRotation = 0f;
        private float _targetXRotation = 0f;
        private float _baseXRotation = 0f;
        private float _catchTimer = 0f;
        private bool _isCountingDown = false;
        private bool _hasDealtDamage = false;
        private float _activationTimer = 0f;
        private Vec3 _colorRed = new Vec3(1, 0, 0);
        private Vec3 _colorGreen = new Vec3(0, 1, 0);
        private float _debugLogTimer = 0f;
        private bool _wasPausedLastFrame = false;
        private float _currentRotationInterval = 8.0f;
        private float _scanTimer = 0f;
        private float _appliedScanYaw = 0f;
        private float _appliedScanPitch = 0f;
        private float _pulseTimer = 0f;
        private static Random _random = new Random();

        // Turn sound
        private const string TURN_SOUND_NAME = "BossTurn";
        private const string TURN_SOUND_PATH = "Resources/Audio/bossturn2.wav";
        private bool _wasTurning = false;

        [Boom.EditorExposed("Turn Sound Volume", "Volume of the turning sound (0.0 - 1.0)")]
        private float _turnSoundVolume = 0.4f;

        // Warning sound (plays when turning away from player - boss turning to rest)
        private const string WARNING_SOUND_NAME = "BossWarning";
        private static readonly string[] WARNING_SOUND_PATHS = new string[]
        {
            "Resources/Audio/Boss_Warning_02.wav",
            "Resources/Audio/Boss_Warning_03.wav",
            "Resources/Audio/Boss_Warning_04.wav",
            "Resources/Audio/Boss_Warning_05.wav",
            "Resources/Audio/Boss_Warning_06.wav",
            "Resources/Audio/Boss_Warning_07.wav",
            "Resources/Audio/Boss_Warning_08.wav"
        };
        private static Random _warningRandom = new Random();

        // Warning line sound (plays 3s before boss turns towards player)
        private const string WARNING_LINE_SOUND_NAME = "BossWarningLine";
        private static readonly string[] WARNING_LINE_SOUND_PATHS = new string[]
        {
            "Resources/Audio/BossWarning_Line1.wav",
            "Resources/Audio/BossWarning_Line2.wav",
            "Resources/Audio/BossWarning_Line3.wav",
            "Resources/Audio/BossWarning_Line4.wav",
            "Resources/Audio/BossWarning_Line5.wav"
        };

        [Boom.EditorExposed("Warning Line Volume", "Volume of warning line when boss turns to player (0.0 - 1.0)")]
        private float _warningLineVolume = 0.3f;

        [Boom.EditorExposed("Warning Sound Volume", "Volume of the warning sound (0.0 - 1.0)")]
        private float _warningSoundVolume = 0.3f;

        [Boom.EditorExposed("Warning Delay", "Seconds to wait after warning before turning towards player")]
        private float _warningDelay = 3.0f;

        [Boom.EditorExposed("Warning Line After Turn Delay", "Seconds after turning towards player to play warning line")]
        private float _warningLineAfterTurnDelay = 1.0f;

        private bool _isWaitingToTurn = false;
        private float _warningTimer = 0f;
        private bool _pendingWatchState = false;

        // Warning line after turn state
        private bool _isWaitingForWarningLine = false;
        private float _warningLineTimer = 0f;

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            string myName = _bossName;
            if (!string.IsNullOrEmpty(myName)) s_instancesByName[myName] = this;

            if (!API.HasTransform(Entity)) return;
            
            _colorRed = ParseVec3(_redColorCSV, new Vec3(1, 0, 0));
            _colorGreen = ParseVec3(_greenColorCSV, new Vec3(0, 1, 0));
            
            _lightEntities.Clear();
            string[] names = _lightNamesCSV.Split(',');
            foreach (var name in names) { ulong id = API.FindEntity(name.Trim()); if (id != 0) _lightEntities.Add(id); }

            _redIntensities = ParseFloatList(_redIntensitiesCSV, _lightEntities.Count, 5.0f);
            _greenIntensities = ParseFloatList(_greenIntensitiesCSV, _lightEntities.Count, 2.0f);

            _warningText = API.FindEntity(_warningTextEntityName);

            // Set initial cinematic rotation
            _currentYRotation = _initialYaw;
            _targetYRotation = _initialYaw;
            Vec3 rot = API.GetRotation(Entity);
            rot.Y = _currentYRotation;
            _baseXRotation = rot.X;
            _currentXRotation = rot.X;
            _targetXRotation = rot.X;
            API.SetRotation(Entity, rot);

            ResetToRestingState(false); // Initialize state without overriding rotation
            
            // Override for cinematic red start (boss is "watching" the player area)
            _isWatching = true;
            UpdateLights(_colorRed, _redIntensities);

            PlayerManager.RegisterEnemy(this);
        }

        private List<float> ParseFloatList(string csv, int count, float defaultVal)
        {
            List<float> list = new List<float>();
            string[] parts = csv.Split(',');
            for (int i = 0; i < count; i++)
            {
                if (i < parts.Length && float.TryParse(parts[i].Trim(), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out float val))
                    list.Add(val);
                else
                    list.Add(defaultVal);
            }
            return list;
        }

        private void ResetToRestingState(bool updateRotation = true)
        {
            _isWatching = false;
            if (updateRotation)
            {
                _currentYRotation = _restingYaw;
                _targetYRotation = _restingYaw;
            }
            _timer = 0f;
            _activationTimer = 0f;
            _isTurning = false;
            _isWaitingToTurn = false;
            _warningTimer = 0f;
            _isWaitingForWarningLine = false;
            _warningLineTimer = 0f;
            _hasDealtDamage = false;
            _catchTimer = _catchDelay;
            _scanTimer = 0f;
            _appliedScanYaw = 0f;
            _appliedScanPitch = 0f;
            _currentXRotation = _baseXRotation;
            _targetXRotation = _baseXRotation;
            
            // Randomize the next interval
            _currentRotationInterval = (float)(_minRotationInterval + _random.NextDouble() * (_maxRotationInterval - _minRotationInterval));

            if (_wasTurning) { API.StopSound(TURN_SOUND_NAME); _wasTurning = false; }
            API.StopSound(WARNING_SOUND_NAME);
            API.StopSound(WARNING_LINE_SOUND_NAME);
            StopCountdown();

            if (updateRotation)
            {
                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
            }
            UpdateLights(_colorGreen, _greenIntensities);
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Handle pausing
            if (Entry.IsGamePaused)
            {
                if (!_wasPausedLastFrame)
                {
                    if (_wasTurning) API.StopSound(TURN_SOUND_NAME);
                    if (_isWaitingToTurn) { API.StopSound(WARNING_SOUND_NAME); API.StopSound(WARNING_LINE_SOUND_NAME); }
                    _wasPausedLastFrame = true;
                }
                return;
            }
            else if (_wasPausedLastFrame)
            {
                if (_wasTurning) { API.PlaySound(TURN_SOUND_NAME, TURN_SOUND_PATH, true); API.SetSoundVolume(TURN_SOUND_NAME, _turnSoundVolume); }
                _wasPausedLastFrame = false;
            }

            if (_drawVisionDebug) DrawDebugVision();

            // 1. ANIMATION STATE CALCULATION
            float targetScanYaw = 0f;
            float targetScanPitch = 0f;

            if (_isActive && _isWatching && !_isTurning && !_isWaitingToTurn && !Entry.IsPlayerDead)
            {
                _scanTimer += dt;
                float totalCycle = _scanForwardDuration + _scanCycleDuration;
                
                // If the turn timer is up, we finish the current scan but don't start a new one
                if (_timer >= _currentRotationInterval && _scanTimer >= totalCycle)
                {
                    targetScanPitch = 0f;
                    targetScanYaw = 0f;
                }
                else
                {
                    float cyclePos = _scanTimer % totalCycle;
                    if (cyclePos >= _scanForwardDuration && _scanCycleDuration > 0)
                    {
                        float scanPhase = (cyclePos - _scanForwardDuration) / _scanCycleDuration;
                        targetScanPitch = (float)Math.Sin(scanPhase * Math.PI) * _scanTiltAmount;
                        targetScanYaw = (float)Math.Sin(scanPhase * Math.PI * 2.0) * _scanYawAmount;
                    }
                }
            }
            else
            {
                _scanTimer = 0f;
            }

            // Smoothly interpolate the scanning offsets to prevent "flicks"
            _appliedScanYaw += (targetScanYaw - _appliedScanYaw) * dt * 8.0f;
            _appliedScanPitch += (targetScanPitch - _appliedScanPitch) * dt * 8.0f;
            
            // Only use scanning pitch if active, otherwise targetX is driven by cinematic triggers
            if (_isActive) _targetXRotation = _baseXRotation + _appliedScanPitch;

            // 2. STATE LOGIC
            if (_isActive && !Entry.IsPlayerDead)
            {
                if (!_isTurning)
                {
                    if (!_isWatching)
                    {
                        if (_timer > _currentRotationInterval - 2.0f)
                        {
                            _pulseTimer += dt * _pulseSpeed;
                            float pulse = 1.0f + (float)Math.Sin(_pulseTimer) * _pulseIntensity;
                            List<float> pulsedIntensities = new List<float>();
                            foreach (var intensity in _greenIntensities) pulsedIntensities.Add(intensity * pulse);
                            UpdateLights(_colorGreen, pulsedIntensities);
                        }
                    }

                    if (_isWaitingToTurn)
                    {
                        _warningTimer += dt;
                        if (_warningTimer >= _warningDelay)
                        {
                            _isWaitingToTurn = false;
                            _warningTimer = 0f;
                            _isTurning = true;
                            _isWatching = _pendingWatchState;
                            _targetYRotation = _watchingYaw;
                            UpdateLights(_isWatching ? _colorRed : _colorGreen, _isWatching ? _redIntensities : _greenIntensities);
                            if (_isWatching && _shakeIntensity > 0) API.TriggerCameraShake(_shakeIntensity, 0.5f);
                        }
                    }
                    else
                    {
                        _timer += dt;
                        if (_timer >= _currentRotationInterval)
                        {
                            float totalCycle = _scanForwardDuration + _scanCycleDuration;
                            float cyclePos = _scanTimer % totalCycle;
                            bool isStaring = cyclePos < _scanForwardDuration;

                            if (!_isWatching || isStaring)
                            {
                                _timer = 0f;
                                _pendingWatchState = !_isWatching;
                                if (_pendingWatchState)
                                {
                                    string randomWarning = WARNING_SOUND_PATHS[_warningRandom.Next(WARNING_SOUND_PATHS.Length)];
                                    API.PlaySound(WARNING_SOUND_NAME, randomWarning, false);
                                    API.SetSoundVolume(WARNING_SOUND_NAME, _warningSoundVolume);
                                    _isWaitingToTurn = true;
                                }
                                else
                                {
                                    _isTurning = true;
                                    _isWatching = false;
                                    _targetYRotation = _restingYaw;
                                    UpdateLights(_colorGreen, _greenIntensities);
                                    _currentRotationInterval = (float)(_minRotationInterval + _random.NextDouble() * (_maxRotationInterval - _minRotationInterval));
                                }
                            }
                        }
                    }

                    if (_isWatching) UpdateDetection(dt);
                }

                if (_isWaitingForWarningLine)
                {
                    _warningLineTimer += dt;
                    if (_warningLineTimer >= _warningLineAfterTurnDelay)
                    {
                        _isWaitingForWarningLine = false;
                        string randomWarningLine = WARNING_LINE_SOUND_PATHS[_warningRandom.Next(WARNING_LINE_SOUND_PATHS.Length)];
                        // Use a unique name to allow multiple lines/instances to overlap naturally
                        string uniqueName = WARNING_LINE_SOUND_NAME + "_" + DateTime.Now.Ticks;
                        API.PlaySound(uniqueName, randomWarningLine, false);
                        API.SetSoundVolume(uniqueName, _warningLineVolume);
                    }
                }
            }
            else if (Entry.IsPlayerDead)
            {
                if (_isCountingDown) StopCountdown();
                if (_wasTurning) { API.StopSound(TURN_SOUND_NAME); _wasTurning = false; }
                if (_isWaitingToTurn) { API.StopSound(WARNING_SOUND_NAME); API.StopSound(WARNING_LINE_SOUND_NAME); _isWaitingToTurn = false; _warningTimer = 0f; }
            }

            // 4. ROTATION APPLICATION (Always runs for cinematic support)
            if (_isTurning && _isActive)
            {
                if (!_wasTurning) { API.PlaySound(TURN_SOUND_NAME, TURN_SOUND_PATH, true); API.SetSoundVolume(TURN_SOUND_NAME, _turnSoundVolume); _wasTurning = true; }
                
                float angleDiff = _targetYRotation - _currentYRotation;
                while (angleDiff > 180f) angleDiff -= 360f;
                while (angleDiff < -180f) angleDiff += 360f;
                float step = _rotationSpeed * dt;
                
                if (Math.Abs(angleDiff) <= step)
                {
                    _currentYRotation = _targetYRotation;
                    _isTurning = false;
                    API.StopSound(TURN_SOUND_NAME);
                    _wasTurning = false;
                    if (_isWatching) _isWaitingForWarningLine = true;
                }
                else { _currentYRotation += Math.Sign(angleDiff) * step; }
            }

            _currentXRotation += (_targetXRotation - _currentXRotation) * dt * 6.0f;

            Vec3 finalRot = API.GetRotation(Entity);
            finalRot.Y = _currentYRotation + _appliedScanYaw;
            finalRot.X = _currentXRotation;
            API.SetRotation(Entity, finalRot);
        }

        private void DrawDebugVision()
        {
            Vec3 pos = API.GetPosition(Entity); pos.Y += 1.5f;
            float yawRad = _currentYRotation * (float)Math.PI / 180.0f;
            float fx = (float)Math.Sin(yawRad); float fz = (float)Math.Cos(yawRad);
            if (_inverseForward) { fx = -fx; fz = -fz; }
            Vec3 forward = new Vec3(fx, 0, fz);
            Vec3 end = new Vec3(pos.X + forward.X * 10f, pos.Y, pos.Z + forward.Z * 10f);
            API.DrawDebugLine(pos, end, _isWatching ? new Vec3(1, 0, 0) : new Vec3(0, 1, 0));
        }

        public static void Activate(ulong entityID)
        {
            if (s_instances.TryGetValue(entityID, out var boss))
            {
                if (!boss._isActive)
                {
                    boss._isActive = true;
                    boss._timer = 0f;
                    boss._activationTimer = 0f;
                    boss._isTurning = true;
                    boss._targetYRotation = boss._restingYaw;
                    boss._isWatching = false;
                    boss.UpdateLights(boss._colorGreen, boss._greenIntensities);
                }
            }
        }

        private void UpdateDetection(float dt)
        {
            _debugLogTimer += dt;
            bool log = (_debugLogTimer >= 0.5f) && _showDebugLogs;
            if (log) _debugLogTimer = 0f;
            ulong player = PlayerMovement.GetPlayerEntity();
            if (player == 0) player = API.FindEntity("Player");
            if (player == 0) return;
            if (PlayerMovement.IsPlayerInvisibleToEnemies()) { 
                if (log) API.Log("[BossHideSeek] Player is invisible to enemies.");
                if (_isCountingDown) StopCountdown(); 
                return; 
            }
            Vec3 bPos = API.GetPosition(Entity); Vec3 pPos = API.GetPosition(player);
            if (Math.Abs(pPos.Y - bPos.Y) > _verticalTolerance) { 
                if (log) API.Log("[BossHideSeek] Player out of vertical tolerance.");
                if (_isCountingDown) StopCountdown(); 
                return; 
            }
            float dx = pPos.X - bPos.X; float dz = pPos.Z - bPos.Z; float dist = (float)Math.Sqrt(dx * dx + dz * dz);
            if (dist < _detectionRange)
            {
                float yawRad = _currentYRotation * (float)Math.PI / 180.0f;
                float fx = (float)Math.Sin(yawRad); float fz = (float)Math.Cos(yawRad);
                if (_inverseForward) { fx = -fx; fz = -fz; }
                float tx = dx / dist; float tz = dz / dist;
                float dot = tx * fx + tz * fz;
                float cosHalf = (float)Math.Cos((_detectionAngle * 0.5f) * Math.PI / 180.0);
                if (dot > cosHalf)
                {
                    if (!_isCountingDown)
                    {
                        if (log) API.Log("[BossHideSeek] Player SPOTTED! Starting countdown...");
                        _isCountingDown = true;
                        _catchTimer = _catchDelay;
                        ShowWarningText(true);
                    }
                    _catchTimer -= dt;
                    UpdateWarningText(_catchTimer);
                    if (_catchTimer <= 0f && !_hasDealtDamage)
                    {
                        if (log) API.Log("[BossHideSeek] Player CAUGHT! Dealing damage.");
                        _hasDealtDamage = true;
                        ShowWarningText(false);
                        PlayerManager.NotifyPlayerCaught(Entity);
                    }
                    return;
                }
            }
            if (_isCountingDown)
            {
                if (log) API.Log("[BossHideSeek] Player lost. Stopping countdown.");
                StopCountdown();
            }
        }

        private void StopCountdown() { if (!_isCountingDown) return; _isCountingDown = false; _catchTimer = _catchDelay; ShowWarningText(false); }
        
        public void OnPlayerRespawned() 
        { 
            _isActive = false; 
            _currentYRotation = _initialYaw;
            _targetYRotation = _initialYaw;
            if (API.HasTransform(Entity))
            {
                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                rot.X = _baseXRotation;
                _currentXRotation = _baseXRotation;
                _targetXRotation = _baseXRotation;
                API.SetRotation(Entity, rot);
            }
            ResetToRestingState(false); 
            _isWatching = true;
            UpdateLights(_colorRed, _redIntensities);
        }

        private void UpdateLights(Vec3 color, List<float> intensities)
        {
            for (int i = 0; i < _lightEntities.Count; i++)
            {
                ulong l = _lightEntities[i];
                float intensity = (i < intensities.Count) ? intensities[i] : 1.0f;
                if (API.HasSpotLight(l)) { API.SetSpotLightColor(l, color); API.SetSpotLightIntensity(l, intensity); }
                else if (API.HasPointLight(l)) { API.SetPointLightColor(l, color); API.SetPointLightIntensity(l, intensity); }
                else if (API.HasDirectLight(l)) { API.SetDirectLightColor(l, color); API.SetDirectLightIntensity(l, intensity); }
            }
        }

        private void UpdateLightsInterpolated(float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            Vec3 color = new Vec3(
                _colorGreen.X + (_colorRed.X - _colorGreen.X) * t,
                _colorGreen.Y + (_colorRed.Y - _colorGreen.Y) * t,
                _colorGreen.Z + (_colorRed.Z - _colorGreen.Z) * t
            );
            List<float> intensities = new List<float>();
            for (int i = 0; i < _lightEntities.Count; i++)
            {
                float g = (i < _greenIntensities.Count) ? _greenIntensities[i] : 1.0f;
                float r = (i < _redIntensities.Count) ? _redIntensities[i] : 1.0f;
                intensities.Add(g + (r - g) * t);
            }
            UpdateLights(color, intensities);
        }

        private void ShowWarningText(bool show) { if (_warningText == 0 || !API.HasText(_warningText)) return; var c = API.GetTextColor(_warningText); c.W = show ? 1f : 0f; API.SetTextColor(_warningText, c); }
        private void UpdateWarningText(float remaining) { if (_warningText == 0 || !API.HasText(_warningText)) return; int s = (int)System.Math.Ceiling(System.Math.Max(0.0f, remaining)); API.SetText(_warningText, "Spotted! HIDE in " + s + "s!"); }
        private Vec3 ParseVec3(string csv, Vec3 def) { try { string[] p = csv.Split(','); return new Vec3(float.Parse(p[0]), float.Parse(p[1]), float.Parse(p[2])); } catch { return def; } }
        public void OnDestroy() 
        { 
            if (_wasTurning) { API.StopSound(TURN_SOUND_NAME); _wasTurning = false; } 
            API.StopSound(WARNING_SOUND_NAME); API.StopSound(WARNING_LINE_SOUND_NAME); 
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity); 
            string myName = _bossName;
            if (!string.IsNullOrEmpty(myName) && s_instancesByName.ContainsKey(myName)) s_instancesByName.Remove(myName);
            PlayerManager.UnregisterEnemy(this); 
        }
    }
}
