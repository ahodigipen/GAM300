using Boom;
using System;

namespace GameScripts
{
    public class UIFreezeController
    {
        public ulong Entity;

        private string _spriteName = "UI_Freeze";
        private ulong _spriteEntity = 0;

        // Settings
        private float _fadeSpeed = 5.0f;
        private float _currentAlpha = 0.0f;
        private float _pulseSpeed = 8.0f;
        private float _pulseTimer = 0.0f;

        public void OnStart(string jsonParams)
        {
            _spriteEntity = API.FindEntity(_spriteName);

            if (_spriteEntity != 0 && API.HasSprite(_spriteEntity))
            {
                // Check if already have the ability
                bool alreadyHasPower = PlayerInventory.HasFreezePower();
                bool isActive = FreezeManager.IsActive();

                if (alreadyHasPower || isActive)
                {
                    _currentAlpha = 1.0f;
                    API.SetSpriteAlpha(_spriteEntity, 1.0f);
                    API.Log($"[UIFreeze] Hot-reload detected: Restoring UI (HasPower: {alreadyHasPower})");
                }
                else
                {
                    _currentAlpha = 0.0f;
                    API.SetSpriteAlpha(_spriteEntity, 0f);
                    API.Log($"[UIFreeze] Initialized freeze UI sprite (Hidden)");
                }
            }
        }

        public void OnUpdate(float dt)
        {
            if (_spriteEntity == 0 || !API.HasSprite(_spriteEntity)) return;

            // 1. GET STATUS
            bool hasCharge = PlayerInventory.HasFreezePower();
            bool isActive = FreezeManager.IsActive();

            float targetAlpha = 0.0f;
            float pulseMultiplier = 1.0f;

            // 2. DETERMINE VISUAL STATE
            if (hasCharge)
            {
                // STATE: HOLDING (Solid)
                targetAlpha = 1.0f;
                _pulseTimer = 0f;
            }
            else if (isActive)
            {
                // STATE: ACTIVATED (Pulsing)
                targetAlpha = 1.0f;

                _pulseTimer += dt * _pulseSpeed;
                pulseMultiplier = 0.3f + 0.7f * (0.5f + 0.5f * (float)Math.Sin(_pulseTimer));
            }
            else
            {
                // STATE: EMPTY
                targetAlpha = 0.0f;
            }

            // 3. APPLY SMOOTH TRANSITION
            _currentAlpha = Lerp(_currentAlpha, targetAlpha, _fadeSpeed * dt);

            // 4. SET FINAL ALPHA
            API.SetSpriteAlpha(_spriteEntity, _currentAlpha * pulseMultiplier);
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            return a + (b - a) * t;
        }
    }
}