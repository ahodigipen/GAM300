using Boom;

namespace GameScripts
{
    // Simple global inventory for keys.
    public static class PlayerInventory
    {
        private static int s_keyCount = 0;

        // New: Track if we are holding a freeze charge
        private static bool s_hasFreezeCharge = false;

        public static void Reset()
        {
            s_keyCount = 0;
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

        public static bool HasKey()
        {
            return s_keyCount > 0;
        }

        // Returns true if a key was consumed.
        public static bool ConsumeKey()
        {
            if (s_keyCount <= 0) return false;
            s_keyCount--;
            API.Log($"[PlayerInventory] Consumed key. Keys left: {s_keyCount}");
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