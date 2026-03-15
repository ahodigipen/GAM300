using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Waypoint indicator that uses the DetectionRing model entity.
    /// - Groups keys: all Key1_* must be collected before targeting Key2_* keys
    /// - Uses 3D distance (XYZ) for proximity detection
    /// - Opacity fades smoothly using a smoothstep curve (nearly invisible far away,
    ///   gradually brighter as the player closes in)
    /// - Never exceeds _maxOpacity so it is never distractingly bright
    /// </summary>
    public class WaypointIndicator
    {
        public ulong Entity;

        // ── Key definitions ──────────────────────────────────────────────────
        private static readonly string[] KEY_NAMES = {
            "Key1_1", "Key1_2", "Key1_3", "Key1_sr1", "Key1_Prison",   // Group 0  (indices 0-4)
            "Key2_1", "Key2_2", "Key2_3", "Key2_sr1", "Key2_sr2"       // Group 1  (indices 5-9)
        };

        private static readonly string[] KEY_VARIANTS = {
            "MainDoor", "MainDoor", "MainDoor", "SmallDoor", "SmallDoor",   // Group 0
            "MainDoor", "MainDoor", "MainDoor", "SmallDoor", "SmallDoor"    // Group 1
        };

        // Group 0 = Key1_*, Group 1 = Key2_*
        // Indices correspond to KEY_NAMES above
        private static readonly int[][] KEY_GROUPS = {
            new int[] { 0, 1, 2, 3, 4 },    // Key1_1, Key1_2, Key1_3, Key1_sr1, Key1_Prison
            new int[] { 5, 6, 7, 8, 9 }     // Key2_1, Key2_2, Key2_3, Key2_sr1, Key2_sr2
        };

        // ── Key tracking ─────────────────────────────────────────────────────
        private ulong[] _keyEntities;
        private Vec3[]  _keyOriginalPositions;
        private bool[]  _keyCollected;
        private int     _currentTargetIndex = -1;
        private int     _currentGroup       = 0;   // 0 = Key1 group, 1 = Key2 group

        // ── DetectionRing model entity ────────────────────────────────────────
        private ulong _ringEntity;
        private Vec3  _ringScale;

        // ── Optional key-type icon sprite ─────────────────────────────────────
        private ulong  _keyIconEntity;
        private string _currentKeyIconVariant = "";

        // ── Ring transform ────────────────────────────────────────────────────
        private float _heightOffset       = 0.3f;
        private float _currentYawDeg      = 0f;
        private float _rotationSmoothSpeed = 6.0f;

        // ── Pulse ─────────────────────────────────────────────────────────────
        private float _pulseTimer = 0f;
        private float _pulseSpeed = 2.5f;

        // ── Re-target timer ───────────────────────────────────────────────────
        private float _retargetTimer    = 0f;
        private float _retargetInterval = 1.0f;

        // ── Opacity / visibility ──────────────────────────────────────────────
        // 3D radius within which the indicator becomes visible at all
        private float _activationRadius  = 20.0f;
        // 3D radius within which the indicator reaches max opacity
        private float _fullOpacityRadius = 8.0f;
        // Hard cap — keeps the ring from being distractingly bright
        private float _maxOpacity        = 0.55f;
        // Current smoothed opacity value
        private float _currentOpacity    = 0f;
        // Exponential-lerp speed for opacity transitions (higher = snappier)
        private float _opacityFadeSpeed  = 2.5f;
        // Max vertical (Y) distance allowed before the indicator is suppressed entirely.
        // Prevents keys on floors above/below from activating the indicator.
        private float _maxVerticalDistance = 5.0f;

        // ── Hidden position ───────────────────────────────────────────────────
        private static readonly Vec3 HIDDEN_POS = new Vec3(0f, -9999f, 0f);

        // ── Textures ──────────────────────────────────────────────────────────
        private const string TEX_MAIN_DOOR_KEY  = "Resources/Textures/PlayerUI/UI_Key.png";
        private const string TEX_SMALL_DOOR_KEY = "Resources/Textures/PlayerUI/KeyTutorial.png";

        // ─────────────────────────────────────────────────────────────────────

        public void OnStart(string jsonParams)
        {
            _keyEntities          = new ulong[KEY_NAMES.Length];
            _keyOriginalPositions = new Vec3[KEY_NAMES.Length];
            _keyCollected         = new bool[KEY_NAMES.Length];

            int foundCount = 0;
            for (int i = 0; i < KEY_NAMES.Length; i++)
            {
                _keyEntities[i] = API.FindEntity(KEY_NAMES[i]);
                if (_keyEntities[i] != 0 && API.HasTransform(_keyEntities[i]))
                {
                    _keyOriginalPositions[i] = API.GetPosition(_keyEntities[i]);
                    _keyCollected[i]         = false;
                    foundCount++;
                }
                else
                {
                    _keyCollected[i] = true; // treat missing entity as already collected
                }
            }

            _ringEntity = API.FindEntity("DetectionRing");
            if (_ringEntity != 0 && API.HasTransform(_ringEntity))
            {
                _ringScale = API.GetScale(_ringEntity);
                API.SetPosition(_ringEntity, HIDDEN_POS);
                API.SetModelOpacity(_ringEntity, 0f);
                API.Log($"[WaypointIndicator] DetectionRing found. Scale: ({_ringScale.X:F1},{_ringScale.Y:F1},{_ringScale.Z:F1})");
            }
            else
            {
                API.Log("[WaypointIndicator] WARNING: DetectionRing entity not found!");
            }

            _keyIconEntity = API.FindEntity("WaypointKeyIcon");
            if (_keyIconEntity != 0 && API.HasSprite(_keyIconEntity))
            {
                API.SetSpriteAlpha(_keyIconEntity, 0f);
                API.Log("[WaypointIndicator] WaypointKeyIcon sprite found.");
            }

            // Start on group 0 (Key1_*)
            _currentGroup = 0;
            FindNearestTarget();
            API.Log($"[WaypointIndicator] Tracking {foundCount} keys across {KEY_GROUPS.Length} groups.");
        }

        public void OnUpdate(float dt)
        {
            if (_ringEntity == 0) return;

            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            if (playerEntity == 0) return;

            CheckCollectedKeys();
            AdvanceGroupIfComplete();

            // If the current target was just collected, re-find immediately
            if (_currentTargetIndex >= 0 && _keyCollected[_currentTargetIndex])
                FindNearestTarget();

            // Periodic re-evaluation
            _retargetTimer += dt;
            if (_retargetTimer >= _retargetInterval)
            {
                _retargetTimer = 0f;
                FindNearestTarget();
            }

            // ── No target: fade out then hide ──
            if (_currentTargetIndex < 0)
            {
                _currentOpacity = ExpLerp(_currentOpacity, 0f, _opacityFadeSpeed, dt);
                API.SetModelOpacity(_ringEntity, _currentOpacity);
                if (_currentOpacity < 0.01f)
                {
                    API.SetPosition(_ringEntity, HIDDEN_POS);
                    API.SetModelOpacity(_ringEntity, 0f);
                }
                if (_keyIconEntity != 0 && API.HasSprite(_keyIconEntity))
                    API.SetSpriteAlpha(_keyIconEntity, 0f);
                return;
            }

            // ── Distance-based opacity (3D) ──
            Vec3 playerPos = API.GetPosition(playerEntity);
            Vec3 keyPos    = _keyOriginalPositions[_currentTargetIndex];

            float dx = keyPos.X - playerPos.X;
            float dy = keyPos.Y - playerPos.Y;
            float dz = keyPos.Z - playerPos.Z;

            // Suppress entirely if the key is too far above or below the player
            float targetOpacity = 0f;
            if (Math.Abs(dy) <= _maxVerticalDistance)
            {
                float dist = (float)Math.Sqrt(dx * dx + dy * dy + dz * dz);
                targetOpacity = ComputeTargetOpacity(dist);
            }

            // Exponential-lerp for a silky smooth fade
            _currentOpacity = ExpLerp(_currentOpacity, targetOpacity, _opacityFadeSpeed, dt);

            // Hide off-screen when fully transparent
            if (_currentOpacity < 0.01f)
            {
                API.SetPosition(_ringEntity, HIDDEN_POS);
                API.SetModelOpacity(_ringEntity, 0f);
                if (_keyIconEntity != 0 && API.HasSprite(_keyIconEntity))
                    API.SetSpriteAlpha(_keyIconEntity, 0f);
                return;
            }

            API.SetModelOpacity(_ringEntity, _currentOpacity);
            UpdateKeyTypeIcon(_currentTargetIndex, _currentOpacity);

            // ── Rotation toward key (XZ plane) ──
            float targetYawRad = (float)Math.Atan2(dx, dz);
            float targetYawDeg = targetYawRad * (180f / (float)Math.PI);
            _currentYawDeg = LerpAngleDeg(_currentYawDeg, targetYawDeg, _rotationSmoothSpeed * dt);

            // ── Pulse scale ──
            _pulseTimer += dt * _pulseSpeed;
            float pulse = 1.0f + 0.08f * (float)Math.Sin(_pulseTimer);

            // ── Position + rotation + scale ──
            Vec3 ringPos = new Vec3(playerPos.X, playerPos.Y + _heightOffset, playerPos.Z);
            API.SetPosition(_ringEntity, ringPos);

            Vec3 rot = API.GetRotation(_ringEntity);
            API.SetRotation(_ringEntity, new Vec3(rot.X, _currentYawDeg + 180f, rot.Z));

            API.SetScale(_ringEntity, new Vec3(
                _ringScale.X * pulse,
                _ringScale.Y * pulse,
                _ringScale.Z * pulse
            ));
        }

        // ── Helpers ───────────────────────────────────────────────────────────

        /// <summary>
        /// Advance to the next group once every key in the current group is collected.
        /// </summary>
        private void AdvanceGroupIfComplete()
        {
            if (_currentGroup >= KEY_GROUPS.Length) return;

            bool allDone = true;
            foreach (int idx in KEY_GROUPS[_currentGroup])
            {
                if (!_keyCollected[idx]) { allDone = false; break; }
            }

            if (allDone && _currentGroup < KEY_GROUPS.Length - 1)
            {
                _currentGroup++;
                _currentTargetIndex = -1; // force re-find in new group
                API.Log($"[WaypointIndicator] Group {_currentGroup - 1} complete. Switching to group {_currentGroup} (Key2_*).");
            }
        }

        /// <summary>
        /// Maps 3D distance to a target opacity using a smoothstep curve.
        /// Returns 0 beyond activation radius, rises smoothly to _maxOpacity.
        /// </summary>
        private float ComputeTargetOpacity(float dist)
        {
            if (dist >= _activationRadius) return 0f;
            if (dist <= _fullOpacityRadius) return _maxOpacity;

            // Smoothstep: 0 at activation edge, 1 at full-opacity radius
            float t = 1f - (dist - _fullOpacityRadius) / (_activationRadius - _fullOpacityRadius);
            t = t * t * (3f - 2f * t); // smoothstep
            return t * _maxOpacity;
        }

        /// <summary>
        /// Exponential lerp — produces a smooth organic fade that slows as it nears the target.
        /// </summary>
        private static float ExpLerp(float current, float target, float speed, float dt)
        {
            return target + (current - target) * (float)Math.Exp(-speed * dt);
        }

        private void UpdateKeyTypeIcon(int targetIndex, float opacity)
        {
            if (_keyIconEntity == 0 || !API.HasSprite(_keyIconEntity)) return;

            string variant = KEY_VARIANTS[targetIndex];
            if (variant != _currentKeyIconVariant)
            {
                _currentKeyIconVariant = variant;
                string tex = (variant == "SmallDoor") ? TEX_SMALL_DOOR_KEY : TEX_MAIN_DOOR_KEY;
                API.SetSpriteTexture(_keyIconEntity, tex);
                API.Log($"[WaypointIndicator] Key type icon changed to: {variant}");
            }

            API.SetSpriteAlpha(_keyIconEntity, opacity);
        }

        private void CheckCollectedKeys()
        {
            for (int i = 0; i < _keyEntities.Length; i++)
            {
                if (_keyCollected[i]) continue;
                if (_keyEntities[i] == 0) { _keyCollected[i] = true; continue; }
                if (API.GetPosition(_keyEntities[i]).Y < -50f)
                {
                    _keyCollected[i] = true;
                    API.Log($"[WaypointIndicator] Collected: {KEY_NAMES[i]}");
                }
            }
        }

        /// <summary>
        /// Finds the nearest uncollected key within the current group only.
        /// </summary>
        private void FindNearestTarget()
        {
            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            if (playerEntity == 0) { _currentTargetIndex = -1; return; }

            if (_currentGroup >= KEY_GROUPS.Length) { _currentTargetIndex = -1; return; }

            Vec3  playerPos  = API.GetPosition(playerEntity);
            float bestDistSq = float.MaxValue;
            int   bestIdx    = -1;

            foreach (int i in KEY_GROUPS[_currentGroup])
            {
                if (_keyCollected[i] || _keyEntities[i] == 0) continue;

                float dx = _keyOriginalPositions[i].X - playerPos.X;
                float dy = _keyOriginalPositions[i].Y - playerPos.Y;
                float dz = _keyOriginalPositions[i].Z - playerPos.Z;
                float distSq = dx * dx + dy * dy + dz * dz;

                if (distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    bestIdx    = i;
                }
            }

            if (bestIdx != _currentTargetIndex)
            {
                _currentTargetIndex = bestIdx;
                if (bestIdx >= 0)
                    API.Log($"[WaypointIndicator] Target: {KEY_NAMES[bestIdx]} (group {_currentGroup}, dist {Math.Sqrt(bestDistSq):F1}m)");
                else
                    API.Log($"[WaypointIndicator] No targets in group {_currentGroup}.");
            }
        }

        private float LerpAngleDeg(float current, float target, float t)
        {
            float diff = target - current;
            while (diff >  180f) diff -= 360f;
            while (diff < -180f) diff += 360f;
            t = Math.Max(0f, Math.Min(1f, t));
            return current + diff * t;
        }
    }
}
