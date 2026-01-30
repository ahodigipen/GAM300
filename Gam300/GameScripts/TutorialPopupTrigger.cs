using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attach to a trigger zone where a tutorial popup should appear
    /// Shows a tutorial UI sprite when player enters
    /// Hides the tutorial UI when player exits
    /// Similar to CrouchTriggerZone
    /// </summary>
    public class TutorialPopupTrigger
    {
        public ulong Entity;

        private static readonly Dictionary<ulong, TutorialPopupTrigger> s_instances = new Dictionary<ulong, TutorialPopupTrigger>();

        // The sprite entity name to show/hide (e.g., "UI_L2_Tutorial")
        [Boom.EditorExposed("Tutorial Sprite", "Name of the sprite entity to show (e.g., UI_L2_Tutorial)")]
        private string _tutorialSpriteName = "UI_L2_Tutorial";

        // Optional: Play a sound when entering the zone
        [Boom.EditorExposed("Play Sound On Enter", "Whether to play a sound when player enters the tutorial zone")]
        private bool _playSoundOnEnter = false;

        [Boom.EditorExposed("Enter Sound", "Sound played when player enters the tutorial zone")]
        private string _enterSound = "Resources/Audio/ambient_notification.wav";

        private ulong _tutorialSprite = 0;

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;

            if (!API.HasCollider(Entity))
            {
                API.Log("[TutorialPopupTrigger] WARNING: Entity has no collider. Trigger will not work.");
                return;
            }

            // Ensure this collider is a trigger
            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log("[TutorialPopupTrigger] Collider set to IsTrigger = true.");
            }

            // Find the tutorial sprite
            _tutorialSprite = API.FindEntity(_tutorialSpriteName);
            if (_tutorialSprite == 0)
            {
                API.Log($"[TutorialPopupTrigger] WARNING: Could not find tutorial sprite '{_tutorialSpriteName}'");
            }
            else if (API.HasSprite(_tutorialSprite))
            {
                API.SetSpriteAlpha(_tutorialSprite, 0f);
                API.Log($"[TutorialPopupTrigger] Found and initialized tutorial sprite: {_tutorialSpriteName}");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log($"[TutorialPopupTrigger] Registered trigger callbacks for sprite: {_tutorialSpriteName}");
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
            TutorialPopupTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player enters this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // *** Show the tutorial popup via UIManager, passing this zone's entity ***
            if (inst._tutorialSprite != 0)
            {
                UIManager.ShowTutorialPopupZone(triggerEntity, inst._tutorialSprite);
            }

            // Optional: Play notification sound
            if (inst._playSoundOnEnter && API.HasTransform(inst.Entity))
            {
                var p = API.GetPosition(inst.Entity);
                API.PlaySoundAt("sfx_tutorial_enter", inst._enterSound, p, false);
                API.SetSoundVolume("sfx_tutorial_enter", 0.5f);
                API.Set3DMinMaxDistance("sfx_tutorial_enter", 1.0f, 12.0f);
            }

            API.Log($"[TutorialPopupTrigger] Player entered tutorial zone - showing sprite: {inst._tutorialSpriteName}");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            TutorialPopupTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player exits this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // *** Hide the tutorial popup from this zone via UIManager ***
            UIManager.HideTutorialPopupZone(triggerEntity);

            API.Log($"[TutorialPopupTrigger] Player exited tutorial zone - hiding sprite");
        }
    }
}
