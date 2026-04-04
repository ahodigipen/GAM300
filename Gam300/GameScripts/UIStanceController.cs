using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// UI controller for the crouch prompt (CanCrouch sprite).
    /// Fades in when the player enters a crouch trigger zone,
    /// fades out when they leave. Hidden at all other times.
    /// </summary>
    public class UIStanceController
    {
        public ulong Entity;

        private string _stanceSpriteName = "UI_Stance";
        private ulong _stanceSpriteEntity = 0;

        private const string CROUCH_TEXTURE = "Resources/Textures/PlayerUI/CanCrouch.png";
        private const float FADE_SPEED = 3.0f; // alpha units per second

        private float _currentAlpha = 0f;

        public void OnStart(string jsonParams)
        {
            _stanceSpriteEntity = API.FindEntity(_stanceSpriteName);

            if (_stanceSpriteEntity != 0 && API.HasSprite(_stanceSpriteEntity))
                InitializeSprite();

            API.Log("[UIStance] Initialized stance UI controller.");
        }

        public void ForceHide()
        {
            if (_stanceSpriteEntity != 0 && API.HasSprite(_stanceSpriteEntity))
            {
                _currentAlpha = 0f;
                API.SetSpriteAlpha(_stanceSpriteEntity, 0f);
            }
        }

        public void ForceShow()
        {
            // No-op: stance visibility is driven by crouch zone state, not forced
        }

        public void OnUpdate(float dt)
        {
            if (UIManager.IsHUDHidden) return;

            // Lazy initialization
            if (_stanceSpriteEntity == 0)
            {
                _stanceSpriteEntity = API.FindEntity(_stanceSpriteName);
                if (_stanceSpriteEntity != 0 && API.HasSprite(_stanceSpriteEntity))
                {
                    API.Log("[UIStance] Lazily found and initialized UI_Stance.");
                    InitializeSprite();
                }
                else return;
            }

            if (!API.HasSprite(_stanceSpriteEntity)) return;

            UpdateVisuals(dt);
        }

        private void InitializeSprite()
        {
            API.SetSpriteTexture(_stanceSpriteEntity, CROUCH_TEXTURE);
            _currentAlpha = 0f;
            API.SetSpriteAlpha(_stanceSpriteEntity, 0f);
        }

        private void UpdateVisuals(float dt)
        {
            float targetAlpha = PlayerMovement.IsInCrouchZone() ? 1f : 0f;

            if (Math.Abs(_currentAlpha - targetAlpha) > 0.001f)
            {
                _currentAlpha = MoveTowards(_currentAlpha, targetAlpha, FADE_SPEED * dt);
                API.SetSpriteAlpha(_stanceSpriteEntity, _currentAlpha);
            }
        }

        private static float MoveTowards(float current, float target, float maxDelta)
        {
            float diff = target - current;
            if (Math.Abs(diff) <= maxDelta) return target;
            return current + (diff > 0 ? maxDelta : -maxDelta);
        }
    }
}
