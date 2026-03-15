using Boom;
using System;
using System.Collections.Generic;

namespace GameScripts
{
    /// <summary>
    /// Mandatory Credits Sequence Script.
    /// Scrolls credits text/logos, then scrolls up a Main Menu layout from below.
    /// </summary>
    public class CreditsScroll
    {
        public ulong Entity; // Main credits text entity

        [EditorExposed(displayName: "Scroll Speed", tooltip: "Speed at which TEXT scrolls up (Pixels/sec)")]
        public float scrollSpeed = 100.0f;

        [EditorExposed(displayName: "Logo Speed Multiplier", tooltip: "Adjust this so logos match text speed.")]
        public float logoSpeedMultiplier = 1.0f;

        [EditorExposed(displayName: "Initial Position", tooltip: "Starting screen coordinates for text (Pixels).")]
        public Vec2 startPosition = new Vec2(960.0f, -100.0f);

        [EditorExposed(displayName: "GUI Z Layer", tooltip: "Z coordinate for 2D elements.")]
        public float guiZ = 0.0f;

        [EditorExposed(displayName: "Next Scene", tooltip: "Scene to load AFTER menu scroll finishes")]
        public string nextSceneName = Entry.MAIN_MENU_SCENE_NAME;

        [EditorExposed(displayName: "End Buffer", tooltip: "Extra pixels to wait after text clears top.")]
        public float endBuffer = 500.0f;

        [EditorExposed(displayName: "Main Menu Layout Name", tooltip: "The parent entity of your menu buttons/layout.")]
        public string mainMenuLayoutName = "MainMenuLayout";

        [EditorExposed(displayName: "Menu Scroll Speed", tooltip: "Speed at which the menu enters (Units/sec).")]
        public float menuScrollSpeed = 0.5f;

        [EditorExposed(displayName: "Menu Start Y", tooltip: "Y position where the menu begins.")]
        public float menuStartY = -1.7f;

        [EditorExposed(displayName: "Menu Target Y", tooltip: "Y position where the menu stops.")]
        public float menuTargetY = 0.0f;

        [EditorExposed(displayName: "Text Move Multiplier", tooltip: "Scale factor for text sync. Try 100 if world is in meters.")]
        public float textMoveMultiplier = 100.0f;

        private float currentY;
        private float viewportWidth;
        private float viewportHeight;
        private float textHeight = 0.0f;
        private bool isCreditsFinished = false;
        private bool isMenuScrolling = false;
        private ulong menuLayoutID = 0;
        private float currentMenuY; 

        private List<ulong> allMenuEntities = new List<ulong>();
        private Dictionary<ulong, Vec3> originalPositions = new Dictionary<ulong, Vec3>();
        private Dictionary<ulong, Vec2> originalTextPositions = new Dictionary<ulong, Vec2>();

        [EditorExposed(displayName: "Header Logos (Top)", tooltip: "Logos appearing BEFORE the text. Comma separated.")]
        public string headerLogoNames = "";

        [EditorExposed(displayName: "Header Offsets", tooltip: "Offsets for Top logos. Format: x1,y1; x2,y2")]
        public string headerOffsets = "0,10.0";

        [EditorExposed(displayName: "Footer Logos (Bottom)", tooltip: "Logos appearing AFTER the text. Comma separated.")]
        public string footerLogoNames = "";

        [EditorExposed(displayName: "Footer Offsets", tooltip: "Offsets for Bottom logos. Format: x1,y1; x2,y2")]
        public string footerOffsets = "0,-50.0";

        private List<ulong> logoIDs = new List<ulong>();
        private List<Vec2> relativeOffsets = new List<Vec2>();

        public void OnStart(string jsonParams)
        {
            API.GetViewportSize(out viewportWidth, out viewportHeight);
            currentY = startPosition.Y;

            if (API.HasText(Entity))
            {
                textHeight = API.GetTextHeight(Entity);
            }

            ParseLogos(headerLogoNames, headerOffsets);
            ParseLogos(footerLogoNames, footerOffsets);

            // 1. Find and Record Menu Layout
            menuLayoutID = API.FindEntity(mainMenuLayoutName);
            if (menuLayoutID != 0)
            {
                // Record target positions (where they are in the editor)
                RecordHierarchy(menuLayoutID);
                
                // Initialize to starting Y pos
                currentMenuY = menuStartY;
                UpdateMenuPositions();
            }

            UpdatePositions();
            API.SetCutsceneMode(true);
        }

        private void RecordHierarchy(ulong entity)
        {
            if (entity == 0) return;
            if (!allMenuEntities.Contains(entity))
            {
                allMenuEntities.Add(entity);
                
                if (API.HasTransform(entity))
                    originalPositions[entity] = API.GetPosition(entity);
                else
                    originalPositions[entity] = new Vec3(0, 0, 0);

                if (API.HasText(entity))
                {
                    originalTextPositions[entity] = API.GetTextPosition(entity);
                }

                ulong[] children = API.GetChildren(entity);
                if (children != null)
                {
                    foreach (var child in children)
                    {
                        RecordHierarchy(child);
                    }
                }
            }
        }

        private void UpdateMenuPositions()
        {
            if (menuLayoutID == 0) return;

            // Calculate how far we are from the target Y
            float moveDelta = currentMenuY - menuTargetY;

            // 1. Move the ROOT (Engine hierarchy handles child sprites/models)
            if (API.HasTransform(menuLayoutID))
            {
                Vec3 rootOrig = originalPositions[menuLayoutID];
                API.SetPosition(menuLayoutID, new Vec3(rootOrig.X, currentMenuY, rootOrig.Z));
            }

            // 2. Move all child TEXT components manually (Absolute screen-space)
            foreach (var id in allMenuEntities)
            {
                if (API.HasText(id))
                {
                    Vec2 tOrig = originalTextPositions[id];
                    // We apply the unit delta scaled by the multiplier to the original pixel position
                    API.SetTextPosition(id, new Vec2(tOrig.X, tOrig.Y + (moveDelta * textMoveMultiplier)));
                }
            }
        }

        public void OnUpdate(float dt)
        {
            // 1. Credits Scrolling
            if (!isCreditsFinished)
            {
                currentY += scrollSpeed * dt;
                UpdatePositions();

                float bottomY = (textHeight > 0) ? (currentY - textHeight) : currentY;
                if (bottomY > viewportHeight + endBuffer) 
                {
                    StartMenuScroll();
                }

                // Skip support
                if (API.IsKeyDown(API.KEY_SPACE) || API.IsKeyDown(API.KEY_ESCAPE) || API.IsKeyDown(API.KEY_ENTER))
                {
                    StartMenuScroll();
                }
            }

            // 2. Menu Scrolling
            if (isMenuScrolling && menuLayoutID != 0)
            {
                currentMenuY += menuScrollSpeed * dt;
                
                if (currentMenuY >= menuTargetY)
                {
                    currentMenuY = menuTargetY;
                    UpdateMenuPositions();
                    isMenuScrolling = false;
                    API.SetCutsceneMode(false);
                    API.LoadScene(nextSceneName);
                }
                else
                {
                    UpdateMenuPositions();
                }
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

        private void StartMenuScroll()
        {
            if (isCreditsFinished) return;
            isCreditsFinished = true;
            
            if (menuLayoutID != 0)
            {
                isMenuScrolling = true;
                currentMenuY = menuStartY;
                UpdateMenuPositions();
            }
            else
            {
                API.SetCutsceneMode(false);
                API.LoadScene(nextSceneName);
            }
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

        public void OnDestroy() => API.SetCutsceneMode(false);
    }
}
