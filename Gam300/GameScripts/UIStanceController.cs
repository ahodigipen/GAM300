using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// UI controller that manages crouch and run UI sprites.
    /// - Looks up `UI_Crouch` and `UI_Run` sprite entities
    /// - Switches texture between available/unavailable based on player state
    /// </summary>
    public class UIStanceController
    {
        public ulong Entity;

        // Single sprite entity in the scene used to show stance
        private string _stanceSpriteName = "UI_Stance";
        private ulong _stanceSpriteEntity = 0;

        // Texture file paths (use exact asset names)
        private const string CROUCH_TEXTURE = "Resources/Textures/PlayerUI/CanCrouch.png";
        private const string RUN_TEXTURE = "Resources/Textures/PlayerUI/CannotCrouch.png";

        // Track current texture to avoid redundant swaps
        private string _currentTexture = string.Empty;

        public void OnStart(string jsonParams)
        {
            _stanceSpriteEntity = API.FindEntity(_stanceSpriteName);

            if (_stanceSpriteEntity != 0 && API.HasSprite(_stanceSpriteEntity))
            {
                // Default to run texture 
                _currentTexture = string.Empty; // force initial set
                API.SetSpriteTexture(_stanceSpriteEntity, RUN_TEXTURE);
                _currentTexture = RUN_TEXTURE;
                API.SetSpriteAlpha(_stanceSpriteEntity, 1.0f);
            }

            // Run an initial update to ensure visuals are correct
            UpdateVisuals(0.0f);
            API.Log("[UIStance] Initialized stance UI controller.");
        }

        public void OnUpdate(float dt)
        {
            if (_stanceSpriteEntity == 0 || !API.HasSprite(_stanceSpriteEntity))
            {
                return;
            }

            UpdateVisuals(dt);
        }

        private void UpdateVisuals(float dt)
        {
            // --- CROUCH / RUN UI (single sprite) ---
            bool canCrouch = PlayerMovement.IsInCrouchZone();

            if (_stanceSpriteEntity == 0 || !API.HasSprite(_stanceSpriteEntity))
                return;

            // Determine desired texture and alpha behavior
            string desiredTexture = canCrouch ? CROUCH_TEXTURE : RUN_TEXTURE;

            if (_currentTexture != desiredTexture)
            {
                API.SetSpriteTexture(_stanceSpriteEntity, desiredTexture);
                _currentTexture = desiredTexture;
            }

            // Simply ensure sprite is fully visible after swap
            API.SetSpriteAlpha(_stanceSpriteEntity, 1.0f);
        }
    }
}
