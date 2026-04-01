using Boom;

namespace GameScripts
{
    public class SpotlightFollower
    {
        public ulong Entity;  // Set by the engine

        private ulong targetHandle;

        [Boom.EditorExposed("Target Name", "Name of sentry entity to follow")]
        private string targetName = "Sentry_1";

        [Boom.EditorExposed("Position Offset Y", "Height offset above target", 0f, 10f, true)]
        private float positionOffsetY = 2.0f;

        [Boom.EditorExposed("Is Patrol Enemy", "Enable if following a PatrolEnemyController (uses different rotation method)")]
        private bool isPatrolEnemy = false;

        private Vec3 positionOffset = new Vec3(0, 2, 0);  // Offset from target (e.g., above the head)
        private bool followRotation = true;

        // Color state for detection
        private Vec3 originalColor;
        private Vec3 alertColor = new Vec3(1.0f, 0.0f, 0.0f);  // Red when detected
        private bool isAlert = false;

        // Static registry so EnemyController can find spotlights by their target name
        private static System.Collections.Generic.Dictionary<string, SpotlightFollower> s_Spotlights
            = new System.Collections.Generic.Dictionary<string, SpotlightFollower>();

        // Track if registry has been cleared this session
        private static bool s_RegistryCleared = false;

        public void OnStart(string jsonParams)
        {
            API.Log($"[SpotlightFollower] OnStart() - Entity: {Entity}");

            // Clear stale references from previous play session (only once per session)
            // The first SpotlightFollower to initialize will clear the registry
            if (!s_RegistryCleared)
            {
                s_Spotlights.Clear();
                s_RegistryCleared = true;
                API.Log("[SpotlightFollower] Cleared stale spotlight registry for new session");
            }

            // Apply the exposed positionOffsetY to the offset vector
            positionOffset.Y = positionOffsetY;

            // Find the target entity
            targetHandle = API.FindEntity(targetName);
            if (targetHandle == 0)
            {
                API.Log($"[SpotlightFollower] Could not find target entity: {targetName}");
            }
            else
            {
                API.Log($"[SpotlightFollower] Following entity: {targetName}");
            }

            // Store original color if we have a spotlight component
            if (API.HasSpotLight(Entity))
            {
                originalColor = API.GetSpotLightColor(Entity);
                API.Log($"[SpotlightFollower] Stored original color: ({originalColor.X}, {originalColor.Y}, {originalColor.Z})");
            }

            // Register this spotlight so EnemyController can find it
            s_Spotlights[targetName] = this;
            API.Log($"[SpotlightFollower] Registered spotlight for target: {targetName} (Total: {s_Spotlights.Count})");
        }

        public void OnUpdate(float dt)
        {
            if (targetHandle == 0 || Entity == 0 || !API.HasTransform(targetHandle))
                return;

            // Get target's position and rotation
            Vec3 targetPos = API.GetPosition(targetHandle);
            Vec3 targetRot = API.GetRotation(targetHandle);

            // Apply position with offset
            Vec3 newPos = new Vec3(
                targetPos.X + positionOffset.X,
                targetPos.Y + positionOffset.Y,
                targetPos.Z + positionOffset.Z
            );
            API.SetPosition(Entity, newPos);

            // Follow rotation if enabled
            if (followRotation)
            {
                // Add 180 to Y rotation because spotlight points -Z but model faces +Z
                if (isPatrolEnemy)
                {
                    // For patrol enemies, use GetRotationY to get the Y value set by SetRotationY
                    float targetYaw = API.GetRotationY(targetHandle);
                    float spotlightYaw = targetYaw + 180f;
                    API.SetRotationY(Entity, spotlightYaw);
                }
                else
                {
                    // For sentry enemies, read/write only Y (same as patrol path) to avoid
                    // quaternion conversion artifacts from GetRotation().Y
                    float targetYaw = API.GetRotationY(targetHandle);
                    float spotlightYaw = targetYaw + 180f;
                    API.SetRotationY(Entity, spotlightYaw);
                }
            }
        }

        public void OnDestroy()
        {
            // Unregister from static registry
            if (s_Spotlights.ContainsKey(targetName))
            {
                s_Spotlights.Remove(targetName);
            }
            API.Log($"[SpotlightFollower] OnDestroy() - Entity: {Entity}");
        }

        // === PUBLIC METHODS FOR COLOR CONTROL ===

        /// <summary>
        /// Set spotlight to alert (red) color
        /// </summary>
        public void SetAlert(bool alert)
        {
            if (!API.HasSpotLight(Entity))
                return;

            if (alert && !isAlert)
            {
                API.SetSpotLightColor(Entity, alertColor);
                isAlert = true;
                API.Log($"[SpotlightFollower] Set to ALERT color (red)");
            }
            else if (!alert && isAlert)
            {
                API.SetSpotLightColor(Entity, originalColor);
                isAlert = false;
                API.Log($"[SpotlightFollower] Reset to ORIGINAL color");
            }
        }

        /// <summary>
        /// Reset spotlight to original color (call on player death/respawn)
        /// </summary>
        public void ResetColor()
        {
            if (!API.HasSpotLight(Entity))
                return;

            API.SetSpotLightColor(Entity, originalColor);
            isAlert = false;
            API.Log($"[SpotlightFollower] Reset to original color");
        }

        /// <summary>
        /// Set a custom alert color (default is red)
        /// </summary>
        public void SetAlertColor(Vec3 color)
        {
            alertColor = color;
        }

        /// <summary>
        /// Directly set the spotlight's Y rotation (for PatrolEnemyController)
        /// </summary>
        public void SetYaw(float yaw)
        {
            if (Entity == 0) return;
            // Add 180 because spotlight points -Z but model faces +Z
            float spotlightYaw = yaw + 180f;
            API.SetRotationY(Entity, spotlightYaw);
        }

        // === STATIC METHODS FOR EXTERNAL ACCESS ===

        /// <summary>
        /// Get the SpotlightFollower instance for a given enemy name
        /// </summary>
        public static SpotlightFollower GetByTargetName(string enemyName)
        {
            if (s_Spotlights.TryGetValue(enemyName, out SpotlightFollower spotlight))
            {
                return spotlight;
            }
            return null;
        }

        /// <summary>
        /// Reset all spotlights to their original colors (call on player death)
        /// </summary>
        public static void ResetAllSpotlights()
        {
            foreach (var kvp in s_Spotlights)
            {
                kvp.Value.ResetColor();
            }
            API.Log($"[SpotlightFollower] Reset all {s_Spotlights.Count} spotlights to original colors");
        }

        /// <summary>
        /// Clear the registry for a new play session. Call this when stopping play mode.
        /// </summary>
        public static void ClearRegistry()
        {
            s_Spotlights.Clear();
            s_RegistryCleared = false;
            API.Log("[SpotlightFollower] Registry cleared for new session");
        }
    }
}
