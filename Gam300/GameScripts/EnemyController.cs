using System;
using Boom;

namespace GameScripts
{
    public class EnemyController
    {
        public ulong Entity;

        // Rotation parameters
        private float _rotationTimer = 0f;
        private float _rotationInterval = 2f;
        private float _currentYRotation = 0f;

        // Vision system
        private VisionComponent _vision;

        public void OnStart(string jsonParams)
        {
            API.Log($"[EnemyController] OnStart() - Entity: {Entity}");

            if (!API.HasTransform(Entity))
            {
                API.Log("[EnemyController] ERROR: Entity missing TransformComponent!");
                return;
            }

            // Initialize vision system
            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(jsonParams);

            _currentYRotation = 0f;
            API.Log("[EnemyController] Controller initialized with vision system");
            _currentYRotation = API.GetRotation(Entity).Y;

            _vision.EnableDebugReasons(true);  // See why targets are rejected
            _vision.EnableDebugLOS(true);      // See LOS results
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Update vision system
            _vision?.OnUpdate(dt);

            //Handle rotation(only when not alert)
            if (_vision?.GetState() != VisionComponent.VisionState.Alert)
            {
                UpdateRotation(dt);
            }
        }

        private void UpdateRotation(float dt)
        {
            var rot = API.GetRotation(Entity);
            _rotationTimer += dt;

            if (_rotationTimer >= _rotationInterval)
            {
                _rotationTimer = 0f;
                _currentYRotation += 90f;

                if (_currentYRotation >= 360f)
                    _currentYRotation -= 360f;

                rot.Y = _currentYRotation;
                API.SetRotation(Entity, rot);
            }
        }

        // === VISION EVENT HANDLERS ===
        private void OnPlayerDetected(ulong target, Vec3 position)
        {
            API.Log(">>> ENEMY ALERTED! STOPPING PATROL! <<<");
            // TODO: Start chase behavior, play alert sound, etc.
        }

        private void OnPlayerLost(ulong target, Vec3 lastKnownPosition)
        {
            API.Log("[EnemyController] Lost sight of player, searching...");
            // TODO: Search behavior, investigate last known position
        }

        private void OnPlayerTracking(ulong target, Vec3 position)
        {
            // TODO: Update aim direction, maintain line of sight
        }

        public void OnDestroy()
        {
            _vision?.OnDestroy();
            API.Log($"[EnemyController] OnDestroy() - Entity: {Entity}");
        }
    }
}