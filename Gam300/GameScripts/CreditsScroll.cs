using Boom;
using System;
using System.Collections.Generic;

namespace GameScripts
{
    /// <summary>
    /// Mandatory Credits Sequence Script.
    /// Transition occurs automatically once the designated 'Last Entity' scrolls off-screen.
    /// </summary>
    public class CreditsScroll
    {
        public ulong Entity; // Main credits text entity

        [EditorExposed(displayName: "Scroll Speed", tooltip: "Speed at which TEXT scrolls up (Pixels/sec)")]
        public float scrollSpeed = 100.0f;

        [EditorExposed(displayName: "Logo Speed Multiplier", tooltip: "Adjust this so logos match text speed. Try 0.01 or 0.1.")]
        public float logoSpeedMultiplier = 1.0f;

        [EditorExposed(displayName: "Initial Position", tooltip: "Starting screen coordinates for text (Pixels).")]
        public Vec2 startPosition = new Vec2(960.0f, -100.0f);

        [EditorExposed(displayName: "GUI Z Layer", tooltip: "Z coordinate for 2D elements.")]
        public float guiZ = 0.0f;

        [EditorExposed(displayName: "Next Scene", tooltip: "Scene to load after credits finish")]
        public string nextSceneName = Entry.MAIN_MENU_SCENE_NAME;

        [EditorExposed(displayName: "Last Entity Name", tooltip: "The name of the VERY LAST entity in the sequence (usually a Footer Logo).")]
        public string lastEntityName = "";

        [EditorExposed(displayName: "End Buffer", tooltip: "Extra pixels to wait after the last entity disappears.")]
        public float endBuffer = 500.0f;

        [EditorExposed(displayName: "Header Logos (Top)", tooltip: "Logos appearing BEFORE the text. Comma separated.")]
        public string headerLogoNames = "";

        [EditorExposed(displayName: "Header Offsets", tooltip: "Offsets for Top logos. Format: x1,y1; x2,y2")]
        public string headerOffsets = "0,10.0";

        [EditorExposed(displayName: "Footer Logos (Bottom)", tooltip: "Logos appearing AFTER the text. Comma separated.")]
        public string footerLogoNames = "";

        [EditorExposed(displayName: "Footer Offsets", tooltip: "Offsets for Bottom logos. Format: x1,y1; x2,y2")]
        public string footerOffsets = "0,-50.0";

        private float currentY;
        private float viewportWidth;
        private float viewportHeight;
        private bool isFinished = false;

        private List<ulong> logoIDs = new List<ulong>();
        private List<Vec2> relativeOffsets = new List<Vec2>();
        private ulong lastEntityID = 0;
        private Vec2 lastEntityOffset = new Vec2(0,0);

        public void OnStart(string jsonParams)
        {
            API.GetViewportSize(out viewportWidth, out viewportHeight);
            currentY = startPosition.Y;

            // Parse Logos
            ParseLogos(headerLogoNames, headerOffsets);
            ParseLogos(footerLogoNames, footerOffsets);

            // Identify the last entity for transition detection
            lastEntityID = API.FindEntity(lastEntityName);
            if (lastEntityID != 0)
            {
                // Find its offset in our list to know its relative position
                for (int i = 0; i < logoIDs.Count; i++) {
                    if (logoIDs[i] == lastEntityID) {
                        lastEntityOffset = relativeOffsets[i];
                        break;
                    }
                }
            }
            else {
                lastEntityID = Entity; // Default to main text
            }

            UpdatePositions();
            API.SetCutsceneMode(true);
        }

        private void ParseLogos(string namesStr, string offsetsStr)
        {
            string[] names = namesStr.Split(new char[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            string[] offsets = offsetsStr.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries);

            for (int i = 0; i < names.Length; i++)
            {
                ulong id = API.FindEntity(names[i].Trim());
                if (id != 0)
                {
                    logoIDs.Add(id);
                    Vec2 offset = new Vec2(0, 0);
                    if (i < offsets.Length)
                    {
                        string[] xy = offsets[i].Split(',');
                        if (xy.Length == 2)
                        {
                            float.TryParse(xy[0].Trim(), out offset.X);
                            float.TryParse(xy[1].Trim(), out offset.Y);
                        }
                    }
                    relativeOffsets.Add(offset);
                    if (API.HasSprite(id)) API.SetSpriteAlpha(id, 1.0f);
                }
            }
        }

        public void OnUpdate(float dt)
        {
            if (isFinished) return;

            currentY += scrollSpeed * dt;
            UpdatePositions();

            // Transition detection
            float finalCheckY;
            if (lastEntityID == Entity) {
                finalCheckY = currentY;
            } else {
                // If it's a logo, we calculate its current screen-space equivalent position
                // We use the same formula as in UpdatePositions but keep it in 'pixel-like' units
                finalCheckY = (currentY * logoSpeedMultiplier) + lastEntityOffset.Y;
                
                // If the multiplier is small (e.g. 0.01), we need to scale the viewportHeight check too
                // Or easier: check if the logo is above the top (which is viewportHeight in pixel-space)
            }

            // Since (0,0) is bottom-left, 'off-screen top' is > viewportHeight
            // We adjust for the multiplier if it's a logo
            float limit = (lastEntityID == Entity) ? viewportHeight : (viewportHeight * logoSpeedMultiplier);

            if (finalCheckY > limit + endBuffer) 
            {
                FinishCredits();
            }

            if (API.IsKeyDown(API.KEY_SPACE) || API.IsKeyDown(API.KEY_ESCAPE) || API.IsKeyDown(API.KEY_ENTER))
            {
                FinishCredits();
            }
        }

        private void UpdatePositions()
        {
            if (API.HasText(Entity))
            {
                API.SetTextPosition(Entity, new Vec2(startPosition.X, currentY));
            }

            for (int i = 0; i < logoIDs.Count; i++)
            {
                float logoX = (startPosition.X * logoSpeedMultiplier) + relativeOffsets[i].X;
                float logoY = (currentY * logoSpeedMultiplier) + relativeOffsets[i].Y;
                API.SetPosition(logoIDs[i], new Vec3(logoX, logoY, guiZ));
            }
        }

        private void FinishCredits()
        {
            if (isFinished) return;
            isFinished = true;
            API.SetCutsceneMode(false);
            API.LoadScene(nextSceneName);
        }

        public void OnDestroy() => API.SetCutsceneMode(false);
    }
}
