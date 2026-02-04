using Boom;
using System;
using System.Collections.Generic;

namespace GameScripts
{
    public class FreezeOverlayBehavior
    {
        public ulong Entity;

        [Boom.EditorExposed("Height Offset", "World units to offset above target")]
        private float _heightOffset = 1.6f;

        [Boom.EditorExposed("Forward Offset", "World units to offset forward (+) or backward (-) from target")]
        private float _forwardOffset = 0.0f;

        // Separate X and Y Scale
        [Boom.EditorExposed("Scale X", "Width scale of the video")]
        private float _scaleX = 0.25f;

        [Boom.EditorExposed("Scale Y", "Height scale of the video")]
        private float _scaleY = 0.45f;

        private bool _isVisible = true;

        // Cache the player ID to avoid string lookup every frame
        private ulong _playerID = 0;

        public void OnStart(string jsonParams)
        {
            // Start hidden
            SetVisible(false);

            // Find player once
            _playerID = API.FindEntity("Samurai");
        }

        public void OnUpdate(float dt)
        {
            // Find the best target (Frozen AND Closest to Player)
            ulong bestTargetID = FindClosestFrozenEnemy();

            if (bestTargetID != 0)
            {
                // We found a frozen enemy! Show chains on them.
                if (!_isVisible) SetVisible(true);

                Vec3 targetPos = API.GetPosition(bestTargetID);
                UpdateScreenPosition(targetPos);
            }
            else
            {
                // No frozen enemies found. Hide chains.
                if (_isVisible) SetVisible(false);
            }
        }

        private ulong FindClosestFrozenEnemy()
        {
            // If player isn't found yet, try to find them (fallback)
            if (_playerID == 0) _playerID = PlayerMovement.GetPlayerEntity();

            Vec3 playerPos = (_playerID != 0) ? API.GetPosition(_playerID) : new Vec3(0, 0, 0);

            ulong closestID = 0;
            float minDistSq = float.MaxValue;

            // Iterate through ALL registered enemies
            foreach (var enemyCtrl in PlayerManager.ActiveEnemies)
            {
                if (enemyCtrl == null) continue;

                // Extract Entity ID safely
                ulong enemyID = 0;
                if (enemyCtrl is PatrolEnemyController patrol) enemyID = patrol.Entity;
                else if (enemyCtrl is EnemyController sentry) enemyID = sentry.Entity;

                if (enemyID == 0) continue;

                // 1. Check if this specific enemy is Frozen
                Vec3 enemyPos = API.GetPosition(enemyID);
                if (FreezeManager.IsFrozen(enemyPos))
                {
                    // 2. Check if it is the closest one so far
                    float dx = playerPos.X - enemyPos.X;
                    float dy = playerPos.Y - enemyPos.Y;
                    float dz = playerPos.Z - enemyPos.Z;
                    float distSq = dx * dx + dy * dy + dz * dz;

                    if (distSq < minDistSq)
                    {
                        minDistSq = distSq;
                        closestID = enemyID;
                    }
                }
            }

            return closestID;
        }

        private void UpdateScreenPosition(Vec3 worldPos)
        {
            // Get player position first
            Vec3 playerPos = API.GetPosition(_playerID);

            // Calculate direction from enemy to player
            Vec3 direction = new Vec3(
                playerPos.X - worldPos.X,
                playerPos.Y - worldPos.Y,
                playerPos.Z - worldPos.Z
            );

            // Normalize direction
            float length = (float)System.Math.Sqrt(
                direction.X * direction.X +
                direction.Y * direction.Y +
                direction.Z * direction.Z
            );

            if (length > 0.001f)
            {
                direction.X /= length;
                direction.Y /= length;
                direction.Z /= length;

                // Apply height offset
                worldPos.Y += _heightOffset;

                // Apply forward offset in the direction towards the player
                worldPos.X += direction.X * _forwardOffset;
                worldPos.Z += direction.Z * _forwardOffset;

                // Set the position
                API.SetPosition(Entity, worldPos);

                // Calculate yaw (rotation around Y axis)
                float yaw = (float)System.Math.Atan2(direction.X, direction.Z);

                // Convert radians to degrees
                float yawDegrees = yaw * (180f / (float)System.Math.PI);

                // Set rotation (Y rotation only for billboard effect, keeps video upright)
                API.SetRotation(Entity, new Vec3(0f, yawDegrees, 0f));
            }
            else
            {
                // Fallback if player is at same position as enemy
                worldPos.Y += _heightOffset;
                API.SetPosition(Entity, worldPos);
            }
        }

        private void SetVisible(bool visible)
        {
            _isVisible = visible;

            if (visible)
            {
                // Apply X & Y scale
                API.SetScale(Entity, new Vec3(_scaleX, _scaleY, 1.0f));

                // Restart the video animation from the beginning
                API.PlayVideo(Entity);
            }
            else
            {
                API.SetScale(Entity, new Vec3(0f, 0f, 0f));
            }
        }
    }
}