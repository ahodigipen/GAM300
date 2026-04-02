using Boom;
using System;

namespace GameScripts
{
    public class GameSplash
    {
        public ulong Entity;

        [EditorExposed(displayName: "Display Duration")]
        public float displayDuration = 3.0f;

        [EditorExposed(displayName: "Fade Duration")]
        public float fadeDuration = 0.5f;

        [EditorExposed(displayName: "Next Scene")]
        public string nextSceneName = Entry.MAIN_MENU_SCENE_NAME;

        [EditorExposed(displayName: "Logo Entity Name")]
        public string logoName = "GameLogo";

        private float timer = 0.0f;
        private bool isFinished = false;
        private ulong logoID;

        public void OnStart(string jsonParams)
        {
            API.Log("[GameSplash] OnStart called.");
            logoID = API.FindEntity(logoName);
            
            // Start with logo transparent
            if (logoID != 0) API.SetSpriteColor(logoID, new Vec4(1, 1, 1, 0));

            API.SetCutsceneMode(true);
        }

        public void OnUpdate(float dt)
        {
            if (isFinished) return;

            timer += dt;

            // Handle Logo Alpha Fading
            float alpha = 0f;
            if (timer < fadeDuration)
            {
                // Fade In
                alpha = timer / fadeDuration;
            }
            else if (timer < displayDuration - fadeDuration)
            {
                // Fully Opaque
                alpha = 1.0f;
            }
            else if (timer < displayDuration)
            {
                // Fade Out
                alpha = 1.0f - ((timer - (displayDuration - fadeDuration)) / fadeDuration);
            }
            else
            {
                alpha = 0f;
            }

            alpha = Math.Max(0, Math.Min(1, alpha));

            if (logoID != 0) API.SetSpriteColor(logoID, new Vec4(1, 1, 1, alpha));

            // Auto-transition after displayDuration
            if (timer >= displayDuration)
            {
                FinishSplash();
                return;
            }

            // Allow skipping after minimum display duration
            if (timer >= fadeDuration)
            {
                if (API.IsKeyDown(API.KEY_SPACE) || API.IsKeyDown(API.KEY_ENTER))
                {
                    FinishSplash();
                }
            }
        }

        private void FinishSplash()
        {
            if (isFinished) return;
            isFinished = true;
            API.Log("[GameSplash] Transitioning to " + nextSceneName);
            API.SetCutsceneMode(false);
            SceneFader.FadeToScene(nextSceneName);
        }
    }
}
