using Boom;

namespace GameScripts
{
    /// <summary>
    /// Place this script on an entity to mark it as the player's spawn point for the scene.
    /// Named "PlayerSpawn" by default to be easily found by PlayerMovement.
    /// </summary>
    public class SpawnPoint
    {
        public ulong Entity;

        public void OnStart(string jsonParams)
        {
            // Apply editor parameters if any
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);
            
            API.Log($"[SpawnPoint] Registered at {API.GetPosition(Entity)}");
        }

        public void OnUpdate(float dt)
        {
            // Nothing to do per frame
        }

        public void OnDestroy()
        {
            // Nothing to cleanup
        }
    }
}
