using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attach to the "Trigger box LionStatue_6" entity.
    /// When the player enters the trigger, the BOSS entity begins a death sequence:
    ///   1. Particles burst from the boss (dissolve effect)
    ///   2. Boss model fades out via opacity over time
    ///   3. Boss light dims to zero
    ///   4. Boss is destroyed after the sequence completes
    /// </summary>
    public class BossDeathTrigger
    {
        public ulong Entity;

        [Boom.EditorExposed("Boss Entity Name", "Name of the BOSS entity in the scene")]
        private string _bossName = "BOSS";

        [Boom.EditorExposed("Fade Duration", "Seconds for the boss to fully fade out")]
        private float _fadeDuration = 3.0f;

        [Boom.EditorExposed("Particle Duration", "How long particles emit before stopping")]
        private float _particleDuration = 2.5f;

        [Boom.EditorExposed("Particle Rate", "Particles emitted per second during death")]
        private float _particleRate = 80.0f;

        [Boom.EditorExposed("Particle Speed Min", "Minimum particle speed")]
        private float _particleSpeedMin = 2.0f;

        [Boom.EditorExposed("Particle Speed Max", "Maximum particle speed")]
        private float _particleSpeedMax = 6.0f;

        [Boom.EditorExposed("Destroy After Fade", "If true, destroy the boss entity after fading")]
        private bool _destroyAfterFade = true;

        [Boom.EditorExposed("One Shot", "If true, only triggers once")]
        private bool _oneShot = true;

        private ulong _bossEntity = 0;
        private bool _triggered = false;
        private bool _dying = false;
        private float _deathTimer = 0f;
        private float _originalIntensity = 0f;
        private bool _bossHasSpotLight = false;
        private bool _bossHasPointLight = false;
        private bool _particlesStopped = false;

        private static readonly System.Collections.Generic.Dictionary<ulong, BossDeathTrigger> s_instances
            = new System.Collections.Generic.Dictionary<ulong, BossDeathTrigger>();

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);

            _bossEntity = API.FindEntity(_bossName);

            if (_bossEntity != 0)
            {
                _bossHasSpotLight = API.HasSpotLight(_bossEntity);
                _bossHasPointLight = API.HasPointLight(_bossEntity);

                if (_bossHasSpotLight)
                    _originalIntensity = API.GetSpotLightIntensity(_bossEntity);
                else if (_bossHasPointLight)
                    _originalIntensity = API.GetPointLightIntensity(_bossEntity);
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnterCallback);
        }

        private bool _keyWasDown = false;

        public void OnUpdate(float dt)
        {
            // Press "0" key to test boss death (GLFW key code 48)
            if (_bossEntity != 0 && !_dying)
            {
                bool keyDown = API.IsKeyDown(48);
                if (keyDown && !_keyWasDown)
                {
                    _triggered = true;
                    StartBossDeath();
                }
                _keyWasDown = keyDown;
            }

            if (!_dying || _bossEntity == 0)
                return;

            _deathTimer += dt;
            float t = Math.Min(_deathTimer / _fadeDuration, 1.0f);

            // Fade opacity from 1 -> 0
            float opacity = 1.0f - t;
            API.SetModelOpacity(_bossEntity, opacity);

            // Dim the light
            float intensity = _originalIntensity * (1.0f - t);
            if (_bossHasSpotLight)
                API.SetSpotLightIntensity(_bossEntity, intensity);
            else if (_bossHasPointLight)
                API.SetPointLightIntensity(_bossEntity, intensity);

            // Stop particles after particle duration
            if (!_particlesStopped && _deathTimer >= _particleDuration)
            {
                if (API.HasParticleEmitter(_bossEntity))
                    API.StopParticleEmitter(_bossEntity);
                _particlesStopped = true;
            }

            // Sequence complete
            if (t >= 1.0f)
            {
                _dying = false;
                if (_destroyAfterFade)
                    API.DestroyEntity(_bossEntity);
            }
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity))
                s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private void StartBossDeath()
        {
            if (_dying) return;
            _dying = true;
            _deathTimer = 0f;
            _particlesStopped = false;

            // Add particle emitter to the boss at runtime and configure it
            API.AddParticleEmitter(_bossEntity);

            // Configure dissolve particles: reddish-orange sparks rising upward
            API.SetParticleEmissionRate(_bossEntity, _particleRate);
            API.SetParticleStartColor(_bossEntity, 1.0f, 0.3f, 0.1f, 1.0f);
            API.SetParticleEndColor(_bossEntity, 0.5f, 0.0f, 0.0f, 0.0f);
            API.SetParticleSpeed(_bossEntity, _particleSpeedMin, _particleSpeedMax);
            API.SetParticleSize(_bossEntity, 0.3f, 0.6f, 0.0f);
            API.SetParticleLifetime(_bossEntity, 0.8f, 1.5f);
            API.SetParticleGravity(_bossEntity, -1.5f);
            API.SetParticleDirection(_bossEntity, 0f, 1f, 0f);

            API.PlayParticleEmitter(_bossEntity);
        }

        private static void OnTriggerEnterCallback(ulong triggerEntity, ulong otherEntity)
        {
            BossDeathTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;
            if (inst._oneShot && inst._triggered) return;

            inst._triggered = true;
            inst.StartBossDeath();
        }
    }
}
