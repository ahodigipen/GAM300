using Boom;
using System.Collections.Generic;

namespace GameScripts
{
    public static class PlayerManager
    {
        private static PlayerMovement s_playerInstance = null;

        // CHANGED: Made public so FreezeOverlayBehavior can read it
        // Renamed from s_activeEnemies to ActiveEnemies
        public static List<IEnemyController> ActiveEnemies = new List<IEnemyController>();

        public static void RegisterPlayer(PlayerMovement player)
        {
            s_playerInstance = player;
            API.Log("[PlayerManager] Player instance registered");
        }

        public static void UnregisterPlayer()
        {
            s_playerInstance = null;
            API.Log("[PlayerManager] Player instance unregistered");
        }

        // Returns the registered player instance (or null if not registered)
        public static PlayerMovement GetPlayer()
        {
            return s_playerInstance;
        }

        // NEW: Register enemy for reset on player respawn
        public static void RegisterEnemy(IEnemyController enemy)
        {
            if (!ActiveEnemies.Contains(enemy))
            {
                ActiveEnemies.Add(enemy);
            }
        }

        // NEW: Unregister enemy
        public static void UnregisterEnemy(IEnemyController enemy)
        {
            if (ActiveEnemies.Contains(enemy))
            {
                ActiveEnemies.Remove(enemy);
            }
        }

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

        // NEW: Notify all enemies that player has respawned
        public static void NotifyPlayerRespawned()
        {
            API.Log($"[PlayerManager] Notifying {ActiveEnemies.Count} enemies of player respawn");

            // Create a copy to avoid modification during iteration
            var enemiesCopy = new List<IEnemyController>(ActiveEnemies);

            foreach (var enemy in enemiesCopy)
            {
                enemy?.OnPlayerRespawned();
            }
        }

        public static bool HasPlayer()
        {
            return s_playerInstance != null;
        }
    }

    // Interface that all enemy controllers must implement
    public interface IEnemyController
    {
        void OnPlayerRespawned();
    }
}