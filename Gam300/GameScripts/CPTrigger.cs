using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// CPTrigger updates the player's spawn/respawn location when they press Q inside this trigger zone.
    /// Place this script on a trigger collider entity in your scene.
    /// </summary>
    public class CPTrigger
    {
        public ulong Entity;

        // Optional: Offset from trigger position for spawn point (e.g., to spawn slightly above ground)
        [Boom.EditorExposed("Spawn Offset Y", "Vertical offset for spawn position")]
        private float _spawnOffsetY = 1.5f;

        // Forward offset distance
        [Boom.EditorExposed("Forward Offset", "Distance in front of checkpoint to spawn")]
        private float _forwardOffset = 5.0f;

        [Boom.EditorExposed("Light Names", "Comma-separated names of light entities to activate")]
        private string _lightNames = "";

        [Boom.EditorExposed("Active Intensity", "Intensity of lights when activated")]
        private float _activeIntensity = 2.5f;

        [Boom.EditorExposed("Active Color", "Color of lights when activated")]
        private Vec3 _activeColor = new Vec3(0.0f, 1.0f, 0.0f);

        [Boom.EditorExposed("Checkpoint Sprite Names", "Comma-separated names of sprite entities to show when activated")]
        private string _checkpointSpriteNames = "";

        [Boom.EditorExposed("Bobbing Sprite Names", "Comma-separated names of sprites that should perform the sin wave bobbing")]
        private string _bobbingSpriteNames = "";

        [Boom.EditorExposed("Floating Sprite Names", "Comma-separated names of sprites that should float upwards")]
        private string _floatingSpriteNames = "";

        [Boom.EditorExposed("Trigger Intro Cutscene", "Whether to play the intro cutscene and checkpoint dialogue when activated")]
        private bool _triggerIntroCutscene = false;

        [Boom.EditorExposed("Cutscene Entity Name", "Name of the entity with CutsceneSequencer to play (only used if Trigger Intro Cutscene is true)")]
        private string _cutsceneEntityName = "Intro CutScene";

        [Boom.EditorExposed("Sprite Display Duration", "How long to show the sprites in seconds")]
        private float _spriteDisplayDuration = 2.0f;

        [Boom.EditorExposed("Sprite Fade Speed", "Speed of the fade effect")]
        private float _spriteFadeSpeed = 4.0f;

        [Boom.EditorExposed("Float Speed", "Speed of the upward float movement")]
        private float _floatSpeed = 50.0f;

        [Boom.EditorExposed("Bob Speed", "Speed of the horizontal bobbing movement")]
        private float _bobSpeed = 2.0f;

        [Boom.EditorExposed("Bob Amount", "Amount of horizontal bobbing movement")]
        private float _bobAmount = 20.0f;

        // Track if checkpoint was already activated
        private bool _activated = false;
        
        private bool _playerInZone = false;
        private bool _wasQPressed = false;
        private bool _wasAPressed = false;

        private List<ulong> _lightIDs = new List<ulong>();
        private List<ulong> _spriteIDs = new List<ulong>();
        private HashSet<ulong> _bobbingSpriteIDs = new HashSet<ulong>();
        private HashSet<ulong> _floatingSpriteIDs = new HashSet<ulong>();
        private Dictionary<ulong, Vec3> _originalSpritePositions = new Dictionary<ulong, Vec3>();

        // Sprite state
        private float _spriteAlpha = 0.0f;
        private float _spriteTimer = 0.0f;
        private float _totalActiveTime = 0.0f;
        // Fired once when sprites fully fade out (used to chain cutscene after UI)
        private Action _spriteOnCompleteAction = null;

        // Static instance tracking
        private static readonly Dictionary<ulong, CPTrigger> s_instances = new Dictionary<ulong, CPTrigger>();

        public void OnStart(string jsonParams)
        {
            // Register this instance
            s_instances[Entity] = this;

            // Apply editor parameters
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);

            API.Log($"[CPTrigger] Starting on entity {Entity}. Light names: '{_lightNames}', Sprites: '{_checkpointSpriteNames}'");

            // Find and initialize lights as OFF
            if (!string.IsNullOrEmpty(_lightNames))
            {
                string[] names = _lightNames.Split(',');
                foreach (string name in names)
                {
                    string trimmedName = name.Trim();
                    ulong id = API.FindEntity(trimmedName);
                    if (id != 0)
                    {
                        if (API.HasSpotLight(id))
                        {
                            API.SetSpotLightIntensity(id, 0.0f);
                          }
                        else if (API.HasPointLight(id))
                        {
                            API.SetPointLightIntensity(id, 0.0f);
                        }
                    }
                }
            }

            // Initialize bobbing sprites lookup
            _bobbingSpriteIDs.Clear();
            if (!string.IsNullOrEmpty(_bobbingSpriteNames))
            {
                string[] names = _bobbingSpriteNames.Split(',');
                foreach (string name in names)
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0) _bobbingSpriteIDs.Add(id);
                }
            }

            // Initialize floating sprites lookup
            _floatingSpriteIDs.Clear();
            if (!string.IsNullOrEmpty(_floatingSpriteNames))
            {
                string[] names = _floatingSpriteNames.Split(',');
                foreach (string name in names)
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0) _floatingSpriteIDs.Add(id);
                }
            }

            // Initialize sprites if specified
            _spriteIDs.Clear();
            _originalSpritePositions.Clear();
            if (!string.IsNullOrEmpty(_checkpointSpriteNames))
            {
                string[] names = _checkpointSpriteNames.Split(',');
                foreach (string name in names)
                {
                    string trimmedName = name.Trim();
                    ulong id = API.FindEntity(trimmedName);
                    if (id != 0 && API.HasSprite(id))
                    {
                        _spriteIDs.Add(id);
                        API.SetSpriteAlpha(id, 0.0f);
                        
                        // Store original world position
                        if (API.HasTransform(id))
                        {
                            _originalSpritePositions[id] = API.GetPosition(id);
                        }
                    }
                    else if (id == 0)
                    {
                        API.Log($"[CPTrigger] WARNING: Could not find sprite entity '{trimmedName}'");
                    }
                }
            }

            // Initialize text as hidden if it exists on this entity
            if (API.HasText(Entity))
            {
                Vec4 color = API.GetTextColor(Entity);
                color.W = 0.0f;
                API.SetTextColor(Entity, color);
            }

            // Ensure trigger is configured
            if (!API.HasCollider(Entity))
            {
                API.Log("[CPTrigger] WARNING: Entity has no collider!");
                return;
            }

            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
            }

            // Register callbacks
            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnterCallback);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExitCallback);
            API.Log("[CPTrigger] Registered trigger callbacks. Checkpoint will activate on entry.");
        }

        public void OnUpdate(float dt)
        {
            // Handle sprites fade, movement and timer
            if (_spriteIDs.Count > 0)
            {
                float targetAlpha = (_spriteTimer > 0) ? 1.0f : 0.0f;
                
                // Continue updating as long as it's visible or supposed to be visible
                if (_spriteAlpha > 0.001f || targetAlpha > 0.001f)
                {
                    _spriteAlpha = Lerp(_spriteAlpha, targetAlpha, _spriteFadeSpeed * dt);
                    
                    // Always increment active time while visible so movement doesn't freeze during fade
                    _totalActiveTime += dt;

                    // Update movement for each sprite
                    foreach (ulong id in _spriteIDs)
                    {
                        if (API.HasSprite(id))
                        {
                            API.SetSpriteAlpha(id, _spriteAlpha);

                            // Apply world transform movement
                            if (_originalSpritePositions.ContainsKey(id))
                            {
                                Vec3 originalPos = _originalSpritePositions[id];
                                
                                // Float up (+Y in world space) ONLY if specified
                                float offsetY = 0f;
                                if (_floatingSpriteIDs.Contains(id))
                                {
                                    offsetY = (_floatSpeed / 100f) * _totalActiveTime;
                                }
                                
                                // Bob left/right (+X in world space) ONLY if specified
                                float offsetX = 0f;
                                if (_bobbingSpriteIDs.Contains(id))
                                {
                                    offsetX = (float)Math.Sin(_totalActiveTime * _bobSpeed) * (_bobAmount / 100f);
                                }

                                Vec3 newPos = new Vec3(originalPos.X + offsetX, originalPos.Y + offsetY, originalPos.Z);
                                API.SetPosition(id, newPos);
                            }
                        }
                    }
                }
                else
                {
                    // Fully hidden: reset timer, alpha, and POSITIONS
                    if (_totalActiveTime > 0.0f) // Only do this once when transitioning to hidden
                    {
                        foreach (ulong id in _spriteIDs)
                        {
                            if (API.HasSprite(id)) API.SetSpriteAlpha(id, 0.0f);
                            
                            if (_originalSpritePositions.ContainsKey(id))
                            {
                                API.SetPosition(id, _originalSpritePositions[id]);
                            }
                        }

                        // Fire the on-complete callback once sprites have fully hidden
                        Action cb = _spriteOnCompleteAction;
                        _spriteOnCompleteAction = null;
                        cb?.Invoke();
                    }
                    
                    _totalActiveTime = 0.0f;
                    _spriteAlpha = 0.0f;
                }

                if (_spriteTimer > 0)
                {
                    _spriteTimer -= dt;
                }
            }
        }

        private void ActivateCheckpoint()
        {
            // Refresh sprite IDs and original positions during activation to be robust
            _spriteIDs.Clear();
            _originalSpritePositions.Clear();
            _bobbingSpriteIDs.Clear();
            _floatingSpriteIDs.Clear();

            // Helper to collect all unique sprite IDs from all lists
            HashSet<ulong> allFoundSprites = new HashSet<ulong>();

            // 1. Collect main sprites
            if (!string.IsNullOrEmpty(_checkpointSpriteNames))
            {
                foreach (string name in _checkpointSpriteNames.Split(','))
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0) allFoundSprites.Add(id);
                }
            }

            // 2. Collect and identify bobbing sprites
            if (!string.IsNullOrEmpty(_bobbingSpriteNames))
            {
                foreach (string name in _bobbingSpriteNames.Split(','))
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0)
                    {
                        allFoundSprites.Add(id); // Auto-add to alpha list
                        _bobbingSpriteIDs.Add(id);
                    }
                }
            }

            // 3. Collect and identify floating sprites
            if (!string.IsNullOrEmpty(_floatingSpriteNames))
            {
                foreach (string name in _floatingSpriteNames.Split(','))
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0)
                    {
                        allFoundSprites.Add(id); // Auto-add to alpha list
                        _floatingSpriteIDs.Add(id);
                    }
                }
            }

            // Populate the active sprite list and store their starting positions
            foreach (ulong id in allFoundSprites)
            {
                if (API.HasSprite(id))
                {
                    _spriteIDs.Add(id);
                    if (API.HasTransform(id))
                    {
                        _originalSpritePositions[id] = API.GetPosition(id);
                    }
                }
            }

            API.Log($"[CPTrigger] Activation: Found {_spriteIDs.Count} sprites total ({_bobbingSpriteIDs.Count} bobbing, {_floatingSpriteIDs.Count} floating).");

            // Get the trigger's position and rotation
            Vec3 checkpointPos = API.GetPosition(Entity);
            Vec3 checkpointRot = API.GetRotation(Entity);

            // Calculate forward direction from Y rotation (yaw)
            float yawRad = checkpointRot.Y * (float)Math.PI / 180f;
            float forwardX = (float)Math.Sin(yawRad);
            float forwardZ = (float)Math.Cos(yawRad);

            // Apply forward offset (5 units in front)
            Vec3 spawnPos = new Vec3(
                checkpointPos.X - forwardX * _forwardOffset,
                checkpointPos.Y + _spawnOffsetY,
                checkpointPos.Z - forwardZ * _forwardOffset
            );

            // Update the player's checkpoint
            PlayerMovement player = PlayerManager.GetPlayer();
            if (player != null)
            {
                player.UpdateCheckpoint(spawnPos);
                player.RestoreHealth(2);
                _activated = true;

                // Trigger the sprites if specified
                if (_spriteIDs.Count > 0)
                {
                    _spriteTimer = _spriteDisplayDuration;
                    _totalActiveTime = 0.0f;
                    _spriteAlpha = 0.0f;
                    foreach (ulong id in _spriteIDs)
                        API.SetSpriteAlpha(id, 0.0f);
                }

                if (_triggerIntroCutscene)
                {
                    // After sprites fade out, THEN play cutscene, THEN play dialogue
                    _spriteOnCompleteAction = () =>
                    {
                        CutsceneSequencer.PlayWithCallback(_cutsceneEntityName, () =>
                        {
                            StoryDialogueManager.PlayCheckpoint1Sequence();
                        });
                    };
                }

                // Re-find light entities during activation to ensure they are found
                _lightIDs.Clear();
                if (!string.IsNullOrEmpty(_lightNames))
                {
                    string[] names = _lightNames.Split(',');
                    foreach (string name in names)
                    {
                        string trimmedName = name.Trim();
                        ulong id = API.FindEntity(trimmedName);
                        if (id != 0)
                        {
                            _lightIDs.Add(id);
                        }
                    }
                }

                API.Log($"[CPTrigger] Activating {_lightIDs.Count} lights...");

                // Turn on lights
                foreach (ulong id in _lightIDs)
                {
                    if (API.HasSpotLight(id))
                    {
                        API.Log($"[CPTrigger] Setting spot light {id} intensity to {_activeIntensity}");
                        API.SetSpotLightIntensity(id, _activeIntensity);
                        API.SetSpotLightColor(id, _activeColor);
                    }
                    else if (API.HasPointLight(id))
                    {
                        API.Log($"[CPTrigger] Setting point light {id} intensity to {_activeIntensity}");
                        API.SetPointLightIntensity(id, _activeIntensity);
                        API.SetPointLightColor(id, _activeColor);
                    }
                    else
                    {
                        API.Log($"[CPTrigger] WARNING: Entity {id} has no SpotLight or PointLight component!");
                    }
                }
                
                // Hide text permanently if it exists on this entity
                if (API.HasText(Entity))
                {
                    Vec4 color = API.GetTextColor(Entity);
                    color.W = 0.0f;
                    API.SetTextColor(Entity, color);
                }

                API.Log($"[CPTrigger] Checkpoint saved and lights activated at ({spawnPos.X:F2}, {spawnPos.Y:F2}, {spawnPos.Z:F2})");
            }
            else
            {
                API.Log("[CPTrigger] ERROR: Could not find player instance!");
            }
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity))
                s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            return a + (b - a) * t;
        }

        private static void OnTriggerEnterCallback(ulong triggerEntity, ulong otherEntity)
        {
            CPTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only player triggers this
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInZone = true;

            if (!inst._activated)
            {
                // Activate checkpoint immediately on entry
                inst.ActivateCheckpoint();
                
                // Show text if it exists on this entity
                if (API.HasText(inst.Entity))
                {
                    Vec4 color = API.GetTextColor(inst.Entity);
                    color.W = 1.0f;
                    API.SetTextColor(inst.Entity, color);
                }
                API.Log("[CPTrigger] Player entered checkpoint zone. Checkpoint activated automatically.");
            }
        }

        private static void OnTriggerExitCallback(ulong triggerEntity, ulong otherEntity)
        {
            CPTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only player triggers this
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInZone = false;

            // Hide text if it exists on this entity
            if (API.HasText(inst.Entity))
            {
                Vec4 color = API.GetTextColor(inst.Entity);
                color.W = 0.0f;
                API.SetTextColor(inst.Entity, color);
            }

            API.Log("[CPTrigger] Player left checkpoint zone.");
        }
    }
}