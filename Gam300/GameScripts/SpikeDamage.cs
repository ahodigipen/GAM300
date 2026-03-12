using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attach to spikes or any hazard. 
    /// When the player enters the trigger, they take damage or die.
    /// </summary>
    public class SpikeDamage
    {
        public ulong Entity;

        [Boom.EditorExposed("Instant Kill", "If true, player dies immediately regardless of health. If false, player loses 1 health.")]
        private bool _instantKill = false;

        [Boom.EditorExposed("Damage Amount", "Amount of health to subtract (ignored if Instant Kill is true)")]
        private int _damageAmount = 1;

        private static readonly Dictionary<ulong, SpikeDamage> s_instances = new Dictionary<ulong, SpikeDamage>();

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;

            if (!API.HasCollider(Entity))
            {
                API.Log("[SpikeDamage] WARNING: Entity has no collider. Trigger will not work.");
                return;
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.Log($"[SpikeDamage] Initialized on Entity {Entity}. InstantKill: {_instantKill}");
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            SpikeDamage inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player enters this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            API.Log("[SpikeDamage] Player collided with spikes!");

            if (inst._instantKill)
            {
                API.Log("[SpikeDamage] Instant kill triggered.");
                // Update HUD so hearts disappear immediately
                HUD.SetHealth(0, 5);
                Entry.TriggerPlayerDeath();
            }
            else
            {
                // Use the existing player damage system
                // NotifyPlayerCaught handles health decrement, sound, and respawn/death menu transition
                for (int i = 0; i < inst._damageAmount; i++)
                {
                    PlayerManager.NotifyPlayerCaught(triggerEntity);
                }
            }
        }
    }
}
