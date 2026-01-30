using Boom;
using System;
using System.Collections.Generic;

namespace GameScripts
{
    /// <summary>
    /// Controls the tutorial popup UI sprites
    /// Shows when player enters a TutorialPopupTrigger zone
    /// Fades in when entering zone, fades out when leaving
    /// Supports multiple tutorial zones - hides only when all zones are exited
    /// </summary>
    public class UITutorialController
    {
        public ulong Entity;

        private ulong _tutorialSprite = 0;

        private float _fadeSpeed = 4.0f;        // Speed of fade in/out
        private float _currentAlpha = 0.0f;
        private bool _shouldShow = false;

        // Track active tutorial zones to support multiple zones
        private Dictionary<ulong, ulong> _activeTutorialZones = new Dictionary<ulong, ulong>();

        // Optional: Pulsing effect while visible
        private bool _enablePulse = true;
        private float _pulseSpeed = 2.0f;
        private float _pulseMin = 0.7f;
        private float _pulseMax = 1.0f;
        private float _pulseTimer = 0.0f;

        public void OnStart(string jsonParams)
        {
            API.Log("[UITutorial] Tutorial controller initialized");
        }

        public void OnUpdate(float dt)
        {
            if (_tutorialSprite == 0 || !API.HasSprite(_tutorialSprite)) return;

            // Target alpha based on whether we should show
            float targetAlpha = _shouldShow ? 1.0f : 0.0f;

            // Apply pulsing effect if visible and enabled
            if (_shouldShow && _enablePulse && _currentAlpha > 0.5f)
            {
                _pulseTimer += dt * _pulseSpeed;
                float pulse = _pulseMin + (_pulseMax - _pulseMin) *
                              (0.5f + 0.5f * (float)Math.Sin(_pulseTimer));
                targetAlpha *= pulse;
            }

            // Smooth transition to target alpha
            _currentAlpha = Lerp(_currentAlpha, targetAlpha, _fadeSpeed * dt);
            API.SetSpriteAlpha(_tutorialSprite, _currentAlpha);
        }

        /// <summary>
        /// Call this when entering a tutorial zone
        /// </summary>
        public void Show(ulong spriteEntity)
        {
            ShowZone(0, spriteEntity);
        }

        /// <summary>
        /// Call this when leaving a tutorial zone
        /// </summary>
        public void Hide()
        {
            HideZone(0);
        }

        /// <summary>
        /// Call this when entering a tutorial zone (zone-aware)
        /// </summary>
        public void ShowZone(ulong zoneEntity, ulong spriteEntity)
        {
            if (spriteEntity != 0 && API.HasSprite(spriteEntity))
            {
                // Track this zone's sprite
                _activeTutorialZones[zoneEntity] = spriteEntity;

                // If we have an active sprite, hide it first if it's different
                if (_tutorialSprite != 0 && _tutorialSprite != spriteEntity)
                {
                    API.SetSpriteAlpha(_tutorialSprite, 0f);
                }

                _tutorialSprite = spriteEntity;
                _shouldShow = true;
                _currentAlpha = 0f;  // Reset alpha for fade in
                _pulseTimer = 0f;
                API.Log($"[UITutorial] Tutorial zone entered - showing sprite {spriteEntity} (total active: {_activeTutorialZones.Count})");
            }
            else
            {
                API.Log($"[UITutorial] ERROR: Invalid sprite entity {spriteEntity}");
            }
        }

        /// <summary>
        /// Call this when leaving a tutorial zone (zone-aware)
        /// Only hide if ALL tutorial zones are exited
        /// </summary>
        public void HideZone(ulong zoneEntity)
        {
            if (_activeTutorialZones.ContainsKey(zoneEntity))
            {
                _activeTutorialZones.Remove(zoneEntity);
            }

            // Only hide UI if NO zones are active
            if (_activeTutorialZones.Count == 0)
            {
                _shouldShow = false;
                _tutorialSprite = 0;
                API.Log("[UITutorial] All tutorial zones exited - hiding popup");
            }
            else
            {
                // Still have active zones - show the first one's sprite
                foreach (var kvp in _activeTutorialZones)
                {
                    _tutorialSprite = kvp.Value;
                    _shouldShow = true;
                    API.Log($"[UITutorial] Tutorial zone exited but still in zone (total active: {_activeTutorialZones.Count})");
                    break;
                }
            }
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            return a + (b - a) * t;
        }
    }
}
