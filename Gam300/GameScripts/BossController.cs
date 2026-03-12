using System;
using System;
using Boom;

namespace GameScripts
{
    // Simple boss controller: floating entity that turns periodically and uses VisionComponent to detect the player.
    public class BossController
    {
        public ulong Entity;

        [Boom.EditorExposed("Rotation Interval", "Time between rotations in seconds", 0.1f, 60f, true)]
        private float _rotationInterval = 3.0f;

        [Boom.EditorExposed("Rotation Angle", "Degrees to rotate each turn", 1f, 360f, true)]
        private float _rotationAngle = 180f;

        [Boom.EditorExposed("Rotation Speed", "Degrees per second for smooth rotation", 1f, 720f, true)]
        private float _rotationSpeed = 120f;

        [Boom.EditorExposed("Clockwise Rotation", "Rotate clockwise when true")]
        private bool _clockwise = true;

        private float _rotationTimer = 0f;
        private float _currentYRotation = 0f;
        private float _targetYRotation = 0f;
        private bool _isRotating = false;

        // Vision system to detect player
        private VisionComponent _vision;

        [Boom.EditorExposed("Detection Range", "Vision detection range for boss", 1f, 50f, true)]
        private float _detectionRange = 14f;

        [Boom.EditorExposed("Detection Angle", "Vision detection cone angle", 10f, 180f, true)]
        private float _detectionAngle = 90f;

        [Boom.EditorExposed("Detection Update Interval", "How often vision checks run", 0.01f, 1f, true)]
        private float _detectionUpdateInterval = 0.12f;

        private bool _hasAlerted = false;

        // --- Gaze / head / lighting / warning UI (merged from BossGazeController) ---
        [Boom.EditorExposed("Head Entity Name", "Name of head/rotating entity (Transform)")]
        private string _headEntityName = "BossHead";
        [Boom.EditorExposed("Look Target Name", "Entity to look at (e.g., Player)")]
        private string _lookTargetName = "Samurai";
        [Boom.EditorExposed("Spotlight Name", "Spotlight entity for boss light")]
        private string _spotlightName = "BossLight";

        [Boom.EditorExposed("Look Duration", "Seconds boss looks/searches", 0.1f, 60f, true)]
        private float _lookDuration = 2f;
        [Boom.EditorExposed("Rest Duration", "Seconds boss rests/looks away", 0.1f, 60f, true)]
        private float _restDuration = 3f;

        // Intro sequence durations
        [Boom.EditorExposed("Intro Look 1", "Intro immediate look duration", 0f, 60f, true)]
        private float _introLook1 = 3f;
        [Boom.EditorExposed("Intro Rest 1", "Intro rest duration", 0f, 60f, true)]
        private float _introRest1 = 1f;
        [Boom.EditorExposed("Intro Look 2", "Intro second look duration", 0f, 60f, true)]
        private float _introLook2 = 2f;
        [Boom.EditorExposed("Intro Rest 2", "Intro long rest before loop", 0f, 60f, true)]
        private float _introRest2 = 4f;

        [Boom.EditorExposed("Rotation Speed (Head)", "Degrees per second for head rotation", 0.1f, 720f, true)]
        private float _headRotationSpeed = 90f;

        [Boom.EditorExposed("Rest Light Intensity", "Light intensity when resting", 0f, 50f, true)]
        private float _lightIntensityRest = 2f;
        [Boom.EditorExposed("Alert Light Intensity", "Light intensity when alert", 0f, 50f, true)]
        private float _lightIntensityAlert = 5f;
        [Boom.EditorExposed("Light Transition Speed", "Speed of light color/intensity transitions", 0.1f, 20f, true)]
        private float _lightTransitionSpeed = 2f;

        // Colors as simple RGB components
        [Boom.EditorExposed("Rest Light Color R", "Rest light color R component", 0f, 1f, true)]
        private float _restLightR = 1f;
        [Boom.EditorExposed("Rest Light Color G", "Rest light color G component", 0f, 1f, true)]
        private float _restLightG = 1f;
        [Boom.EditorExposed("Rest Light Color B", "Rest light color B component", 0f, 1f, true)]
        private float _restLightB = 1f;

        [Boom.EditorExposed("Alert Light Color R", "Alert light color R component", 0f, 1f, true)]
        private float _alertLightR = 1f;
        [Boom.EditorExposed("Alert Light Color G", "Alert light color G component", 0f, 1f, true)]
        private float _alertLightG = 0f;
        [Boom.EditorExposed("Alert Light Color B", "Alert light color B component", 0f, 1f, true)]
        private float _alertLightB = 0f;

        [Boom.EditorExposed("Warning Text Entity", "Name of Text entity to show warnings")]
        private string _warningTextEntityName = "UI_WarningText";
        [Boom.EditorExposed("Catch Delay", "Seconds until player is caught when looked at while standing", 0.1f, 30f, true)]
        private float _catchDelay = 3f;

        // Internal handles for gaze
        private ulong _head = 0;
        private ulong _lookTarget = 0;
        private ulong _light = 0;
        private ulong _warningText = 0;

        private float _restYaw = 0f;
        private float _currentYaw = 0f;
        private float _targetYaw = 0f;

        private enum GazePhase { Intro1Look, Intro1Rest, Intro2Look, Intro2Rest, NormalLook, NormalRest }
        private GazePhase _gazePhase;
        private float _gazePhaseTimer = 0f;

        private float _catchTimer = 0f;
        private bool _isCountingDown = false;

        public void OnStart(string jsonParams)
        {
            if (!API.HasTransform(Entity)) return;

            _currentYRotation = API.GetRotation(Entity).Y;
            _targetYRotation = _currentYRotation;

            // Initialize vision
            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(jsonParams);

            _vision.SetDetectionRange(_detectionRange);
            _vision.SetDetectionAngle(_detectionAngle);
            _vision.SetUpdateInterval(_detectionUpdateInterval);

            _vision.EnableDebugLOS(false);
            _vision.EnableDebugReasons(false);

            // Initialize gaze/lighting/warning handles
            _head = API.FindEntity(_headEntityName);
            _lookTarget = API.FindEntity(_lookTargetName);
            _light = API.FindEntity(_spotlightName);
            _warningText = API.FindEntity(_warningTextEntityName);

            if (_head != 0 && API.HasTransform(_head))
            {
                _restYaw = API.GetRotation(_head).Y;
                _currentYaw = _restYaw;
                _targetYaw = _restYaw;
            }
            // hide warning text initially if present
            if (_warningText != 0 && API.HasText(_warningText))
            {
                var c = API.GetTextColor(_warningText);
                c.W = 0f;
                API.SetTextColor(_warningText, c);
                API.SetText(_warningText, "");
            }

            _gazePhase = GazePhase.Intro1Look;
            _gazePhaseTimer = 0f;
            _catchTimer = _catchDelay;

        }

        // ---------------- Gaze / Lighting / Warning Logic ----------------
        private void UpdateGaze(float dt)
        {
            if (_head == 0) return;

            _gazePhaseTimer += dt;

            switch (_gazePhase)
            {
                case GazePhase.Intro1Look:
                    GazeUpdateLook(dt);
                    if (_gazePhaseTimer >= _introLook1) { _gazePhase = GazePhase.Intro1Rest; _gazePhaseTimer = 0f; }
                    break;
                case GazePhase.Intro1Rest:
                    GazeUpdateRest(dt);
                    if (_gazePhaseTimer >= _introRest1) { _gazePhase = GazePhase.Intro2Look; _gazePhaseTimer = 0f; }
                    break;
                case GazePhase.Intro2Look:
                    GazeUpdateLook(dt);
                    if (_gazePhaseTimer >= _introLook2) { _gazePhase = GazePhase.Intro2Rest; _gazePhaseTimer = 0f; }
                    break;
                case GazePhase.Intro2Rest:
                    GazeUpdateRest(dt);
                    if (_gazePhaseTimer >= _introRest2) { _gazePhase = GazePhase.NormalLook; _gazePhaseTimer = 0f; }
                    break;
                case GazePhase.NormalLook:
                    GazeUpdateLook(dt);
                    if (_gazePhaseTimer >= _lookDuration) { _gazePhase = GazePhase.NormalRest; _gazePhaseTimer = 0f; }
                    break;
                case GazePhase.NormalRest:
                    GazeUpdateRest(dt);
                    if (_gazePhaseTimer >= _restDuration) { _gazePhase = GazePhase.NormalLook; _gazePhaseTimer = 0f; }
                    break;
            }
        }

        private void GazeUpdateLook(float dt)
        {
            // Determine target yaw toward lookTarget if available
            if (_lookTarget != 0 && API.HasTransform(_lookTarget))
            {
                Vec3 headPos = API.GetPosition(_head);
                Vec3 tgtPos = API.GetPosition(_lookTarget);
                float dx = tgtPos.X - headPos.X;
                float dz = tgtPos.Z - headPos.Z;
                float yaw = (float)(System.Math.Atan2(dx, dz) * 180.0 / System.Math.PI);
                _targetYaw = yaw;
            }

            RotateHeadTowardsTarget(dt);
            SmoothLightTransition(new Vec3(_alertLightR, _alertLightG, _alertLightB), _lightIntensityAlert, dt);

            // Detection: if player not crouching, start countdown
            if (!PlayerMovement.IsCrouching())
            {
                if (!_isCountingDown)
                {
                    _isCountingDown = true;
                    _catchTimer = _catchDelay;
                    ShowWarningText(true);
                }

                _catchTimer -= dt;
                UpdateWarningText(_catchTimer);

                if (_catchTimer <= 0f)
                {
                    ShowWarningText(false);
                    Entry.TriggerPlayerDeath();
                }
            }
            else
            {
                if (_isCountingDown)
                {
                    _isCountingDown = false;
                    ShowWarningText(false);
                }
            }
        }

        private void GazeUpdateRest(float dt)
        {
            _targetYaw = _restYaw;
            RotateHeadTowardsTarget(dt);
            SmoothLightTransition(new Vec3(_restLightR, _restLightG, _restLightB), _lightIntensityRest, dt);

            if (_isCountingDown)
            {
                _isCountingDown = false;
                ShowWarningText(false);
            }
        }

        private void RotateHeadTowardsTarget(float dt)
        {
            float diff = _targetYaw - _currentYaw;
            while (diff > 180f) diff -= 360f;
            while (diff < -180f) diff += 360f;

            float step = _headRotationSpeed * dt;
            if (System.Math.Abs(diff) <= step)
            {
                _currentYaw = _targetYaw;
            }
            else
            {
                _currentYaw += System.Math.Sign(diff) * step;
                while (_currentYaw >= 360f) _currentYaw -= 360f;
                while (_currentYaw < 0f) _currentYaw += 360f;
            }

            Vec3 r = API.GetRotation(_head);
            r.Y = _currentYaw;
            API.SetRotation(_head, r);
        }

        private void SmoothLightTransition(Vec3 targetColor, float targetIntensity, float dt)
        {
            if (_light == 0 || !API.HasSpotLight(_light)) return;

            Vec3 curColor = API.GetSpotLightColor(_light);
            Vec3 newColor = new Vec3(
                Lerp(curColor.X, targetColor.X, _lightTransitionSpeed * dt),
                Lerp(curColor.Y, targetColor.Y, _lightTransitionSpeed * dt),
                Lerp(curColor.Z, targetColor.Z, _lightTransitionSpeed * dt)
            );
            API.SetSpotLightColor(_light, newColor);

            float curIntensity = API.GetSpotLightIntensity(_light);
            float ni = Lerp(curIntensity, targetIntensity, _lightTransitionSpeed * dt);
            API.SetSpotLightIntensity(_light, ni);
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
            int secs = (int)System.Math.Ceiling(System.Math.Max(0.0f, remaining));
            API.SetText(_warningText, "About to be caught in " + secs + "s!");
        }

        private float Lerp(float a, float b, float t)
        {
            return a + (b - a) * System.Math.Max(0f, System.Math.Min(1f, t));
        }
        

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Update vision every frame (internally throttled by its interval)
            _vision?.OnUpdate(dt);

            // Only rotate when not actively alerting the player
            if (_vision == null || _vision.GetState() != VisionComponent.VisionState.Alert)
            {
                UpdateRotation(dt);
            }

            // Update gaze behavior (head, light, warning)
            UpdateGaze(dt);
        }

        private void UpdateRotation(float dt)
        {
            if (!_isRotating)
            {
                _rotationTimer += dt;
                if (_rotationTimer >= _rotationInterval)
                {
                    _rotationTimer = 0f;
                    if (_clockwise) _targetYRotation += _rotationAngle; else _targetYRotation -= _rotationAngle;

                    // normalize
                    if (_targetYRotation >= 360f) _targetYRotation -= 360f;
                    if (_targetYRotation < 0f) _targetYRotation += 360f;

                    _isRotating = true;
                }
            }

            if (_isRotating)
            {
                float diff = _targetYRotation - _currentYRotation;
                while (diff > 180f) diff -= 360f;
                while (diff < -180f) diff += 360f;

                float step = _rotationSpeed * dt;
                if (Math.Abs(diff) <= step)
                {
                    _currentYRotation = _targetYRotation;
                    _isRotating = false;
                }
                else
                {
                    _currentYRotation += Math.Sign(diff) * step;
                    while (_currentYRotation >= 360f) _currentYRotation -= 360f;
                    while (_currentYRotation < 0f) _currentYRotation += 360f;
                }

                Vec3 r = API.GetRotation(Entity);
                r.Y = _currentYRotation;
                API.SetRotation(Entity, r);
            }
        }

        private void OnPlayerDetected(ulong target, Vec3 pos)
        {
            if (_hasAlerted) return;
            _hasAlerted = true;

            // Snap to face player
            Vec3 bossPos = API.GetPosition(Entity);
            Vec3 dir = new Vec3(pos.X - bossPos.X, 0f, pos.Z - bossPos.Z);
            float dist = (float)Math.Sqrt(dir.X * dir.X + dir.Z * dir.Z);
            if (dist > 0f)
            {
                float yaw = (float)(Math.Atan2(dir.X, dir.Z) * 180.0 / Math.PI);
                _targetYRotation = yaw;
                _currentYRotation = yaw;
                _isRotating = false;
                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
            }

            // Notify player manager (treated as detection/capture)
            PlayerManager.NotifyPlayerCaught(Entity);
        }

        private void OnPlayerLost(ulong target, Vec3 lastPos)
        {
            _hasAlerted = false;
            // resume turning behavior
            _rotationTimer = 0f;
            _isRotating = false;
        }

        private void OnPlayerTracking(ulong target, Vec3 pos)
        {
            // Keep facing the player while visible
            Vec3 bossPos = API.GetPosition(Entity);
            Vec3 dir = new Vec3(pos.X - bossPos.X, 0f, pos.Z - bossPos.Z);
            float dist = (float)Math.Sqrt(dir.X * dir.X + dir.Z * dir.Z);
            if (dist > 0f)
            {
                float yaw = (float)(Math.Atan2(dir.X, dir.Z) * 180.0 / Math.PI);
                _targetYRotation = yaw;
                _currentYRotation = yaw;
                Vec3 rot = API.GetRotation(Entity);
                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
            }
        }

        public void OnDestroy()
        {
            _vision?.OnDestroy();
        }
    }
}
