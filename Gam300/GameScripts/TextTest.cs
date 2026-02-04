using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Example script showing all TextComponent API features
    /// Attach this to an entity with a TextComponent to see dynamic text updates
    /// </summary>
    public class TextTest
    {
        public ulong Entity;

        [EditorExposed]
        public int score = 0;

        [EditorExposed]
        public float pulseSpeed = 2.0f;

        [EditorExposed]
        public bool enableRainbow = true;

        private float timeElapsed = 0f;

        public void OnStart(string jsonParams)
        {
            API.Log($"[TextTest] OnStart() - Entity: {Entity}");

            // Verify this entity has a TextComponent
            if (!API.HasText(Entity))
            {
                API.Log("[TextTest] ERROR: This entity doesn't have a TextComponent!");
                return;
            }

            // Set initial text
            API.SetText(Entity, "TextComponent API Test\nPress SPACE to add score");

            API.Log("[TextTest] Started! Press SPACE to increase score.");
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasText(Entity))
                return;

            timeElapsed += dt;

            // Update score when space is pressed
            if (API.IsKeyDown(API.KEY_SPACE))
            {
                score += 10;
                API.Log($"[TextTest] Score increased to {score}");
            }

            // Update text content with score and time
            string displayText = $"Score: {score}\nTime: {timeElapsed:F1}s\n\nPress SPACE for +10 points";
            API.SetText(Entity, displayText);

            // Pulse effect - scale oscillates between 1.0 and 1.5
            float scale = 1.0f + 0.25f * (float)Math.Sin(timeElapsed * pulseSpeed);
            API.SetTextScale(Entity, scale);

            // Rainbow color effect
            if (enableRainbow)
            {
                float r = (float)(Math.Sin(timeElapsed) * 0.5f + 0.5f);
                float g = (float)(Math.Sin(timeElapsed + 2.0f) * 0.5f + 0.5f);
                float b = (float)(Math.Sin(timeElapsed + 4.0f) * 0.5f + 0.5f);
                API.SetTextColor(Entity, new Vec4(r, g, b, 1.0f));
            }
            else
            {
                // White text
                API.SetTextColor(Entity, new Vec4(1, 1, 1, 1));
            }

            // Bonus: Change color based on score
            if (score >= 100)
            {
                // Gold color for high score
                API.SetTextColor(Entity, new Vec4(1.0f, 0.84f, 0.0f, 1.0f));
            }
        }

        public void OnDestroy()
        {
            API.Log($"[TextTest] Final score: {score}");
        }
    }
}
