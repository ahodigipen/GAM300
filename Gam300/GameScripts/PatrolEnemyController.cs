using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Enemy controller that patrols between waypoints without rotating.
    /// Vision is always forward-facing in the entity's initial forward direction.
    /// </summary>
    public class PatrolEnemyController
    {
        public ulong Entity;

        // Patrol parameters
        private Vec3[] _waypoints;
        private int _currentWaypointIndex = 0;
        private float _patrolSpeed = 2f;
        private float _waypointReachedDistance = 0.5f;

        // Vision system (always forward-facing)
        private VisionComponent _vision;
        private float _initialForwardYaw = 0f; // Store initial forward direction

        // Movement state
        private bool _isPatrolling = true;
        private Vec3 _alertPosition;

        public void OnStart(string jsonParams)
        {
            API.Log($"[PatrolEnemyController] OnStart() - Entity: {Entity}");

            if (!API.HasTransform(Entity))
            {
                API.Log("[PatrolEnemyController] ERROR: Entity missing TransformComponent!");
                return;
            }

            // Store initial forward direction (Y rotation)
            _initialForwardYaw = API.GetRotation(Entity).Y;
            API.Log($"[PatrolEnemyController] Initial forward direction: {_initialForwardYaw} degrees");

            // Initialize vision system
            _vision = new VisionComponent { Entity = Entity };
            _vision.OnTargetDetected += OnPlayerDetected;
            _vision.OnTargetLost += OnPlayerLost;
            _vision.OnTargetUpdated += OnPlayerTracking;
            _vision.OnStart(jsonParams);

            // Set up default patrol waypoints (relative to starting position)
            Vec3 startPos = API.GetPosition(Entity);
            _waypoints = new Vec3[]
            {
                startPos,
                new Vec3(startPos.X, startPos.Y, startPos.Z + 5f),
                new Vec3(startPos.X, startPos.Y, startPos.Z + 10f),
                new Vec3(startPos.X, startPos.Y, startPos.Z + 5f)
            };

            _vision.EnableDebugReasons(true);
            _vision.EnableDebugLOS(true);

            API.Log("[PatrolEnemyController] Initialized with forward-facing vision");
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            // Update vision system
            _vision?.OnUpdate(dt);

            // Keep enemy locked to initial forward direction (no rotation)
            EnforceForwardDirection();

            // Handle patrol behavior (only when not alert)
            if (_isPatrolling && _vision?.GetState() != VisionComponent.VisionState.Alert)
            {
                UpdatePatrol(dt);
            }
            else if (_vision?.GetState() == VisionComponent.VisionState.Alert)
            {
                // Stop movement when alert
                API.SetLinearVelocity(Entity, new Vec3(0f, API.GetLinearVelocity(Entity).Y, 0f));
            }
        }

        /// <summary>
        /// Enforces that the enemy always faces the initial forward direction
        /// </summary>
        private void EnforceForwardDirection()
        {
            Vec3 currentRot = API.GetRotation(Entity);

            // Only update if rotation has changed
            if (Math.Abs(currentRot.Y - _initialForwardYaw) > 0.01f)
            {
                currentRot.Y = _initialForwardYaw;
                API.SetRotation(Entity, currentRot);
            }
        }

        /// <summary>
        /// Updates patrol behavior moving between waypoints
        /// </summary>
        private void UpdatePatrol(float dt)
        {
            if (_waypoints == null || _waypoints.Length == 0) return;

            Vec3 currentPos = API.GetPosition(Entity);
            Vec3 targetWaypoint = _waypoints[_currentWaypointIndex];

            // Calculate direction to waypoint (only X and Z for ground movement)
            Vec3 direction = new Vec3(
                targetWaypoint.X - currentPos.X,
                0f,
                targetWaypoint.Z - currentPos.Z
            );

            float distanceToWaypoint = (float)Math.Sqrt(
                direction.X * direction.X + direction.Z * direction.Z
            );

            // Check if reached waypoint
            if (distanceToWaypoint < _waypointReachedDistance)
            {
                // Move to next waypoint
                _currentWaypointIndex = (_currentWaypointIndex + 1) % _waypoints.Length;
                API.Log($"[PatrolEnemyController] Reached waypoint, moving to index {_currentWaypointIndex}");
                return;
            }

            // Normalize direction and apply patrol speed
            if (distanceToWaypoint > 0f)
            {
                direction.X = (direction.X / distanceToWaypoint) * _patrolSpeed;
                direction.Z = (direction.Z / distanceToWaypoint) * _patrolSpeed;
            }

            // Apply movement velocity (preserve Y velocity for physics)
            Vec3 velocity = API.GetLinearVelocity(Entity);
            velocity.X = direction.X;
            velocity.Z = direction.Z;
            API.SetLinearVelocity(Entity, velocity);
        }

        // === VISION EVENT HANDLERS ===
        private void OnPlayerDetected(ulong target, Vec3 position)
        {
            API.Log(">>> PATROL ENEMY ALERTED! PLAYER DETECTED! <<<");
            _isPatrolling = false;
            _alertPosition = position;

            // Play alert sound at enemy position
            Vec3 enemyPos = API.GetPosition(Entity);
            API.PlaySoundAt("enemy_alert", "Resources/Audio/playerPunch_1.wav", enemyPos, false);
            API.SetSoundVolume("enemy_alert", 0.8f);

            // TODO: Implement alert behavior (damage player, trigger game over, etc.)
        }

        private void OnPlayerLost(ulong target, Vec3 lastKnownPosition)
        {
            API.Log("[PatrolEnemyController] Lost sight of player, resuming patrol...");
            _isPatrolling = true;
            // TODO: Could add search behavior before resuming patrol
        }

        private void OnPlayerTracking(ulong target, Vec3 position)
        {
            // Player is still visible - maintain alert state
            _alertPosition = position;
        }

        // === PUBLIC CONFIGURATION METHODS ===

        /// <summary>
        /// Set custom patrol waypoints
        /// </summary>
        public void SetWaypoints(Vec3[] waypoints)
        {
            if (waypoints != null && waypoints.Length > 0)
            {
                _waypoints = waypoints;
                _currentWaypointIndex = 0;
                API.Log($"[PatrolEnemyController] Set {waypoints.Length} patrol waypoints");
            }
        }

        /// <summary>
        /// Set patrol movement speed
        /// </summary>
        public void SetPatrolSpeed(float speed)
        {
            _patrolSpeed = speed;
            API.Log($"[PatrolEnemyController] Patrol speed set to: {_patrolSpeed}");
        }

        /// <summary>
        /// Set waypoint reached threshold distance
        /// </summary>
        public void SetWaypointReachedDistance(float distance)
        {
            _waypointReachedDistance = distance;
            API.Log($"[PatrolEnemyController] Waypoint reached distance set to: {_waypointReachedDistance}");
        }

        /// <summary>
        /// Override the forward direction (in degrees)
        /// </summary>
        public void SetForwardDirection(float yawDegrees)
        {
            _initialForwardYaw = yawDegrees;
            EnforceForwardDirection();
            API.Log($"[PatrolEnemyController] Forward direction set to: {_initialForwardYaw} degrees");
        }

        public void OnDestroy()
        {
            _vision?.OnDestroy();
            API.Log($"[PatrolEnemyController] OnDestroy() - Entity: {Entity}");
        }
    }
}