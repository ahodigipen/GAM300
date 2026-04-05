using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attach to the "Trigger box LionStatue_6" entity.
    /// When the player enters the trigger, the BOSS entity begins a death sequence:
    ///   1. Particles burst from the boss in a sphere (evaporation / disintegration effect)
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
        private float _fadeDuration = 4.0f;

        [Boom.EditorExposed("Particle Duration", "How long particles emit before stopping")]
        private float _particleDuration = 3.5f;

        [Boom.EditorExposed("Particle Rate", "Particles emitted per second during death")]
        private float _particleRate = 150.0f;

        [Boom.EditorExposed("Particle Speed Min", "Minimum particle speed")]
        private float _particleSpeedMin = 1.0f;

        [Boom.EditorExposed("Particle Speed Max", "Maximum particle speed")]
        private float _particleSpeedMax = 3.5f;

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

        // Evaporation phase tracking
        private bool _phase2Started = false;   // ramp-up burst
        private bool _phase3Started = false;   // final wisp

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

            // ── Phase 1 (0% – 25%): hold solid, particles ramping up ──
            // ── Phase 2 (25% – 75%): main dissolve, opacity drops, particles peak ──
            // ── Phase 3 (75% – 100%): final wisps, almost fully transparent ──

            // Opacity: hold for first 25%, then fade smoothly
            float opacity;
            if (t < 0.25f)
            {
                opacity = 1.0f;
            }
            else
            {
                float fadeT = (t - 0.25f) / 0.75f; // 0..1 over remaining time
                // Ease-in curve for more dramatic end
                opacity = 1.0f - (fadeT * fadeT);
            }
            API.SetModelOpacity(_bossEntity, opacity);

            // Phase 2: Ramp up emission and expand sphere at 25%
            if (!_phase2Started && t >= 0.25f)
            {
                _phase2Started = true;
                API.SetParticleEmissionRate(_bossEntity, _particleRate * 2.0f);
                API.SetParticleShapeSize(_bossEntity, 4.0f, 5.0f, 4.0f); // expand box
                API.SetParticleSpeed(_bossEntity, _particleSpeedMin * 1.5f, _particleSpeedMax * 1.5f);
            }

            // Phase 3: Slow down to final wisps at 75%
            if (!_phase3Started && t >= 0.75f)
            {
                _phase3Started = true;
                API.SetParticleEmissionRate(_bossEntity, _particleRate * 0.4f);
                API.SetParticleSpeed(_bossEntity, _particleSpeedMin * 0.5f, _particleSpeedMax * 0.7f);
                API.SetParticleStartColor(_bossEntity, 0.3f, 0.3f, 0.3f, 0.6f);  // Ashy grey
                API.SetParticleEndColor(_bossEntity, 0.1f, 0.1f, 0.1f, 0.0f);    // Fade to nothing
            }

            // Dim the light proportionally
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
            _phase2Started = false;
            _phase3Started = false;

            // Add particle emitter to the boss at runtime
            API.AddParticleEmitter(_bossEntity);

            // ── Evaporation particle configuration ──
            // Spawn shape: box around the boss so particles appear all around it
            API.SetParticleShapeType(_bossEntity, 3);              // 3 = box
            API.SetParticleShapeSize(_bossEntity, 3.0f, 4.0f, 3.0f); // box half-extents (wide & tall)
            API.SetParticleMaxParticles(_bossEntity, 1000);
            API.SetParticleLooping(_bossEntity, false);

            // Emission: start moderate, will ramp in phase 2
            API.SetParticleEmissionRate(_bossEntity, _particleRate);

            // Particles scatter outward in all directions, slight upward bias
            API.SetParticleDirection(_bossEntity, 0f, 0.3f, 0f);
            API.SetParticleSpeed(_bossEntity, _particleSpeedMin, _particleSpeedMax);
            API.SetParticleGravity(_bossEntity, -0.3f);     // gentle upward drift

            // Start as glowing ember, end as transparent ash
            API.SetParticleStartColor(_bossEntity, 1.0f, 0.4f, 0.1f, 1.0f);  // hot orange
            API.SetParticleEndColor(_bossEntity, 0.6f, 0.1f, 0.0f, 0.0f);    // fade to transparent red

            // Bigger particles for a more visible evaporation effect
            API.SetParticleSize(_bossEntity, 0.5f, 1.0f, 0.0f);

            // Longer lifetime so particles linger and drift
            API.SetParticleLifetime(_bossEntity, 1.2f, 2.8f);

            // Additive blend for glowing ember look
            API.SetParticleAdditiveBlend(_bossEntity, true);

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
