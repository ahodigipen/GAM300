using Boom;

namespace GameScripts
{
    // Simple global inventory for keys.
    public static class PlayerInventory
    {
        private static int s_keyCount = 0;

        // Track specific key identifiers for unlocking doors (e.g. "key1", "boss_key")
        private static System.Collections.Generic.HashSet<string> s_keyTypes = new System.Collections.Generic.HashSet<string>();

        // Track key variants for total counts (used by door missing-key dialogue logic)
        private static System.Collections.Generic.Dictionary<string, int> s_keyVariants =
            new System.Collections.Generic.Dictionary<string, int>();

        // Map from keyType → keyVariant (for consume logic)
        private static System.Collections.Generic.Dictionary<string, string> s_typeToVariant =
            new System.Collections.Generic.Dictionary<string, string>();

        // Map from keyType → doorName (so ConsumeKeyType knows which door slot to decrement)
        private static System.Collections.Generic.Dictionary<string, string> s_typeToDoor =
            new System.Collections.Generic.Dictionary<string, string>();

        // Per-door key count  (doorName → how many keys the player holds for this door)
        private static System.Collections.Generic.Dictionary<string, int> s_doorKeyCount =
            new System.Collections.Generic.Dictionary<string, int>();

        // Per-door variant   (doorName → "MainDoor" / "SmallDoor") for InventoryMenu icon lookup
        private static System.Collections.Generic.Dictionary<string, string> s_doorKeyVariant =
            new System.Collections.Generic.Dictionary<string, string>();

        // Ordered inventory slots — stores doorName for key slots, "Freeze" for freeze
        public static System.Collections.Generic.List<string> s_inventorySlots = new System.Collections.Generic.List<string>();

        // Track the count of freeze charges
        private static int s_freezeChargeCount = 0;

        // Pickup counts for tutorial system (tracks total lifetime pickups per item type)
        private static int s_largeTokenPickupCount = 0;
        private static int s_smallTokenPickupCount = 0;
        private static int s_talismanPickupCount = 0;

        public static void Reset()
        {
            s_keyCount = 0;
            s_keyTypes.Clear();
            s_keyVariants.Clear();
            s_typeToVariant.Clear();
            s_typeToDoor.Clear();
            s_doorKeyCount.Clear();
            s_doorKeyVariant.Clear();
            s_inventorySlots.Clear();
            s_freezeChargeCount = 0;
            s_largeTokenPickupCount = 0;
            s_smallTokenPickupCount = 0;
            s_talismanPickupCount = 0;
            TutorialManager.Reset();
            API.Log("[PlayerInventory] Reset");
        }

        // --- Key Logic ---
        public static void AddKey(int count = 1)
        {
            if (count < 1) return;
            s_keyCount += count;
            API.Log($"[PlayerInventory] Keys: {s_keyCount}");
        }

        // Add a specific key with its type identifier, variant and target door name.
        // doorName matches MultiKeyDoor._doorName and is used as the inventory slot identifier.
        public static void AddKey(string keyType, string keyVariant, string doorName)
        {
            if (!string.IsNullOrEmpty(keyType))
            {
                s_keyTypes.Add(keyType);
                if (!string.IsNullOrEmpty(keyVariant))
                    s_typeToVariant[keyType] = keyVariant;
                if (!string.IsNullOrEmpty(doorName))
                    s_typeToDoor[keyType] = doorName;
            }

            if (!string.IsNullOrEmpty(keyVariant))
            {
                if (s_keyVariants.ContainsKey(keyVariant))
                    s_keyVariants[keyVariant]++;
                else
                    s_keyVariants[keyVariant] = 1;

                // Increment pickup count for tutorial tracking
                if (keyVariant == "MainDoor")
                    s_largeTokenPickupCount++;
                else if (keyVariant == "SmallDoor")
                    s_smallTokenPickupCount++;
            }

            // Use doorName as the slot identifier so all keys for the same door stack together
            string slotId = !string.IsNullOrEmpty(doorName) ? doorName : keyType;
            if (!s_doorKeyCount.ContainsKey(slotId))
            {
                s_doorKeyCount[slotId] = 0;
                s_inventorySlots.Add(slotId);
            }
            s_doorKeyCount[slotId]++;

            // Remember the variant for this slot (for InventoryMenu icon lookup)
            if (!string.IsNullOrEmpty(keyVariant))
                s_doorKeyVariant[slotId] = keyVariant;

            s_keyCount++;
            API.Log($"[PlayerInventory] Added key '{keyType}' (Variant: {keyVariant}, Door: {slotId}). Count for door: {s_doorKeyCount[slotId]}. Total keys: {s_keyCount}");
        }

        // Overload without doorName — falls back to keyType as slot identifier (backward compat)
        public static void AddKey(string keyType, string keyVariant)
        {
            AddKey(keyType, keyVariant, keyType);
        }

        public static bool HasKey()
        {
            return s_keyCount > 0;
        }

        // Check if player has a specific key identifier (for opening doors)
        public static bool HasKey(string keyType)
        {
            if (string.IsNullOrEmpty(keyType)) return HasKey();
            return s_keyTypes.Contains(keyType);
        }

        // Returns true if a key was consumed.
        public static bool ConsumeKey()
        {
            if (s_keyCount <= 0) return false;
            s_keyCount--;
            API.Log($"[PlayerInventory] Consumed key. Keys left: {s_keyCount}");
            return true;
        }

        // Consume a specific key identifier and decrement its door slot count
        public static bool ConsumeKeyType(string keyType)
        {
            if (string.IsNullOrEmpty(keyType)) return ConsumeKey();
            if (!s_keyTypes.Contains(keyType)) return false;

            // Decrement variant count
            if (s_typeToVariant.TryGetValue(keyType, out string variant))
            {
                if (s_keyVariants.ContainsKey(variant) && s_keyVariants[variant] > 0)
                    s_keyVariants[variant]--;
                s_typeToVariant.Remove(keyType);
            }

            // Decrement door slot count and remove slot if empty
            string slotId;
            if (!s_typeToDoor.TryGetValue(keyType, out slotId))
                slotId = keyType; // fallback

            if (s_doorKeyCount.ContainsKey(slotId))
            {
                s_doorKeyCount[slotId]--;
                if (s_doorKeyCount[slotId] <= 0)
                {
                    s_doorKeyCount.Remove(slotId);
                    s_doorKeyVariant.Remove(slotId);
                    s_inventorySlots.Remove(slotId);
                }
            }

            s_typeToDoor.Remove(keyType);
            s_keyTypes.Remove(keyType);
            s_keyCount--;
            API.Log($"[PlayerInventory] Consumed key '{keyType}' (door slot: {slotId}). Keys left: {s_keyCount}");
            return true;
        }

        // Returns the variant ("MainDoor" / "SmallDoor") for a given door slot, for InventoryMenu.
        public static string GetDoorKeyVariant(string doorName)
        {
            if (string.IsNullOrEmpty(doorName)) return null;
            return s_doorKeyVariant.TryGetValue(doorName, out string v) ? v : null;
        }

        // Returns how many keys the player holds for a specific door slot.
        public static int GetDoorKeyCount(string doorName)
        {
            if (string.IsNullOrEmpty(doorName)) return 0;
            return s_doorKeyCount.TryGetValue(doorName, out int c) ? c : 0;
        }

        public static int GetKeyCount() => s_keyCount;

        // Returns count of a specific key variant for UI (0 if none held)
        public static int GetKeyCount(string keyVariant)
        {
            if (string.IsNullOrEmpty(keyVariant)) return s_keyCount;
            return s_keyVariants.TryGetValue(keyVariant, out int count) ? count : 0;
        }

        // --- Tutorial Pickup Count Getters ---
        public static int GetLargeTokenPickupCount() => s_largeTokenPickupCount;
        public static void SetLargeTokenPickupCount(int count) => s_largeTokenPickupCount = count;
        public static int GetSmallTokenPickupCount() => s_smallTokenPickupCount;
        public static void SetSmallTokenPickupCount(int count) => s_smallTokenPickupCount = count;
        public static int GetTalismanPickupCount() => s_talismanPickupCount;
        public static void SetTalismanPickupCount(int count) => s_talismanPickupCount = count;

        // Returns a snapshot of currently held key identifiers.
        public static string[] GetKeyTypes()
        {
            var arr = new string[s_keyTypes.Count];
            s_keyTypes.CopyTo(arr);
            return arr;
        }

        // --- Freeze Ability Logic ---

        public static bool HasFreezePower() => s_freezeChargeCount > 0;
        public static int GetFreezeChargeCount() => s_freezeChargeCount;

        // Returns true if picked up successfully, false if full
        public static bool TryAddFreezeCharge()
        {

            s_freezeChargeCount++;
            s_talismanPickupCount++;
            if (!s_inventorySlots.Contains("Freeze"))
            {
                s_inventorySlots.Add("Freeze");
            }
            API.Log($"[PlayerInventory] Freeze Charge Acquired! Count: {s_freezeChargeCount}. Press E to use.");
            return true;
        }

        public static bool ConsumeFreezeCharge()
        {
            if (s_freezeChargeCount <= 0) return false;

            s_freezeChargeCount--;
            if (s_freezeChargeCount == 0)
            {
                s_inventorySlots.Remove("Freeze");
            }
            API.Log($"[PlayerInventory] Freeze Charge used. Left: {s_freezeChargeCount}");
            return true;
        }
    }
}