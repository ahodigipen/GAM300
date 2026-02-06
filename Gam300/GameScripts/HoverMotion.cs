using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Makes an object hover up and down with optional rotation.
    /// No rigidbody required - directly manipulates the transform.
    /// </summary>
    public class HoverMotion
    {
        public ulong Entity;

        // Hovering parameters
        [EditorExposed("Hover Height", "The vertical distance the object moves up and down", 0.0f, 10.0f, false)]
        private float _hoverHeight = 0.5f;

        [EditorExposed("Hover Speed", "How fast the object hovers (higher = faster)", 0.0f, 10.0f, false)]
        private float _hoverSpeed = 1.0f;

        [EditorExposed("Hover Offset", "Starting offset in the hover cycle (0-1)", 0.0f, 1.0f, true)]
        private float _hoverOffset = 0.0f;

        // Rotation parameters
        [EditorExposed("Enable Rotation", "Whether the object should rotate")]
        private bool _enableRotation = false;

        [EditorExposed("Rotation Speed X", "Rotation speed around X axis (degrees/sec)", -360.0f, 360.0f, false)]
        private float _rotationSpeedX = 0.0f;

        [EditorExposed("Rotation Speed Y", "Rotation speed around Y axis (degrees/sec)", -360.0f, 360.0f, false)]
        private float _rotationSpeedY = 30.0f;

        [EditorExposed("Rotation Speed Z", "Rotation speed around Z axis (degrees/sec)", -360.0f, 360.0f, false)]
        private float _rotationSpeedZ = 0.0f;

        // Internal state
        private Vec3 _startPosition;
        private Vec3 _startRotation;
        private float _time = 0.0f;
        private bool _initialized = false;

        public void OnStart(string _)
        {
            if (!API.HasTransform(Entity))
            {
                API.Log($"[HoverMotion] Entity {Entity} has no transform!");
                return;
            }

            // Store initial position and rotation
            _startPosition = API.GetPosition(Entity);
            _startRotation = API.GetRotation(Entity);

            // Apply hover offset to starting time
            _time = _hoverOffset * 2.0f * (float)Math.PI;

            _initialized = true;
        }

        public void OnUpdate(float dt)
        {
            if (!_initialized || !API.HasTransform(Entity))
                return;

            // Update time
            _time += dt * _hoverSpeed;

            // Calculate hover offset using sine wave
            float hoverY = (float)Math.Sin(_time) * _hoverHeight;

            // Apply hovering motion
            Vec3 newPosition = new Vec3(
                _startPosition.X,
                _startPosition.Y + hoverY,
                _startPosition.Z
            );
            API.SetPosition(Entity, newPosition);

            // Apply rotation if enabled
            if (_enableRotation)
            {
                Vec3 currentRotation = API.GetRotation(Entity);

                Vec3 newRotation = new Vec3(
                    currentRotation.X + _rotationSpeedX * dt,
                    currentRotation.Y + _rotationSpeedY * dt,
                    currentRotation.Z + _rotationSpeedZ * dt
                );

                // Normalize rotation to prevent overflow (keep between -360 and 360)
                newRotation.X = NormalizeAngle(newRotation.X);
                newRotation.Y = NormalizeAngle(newRotation.Y);
                newRotation.Z = NormalizeAngle(newRotation.Z);

                API.SetRotation(Entity, newRotation);
            }
        }

        public void OnDestroy()
        {
            // Optional: Reset to original position/rotation when destroyed
            if (_initialized && API.HasTransform(Entity))
            {
                API.SetPosition(Entity, _startPosition);
                API.SetRotation(Entity, _startRotation);
            }
        }

        /// <summary>
        /// Normalize an angle to be within -360 to 360 degrees
        /// </summary>
        private float NormalizeAngle(float angle)
        {
            while (angle > 360.0f)
                angle -= 360.0f;
            while (angle < -360.0f)
                angle += 360.0f;
            return angle;
        }
    }
}
