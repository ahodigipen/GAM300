using Boom;
using System;
using System.Collections.Generic;

namespace GameScripts
{
    public class FreezeOverlayBehavior
    {
        public ulong Entity;

        [Boom.EditorExposed("Height Offset", "World units to offset above target")]
        private float _heightOffset = 1.0f;

        // Separate X and Y Scale 
        [Boom.EditorExposed("Scale X", "Width scale of the video")]
        private float _scaleX = 0.25f;

        [Boom.EditorExposed("Scale Y", "Height scale of the video")]
        private float _scaleY = 0.45f;

        // Set Z position
        [Boom.EditorExposed("Z Depth", "Layer depth (0 is default, use -0.1 to bring forward)")]
        private float _zIndex = -0.5f;

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
            worldPos.Y += _heightOffset;

            // 1. Get Pixel Coordinates
            Vec2 screenPos;
            bool onScreen = API.ProjectWorldToViewport(worldPos, out screenPos);

            if (onScreen)
            {
                // 2. Get Screen Size
                float screenW, screenH;
                API.GetViewportSize(out screenW, out screenH);

                if (screenW <= 1.0f || screenH <= 1.0f) return;

                // 3. Convert Pixels to NDC (-1 to 1)
                float ndcX = (screenPos.X / screenW) * 2.0f - 1.0f;
                float ndcY = (screenPos.Y / screenH) * 2.0f - 1.0f;

                API.SetPosition(Entity, new Vec3(ndcX, ndcY, _zIndex));
            }
            else
            {
                // Hide if off-screen
                API.SetScale(Entity, new Vec3(0f, 0f, 0f));
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