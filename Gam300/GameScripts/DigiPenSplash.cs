using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Script for the mandatory DigiPen Splash Screen.
    /// Requirements:
    /// 1. Centered DigiPen Logo (Large)
    /// 2. Copyright notice at bottom center
    /// 3. Minimum 2 second display time
    /// 4. Theme options: White on Black or Red on White
    /// </summary>
    public class DigiPenSplash
    {
        public ulong Entity; // Usually a manager or the background entity

        public enum SplashTheme { WhiteOnBlack, RedOnWhite }

        [EditorExposed(displayName: "Theme", tooltip: "Choose between White on Black or Red on White")]
        public SplashTheme theme = SplashTheme.WhiteOnBlack;

        [EditorExposed(displayName: "Logo Entity Name", tooltip: "Name of the entity with the DigiPen Logo Sprite")]
        public string logoName = "DigiPenLogo";

        [EditorExposed(displayName: "Copyright Entity Name", tooltip: "Name of the entity with the Copyright TextComponent")]
        public string copyrightName = "CopyrightNotice";

        [EditorExposed(displayName: "Background Entity Name", tooltip: "Name of the background sprite to change color")]
        public string backgroundName = "SplashBackground";

        [EditorExposed(displayName: "Display Duration", tooltip: "Minimum time to show the logo in seconds")]
        public float displayDuration = 2.0f;

        [EditorExposed(displayName: "Next Scene", tooltip: "Scene to load after the splash")]
        public string nextSceneName = Entry.MAIN_MENU_SCENE_NAME;

        private float timer = 0.0f;
        private bool isFinished = false;
        private float viewportWidth;
        private float viewportHeight;

        private ulong logoID;
        private ulong copyrightID;
        private ulong backgroundID;

        public void OnStart(string jsonParams)
        {
            API.GetViewportSize(out viewportWidth, out viewportHeight);
            
            logoID = API.FindEntity(logoName);
            copyrightID = API.FindEntity(copyrightName);
            backgroundID = API.FindEntity(backgroundName);

            ApplyTheme();
            CenterElements();

            API.SetCutsceneMode(true);
        }

        private void ApplyTheme()
        {
            Vec4 bgColor = (theme == SplashTheme.WhiteOnBlack) ? new Vec4(0, 0, 0, 1) : new Vec4(1, 1, 1, 1);
            Vec4 logoColor = (theme == SplashTheme.WhiteOnBlack) ? new Vec4(1, 1, 1, 1) : new Vec4(0.5f, 0, 0, 1); // Red-ish for RedOnWhite
            Vec4 textColor = (theme == SplashTheme.WhiteOnBlack) ? new Vec4(1, 1, 1, 1) : new Vec4(0, 0, 0, 1);

            if (backgroundID != 0) API.SetSpriteColor(backgroundID, bgColor);
            if (logoID != 0) API.SetSpriteColor(logoID, logoColor);
            if (copyrightID != 0) API.SetTextColor(copyrightID, textColor);
        }

        private void CenterElements()
        {
            float centerX = viewportWidth / 2.0f;
            float centerY = viewportHeight / 2.0f;

            // Center Logo
            if (logoID != 0)
            {
                API.SetPosition(logoID, new Vec3(centerX, centerY, 0));
            }

            // Bottom Center Copyright
            if (copyrightID != 0)
            {
                API.SetTextPosition(copyrightID, new Vec2(centerX, 50.0f));
            }
        }

        public void OnUpdate(float dt)
        {
            if (isFinished) return;

            timer += dt;

            // Optional: Skip after minimum time
            if (timer >= displayDuration)
            {
                if (API.IsKeyDown(API.KEY_SPACE) || API.IsKeyDown(API.KEY_ENTER) || timer >= displayDuration + 1.0f)
                {
                    FinishSplash();
                }
            }
        }

        private void FinishSplash()
        {
            if (isFinished) return;
            isFinished = true;
            API.SetCutsceneMode(false);
            API.LoadScene(nextSceneName);
        }

        public void OnDestroy()
        {
            API.SetCutsceneMode(false);
        }
    }
}
