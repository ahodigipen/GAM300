using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Pulses a spot/point light's intensity and optionally scales the entity vertically
    /// to create a portal-like illumination effect that glows brighter up and down.
    /// Attach this script to the ENDGATE entity (or any entity with a light component).
    /// </summary>
    public class PortalLightPulse
    {
        public ulong Entity;

        [Boom.EditorExposed("Min Intensity", "Minimum light intensity during pulse")]
        private float _minIntensity = 3.0f;

        [Boom.EditorExposed("Max Intensity", "Maximum light intensity during pulse")]
        private float _maxIntensity = 12.0f;

        [Boom.EditorExposed("Pulse Speed", "How fast the light pulses (cycles per second)")]
        private float _pulseSpeed = 1.5f;

        [Boom.EditorExposed("Pulse Scale Y", "If true, also pulses the Y scale for a vertical stretch effect")]
        private bool _pulseScaleY = true;

        [Boom.EditorExposed("Min Scale Y", "Minimum Y scale multiplier")]
        private float _minScaleY = 0.8f;

        [Boom.EditorExposed("Max Scale Y", "Maximum Y scale multiplier")]
        private float _maxScaleY = 1.3f;

        [Boom.EditorExposed("Pulse Color", "If true, shifts color between base and bright white")]
        private bool _pulseColor = true;

        [Boom.EditorExposed("Glow Color R", "Red component of the glow color (0-1)")]
        private float _glowR = 0.6f;

        [Boom.EditorExposed("Glow Color G", "Green component of the glow color (0-1)")]
        private float _glowG = 0.8f;

        [Boom.EditorExposed("Glow Color B", "Blue component of the glow color (0-1)")]
        private float _glowB = 1.0f;

        private float _timer = 0f;
        private Vec3 _originalScale;
        private Vec3 _baseColor;
        private bool _hasSpotLight;
        private bool _hasPointLight;

        public void OnStart(string jsonParams)
        {
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);

            _hasSpotLight = API.HasSpotLight(Entity);
            _hasPointLight = API.HasPointLight(Entity);

            if (API.HasTransform(Entity))
                _originalScale = API.GetScale(Entity);

            if (_hasSpotLight)
                _baseColor = API.GetSpotLightColor(Entity);
            else if (_hasPointLight)
                _baseColor = API.GetPointLightColor(Entity);
        }

        public void OnUpdate(float dt)
        {
            _timer += dt * _pulseSpeed;

            // Sine wave oscillation normalized to 0..1
            float t = (float)(Math.Sin(_timer * 2.0 * Math.PI) * 0.5 + 0.5);

            // Pulse intensity
            float intensity = _minIntensity + (_maxIntensity - _minIntensity) * t;

            if (_hasSpotLight)
                API.SetSpotLightIntensity(Entity, intensity);
            else if (_hasPointLight)
                API.SetPointLightIntensity(Entity, intensity);

            // Pulse color toward a bright glow
            if (_pulseColor)
            {
                Vec3 glowColor = new Vec3(_glowR, _glowG, _glowB);
                Vec3 color = new Vec3(
                    _baseColor.X + (glowColor.X - _baseColor.X) * t,
                    _baseColor.Y + (glowColor.Y - _baseColor.Y) * t,
                    _baseColor.Z + (glowColor.Z - _baseColor.Z) * t
                );

                if (_hasSpotLight)
                    API.SetSpotLightColor(Entity, color);
                else if (_hasPointLight)
                    API.SetPointLightColor(Entity, color);
            }

            // Pulse Y scale for vertical stretch effect
            if (_pulseScaleY && API.HasTransform(Entity))
            {
                float scaleY = _minScaleY + (_maxScaleY - _minScaleY) * t;
                Vec3 newScale = new Vec3(
                    _originalScale.X,
                    _originalScale.Y * scaleY,
                    _originalScale.Z
                );
                API.SetScale(Entity, newScale);
            }
        }

        public void OnDestroy()
        {
            // Restore original values
            if (_hasSpotLight)
            {
                API.SetSpotLightColor(Entity, _baseColor);
            }
            else if (_hasPointLight)
            {
                API.SetPointLightColor(Entity, _baseColor);
            }

            if (_pulseScaleY && API.HasTransform(Entity))
            {
                API.SetScale(Entity, _originalScale);
            }
        }
    }
}
