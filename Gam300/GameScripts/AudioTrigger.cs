using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Plays a sound effect when the player walks through this trigger volume.
    /// Attach to any entity with a trigger collider.
    /// </summary>
    public class AudioTrigger
    {
        public ulong Entity;

        [Boom.EditorExposed("Sound Path", "Path to the audio file (e.g. Resources/Audio/ambience.wav)")]
        private string _soundPath = "";

        [Boom.EditorExposed("Volume", "Playback volume (0.0 - 1.0)", 0f, 1f, true)]
        private float _volume = 1.0f;

        [Boom.EditorExposed("Loop", "Whether the sound loops while the player is inside")]
        private bool _loop = false;

        [Boom.EditorExposed("Use 3D Positional Audio", "If true, sound plays from the trigger's world position")]
        private bool _use3D = false;

        [Boom.EditorExposed("One Shot", "If true, the sound only plays the first time the player enters")]
        private bool _oneShot = false;

        [Boom.EditorExposed("Cooldown", "Minimum seconds between plays (ignored if One Shot is true)", 0f, 30f, true)]
        private float _cooldown = 0f;

        [Boom.EditorExposed("Exit Sound Path", "Optional sound to play when the player exits (leave empty for none)")]
        private string _exitSoundPath = "";

        [Boom.EditorExposed("Exit Sound Volume", "Volume for the exit sound", 0f, 1f, true)]
        private float _exitVolume = 1.0f;

        private string _soundKey;
        private string _exitSoundKey;

        private bool _hasPlayed    = false;
        private float _cooldownTimer = 0f;
        private bool _playerInside = false;

        private static readonly Dictionary<ulong, AudioTrigger> s_instances = new Dictionary<ulong, AudioTrigger>();

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);

            // Unique sound keys per entity so multiple triggers don't share a channel
            _soundKey     = $"audio_trigger_{Entity}";
            _exitSoundKey = $"audio_trigger_exit_{Entity}";

            if (!string.IsNullOrWhiteSpace(_soundPath))
            {
                API.PreloadSound(_soundKey, _soundPath);
                API.SetSoundVolume(_soundKey, _volume);
            }

            if (!string.IsNullOrWhiteSpace(_exitSoundPath))
            {
                API.PreloadSound(_exitSoundKey, _exitSoundPath);
                API.SetSoundVolume(_exitSoundKey, _exitVolume);
            }

            if (!API.IsTrigger(Entity))
                API.SetTrigger(Entity, true);

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
        }

        public void OnUpdate(float dt)
        {
            if (_cooldownTimer > 0f)
                _cooldownTimer -= dt;
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            AudioTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInside = true;

            if (string.IsNullOrWhiteSpace(inst._soundPath)) return;
            if (inst._oneShot && inst._hasPlayed) return;
            if (inst._cooldownTimer > 0f) return;

            if (inst._use3D)
            {
                Vec3 pos = API.GetPosition(triggerEntity);
                API.PlaySoundAt(inst._soundKey, inst._soundPath, pos, inst._loop);
            }
            else
            {
                API.PlaySound(inst._soundKey, inst._soundPath, inst._loop);
            }

            API.SetSoundVolume(inst._soundKey, inst._volume);
            inst._hasPlayed    = true;
            inst._cooldownTimer = inst._cooldown;
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            AudioTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInside = false;

            // Stop looping sound when player leaves
            if (inst._loop && !string.IsNullOrWhiteSpace(inst._soundPath))
                API.StopSound(inst._soundKey);

            // Play exit sound if configured
            if (!string.IsNullOrWhiteSpace(inst._exitSoundPath))
            {
                if (inst._use3D)
                {
                    Vec3 pos = API.GetPosition(triggerEntity);
                    API.PlaySoundAt(inst._exitSoundKey, inst._exitSoundPath, pos, false);
                }
                else
                {
                    API.PlaySound(inst._exitSoundKey, inst._exitSoundPath, false);
                }
                API.SetSoundVolume(inst._exitSoundKey, inst._exitVolume);
            }
        }
    }
}
