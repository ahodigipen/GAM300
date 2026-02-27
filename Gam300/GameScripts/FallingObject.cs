using System;
using Boom;

namespace GameScripts
{
    // FallingObject: placed above the player path. Starts falling when player walks toward it.
    public class FallingObject
    {
        public ulong Entity;

        [Boom.EditorExposed("Trigger Radius", "Player distance to start checking for fall", 0.5f, 50f, true)]
        private float _triggerRadius = 6.0f;

        [Boom.EditorExposed("Fall Speed", "Initial downward velocity when triggered", 0.1f, 50f, true)]
        private float _fallSpeed = 6.0f;

        [Boom.EditorExposed("Gravity Multiplier", "Extra gravity applied while falling", 0f, 50f, true)]
        private float _gravity = 9.8f;

        // Only trigger once
        private bool _hasFallen = false;

        // Track previous horizontal distance to player to detect approach (distance decreasing)
        private float _lastHorizontalDistance = float.MaxValue;

        // Small delay before enabling physics (makes object stable atop a parent)
        private float _enableTimer = 0f;
        private const float ENABLE_DELAY = 0.05f;

        public void OnStart(string json)
        {
            _hasFallen = false;
            _lastHorizontalDistance = float.MaxValue;
            _enableTimer = 0f;

            // Make sure entity exists and has transform
            if (!API.HasTransform(Entity)) return;

            // Ensure the object is not affected by physics until triggered (engine-specific)
            // We rely on SetLinearVelocity to move it when falling.
        }

        public void OnUpdate(float dt)
        {
            if (_hasFallen) return;

            _enableTimer += dt;
            if (_enableTimer < ENABLE_DELAY) return;

            ulong player = PlayerMovement.GetPlayerEntity();
            if (player == 0) return;

            Vec3 objPos = API.GetPosition(Entity);
            Vec3 playerPos = API.GetPosition(player);

            // Horizontal distance (X,Z) only
            float dx = playerPos.X - objPos.X;
            float dz = playerPos.Z - objPos.Z;
            float horizDist = (float)Math.Sqrt(dx * dx + dz * dz);

            // If player is moving toward the object (distance decreasing) and within trigger radius, trigger fall
            if (horizDist <= _triggerRadius && horizDist < _lastHorizontalDistance)
            {
                TriggerFall();
                return;
            }

            _lastHorizontalDistance = horizDist;
        }

        private void TriggerFall()
        {
            _hasFallen = true;

            // Give a downward velocity; if the engine uses gravity, velocity will be integrated.
            Vec3 vel = API.GetLinearVelocity(Entity);
            vel.Y = -Math.Abs(_fallSpeed);
            API.SetLinearVelocity(Entity, vel);

            // Optionally make this object a non-trigger so it collides with the player/ground
            if (API.HasCollider(Entity))
            {
                API.SetTrigger(Entity, false);
            }

            API.PlaySoundAt("falling_obj_" + Entity, "Resources/Audio/rock_fall.wav", API.GetPosition(Entity), false);
            API.SetSoundVolume("falling_obj_" + Entity, 1.0f);
            API.Set3DMinMaxDistance("falling_obj_" + Entity, 1.0f, 40.0f);

            // Optionally enable continuous gravity effect by applying extra downward velocity over time
            // but for simplicity rely on physics engine gravity. If not available, we could start a simple update coroutine.
        }

        public void OnDestroy()
        {
            // cleanup if needed
        }
    }
}
