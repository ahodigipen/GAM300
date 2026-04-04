using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attach this script to an entity that has a ParticleEmitterComponent.
    /// It follows the player each frame so floating dust particles are always
    /// visible around the camera, giving the whole scene an ambient dusty feel.
    /// All visual parameters are exposed in the Inspector for live tweaking.
    /// </summary>
    public class EnvironmentalDust
    {
        public ulong Entity;

        // ── Tracking ───────────────────────────────────────────────────────────

        [Boom.EditorExposed("Follow Player", "If true, the emitter follows the player so dust is always around the camera")]
        private bool _followPlayer = true;

        [Boom.EditorExposed("Height Offset", "Vertical offset above the player position", -5f, 15f, true)]
        private float _heightOffset = 3.0f;

        // ── Emission ───────────────────────────────────────────────────────────

        [Boom.EditorExposed("Emission Rate", "Dust particles emitted per second", 1f, 500f, true)]
        private float _emissionRate = 80f;

        // ── Movement ───────────────────────────────────────────────────────────

        [Boom.EditorExposed("Speed Min", "Minimum drift speed of dust motes", 0.01f, 5f, true)]
        private float _speedMin = 0.05f;

        [Boom.EditorExposed("Speed Max", "Maximum drift speed of dust motes", 0.01f, 5f, true)]
        private float _speedMax = 0.3f;

        [Boom.EditorExposed("Gravity", "Gravity on dust (small negative = gentle float up)", -2f, 2f, true)]
        private float _gravity = -0.02f;

        // ── Size ───────────────────────────────────────────────────────────────

        [Boom.EditorExposed("Start Size Min", "Smallest dust mote starting size", 0.001f, 0.5f, true)]
        private float _startSizeMin = 0.01f;

        [Boom.EditorExposed("Start Size Max", "Largest dust mote starting size", 0.001f, 0.5f, true)]
        private float _startSizeMax = 0.04f;

        [Boom.EditorExposed("End Size", "Size of each mote at end of life", 0.0f, 0.5f, true)]
        private float _endSize = 0.0f;

        // ── Colour ─────────────────────────────────────────────────────────────

        [Boom.EditorExposed("Start R", "Start colour red", 0f, 1f, true)]
        private float _startR = 0.85f;

        [Boom.EditorExposed("Start G", "Start colour green", 0f, 1f, true)]
        private float _startG = 0.78f;

        [Boom.EditorExposed("Start B", "Start colour blue", 0f, 1f, true)]
        private float _startB = 0.65f;

        [Boom.EditorExposed("Start A", "Start colour alpha (opacity)", 0f, 1f, true)]
        private float _startA = 0.35f;

        [Boom.EditorExposed("End R", "End colour red", 0f, 1f, true)]
        private float _endR = 0.75f;

        [Boom.EditorExposed("End G", "End colour green", 0f, 1f, true)]
        private float _endG = 0.70f;

        [Boom.EditorExposed("End B", "End colour blue", 0f, 1f, true)]
        private float _endB = 0.60f;

        [Boom.EditorExposed("End A", "End colour alpha (fades out)", 0f, 1f, true)]
        private float _endA = 0.0f;

        // ── Runtime ────────────────────────────────────────────────────────────

        private ulong _playerEntity = 0;
        private bool _configured = false;

        public void OnStart(string jsonParams)
        {
            if (!API.HasParticleEmitter(Entity))
            {
                API.Log("[EnvironmentalDust] ERROR: Entity has no ParticleEmitterComponent. Add one in the editor.");
                return;
            }

            _playerEntity = PlayerMovement.GetPlayerEntity();

            ConfigureParticles();
            API.PlayParticleEmitter(Entity);
            API.Log("[EnvironmentalDust] Dust emitter started.");
        }

        public void OnUpdate(float dt)
        {
            if (!API.HasParticleEmitter(Entity)) return;

            // Re-acquire the player entity if it wasn't available at start (scene load order)
            if (_playerEntity == 0)
            {
                _playerEntity = PlayerMovement.GetPlayerEntity();
                if (_playerEntity == 0) return;
            }

            // Reapply settings each frame so Inspector slider changes are reflected live
            ConfigureParticles();

            if (_followPlayer && API.HasTransform(_playerEntity) && API.HasTransform(Entity))
            {
                Vec3 playerPos = API.GetPosition(_playerEntity);
                Vec3 dustPos = new Vec3(playerPos.X, playerPos.Y + _heightOffset, playerPos.Z);
                API.SetPosition(Entity, dustPos);
            }

            // Make sure the emitter stays playing
            if (!API.IsParticleEmitterPlaying(Entity))
                API.PlayParticleEmitter(Entity);
        }

        public void OnDestroy()
        {
            if (API.HasParticleEmitter(Entity))
                API.StopParticleEmitter(Entity);
        }

        private void ConfigureParticles()
        {
            API.SetParticleStartColor(Entity, _startR, _startG, _startB, _startA);
            API.SetParticleEndColor(Entity, _endR, _endG, _endB, _endA);
            API.SetParticleEmissionRate(Entity, _emissionRate);
            API.SetParticleSpeed(Entity, _speedMin, _speedMax);
            API.SetParticleGravity(Entity, _gravity);
            API.SetParticleSize(Entity, _startSizeMin, _startSizeMax, _endSize);
        }
    }
}
