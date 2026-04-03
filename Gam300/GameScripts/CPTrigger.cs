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

        [Boom.EditorExposed("Checkpoint Sprite Names", "Comma-separated names of sprite entities to show ALL AT ONCE")]
        private string _checkpointSpriteNames = "";

        [Boom.EditorExposed("Sequential Sprite Names", "Comma-separated names of sprites to show ONE AFTER ANOTHER")]
        private string _sequentialSpriteNames = "";

        [Boom.EditorExposed("Bobbing Sprite Names", "Comma-separated names of sprites that should perform the sin wave bobbing")]
        private string _bobbingSpriteNames = "";

        [Boom.EditorExposed("Floating Sprite Names", "Comma-separated names of sprites that should float upwards")]
        private string _floatingSpriteNames = "";

        [Boom.EditorExposed("Is Checkpoint 1", "Whether to play the intro cutscene and checkpoint dialogue when activated")]
        private bool _triggerIntroCutscene = false;

        [Boom.EditorExposed("Is Checkpoint 2", "Whether to play the Checkpoint 2 dialogue sequence instead of Checkpoint 1")]
        private bool _isCheckpoint2 = false;

        [Boom.EditorExposed("Is Boss Checkpoint", "Whether to play the Boss Checkpoint dialogue sequence (overrides Checkpoint 2)")]
        private bool _isBossCheckpoint = false;

        [Boom.EditorExposed("Cutscene Entity Name", "Name of the entity with CutsceneSequencer to play (only used if Trigger Intro Cutscene is true)")]
        private string _cutsceneEntityName = "Intro CutScene";

        [Boom.EditorExposed("Sprite Display Duration", "How long to show each sprite (sequential) or all sprites (simultaneous) in seconds")]
        private float _spriteDisplayDuration = 2.0f;

        [Boom.EditorExposed("Sprite Fade Speed", "Speed of the fade effect")]
        private float _spriteFadeSpeed = 4.0f;

        [Boom.EditorExposed("Float Speed", "Speed of the upward float movement")]
        private float _floatSpeed = 50.0f;

        [Boom.EditorExposed("Bob Speed", "Speed of the horizontal bobbing movement")]
        private float _bobSpeed = 2.0f;

        [Boom.EditorExposed("Bob Amount", "Amount of horizontal bobbing movement")]
        private float _bobAmount = 20.0f;

        [Boom.EditorExposed("VO Audio Clip 1", "Path to first VO audio file (e.g. Resources/Audio/VO_checkpoint1_1.wav)")]
        private string _voAudioClip1 = "";

        [Boom.EditorExposed("VO Audio Clip 2", "Path to second VO audio file (e.g. Resources/Audio/VO_checkpoint1_2.wav)")]
        private string _voAudioClip2 = "";

        private static readonly System.Random s_rng = new System.Random();

        // Track if checkpoint was already activated
        private bool _activated = false;
        private bool _wasInventoryOpen = false;
        private bool _wasGodModeOn = false;
        
        private bool _playerInZone = false;

        private List<ulong> _lightIDs = new List<ulong>();
        private HashSet<ulong> _bobbingSpriteIDs = new HashSet<ulong>();
        private HashSet<ulong> _floatingSpriteIDs = new HashSet<ulong>();
        private Dictionary<ulong, Vec3> _originalSpritePositions = new Dictionary<ulong, Vec3>();

        // Simultaneous Sprite state
        private List<ulong> _simultaneousIDs = new List<ulong>();
        private float _simAlpha = 0.0f;
        private float _simTimer = 0.0f;
        private float _simActiveTime = 0.0f;

        // Sequential Sprite state
        private List<ulong> _sequentialIDs = new List<ulong>();
        private float _seqAlpha = 0.0f;
        private float _seqTimer = 0.0f;
        private float _seqActiveTime = 0.0f;
        private int _currentSpriteIndex = -1;

        // Fired once when ALL sprites fully fade out (used to chain cutscene after UI)
        private Action _spriteOnCompleteAction = null;

        // Short delay before starting cutscene (lets checkpoint sound finish)
        private float _cutsceneDelay = 0f;

        // Static instance tracking
        private static readonly Dictionary<ulong, CPTrigger> s_instances = new Dictionary<ulong, CPTrigger>();

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);

            // Find and initialize lights as OFF
            if (!string.IsNullOrEmpty(_lightNames))
            {
                foreach (string name in _lightNames.Split(','))
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0)
                    {
                        if (API.HasSpotLight(id)) API.SetSpotLightIntensity(id, 0.0f);
                        else if (API.HasPointLight(id)) API.SetPointLightIntensity(id, 0.0f);
                    }
                }
            }

            // Pre-cache sprite metadata (order doesn't matter for bobbing/floating)
            CacheSpriteLists();

            // Initialize text as hidden
            if (API.HasText(Entity))
            {
                Vec4 color = API.GetTextColor(Entity);
                color.W = 0.0f;
                API.SetTextColor(Entity, color);
            }

            if (API.HasCollider(Entity) && !API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnterCallback);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExitCallback);
        }

        private void CacheSpriteLists()
        {
            _bobbingSpriteIDs.Clear();
            if (!string.IsNullOrEmpty(_bobbingSpriteNames))
            {
                foreach (string name in _bobbingSpriteNames.Split(','))
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0) _bobbingSpriteIDs.Add(id);
                }
            }

            _floatingSpriteIDs.Clear();
            if (!string.IsNullOrEmpty(_floatingSpriteNames))
            {
                foreach (string name in _floatingSpriteNames.Split(','))
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0) _floatingSpriteIDs.Add(id);
                }
            }
        }

        public void OnUpdate(float dt)
        {
            // Cutscene delay countdown (fires action once timer expires)
            if (_cutsceneDelay > 0f)
            {
                _cutsceneDelay -= dt;
                if (_cutsceneDelay <= 0f)
                {
                    _cutsceneDelay = 0f;
                    Action cb = _spriteOnCompleteAction;
                    _spriteOnCompleteAction = null;
                    cb?.Invoke();
                }
            }

            bool anySimVisible = UpdateSimultaneousLogos(dt);
            bool anySeqVisible = UpdateSequentialLogos(dt);

            // Trigger completion ONLY when both systems are finished (non-cutscene path)
            if (_cutsceneDelay <= 0f && !anySimVisible && !anySeqVisible && _spriteOnCompleteAction != null)
            {
                Action cb = _spriteOnCompleteAction;
                _spriteOnCompleteAction = null;
                cb?.Invoke();
            }
        }

        private bool UpdateSimultaneousLogos(float dt)
        {
            if (_simultaneousIDs.Count == 0) return false;

            float targetAlpha = (_simTimer > 0) ? 1.0f : 0.0f;
            
            if (_simAlpha > 0.001f || targetAlpha > 0.001f)
            {
                _simAlpha = Lerp(_simAlpha, targetAlpha, _spriteFadeSpeed * dt);
                _simActiveTime += dt;

                foreach (ulong id in _simultaneousIDs)
                {
                    if (API.HasSprite(id))
                    {
                        API.SetSpriteAlpha(id, _simAlpha);
                        ApplyMovement(id, _simActiveTime);
                    }
                }
                
                if (_simTimer > 0) _simTimer -= dt;
                return true;
            }
            else
            {
                // Reset positions when hidden
                if (_simActiveTime > 0f)
                {
                    foreach (ulong id in _simultaneousIDs)
                    {
                        if (API.HasSprite(id)) API.SetSpriteAlpha(id, 0f);
                        if (_originalSpritePositions.ContainsKey(id)) API.SetPosition(id, _originalSpritePositions[id]);
                    }
                    _simActiveTime = 0f;
                }
                _simAlpha = 0f;
                return false;
            }
        }

        private bool UpdateSequentialLogos(float dt)
        {
            if (_sequentialIDs.Count == 0) return false;

            // Fading out the final logo or sequence finished
            if (_currentSpriteIndex < 0 || _currentSpriteIndex >= _sequentialIDs.Count)
            {
                if (_seqAlpha > 0.001f)
                {
                    _seqAlpha = Lerp(_seqAlpha, 0.0f, _spriteFadeSpeed * dt);
                    // Update the last active one for movement while it fades
                    int lastIdx = Math.Max(0, Math.Min(_currentSpriteIndex, _sequentialIDs.Count - 1));
                    ulong lastID = _sequentialIDs[lastIdx];
                    API.SetSpriteAlpha(lastID, _seqAlpha);
                    ApplyMovement(lastID, _seqActiveTime + dt);
                    return true;
                }
                else if (_seqActiveTime > 0f)
                {
                    // Full reset
                    foreach (ulong id in _sequentialIDs)
                    {
                        if (API.HasSprite(id)) API.SetSpriteAlpha(id, 0f);
                        if (_originalSpritePositions.ContainsKey(id)) API.SetPosition(id, _originalSpritePositions[id]);
                    }
                    _seqActiveTime = 0f;
                    _seqAlpha = 0f;
                    _currentSpriteIndex = -1;
                }
                return false;
            }

            ulong currentID = _sequentialIDs[_currentSpriteIndex];
            float targetAlpha = (_seqTimer > 0) ? 1.0f : 0.0f;

            _seqAlpha = Lerp(_seqAlpha, targetAlpha, _spriteFadeSpeed * dt);
            _seqActiveTime += dt;

            // Set alpha for current and ensure others are hidden
            foreach (ulong id in _sequentialIDs)
            {
                if (id == currentID)
                {
                    API.SetSpriteAlpha(id, _seqAlpha);
                    ApplyMovement(id, _seqActiveTime);
                }
                else
                {
                    API.SetSpriteAlpha(id, 0f);
                }
            }

            if (_seqTimer > 0)
            {
                _seqTimer -= dt;
            }
            else if (_seqAlpha < 0.01f)
            {
                // Move to next
                _currentSpriteIndex++;
                if (_currentSpriteIndex < _sequentialIDs.Count)
                {
                    _seqTimer = _spriteDisplayDuration;
                    _seqActiveTime = 0f;
                }
            }
            return true;
        }

        private void ApplyMovement(ulong id, float time)
        {
            if (!_originalSpritePositions.ContainsKey(id)) return;

            Vec3 originalPos = _originalSpritePositions[id];
            float offsetY = _floatingSpriteIDs.Contains(id) ? (_floatSpeed / 100f) * time : 0f;
            float offsetX = _bobbingSpriteIDs.Contains(id) ? (float)Math.Sin(time * _bobSpeed) * (_bobAmount / 100f) : 0f;
            
            API.SetPosition(id, new Vec3(originalPos.X + offsetX, originalPos.Y + offsetY, originalPos.Z));
        }

        private void ActivateCheckpoint()
        {
            _simultaneousIDs.Clear();
            _sequentialIDs.Clear();
            _originalSpritePositions.Clear();

            // Populate simultaneous
            if (!string.IsNullOrEmpty(_checkpointSpriteNames))
            {
                foreach (string name in _checkpointSpriteNames.Split(','))
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0 && API.HasSprite(id))
                    {
                        _simultaneousIDs.Add(id);
                        if (API.HasTransform(id)) _originalSpritePositions[id] = API.GetPosition(id);
                    }
                }
            }

            // Populate sequential
            if (!string.IsNullOrEmpty(_sequentialSpriteNames))
            {
                foreach (string name in _sequentialSpriteNames.Split(','))
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id != 0 && API.HasSprite(id))
                    {
                        _sequentialIDs.Add(id);
                        if (API.HasTransform(id)) _originalSpritePositions[id] = API.GetPosition(id);
                    }
                }
            }

            // Logic for spawn point
            Vec3 checkpointPos = API.GetPosition(Entity);
            Vec3 checkpointRot = API.GetRotation(Entity);
            float yawRad = checkpointRot.Y * (float)Math.PI / 180f;
            float fx = (float)Math.Sin(yawRad);
            float fz = (float)Math.Cos(yawRad);
            Vec3 spawnPos = new Vec3(checkpointPos.X - fx * _forwardOffset, checkpointPos.Y + _spawnOffsetY, checkpointPos.Z - fz * _forwardOffset);

            PlayerMovement player = PlayerManager.GetPlayer();
            if (player != null)
            {
                player.UpdateCheckpoint(spawnPos);
                player.RestoreHealth(2);
                _activated = true;

                // Keep sprites hidden until after dialogue — showPopup will start them
                _simTimer = 0f;
                _simAlpha = 0f;
                _simActiveTime = 0f;

                _currentSpriteIndex = -1;
                _seqTimer = 0f;
                _seqAlpha = 0f;
                _seqActiveTime = 0f;

                foreach (ulong id in _originalSpritePositions.Keys) API.SetSpriteAlpha(id, 0f);

                // Show popup (sprites + text) after dialogue completes
                Action showPopup = () =>
                {
                    // Restore HUD and (if applicable) inventory when popup appears
                    UIManager.ShowHUD();

                    // Play random VO
                    bool hasClip1 = !string.IsNullOrEmpty(_voAudioClip1);
                    bool hasClip2 = !string.IsNullOrEmpty(_voAudioClip2);
                    if (hasClip1 || hasClip2)
                    {
                        string clip = (hasClip1 && hasClip2)
                            ? (s_rng.Next(2) == 0 ? _voAudioClip1 : _voAudioClip2)
                            : (hasClip1 ? _voAudioClip1 : _voAudioClip2);
                        API.PlaySound($"VO_CP_{Entity}", clip, false);
                    }

                    if (_wasInventoryOpen)
                        Entry.IsInventoryOpen = true;
                    if (_wasGodModeOn)
                        PlayerMovement.ForceShowGodModeText();
                    _wasInventoryOpen = false;
                    _wasGodModeOn = false;

                    if (_simultaneousIDs.Count > 0)
                    {
                        _simTimer = _spriteDisplayDuration;
                        _simAlpha = 0f;
                        _simActiveTime = 0f;
                    }
                    if (_sequentialIDs.Count > 0)
                    {
                        _currentSpriteIndex = 0;
                        _seqTimer = _spriteDisplayDuration;
                        _seqAlpha = 0f;
                        _seqActiveTime = 0f;
                    }
                    if (API.HasText(Entity))
                    {
                        Vec4 color = API.GetTextColor(Entity);
                        color.W = 1.0f;
                        API.SetTextColor(Entity, color);
                    }
                };

                Action playDialogue = () =>
                {
                    if (_isBossCheckpoint) StoryDialogueManager.PlayBossCheckpointSequence(showPopup);
                    else if (_isCheckpoint2) StoryDialogueManager.PlayCheckpoint2Sequence(showPopup);
                    else StoryDialogueManager.PlayCheckpoint1Sequence(showPopup);
                };

                // Always save and hide HUD/inventory for any checkpoint with dialogue
                _wasInventoryOpen = Entry.IsInventoryOpen;
                _wasGodModeOn = PlayerMovement.IsGodModeActive;
                UIManager.HideHUD();
                if (_wasInventoryOpen)
                    Entry.IsInventoryOpen = false;

                if (_triggerIntroCutscene)
                {
                    ulong cutsceneId = API.FindEntity(_cutsceneEntityName);
                    if (cutsceneId != 0 && LevelTransitionCutscene.InstancesById.ContainsKey(cutsceneId))
                        LevelTransitionCutscene.PlayWithCallback(_cutsceneEntityName, playDialogue);
                    else
                        CutsceneSequencer.PlayWithCallback(_cutsceneEntityName, playDialogue);
                }
                else
                {
                    playDialogue();
                }

                // Turn on lights
                if (!string.IsNullOrEmpty(_lightNames))
                {
                    foreach (string name in _lightNames.Split(','))
                    {
                        ulong id = API.FindEntity(name.Trim());
                        if (id == 0) continue;
                        if (API.HasSpotLight(id)) { API.SetSpotLightIntensity(id, _activeIntensity); API.SetSpotLightColor(id, _activeColor); }
                        else if (API.HasPointLight(id)) { API.SetPointLightIntensity(id, _activeIntensity); API.SetPointLightColor(id, _activeColor); }
                    }
                }
                
                if (API.HasText(Entity))
                {
                    Vec4 color = API.GetTextColor(Entity);
                    color.W = 0.0f;
                    API.SetTextColor(Entity, color);
                }
            }
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
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
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            if (!inst._activated)
            {
                inst.ActivateCheckpoint();
            }
        }

        private static void OnTriggerExitCallback(ulong triggerEntity, ulong otherEntity)
        {
            CPTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            if (API.HasText(inst.Entity))
            {
                Vec4 color = API.GetTextColor(inst.Entity);
                color.W = 0.0f;
                API.SetTextColor(inst.Entity, color);
            }
        }
    }
}