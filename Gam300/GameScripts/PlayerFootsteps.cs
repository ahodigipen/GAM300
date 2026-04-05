using System;
using System.Collections.Generic;
using Boom;

namespace Boom
{
    /// <summary>
    /// Component that handles footstep sounds for moving entities.
    /// Attach this to any entity that should make footstep sounds when moving.
    /// Supports surface-type based sound selection using trigger colliders.
    /// </summary>
    public class FootstepComponent
    {
        public ulong Entity;

        // Footstep configuration
        [EditorExposed("Footstep Interval", "Time between footstep sounds in seconds", 0.1f, 2f, true)]
        private float _footstepInterval = 0.4f;

        [EditorExposed("Min Speed", "Minimum movement speed to trigger footsteps", 0.1f, 5f, true)]
        private float _minSpeed = 1.0f;

        [EditorExposed("Footstep Volume", "Volume of footstep sounds", 0f, 1f, true)]
        private float _footstepVolume = 0.35f;

        // Runtime state
        private float _timeSinceLastFootstep = 0f;
        private Vec3 _lastPosition;
        private bool _isMoving = false;
        private API.SurfaceType _currentSurface = API.SurfaceType.DEFAULT;
        private float _debugSurfaceTimer = 0f;

        // Track surface triggers we're overlapping (using callback system)
        private HashSet<ulong> _activeSurfaceTriggers = new HashSet<ulong>();
        private static FootstepComponent s_instance = null;

        // Default footstep sounds (used for DEFAULT surface and as fallback)
        [EditorExposed("Default Sound 1", "Default footstep sound variant 1")]
        private string _defaultSound1 = "Resources/Audio/playerWalk_01.wav";
        [EditorExposed("Default Sound 2", "Default footstep sound variant 2")]
        private string _defaultSound2 = "Resources/Audio/playerWalk_02.wav";
        [EditorExposed("Default Sound 3", "Default footstep sound variant 3")]
        private string _defaultSound3 = "Resources/Audio/playerWalk_03.wav";
        [EditorExposed("Default Sound 4", "Default footstep sound variant 4")]
        private string _defaultSound4 = "Resources/Audio/playerWalk_04.wav";

        // Wood surface sounds
        [EditorExposed("Wood Sound 1", "Wood footstep sound variant 1")]
        private string _woodSound1 = "Resources/Audio/footstep_tatami_1.wav";
        [EditorExposed("Wood Sound 2", "Wood footstep sound variant 2")]
        private string _woodSound2 = "Resources/Audio/footstep_tatami_2.wav";
        [EditorExposed("Wood Sound 3", "Wood footstep sound variant 3")]
        private string _woodSound3 = "Resources/Audio/footstep_tatami_3.wav";
        [EditorExposed("Wood Sound 4", "Wood footstep sound variant 4")]
        private string _woodSound4 = "Resources/Audio/footstep_tatami_4.wav";
        [EditorExposed("Wood Sound 5", "Wood footstep sound variant 5")]
        private string _woodSound5 = "Resources/Audio/footstep_tatami_5.wav";
        [EditorExposed("Wood Sound 6", "Wood footstep sound variant 6")]
        private string _woodSound6 = "Resources/Audio/footstep_tatami_6.wav";
        [EditorExposed("Wood Sound 7", "Wood footstep sound variant 7")]
        private string _woodSound7 = "Resources/Audio/footstep_tatami_7.wav";
        [EditorExposed("Wood Sound 8", "Wood footstep sound variant 8")]
        private string _woodSound8 = "Resources/Audio/footstep_tatami_8.wav";
        [EditorExposed("Wood Sound 9", "Wood footstep sound variant 9")]
        private string _woodSound9 = "Resources/Audio/footstep_tatami_9.wav";
        [EditorExposed("Wood Sound 10", "Wood footstep sound variant 10")]
        private string _woodSound10 = "Resources/Audio/footstep_tatami_10.wav";
        [EditorExposed("Wood Sound 11", "Wood footstep sound variant 11")]
        private string _woodSound11 = "Resources/Audio/footstep_tatami_11.wav";
        [EditorExposed("Wood Sound 12", "Wood footstep sound variant 12")]
        private string _woodSound12 = "Resources/Audio/footstep_tatami_12.wav";
        [EditorExposed("Wood Sound 13", "Wood footstep sound variant 13")]
        private string _woodSound13 = "Resources/Audio/footstep_tatami_13.wav";
        [EditorExposed("Wood Sound 14", "Wood footstep sound variant 14")]
        private string _woodSound14 = "Resources/Audio/footstep_tatami_14.wav";


        // Sand surface sounds (fill with sand audio files)
        [EditorExposed("Sand Sound 1", "Sand footstep sound variant 1")]
        private string _sandSound1 = "Resources/Audio/footstep_sand_1.wav";
        [EditorExposed("Sand Sound 2", "Sand footstep sound variant 2")]
        private string _sandSound2 = "Resources/Audio/footstep_sand_2.wav";
        [EditorExposed("Sand Sound 3", "Sand footstep sound variant 3")]
        private string _sandSound3 = "Resources/Audio/footstep_sand_3.wav";
        [EditorExposed("Sand Sound 4", "Sand footstep sound variant 4")]
        private string _sandSound4 = "Resources/Audio/footstep_sand_4.wav";
        [EditorExposed("Sand Sound 5", "Sand footstep sound variant 5")]
        private string _sandSound5 = "Resources/Audio/footstep_sand_5.wav";
        [EditorExposed("Sand Sound 6", "Sand footstep sound variant 6")]
        private string _sandSound6 = "Resources/Audio/footstep_sand_6.wav";
        [EditorExposed("Sand Sound 7", "Sand footstep sound variant 7")]
        private string _sandSound7 = "Resources/Audio/footstep_sand_7.wav";
        [EditorExposed("Sand Sound 8", "Sand footstep sound variant 8")]
        private string _sandSound8 = "Resources/Audio/footstep_sand_8.wav";
        [EditorExposed("Sand Sound 9", "Sand footstep sound variant 9")]
        private string _sandSound9 = "Resources/Audio/footstep_sand_9.wav";
        [EditorExposed("Sand Sound 10", "Sand footstep sound variant 10")]
        private string _sandSound10 = "Resources/Audio/footstep_sand_10.wav";
        [EditorExposed("Sand Sound 11", "Sand footstep sound variant 11")]
        private string _sandSound11 = "Resources/Audio/footstep_sand_11.wav";

        // Stone surface sounds (fill with  stone audio files)
        [EditorExposed("Stone Sound 1", "Stone footstep sound variant 1")]
        private string _stoneSound1 = "Resources/Audio/footstep_stone_1.wav";
        [EditorExposed("Stone Sound 2", "Stone footstep sound variant 2")]
        private string _stoneSound2 = "Resources/Audio/footstep_stone_2.wav";
        [EditorExposed("Stone Sound 3", "Stone footstep sound variant 3")]
        private string _stoneSound3 = "Resources/Audio/footstep_stone_3.wav";
        [EditorExposed("Stone Sound 4", "Stone footstep sound variant 4")]
        private string _stoneSound4 = "Resources/Audio/footstep_stone_4.wav";
        [EditorExposed("Stone Sound 5", "Stone footstep sound variant 5")]
        private string _stoneSound5 = "Resources/Audio/footstep_stone_5.wav";
        [EditorExposed("Stone Sound 6", "Stone footstep sound variant 6")]
        private string _stoneSound6 = "Resources/Audio/footstep_stone_6.wav";
        [EditorExposed("Stone Sound 7", "Stone footstep sound variant 7")]
        private string _stoneSound7 = "Resources/Audio/footstep_stone_7.wav";
        [EditorExposed("Stone Sound 8", "Stone footstep sound variant 8")]
        private string _stoneSound8 = "Resources/Audio/footstep_stone_8.wav";
        [EditorExposed("Stone Sound 9", "Stone footstep sound variant 9")]
        private string _stoneSound9 = "Resources/Audio/footstep_stone_9.wav";
        [EditorExposed("Stone Sound 10", "Stone footstep sound variant 10")]
        private string _stoneSound10 = "Resources/Audio/footstep_stone_10.wav";
        // Metal surface sounds (fill with your metal audio files)
        [EditorExposed("Metal Sound 1", "Metal footstep sound variant 1")]
        private string _metalSound1 = "Resources/Audio/playerWalk_01.wav";
        [EditorExposed("Metal Sound 2", "Metal footstep sound variant 2")]
        private string _metalSound2 = "Resources/Audio/playerWalk_02.wav";
        [EditorExposed("Metal Sound 3", "Metal footstep sound variant 3")]
        private string _metalSound3 = "Resources/Audio/playerWalk_03.wav";
        [EditorExposed("Metal Sound 4", "Metal footstep sound variant 4")]
        private string _metalSound4 = "Resources/Audio/playerWalk_04.wav";

        // Grass surface sounds (fill with your grass audio files)
        [EditorExposed("Grass Sound 1", "Grass footstep sound variant 1")]
        private string _grassSound1 = "Resources/Audio/playerWalk_01.wav";
        [EditorExposed("Grass Sound 2", "Grass footstep sound variant 2")]
        private string _grassSound2 = "Resources/Audio/playerWalk_02.wav";
        [EditorExposed("Grass Sound 3", "Grass footstep sound variant 3")]
        private string _grassSound3 = "Resources/Audio/playerWalk_03.wav";
        [EditorExposed("Grass Sound 4", "Grass footstep sound variant 4")]
        private string _grassSound4 = "Resources/Audio/playerWalk_04.wav";

        private Random _random = new Random();

        /// <summary>
        /// Get sounds array for a specific surface type
        /// </summary>
        private string[] GetSoundsForSurface(API.SurfaceType surface)
        {
            switch (surface)
            {
                case API.SurfaceType.WOOD:
                    return new string[] { _woodSound1, _woodSound2, _woodSound3, _woodSound4, _woodSound5, _woodSound6, _woodSound7, _woodSound8, _woodSound9, _woodSound10, _woodSound11, _woodSound12, _woodSound13, _woodSound14};
                case API.SurfaceType.SAND:
                    return new string[] { _sandSound1, _sandSound2, _sandSound3, _sandSound4, _sandSound5, _sandSound6, _sandSound7, _sandSound8, _sandSound9, _sandSound10, _sandSound11 };
                case API.SurfaceType.STONE:
                    return new string[] { _stoneSound1, _stoneSound2, _stoneSound3, _stoneSound4, _stoneSound5, _stoneSound6, _stoneSound7, _stoneSound8, _stoneSound9, _stoneSound10 };
                case API.SurfaceType.METAL:
                    return new string[] { _metalSound1, _metalSound2, _metalSound3, _metalSound4 };
                case API.SurfaceType.GRASS:
                    return new string[] { _grassSound1, _grassSound2, _grassSound3, _grassSound4 };
                case API.SurfaceType.WATER:
                case API.SurfaceType.CARPET:
                case API.SurfaceType.TILE:
                default:
                    return new string[] { _defaultSound1, _defaultSound2, _defaultSound3, _defaultSound4 };
            }
        }

        /// <summary>
        /// Initialize the footstep component
        /// </summary>
        public void OnStart(string jsonParams)
        {
            API.Log($"[FootstepComponent] OnStart() - Entity: {Entity}");
            s_instance = this;

            if (!API.HasTransform(Entity))
            {
                API.Log("[FootstepComponent] ERROR: Entity missing TransformComponent!");
                return;
            }

            _lastPosition = API.GetPosition(Entity);

            // Preload all sounds for all surface types
            PreloadSurfaceSounds(API.SurfaceType.DEFAULT);
            PreloadSurfaceSounds(API.SurfaceType.WOOD);
            PreloadSurfaceSounds(API.SurfaceType.SAND);
            PreloadSurfaceSounds(API.SurfaceType.STONE);
            PreloadSurfaceSounds(API.SurfaceType.METAL);
            PreloadSurfaceSounds(API.SurfaceType.GRASS);

            API.Log("[FootstepComponent] Footstep sounds preloaded for all surfaces");

            // Register for surface trigger callbacks - scan for all triggers with non-DEFAULT surface type
            RegisterSurfaceTriggers();

            // Check if we're already inside any surface triggers at startup
            // (PhysX won't fire ENTER if we spawn inside a trigger)
            CheckInitialOverlaps();
        }

        /// <summary>
        /// Check if player is already inside any surface triggers at startup
        /// </summary>
        private void CheckInitialOverlaps()
        {
            ulong[] overlaps = API.GetControllerTriggerOverlaps(Entity);
            if (overlaps != null && overlaps.Length > 0)
            {
                API.Log($"[Footstep] Initial overlap check found {overlaps.Length} triggers");
                foreach (ulong triggerId in overlaps)
                {
                    API.SurfaceType surface = API.GetSurfaceType(triggerId);
                    API.Log($"[Footstep]   Already inside trigger {triggerId}, surface: {surface}");
                    if (surface != API.SurfaceType.DEFAULT)
                    {
                        _activeSurfaceTriggers.Add(triggerId);
                    }
                }
                UpdateCurrentSurface();
            }
            else
            {
                API.Log("[Footstep] No initial trigger overlaps");
            }
        }

        /// <summary>
        /// Find and register callbacks for all triggers that have a surface type set
        /// </summary>
        private void RegisterSurfaceTriggers()
        {
            // We need to find all trigger entities. Since we can't enumerate all entities easily,
            // we'll look for common naming patterns. You can extend this list.
            string[] triggerPrefixes = {
                "SurfaceTrigger", "Surface_", "Ground_", "Floor_",
                "TestTrigger", "Trigger_", "SandTrigger", "WoodTrigger",
                "StoneTrigger", "MetalTrigger", "GrassTrigger","Tatami_Level1","Mini_Tatami_Level1","Stone_Stairs_Left","Stone_Stairs_Right","Sand_Floor_Level2"
            };

            for (int i = 0; i < 50; i++) // Check up to 50 of each type
            {
                foreach (string prefix in triggerPrefixes)
                {
                    string name = i == 0 ? prefix : $"{prefix}{i}";
                    TryRegisterSurfaceTrigger(name);
                    TryRegisterSurfaceTrigger($"{prefix}_{i}");
                }
            }

            // Also try some specific names that might be used
            //TryRegisterSurfaceTrigger("TestTriggerSAnd");
            //TryRegisterSurfaceTrigger("testsandtrigger");

            API.Log($"[FootstepComponent] Registered {_activeSurfaceTriggers.Count} potential surface triggers");
        }

        private void TryRegisterSurfaceTrigger(string name)
        {
            ulong id = API.FindEntity(name);
            if (id != 0 && API.HasCollider(id) && API.IsTrigger(id))
            {
                API.SurfaceType surface = API.GetSurfaceType(id);
                API.Log($"[FootstepComponent] Found surface trigger: {name} (ID: {id}, Surface: {surface})");
                API.RegisterTriggerEnterCallback(id, OnSurfaceTriggerEnter);
                API.RegisterTriggerExitCallback(id, OnSurfaceTriggerExit);
            }
        }

        private static void OnSurfaceTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            API.Log($"[Footstep] ENTER callback - trigger={triggerEntity}, other={otherEntity}, player={s_instance?.Entity}");
            if (s_instance == null) return;
            if (otherEntity != s_instance.Entity) return;

            API.SurfaceType surface = API.GetSurfaceType(triggerEntity);
            API.Log($"[Footstep] ENTERED surface trigger {triggerEntity}, surface type: {surface}");

            s_instance._activeSurfaceTriggers.Add(triggerEntity);
            s_instance.UpdateCurrentSurface();
        }

        private static void OnSurfaceTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            API.Log($"[Footstep] EXIT callback - trigger={triggerEntity}, other={otherEntity}, player={s_instance?.Entity}");
            if (s_instance == null) return;
            if (otherEntity != s_instance.Entity) return;

            API.Log($"[Footstep] Removing trigger {triggerEntity} from active set");
            s_instance._activeSurfaceTriggers.Remove(triggerEntity);
            s_instance.UpdateCurrentSurface();
        }

        private void UpdateCurrentSurface()
        {
            // Find the highest priority non-DEFAULT surface from active triggers
            API.SurfaceType newSurface = API.SurfaceType.DEFAULT;

            API.Log($"[Footstep] UpdateCurrentSurface - {_activeSurfaceTriggers.Count} active triggers");

            foreach (ulong triggerId in _activeSurfaceTriggers)
            {
                API.SurfaceType surface = API.GetSurfaceType(triggerId);
                API.Log($"[Footstep]   Trigger {triggerId} -> surface {surface}");
                if (surface != API.SurfaceType.DEFAULT)
                {
                    newSurface = surface;
                    break; // Use first non-default found
                }
            }

            API.Log($"[Footstep] Decision: {newSurface} (was {_currentSurface})");
            if (newSurface != _currentSurface)
            {
                API.Log($"[Footstep] Surface CHANGED: {_currentSurface} -> {newSurface}");
                _currentSurface = newSurface;
            }
        }

        private void PreloadSurfaceSounds(API.SurfaceType surface)
        {
            string[] sounds = GetSoundsForSurface(surface);
            for (int i = 0; i < sounds.Length; i++)
            {
                string soundName = $"footstep_{Entity}_{surface}_{i}";
                API.PreloadSound(soundName, sounds[i]);
            }
        }

        /// <summary>
        /// Detect surface type directly beneath the entity using engine ground probe
        /// </summary>
        private void DetectSurfaceUnderFeet()
        {
            // IF we have trigger overlaps, they take priority
            if (_activeSurfaceTriggers.Count > 0)
            {
                UpdateCurrentSurface();
                return;
            }

            // Reuse the engine's built-in grounded check data to find what we're standing on
            ulong floorEntity = API.GetStandingOnEntity(Entity);
            
            if (floorEntity != 0)
            {
                API.SurfaceType surface = API.GetSurfaceType(floorEntity);
                if (surface != _currentSurface)
                {
                    //API.Log($"[Footstep] Floor probe detected new surface: {surface} on entity {floorEntity}");
                    _currentSurface = surface;
                }
            }
            else
            {
                // If nothing under feet, fallback to default
                if (_currentSurface != API.SurfaceType.DEFAULT && _activeSurfaceTriggers.Count == 0)
                {
                    _currentSurface = API.SurfaceType.DEFAULT;
                }
            }
        }

        /// <summary>
        /// Update footstep logic every frame
        /// </summary>
        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity))
                return;

            // Decrement debug timer
            if (_debugSurfaceTimer > 0f)
                _debugSurfaceTimer -= dt;

            // Get current position and calculate movement
            Vec3 currentPosition = API.GetPosition(Entity);
            Vec3 movement = new Vec3(
                currentPosition.X - _lastPosition.X,
                0f,
                currentPosition.Z - _lastPosition.Z
            );

            // Calculate horizontal movement speed
            float moveDistance = (float)Math.Sqrt(movement.X * movement.X + movement.Z * movement.Z);
            float speed = moveDistance / dt;

            // Determine if we're moving fast enough to make footsteps
            _isMoving = speed >= _minSpeed;

            if (_isMoving)
            {
                _timeSinceLastFootstep += dt;

                // Adjust footstep timing based on movement speed
                float dynamicInterval = Math.Max(0.2f, _footstepInterval / (speed / 5.0f));

                if (_timeSinceLastFootstep >= dynamicInterval)
                {
                    // --- OPTIMIZED: Detect surface only when sound is about to play ---
                    DetectSurfaceUnderFeet();

                    PlayFootstepSound(currentPosition);
                    _timeSinceLastFootstep = 0f;
                }
            }
            else
            {
                _timeSinceLastFootstep = 0f;
            }

            _lastPosition = currentPosition;
        }

        /// <summary>
        /// Detect current surface type using controller trigger overlaps
        /// </summary>
        private void DetectCurrentSurface()
        {
            // Get all trigger entities the controller is overlapping
            ulong[] overlaps = API.GetControllerTriggerOverlaps(Entity);

            if (overlaps == null || overlaps.Length == 0)
            {
                // No trigger overlaps - use DEFAULT surface
                if (_currentSurface != API.SurfaceType.DEFAULT)
                {
                    API.Log($"[Footstep] No overlaps, switching from {_currentSurface} to DEFAULT");
                }
                _currentSurface = API.SurfaceType.DEFAULT;
                return;
            }

            // Debug: Log all overlaps found
            if (_debugSurfaceTimer <= 0f)
            {
                API.Log($"[Footstep] Found {overlaps.Length} trigger overlaps");
                foreach (ulong triggerEntity in overlaps)
                {
                    API.SurfaceType surface = API.GetSurfaceType(triggerEntity);
                    API.Log($"[Footstep]   - Entity {triggerEntity}: SurfaceType = {surface}");
                }
                _debugSurfaceTimer = 2.0f; // Only log every 2 seconds
            }

            // Check each overlapping trigger for surface type
            // Use the first non-DEFAULT surface found (you could also prioritize differently)
            foreach (ulong triggerEntity in overlaps)
            {
                API.SurfaceType surface = API.GetSurfaceType(triggerEntity);
                if (surface != API.SurfaceType.DEFAULT)
                {
                    if (_currentSurface != surface)
                    {
                        API.Log($"[Footstep] Surface changed: {_currentSurface} -> {surface}");
                    }
                    _currentSurface = surface;
                    return;
                }
            }

            // All overlapping triggers are DEFAULT
            _currentSurface = API.SurfaceType.DEFAULT;
        }

        /// <summary>
        /// Play a random footstep sound at the entity's position based on current surface
        /// </summary>
        private void PlayFootstepSound(Vec3 position)
        {
            string[] sounds = GetSoundsForSurface(_currentSurface);
            int soundIndex = _random.Next(sounds.Length);

            // IMPORTANT: Include the file path in the sound name to avoid FMOD caching issues.
            // FMOD caches sounds by name, so using the same name with different file paths
            // will cause it to play the first cached sound instead of the new one.
            string soundFile = sounds[soundIndex];
            string soundName = $"footstep_{Entity}_{_currentSurface}_{soundIndex}";

            // Debug: Log which surface sound is playing
            API.Log($"[Footstep] Playing: {_currentSurface} -> {soundFile} (name: {soundName})");

            // Stop any previous footstep from this entity (stop all surface variants)
            StopAllFootstepVariants();

            // Play the footstep sound at the entity's position (3D sound)
            API.PlaySound(soundName, soundFile, false);
            API.SetSoundVolume(soundName, _footstepVolume);
        }

        /// <summary>
        /// Stop all footstep sound variants for this entity
        /// </summary>
        private void StopAllFootstepVariants()
        {
            // Stop sounds for all surface types to ensure clean switching
            foreach (API.SurfaceType surface in Enum.GetValues(typeof(API.SurfaceType)))
            {
                string[] sounds = GetSoundsForSurface(surface);
                for (int i = 0; i < sounds.Length; i++)
                {
                    string soundName = $"footstep_{Entity}_{surface}_{i}";
                    API.StopSound(soundName);
                }
            }
        }

        /// <summary>
        /// Cleanup when component is destroyed
        /// </summary>
        public void OnDestroy()
        {
            API.Log($"[FootstepComponent] OnDestroy() - Entity: {Entity}");
            StopAllFootstepVariants();
            _activeSurfaceTriggers.Clear();
            if (s_instance == this)
                s_instance = null;
        }

        // Public methods for customization

        public void SetFootstepInterval(float interval)
        {
            _footstepInterval = Math.Max(0.1f, interval);
        }

        public void SetMinimumSpeed(float speed)
        {
            _minSpeed = Math.Max(0.1f, speed);
        }

        public void SetFootstepVolume(float volume)
        {
            _footstepVolume = Math.Max(0f, Math.Min(1f, volume));
        }

        public bool IsMoving()
        {
            return _isMoving;
        }

        /// <summary>
        /// Get the current surface type being walked on
        /// </summary>
        public API.SurfaceType GetCurrentSurface()
        {
            return _currentSurface;
        }

        /// <summary>
        /// Manually set the surface type (useful for testing or overriding detection)
        /// </summary>
        public void SetCurrentSurface(API.SurfaceType surface)
        {
            _currentSurface = surface;
        }
    }
}
