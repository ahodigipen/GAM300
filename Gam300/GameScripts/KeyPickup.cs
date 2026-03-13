using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    // Attach to a key entity with a Collider marked as IsTrigger.
    // When the player enters, grant a key and hide/disable this entity.
    public class KeyPickup
    {
        public ulong Entity;

        // Key type identifier (e.g., "key1", "key2", "red_key", "blue_key")
        [Boom.EditorExposed("Key Type", "Unique identifier for this key (e.g., 'key1', 'key2')")]
        private string _keyType = "key1";

        // Which specific door this opens — dropdown restricted to valid types
        [Boom.EditorExposed("Key Variant", "Which door this key opens",
            options: new[] { "MainDoor", "SmallDoor" })]
        private string _keyVariant = "MainDoor";

        // Optional: sound to play on pickup
        [Boom.EditorExposed("Pickup Sound", "Sound played when the key is collected")]
        private string _pickupSound = "Resources/Audio/pickup.wav";

        private static readonly Dictionary<ulong, KeyPickup> s_instances = new Dictionary<ulong, KeyPickup>();
        private bool _collected = false;
        private bool _playerInRange = false;

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;

            if (!API.HasCollider(Entity))
            {
                API.Log("[KeyPickup] WARNING: Entity has no collider. Pickup will not trigger.");
                return;
            }

            // Ensure this collider is a trigger
            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log("[KeyPickup] Collider set to IsTrigger = true.");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log("[KeyPickup] Registered trigger callbacks.");
        }

        public void OnUpdate(float dt)
        {
            if (_collected) return;

            if (_playerInRange)
            {
                Collect();
            }
        }

        private void Collect()
        {
            // Don't allow pickup if any popup/tutorial is active (prevents UI overlap)
            if (TutorialPopupTrigger.IsPopupActive() || TutorialManager.IsTutorialActive())
            {
                return;
            }

            _collected = true;
            PlayerInventory.AddKey(_keyType, _keyVariant);

            // Show pickup tutorial (first-time or repeat) based on key variant
            TutorialManager.ItemType itemType = (_keyVariant == "SmallDoor")
                ? TutorialManager.ItemType.SmallToken
                : TutorialManager.ItemType.LargeToken;
            int pickupCount = (_keyVariant == "SmallDoor")
                ? PlayerInventory.GetSmallTokenPickupCount()
                : PlayerInventory.GetLargeTokenPickupCount();
            TutorialManager.ShowPickupTutorial(itemType, pickupCount);

            // Play pickup SFX at key's position
            if (API.HasTransform(Entity))
            {
                var p = API.GetPosition(Entity);
                API.PlaySoundAt("sfx_key_pickup", _pickupSound, p, false);
                API.SetSoundVolume("sfx_key_pickup", 0.9f);
            }

            // "Destroy" key: unregister callbacks and teleport it far below the map
            API.UnregisterTriggerCallbacks(Entity);

            // Teleport key to bottom of map (far below Y = -100)
            var currentPos = API.GetPosition(Entity);
            API.SetPosition(Entity, new Vec3(currentPos.X, -100f, currentPos.Z));

            API.Log($"[KeyPickup] {_keyType} '{_keyVariant}' collected! Total keys: {PlayerInventory.GetKeyCount()}");
        }

        public void OnDestroy()
        {
            // Cleanup
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            KeyPickup inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            if (inst._collected) return;

            // Only react when the player enters this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInRange = true;
            API.Log("[KeyPickup] Player in range. Automatic pickup triggered.");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            KeyPickup inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInRange = false;
        }
    }
}