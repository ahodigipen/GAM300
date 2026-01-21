using Boom;
using System;

namespace GameScripts
{
    public static class FreezeManager
    {
        private static float s_freezeTimer = 0f;
        private static Vec3 s_freezeCenter;
        private static float s_freezeRadius = 0f;

        public static void TriggerFreeze(Vec3 center, float radius, float duration)
        {
            s_freezeTimer = duration;
            s_freezeCenter = center;
            s_freezeRadius = radius;

            // Log confirmation
            API.Log($"[FreezeManager] ACTIVATED! Duration: {duration}s");
        }

        public static void Update(float dt)
        {
            if (s_freezeTimer > 0f)
            {
                s_freezeTimer -= dt;
                if (s_freezeTimer <= 0f)
                {
                    s_freezeTimer = 0f;
                    API.Log("[FreezeManager] Time resumed.");
                }
            }
        }

        public static bool IsFrozen(Vec3 entityPos)
        {
            if (s_freezeTimer <= 0f) return false;

            float dx = entityPos.X - s_freezeCenter.X;
            float dz = entityPos.Z - s_freezeCenter.Z;
            float distSq = (dx * dx) + (dz * dz);
            bool isInside = distSq <= (s_freezeRadius * s_freezeRadius);

            // --- DEBUG LOGGING FOR ENEMY ---
            float actualDistance = (float)Math.Sqrt(distSq);
            if (actualDistance < (s_freezeRadius * 2.0f))
            {
                string status = isInside ? "FROZEN" : "NOT FROZEN";
                API.Log($"[FreezeCheck] Enemy at {entityPos.X:F1},{entityPos.Z:F1} is {actualDistance:F1}m from Center {s_freezeCenter.X:F1},{s_freezeCenter.Z:F1}. Result: {status}");
            }

            return isInside;
        }

        public static bool IsActive()
        {
            return s_freezeTimer > 0f;
        }

        /// <summary>
        /// Reset static state (call on scene change)
        /// </summary>
        public static void Reset()
        {
            s_freezeTimer = 0f;
            s_freezeRadius = 0f;
            API.Log("[FreezeManager] Reset");
        }
    }
}