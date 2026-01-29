using Boom;
using System;
using System.Collections.Generic;
using System.IO;

namespace GameScripts
{
    public class CutsceneSequencer
    {
        public ulong Entity;

        // Properties
        public string CutsceneFile = "Test.seq";
        public bool PlayOnStart = false;
        public bool Loop = false;

        private float _currentTime = 0f;
        private bool _isPlaying = false;
        private int _duration = 600;

        private class KeyFrame
        {
            public int frame;
            public float time; // frame / 60.0f
            public float vX, vY, vZ, vW;
            public string valStr = ""; // Animation Name
        }

        private class Track
        {
            public string label; // "EntityName : Type"
            public string targetEntityName;
            public int type; // 0=Pos, 1=Rot, 2=Scale, 3=Anim
            public List<KeyFrame> keyframes = new List<KeyFrame>();
            public ulong cachedEntityID = 0;
            public string lastAnim = ""; // Cache to prevent spamming Play
        }

        private List<Track> _tracks = new List<Track>();

        public void OnStart(string jsonParams)
        {
             API.Log($"[Cutscene] Initializing for {CutsceneFile}");
             LoadCutscene(CutsceneFile);

             if (PlayOnStart) Play();
        }

        public void Play()
        {
            _isPlaying = true;
            _currentTime = 0f;
            // Reset animation cache on play
            foreach(var t in _tracks) t.lastAnim = "";
        }

        public void Stop()
        {
            _isPlaying = false;
            _currentTime = 0f;
        }

        public void OnUpdate(float dt)
        {
            if (!_isPlaying) return;

            _currentTime += dt * 60.0f; // Convert time to frames (60fps base)

            if (_currentTime >= _duration)
            {
                if (Loop) {
                    _currentTime = 0f;
                    foreach(var t in _tracks) t.lastAnim = ""; // Reset cache on loop
                }
                else {
                    _currentTime = _duration;
                    _isPlaying = false;
                }
            }

            ApplyTracks(_currentTime);
        }

        private void ApplyTracks(float currentFrame)
        {
            foreach (var track in _tracks)
            {
                 // Find entity (cache it)
                 if (track.cachedEntityID == 0) {
                     track.cachedEntityID = API.FindEntity(track.targetEntityName);
                     if (track.cachedEntityID == 0) continue;
                 }
                 
                 // ANIMATION TRACK (Type 3)
                 if (track.type == 3)
                 {
                     if (track.keyframes.Count < 1) continue;

                     // Find the active keyframe (the last one passed)
                     KeyFrame activeKF = null;
                     for (int i = 0; i < track.keyframes.Count; i++)
                     {
                         if (currentFrame >= track.keyframes[i].frame) {
                             activeKF = track.keyframes[i];
                         }
                        else {
                             break;
                         }
                     }
                     
                     if (activeKF != null && !string.IsNullOrEmpty(activeKF.valStr) && activeKF.valStr != "None")
                     {
                         // Only play if changed
                         if (track.lastAnim != activeKF.valStr)
                         {
                             API.AnimatorPlay(track.cachedEntityID, activeKF.valStr);
                             track.lastAnim = activeKF.valStr;
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
                     if (track.type == 0) API.SetPosition(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                     else if (track.type == 1) API.SetRotation(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                     else if (track.type == 2) API.SetScale(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                     continue;
                 }

                 // 2. Handle "After End" -> Hold Last Keyframe
                 if (currentFrame >= track.keyframes[track.keyframes.Count - 1].frame)
                 {
                     KeyFrame k = track.keyframes[track.keyframes.Count - 1];
                     if (track.type == 0) API.SetPosition(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                     else if (track.type == 1) API.SetRotation(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                     else if (track.type == 2) API.SetScale(track.cachedEntityID, new Vec3(k.vX, k.vY, k.vZ));
                     continue;
                 }

                 // 3. Interpolate between Keyframes
                 KeyFrame k1 = null, k2 = null;
                 for (int i = 0; i < track.keyframes.Count - 1; i++)
                 {
                     if (currentFrame >= track.keyframes[i].frame && currentFrame < track.keyframes[i+1].frame)
                     {
                         k1 = track.keyframes[i];
                         k2 = track.keyframes[i+1];
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
            string path = "Resources/Cutscenes/" + filename;
            if (!File.Exists(path)) {
                 path = filename; 
                 if (!File.Exists(path)) {
                     API.Log($"[Cutscene] File not found: {filename}");
                     return;
                 }
            }

            _tracks.Clear();
            string[] lines = File.ReadAllLines(path);
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
                    if (parts.Length >= 6) {
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
            API.Log($"[Cutscene] Loaded {_tracks.Count} tracks");
        }
    }
}
