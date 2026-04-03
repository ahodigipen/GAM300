using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Sets the intensity of specified point/spot lights when the player enters this trigger zone.
    /// Optionally resets intensity on exit. Supports one-shot mode.
    /// </summary>
    public class LightIntensityTrigger
    {
        public ulong Entity;

        [Boom.EditorExposed("Light Names", "Comma-separated names of light entities to affect")]
        private string _lightNames = "";

        [Boom.EditorExposed("Target Intensity", "Intensity to set when player enters the trigger")]
        private float _targetIntensity = 2.5f;

        [Boom.EditorExposed("Reset On Exit", "If true, restores original intensity when player leaves")]
        private bool _resetOnExit = false;

        [Boom.EditorExposed("One Shot", "If true, only triggers once and never fires again")]
        private bool _oneShot = false;

        private List<ulong> _lightIDs = new List<ulong>();
        private Dictionary<ulong, float> _originalIntensities = new Dictionary<ulong, float>();
        private bool _hasTriggered = false;

        private static readonly Dictionary<ulong, LightIntensityTrigger> s_instances = new Dictionary<ulong, LightIntensityTrigger>();

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);

            // Cache light entity IDs and store their original intensities
            if (!string.IsNullOrEmpty(_lightNames))
            {
                foreach (string name in _lightNames.Split(','))
                {
                    ulong id = API.FindEntity(name.Trim());
                    if (id == 0) continue;

                    if (API.HasPointLight(id))
                    {
                        _originalIntensities[id] = API.GetPointLightIntensity(id);
                        _lightIDs.Add(id);
                    }
                    else if (API.HasSpotLight(id))
                    {
                        _originalIntensities[id] = API.GetSpotLightIntensity(id);
                        _lightIDs.Add(id);
                    }
                }
            }

            if (API.HasCollider(Entity) && !API.IsTrigger(Entity))
                API.SetTrigger(Entity, true);

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnterCallback);
            if (_resetOnExit)
                API.RegisterTriggerExitCallback(Entity, OnTriggerExitCallback);
        }

        public void OnUpdate(float dt) { }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private void SetLightIntensities(float intensity)
        {
            foreach (ulong id in _lightIDs)
            {
                if (API.HasPointLight(id))
                    API.SetPointLightIntensity(id, intensity);
                else if (API.HasSpotLight(id))
                    API.SetSpotLightIntensity(id, intensity);
            }
        }

        private void RestoreLightIntensities()
        {
            foreach (ulong id in _lightIDs)
            {
                if (!_originalIntensities.ContainsKey(id)) continue;
                float original = _originalIntensities[id];

                if (API.HasPointLight(id))
                    API.SetPointLightIntensity(id, original);
                else if (API.HasSpotLight(id))
                    API.SetSpotLightIntensity(id, original);
            }
        }

        private static void OnTriggerEnterCallback(ulong triggerEntity, ulong otherEntity)
        {
            LightIntensityTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;
            if (inst._oneShot && inst._hasTriggered) return;

            inst._hasTriggered = true;
            inst.SetLightIntensities(inst._targetIntensity);
        }

        private static void OnTriggerExitCallback(ulong triggerEntity, ulong otherEntity)
        {
            LightIntensityTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;
            if (inst._oneShot && inst._hasTriggered) return;

            inst.RestoreLightIntensities();
        }
    }
}
