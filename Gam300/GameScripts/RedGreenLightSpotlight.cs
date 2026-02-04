using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Attaches to a spotlight and alternates between red and green colors.
    /// Plays audio cues when switching between states.
    /// Useful for "Red Light, Green Light" game mechanics.
    /// </summary>
    public class RedGreenLightSpotlight
    {
        public ulong Entity;

        // ===== EXPOSED PARAMETERS =====

        [EditorExposed("Interval Duration", "How long each color lasts (seconds)", 0.5f, 10f, true)]
        private float _intervalDuration = 3.0f;

        [EditorExposed("Red Color R", "Red component of red light (0-1)", 0f, 1f, true)]
        private float _redColorR = 1.0f;

        [EditorExposed("Red Color G", "Green component of red light (0-1)", 0f, 1f, true)]
        private float _redColorG = 0.0f;

        [EditorExposed("Red Color B", "Blue component of red light (0-1)", 0f, 1f, true)]
        private float _redColorB = 0.0f;

        [EditorExposed("Green Color R", "Red component of green light (0-1)", 0f, 1f, true)]
        private float _greenColorR = 0.0f;

        [EditorExposed("Green Color G", "Green component of green light (0-1)", 0f, 1f, true)]
        private float _greenColorG = 1.0f;

        [EditorExposed("Green Color B", "Blue component of green light (0-1)", 0f, 1f, true)]
        private float _greenColorB = 0.0f;

        [EditorExposed("Red Light Audio", "Path to red light audio file")]
        private string _redLightAudioPath = "Resources/Audio/Redlight.wav";

        [EditorExposed("Green Light Audio", "Path to green light audio file")]
        private string _greenLightAudioPath = "Resources/Audio/Greenlight.wav";

        [EditorExposed("Audio Volume", "Volume of the audio cues (0-1)", 0f, 1f, true)]
        private float _audioVolume = 1.0f;

        [EditorExposed("Audio Min Distance", "Minimum 3D audio distance", 1f, 100f, true)]
        private float _audioMinDistance = 5.0f;

        [EditorExposed("Audio Max Distance", "Maximum 3D audio distance", 1f, 500f, true)]
        private float _audioMaxDistance = 50.0f;

        [EditorExposed("Start With Red", "Start with red light (otherwise starts with green)")]
        private bool _startWithRed = true;

        [EditorExposed("Play Audio", "Enable audio playback")]
        private bool _playAudio = false;

        [EditorExposed("Auto Start", "Automatically start cycling on scene load")]
        private bool _autoStart = true;

        // ===== PRIVATE FIELDS =====
        private bool _isRed = true;
        private float _timer = 0f;
        private bool _isRunning = false;
        private string _redSoundId = "redlight_sound";
        private string _greenSoundId = "greenlight_sound";
        private Vec3 _lightPosition;

        public void OnStart(string jsonParams)
        {
            API.Log($"[RedGreenLightSpotlight] OnStart called! Entity ID: {Entity}");

            if (Entity == 0)
            {
                API.Log("[RedGreenLightSpotlight] ERROR: Entity is 0! Script not properly attached.");
                return;
            }

            try
            {
                // Check if entity has a spotlight component
                if (!API.HasSpotLight(Entity))
                {
                    API.Log("[RedGreenLightSpotlight] WARNING: Entity has no spotlight component! Script will be disabled.");
                    _isRunning = false;
                    _autoStart = false;
                    return;
                }

                // Get light position for audio
                if (API.HasTransform(Entity))
                {
                    _lightPosition = API.GetPosition(Entity);
                }

                // Initialize state
                _isRed = _startWithRed;
                _timer = 0f;

                // Set initial color
                if (_isRed)
                {
                    SetRedLight();
                }
                else
                {
                    SetGreenLight();
                }

                // Start if auto-start is enabled
                if (_autoStart)
                {
                    StartCycling();
                }

                API.Log("[RedGreenLightSpotlight] Initialized successfully!");
            }
            catch (Exception ex)
            {
                API.Log($"[RedGreenLightSpotlight] CRITICAL ERROR in OnStart: {ex.Message}");
                _isRunning = false;
            }
        }

        public void OnUpdate(float deltaTime)
        {
            // Safety check - don't run if entity is invalid
            if (Entity == 0 || !_isRunning) return;

            // Update timer
            _timer += deltaTime;

            // Check if it's time to switch
            if (_timer >= _intervalDuration)
            {
                SwitchLight();
                _timer = 0f;
            }
        }

        /// <summary>
        /// Switch between red and green light
        /// </summary>
        private void SwitchLight()
        {
            // Safety check before switching
            if (Entity == 0) return;

            _isRed = !_isRed;

            if (_isRed)
            {
                SetRedLight();
            }
            else
            {
                SetGreenLight();
            }
        }

        /// <summary>
        /// Set spotlight to red and play red audio
        /// </summary>
        private void SetRedLight()
        {
            // Safety check
            if (Entity == 0) return;

            API.Log("[RedGreenLightSpotlight] Switching to RED light");

            // Set spotlight color
            if (API.HasSpotLight(Entity))
            {
                Vec3 redColor = new Vec3(_redColorR, _redColorG, _redColorB);
                API.SetSpotLightColor(Entity, redColor);
            }

            // Play red audio
            if (_playAudio)
            {
                PlayRedAudio();
            }
        }

        /// <summary>
        /// Set spotlight to green and play green audio
        /// </summary>
        private void SetGreenLight()
        {
            // Safety check
            if (Entity == 0) return;

            API.Log("[RedGreenLightSpotlight] Switching to GREEN light");

            // Set spotlight color
            if (API.HasSpotLight(Entity))
            {
                Vec3 greenColor = new Vec3(_greenColorR, _greenColorG, _greenColorB);
                API.SetSpotLightColor(Entity, greenColor);
            }

            // Play green audio
            if (_playAudio)
            {
                PlayGreenAudio();
            }
        }

        /// <summary>
        /// Play red light audio cue
        /// </summary>
        private void PlayRedAudio()
        {
            // Safety check - don't play audio if entity is being destroyed
            if (Entity == 0) return;

            try
            {
                // Update position in case light has moved
                if (API.HasTransform(Entity))
                {
                    _lightPosition = API.GetPosition(Entity);
                }

                // Play the red light sound
                API.PlaySoundAt(_redSoundId, _redLightAudioPath, _lightPosition, false);
                API.Set3DMinMaxDistance(_redSoundId, _audioMinDistance, _audioMaxDistance);
                API.SetSoundVolume(_redSoundId, _audioVolume);

                API.Log($"[RedGreenLightSpotlight] Playing red light audio: {_redLightAudioPath}");
            }
            catch (Exception ex)
            {
                API.Log($"[RedGreenLightSpotlight] ERROR playing red audio: {ex.Message}");
            }
        }

        /// <summary>
        /// Play green light audio cue
        /// </summary>
        private void PlayGreenAudio()
        {
            // Safety check - don't play audio if entity is being destroyed
            if (Entity == 0) return;

            try
            {
                // Update position in case light has moved
                if (API.HasTransform(Entity))
                {
                    _lightPosition = API.GetPosition(Entity);
                }

                // Play the green light sound
                API.PlaySoundAt(_greenSoundId, _greenLightAudioPath, _lightPosition, false);
                API.Set3DMinMaxDistance(_greenSoundId, _audioMinDistance, _audioMaxDistance);
                API.SetSoundVolume(_greenSoundId, _audioVolume);

                API.Log($"[RedGreenLightSpotlight] Playing green light audio: {_greenLightAudioPath}");
            }
            catch (Exception ex)
            {
                API.Log($"[RedGreenLightSpotlight] ERROR playing green audio: {ex.Message}");
            }
        }

        /// <summary>
        /// Start the light cycling
        /// </summary>
        public void StartCycling()
        {
            API.Log("[RedGreenLightSpotlight] Starting light cycling");
            _isRunning = true;
            _timer = 0f;
        }

        /// <summary>
        /// Stop the light cycling
        /// </summary>
        public void StopCycling()
        {
            API.Log("[RedGreenLightSpotlight] Stopping light cycling");
            _isRunning = false;
        }

        /// <summary>
        /// Reset to initial state
        /// </summary>
        public void Reset()
        {
            API.Log("[RedGreenLightSpotlight] Resetting to initial state");
            _isRed = _startWithRed;
            _timer = 0f;

            if (_isRed)
            {
                SetRedLight();
            }
            else
            {
                SetGreenLight();
            }
        }

        /// <summary>
        /// Force set to red light
        /// </summary>
        public void ForceRed()
        {
            _isRed = true;
            SetRedLight();
            _timer = 0f;
        }

        /// <summary>
        /// Force set to green light
        /// </summary>
        public void ForceGreen()
        {
            _isRed = false;
            SetGreenLight();
            _timer = 0f;
        }

        /// <summary>
        /// Get current light state
        /// </summary>
        public bool IsRed()
        {
            return _isRed;
        }

        /// <summary>
        /// Get current light state
        /// </summary>
        public bool IsGreen()
        {
            return !_isRed;
        }

        public void OnDestroy()
        {
            // Stop the cycling immediately to prevent any updates during destruction
            _isRunning = false;

            // Try to stop sounds, but catch any errors during scene transition
            try
            {
                // Stop any playing sounds - this is best effort
                API.StopSound(_redSoundId);
            }
            catch { /* Ignore errors during cleanup */ }

            try
            {
                API.StopSound(_greenSoundId);
            }
            catch { /* Ignore errors during cleanup */ }

            API.Log("[RedGreenLightSpotlight] Destroyed");
        }
    }
}
