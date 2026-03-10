using Boom;

namespace GameScripts
{
    public class InventoryMenu
    {
        public ulong Entity;

        // Entity names
        private const string INVENTORY_BG_NAME   = "Inventory_BG";
        private const string MAINDOOR_ICON_NAME  = "Inventory_MainDoorIcon";
        private const string MAINDOOR_TEXT_NAME  = "Inventory_MainDoorKey";
        private const string SMALLDOOR_ICON_NAME = "Inventory_SmallDoorIcon";
        private const string SMALLDOOR_TEXT_NAME = "Inventory_SmallDoorKey";

        // Freeze row
        private const string FREEZE_ICON_NAME       = "Inventory_FreezeIcon";
        private const string FREEZE_TEXT_NAME       = "Inventory_FreezeStatus";
        private const string FREEZE_TEX_AVAILABLE   = "Resources/Textures/PlayerUI/UI_Freeze_Available.png";
        private const string FREEZE_TEX_UNAVAILABLE = "Resources/Textures/PlayerUI/UI_Freeze_Unavailable.png";

        // Cached handles
        private ulong _bgEntity        = 0;
        private ulong _mainDoorIcon    = 0;
        private ulong _mainDoorText    = 0;
        private ulong _smallDoorIcon   = 0;
        private ulong _smallDoorText   = 0;
        private ulong _freezeIcon      = 0;
        private ulong _freezeText      = 0;

        private string _currentFreezeTexture = "";

        public void OnStart(string jsonParams)
        {
            Entry.s_ActiveInventoryMenuInstance = this;

            _bgEntity        = API.FindEntity(INVENTORY_BG_NAME);
            _mainDoorIcon    = API.FindEntity(MAINDOOR_ICON_NAME);
            _mainDoorText    = API.FindEntity(MAINDOOR_TEXT_NAME);
            _smallDoorIcon   = API.FindEntity(SMALLDOOR_ICON_NAME);
            _smallDoorText   = API.FindEntity(SMALLDOOR_TEXT_NAME);
            _freezeIcon      = API.FindEntity(FREEZE_ICON_NAME);
            _freezeText      = API.FindEntity(FREEZE_TEXT_NAME);

            // Only hide at startup when we are loaded from the gameplay scene.
            // When opening InventoryMenu.yaml directly in the editor,
            // Entry._currentSceneName is null so we skip this and preserve edit-time alpha.
            if (Entry._currentSceneName == Entry.GAMEPLAY_SCENE_NAME)
            {
                HideAll();
            }
        }

        public void OnUpdate(float dt)
        {
            // Do nothing outside of the gameplay scene (e.g. when editing the scene file directly)
            if (Entry._currentSceneName != Entry.GAMEPLAY_SCENE_NAME) return;
            if (Entry.IsInventoryOpen)
            {
                // Show everything
                SetSpriteAlpha(_bgEntity,      1.0f);
                SetSpriteAlpha(_mainDoorIcon,  1.0f);
                SetSpriteAlpha(_smallDoorIcon, 1.0f);
                SetSpriteAlpha(_freezeIcon,    1.0f);
                SetTextAlpha(_mainDoorText,    1.0f);
                SetTextAlpha(_smallDoorText,   1.0f);
                SetTextAlpha(_freezeText,      1.0f);

                // Update text content with counts per type
                int mainCount  = PlayerInventory.GetKeyCount("MainDoor");
                int smallCount = PlayerInventory.GetKeyCount("SmallDoor");

                SetText(_mainDoorText,  $"{mainCount}");
                SetText(_smallDoorText, $"{smallCount}");

                // Dim icons when count is 0
                SetSpriteAlpha(_mainDoorIcon,  mainCount  > 0 ? 1.0f : 0.35f);
                SetSpriteAlpha(_smallDoorIcon, smallCount > 0 ? 1.0f : 0.35f);

                // Freeze row — alpha always 1; swap texture based on collection state
                bool hasFreeze = PlayerInventory.HasFreezePower();
                SetText(_freezeText, hasFreeze ? "1" : "0");
                
                string targetTexture = hasFreeze ? FREEZE_TEX_AVAILABLE : FREEZE_TEX_UNAVAILABLE;
                if (_currentFreezeTexture != targetTexture)
                {
                    API.SetSpriteTexture(_freezeIcon, targetTexture);
                    _currentFreezeTexture = targetTexture;
                }
                
                SetSpriteAlpha(_freezeIcon, 1.0f);
            }
            else
            {
                HideAll();
            }
        }

        private void HideAll()
        {
            SetSpriteAlpha(_bgEntity,      0.0f);
            SetSpriteAlpha(_mainDoorIcon,  0.0f);
            SetSpriteAlpha(_smallDoorIcon, 0.0f);
            SetSpriteAlpha(_freezeIcon,    0.0f);
            SetTextAlpha(_mainDoorText,    0.0f);
            SetTextAlpha(_smallDoorText,   0.0f);
            SetTextAlpha(_freezeText,      0.0f);
        }

        // Helpers
        private static void SetText(ulong e, string text)
        {
            if (e != 0) API.SetText(e, text);
        }

        private static void SetTextAlpha(ulong e, float a)
        {
            if (e != 0)
            {
                Vec4 c = API.GetTextColor(e);
                c.W = a;
                API.SetTextColor(e, c);
            }
        }

        private static void SetSpriteAlpha(ulong e, float a)
        {
            if (e != 0) API.SetSpriteAlpha(e, a);
        }
    }
}
