using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Manages item pickup tutorial popups (sprite-based, no additive scenes).
    ///
    /// First-time pickup: Shows the item sprite first, then dialogue slides.
    ///   LargeToken / SmallToken: Item → Dialogue1 → Dialogue2 → close  (3 ENTER presses)
    ///   Talisman:                Item → Dialogue1 → Dialogue2 → Dialogue3 → close  (4 ENTER presses)
    ///
    /// Repeat pickup: Shows the AfterFirstTime sprite immediately → close  (1 ENTER press)
    ///
    /// Game is paused for the entire duration. ENTER (or gamepad A) advances / dismisses.
    /// Input is handled in Update() which MUST be called from Entry.Update() every frame.
    /// </summary>
    public static class TutorialManager
    {
        // ── Item type enum ──────────────────────────────────────────────────
        public enum ItemType { LargeToken, SmallToken, Talisman }

        // ── State machine ───────────────────────────────────────────────────
        private enum TutorialState
        {
            None,
            ShowingFirstTimeItem,   // "FirstTime_[Item]" visible
            ShowingDialogue1,       // "FirstTime_Dialogue1_[Item]" visible
            ShowingDialogue2,       // "FirstTime_Dialogue2_[Item]" visible
            ShowingDialogue3,       // "FirstTime_Dialogue3_[Item]" visible (Talisman only)
            ShowingRepeatItem       // "AfterFirstTime_[Item]" visible
        }

        private static TutorialState s_state = TutorialState.None;
        private static ItemType s_activeItem;

        // ── Cached entity IDs (resolved lazily on first ShowPickupTutorial call) ──
        // Large Token
        private static ulong s_eFirstTime_LargeToken       = 0;
        private static ulong s_eDialogue1_LargeToken       = 0;
        private static ulong s_eDialogue2_LargeToken       = 0;
        private static ulong s_eAfterFirstTime_LargeToken  = 0;

        // Small Token
        private static ulong s_eFirstTime_SmallToken       = 0;
        private static ulong s_eDialogue1_SmallToken       = 0;
        private static ulong s_eDialogue2_SmallToken       = 0;
        private static ulong s_eAfterFirstTime_SmallToken  = 0;

        // Talisman
        private static ulong s_eFirstTime_Talisman         = 0;
        private static ulong s_eDialogue1_Talisman         = 0;
        private static ulong s_eDialogue2_Talisman         = 0;
        private static ulong s_eDialogue3_Talisman         = 0;
        private static ulong s_eAfterFirstTime_Talisman    = 0;

        // Background dimming
        private static ulong s_eTutorialDimBG              = 0;
        private const float DIM_BG_ALPHA                   = 1.0f;

        private static bool s_entitiesResolved = false;

        // ── Fade state ───────────────────────────────────────────────────────
        private enum FadeMode { None, FadeIn, FadeOut }
        private static FadeMode  s_fadeMode    = FadeMode.None;
        private static float     s_fadeTimer   = 0f;
        private const  float     FADE_DURATION = 0.15f;
        // Currently fading entity (only one at a time for tutorial)
        private static ulong     s_fadingEntity = 0;
        // Action to run when current fade-out finishes
        private static System.Action s_pendingAfterFadeOut = null;

        // ── HUD/inventory save state ─────────────────────────────────────────
        private static bool s_wasInventoryOpen = false;
        private static bool s_wasGodModeOn     = false;

        // ── Frame-level flags ───────────────────────────────────────────────
        private static bool s_justDismissed = false;

        // ── Input edge-detection state ──────────────────────────────────────
        private static bool s_enterWasDown       = false;
        private static bool s_aButtonWasDown     = false;

        // ──────────────────────────────────────────────────────────────────────
        // Public API
        // ──────────────────────────────────────────────────────────────────────

        /// <summary>
        /// Call this when the player picks up an item.
        /// pickupCount should be the LIFETIME total for that item after the pickup
        /// (i.e., already incremented in PlayerInventory).
        /// </summary>
        public static void ShowPickupTutorial(ItemType item, int pickupCount)
        {
            if (s_state != TutorialState.None)
            {
                API.Log("[TutorialManager] Already showing a tutorial — skipping new one.");
                return;
            }

            ResolveEntities();
            s_activeItem = item;

            // Reset input edge-detection so we don't dismiss immediately
            s_enterWasDown   = true;
            s_aButtonWasDown = true;

            if (pickupCount == 1)
            {
                // First-time sequence: item sprite appears right away; dialogue text fades in.
                ulong itemSprite  = GetEntityForState(TutorialState.ShowingFirstTimeItem, item);
                ulong diag1Sprite = GetEntityForState(TutorialState.ShowingDialogue1, item);

                SetEntityAlpha(itemSprite,  1f);   // item art: shown immediately (no fade needed)
                FadeInEntity(diag1Sprite);          // dialogue text: fades in smoothly

                // Show black background only for first-time tutorial
                if (s_eTutorialDimBG != 0 && API.HasSprite(s_eTutorialDimBG))
                    API.SetSpriteAlpha(s_eTutorialDimBG, DIM_BG_ALPHA);

                s_state = TutorialState.ShowingDialogue1;
                API.Log($"[TutorialManager] First-time pickup for {item}. Showing item sprite and Dialogue 1.");
            }
            else
            {
                // Repeat pickup
                ulong repeatSprite = GetEntityForState(TutorialState.ShowingRepeatItem, item);
                FadeInEntity(repeatSprite);
                s_state = TutorialState.ShowingRepeatItem;
                API.Log($"[TutorialManager] Repeat pickup #{pickupCount} for {item}. Showing repeat sprite.");
            }

            // Hide HUD and inventory for the duration of the tutorial
            s_wasInventoryOpen = Entry.IsInventoryOpen;
            s_wasGodModeOn     = PlayerMovement.IsGodModeActive;
            UIManager.HideHUD();
            CPTrigger.HideAllNotifications();
            if (s_wasInventoryOpen)
                Entry.IsInventoryOpen = false;

            API.SetGameLogicPaused(true);
        }

        /// <summary>
        /// Returns true while any tutorial popup is showing (used by Entry + KeyPickup + PlayerMovement
        /// to block other popups / pause menu while tutorial is active).
        /// </summary>
        public static bool IsTutorialActive() => s_state != TutorialState.None;

        /// <summary>Alias kept for Entry.cs compatibility.</summary>
        public static bool IsKeyTutorialActive() => IsTutorialActive();

        /// <summary>Returns true for one frame after a tutorial is dismissed.</summary>
        public static bool WasJustDismissed() => s_justDismissed;

        /// <summary>Reset all tutorial state (called on game restart via PlayerInventory.Reset).</summary>
        public static void Reset()
        {
            // Immediately hide any currently visible sprites (no graceful fade on reset)
            if (s_state != TutorialState.None)
            {
                SetEntityAlpha(GetEntityForState(s_state, s_activeItem), 0f);
                SetEntityAlpha(GetEntityForState(TutorialState.ShowingFirstTimeItem, s_activeItem), 0f);
            }
            // Kill any in-progress fade
            if (s_fadingEntity != 0) SetEntityAlpha(s_fadingEntity, 0f);

            s_state            = TutorialState.None;
            s_justDismissed    = false;
            s_enterWasDown     = false;
            s_aButtonWasDown   = false;
            s_wasInventoryOpen = false;
            s_wasGodModeOn     = false;
            s_entitiesResolved = false;
            s_fadeMode       = FadeMode.None;
            s_fadeTimer      = 0f;
            s_fadingEntity   = 0;
            s_pendingAfterFadeOut = null;

            // Clear all cached entity IDs (scene may reload)
            s_eFirstTime_LargeToken      = 0;
            s_eDialogue1_LargeToken      = 0;
            s_eDialogue2_LargeToken      = 0;
            s_eAfterFirstTime_LargeToken = 0;
            s_eFirstTime_SmallToken      = 0;
            s_eDialogue1_SmallToken      = 0;
            s_eDialogue2_SmallToken      = 0;
            s_eAfterFirstTime_SmallToken = 0;
            s_eFirstTime_Talisman        = 0;
            s_eDialogue1_Talisman        = 0;
            s_eDialogue2_Talisman        = 0;
            s_eDialogue3_Talisman        = 0;
            s_eAfterFirstTime_Talisman   = 0;
            s_eTutorialDimBG             = 0;

            API.Log("[TutorialManager] Reset");
        }

        // ── Kept for Entry.cs compatibility ──────────────────────────────────
        public static bool GetEscapeKeyWasDown() => false;
        public static void SetEscapeKeyWasDown(bool state) { /* no-op */ }

        /// <summary>
        /// Immediately dismiss any active tutorial popup without advancing through the sequence.
        /// Intended for cutscene use (e.g. a scene transition that must close the tutorial instantly).
        /// Does nothing if no tutorial is currently active.
        /// </summary>
        public static void DismissTutorial()
        {
            if (s_state == TutorialState.None) return;

            API.Log($"[TutorialManager] DismissTutorial() called externally — force-closing tutorial for {s_activeItem}.");
            CloseTutorial();
        }

        // ──────────────────────────────────────────────────────────────────────
        // Update — called from Entry.Update() every frame (works even when paused)
        // ──────────────────────────────────────────────────────────────────────
        public static void Update(float dt)
        {
            s_justDismissed = false;

            // --- Tick any active fade ---
            if (s_fadeMode != FadeMode.None)
            {
                s_fadeTimer += dt;
                float t = Math.Min(1f, s_fadeTimer / FADE_DURATION);

                if (s_fadeMode == FadeMode.FadeIn)
                {
                    SetEntityAlpha(s_fadingEntity, t);
                    if (t >= 1f)
                    {
                        s_fadeMode  = FadeMode.None;
                        s_fadeTimer = 0f;
                        s_fadingEntity = 0;
                    }
                }
                else // FadeOut
                {
                    SetEntityAlpha(s_fadingEntity, 1f - t);
                    if (t >= 1f)
                    {
                        SetEntityAlpha(s_fadingEntity, 0f);
                        s_fadeMode  = FadeMode.None;
                        s_fadeTimer = 0f;
                        s_fadingEntity = 0;

                        System.Action pending = s_pendingAfterFadeOut;
                        s_pendingAfterFadeOut = null;
                        pending?.Invoke();
                    }
                }
            }

            if (s_state == TutorialState.None) return;

            // Block input while fading — only advance state after fades settle
            if (s_fadeMode != FadeMode.None) return;

            // --- Input edge detection: SPACE key and gamepad A button ---
            bool enterDown   = API.IsKeyDown(API.KEY_SPACE);
            bool aButtonDown = API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A);

            bool enterPressed   = enterDown   && !s_enterWasDown;
            bool aButtonPressed = aButtonDown && !s_aButtonWasDown;

            s_enterWasDown   = enterDown;
            s_aButtonWasDown = aButtonDown;

            if (!enterPressed && !aButtonPressed) return;

            // ENTER pressed — advance state machine
            API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);
            AdvanceState();
        }

        // ──────────────────────────────────────────────────────────────────────
        // Private helpers
        // ──────────────────────────────────────────────────────────────────────

        private static void AdvanceState()
        {
            switch (s_state)
            {
                case TutorialState.ShowingFirstTimeItem:
                    TransitionTo(TutorialState.ShowingDialogue1);
                    break;

                case TutorialState.ShowingDialogue1:
                    TransitionTo(TutorialState.ShowingDialogue2);
                    break;

                case TutorialState.ShowingDialogue2:
                    if (s_activeItem == ItemType.Talisman)
                        TransitionTo(TutorialState.ShowingDialogue3);
                    else
                        CloseTutorial();
                    break;

                case TutorialState.ShowingDialogue3:
                    CloseTutorial();
                    break;

                case TutorialState.ShowingRepeatItem:
                    CloseTutorial();
                    break;
            }
        }

        private static void TransitionTo(TutorialState next)
        {
            // Fade out the current sprite, then swap to the next
            ulong currentEntity = GetEntityForState(s_state, s_activeItem);
            TutorialState capturedNext = next;

            FadeOutEntity(currentEntity, () =>
            {
                // State changes happen after fade-out completes
                s_state = capturedNext;
                ulong nextEntity = GetEntityForState(capturedNext, s_activeItem);
                FadeInEntity(nextEntity);
                API.Log($"[TutorialManager] Advancing to state {capturedNext} for {s_activeItem}.");
            });
        }

        private static void CloseTutorial()
        {
            ulong currentEntity = GetEntityForState(s_state, s_activeItem);
            ulong baseItemEntity = GetEntityForState(TutorialState.ShowingFirstTimeItem, s_activeItem);
            ItemType capturedItem = s_activeItem;

            FadeOutEntity(currentEntity, () =>
            {
                // Also hide the base item sprite (shown alongside Dialogue 1)
                SetEntityAlpha(baseItemEntity, 0f);

                // Hide dimmed background
                if (s_eTutorialDimBG != 0 && API.HasSprite(s_eTutorialDimBG))
                    API.SetSpriteAlpha(s_eTutorialDimBG, 0f);

                s_state = TutorialState.None;
                s_justDismissed = true;
                UIManager.ShowHUD();
                CPTrigger.ShowAllNotifications();
                if (s_wasInventoryOpen)
                    Entry.IsInventoryOpen = true;
                if (s_wasGodModeOn)
                    PlayerMovement.ForceShowGodModeText();
                s_wasInventoryOpen = false;
                s_wasGodModeOn     = false;
                API.SetGameLogicPaused(false);
                API.Log($"[TutorialManager] Tutorial closed for {capturedItem}. Game resumed.");
            });
        }

        private static void HideCurrentSprite()
        {
            ulong entity = GetEntityForState(s_state, s_activeItem);
            SetEntityAlpha(entity, 0f);
        }

        /// <summary>Fade-in a single entity.</summary>
        private static void FadeInEntity(ulong entity)
        {
            if (entity == 0) return;
            SetEntityAlpha(entity, 0f);
            s_fadingEntity = entity;
            s_fadeMode     = FadeMode.FadeIn;
            s_fadeTimer    = 0f;
        }

        /// <summary>Fade-out a single entity, then run <paramref name="onDone"/>.</summary>
        private static void FadeOutEntity(ulong entity, System.Action onDone)
        {
            if (entity == 0)
            {
                // Nothing to fade out — run callback immediately
                onDone?.Invoke();
                return;
            }
            s_fadingEntity        = entity;
            s_fadeMode            = FadeMode.FadeOut;
            s_fadeTimer           = 0f;
            s_pendingAfterFadeOut = onDone;
        }

        private static void ShowSprite(ulong entity)
        {
            FadeInEntity(entity);
        }

        private static void HideSprite(ulong entity)
        {
            SetEntityAlpha(entity, 0f);
        }

        private static void SetEntityAlpha(ulong entity, float alpha)
        {
            if (entity == 0) return;
            if (API.HasSprite(entity))
                API.SetSpriteAlpha(entity, alpha);
        }

        /// <summary>Returns the cached entity ID for the given state + item combination.</summary>
        private static ulong GetEntityForState(TutorialState state, ItemType item)
        {
            switch (item)
            {
                case ItemType.LargeToken:
                    switch (state)
                    {
                        case TutorialState.ShowingFirstTimeItem: return s_eFirstTime_LargeToken;
                        case TutorialState.ShowingDialogue1:     return s_eDialogue1_LargeToken;
                        case TutorialState.ShowingDialogue2:     return s_eDialogue2_LargeToken;
                        case TutorialState.ShowingRepeatItem:    return s_eAfterFirstTime_LargeToken;
                    }
                    break;

                case ItemType.SmallToken:
                    switch (state)
                    {
                        case TutorialState.ShowingFirstTimeItem: return s_eFirstTime_SmallToken;
                        case TutorialState.ShowingDialogue1:     return s_eDialogue1_SmallToken;
                        case TutorialState.ShowingDialogue2:     return s_eDialogue2_SmallToken;
                        case TutorialState.ShowingRepeatItem:    return s_eAfterFirstTime_SmallToken;
                    }
                    break;

                case ItemType.Talisman:
                    switch (state)
                    {
                        case TutorialState.ShowingFirstTimeItem: return s_eFirstTime_Talisman;
                        case TutorialState.ShowingDialogue1:     return s_eDialogue1_Talisman;
                        case TutorialState.ShowingDialogue2:     return s_eDialogue2_Talisman;
                        case TutorialState.ShowingDialogue3:     return s_eDialogue3_Talisman;
                        case TutorialState.ShowingRepeatItem:    return s_eAfterFirstTime_Talisman;
                    }
                    break;
            }
            return 0;
        }

        /// <summary>
        /// Resolve all sprite entity IDs from the scene.
        /// Called lazily before the first ShowPickupTutorial so that entities have time to start.
        /// </summary>
        private static void ResolveEntities()
        {
            if (s_entitiesResolved) return;

            s_eFirstTime_LargeToken      = FindAndLog("FirstTime_LargeToken");
            s_eDialogue1_LargeToken      = FindAndLog("FirstTime_Dialogue1_LargeToken");
            s_eDialogue2_LargeToken      = FindAndLog("FirstTime_Dialogue2_LargeToken");
            s_eAfterFirstTime_LargeToken = FindAndLog("AfterFirstTime_LargeToken");

            s_eFirstTime_SmallToken      = FindAndLog("FirstTime_SmallToken");
            s_eDialogue1_SmallToken      = FindAndLog("FirstTime_Dialogue1_SmallToken");
            s_eDialogue2_SmallToken      = FindAndLog("FirstTime_Dialogue2_SmallToken");
            s_eAfterFirstTime_SmallToken = FindAndLog("AfterFirstTime_SmallToken");

            s_eFirstTime_Talisman        = FindAndLog("FirstTime_Talisman");
            s_eDialogue1_Talisman        = FindAndLog("FirstTime_Dialogue1_Talisman");
            s_eDialogue2_Talisman        = FindAndLog("FirstTime_Dialogue2_Talisman");
            s_eDialogue3_Talisman        = FindAndLog("FirstTime_Dialogue3_Talisman");
            s_eAfterFirstTime_Talisman   = FindAndLog("AfterFirstTime_Talisman");

            s_eTutorialDimBG             = FindAndLog("Tutorial_BlackBackground");

            s_entitiesResolved = true;
        }

        private static ulong FindAndLog(string entityName)
        {
            ulong id = API.FindEntity(entityName);
            if (id == 0)
                API.Log($"[TutorialManager] WARNING: Could not find sprite entity '{entityName}'");
            else
                API.Log($"[TutorialManager] Resolved '{entityName}' -> ID {id}");
            return id;
        }
    }
}
