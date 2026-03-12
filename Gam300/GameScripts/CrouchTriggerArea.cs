using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>                                                                                               
    /// Attach to a trigger zone where the player needs to crouch to pass
    /// Shows "Hold to Crouch" UI prompt when player enters
    /// Hides the prompt when player exits
    /// </summary>
    public class CrouchTriggerArea
    {
        public ulong Entity;

        private static readonly Dictionary<ulong, CrouchTriggerArea> s_instances = new Dictionary<ulong, CrouchTriggerArea>();

        // Optional: Play a sound when entering the zone
        [Boom.EditorExposed("Play Sound On Enter", "Whether to play a sound when player enters the zone")]
        private bool _playSoundOnEnter = false;

        [Boom.EditorExposed("Enter Sound", "Sound played when player enters the crouch zone")]
        private string _enterSound = "Resources/Audio/ambient_warning.wav";

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;

            if (!API.HasCollider(Entity))
            {
                API.Log("[CrouchTriggerArea] WARNING: Entity has no collider. Trigger will not work.");
                return;
            }

            // Ensure this collider is a trigger
            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log("[CrouchTriggerArea] Collider set to IsTrigger = true.");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log($"[CrouchTriggerArea] Registered trigger callbacks for entity {Entity}.");
        }

        public void OnUpdate(float dt)
        {
            // No-op
        }

        public void OnDestroy()
        {
            // Cleanup
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            CrouchTriggerArea inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player enters this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // *** CRITICAL: Notify PlayerMovement that we're in a crouch zone ***
            PlayerMovement.SetInCrouchZone(true);

            // *** Show the "Hold to Crouch" UI prompt ***
            UIManager.ShowHoldPrompt();

            // Optional: Play warning sound
            if (inst._playSoundOnEnter && API.HasTransform(inst.Entity))
            {
                var p = API.GetPosition(inst.Entity);
                API.PlaySoundAt("sfx_crouch_zone_enter", inst._enterSound, p, false);
                API.SetSoundVolume("sfx_crouch_zone_enter", 0.5f);
                API.Set3DMinMaxDistance("sfx_crouch_zone_enter", 1.0f, 12.0f);
            }

            API.Log("[CrouchTriggerArea] Player entered crouch zone - showing UI prompt + notifying PlayerMovement");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            CrouchTriggerArea inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player exits this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // *** CRITICAL: Notify PlayerMovement that we left the crouch zone ***
            PlayerMovement.SetInCrouchZone(false);

            // *** Hide the "Hold to Crouch" UI prompt ***
            UIManager.HideHoldPrompt();

            API.Log("[CrouchTriggerArea] Player exited crouch zone - hiding UI prompt + notifying PlayerMovement");
        }
    }
}