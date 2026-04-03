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
            // Optimistic first try (may fail if loaded additively)
            _stanceSpriteEntity = API.FindEntity(_stanceSpriteName);

            if (_stanceSpriteEntity != 0 && API.HasSprite(_stanceSpriteEntity))
            {
                InitializeSprite();
            }

            API.Log("[UIStance] Initialized stance UI controller.");
        }

        public void ForceHide()
        {
            if (_stanceSpriteEntity != 0 && API.HasSprite(_stanceSpriteEntity))
                API.SetSpriteAlpha(_stanceSpriteEntity, 0f);
        }

        public void OnUpdate(float dt)
        {
            if (UIManager.IsHUDHidden) return;

            // --- NEW FIX: Lazy Initialization ---
            // If the entity wasn't found in OnStart, keep looking for it
            if (_stanceSpriteEntity == 0)
            {
                _stanceSpriteEntity = API.FindEntity(_stanceSpriteName);

                // The moment we find it, run the initial setup
                if (_stanceSpriteEntity != 0 && API.HasSprite(_stanceSpriteEntity))
                {
                    API.Log("[UIStance] Lazily found and initialized UI_Stance.");
                    InitializeSprite();
                }
                else
                {
                    return; // Still not found, exit and try again next frame
                }
            }

            // Double-check it still has the sprite component before proceeding
            if (!API.HasSprite(_stanceSpriteEntity))
            {
                return;
            }

            UpdateVisuals(dt);
        }

        // Helper method to set up the default visual state once the entity is found
        private void InitializeSprite()
        {
            // Default to run texture 
            _currentTexture = string.Empty; // force initial set
            API.SetSpriteTexture(_stanceSpriteEntity, RUN_TEXTURE);
            _currentTexture = RUN_TEXTURE;
            API.SetSpriteAlpha(_stanceSpriteEntity, 1.0f);
        }

        private void UpdateVisuals(float dt)
        {
            // --- CROUCH / RUN UI (single sprite) ---
            bool canCrouch = PlayerMovement.IsInCrouchZone();

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