using Boom;

namespace GameScripts
{
    /// <summary>
    /// Singleton manager to handle player instance access for enemy interactions
    /// </summary>
    public static class PlayerManager
    {
        private static PlayerMovement s_playerInstance = null;

        /// <summary>
        /// Register the player instance (called from PlayerMovement.OnStart)
        /// </summary>
        public static void RegisterPlayer(PlayerMovement player)
        {
            s_playerInstance = player;
            API.Log("[PlayerManager] Player instance registered");
        }

        /// <summary>
        /// Unregister the player instance (called from PlayerMovement.OnDestroy)
        /// </summary>
        public static void UnregisterPlayer()
        {
            s_playerInstance = null;
            API.Log("[PlayerManager] Player instance unregistered");
        }

        /// <summary>
        /// Notify the player they were caught by an enemy
        /// </summary>
        public static void NotifyPlayerCaught(ulong enemyEntity)
        {
            if (s_playerInstance != null)
            {
                API.Log($"[PlayerManager] Notifying player of enemy detection (Enemy ID: {enemyEntity})");
                s_playerInstance.OnCaughtByEnemy(enemyEntity);
            }
            else
            {
                API.Log("[PlayerManager] WARNING: No player instance registered!");
            }
        }

        /// <summary>
        /// Check if player instance exists
        /// </summary>
        public static bool HasPlayer()
        {
            return s_playerInstance != null;
        }
    }
}