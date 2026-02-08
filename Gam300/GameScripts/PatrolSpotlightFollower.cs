using Boom;

namespace GameScripts
{
    /// <summary>
    /// Spotlight follower for patrol enemies.
    /// Copies the target's Y rotation directly to the spotlight.
    /// </summary>
    public class PatrolSpotlightFollower
    {
        public ulong Entity;

        [Boom.EditorExposed("Target Name", "Name of patrol enemy entity to follow")]
        private string targetName = "Patrol_1";

        [Boom.EditorExposed("Position Offset Y", "Height offset above target", -10f, 10f, true)]
        private float positionOffsetY = 2.0f;

        [Boom.EditorExposed("Rotation Offset Y", "Extra Y rotation offset if needed", -180f, 180f, true)]
        private float rotationOffsetY = 0f;

        private ulong targetHandle;

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

            // Update rotation - read yaw directly from the controller to avoid
            // Euler angle decomposition issues with the engine transform
            float enemyYaw;
            var controller = PatrolEnemyController.GetByName(targetName);
            if (controller != null)
                enemyYaw = controller.GetYaw();
            else
                enemyYaw = API.GetRotationY(targetHandle); // fallback

            float spotlightYaw = enemyYaw + 180f + rotationOffsetY;
            // Normalize to 0-360 range to avoid wrapping issues
            while (spotlightYaw >= 360f) spotlightYaw -= 360f;
            while (spotlightYaw < 0f) spotlightYaw += 360f;
            API.SetRotationY(Entity, spotlightYaw);
          
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
