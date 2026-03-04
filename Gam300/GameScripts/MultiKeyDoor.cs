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
    // - When the player with ALL required keys enters the trigger, both the trigger and door slide left
    //   by 'distance' meters at 'movespeed' m/s. Optionally consumes the keys.
    // - If autoclose=true, when the player exits they slide back after 'closedelay' seconds.
    public class MultiKeyDoor
    {
        public ulong Entity;

        // Config
        [Boom.EditorExposed("Door Name", "Name of the door entity (leave empty to use first child)")]
        private string _doorName = "";

        [Boom.EditorExposed("Required Keys", "Comma-separated list of keys required to open this door (e.g., 'key1,key2')")]
        private string _requiredKeysString = "key1,key2";

        [Boom.EditorExposed("Slide Distance", "Distance the door slides in meters", 0.1f, 20f, true)]
        private float _slideDistance = 5.5f;   // meters

        [Boom.EditorExposed("Move Speed", "Speed of door movement in m/s", 0.1f, 10f, true)]
        private float _moveSpeed = 2.0f;       // m/s

        [Boom.EditorExposed("Consume Keys", "Whether opening the door uses up all the required keys")]
        private bool _consumeKey = true;

        [Boom.EditorExposed("Auto Close On Exit", "Whether the door closes when player leaves")]
        private bool _autoCloseOnExit = false;

        [Boom.EditorExposed("Close Delay", "Delay before auto-closing in seconds", 0f, 5f, true)]
        private float _closeDelay = 0f;

        [Boom.EditorExposed("Slide Direction", "Direction the door slides when opened",
            options: new[] { "left", "right", "front", "back" })]
        private string _slideDirection = "left";

        // Audio
        [Boom.EditorExposed("Door Sound", "Sound played when door opens/closes")]
        private string _doorSoundPath = "Resources/Audio/unlock.wav";

        private List<string> _requiredKeysList = new List<string>();

        // Cached positions and direction
        private Vec3 _basePos;
        private Vec3 _targetPos;
        private Vec3 _slideDir; // unit vector

        // Door entity (found as child)
        private ulong _doorEntity = 0;
        private Vec3 _doorOffset; // Offset from trigger to door

        // "No Key" message
        private ulong _noKeyTextEntity = 0;
        private string _noKeyMessage = "This door is locked, need to find a key.";
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
        private static readonly Dictionary<ulong, MultiKeyDoor> s_instances = new Dictionary<ulong, MultiKeyDoor>();
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

            // Parse required keys
            string[] keys = _requiredKeysString.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            foreach (var k in keys)
            {
                _requiredKeysList.Add(k.Trim());
            }

            // Ensure trigger is configured
            if (!API.HasCollider(Entity))
            {
                API.Log("[MultiKeyDoor] WARNING: Trigger entity has no collider!");
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
                    API.Log($"[MultiKeyDoor] ERROR: Could not find door entity '{_doorName}'");
                    return;
                }
            }
            else
            {
                // Try to find first child
                API.Log("[MultiKeyDoor] WARNING: No door name specified. Please set the door entity name.");
                return;
            }

            // Cache base position and Euler rotations
            _basePos = API.GetPosition(Entity);
            Vec3 rot = API.GetRotation(Entity);
            float yawDeg = rot.Y;
            float rollDeg = rot.Z; // Grab the Z rotation

            float yawRad = yawDeg * (float)Math.PI / 180f;

            // Base forward and right vectors based purely on Yaw
            float fx = (float)Math.Sin(yawRad);
            float fz = (float)Math.Cos(yawRad);

            // If the door is rolled ~180 degrees, it's visually flipped, so we invert the left/right axis
            bool isFlipped = Math.Abs(rollDeg) > 90f && Math.Abs(rollDeg) < 270f;
            float flipMult = isFlipped ? -1f : 1f;

            Vec3 moveDir = new Vec3(0f, 0f, 0f);
            string dirLower = _slideDirection.Trim().ToLowerInvariant();

            if (dirLower == "right")
            {
                moveDir = new Vec3(fz * flipMult, 0f, -fx * flipMult);
            }
            else if (dirLower == "front")
            {
                // Note: If your forward/back are also feeling inverted, just add * flipMult to fx and fz here too
                moveDir = new Vec3(fx, 0f, fz);
            }
            else if (dirLower == "back")
            {
                moveDir = new Vec3(-fx, 0f, -fz);
            }
            else // Default to Left
            {
                moveDir = new Vec3(-fz * flipMult, 0f, fx * flipMult);
            }
            _slideDir = moveDir;

            // Target position = base + moveDir * distance
            _targetPos = new Vec3(
                _basePos.X + _slideDir.X * _slideDistance,
                _basePos.Y,
                _basePos.Z + _slideDir.Z * _slideDistance
            );

            // Calculate and store offset from trigger to door
            Vec3 doorPos = API.GetPosition(_doorEntity);
            _doorOffset = new Vec3(
                doorPos.X - _basePos.X,
                doorPos.Y - _basePos.Y,
                doorPos.Z - _basePos.Z
            );

            API.Log($"[MultiKeyDoor] Trigger basePos=({_basePos.X:F2},{_basePos.Y:F2},{_basePos.Z:F2}) targetPos=({_targetPos.X:F2},{_targetPos.Y:F2},{_targetPos.Z:F2})");
            API.Log($"[MultiKeyDoor] Door offset=({_doorOffset.X:F2},{_doorOffset.Y:F2},{_doorOffset.Z:F2})");

            // Find and initialize the "no key" text entity
            _noKeyTextEntity = API.FindEntity("UI_NoKeyText");
            if (_noKeyTextEntity != 0 && API.HasText(_noKeyTextEntity))
            {
                API.SetText(_noKeyTextEntity, "");
                API.SetTextColor(_noKeyTextEntity, new Vec4(1, 1, 1, 0)); // White text, 0 alpha
                API.Log("[MultiKeyDoor] Initialized UI_NoKeyText");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log("[MultiKeyDoor] Registered trigger callbacks.");
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
                    API.Log("[MultiKeyDoor] Door moved left (opened).");
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
                    API.Log("[MultiKeyDoor] Door returned (closed).");
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
                    API.Log("[MultiKeyDoor] Shift+K: played 3D positional door SFX.");
                }
                else
                {
                    // 2D guaranteed-audible fallback (no attenuation)
                    API.PlaySound("sfx_door_slide_open_2d", _doorSoundPath, false);
                    API.SetSoundVolume("sfx_door_slide_open_2d", 1.0f);
                    API.Log("[MultiKeyDoor] K: played 2D door SFX (always audible).");
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
            MultiKeyDoor inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only player triggers this
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // If already open or opening, do nothing further
            if (inst._opening || NearlyEqual(API.GetPosition(inst.Entity), inst._targetPos)) return;

            // Collect missing keys
            List<string> missingKeys = new List<string>();
            foreach (var key in inst._requiredKeysList)
            {
                if (!PlayerInventory.HasKey(key))
                {
                    missingKeys.Add(key);
                }
            }

            if (missingKeys.Count > 0)
            {
                // Show dynamic missing keys message
                if (missingKeys.Count > 1) {
                    inst.ShowNoKeyMessage($"This door is locked, missing {missingKeys.Count} keys.");
                }
                else {
                    inst.ShowNoKeyMessage($"This door is locked, missing {missingKeys.Count} key.");
                }

                return;
            }

            // All keys are present - consume them
            if (inst._consumeKey)
            {
                foreach (var key in inst._requiredKeysList)
                {
                    if (!PlayerInventory.ConsumeKey(key))
                    {
                        API.Log($"[MultiKeyDoor] Failed to consume key type '{key}'.");
                    }
                }
            }

            // Start opening (sliding left)
            inst._opening = true;
            inst._closing = false;

            var pos = API.GetPosition(inst._doorEntity);
            API.PlaySound("sfx_door_slide_open_2d", inst._doorSoundPath, false);
            API.SetSoundVolume("sfx_door_slide_open_2d", 1.0f);
            API.Log("[MultiKeyDoor] Playing door open sound.");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            MultiKeyDoor inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react to player exiting
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;
            if (!inst._autoCloseOnExit) return;

            // Queue close (optionally with delay)
            inst._opening = false;
            inst._closing = true;
            inst._closeTimer = inst._closeDelay;

            API.Log("[MultiKeyDoor] Player left trigger - door will slide back.");
        }

        /// <summary>
        /// Show the "no key" message with typewriter effect
        /// </summary>
        private void ShowNoKeyMessage(string customMessage = null)
        {
            if (_noKeyTextEntity == 0 || !API.HasText(_noKeyTextEntity)) return;

            if (customMessage != null)
            {
                _noKeyMessage = customMessage;
            }

            // Restart typing effect even if currently displayed to update text dynamically
            _messageState = MessageState.Typing;
            _currentCharIndex = 0;
            _typewriterTimer = 0f;
            _messageAlpha = 1f;
            API.SetText(_noKeyTextEntity, "");
            API.SetTextColor(_noKeyTextEntity, new Vec4(1, 1, 1, 1)); // White, full alpha
            API.Log("[MultiKeyDoor] Starting 'no key' message: " + _noKeyMessage);
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
                        _currentCharIndex = Math.Min(targetIndex, _noKeyMessage.Length);
                        string displayText = _noKeyMessage.Substring(0, _currentCharIndex);
                        API.SetText(_noKeyTextEntity, displayText);

                        // Check if we've typed the full message
                        if (_currentCharIndex >= _noKeyMessage.Length)
                        {
                            _messageState = MessageState.Displaying;
                            _displayTimer = DISPLAY_DURATION;
                            API.Log("[MultiKeyDoor] Finished typing message, displaying");
                        }
                    }
                    break;

                case MessageState.Displaying:
                    _displayTimer -= dt;
                    if (_displayTimer <= 0f)
                    {
                        _messageState = MessageState.FadingOut;
                        API.Log("[MultiKeyDoor] Starting fade out");
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
                        API.Log("[MultiKeyDoor] Message hidden");
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
                    case "requiredkeys":
                    case "keys":
                        _requiredKeysString = val;
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
                    case "consumekeys":
                        if (bool.TryParse(val, out b)) _consumeKey = b;
                        break;

                    case "autoclose":
                    case "autocloseonexit":
                        if (bool.TryParse(val, out b)) _autoCloseOnExit = b;
                        break;

                    case "closedelay":
                        if (float.TryParse(val, out f)) _closeDelay = Math.Max(0f, f);
                        break;
                        
                    case "slidedirection":
                    case "slide direction":
                    case "direction":
                        _slideDirection = val;
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
