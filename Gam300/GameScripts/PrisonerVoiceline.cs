using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Plays random prisoner voicelines when the player is nearby.
    /// Attach to a prisoner NPC entity.
    /// </summary>
    public class PrisonerVoiceline
    {
        public ulong Entity;

        [EditorExposed("Min Distance", "Distance at full volume", 0f, 50f, true)]
        private float _minDistance = 1.0f;

        [EditorExposed("Max Distance", "Distance at which sound is silent", 0f, 100f, true)]
        private float _maxDistance = 25.0f;

        [EditorExposed("Trigger Distance", "Player must be within this distance to trigger voiceline", 0f, 50f, true)]
        private float _triggerDistance = 25.0f;

        [EditorExposed("Volume", "Playback volume", 0f, 1f, true)]
        private float _volume = 0.7f;

        [EditorExposed("Min Delay", "Minimum seconds between voicelines", 0f, 60f, true)]
        private float _minDelay = 1.0f;

        [EditorExposed("Max Delay", "Maximum seconds between voicelines", 0f, 120f, true)]
        private float _maxDelay = 3.0f;

        [EditorExposed("Initial Delay", "Seconds to wait before first voiceline", 0f, 30f, true)]
        private float _initialDelay = 0.5f;

        [EditorExposed("First Voice File", "Select the _1.wav file (e.g. VO_Prisoner1_FX_1.wav)")]
        private string _firstVoiceFile = "Resources/Audio/VO_Prisoner1_FX_1.wav";

        [EditorExposed("File Count", "Number of audio files (1 to 9)", 1f, 20f, true)]
        private int _fileCount = 9;

        private string[] _voicelinePaths;
        private Random _random;
        private float _timer;
        private float _nextPlayTime;
        private bool _initialized;
        private string _soundKeyBase;
        private int _lastPlayedIndex = -1;

        public void OnStart(string jsonParams)
        {
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);

            _random = new Random();
            _soundKeyBase = $"npc_vo_{Entity}";

            // Build file paths from first file pattern
            // Extracts prefix by removing the "1.wav" from end of first file
            string basePattern = _firstVoiceFile;
            if (basePattern.EndsWith("1.wav"))
            {
                basePattern = basePattern.Substring(0, basePattern.Length - 5); // Remove "1.wav"
            }
            else if (basePattern.EndsWith("_1.wav"))
            {
                basePattern = basePattern.Substring(0, basePattern.Length - 6); // Remove "_1.wav"
            }

            _voicelinePaths = new string[_fileCount];
            for (int i = 0; i < _fileCount; i++)
            {
                _voicelinePaths[i] = $"{basePattern}{i + 1}.wav";
            }

            // Preload all voicelines for faster playback
            for (int i = 0; i < _voicelinePaths.Length; i++)
            {
                API.PreloadSound($"{_soundKeyBase}_{i}", _voicelinePaths[i], false);
            }

            // Set initial delay before first voiceline
            _timer = 0f;
            _nextPlayTime = _initialDelay;
            _initialized = true;
        }

        public void OnUpdate(float dt)
        {
            if (!_initialized) return;

            _timer += dt;

            // Check if it's time to potentially play a voiceline
            if (_timer < _nextPlayTime) return;

            // Get player entity
            ulong playerEntity = PlayerMovement.GetPlayerEntity();
            if (playerEntity == 0) return;

            // Check distance to player
            Vec3 myPos = API.GetPosition(Entity);
            Vec3 playerPos = API.GetPosition(playerEntity);

            float dx = playerPos.X - myPos.X;
            float dy = playerPos.Y - myPos.Y;
            float dz = playerPos.Z - myPos.Z;
            float distSq = dx * dx + dy * dy + dz * dz;
            float triggerDistSq = _triggerDistance * _triggerDistance;

            // Only play if player is within trigger distance
            if (distSq <= triggerDistSq)
            {
                PlayRandomVoiceline(myPos);
            }

            // Schedule next check/play time (random delay)
            _nextPlayTime = _timer + _minDelay + (float)(_random.NextDouble() * (_maxDelay - _minDelay));
        }

        private void PlayRandomVoiceline(Vec3 position)
        {
            // Pick a random voiceline (avoid repeating the same one)
            int index;
            if (_voicelinePaths.Length > 1)
            {
                do
                {
                    index = _random.Next(_voicelinePaths.Length);
                } while (index == _lastPlayedIndex);
            }
            else
            {
                index = 0;
            }
            _lastPlayedIndex = index;

            string soundKey = $"{_soundKeyBase}_{index}";
            string filePath = _voicelinePaths[index];

            // Play at entity position with 3D audio
            API.PlaySoundAt(soundKey, filePath, position, false);
            API.SetSoundVolume(soundKey, _volume);
            API.Set3DMinMaxDistance(soundKey, _minDistance, _maxDistance);
        }

        public void OnDestroy()
        {
            // Sounds will be cleaned up automatically by SoundEngine
        }
    }
}
