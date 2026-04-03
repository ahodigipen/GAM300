using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    public class BossHideSeekController : IEnemyController
    {
        public ulong Entity;
        private static Dictionary<ulong, BossHideSeekController> s_instances = new Dictionary<ulong, BossHideSeekController>();

        [Boom.EditorExposed("Is Active", "Whether the boss is currently active")]
        private bool _isActive = false;

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

        [Boom.EditorExposed("Rotation Interval", "Time between turns")]
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
        private float _catchTimer = 0f;
        private bool _isCountingDown = false;
        private bool _hasDealtDamage = false;
        private float _activationTimer = 0f;
        private Vec3 _colorRed = new Vec3(1, 0, 0);
        private Vec3 _colorGreen = new Vec3(0, 1, 0);
        private float _debugLogTimer = 0f;
        private bool _wasPausedLastFrame = false;

        // Turn sound
        private const string TURN_SOUND_NAME = "BossTurn";
        private const string TURN_SOUND_PATH = "Resources/Audio/bossturn2.wav";
        private bool _wasTurning = false;

        [Boom.EditorExposed("Turn Sound Volume", "Volume of the turning sound (0.0 - 1.0)")]
        private float _turnSoundVolume = 1.0f;

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
        private float _warningLineVolume = 0.5f;

        [Boom.EditorExposed("Warning Sound Volume", "Volume of the warning sound (0.0 - 1.0)")]
        private float _warningSoundVolume = 0.5f;

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
                    // Stop sounds when entering pause
                    if (_wasTurning) API.StopSound(TURN_SOUND_NAME);
                    if (_isWaitingToTurn)
                    {
                        API.StopSound(WARNING_SOUND_NAME);
                        API.StopSound(WARNING_LINE_SOUND_NAME);
                    }
                    _wasPausedLastFrame = true;
                }
                return;
            }
            else if (_wasPausedLastFrame)
            {
                // Resume sounds when leaving pause
                if (_wasTurning)
                {
                    API.PlaySound(TURN_SOUND_NAME, TURN_SOUND_PATH, true);
                    API.SetSoundVolume(TURN_SOUND_NAME, _turnSoundVolume);
                }
                // Note: Warning sound is short and random, probably better to just let it be gone 
                // or wait for the next cycle rather than trying to resume mid-clip.
                _wasPausedLastFrame = false;
            }

            if (_drawVisionDebug) DrawDebugVision();
            if (!_isActive) return;
            if (Entry.IsPlayerDead)
            {
                if (_isCountingDown) StopCountdown();
                // Stop all boss sounds when player dies
                if (_wasTurning)
                {
                    API.StopSound(TURN_SOUND_NAME);
                    _wasTurning = false;
                }
                if (_isWaitingToTurn)
                {
                    API.StopSound(WARNING_SOUND_NAME);
                    API.StopSound(WARNING_LINE_SOUND_NAME);
                    _isWaitingToTurn = false;
                    _warningTimer = 0f;
                }
                return;
            }

            if (_activationTimer < _activationDelay) { _activationTimer += dt; return; }

            if (_isWaitingToTurn)
            {
                _warningTimer += dt;
                if (_warningTimer >= _warningDelay)
                {
                    _isWaitingToTurn = false;
                    _warningTimer = 0f;
                    _isTurning = true;
                    _isWatching = _pendingWatchState;
                    _targetYRotation = _isWatching ? _watchingYaw : _restingYaw;
                    UpdateLights(_isWatching ? _colorRed : _colorGreen, _isWatching ? _redIntensities : _greenIntensities);
                    if (!_isWatching) StopCountdown();
                }
                return;
            }

            // Handle warning line sound after turn towards player completes
            if (_isWaitingForWarningLine)
            {
                _warningLineTimer += dt;
                if (_warningLineTimer >= _warningLineAfterTurnDelay)
                {
                    _isWaitingForWarningLine = false;
                    _warningLineTimer = 0f;
                    // Play warning line sound
                    string randomWarningLine = WARNING_LINE_SOUND_PATHS[_warningRandom.Next(WARNING_LINE_SOUND_PATHS.Length)];
                    string uniqueName = WARNING_LINE_SOUND_NAME + "_" + DateTime.Now.Ticks;
                    API.PlaySound(uniqueName, randomWarningLine, false);
                    API.SetSoundVolume(uniqueName, _warningLineVolume);
                }
            }

            if (!_isTurning)
            {
                _timer += dt;
                if (_timer >= _rotationInterval)
                {
                    _timer = 0f;
                    _pendingWatchState = !_isWatching;

                    if (_pendingWatchState)
                    {
                        // Play warning sound (boss is about to turn towards player)
                        string randomWarning = WARNING_SOUND_PATHS[_warningRandom.Next(WARNING_SOUND_PATHS.Length)];
                        API.PlaySound(WARNING_SOUND_NAME, randomWarning, false);
                        API.SetSoundVolume(WARNING_SOUND_NAME, _warningSoundVolume);
                        _isWaitingToTurn = true;
                        _warningTimer = 0f;
                    }
                    else
                    {
                        // Turn away immediately (no warning delay)
                        _isTurning = true;
                        _isWatching = false;
                        _targetYRotation = _restingYaw;
                        UpdateLights(_colorGreen, _greenIntensities);
                        StopCountdown();
                    }
                }
                if (_isWatching) UpdateDetection(dt);
            }

            if (_isTurning)
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
                    // Start warning line timer when boss finishes turning towards player
                    if (_isWatching)
                    {
                        _isWaitingForWarningLine = true;
                        _warningLineTimer = 0f;
                    }
                }
                else { _currentYRotation += Math.Sign(angleDiff) * step; }
                Vec3 rot = API.GetRotation(Entity); rot.Y = _currentYRotation; API.SetRotation(Entity, rot);
            }
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
                    
                    // Trigger the cinematic "turn to back" immediately upon activation
                    boss._isTurning = true;
                    boss._targetYRotation = boss._restingYaw;
                    boss._isWatching = false; // Turn green as we turn away
                    
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
            if (PlayerMovement.IsPlayerInvisibleToEnemies()) { if (_isCountingDown) StopCountdown(); return; }
            Vec3 bPos = API.GetPosition(Entity); Vec3 pPos = API.GetPosition(player);
            if (Math.Abs(pPos.Y - bPos.Y) > _verticalTolerance) { if (_isCountingDown) StopCountdown(); return; }
            float dx = pPos.X - bPos.X; float dz = pPos.Z - bPos.Z; float dist = (float)Math.Sqrt(dx * dx + dz * dz);
            if (dist < _detectionRange)
            {
                float yawRad = _currentYRotation * (float)Math.PI / 180.0f;
                float fx = (float)Math.Sin(yawRad); float fz = (float)Math.Cos(yawRad);
                if (_inverseForward) { fx = -fx; fz = -fz; }

                float tx = dx / dist;
                float tz = dz / dist;

                float dot = tx * fx + tz * fz;
                float cosHalf = (float)Math.Cos((_detectionAngle * 0.5f) * Math.PI / 180.0);
                if (log) Console.WriteLine($"[BossHideSeek] Watch: Dist:{dist:F1}, Dot:{dot:F2} (Target:{cosHalf:F2})");
                if (dot > cosHalf)
                {
                    if (!_isCountingDown)
                    {
                        _isCountingDown = true;
                        _catchTimer = _catchDelay;
                        ShowWarningText(true);
                        Console.WriteLine("[BossHideSeek] >>> SPOTTED! <<<");
                    }
                    _catchTimer -= dt;
                    UpdateWarningText(_catchTimer);
                    if (_catchTimer <= 0f && !_hasDealtDamage)
                    {
                        _hasDealtDamage = true;
                        ShowWarningText(false);
                        Console.WriteLine("[BossHideSeek] !!! CAUGHT !!!");
                        PlayerManager.NotifyPlayerCaught(Entity);
                    }
                    return;
                }
            }
            if (_isCountingDown) StopCountdown();
        }

        private void StopCountdown() { if (!_isCountingDown) return; _isCountingDown = false; _catchTimer = _catchDelay; ShowWarningText(false); if (_showDebugLogs) Console.WriteLine("[BossHideSeek] Lost Sight."); }
        
        public void OnPlayerRespawned() 
        { 
            _isActive = false; 

            // Reset rotation to initial cinematic state
            _currentYRotation = _initialYaw;
            _targetYRotation = _initialYaw;
            if (API.HasTransform(Entity))
            {
                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
            }

            ResetToRestingState(false); 

            // Reset to cinematic red start
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

        private void ShowWarningText(bool show) { if (_warningText == 0 || !API.HasText(_warningText)) return; var c = API.GetTextColor(_warningText); c.W = show ? 1f : 0f; API.SetTextColor(_warningText, c); }
        private void UpdateWarningText(float remaining) { if (_warningText == 0 || !API.HasText(_warningText)) return; int s = (int)System.Math.Ceiling(System.Math.Max(0.0f, remaining)); API.SetText(_warningText, "Spotted! HIDE in " + s + "s!"); }
        private Vec3 ParseVec3(string csv, Vec3 def) { try { string[] p = csv.Split(','); return new Vec3(float.Parse(p[0]), float.Parse(p[1]), float.Parse(p[2])); } catch { return def; } }
        public void OnDestroy() { if (_wasTurning) { API.StopSound(TURN_SOUND_NAME); _wasTurning = false; } API.StopSound(WARNING_SOUND_NAME); API.StopSound(WARNING_LINE_SOUND_NAME); if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity); PlayerManager.UnregisterEnemy(this); }
    }
}
