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

        [Boom.EditorExposed("Red Intensity", "Intensity when watching")]
        private float _redIntensity = 5.0f;

        [Boom.EditorExposed("Green Intensity", "Intensity when resting")]
        private float _greenIntensity = 2.0f;

        [Boom.EditorExposed("Warning Text Entity", "UI text name")]
        private string _warningTextEntityName = "UI_WarningText";

        [Boom.EditorExposed("Enable Debug Logs", "Show info in console")]
        private bool _showDebugLogs = true;

        [Boom.EditorExposed("Draw Vision Debug", "Draw a red line in scene")]
        private bool _drawVisionDebug = true;

        private List<ulong> _lightEntities = new List<ulong>();
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

        // Turn sound
        private const string TURN_SOUND_NAME = "BossTurnLoop";
        private const string TURN_SOUND_PATH = "Resources/Audio/BossTurn_Loop.wav";
        private bool _wasTurning = false;

        [Boom.EditorExposed("Turn Sound Volume", "Volume of the turning sound (0.0 - 1.0)")]
        private float _turnSoundVolume = 1.0f;

        // Warning sound (plays when turning towards player)
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

        [Boom.EditorExposed("Warning Sound Volume", "Volume of the warning sound (0.0 - 1.0)")]
        private float _warningSoundVolume = 1.0f;

        [Boom.EditorExposed("Warning Delay", "Seconds to wait after warning before turning")]
        private float _warningDelay = 3.0f;

        private bool _isWaitingToTurn = false;
        private float _warningTimer = 0f;
        private bool _pendingWatchState = false;

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            if (!API.HasTransform(Entity)) return;
            _colorRed = ParseVec3(_redColorCSV, new Vec3(1, 0, 0));
            _colorGreen = ParseVec3(_greenColorCSV, new Vec3(0, 1, 0));
            _lightEntities.Clear();
            string[] names = _lightNamesCSV.Split(',');
            foreach (var name in names) { ulong id = API.FindEntity(name.Trim()); if (id != 0) _lightEntities.Add(id); }
            _warningText = API.FindEntity(_warningTextEntityName);
            ResetToRestingState();
            PlayerManager.RegisterEnemy(this);
        }

        private void ResetToRestingState()
        {
            _isWatching = false;
            _currentYRotation = _restingYaw;
            _targetYRotation = _restingYaw;
            _timer = 0f;
            _activationTimer = 0f;
            _isTurning = false;
            _isWaitingToTurn = false;
            _warningTimer = 0f;
            _hasDealtDamage = false;
            _catchTimer = _catchDelay;
            // Stop turn sound if playing
            if (_wasTurning)
            {
                API.StopSound(TURN_SOUND_NAME);
                _wasTurning = false;
            }
            // Stop warning sound
            API.StopSound(WARNING_SOUND_NAME);
            StopCountdown();
            Vec3 rot = API.GetRotation(Entity);
            rot.Y = _currentYRotation;
            API.SetRotation(Entity, rot);
            UpdateLights(_colorGreen, _greenIntensity);
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;
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
                    _isWaitingToTurn = false;
                    _warningTimer = 0f;
                }
                return;
            }

            if (_activationTimer < _activationDelay) { _activationTimer += dt; return; }

            // Warning phase - play warning sound and wait before turning towards player
            if (_isWaitingToTurn)
            {
                _warningTimer += dt;
                if (_warningTimer >= _warningDelay)
                {
                    _isWaitingToTurn = false;
                    _warningTimer = 0f;
                    // Now actually start the turn
                    _isTurning = true;
                    _isWatching = _pendingWatchState;
                    _targetYRotation = _isWatching ? _watchingYaw : _restingYaw;
                    UpdateLights(_isWatching ? _colorRed : _colorGreen, _isWatching ? _redIntensity : _greenIntensity);
                }
                return; // Don't do anything else while waiting
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
                        // About to turn towards player - play warning first, then wait
                        string randomWarning = WARNING_SOUND_PATHS[_warningRandom.Next(WARNING_SOUND_PATHS.Length)];
                        API.PlaySound(WARNING_SOUND_NAME, randomWarning, false);
                        API.SetSoundVolume(WARNING_SOUND_NAME, _warningSoundVolume);
                        _isWaitingToTurn = true;
                        _warningTimer = 0f;
                    }
                    else
                    {
                        // Turning away from player - no warning, turn immediately
                        _isTurning = true;
                        _isWatching = false;
                        _targetYRotation = _restingYaw;
                        UpdateLights(_colorGreen, _greenIntensity);
                        StopCountdown();
                    }
                }
                if (_isWatching) UpdateDetection(dt);
            }

            if (_isTurning)
            {
                // Start turn sound when turning begins
                if (!_wasTurning)
                {
                    API.PlaySound(TURN_SOUND_NAME, TURN_SOUND_PATH, true);
                    API.SetSoundVolume(TURN_SOUND_NAME, _turnSoundVolume);
                    _wasTurning = true;
                }

                float angleDiff = _targetYRotation - _currentYRotation;
                while (angleDiff > 180f) angleDiff -= 360f;
                while (angleDiff < -180f) angleDiff += 360f;
                float step = _rotationSpeed * dt;
                if (Math.Abs(angleDiff) <= step)
                {
                    _currentYRotation = _targetYRotation;
                    _isTurning = false;
                    // Stop turn sound when turning ends
                    API.StopSound(TURN_SOUND_NAME);
                    _wasTurning = false;
                }
                else { _currentYRotation += Math.Sign(angleDiff) * step; }
                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
            }
        }

        private void DrawDebugVision()
        {
            Vec3 pos = API.GetPosition(Entity);
            pos.Y += 1.5f;
            float yawRad = _currentYRotation * (float)Math.PI / 180.0f;
            float fx = (float)Math.Sin(yawRad);
            float fz = (float)Math.Cos(yawRad);
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
                    boss.UpdateLights(boss._isWatching ? boss._colorRed : boss._colorGreen, boss._isWatching ? boss._redIntensity : boss._greenIntensity);
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

            Vec3 bPos = API.GetPosition(Entity);
            Vec3 pPos = API.GetPosition(player);

            if (Math.Abs(pPos.Y - bPos.Y) > _verticalTolerance) { if (_isCountingDown) StopCountdown(); return; }

            float dx = pPos.X - bPos.X;
            float dz = pPos.Z - bPos.Z;
            float dist = (float)Math.Sqrt(dx * dx + dz * dz);

            if (dist < _detectionRange)
            {
                float yawRad = _currentYRotation * (float)Math.PI / 180.0f;
                float fx = (float)Math.Sin(yawRad);
                float fz = (float)Math.Cos(yawRad);
                if (_inverseForward) { fx = -fx; fz = -fz; }
                
                float tx = dx / dist;
                float tz = dz / dist;

                float dot = tx * fx + tz * fz;
                float cosHalf = (float)Math.Cos((_detectionAngle * 0.5f) * Math.PI / 180.0);

                if (log) Console.WriteLine($"[BossHideSeek] Watch: Dist:{dist:F1}, Dot:{dot:F2} (Target:{cosHalf:F2})");

                if (dot > cosHalf)
                {
                    if (!_isCountingDown) {
                        _isCountingDown = true;
                        _catchTimer = _catchDelay;
                        ShowWarningText(true);
                        Console.WriteLine("[BossHideSeek] >>> SPOTTED! <<<");
                    }
                    _catchTimer -= dt;
                    UpdateWarningText(_catchTimer);
                    if (_catchTimer <= 0f && !_hasDealtDamage) {
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

        private void StopCountdown()
        {
            if (!_isCountingDown) return;
            _isCountingDown = false;
            _catchTimer = _catchDelay;
            ShowWarningText(false);
            if (_showDebugLogs) Console.WriteLine("[BossHideSeek] Lost Sight.");
        }

        public void OnPlayerRespawned()
        {
            _isActive = false; 
            ResetToRestingState();
        }

        private void UpdateLights(Vec3 color, float intensity)
        {
            foreach (var l in _lightEntities)
            {
                if (API.HasSpotLight(l)) { API.SetSpotLightColor(l, color); API.SetSpotLightIntensity(l, intensity); }
                else if (API.HasPointLight(l)) { API.SetPointLightColor(l, color); API.SetPointLightIntensity(l, intensity); }
            }
        }

        private void ShowWarningText(bool show)
        {
            if (_warningText == 0 || !API.HasText(_warningText)) return;
            var c = API.GetTextColor(_warningText);
            c.W = show ? 1f : 0f;
            API.SetTextColor(_warningText, c);
        }

        private void UpdateWarningText(float remaining)
        {
            if (_warningText == 0 || !API.HasText(_warningText)) return;
            int s = (int)System.Math.Ceiling(System.Math.Max(0.0f, remaining));
            API.SetText(_warningText, "Spotted! HIDE in " + s + "s!");
        }

        private Vec3 ParseVec3(string csv, Vec3 def) { try { string[] p = csv.Split(','); return new Vec3(float.Parse(p[0]), float.Parse(p[1]), float.Parse(p[2])); } catch { return def; } }
        public void OnDestroy()
        {
            // Stop turn sound on destroy
            if (_wasTurning)
            {
                API.StopSound(TURN_SOUND_NAME);
                _wasTurning = false;
            }
            // Stop warning sound on destroy
            API.StopSound(WARNING_SOUND_NAME);
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            PlayerManager.UnregisterEnemy(this);
        }
    }
}
