using Boom;

namespace GameScripts
{
    public class SpotlightFollower
    {
        public ulong Entity;  // Set by the engine

        private ulong targetHandle;
        private string targetName = "Sentry_1";  // Name of entity to follow
        private Vec3 positionOffset = new Vec3(0, 2, 0);  // Offset from target (e.g., above the head)
        private bool followRotation = true;

        // Color state for detection
        private Vec3 originalColor;
        private Vec3 alertColor = new Vec3(1.0f, 0.0f, 0.0f);  // Red when detected
        private bool isAlert = false;

        // Static registry so EnemyController can find spotlights by their target name
        private static System.Collections.Generic.Dictionary<string, SpotlightFollower> s_Spotlights
            = new System.Collections.Generic.Dictionary<string, SpotlightFollower>();

        public void OnStart(string jsonParams)
        {
            API.Log($"[SpotlightFollower] OnStart() - Entity: {Entity}");

            // Parse params if provided
            if (!string.IsNullOrEmpty(jsonParams) && jsonParams != "{}")
            {
                try
                {
                    // Simple JSON parsing for targetName
                    if (jsonParams.Contains("targetName"))
                    {
                        int start = jsonParams.IndexOf("targetName") + 13;
                        int end = jsonParams.IndexOf("\"", start);
                        if (end > start)
                        {
                            targetName = jsonParams.Substring(start, end - start);
                        }
                    }
                }
                catch { }
            }

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
        }

        public void OnUpdate(float dt)
        {
            if (targetHandle == 0 || Entity == 0)
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
                targetRot.Y += 180f;
                API.SetRotation(Entity, targetRot);
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
    }
}
