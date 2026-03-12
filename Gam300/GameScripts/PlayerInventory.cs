using Boom;

namespace GameScripts
{
    // Simple global inventory for keys.
    public static class PlayerInventory
    {
        private static int s_keyCount = 0;

        // Track specific key identifiers for unlocking doors (e.g. "key1", "boss_key")
        private static System.Collections.Generic.HashSet<string> s_keyTypes = new System.Collections.Generic.HashSet<string>();

        // Track key variants for UI counts (e.g. "MainDoor": 2, "SmallDoor": 1)
        private static System.Collections.Generic.Dictionary<string, int> s_keyVariants =
            new System.Collections.Generic.Dictionary<string, int>();

        // Map from KeyType identifier to KeyVariant so we know which UI count to decrement
        private static System.Collections.Generic.Dictionary<string, string> s_typeToVariant =
            new System.Collections.Generic.Dictionary<string, string>();

        // New: Track the order of item types in the player's inventory
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
            s_inventorySlots.Clear(); // Keep our new slots clean
            s_freezeChargeCount = 0; // Reset ability on game restart
            s_largeTokenPickupCount = 0;
            s_smallTokenPickupCount = 0;
            s_talismanPickupCount = 0;
            TutorialManager.Reset(); // Reset tutorial states
            API.Log("[PlayerInventory] Reset");
        }

        // --- Key Logic ---
        public static void AddKey(int count = 1)
        {
            if (count < 1) return;
            s_keyCount += count;
            API.Log($"[PlayerInventory] Keys: {s_keyCount}");
        }

        // Add a specific key with its type identifier and variant count
        public static void AddKey(string keyType, string keyVariant)
        {
            if (!string.IsNullOrEmpty(keyType))
            {
                s_keyTypes.Add(keyType);
                if (!string.IsNullOrEmpty(keyVariant))
                {
                    s_typeToVariant[keyType] = keyVariant;
                }
            }

            if (!string.IsNullOrEmpty(keyVariant))
            {
                if (s_keyVariants.ContainsKey(keyVariant))
                    s_keyVariants[keyVariant]++;
                else
                    s_keyVariants[keyVariant] = 1;

                if (!s_inventorySlots.Contains(keyVariant))
                {
                    s_inventorySlots.Add(keyVariant);
                }

                // Increment pickup count for tutorial tracking
                if (keyVariant == "MainDoor")
                    s_largeTokenPickupCount++;
                else if (keyVariant == "SmallDoor")
                    s_smallTokenPickupCount++;
            }

            s_keyCount++;
            API.Log($"[PlayerInventory] Added key '{keyType}' (Variant: {keyVariant}). Total keys: {s_keyCount}");
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

        // Consume a specific key identifier and decrement its corresponding variant count
        public static bool ConsumeKeyType(string keyType)
        {
            if (string.IsNullOrEmpty(keyType)) return ConsumeKey();
            if (!s_keyTypes.Contains(keyType)) return false;

            // Find and decrement the corresponding variant count
            if (s_typeToVariant.TryGetValue(keyType, out string variant))
            {
                if (s_keyVariants.ContainsKey(variant) && s_keyVariants[variant] > 0)
                {
                    s_keyVariants[variant]--;
                    
                    if (s_keyVariants[variant] == 0)
                    {
                        s_inventorySlots.Remove(variant);
                    }
                }
                s_typeToVariant.Remove(keyType);
            }

            s_keyTypes.Remove(keyType);
            s_keyCount--;
            API.Log($"[PlayerInventory] Consumed key type '{keyType}'. Keys left: {s_keyCount}");
            return true;
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
        public static int GetSmallTokenPickupCount() => s_smallTokenPickupCount;
        public static int GetTalismanPickupCount() => s_talismanPickupCount;

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