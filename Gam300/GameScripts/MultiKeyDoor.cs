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

        [Boom.EditorExposed("Is Main Door", "If true, shows dialogue on failure. If false, just plays locked sound.")]
        private bool _isMainDoor = false;

        // UI Entity Names
        [Boom.EditorExposed("Interaction Prompt Name", "Name of the UI entity for interaction (e.g. 'A to interact')")]
        private string _ePromptName = "UI_E_OpenDoor";

        [Boom.EditorExposed("Keys Needed UI Name", "Name of the UI entity showing missing keys photo")]
        private string _keysNeededName = "UI_KeysNeeded";

        [Boom.EditorExposed("Dialogue UI Name", "Name of the UI entity showing dialogue text image")]
        private string _dialogueName = "UI_NotEnoughKeysDialogue";

        // Audio
        [Boom.EditorExposed("Door Sound", "Sound played when door opens/closes")]
        private string _doorSoundPath = "Resources/Audio/unlock.wav";

        [Boom.EditorExposed("Door Locked Sound", "Sound played when door is locked")]
        private string _doorLockedSoundPath = "Resources/Audio/doorLocked.wav";

        [Boom.EditorExposed("Lock Entity Name", "Name of the lock entity to hide when the door opens (leave empty for none)")]
        private string _lockEntityName = "";

        private List<string> _requiredKeysList = new List<string>();

        // Lock entity
        private ulong _lockEntity = 0;

        // Cached positions and direction
        private Vec3 _basePos;
        private Vec3 _targetPos;
        private Vec3 _slideDir; // unit vector

        // Door entity (found as child)
        private ulong _doorEntity = 0;
        private Vec3 _doorOffset; // Offset from trigger to door

        // Interaction State
        private bool _playerInRange = false;
        private ulong _ePromptEntity = 0;
        private ulong _keysNeededEntity = 0;
        private ulong _dialogueEntity = 0;

        // E Prompt Fade State (used for interaction prompt)
        private enum EPromptFadeState { None, FadeIn, FadeOut }
        private EPromptFadeState _eFadeState = EPromptFadeState.None;
        private float _eFadeTimer = 0f;
        private const float E_FADE_DURATION = 0.15f;
        private float _eCurrentAlpha = 0f;

        private enum DialogueState { None, Dialogue1, Dialogue2 }
        private DialogueState _dialogueState = DialogueState.None;

        private bool _interactWasDown = false;
        private bool _enterWasDown = false;
        private const int KEY_E = 69; 
        private const int KEY_SPACE = 32;

        // Static tracking for active dialogue (drives pause from Entry.cs)
        private static MultiKeyDoor s_activeDialogueDoor = null;
        private static bool s_dialogueEnterWasDown = false;

        // HUD/inventory save state for dialogue
        private bool _wasInventoryOpen = false;

        // ── Dialogue fade ────────────────────────────────────────────────────
        private enum FadeMode { None, FadeIn, FadeOut }
        private FadeMode  _fadeMode      = FadeMode.None;
        private float     _fadeTimer     = 0f;
        private const float FADE_DURATION = 0.25f;
        // What to do when the current fade finishes
        private enum PendingAction { None, AdvanceToDialogue2, CloseDialogue }
        private PendingAction _pendingAction = PendingAction.None;

        // State
        private static readonly Dictionary<ulong, MultiKeyDoor> s_instances = new Dictionary<ulong, MultiKeyDoor>();
        private bool _opening = false;
        private bool _closing = false;
        private float _closeTimer = 0f;

        private bool _kWasDown  = false;
        private bool _f5WasDown = false;

        // ── Seal animation (MainDoor only) ───────────────────────────────────
        [Boom.EditorExposed("Seal 1 Names", "Comma-separated entity names for seal group 1 (lifts first, e.g. SEAL1_1,SEAL1_2)")]
        private string _seal1Name = "";

        [Boom.EditorExposed("Seal 2 Names", "Comma-separated entity names for seal group 2 (lifts second, e.g. SEAL2_1,SEAL2_2)")]
        private string _seal2Name = "";

        [Boom.EditorExposed("Seal 3 Names", "Comma-separated entity names for seal group 3 (lifts third, e.g. SEAL3_1,SEAL3_2)")]
        private string _seal3Name = "";

        [Boom.EditorExposed("Seal 2 Start Delay", "Seconds after seal 1 starts that seal 2 begins lifting", 0f, 3f, true)]
        private float _seal2Delay = 0.15f;

        [Boom.EditorExposed("Seal 3 Start Delay", "Seconds after seal 1 starts that seal 3 begins lifting", 0f, 3f, true)]
        private float _seal3Delay = 0.30f;

        [Boom.EditorExposed("Seal Lift Duration", "Seconds each seal takes to fully lift and fade", 0.1f, 5f, true)]
        private float _sealLiftDuration = 1.0f;

        [Boom.EditorExposed("Seal Lift Height", "Units each seal rises before disappearing", 0f, 20f, true)]
        private float _sealLiftHeight = 2.5f;

        // Runtime seal state — each group holds multiple entities
        private List<ulong> _sealGroupEntities0 = new List<ulong>();
        private List<ulong> _sealGroupEntities1 = new List<ulong>();
        private List<ulong> _sealGroupEntities2 = new List<ulong>();
        private List<Vec3>  _sealGroupBasePos0  = new List<Vec3>();
        private List<Vec3>  _sealGroupBasePos1  = new List<Vec3>();
        private List<Vec3>  _sealGroupBasePos2  = new List<Vec3>();
        private bool        _sealGroup0Done     = false;
        private bool        _sealGroup1Done     = false;
        private bool        _sealGroup2Done     = false;
        private bool        _sealAnimActive     = false;
        private float       _sealMasterTimer    = 0f;

        // Constants (GLFW)
        private const int KEY_K  = 75;          // GLFW_KEY_K
        private const int KEY_F5 = 294;         // GLFW_KEY_F5 — cheat: force-open door
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

            // Find and initialize UI entities
            _ePromptEntity = API.FindEntity(_ePromptName);
            if (_ePromptEntity != 0 && API.HasSprite(_ePromptEntity))
            {
                API.SetSpriteAlpha(_ePromptEntity, 0f);
            }

            _keysNeededEntity = API.FindEntity(_keysNeededName);
            if (_keysNeededEntity != 0 && API.HasSprite(_keysNeededEntity))
            {
                API.SetSpriteAlpha(_keysNeededEntity, 0f);
            }

            _dialogueEntity = API.FindEntity(_dialogueName);
            if (_dialogueEntity != 0 && API.HasSprite(_dialogueEntity))
            {
                API.SetSpriteAlpha(_dialogueEntity, 0f);
            }

            _eCurrentAlpha = 0f;
            _eFadeState = EPromptFadeState.None;

            // Find lock entity
            if (!string.IsNullOrWhiteSpace(_lockEntityName))
            {
                _lockEntity = API.FindEntity(_lockEntityName);
                if (_lockEntity == 0)
                    API.Log($"[MultiKeyDoor] WARNING: Could not find lock entity '{_lockEntityName}'.");
                else
                    API.Log($"[MultiKeyDoor] Lock entity '{_lockEntityName}' found (id={_lockEntity}).");
            }

            // Find seal entities (MainDoor only)
            if (_isMainDoor)
                FindSealEntities();

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log("[MultiKeyDoor] Registered trigger callbacks.");
        }

        public void OnUpdate(float dt)
        {
            if (_doorEntity == 0) return;

            if (_sealAnimActive) UpdateSealAnimation(dt);

            // Handle E Prompt Fading
            if (_ePromptEntity != 0 && _eFadeState != EPromptFadeState.None)
            {
                _eFadeTimer += dt;
                float t = Math.Min(1f, _eFadeTimer / E_FADE_DURATION);

                if (_eFadeState == EPromptFadeState.FadeIn)
                {
                    _eCurrentAlpha = t;
                    API.SetSpriteAlpha(_ePromptEntity, _eCurrentAlpha);
                    if (t >= 1f)
                    {
                        _eFadeState = EPromptFadeState.None;
                        _eFadeTimer = 0f;
                    }
                }
                else if (_eFadeState == EPromptFadeState.FadeOut)
                {
                    _eCurrentAlpha = 1f - t;
                    API.SetSpriteAlpha(_ePromptEntity, _eCurrentAlpha);
                    if (t >= 1f)
                    {
                        _eFadeState = EPromptFadeState.None;
                        _eFadeTimer = 0f;
                    }
                }
            }

            // Handle Interaction logic
            bool interactDown = API.IsKeyDown(KEY_E) || (API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A));
            bool enterDown = API.IsKeyDown(KEY_SPACE);

            bool interactPressed = interactDown && !_interactWasDown;
            bool enterPressed = enterDown && !_enterWasDown;

            _interactWasDown = interactDown;
            _enterWasDown = enterDown;

            if (_playerInRange && !_opening && !NearlyEqual(API.GetPosition(Entity), _targetPos))
            {
                if (_dialogueState == DialogueState.None)
                {
                    if (interactPressed)
                    {
                        // Check keys
                        List<string> missingKeys = new List<string>();
                        foreach (var key in _requiredKeysList)
                        {
                            if (!PlayerInventory.HasKey(key))
                            {
                                missingKeys.Add(key);
                            }
                        }

                        if (missingKeys.Count > 0)
                        {
                            if (_isMainDoor)
                            {
                                // Start dialogue and pause
                                _dialogueState = DialogueState.Dialogue1;
                                s_activeDialogueDoor = this;
                                s_dialogueEnterWasDown = true; // prevent immediate advance
                                API.SetGameLogicPaused(true);
                                API.Log("[MultiKeyDoor] Dialogue started - game paused.");

                                // Hide inventory and HUD for the duration of the dialogue
                                _wasInventoryOpen = Entry.IsInventoryOpen;
                                Entry.IsInventoryOpen = false;
                                UIManager.HideHUD();

                                // Hide E prompt immediately (game logic is paused during dialogue
                                // so the fade animation won't run — snap to hidden instead)
                                if (_ePromptEntity != 0)
                                {
                                    _eFadeState    = EPromptFadeState.None;
                                    _eCurrentAlpha = 0f;
                                    API.SetSpriteAlpha(_ePromptEntity, 0f);
                                }

                                // Prepare and fade-in KeysNeeded + Dialogue 1
                                if (_keysNeededEntity != 0)
                                {
                                    int count = Math.Min(3, missingKeys.Count);
                                    API.SetSpriteTexture(_keysNeededEntity, $"Resources/Textures/PlayerUI/KeysNeeded_{count}.png");
                                    API.SetSpriteAlpha(_keysNeededEntity, 0f);
                                }
                                if (_dialogueEntity != 0)
                                {
                                    API.SetSpriteTexture(_dialogueEntity, "Resources/Textures/PlayerUI/NotEnoughKeys_Dialogue1.png");
                                    API.SetSpriteAlpha(_dialogueEntity, 0f);
                                }
                                // Kick off fade-in
                                _fadeMode  = FadeMode.FadeIn;
                                _fadeTimer = 0f;
                                _pendingAction = PendingAction.None;

                                // Play locked sound immediately as dialogue appears
                                API.PlaySound("sfx_door_locked_2d", _doorLockedSoundPath, false);
                                API.SetSoundVolume("sfx_door_locked_2d", 0.2f);
                            }
                            else
                            {
                                // Play locked sound
                                API.PlaySound("sfx_door_locked_2d", _doorLockedSoundPath, false);
                                API.SetSoundVolume("sfx_door_locked_2d", 0.2f);
                            }
                        }
                        else
                        {
                            // Open door
                            if (_consumeKey)
                            {
                                foreach (var key in _requiredKeysList)
                                {
                                    PlayerInventory.ConsumeKeyType(key);
                                }
                            }

                            _opening = true;
                            _closing = false;

                            // Start seal lift animation
                            if (_isMainDoor) StartSealAnimation();

                            // Vanish the lock entity
                            if (_lockEntity != 0 && API.HasTransform(_lockEntity))
                            {
                                Vec3 lp = API.GetPosition(_lockEntity);
                                API.SetPosition(_lockEntity, new Vec3(lp.X, -100f, lp.Z));
                                API.Log($"[MultiKeyDoor] Lock entity '{_lockEntityName}' vanished.");
                            }

                            if (_ePromptEntity != 0)
                            {
                                _eFadeState = EPromptFadeState.FadeOut;
                                _eFadeTimer = (1f - _eCurrentAlpha) * E_FADE_DURATION;
                            }

                            API.PlaySound("sfx_door_slide_open_2d", _doorSoundPath, false);
                            API.SetSoundVolume("sfx_door_slide_open_2d", 0.2f);
                            API.Log("[MultiKeyDoor] Door opening.");
                        }
                    }
                }
                else
                {
                    // Dialogue input is fully handled by UpdateDialogue() called from Entry.Update()
                    // (which runs even while game logic is paused).  Nothing to do here.
                }
            }

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
                    API.SetSoundVolume("sfx_door_slide_open_3d", 0.2f);
                    API.Set3DMinMaxDistance("sfx_door_slide_open_3d", 1.5f, 35.0f);
                    API.Log("[MultiKeyDoor] Shift+K: played 3D positional door SFX.");
                }
                else
                {
                    // 2D guaranteed-audible fallback (no attenuation)
                    API.PlaySound("sfx_door_slide_open_2d", _doorSoundPath, false);
                    API.SetSoundVolume("sfx_door_slide_open_2d", 0.2f);
                    API.Log("[MultiKeyDoor] K: played 2D door SFX (always audible).");
                }
            }
            _kWasDown = kDown;

            // F5 cheat — force-open this door instantly, no keys required
            bool f5Down = API.IsKeyDown(KEY_F5);
            if (f5Down && !_f5WasDown && _playerInRange && !_opening && !NearlyEqual(API.GetPosition(Entity), _targetPos))
            {
                API.Log($"[MultiKeyDoor] F5 CHEAT: Force-opening door '{_doorName}'.");

                if (_lockEntity != 0 && API.HasTransform(_lockEntity))
                {
                    Vec3 lp = API.GetPosition(_lockEntity);
                    API.SetPosition(_lockEntity, new Vec3(lp.X, -100f, lp.Z));
                }

                if (_ePromptEntity != 0)
                {
                    _eFadeState = EPromptFadeState.FadeOut;
                    _eFadeTimer = (1f - _eCurrentAlpha) * E_FADE_DURATION;
                }

                if (_isMainDoor) StartSealAnimation();

                _opening = true;
                _closing = false;
                API.PlaySound("sfx_door_slide_open_2d", _doorSoundPath, false);
                API.SetSoundVolume("sfx_door_slide_open_2d", 0.2f);
            }
            _f5WasDown = f5Down;
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

            inst._playerInRange = true;
            if (inst._ePromptEntity != 0 && inst._dialogueState == DialogueState.None)
            {
                inst._eFadeState = EPromptFadeState.FadeIn;
                inst._eFadeTimer = inst._eCurrentAlpha * E_FADE_DURATION;
            }
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            MultiKeyDoor inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react to player exiting
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInRange = false;

            // Immediately hide UI elements (player walked away - no need for graceful fade)
            inst._fadeMode  = FadeMode.None;
            inst._fadeTimer = 0f;
            inst._pendingAction = PendingAction.None;
            if (inst._ePromptEntity != 0) 
            {
                inst._eFadeState = EPromptFadeState.FadeOut;
                inst._eFadeTimer = (1f - inst._eCurrentAlpha) * E_FADE_DURATION;
            }
            if (inst._keysNeededEntity != 0) API.SetSpriteAlpha(inst._keysNeededEntity, 0f);
            if (inst._dialogueEntity != 0) API.SetSpriteAlpha(inst._dialogueEntity, 0f);

            inst._dialogueState = DialogueState.None;
            if (s_activeDialogueDoor == inst) s_activeDialogueDoor = null;

            if (!inst._autoCloseOnExit) return;

            // Queue close (optionally with delay)
            inst._opening = false;
            inst._closing = true;
            inst._closeTimer = inst._closeDelay;

            API.Log("[MultiKeyDoor] Player left trigger - door will slide back.");
        }

        /// <summary>
        /// Returns true while a main door dialogue is showing (including fade transitions).
        /// Called from Entry.Update() to keep the game paused.
        /// </summary>
        public static bool IsDialogueActive()
        {
            return s_activeDialogueDoor != null &&
                   (s_activeDialogueDoor._dialogueState != DialogueState.None ||
                    s_activeDialogueDoor._fadeMode != FadeMode.None);
        }

        // ── Fade helpers ──────────────────────────────────────────────────────

        private void ApplyDialoguePanelAlpha(float alpha)
        {
            if (_dialogueEntity != 0 && API.HasSprite(_dialogueEntity))
                API.SetSpriteAlpha(_dialogueEntity, alpha);

            if (_keysNeededEntity != 0 && API.HasSprite(_keysNeededEntity))
            {
                if (_pendingAction == PendingAction.AdvanceToDialogue2 ||
                    (_dialogueState == DialogueState.Dialogue2 && _fadeMode == FadeMode.FadeIn))
                {
                    API.SetSpriteAlpha(_keysNeededEntity, 1f);
                }
                else
                {
                    API.SetSpriteAlpha(_keysNeededEntity, alpha);
                }
            }
        }

        /// <summary>Run the queued action after a fade-out finishes.</summary>
        private void ExecutePendingAction()
        {
            switch (_pendingAction)
            {
                case PendingAction.AdvanceToDialogue2:
                    _pendingAction = PendingAction.None;
                    _dialogueState = DialogueState.Dialogue2;
                    if (_dialogueEntity != 0)
                        API.SetSpriteTexture(_dialogueEntity, "Resources/Textures/PlayerUI/NotEnoughKeys_Dialogue2.png");
                    
                    // Reset text panel alpha to 0, then fade in (leave KeysNeeded at 1)
                    if (_dialogueEntity != 0) API.SetSpriteAlpha(_dialogueEntity, 0f);
                    
                    _fadeMode  = FadeMode.FadeIn;
                    _fadeTimer = 0f;
                    break;

                case PendingAction.CloseDialogue:
                    _pendingAction = PendingAction.None;

                    // Force fully hidden
                    if (_dialogueEntity != 0) API.SetSpriteAlpha(_dialogueEntity, 0f);
                    if (_keysNeededEntity != 0) API.SetSpriteAlpha(_keysNeededEntity, 0f);

                    _dialogueState = DialogueState.None;
                    s_activeDialogueDoor = null;
                    if (_ePromptEntity != 0)
                    {
                        _eFadeState = EPromptFadeState.FadeIn;
                        _eFadeTimer = _eCurrentAlpha * E_FADE_DURATION;
                    }

                    // Restore inventory and HUD
                    Entry.IsInventoryOpen = _wasInventoryOpen;
                    UIManager.ShowHUD();

                    API.SetGameLogicPaused(false);
                    API.Log("[MultiKeyDoor] Dialogue ended - game resumed.");
                    break;
            }
        }

        /// <summary>
        /// Input + fade handler for the dialogue - must be called from Entry.Update() so it
        /// runs even while game logic is paused.
        /// </summary>
        public static void UpdateDialogue(float dt)
        {
            if (s_activeDialogueDoor == null) return;

            MultiKeyDoor door = s_activeDialogueDoor;

            // ── Tick fade ──────────────────────────────────────────────────────
            if (door._fadeMode != FadeMode.None)
            {
                door._fadeTimer += dt;
                float t = Math.Min(1f, door._fadeTimer / FADE_DURATION);

                if (door._fadeMode == FadeMode.FadeIn)
                {
                    door.ApplyDialoguePanelAlpha(t);
                    if (t >= 1f)
                    {
                        door._fadeMode  = FadeMode.None;
                        door._fadeTimer = 0f;
                    }
                }
                else if (door._fadeMode == FadeMode.FadeOut)
                {
                    door.ApplyDialoguePanelAlpha(1f - t);
                    if (t >= 1f)
                    {
                        door._fadeMode  = FadeMode.None;
                        door._fadeTimer = 0f;
                        door.ExecutePendingAction();
                    }
                }
            }

            // ── Input ─────────────────────────────────────────────────────────
            bool gamepadA = API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A);
            bool advanceDown = API.IsKeyDown(KEY_SPACE) || API.IsKeyDown(KEY_E) || gamepadA;
            bool advancePressed = advanceDown && !s_dialogueEnterWasDown;
            s_dialogueEnterWasDown = advanceDown;

            if (!advancePressed) return;

            API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);

            // Skip remaining fade-in immediately
            if (door._fadeMode == FadeMode.FadeIn)
            {
                door._fadeMode  = FadeMode.None;
                door._fadeTimer = 0f;
                door.ApplyDialoguePanelAlpha(1f);
                return;
            }

            // Block input while fading out (action already queued)
            if (door._fadeMode == FadeMode.FadeOut) return;

            if (door._dialogueState == DialogueState.Dialogue1)
            {
                door._fadeMode  = FadeMode.FadeOut;
                door._fadeTimer = 0f;
                door._pendingAction = PendingAction.AdvanceToDialogue2;
            }
            else if (door._dialogueState == DialogueState.Dialogue2)
            {
                door._fadeMode  = FadeMode.FadeOut;
                door._fadeTimer = 0f;
                door._pendingAction = PendingAction.CloseDialogue;
            }
        }

        // ── Seal animation ────────────────────────────────────────────────────

        private void FindSealEntities()
        {
            string[] nameFields = { _seal1Name, _seal2Name, _seal3Name };
            var groups = new[] {
                (_sealGroupEntities0, _sealGroupBasePos0),
                (_sealGroupEntities1, _sealGroupBasePos1),
                (_sealGroupEntities2, _sealGroupBasePos2),
            };

            for (int g = 0; g < 3; g++)
            {
                var (entities, positions) = groups[g];
                entities.Clear();
                positions.Clear();

                if (string.IsNullOrWhiteSpace(nameFields[g])) continue;

                string[] names = nameFields[g].Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
                foreach (string raw in names)
                {
                    string name = raw.Trim();
                    ulong id = API.FindEntity(name);
                    if (id != 0 && API.HasTransform(id))
                    {
                        entities.Add(id);
                        positions.Add(API.GetPosition(id));
                        API.Log($"[MultiKeyDoor] Seal group {g + 1}: found '{name}' (id={id})");
                    }
                    else
                    {
                        API.Log($"[MultiKeyDoor] WARNING: Seal group {g + 1}: entity '{name}' not found.");
                    }
                }
            }
        }

        private void StartSealAnimation()
        {
            _sealGroup0Done  = false;
            _sealGroup1Done  = false;
            _sealGroup2Done  = false;
            _sealMasterTimer = 0f;

            int total = _sealGroupEntities0.Count + _sealGroupEntities1.Count + _sealGroupEntities2.Count;
            _sealAnimActive = total > 0;

            if (total == 0)
                API.Log("[MultiKeyDoor] WARNING: Seal animation triggered but no seal entities found. Check Seal 1/2/3 Names fields.");
            else
                API.Log($"[MultiKeyDoor] Seal animation started ({total} entities across 3 groups). Delays: 0s / {_seal2Delay}s / {_seal3Delay}s, lift={_sealLiftHeight}u over {_sealLiftDuration}s.");
        }

        private void UpdateSealAnimation(float dt)
        {
            _sealMasterTimer += dt;

            float[] startDelays = { 0f, _seal2Delay, _seal3Delay };
            var groups = new[] {
                (_sealGroupEntities0, _sealGroupBasePos0),
                (_sealGroupEntities1, _sealGroupBasePos1),
                (_sealGroupEntities2, _sealGroupBasePos2),
            };
            bool[] groupDone = { _sealGroup0Done, _sealGroup1Done, _sealGroup2Done };

            for (int g = 0; g < 3; g++)
            {
                if (groupDone[g]) continue;

                var (entities, positions) = groups[g];
                if (entities.Count == 0) { groupDone[g] = true; continue; }

                // Not yet time for this group to start
                if (_sealMasterTimer < startDelays[g]) continue;

                float localT = _sealMasterTimer - startDelays[g];
                float t01    = Math.Min(1f, localT / _sealLiftDuration);

                for (int j = 0; j < entities.Count; j++)
                {
                    API.SetPosition(entities[j], new Vec3(
                        positions[j].X,
                        positions[j].Y + _sealLiftHeight * t01,
                        positions[j].Z
                    ));
                    API.SetModelOpacity(entities[j], 1f - t01);
                }

                // Group complete — teleport all away and restore opacity
                if (t01 >= 1f)
                {
                    for (int j = 0; j < entities.Count; j++)
                    {
                        API.SetPosition(entities[j], new Vec3(positions[j].X, -100f, positions[j].Z));
                        API.SetModelOpacity(entities[j], 1f);
                    }
                    groupDone[g] = true;
                    API.Log($"[MultiKeyDoor] Seal group {g + 1} done ({entities.Count} entities).");
                }
            }

            _sealGroup0Done = groupDone[0];
            _sealGroup1Done = groupDone[1];
            _sealGroup2Done = groupDone[2];

            if (_sealGroup0Done && _sealGroup1Done && _sealGroup2Done)
            {
                _sealAnimActive = false;
                API.Log("[MultiKeyDoor] All seal groups finished.");
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

                    case "ismaindoor":
                        if (bool.TryParse(val, out b)) _isMainDoor = b;
                        break;
                        
                    case "epromptname":
                        _ePromptName = val;
                        break;

                    case "keysneededname":
                        _keysNeededName = val;
                        break;

                    case "dialoguename":
                        _dialogueName = val;
                        break;

                    case "closedelay":
                        if (float.TryParse(val, out f)) _closeDelay = Math.Max(0f, f);
                        break;
                        
                    case "slidedirection":
                    case "slide direction":
                    case "direction":
                        _slideDirection = val;
                        break;

                    case "seal1name": case "seal1":
                        _seal1Name = val; break;
                    case "seal2name": case "seal2":
                        _seal2Name = val; break;
                    case "seal3name": case "seal3":
                        _seal3Name = val; break;
                    case "seal2delay":
                        if (float.TryParse(val, out f)) _seal2Delay = Math.Max(0f, f); break;
                    case "seal3delay":
                        if (float.TryParse(val, out f)) _seal3Delay = Math.Max(0f, f); break;
                    case "sealliftduration": case "seallifetime":
                        if (float.TryParse(val, out f)) _sealLiftDuration = Math.Max(0.1f, f); break;
                    case "sealliftHeight": case "sealheight":
                        if (float.TryParse(val, out f)) _sealLiftHeight = Math.Max(0f, f); break;
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
