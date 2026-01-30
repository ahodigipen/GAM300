using System;
using System.Collections.Generic;
using Boom;

namespace Boom
{
    /// <summary>
    /// Static library mapping surface types to footstep sound files.
    /// Add your audio files here for each surface type.
    /// </summary>
    public static class SurfaceAudioLibrary
    {
        // Base path for footstep sounds
        private static readonly string BasePath = "Resources/Audio/";

        // Surface type -> array of sound file paths
        private static readonly Dictionary<string, string[]> SurfaceSounds = new Dictionary<string, string[]>
        {
            // Default fallback sounds
            { "default", new[] {
                "Resources/Audio/playerWalk_01.wav",
                "Resources/Audio/playerWalk_02.wav",
                "Resources/Audio/playerWalk_03.wav",
                "Resources/Audio/playerWalk_04.wav"
            }},

            // Stone/concrete surfaces
            { "stone", new[] {
                BasePath + "stone_01.wav",
                BasePath + "stone_02.wav",
                BasePath + "stone_03.wav",
                BasePath + "stone_04.wav"
            }},

            // Wood surfaces
            { "wood", new[] {
                BasePath + "wood_01.wav",
                BasePath + "wood_02.wav",
                BasePath + "wood_03.wav",
                BasePath + "wood_04.wav"
            }},

            // Metal surfaces
            { "metal", new[] {
                BasePath + "metal_01.wav",
                BasePath + "metal_02.wav",
                BasePath + "metal_03.wav",
                BasePath + "metal_04.wav"
            }},

            // Tatami (Japanese mat) surfaces
            { "tatami", new[] {
                BasePath + "tatami_01.wav",
                BasePath + "tatami_02.wav",
                BasePath + "tatami_03.wav",
                BasePath + "tatami_04.wav"
            }},

            // Sand surfaces
            { "sand", new[] {
                BasePath + "footstep_sand_1.wav",
                BasePath + "footstep_sand_2.wav",
                BasePath + "footstep_sand_3.wav",
                BasePath + "footstep_sand_4.wav"
                //BasePath + "footstep_sand_5.wav",
                //BasePath + "footstep_sand_6.wav",
                //BasePath + "footstep_sand_7.wav"
            }},

            // Grass surfaces
            { "grass", new[] {
                BasePath + "grass_01.wav",
                BasePath + "grass_02.wav",
                BasePath + "grass_03.wav",
                BasePath + "grass_04.wav"
            }},

            // Dirt surfaces
            { "dirt", new[] {
                BasePath + "dirt_01.wav",
                BasePath + "dirt_02.wav",
                BasePath + "dirt_03.wav",
                BasePath + "dirt_04.wav"
            }},

            // Gravel surfaces
            { "gravel", new[] {
                BasePath + "gravel_01.wav",
                BasePath + "gravel_02.wav",
                BasePath + "gravel_03.wav",
                BasePath + "gravel_04.wav"
            }},

            // Water/puddle surfaces
            { "water", new[] {
                BasePath + "water_01.wav",
                BasePath + "water_02.wav",
                BasePath + "water_03.wav",
                BasePath + "water_04.wav"
            }},

            // Carpet surfaces
            { "carpet", new[] {
                BasePath + "carpet_01.wav",
                BasePath + "carpet_02.wav",
                BasePath + "carpet_03.wav",
                BasePath + "carpet_04.wav"
            }}
        };

        /// <summary>
        /// Get sound files for a surface type. Returns default if not found.
        /// </summary>
        public static string[] GetSoundsForSurface(string surfaceType)
        {
            if (string.IsNullOrEmpty(surfaceType))
                surfaceType = "default";

            // Case-insensitive lookup
            string key = surfaceType.ToLower().Trim();

            if (SurfaceSounds.TryGetValue(key, out string[] sounds))
                return sounds;

            // Fallback to default
            return SurfaceSounds["default"];
        }

        /// <summary>
        /// Get all registered surface types
        /// </summary>
        public static string[] GetAllSurfaceTypes()
        {
            var keys = new string[SurfaceSounds.Count];
            SurfaceSounds.Keys.CopyTo(keys, 0);
            return keys;
        }
    }

    /// <summary>
    /// Component that handles dynamic footstep sounds based on surface type.
    /// Attach this to any entity that should make footstep sounds when moving.
    /// Automatically detects the surface type below the entity and plays appropriate sounds.
    /// </summary>
    public class FootstepComponent
    {
        public ulong Entity;

        // Footstep configuration
        [EditorExposed("Footstep Interval", "Time between footstep sounds in seconds", 0.1f, 2f, true)]
        private float _footstepInterval = 0.5f;

        [EditorExposed("Min Speed", "Minimum movement speed to trigger footsteps", 0.1f, 5f, true)]
        private float _minSpeed = 1.0f;

        [EditorExposed("Footstep Volume", "Volume of footstep sounds", 0f, 1f, true)]
        private float _footstepVolume = 0.95f;

        [EditorExposed("Ground Check Distance", "How far to raycast down for surface detection", 0.1f, 2f, true)]
        private float _groundCheckDistance = 1.0f;

        [EditorExposed("Debug Logging", "Enable debug logging for surface detection")]
        private bool _debugLogging = true;

        // Runtime state
        private float _timeSinceLastFootstep = 0f;
        private Vec3 _lastPosition;
        private bool _isMoving = false;
        private string _currentSurfaceType = "default";
        private string _lastSurfaceType = "default";

        private Random _random = new Random();

        // Cache for preloaded sounds per surface type
        private HashSet<string> _preloadedSurfaces = new HashSet<string>();

        // Debug: timer to log surface type periodically (not every frame)
        private float _debugLogTimer = 0f;
        private const float DEBUG_LOG_INTERVAL = 1.0f; // Log every 1 second while moving

        /// <summary>
        /// Initialize the footstep component
        /// </summary>
        public void OnStart(string jsonParams)
        {
            API.Log($"[FootstepComponent] OnStart() - Entity: {Entity}");

            if (!API.HasTransform(Entity))
            {
                API.Log("[FootstepComponent] ERROR: Entity missing TransformComponent!");
                return;
            }

            _lastPosition = API.GetPosition(Entity);

            // Preload default sounds
            PreloadSurfaceSounds("default");

            API.Log("[FootstepComponent] Dynamic surface footsteps initialized");
        }

        /// <summary>
        /// Preload sounds for a specific surface type
        /// </summary>
        private void PreloadSurfaceSounds(string surfaceType)
        {
            if (_preloadedSurfaces.Contains(surfaceType))
                return;

            string[] sounds = SurfaceAudioLibrary.GetSoundsForSurface(surfaceType);
            for (int i = 0; i < sounds.Length; i++)
            {
                string soundName = $"footstep_{Entity}_{surfaceType}_{i}";
                API.PreloadSound(soundName, sounds[i]);
            }

            _preloadedSurfaces.Add(surfaceType);

            if (_debugLogging)
                API.Log($"[FootstepComponent] Preloaded {sounds.Length} sounds for surface: {surfaceType}");
        }

        /// <summary>
        /// Update footstep logic every frame
        /// </summary>
        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity))
                return;

            // Get current position and calculate movement
            Vec3 currentPosition = API.GetPosition(Entity);
            Vec3 movement = new Vec3(
                currentPosition.X - _lastPosition.X,
                0f, // Ignore Y movement for footsteps
                currentPosition.Z - _lastPosition.Z
            );

            // Calculate horizontal movement speed
            float moveDistance = (float)Math.Sqrt(movement.X * movement.X + movement.Z * movement.Z);
            float speed = moveDistance / dt;

            // Determine if we're moving fast enough to make footsteps
            _isMoving = speed >= _minSpeed;

            if (_isMoving)
            {
                // Detect surface type
                string rawSurfaceType = API.GetGroundSurfaceType(Entity, _groundCheckDistance);
                _currentSurfaceType = string.IsNullOrEmpty(rawSurfaceType) ? "default" : rawSurfaceType;

                // Periodic debug logging of current surface type
                if (_debugLogging)
                {
                    _debugLogTimer += dt;
                    if (_debugLogTimer >= DEBUG_LOG_INTERVAL)
                    {
                        Vec3 pos = API.GetPosition(Entity);
                        API.Log($"[FootstepComponent] Current surface: '{_currentSurfaceType}' at pos ({pos.X:F1}, {pos.Y:F1}, {pos.Z:F1})");
                        _debugLogTimer = 0f;
                    }
                }

                // Preload sounds for new surface if needed
                if (_currentSurfaceType != _lastSurfaceType)
                {
                    PreloadSurfaceSounds(_currentSurfaceType);

                    if (_debugLogging)
                    {
                        API.Log($"[FootstepComponent] Surface CHANGED: '{_lastSurfaceType}' -> '{_currentSurfaceType}'");
                        string[] sounds = SurfaceAudioLibrary.GetSoundsForSurface(_currentSurfaceType);
                        API.Log($"[FootstepComponent] Will use {sounds.Length} sounds, first: {(sounds.Length > 0 ? sounds[0] : "none")}");
                    }

                    _lastSurfaceType = _currentSurfaceType;
                }

                _timeSinceLastFootstep += dt;

                // Adjust footstep timing based on movement speed
                float dynamicInterval = Math.Max(0.2f, _footstepInterval / (speed / 5.0f));

                if (_timeSinceLastFootstep >= dynamicInterval)
                {
                    PlayFootstepSound(currentPosition);
                    _timeSinceLastFootstep = 0f;
                }
            }
            else
            {
                // Reset timer when not moving
                _timeSinceLastFootstep = 0f;
            }

            _lastPosition = currentPosition;
        }

        /// <summary>
        /// Play a random footstep sound for the current surface at the entity's position
        /// </summary>
        private void PlayFootstepSound(Vec3 position)
        {
            string[] sounds = SurfaceAudioLibrary.GetSoundsForSurface(_currentSurfaceType);

            // Choose random footstep sound
            int soundIndex = _random.Next(sounds.Length);
            string soundName = $"footstep_{Entity}_current";

            // Stop any previous footstep from this entity
            API.StopSound(soundName);

            // Play the footstep sound at the entity's position (3D sound)
            API.PlaySoundAt(soundName, sounds[soundIndex], position, false);
            API.SetSoundVolume(soundName, _footstepVolume);

            if (_debugLogging)
                API.Log($"[FootstepComponent] Played {_currentSurfaceType} footstep at ({position.X:F1}, {position.Y:F1}, {position.Z:F1})");
        }

        /// <summary>
        /// Cleanup when component is destroyed
        /// </summary>
        public void OnDestroy()
        {
            API.Log($"[FootstepComponent] OnDestroy() - Entity: {Entity}");

            // Stop any playing footstep sounds
            API.StopSound($"footstep_{Entity}_current");
        }

        // Public methods for customization

        /// <summary>
        /// Get the current detected surface type
        /// </summary>
        public string GetCurrentSurfaceType()
        {
            return _currentSurfaceType;
        }

        /// <summary>
        /// Set the time interval between footsteps (in seconds)
        /// </summary>
        public void SetFootstepInterval(float interval)
        {
            _footstepInterval = Math.Max(0.1f, interval);
        }

        /// <summary>
        /// Set the minimum movement speed required to trigger footsteps
        /// </summary>
        public void SetMinimumSpeed(float speed)
        {
            _minSpeed = Math.Max(0.1f, speed);
        }

        /// <summary>
        /// Set the volume of footstep sounds (0.0 - 1.0)
        /// </summary>
        public void SetFootstepVolume(float volume)
        {
            _footstepVolume = Math.Max(0f, Math.Min(1f, volume));
        }

        /// <summary>
        /// Check if the entity is currently moving
        /// </summary>
        public bool IsMoving()
        {
            return _isMoving;
        }
    }
}
