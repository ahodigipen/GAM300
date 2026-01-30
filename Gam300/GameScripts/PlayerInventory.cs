using Boom;

namespace GameScripts
{
    // Simple global inventory for keys.
    public static class PlayerInventory
    {
        private static int s_keyCount = 0;

        // New: Track specific key types
        private static System.Collections.Generic.HashSet<string> s_keyTypes = new System.Collections.Generic.HashSet<string>();

        // New: Track if we are holding a freeze charge
        private static bool s_hasFreezeCharge = false;

        public static void Reset()
        {
            s_keyCount = 0;
            s_keyTypes.Clear();
            s_hasFreezeCharge = false; // Reset ability on game restart
            API.Log("[PlayerInventory] Reset");
        }

        // --- Key Logic ---
        public static void AddKey(int count = 1)
        {
            if (count < 1) return;
            s_keyCount += count;
            API.Log($"[PlayerInventory] Keys: {s_keyCount}");
        }

        // New: Add a specific key type
        public static void AddKey(string keyType)
        {
            if (string.IsNullOrEmpty(keyType)) return;
            s_keyTypes.Add(keyType);
            s_keyCount++;
            API.Log($"[PlayerInventory] Added key type '{keyType}'. Total keys: {s_keyCount}");
        }

        public static bool HasKey()
        {
            return s_keyCount > 0;
        }

        // New: Check if player has a specific key type
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

        // New: Consume a specific key type
        public static bool ConsumeKey(string keyType)
        {
            if (string.IsNullOrEmpty(keyType)) return ConsumeKey();
            if (!s_keyTypes.Contains(keyType)) return false;

            s_keyTypes.Remove(keyType);
            s_keyCount--;
            API.Log($"[PlayerInventory] Consumed key type '{keyType}'. Keys left: {s_keyCount}");
            return true;
        }

        public static int GetKeyCount() => s_keyCount;

        // --- Freeze Ability Logic ---

        public static bool HasFreezePower() => s_hasFreezeCharge;

        // Returns true if picked up successfully, false if full
        public static bool TryAddFreezeCharge()
        {
            if (s_hasFreezeCharge)
            {
                API.Log("[PlayerInventory] Cannot pick up Freeze: Already holding one!");
                return false;
            }

            s_hasFreezeCharge = true;
            API.Log("[PlayerInventory] Freeze Charge Acquired! Press F to use.");
            return true;
        }

        public static bool ConsumeFreezeCharge()
        {
            if (!s_hasFreezeCharge) return false;

            s_hasFreezeCharge = false;
            API.Log("[PlayerInventory] Freeze Charge used.");
            return true;
        }
    }
}