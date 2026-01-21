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
        }

        private class Track
        {
            public string label; // "EntityName - Properties"
            public string targetEntityName;
            public int type; // 0=Pos, 1=Rot, 2=Scale, 3=Color
            public List<KeyFrame> keyframes = new List<KeyFrame>();
            public ulong cachedEntityID = 0;
        }

        private List<Track> _tracks = new List<Track>();

        public void OnStart(string jsonParams)
        {
            // Parse JSON params for properties if needed
            // For now, hardcode or assume set via Inspector if we had one
            
            // Simple param parsing if json provided (optional)
            /*
            if (!string.IsNullOrEmpty(jsonParams)) {
                // simple json parser here or use what's available
            }
            */

             API.Log($"[Cutscene] Initializing for {CutsceneFile}");
             LoadCutscene(CutsceneFile);

             if (PlayOnStart) Play();
        }

        public void Play()
        {
            _isPlaying = true;
            _currentTime = 0f;
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
                if (Loop) _currentTime = 0f;
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
                 if (track.keyframes.Count < 2) continue;

                 // Find entity (cache it)
                 if (track.cachedEntityID == 0) {
                     track.cachedEntityID = API.FindEntity(track.targetEntityName);
                     if (track.cachedEntityID == 0) continue;
                 }

                 // Find keyframes surrounding currentFrame
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
                     float t = (currentFrame - k1.frame) / (float)(k2.frame - k1.frame);
                     // Lerp
                     float x = Lerp(k1.vX, k2.vX, t);
                     float y = Lerp(k1.vY, k2.vY, t);
                     float z = Lerp(k1.vZ, k2.vZ, t);
                     // float w = Lerp(k1.vW, k2.vW, t);

                     if (track.type == 0) // Pos
                     {
                         // API.SetPosition takes Vec3
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
            // Assuming path relative to Resources or Root
            // Let's try full path construction
            string path = "Resources/Cutscenes/" + filename;
            
            // Check file exist
            if (!File.Exists(path)) {
                // Fallback to absolute if creating from editor
                 path = filename; // Try direct path
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
                    int.TryParse(parts[1], out _duration);
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
                        
                        // Split label to get entity name: "Entity - Type"
                        string[] labelParts = label.Split(new string[]{" - "}, StringSplitOptions.None);
                        if (labelParts.Length > 0) currentTrack.targetEntityName = labelParts[0];
                        
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
                        currentTrack.keyframes.Add(kf);
                    }
                }
            }
            API.Log($"[Cutscene] Loaded {_tracks.Count} tracks");
        }
    }
}
