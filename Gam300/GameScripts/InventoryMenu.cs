using Boom;

namespace GameScripts
{
    public class InventoryMenu
    {
        public ulong Entity;

        // Entity names
        private const string INVENTORY_BG_NAME   = "Inventory_BG";

        // Textures
        private const string MAINDOOR_TEX = "Resources/Textures/PlayerUI/Inventory_BigToken.png";
        private const string SMALLDOOR_TEX = "Resources/Textures/PlayerUI/Inventory_SmallToken.png";
        private const string FREEZE_TEX = "Resources/Textures/PlayerUI/Inventory_Talisman.png";

        // Cached handles
        private ulong _bgEntity = 0;
        private ulong[] _slotIcons = new ulong[5];
        private ulong[] _slotTexts = new ulong[5];

        public void OnStart(string jsonParams)
        {
            Entry.s_ActiveInventoryMenuInstance = this;

            _bgEntity = API.FindEntity(INVENTORY_BG_NAME);

            for (int i = 0; i < 5; i++)
            {
                int slotNum = i + 1;
                _slotIcons[i] = API.FindEntity($"Inventory_Slot{slotNum}_Icon");
                _slotTexts[i] = API.FindEntity($"Inventory_Slot{slotNum}_Text");
            }

            // Only hide at startup when we are loaded from a gameplay scene.
            if (Entry.IsGameplayScene(Entry._currentSceneName))
            {
                HideAll();
            }
        }

        public void OnUpdate(float dt)
        {
            // Do nothing outside of a gameplay scene (e.g. when editing the scene file directly)
            if (!Entry.IsGameplayScene(Entry._currentSceneName)) return;
            if (Entry.IsInventoryOpen)
            {
                SetSpriteAlpha(_bgEntity, 1.0f);

                for (int i = 0; i < 5; i++)
                {
                    if (i < PlayerInventory.s_inventorySlots.Count)
                    {
                        string itemType = PlayerInventory.s_inventorySlots[i];

                        // Show slot
                        SetSpriteAlpha(_slotIcons[i], 1.0f);
                        SetTextAlpha(_slotTexts[i], 1.0f);

                        if (itemType == "Freeze")
                        {
                            // Freeze stacks — show total charge count
                            API.SetSpriteTexture(_slotIcons[i], FREEZE_TEX);
                            SetText(_slotTexts[i], $"{PlayerInventory.GetFreezeChargeCount()}");
                        }
                        else
                        {
                            // itemType is a doorName — look up variant for icon and per-door count
                            string variant = PlayerInventory.GetDoorKeyVariant(itemType);
                            if (variant == "MainDoor")
                            {
                                API.SetSpriteTexture(_slotIcons[i], MAINDOOR_TEX);
                            }
                            else if (variant == "SmallDoor")
                            {
                                API.SetSpriteTexture(_slotIcons[i], SMALLDOOR_TEX);
                            }
                            SetText(_slotTexts[i], $"{PlayerInventory.GetDoorKeyCount(itemType)}");
                        }
                    }
                    else
                    {
                        // Hide empty slot
                        SetSpriteAlpha(_slotIcons[i], 0.0f);
                        SetTextAlpha(_slotTexts[i], 0.0f);
                    }
                }
            }
            else
            {
                HideAll();
            }
        }

        private void HideAll()
        {
            SetSpriteAlpha(_bgEntity, 0.0f);

            for (int i = 0; i < 5; i++)
            {
                SetSpriteAlpha(_slotIcons[i], 0.0f);
                SetTextAlpha(_slotTexts[i], 0.0f);
            }
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
