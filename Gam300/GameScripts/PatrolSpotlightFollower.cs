using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Spotlight follower specifically for PatrolEnemyController.
    /// Gets yaw directly from the controller instead of using API.GetRotation().
    /// </summary>
    public class PatrolSpotlightFollower
    {
        public ulong Entity;

        [Boom.EditorExposed("Target Name", "Name of patrol enemy entity to follow")]
        private string targetName = "Patrol_1";

        [Boom.EditorExposed("Position Offset Y", "Height offset above target", 0f, 10f, true)]
        private float positionOffsetY = 2.0f;

        private ulong targetHandle;
        private PatrolEnemyController _controller;

        // Color state for detection
        private Vec3 originalColor;
        private Vec3 alertColor = new Vec3(1.0f, 0.0f, 0.0f);
        private bool isAlert = false;

        // Static registry so PatrolEnemyController can find spotlights
        private static System.Collections.Generic.Dictionary<string, PatrolSpotlightFollower> s_Spotlights
            = new System.Collections.Generic.Dictionary<string, PatrolSpotlightFollower>();

        public void OnStart(string jsonParams)
        {
            // Detect stale registry from previous play session by checking if our key exists with different instance
            if (s_Spotlights.TryGetValue(targetName, out PatrolSpotlightFollower existing) && existing != this)
            {
                API.Log("[PatrolSpotlightFollower] Detected stale registry - clearing for new session");
                s_Spotlights.Clear();
            }

            // Find target entity
            targetHandle = API.FindEntity(targetName);
            if (targetHandle == 0)
            {
                API.Log($"[PatrolSpotlightFollower] Could not find target entity: {targetName}");
            }
            else
            {
                API.Log($"[PatrolSpotlightFollower] Following entity: {targetName}");
            }

            // Get controller reference
            _controller = PatrolEnemyController.GetByName(targetName);
            if (_controller == null)
            {
                API.Log($"[PatrolSpotlightFollower] WARNING: Could not find PatrolEnemyController for: {targetName}");
            }

            // Store original color
            if (API.HasSpotLight(Entity))
            {
                originalColor = API.GetSpotLightColor(Entity);
            }

            // Register
            s_Spotlights[targetName] = this;
        }

        public void OnUpdate(float dt)
        {
            if (targetHandle == 0 || Entity == 0)
                return;

            // Update position
            Vec3 targetPos = API.GetPosition(targetHandle);
            Vec3 newPos = new Vec3(
                targetPos.X,
                targetPos.Y + positionOffsetY,
                targetPos.Z
            );
            API.SetPosition(Entity, newPos);

            // Update rotation - get yaw directly from controller
            if (_controller == null)
            {
                // Try to get controller again (might have initialized after us)
                _controller = PatrolEnemyController.GetByName(targetName);
            }

            if (_controller != null)
            {
                float yaw = _controller.GetYaw();
                // Add 180 because spotlight points -Z but model faces +Z
                float spotlightYaw = yaw + 180f;
                API.SetRotationY(Entity, spotlightYaw);
            }
          
        }

        public void OnDestroy()
        {
            if (s_Spotlights.ContainsKey(targetName))
            {
                s_Spotlights.Remove(targetName);
            }
        }

        // === COLOR CONTROL ===

        public void SetAlert(bool alert)
        {
            if (!API.HasSpotLight(Entity))
                return;

            if (alert && !isAlert)
            {
                API.SetSpotLightColor(Entity, alertColor);
                isAlert = true;
            }
            else if (!alert && isAlert)
            {
                API.SetSpotLightColor(Entity, originalColor);
                isAlert = false;
            }
        }

        public void ResetColor()
        {
            if (!API.HasSpotLight(Entity))
                return;

            API.SetSpotLightColor(Entity, originalColor);
            isAlert = false;
        }

        // === STATIC METHODS ===

        public static PatrolSpotlightFollower GetByTargetName(string name)
        {
            if (s_Spotlights.TryGetValue(name, out PatrolSpotlightFollower spotlight))
            {
                return spotlight;
            }
            return null;
        }

        public static void ResetAllSpotlights()
        {
            foreach (var kvp in s_Spotlights)
            {
                kvp.Value.ResetColor();
            }
        }

        /// <summary>
        /// Clear the registry for a new play session.
        /// </summary>
        public static void ClearRegistry()
        {
            s_Spotlights.Clear();
            API.Log("[PatrolSpotlightFollower] Registry cleared");
        }
    }
}
