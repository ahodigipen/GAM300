using Boom;
using System;
using System.Collections.Generic;
using System.IO;

namespace GameScripts
{
    public class CutsceneSequencer
    {
        public ulong Entity;

        // Static Registry by ID (Used by animation tracks/events)
        public static System.Collections.Generic.Dictionary<ulong, CutsceneSequencer> InstancesById = new System.Collections.Generic.Dictionary<ulong, CutsceneSequencer>();

        public static void PlayCutscene(string entityName)
        {
            ulong id = API.FindEntity(entityName);
            if (id != 0 && InstancesById.ContainsKey(id))
            {
                InstancesById[id].Play();
            }
            else
            {
                API.Log($"[Cutscene] Could not find cutscene script on '{entityName}'");
            }
        }

        // Properties (Exposed to Editor)
        [Boom.EditorExposed("Cutscene File", "The .seq file to load from Resources/Cutscenes/")]
        public string CutsceneFile = "Test.seq";
        
        [Boom.EditorExposed("Play On Start", "Whether the cutscene plays automatically when the scene loads")]
        public bool PlayOnStart = false;
        
        [Boom.EditorExposed("Loop Cutscene", "Whether the sequence loops at the end")]
        public bool Loop = false;
        
        [Boom.EditorExposed("Block Input", "Whether player input is disabled during the cutscene")]
        public bool BlockInput = true;
        
        [Boom.EditorExposed("Allow Skip", "Whether the player can press a button to skip the cinematic")]
        public bool AllowSkip = true;
        
        [Boom.EditorExposed("Console Debug", "Log position and sequencer events to the console")]
        public bool ConsoleDebug = true;
        
        [Boom.EditorExposed("Visual Debug", "Draw lines indicating track paths")]
        public bool VisualDebug = false;

        [Boom.EditorExposed("Start Delay", "Seconds to wait before starting", 0.0f, 60.0f, true)]
        public float StartDelay = 0.0f;

        private float _currentTime = 0f;
        private bool _isPlaying = false;
        private bool _pendingPlay = false;
        private float _delayTimer = 0f;
        private float _logTimer = 0f;
        private int _duration = 600;

        public class KeyFrame
        {
            public int frame;
            public float time; // frame / 60.0f
            public float vX, vY, vZ, vW;
            public string valStr = ""; // Animation Name
        }

        public class Track
        {
            public string label; // "EntityName : Type"
            public string targetEntityName;
            public int type; // 0=Pos, 1=Rot, 2=Scale, 3=Anim, 4=Look At
            public List<KeyFrame> keyframes = new List<KeyFrame>();
            public ulong cachedEntityID = 0;
            public string lastAnim = ""; // Cache to prevent spamming Play
        }

        public void AddTrack(Track t)
        {
            _tracks.Add(t);
        }

        public void ClearTracks()
        {
            _tracks.Clear();
            _duration = 0;
        }

        private List<Track> _tracks = new List<Track>();



        public void OnStart(string jsonParams)
        {
            // Register self
            //string myName = "Unknown";
            // We don't have GetName in API directly exposed for Entity ID? 
            // We can assume the user passes unique names or we use Entity ID registry.
            // For now, let's use the Entity Name searching.

            // WORKAROUND: We can't easily get our own name if not provided.
            // But the user usually searches by name.
            // Let's rely on finding our own name or registering by ID if needed.
            // Actually, we can just register by the cutscene file name or require a name param.

            // Simplification: Register by Entity ID as string? Or just assume the user knows the name.
            // Let's try to find our own name via reverse lookup if possible, or just expect the user to ensure distinct names.

            // Better: User provides "MyName" in jsonParams? 
            // Or we just don't register by name yet.

            // Wait, API.FindEntity("Name") returns ID. We have ID.
            // We can register via a hack: The user calls Play("EntityName"). 
            // Use FindEntity to get ID -> Look up in ID-based registry.

            RegisterInstance();

            API.Log($"[CutsceneDebug] OnStart Called. Raw Params: '{jsonParams}'");

            // Parse JSON Params - No longer needed, EditorExposed handles this.
            // The manual JSON parsing block has been removed as EditorExposed fields now handle these properties.

            API.Log($"[Cutscene] Initializing... File: '{CutsceneFile}', BlockInput: {BlockInput}, Delay: {StartDelay}");
            LoadCutscene(CutsceneFile);

            if (PlayOnStart)
            {
                if (StartDelay > 0f)
                {
                    _pendingPlay = true;
                    _delayTimer = StartDelay;
                    API.Log($"[Cutscene] Playing deferred by {StartDelay}s...");
                }
                else
                {
                    Play();
                }
            }
        }

        private void RegisterInstance()
        {
            // Static registry by Entity ID is safer
            if (!InstancesById.ContainsKey(Entity)) InstancesById.Add(Entity, this);
        }

        public void OnDestroy()
        {
            if (InstancesById.ContainsKey(Entity)) InstancesById.Remove(Entity);
            PlayerMovement.CutsceneMode = false;
        }

        public void Play()
        {
            API.Log("[CutsceneSequencer] Play() Called!");
            _isPlaying = true;
            _currentTime = 0f;

            // Explicit Blocking
            if (BlockInput)
            {
                PlayerMovement.CutsceneMode = true;
                API.Log("[Cutscene] Player Input BLOCKED.");
            }

            // NEW: Disable Engine ThirdPersonCamera logic
            API.SetCutsceneMode(true);

            // Recalculate duration in case tracks were added programmatically
            RecalculateDuration();

            // Reset animation cache on play
            foreach (var t in _tracks) t.lastAnim = "";
        }

        public void RecalculateDuration()
        {
            int lastKeyFrame = 0;
            foreach (var t in _tracks)
            {
                foreach (var k in t.keyframes)
                {
                    if (k.frame > lastKeyFrame) lastKeyFrame = k.frame;
                }
            }
            _duration = lastKeyFrame;
            API.Log($"[CutsceneSequencer] Recalculated Duration: {_duration}");
        }

        public void Skip()
        {
            if (!_isPlaying) return;
            API.Log("[CutsceneSequencer] Skip triggered.");
            _currentTime = _duration;
            ApplyTracks(_duration); // Force final state
            Stop();
        }

        public void Stop()
        {
            _isPlaying = false;
            _currentTime = 0f;

            PlayerMovement.CutsceneMode = false;

            // NEW: Re-enable Engine ThirdPersonCamera logic
            API.SetCutsceneMode(false);

            // Re-enable state machines for all affected entities

            // Re-enable state machines for all affected entities
            foreach (var t in _tracks)
            {
                if (t.cachedEntityID != 0 && t.type == 3) // Anim Track
                {
                    if (API.HasAnimator(t.cachedEntityID))
                    {
                        // Force reset to empty or Idle before re-enabling SM
                        // otherwise the old clip might keep looping
                        API.AnimatorPlay(t.cachedEntityID, "");
                        API.AnimatorSetStateMachineEnabled(t.cachedEntityID, true);
                    }
                }
            }
        }

        public void OnUpdate(float dt)
        {
            try
            {
                if (_pendingPlay)
                {
                    _delayTimer -= dt;
                    if (_delayTimer <= 0f)
                    {
                        _pendingPlay = false;
                        Play();
                    }
                    return;
                }

                if (!_isPlaying) return;

                // INPUT SKIPPING
                if (AllowSkip)
                {
                    if (API.IsKeyDown(API.KEY_SPACE) || API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_START) || API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A))
                    {
                        Skip();
                        return;
                    }
                }

                _currentTime += dt * 60.0f; // Convert time to frames

                if (_currentTime >= _duration)
                {
                    API.Log($"[CutsceneDebug] Reached End! Time: {_currentTime}, Duration: {_duration}");
                    if (Loop)
                    {
                        _currentTime = 0f;
                        foreach (var t in _tracks) t.lastAnim = "";
                    }
                    else
                    {
                        _currentTime = _duration;
                        Stop();
                        return;
                    }
                }

                ApplyTracks(_currentTime);

                if (ConsoleDebug)
                {
                    _logTimer += dt;
                    if (_logTimer >= 1.0f)
                    {
                        LogCameraStatus();
                        _logTimer = 0f;
                    }
                }
                if (VisualDebug) DrawDebugVisuals();
            }
            catch (Exception ex)
            {
                API.Log($"[CutsceneDebug] CRITICAL ERROR in OnUpdate: {ex.Message}");
            }
        }

        private void LogCameraStatus()
        {
            // Try to find the camera by common names
            ulong camID = API.FindEntity("Camera");
            if (camID == 0) camID = API.FindEntity("Main Camera");
            if (camID == 0) camID = API.FindEntity("MainCamera");

            // If still not found, try to grab it from the first track
            if (camID == 0 && _tracks.Count > 0) camID = _tracks[0].cachedEntityID;

            // Fallback to self
            Vec3 camPos = (camID != 0) ? API.GetPosition(camID) : API.GetPosition(Entity);

            string targetInfo = "None";
            foreach (var t in _tracks)
            {
                if (t.type == 4)
                {
                    foreach (var k in t.keyframes)
                    {
                        if (_currentTime >= k.frame) targetInfo = k.valStr;
                    }
                }
            }
            API.Log($"[CutsceneStatus] Frame: {(int)_currentTime} | CamPos: {camPos.ToString()} | Target: {targetInfo}");
        }

        private void DrawDebugVisuals()
        {
            if (!_isPlaying) return;
            foreach (var t in _tracks)
            {
                if (t.type == 0 && t.keyframes.Count > 1)
                {
                    for (int i = 0; i < t.keyframes.Count - 1; i++)
                    {
                        var k1 = t.keyframes[i];
                        var k2 = t.keyframes[i + 1];
                        API.DrawDebugLine(new Vec3(k1.vX, k1.vY, k1.vZ), new Vec3(k2.vX, k2.vY, k2.vZ), new Vec3(1f, 1f, 0f));
                    }
                }
                if (t.type == 4)
                {
                    string targetName = "";
                    foreach (var k in t.keyframes) if (_currentTime >= k.frame) targetName = k.valStr;

                    if (!string.IsNullOrEmpty(targetName))
                    {
                        ulong targetID = API.FindEntity(targetName);
                        if (targetID != 0)
                        {
                            Vec3 camPos = API.GetPosition(t.cachedEntityID);
                            Vec3 targetPos = API.GetPosition(targetID);
                            API.DrawDebugLine(camPos, targetPos, new Vec3(0f, 0.5f, 1f));
                        }
                    }
                }
            }
        }

        private void ApplyTracks(float currentFrame)
        {
            foreach (var track in _tracks)
            {
                // Find entity (cache it)
                if (track.cachedEntityID == 0)
                {
                    track.cachedEntityID = API.FindEntity(track.targetEntityName);
                    if (track.cachedEntityID == 0)
                    {
                        // Throttle error
                        if ((int)currentFrame % 60 == 0) API.Log($"[CutsceneDebug] FAIL: Could not find entity '{track.targetEntityName}' for track '{track.label}'");
                        continue;
                    }
                }

                // ANIMATION TRACK (Type 3)
                if (track.type == 3)
                {
                    if (track.keyframes.Count < 1) continue;

                    // Find the active keyframe (the last one passed)
                    KeyFrame activeKF = null;
                    for (int i = 0; i < track.keyframes.Count; i++)
                    {
                        if (currentFrame >= track.keyframes[i].frame)
                        {
                            activeKF = track.keyframes[i];
                        }
                        else
                        {
                            break;
                        }
                    }

                    if (activeKF != null && !string.IsNullOrEmpty(activeKF.valStr) && activeKF.valStr != "None")
                    {
                        // Only play if changed
                        if (track.lastAnim != activeKF.valStr)
                        {
                            API.AnimatorSetStateMachineEnabled(track.cachedEntityID, false);
                            API.AnimatorPlay(track.cachedEntityID, activeKF.valStr);
                            track.lastAnim = activeKF.valStr;
                        }
                    }
                    continue;
                }


                // LOOK AT TRACK (Type 4)
                if (track.type == 4)
                {
                    if (track.keyframes.Count < 1) continue;

                    // Find active keyframe
                    KeyFrame activeKF = null;
                    for (int i = 0; i < track.keyframes.Count; i++)
                    {
                        if (currentFrame >= track.keyframes[i].frame) activeKF = track.keyframes[i];
                        else break;
                    }

                    if (activeKF != null && !string.IsNullOrEmpty(activeKF.valStr) && activeKF.valStr != "None")
                    {
                        ulong targetID = API.FindEntity(activeKF.valStr);
                        if (targetID != 0)
                        {
                            Vec3 targetPos = API.GetPosition(targetID);
                            // Offset to look at Head/Chest (approx 1.5m up) instead of feet/pivot
                            targetPos.Y += 1.5f;

                            Vec3 camPos = API.GetPosition(track.cachedEntityID);

                            float dx = targetPos.X - camPos.X;
                            float dz = targetPos.Z - camPos.Z;
                            float dy = targetPos.Y - camPos.Y;

                            // Pitch
                            float dist = (float)Math.Sqrt(dx * dx + dz * dz);
                            // TRY RADIANS (Remove 180/PI)
                            float pitch = (float)(Math.Atan2(dy, dist));

                            // Yaw
                            float yaw;
                            // SINGULARITY CHECK
                            if (dist < 0.5f)
                            {
                                yaw = API.GetRotation(track.cachedEntityID).Y;
                            }
                            else
                            {
                                // TRY RADIANS (Remove 180/PI)
                                yaw = (float)(Math.Atan2(dx, dz));
                            }

                            // X=Pitch, Y=Yaw, Z=Roll
                            API.SetRotation(track.cachedEntityID, new Vec3(-pitch, yaw, 0f));
                        }
                    }
                    continue;
                }


                // EVENT TRIGGER TRACK (Type 5)
                if (track.type == 5)
                {
                    if (track.keyframes.Count < 1) continue;

                    // Find active keyframe
                    KeyFrame activeKF = null;
                    for (int i = 0; i < track.keyframes.Count; i++)
                    {
                        if (currentFrame >= track.keyframes[i].frame) activeKF = track.keyframes[i];
                        else break;
                    }

                    if (activeKF != null && !string.IsNullOrEmpty(activeKF.valStr) && activeKF.valStr != "None")
                    {
                        if (track.lastAnim != activeKF.valStr)
                        {
                            track.lastAnim = activeKF.valStr;
                            API.Log($"[CutsceneSequencer] Triggering Event: {activeKF.valStr}");

                            try
                            {
                                // We assume format "GameScripts.ClassName.MethodName"
                                int lastDotPos = activeKF.valStr.LastIndexOf('.');
                                if (lastDotPos > 0 && lastDotPos < activeKF.valStr.Length - 1)
                                {
                                    string className = activeKF.valStr.Substring(0, lastDotPos);
                                    string methodName = activeKF.valStr.Substring(lastDotPos + 1);

                                    Type type = Type.GetType(className);
                                    if (type != null)
                                    {
                                        System.Reflection.MethodInfo method = type.GetMethod(methodName, System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static);
                                        if (method != null)
                                        {
                                            method.Invoke(null, null);
                                        }
                                        else
                                        {
                                            API.Log($"[CutsceneSequencer] Event Error: Static method '{methodName}' not found on '{className}'");
                                        }
                                    }
                                    else
                                    {
                                        API.Log($"[CutsceneSequencer] Event Error: Class '{className}' not found.");
                                    }
                                }
                                else
                                {
                                    API.Log($"[CutsceneSequencer] Event format must be 'Namespace.Class.Method'. Received: {activeKF.valStr}");
                                }
                            }
                            catch (Exception ex)
                            {
                                API.Log($"[CutsceneSequencer] Failed to trigger event '{activeKF.valStr}': {ex.Message}");
                            }
                        }
                    }
                    continue;
                }


                // TRANSFORM TRACKS (Need at least 1 keyframe)
                if (track.keyframes.Count == 0) continue;

                // 1. Handle "Before Start" -> Hold First Keyframe
                if (currentFrame <= track.keyframes[0].frame)
                {
                    KeyFrame k = track.keyframes[0];
                    if (track.type == 0) API.TeleportRigidBody(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                    else if (track.type == 1) API.SetRotation(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                    else if (track.type == 2) API.SetScale(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                    continue;
                }

                // 2. Handle "After End" -> Hold Last Keyframe
                if (currentFrame >= track.keyframes[track.keyframes.Count - 1].frame)
                {
                    KeyFrame k = track.keyframes[track.keyframes.Count - 1];
                    if (track.type == 0) API.TeleportRigidBody(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                    else if (track.type == 1) API.SetRotation(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                    else if (track.type == 2) API.SetScale(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                    continue;
                }

                // 3. Interpolate between Keyframes
                KeyFrame k1 = null, k2 = null;
                for (int i = 0; i < track.keyframes.Count - 1; i++)
                {
                    if (currentFrame >= track.keyframes[i].frame && currentFrame < track.keyframes[i + 1].frame)
                    {
                        k1 = track.keyframes[i];
                        k2 = track.keyframes[i + 1];
                        break;
                    }
                }

                if (k1 != null && k2 != null)
                {
                    float range = (float)(k2.frame - k1.frame);
                    float t = 0.0f;
                    if (range > 0.0001f) t = (currentFrame - k1.frame) / range;

                    // Clamp t just in case
                    if (t < 0f) t = 0f;
                    if (t > 1f) t = 1f;

                    // Lerp
                    float x = Lerp(k1.vX, k2.vX, t);
                    float y = Lerp(k1.vY, k2.vY, t);
                    float z = Lerp(k1.vZ, k2.vZ, t);

                    if (track.type == 0) // Pos
                    {
                        // DEBUG: Log the position we are setting
                        if (((int)currentFrame) % 120 == 0) API.Log($"[CutsceneDebug] Moving '{track.targetEntityName}' to {x:F2}, {y:F2}, {z:F2}");

                        // Use SetPosition for generic objects (works for Camera too)
                        API.SetPosition(track.cachedEntityID, new Vec3(x, y, z));
                    }
                    else if (track.type == 1) // Rot
                    {
                        API.SetRotation(track.cachedEntityID, new Vec3(x, y, z));
                    }
                    else if (track.type == 2) // Scale
                    {
                        API.SetScale(track.cachedEntityID, new Vec3(x, y, z));
                    }
                }
            }
        }
        private float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        private void LoadCutscene(string filename)
        {
            try
            {
                // 1. Try Resources path
                string path = "Resources/Cutscenes/" + filename;
                if (!File.Exists(path))
                {
                    API.Log($"[CutsceneDebug] Not found at: {path}");
                    // 2. Try direct path
                    path = filename;
                    if (!File.Exists(path))
                    {
                        API.Log($"[Cutscene] FATAL: File not found: {filename}");
                        return;
                    }
                }

                API.Log($"[CutsceneDebug] Loading from: {path}");

                string content = File.ReadAllText(path);
                if (string.IsNullOrEmpty(content)) API.Log("[Cutscene] File was empty!");

                _tracks.Clear();
                string[] lines = content.Split('\n');
                // ... rest of loading ...
                Track currentTrack = null;

                foreach (string line in lines)
                {
                    if (string.IsNullOrWhiteSpace(line)) continue;
                    string[] parts = line.Split(' ');
                    string token = parts[0];

                    if (token == "DURATION")
                    {
                        if (parts.Length > 1) int.TryParse(parts[1], out _duration);
                    }
                    else if (token == "TRACK")
                    {
                        // TRACK "Label" Type
                        int firstQuote = line.IndexOf('"');
                        int lastQuote = line.LastIndexOf('"');
                        if (firstQuote != -1 && lastQuote != -1)
                        {
                            string label = line.Substring(firstQuote + 1, lastQuote - firstQuote - 1);
                            string typeStr = line.Substring(lastQuote + 2);
                            int type = 0;
                            int.TryParse(typeStr, out type);

                            currentTrack = new Track();
                            currentTrack.label = label;
                            currentTrack.type = type;

                            // Parse "Entity : Type" to get Entity
                            // C++ saves as "Entity : Type"
                            int sep = label.IndexOf(" : ");
                            if (sep != -1) currentTrack.targetEntityName = label.Substring(0, sep);
                            else currentTrack.targetEntityName = label; // Fallback

                            _tracks.Add(currentTrack);
                        }
                    }
                    else if (token == "KEY" && currentTrack != null)
                    {
                        if (parts.Length >= 6)
                        {
                            KeyFrame kf = new KeyFrame();
                            int.TryParse(parts[1], out kf.frame);
                            float.TryParse(parts[2], out kf.vX);
                            float.TryParse(parts[3], out kf.vY);
                            float.TryParse(parts[4], out kf.vZ);
                            float.TryParse(parts[5], out kf.vW);

                            // Parse String if present (Animation name)
                            int firstQ = line.IndexOf('"');
                            int lastQ = line.LastIndexOf('"');
                            if (firstQ != -1 && lastQ > firstQ)
                            {
                                kf.valStr = line.Substring(firstQ + 1, lastQ - firstQ - 1);
                            }

                            currentTrack.keyframes.Add(kf);
                        }
                    }
                }

                // Auto-Calculate Duration to match the last keyframe
                int lastKeyFrame = 0;
                foreach (var t in _tracks)
                {
                    foreach (var k in t.keyframes)
                    {
                        if (k.frame > lastKeyFrame) lastKeyFrame = k.frame;
                    }
                }

                // If we found keys, update duration to avoid dead air
                if (lastKeyFrame > 0)
                {
                    _duration = lastKeyFrame;
                }

                API.Log($"[Cutscene] Loaded {_tracks.Count} tracks. Duration: {_duration} (Auto-Trimmed)");
                foreach (var t in _tracks)
                {
                    API.Log($"[CutsceneDebug] Track: '{t.label}' -> Target: '{t.targetEntityName}' (Type: {t.type}, KFs: {t.keyframes.Count})");
                }
            }
            catch (Exception ex)
            {
                API.Log($"[Cutscene] Error loading file: {ex.Message}");
            }
        }
    }
}
