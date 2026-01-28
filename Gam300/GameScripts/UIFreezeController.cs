using Boom;
using System;

namespace GameScripts
{
    public class UIFreezeController
    {
        public ulong Entity;

        private string _spriteName = "UI_Freeze";
        private ulong _spriteEntity = 0;
        private const string FREEZE_AVAILABLE = "Resources/Textures/PlayerUI/UI_Freeze_Available.png";
        private const string FREEZE_UNAVAILABLE = "Resources/Textures/PlayerUI/UI_Freeze_Unavailable.png";
        private string _currentTextureState = "";

        private float _pulseSpeed = 8.0f;
        private float _pulseTimer = 0.0f;

        public void OnStart(string jsonParams)
        {
            _spriteEntity = API.FindEntity(_spriteName);

            if (_spriteEntity != 0 && API.HasSprite(_spriteEntity))
            {
                API.SetSpriteAlpha(_spriteEntity, 1.0f);

                UpdateVisuals(0.0f);
                API.Log("[UIFreeze] Initialized Freeze UI Controller.");
            }
            else
            {
                API.Log("[UIFreeze] ERROR: Could not find UI_Freeze sprite entity!");
            }
        }

        public void OnUpdate(float dt)
        {
            if (_spriteEntity == 0 || !API.HasSprite(_spriteEntity)) return;
            UpdateVisuals(dt);
        }

        private void UpdateVisuals(float dt)
        {
            // 1. GET STATUS
            bool hasCharge = PlayerInventory.HasFreezePower();
            bool isActive = FreezeManager.IsActive();

            // 2. DETERMINE TARGET TEXTURE
            string targetTexture = (hasCharge || isActive) ? FREEZE_AVAILABLE : FREEZE_UNAVAILABLE;

            // 3. SWAP TEXTURE (Only if changed)
            if (_currentTextureState != targetTexture)
            {
                API.SetSpriteTexture(_spriteEntity, targetTexture);
                _currentTextureState = targetTexture;
            }

            // 4. HANDLE PULSE EFFECT (Only when Active)
            float finalAlpha = 1.0f;

            if (isActive)
            {
                // Pulse alpha between 0.5 and 1.0 while the effect is active
                _pulseTimer += dt * _pulseSpeed;
                finalAlpha = 0.5f + 0.5f * (float)Math.Sin(_pulseTimer);
            }
            else
            {
                // Reset pulse timer so it starts fresh next time
                _pulseTimer = 0.0f; 
                finalAlpha = 1.0f;
            }

            // 5. SET ALPHA
            API.SetSpriteAlpha(_spriteEntity, finalAlpha);
        }
    }
}