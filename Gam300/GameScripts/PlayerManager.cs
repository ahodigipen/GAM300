using Boom;
using System.Collections.Generic;

namespace GameScripts
{
    public static class PlayerManager
    {
        private static PlayerMovement s_playerInstance = null;

        // NEW: Track all active enemies
        private static List<IEnemyController> s_activeEnemies = new List<IEnemyController>();

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

        // NEW: Register enemy for reset on player respawn
        public static void RegisterEnemy(IEnemyController enemy)
        {
            if (!s_activeEnemies.Contains(enemy))
            {
                s_activeEnemies.Add(enemy);
            }
        }

        // NEW: Unregister enemy
        public static void UnregisterEnemy(IEnemyController enemy)
        {
            s_activeEnemies.Remove(enemy);
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
            API.Log($"[PlayerManager] Notifying {s_activeEnemies.Count} enemies of player respawn");

            // Create a copy to avoid modification during iteration
            var enemiesCopy = new List<IEnemyController>(s_activeEnemies);

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

    // NEW: Interface that all enemy controllers must implement
    public interface IEnemyController
    {
        void OnPlayerRespawned();
    }
}