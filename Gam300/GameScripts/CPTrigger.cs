using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// CPTrigger updates the player's spawn/respawn location when they press Q inside this trigger zone.
    /// Place this script on a trigger collider entity in your scene.
    /// </summary>
    public class CPTrigger
    {
        public ulong Entity;

        // Optional: Offset from trigger position for spawn point (e.g., to spawn slightly above ground)
        [Boom.EditorExposed("Spawn Offset Y", "Vertical offset for spawn position")]
        private float _spawnOffsetY = 1.5f;

        // Forward offset distance
        [Boom.EditorExposed("Forward Offset", "Distance in front of checkpoint to spawn")]
        private float _forwardOffset = 5.0f;

        // Track if checkpoint was already activated
        private bool _activated = false;

        // Track if player is inside the trigger zone
        private bool _playerInZone = false;

        // Track previous key state to detect press (not hold)
        private bool _wasQPressed = false;

        // Static instance tracking
        private static readonly Dictionary<ulong, CPTrigger> s_instances = new Dictionary<ulong, CPTrigger>();

        public void OnStart(string jsonParams)
        {
            // Register this instance
            s_instances[Entity] = this;

            // Initialize text as hidden if it exists on this entity
            if (API.HasText(Entity))
            {
                Vec4 color = API.GetTextColor(Entity);
                color.W = 0.0f;
                API.SetTextColor(Entity, color);
            }

            // Ensure trigger is configured
            if (!API.HasCollider(Entity))
            {
                API.Log("[CPTrigger] WARNING: Entity has no collider!");
                return;
            }

            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
            }

            // Register callbacks
            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnterCallback);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExitCallback);
            API.Log("[CPTrigger] Registered trigger callbacks. Press Q to activate checkpoint.");
        }

        public void OnUpdate(float dt)
        {
            // Only check for Q key if player is in zone and checkpoint not yet activated
            if (!_playerInZone || _activated) return;

            bool isQPressed = API.IsKeyDown(API.KEY_Q);
            bool justPressed = isQPressed && !_wasQPressed;
            _wasQPressed = isQPressed;

            if (justPressed)
            {
                ActivateCheckpoint();
            }
        }

        private void ActivateCheckpoint()
        {
            // Get the trigger's position and rotation
            Vec3 checkpointPos = API.GetPosition(Entity);
            Vec3 checkpointRot = API.GetRotation(Entity);

            // Calculate forward direction from Y rotation (yaw)
            float yawRad = checkpointRot.Y * (float)Math.PI / 180f;
            float forwardX = (float)Math.Sin(yawRad);
            float forwardZ = (float)Math.Cos(yawRad);

            // Apply forward offset (5 units in front)
            Vec3 spawnPos = new Vec3(
                checkpointPos.X - forwardX * _forwardOffset,
                checkpointPos.Y + _spawnOffsetY,
                checkpointPos.Z - forwardZ * _forwardOffset
            );

            // Update the player's checkpoint
            PlayerMovement player = PlayerManager.GetPlayer();
            if (player != null)
            {
                player.UpdateCheckpoint(spawnPos);
                _activated = true;

                // Hide text permanently if it exists on this entity
                if (API.HasText(Entity))
                {
                    Vec4 color = API.GetTextColor(Entity);
                    color.W = 0.0f;
                    API.SetTextColor(Entity, color);
                }

                API.Log($"[CPTrigger] Checkpoint saved at ({spawnPos.X:F2}, {spawnPos.Y:F2}, {spawnPos.Z:F2})");
            }
            else
            {
                API.Log("[CPTrigger] ERROR: Could not find player instance!");
            }
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity))
                s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnterCallback(ulong triggerEntity, ulong otherEntity)
        {
            CPTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only player triggers this
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInZone = true;

            if (!inst._activated)
            {
                // Show text if it exists on this entity
                if (API.HasText(inst.Entity))
                {
                    Vec4 color = API.GetTextColor(inst.Entity);
                    color.W = 1.0f;
                    API.SetTextColor(inst.Entity, color);
                }
                API.Log("[CPTrigger] Player entered checkpoint zone. Press Q to save checkpoint.");
            }
        }

        private static void OnTriggerExitCallback(ulong triggerEntity, ulong otherEntity)
        {
            CPTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only player triggers this
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInZone = false;

            // Hide text if it exists on this entity
            if (API.HasText(inst.Entity))
            {
                Vec4 color = API.GetTextColor(inst.Entity);
                color.W = 0.0f;
                API.SetTextColor(inst.Entity, color);
            }

            API.Log("[CPTrigger] Player left checkpoint zone.");
        }
    }
}