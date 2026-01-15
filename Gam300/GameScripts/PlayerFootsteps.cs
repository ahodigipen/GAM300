using System;
using Boom;

namespace Boom
{
    /// <summary>
    /// Component that handles footstep sounds for moving entities.
    /// Attach this to any entity that should make footstep sounds when moving.
    /// </summary>
    public class FootstepComponent
    {
        public ulong Entity;

        // Footstep configuration
        [EditorExposed("Footstep Interval", "Time between footstep sounds in seconds", 0.1f, 2f, true)]
        private float _footstepInterval = 0.5f; // Time between footsteps

        [EditorExposed("Min Speed", "Minimum movement speed to trigger footsteps", 0.1f, 5f, true)]
        private float _minSpeed = 1.0f; // Minimum movement speed to trigger footsteps

        [EditorExposed("Footstep Volume", "Volume of footstep sounds", 0f, 1f, true)]
        private float _footstepVolume = 0.95f;

        // Runtime state
        private float _timeSinceLastFootstep = 0f;
        private Vec3 _lastPosition;
        private bool _isMoving = false;

        // Sound files (customize these paths for your project)
        [EditorExposed("Footstep Sound 1", "First footstep sound variant")]
        private string _footstepSound1 = "Resources/Audio/playerWalk_01.wav";

        [EditorExposed("Footstep Sound 2", "Second footstep sound variant")]
        private string _footstepSound2 = "Resources/Audio/playerWalk_02.wav";

        [EditorExposed("Footstep Sound 3", "Third footstep sound variant")]
        private string _footstepSound3 = "Resources/Audio/playerWalk_03.wav";

        [EditorExposed("Footstep Sound 4", "Fourth footstep sound variant")]
        private string _footstepSound4 = "Resources/Audio/playerWalk_04.wav";

        // Helper to get footstep sounds as array
        private string[] GetFootstepSounds() => new string[] { _footstepSound1, _footstepSound2, _footstepSound3, _footstepSound4 };

        private Random _random = new Random();

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

            // Preload all footstep sounds for better performance
            for (int i = 0; i < GetFootstepSounds().Length; i++)
            {
                string soundName = $"footstep_{Entity}_{i}";
                API.PreloadSound(soundName, GetFootstepSounds()[i]);
            }

            API.Log("[FootstepComponent] Footstep sounds preloaded successfully");
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
        /// Play a random footstep sound at the entity's position
        /// </summary>
        private void PlayFootstepSound(Vec3 position)
        {
            // Choose random footstep sound
            int soundIndex = _random.Next(GetFootstepSounds().Length);
            string soundName = $"footstep_{Entity}_current";

            // Stop any previous footstep from this entity
            API.StopSound(soundName);

            // Play the footstep sound at the entity's position (3D sound)
            API.PlaySoundAt(soundName, GetFootstepSounds()[soundIndex], position, false);
            API.SetSoundVolume(soundName, _footstepVolume);
            // Note: No Set3DMinMaxDistance needed - player should always hear their own footsteps clearly
            // since the camera/listener follows right behind the player

            API.Log($"[FootstepComponent] Played footstep {soundIndex + 1} at position ({position.X:F1}, {position.Y:F1}, {position.Z:F1})");
        }

        /// <summary>
        /// Cleanup when component is destroyed
        /// </summary>
        public void OnDestroy()
        {
            API.Log($"[FootstepComponent] OnDestroy() - Entity: {Entity}");

            // Stop any playing footstep sounds
            API.StopSound($"footstep_{Entity}_current");

            // Note: Preloaded sounds will be cleaned up by the sound engine
        }

        // Public methods for customization

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