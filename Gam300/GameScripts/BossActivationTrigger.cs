using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attach to a trigger entity. When the player enters, it activates the specified BossHideSeekController.
    /// </summary>
    public class BossActivationTrigger
    {
        public ulong Entity;

        [Boom.EditorExposed("Boss Entity Name", "Name of the boss entity to activate")]
        private string _bossEntityName = "Boss";

        [Boom.EditorExposed("Only Trigger Once", "Whether this trigger should only work once")]
        private bool _onlyTriggerOnce = true;

        private bool _hasTriggered = false;

        public void OnStart(string jsonParams)
        {
            if (!API.HasCollider(Entity))
            {
                API.Log("[BossActivationTrigger] WARNING: Entity has no collider.");
                return;
            }

            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
        }

        private void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            if (_onlyTriggerOnce && _hasTriggered) return;

            // Only react to the player
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            ulong bossID = API.FindEntity(_bossEntityName);
            if (bossID != 0)
            {
                BossHideSeekController.Activate(bossID);
                _hasTriggered = true;
                API.Log($"[BossActivationTrigger] Activated boss: {_bossEntityName}");
            }
            else
            {
                API.Log($"[BossActivationTrigger] ERROR: Could not find boss entity '{_bossEntityName}'");
            }
        }

        public void OnDestroy()
        {
            API.UnregisterTriggerCallbacks(Entity);
        }
    }
}
