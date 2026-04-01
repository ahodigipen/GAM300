using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Spotlight follower for patrol enemies with multi-directional waypoints.
    /// Instead of reading the enemy's Y rotation (which suffers from Euler decomposition
    /// issues), this computes the spotlight yaw from the enemy's actual velocity vector
    /// and smoothly interpolates it. Works reliably regardless of movement axis.
    /// </summary>
    public class PatrolMultiDirectionSpotlight
    {
        public ulong Entity;

        [Boom.EditorExposed("Target Name", "Name of patrol enemy entity to follow")]
        private string targetName = "Patrol_1";

        [Boom.EditorExposed("Position Offset Y", "Height offset above target", -10f, 10f, true)]
        private float positionOffsetY = 2.0f;

        [Boom.EditorExposed("Rotation Offset Y", "Extra Y rotation offset if needed", -180f, 180f, true)]
        private float rotationOffsetY = 0f;

        [Boom.EditorExposed("Rotation Speed", "How fast the spotlight rotates to match direction (deg/s)", 60f, 720f, true)]
        private float rotationSpeedDeg = 360f;

        [Boom.EditorExposed("Min Speed To Rotate", "Minimum XZ speed before spotlight rotates", 0.01f, 1f, true)]
        private float minSpeedToRotate = 0.15f;

        private ulong targetHandle;
        private float currentYaw;
        private bool yawInitialized = false;

        // Color state for detection
        private Vec3 originalColor;
        private Vec3 alertColor = new Vec3(1.0f, 0.0f, 0.0f);
        private bool isAlert = false;

        // Static registry so PatrolEnemyController can find spotlights
        private static System.Collections.Generic.Dictionary<string, PatrolMultiDirectionSpotlight> s_Spotlights
            = new System.Collections.Generic.Dictionary<string, PatrolMultiDirectionSpotlight>();

        public void OnStart(string jsonParams)
        {
            // Detect stale registry from previous play session
            if (s_Spotlights.TryGetValue(targetName, out PatrolMultiDirectionSpotlight existing) && existing != this)
            {
                API.Log("[PatrolMultiDirSpotlight] Detected stale registry - clearing for new session");
                s_Spotlights.Clear();
            }

            // Find target entity
            targetHandle = API.FindEntity(targetName);
            if (targetHandle == 0)
            {
                API.Log($"[PatrolMultiDirSpotlight] Could not find target entity: {targetName}");
            }
            else
            {
                API.Log($"[PatrolMultiDirSpotlight] Following entity: {targetName}");
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
            if (targetHandle == 0 || Entity == 0 || dt <= 0f || !API.HasTransform(targetHandle))
                return;

            // --- Position: follow target with Y offset ---
            Vec3 targetPos = API.GetPosition(targetHandle);
            API.SetPosition(Entity, new Vec3(
                targetPos.X,
                targetPos.Y + positionOffsetY,
                targetPos.Z
            ));

            // --- Rotation: compute yaw from velocity, bypassing engine Euler decomposition ---
            float targetYaw = GetTargetYaw();

            if (!yawInitialized)
            {
                // First frame: snap to the target yaw immediately
                currentYaw = targetYaw;
                yawInitialized = true;
            }
            else
            {
                // Smoothly rotate towards target yaw
                float delta = Wrap180(targetYaw - currentYaw);
                float maxStep = rotationSpeedDeg * dt;

                if (Math.Abs(delta) <= maxStep)
                    currentYaw = targetYaw;
                else
                    currentYaw += Math.Sign(delta) * maxStep;

                currentYaw = Wrap360(currentYaw);
            }

            // Spotlight points opposite to the enemy's forward (+180)
            float spotlightYaw = Wrap360(currentYaw + 180f + rotationOffsetY);

            // Set the full rotation to avoid any axis crosstalk
            Vec3 spotRot = API.GetRotation(Entity);
            spotRot.Y = spotlightYaw;
            API.SetRotation(Entity, spotRot);
        }

        /// <summary>
        /// Gets the target yaw. Prefers the PatrolEnemyController's authoritative yaw,
        /// falls back to computing yaw from velocity, and lastly reads engine rotation.
        /// </summary>
        private float GetTargetYaw()
        {
            // Priority 1: Read directly from PatrolEnemyController (no Euler issues)
            var controller = PatrolEnemyController.GetByName(targetName);
            if (controller != null)
                return controller.GetYaw();

            // Priority 2: Compute from velocity (works for any movement direction)
            Vec3 vel = API.GetLinearVelocity(targetHandle);
            float speedXZ = (float)Math.Sqrt(vel.X * vel.X + vel.Z * vel.Z);

            if (speedXZ >= minSpeedToRotate)
            {
                float yaw = (float)(Math.Atan2(vel.X, vel.Z) * 180.0 / Math.PI);
                return Wrap360(yaw);
            }

            // Priority 3: Fallback - keep current yaw (enemy is stationary)
            return currentYaw;
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

        public static PatrolMultiDirectionSpotlight GetByTargetName(string name)
        {
            if (s_Spotlights.TryGetValue(name, out PatrolMultiDirectionSpotlight spotlight))
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

        public static void ClearRegistry()
        {
            s_Spotlights.Clear();
            API.Log("[PatrolMultiDirSpotlight] Registry cleared");
        }

        // === UTILS ===
        private static float Wrap360(float a)
        {
            while (a >= 360f) a -= 360f;
            while (a < 0f) a += 360f;
            return a;
        }

        private static float Wrap180(float a)
        {
            while (a > 180f) a -= 360f;
            while (a <= -180f) a += 360f;
            return a;
        }
    }
}
