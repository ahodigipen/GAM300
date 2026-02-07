using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    // Attach to a trigger volume positioned near the door.
    // Set the door entity name in the inspector.
    // IMPORTANT: Do NOT parent the door to the trigger - keep them as separate entities.
    //
    // Behavior:
    // - When the player with a key enters the trigger, both the trigger and door slide left
    //   by 'distance' meters at 'movespeed' m/s. Optionally consumes a key.
    // - If autoclose=true, when the player exits they slide back after 'closedelay' seconds.
    public class DoorTriggerLeft
    {
        public ulong Entity;

        // Config
        [Boom.EditorExposed("Door Name", "Name of the door entity (leave empty to use first child)")]
        private string _doorName = "";

        [Boom.EditorExposed("Required Key Type", "Key type required to open this door (e.g., 'key1', 'key2')")]
        private string _requiredKeyType = "key1";

        [Boom.EditorExposed("Slide Distance", "Distance the door slides in meters", 0.1f, 20f, true)]
        private float _slideDistance = 5.5f;   // meters

        [Boom.EditorExposed("Move Speed", "Speed of door movement in m/s", 0.1f, 10f, true)]
        private float _moveSpeed = 2.0f;       // m/s

        [Boom.EditorExposed("Consume Key", "Whether opening the door uses up a key")]
        private bool _consumeKey = true;

        [Boom.EditorExposed("Auto Close On Exit", "Whether the door closes when player leaves")]
        private bool _autoCloseOnExit = false;

        [Boom.EditorExposed("Close Delay", "Delay before auto-closing in seconds", 0f, 5f, true)]
        private float _closeDelay = 0f;

        // Audio
        [Boom.EditorExposed("Door Sound", "Sound played when door opens/closes")]
        private string _doorSoundPath = "Resources/Audio/unlock.wav";

        // Cached positions and direction
        private Vec3 _basePos;
        private Vec3 _targetPos;
        private Vec3 _leftDir; // unit vector

        // Door entity (found as child)
        private ulong _doorEntity = 0;
        private Vec3 _doorOffset; // Offset from trigger to door

        // "No Key" message
        private ulong _noKeyTextEntity = 0;
        private const string NO_KEY_MESSAGE = "This door is locked, need to find a key.";
        private enum MessageState { Hidden, Typing, Displaying, FadingOut }
        private MessageState _messageState = MessageState.Hidden;
        private float _typewriterTimer = 0f;
        private int _currentCharIndex = 0;
        private const float CHARS_PER_SECOND = 20f; // Typing speed
        private const float DISPLAY_DURATION = 2f; // How long to show full message
        private const float FADE_OUT_SPEED = 2f; // Fade out speed
        private float _displayTimer = 0f;
        private float _messageAlpha = 1f;

        // State
        private static readonly Dictionary<ulong, DoorTriggerLeft> s_instances = new Dictionary<ulong, DoorTriggerLeft>();
        private bool _opening = false;
        private bool _closing = false;
        private float _closeTimer = 0f;

        private bool _kWasDown = false;

        // Constants (GLFW)
        private const int KEY_K = 75;          // GLFW_KEY_K
        private const int KEY_LEFT_SHIFT = 340;

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            ParseParams(jsonParams);

            // Ensure trigger is configured
            if (!API.HasCollider(Entity))
            {
                API.Log("[DoorTriggerLeft] WARNING: Trigger entity has no collider!");
            }
            else if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
            }

            // Find the door entity (by name or as child)
            if (!string.IsNullOrEmpty(_doorName))
            {
                _doorEntity = API.FindEntity(_doorName);
                if (_doorEntity == 0)
                {
                    API.Log($"[DoorTriggerLeft] ERROR: Could not find door entity '{_doorName}'");
                    return;
                }
            }
            else
            {
                // Try to find first child
                // Note: If API doesn't support getting children, you must set the door name manually
                API.Log("[DoorTriggerLeft] WARNING: No door name specified. Please set the door entity name.");
                return;
            }

            // Cache base position and compute left direction from yaw
            _basePos = API.GetPosition(Entity);
            float yawDeg = API.GetRotation(Entity).Y;
            float yawRad = yawDeg * (float)Math.PI / 180f;

            // Forward = (sin(yaw), cos(yaw)); Left (CCW) = (-cos(yaw), sin(yaw))
            float fx = (float)Math.Sin(yawRad);
            float fz = (float)Math.Cos(yawRad);
            _leftDir = new Vec3(fz, 0f, -fx); // unit left vector in XZ

            // Target position = base + left * distance
            _targetPos = new Vec3(
                _basePos.X + _leftDir.X * _slideDistance,
                _basePos.Y,
                _basePos.Z + _leftDir.Z * _slideDistance
            );

            // Calculate and store offset from trigger to door
            Vec3 doorPos = API.GetPosition(_doorEntity);
            _doorOffset = new Vec3(
                doorPos.X - _basePos.X,
                doorPos.Y - _basePos.Y,
                doorPos.Z - _basePos.Z
            );

            API.Log($"[DoorTriggerLeft] Trigger basePos=({_basePos.X:F2},{_basePos.Y:F2},{_basePos.Z:F2}) targetPos=({_targetPos.X:F2},{_targetPos.Y:F2},{_targetPos.Z:F2})");
            API.Log($"[DoorTriggerLeft] Door offset=({_doorOffset.X:F2},{_doorOffset.Y:F2},{_doorOffset.Z:F2})");

            // Find and initialize the "no key" text entity
            _noKeyTextEntity = API.FindEntity("UI_NoKeyText");
            if (_noKeyTextEntity != 0 && API.HasText(_noKeyTextEntity))
            {
                API.SetText(_noKeyTextEntity, "");
                API.SetTextColor(_noKeyTextEntity, new Vec4(1, 1, 1, 0)); // White text, 0 alpha
                API.Log("[DoorTriggerLeft] Initialized UI_NoKeyText");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log("[DoorTriggerLeft] Registered trigger callbacks.");
        }

        public void OnUpdate(float dt)
        {
            if (_doorEntity == 0) return;

            // Update "no key" message typewriter effect
            UpdateNoKeyMessage(dt);

            // Handle delayed close
            if (_autoCloseOnExit && _closeDelay > 0f && !_opening && _closing)
            {
                _closeTimer -= dt;
                if (_closeTimer > 0f) return;
                _closeDelay = 0f; // start closing now (delay done)
            }

            // Current position
            Vec3 cur = API.GetPosition(Entity);

            if (_opening)
            {
                Vec3 next = MoveTowards(cur, _targetPos, _moveSpeed * dt);

                // Move trigger
                API.TeleportRigidBody(Entity, next);

                // Move door with offset
                Vec3 doorNext = new Vec3(
                    next.X + _doorOffset.X,
                    next.Y + _doorOffset.Y,
                    next.Z + _doorOffset.Z
                );
                API.TeleportRigidBody(_doorEntity, doorNext);

                if (NearlyEqual(next, _targetPos))
                {
                    API.TeleportRigidBody(Entity, _targetPos);
                    Vec3 doorTarget = new Vec3(
                        _targetPos.X + _doorOffset.X,
                        _targetPos.Y + _doorOffset.Y,
                        _targetPos.Z + _doorOffset.Z
                    );
                    API.TeleportRigidBody(_doorEntity, doorTarget);
                    _opening = false;
                    _closing = false;
                    API.Log("[DoorTriggerLeft] Door moved left (opened).");
                }
            }
            else if (_closing)
            {
                Vec3 next = MoveTowards(cur, _basePos, _moveSpeed * dt);

                // Move trigger
                API.TeleportRigidBody(Entity, next);

                // Move door with offset
                Vec3 doorNext = new Vec3(
                    next.X + _doorOffset.X,
                    next.Y + _doorOffset.Y,
                    next.Z + _doorOffset.Z
                );
                API.TeleportRigidBody(_doorEntity, doorNext);

                if (NearlyEqual(next, _basePos))
                {
                    API.TeleportRigidBody(Entity, _basePos);
                    Vec3 doorBase = new Vec3(
                        _basePos.X + _doorOffset.X,
                        _basePos.Y + _doorOffset.Y,
                        _basePos.Z + _doorOffset.Z
                    );
                    API.TeleportRigidBody(_doorEntity, doorBase);
                    _closing = false;
                    API.Log("[DoorTriggerLeft] Door returned (closed).");
                }
            }

            // Debug key testing
            bool kDown = API.IsKeyDown(KEY_K);
            if (kDown && !_kWasDown)
            {
                bool shift = API.IsKeyDown(KEY_LEFT_SHIFT);
                if (shift)
                {
                    // 3D positional version (subject to distance & mono asset rules)
                    var pos = API.GetPosition(_doorEntity);
                    API.PlaySoundAt("sfx_door_slide_open_3d", _doorSoundPath, pos, false);
                    API.SetSoundVolume("sfx_door_slide_open_3d", 1.0f);
                    API.Set3DMinMaxDistance("sfx_door_slide_open_3d", 1.5f, 35.0f);
                    API.Log("[DoorTriggerLeft] Shift+K: played 3D positional door SFX.");
                }
                else
                {
                    // 2D guaranteed-audible fallback (no attenuation)
                    API.PlaySound("sfx_door_slide_open_2d", _doorSoundPath, false);
                    API.SetSoundVolume("sfx_door_slide_open_2d", 1.0f);
                    API.Log("[DoorTriggerLeft] K: played 2D door SFX (always audible).");
                }
            }
            _kWasDown = kDown;
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            DoorTriggerLeft inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only player triggers this
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            if (!PlayerInventory.HasKey(inst._requiredKeyType))
            {
                API.Log($"[DoorTriggerLeft] Player does not have required key type '{inst._requiredKeyType}' - door will not move.");
                inst.ShowNoKeyMessage();
                return;
            }

            if (inst._consumeKey && !PlayerInventory.ConsumeKey(inst._requiredKeyType))
            {
                API.Log($"[DoorTriggerLeft] Failed to consume key type '{inst._requiredKeyType}'.");
                return;
            }

            // Start opening (sliding left)
            inst._opening = true;
            inst._closing = false;

            var pos = API.GetPosition(inst._doorEntity);
            API.PlaySound("sfx_door_slide_open_2d", inst._doorSoundPath, false);
            API.SetSoundVolume("sfx_door_slide_open_2d", 1.0f);
            API.Log("[DoorTriggerLeft] Playing door open sound.");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            DoorTriggerLeft inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react to player exiting
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;
            if (!inst._autoCloseOnExit) return;

            // Queue close (optionally with delay)
            inst._opening = false;
            inst._closing = true;
            inst._closeTimer = inst._closeDelay;

            API.Log("[DoorTriggerLeft] Player left trigger - door will slide back.");
        }

        /// <summary>
        /// Show the "no key" message with typewriter effect
        /// </summary>
        private void ShowNoKeyMessage()
        {
            if (_noKeyTextEntity == 0 || !API.HasText(_noKeyTextEntity)) return;
            if (_messageState != MessageState.Hidden) return; // Already showing

            _messageState = MessageState.Typing;
            _currentCharIndex = 0;
            _typewriterTimer = 0f;
            _messageAlpha = 1f;
            API.SetText(_noKeyTextEntity, "");
            API.SetTextColor(_noKeyTextEntity, new Vec4(1, 1, 1, 1)); // White, full alpha
            API.Log("[DoorTriggerLeft] Starting 'no key' message");
        }

        /// <summary>
        /// Update the typewriter effect and fade out
        /// </summary>
        private void UpdateNoKeyMessage(float dt)
        {
            if (_noKeyTextEntity == 0 || !API.HasText(_noKeyTextEntity)) return;
            if (_messageState == MessageState.Hidden) return;

            switch (_messageState)
            {
                case MessageState.Typing:
                    _typewriterTimer += dt;
                    float charsToShow = _typewriterTimer * CHARS_PER_SECOND;
                    int targetIndex = (int)charsToShow;

                    if (targetIndex > _currentCharIndex)
                    {
                        _currentCharIndex = Math.Min(targetIndex, NO_KEY_MESSAGE.Length);
                        string displayText = NO_KEY_MESSAGE.Substring(0, _currentCharIndex);
                        API.SetText(_noKeyTextEntity, displayText);

                        // Check if we've typed the full message
                        if (_currentCharIndex >= NO_KEY_MESSAGE.Length)
                        {
                            _messageState = MessageState.Displaying;
                            _displayTimer = DISPLAY_DURATION;
                            API.Log("[DoorTriggerLeft] Finished typing message, displaying");
                        }
                    }
                    break;

                case MessageState.Displaying:
                    _displayTimer -= dt;
                    if (_displayTimer <= 0f)
                    {
                        _messageState = MessageState.FadingOut;
                        API.Log("[DoorTriggerLeft] Starting fade out");
                    }
                    break;

                case MessageState.FadingOut:
                    _messageAlpha -= FADE_OUT_SPEED * dt;
                    if (_messageAlpha <= 0f)
                    {
                        _messageAlpha = 0f;
                        API.SetTextColor(_noKeyTextEntity, new Vec4(1, 1, 1, 0));
                        API.SetText(_noKeyTextEntity, "");
                        _messageState = MessageState.Hidden;
                        API.Log("[DoorTriggerLeft] Message hidden");
                    }
                    else
                    {
                        API.SetTextColor(_noKeyTextEntity, new Vec4(1, 1, 1, _messageAlpha));
                    }
                    break;
            }
        }

        private void ParseParams(string p)
        {
            if (string.IsNullOrWhiteSpace(p)) return;

            var parts = p.Split(new[] { ';', ',' }, StringSplitOptions.RemoveEmptyEntries);
            foreach (var raw in parts)
            {
                var kv = raw.Split(new[] { '=' }, 2);
                if (kv.Length != 2) continue;

                var key = kv[0].Trim().ToLowerInvariant();
                var val = kv[1].Trim();

                float f;
                bool b;

                switch (key)
                {
                    case "keytype":
                    case "requiredkey":
                    case "requiredkeytype":
                        _requiredKeyType = val;
                        break;

                    case "distance":
                    case "slide":
                    case "slidedistance":
                        if (float.TryParse(val, out f)) _slideDistance = Math.Max(0f, f);
                        break;

                    case "movespeed":
                    case "speed":
                        if (float.TryParse(val, out f)) _moveSpeed = Math.Max(0.01f, f);
                        break;

                    case "consumekey":
                        if (bool.TryParse(val, out b)) _consumeKey = b;
                        break;

                    case "autoclose":
                    case "autocloseonexit":
                        if (bool.TryParse(val, out b)) _autoCloseOnExit = b;
                        break;

                    case "closedelay":
                        if (float.TryParse(val, out f)) _closeDelay = Math.Max(0f, f);
                        break;
                }
            }
        }

        // Position helpers
        private static Vec3 MoveTowards(Vec3 current, Vec3 target, float maxDelta)
        {
            float dx = target.X - current.X;
            float dy = target.Y - current.Y;
            float dz = target.Z - current.Z;

            float dist = (float)Math.Sqrt(dx * dx + dy * dy + dz * dz);
            if (dist <= maxDelta || dist < 1e-6f) return target;

            float inv = maxDelta / dist;
            return new Vec3(current.X + dx * inv, current.Y + dy * inv, current.Z + dz * inv);
        }

        private static bool NearlyEqual(Vec3 a, Vec3 b, float eps = 0.001f)
        {
            return Math.Abs(a.X - b.X) <= eps &&
                   Math.Abs(a.Y - b.Y) <= eps &&
                   Math.Abs(a.Z - b.Z) <= eps;
        }
    }
}
