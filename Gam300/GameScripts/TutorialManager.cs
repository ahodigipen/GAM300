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
                // First-time sequence: Show item sprite AND Dialogue 1 simultaneously
                ulong itemSprite = GetEntityForState(TutorialState.ShowingFirstTimeItem, item);
                ShowSprite(itemSprite);

                ulong diag1Sprite = GetEntityForState(TutorialState.ShowingDialogue1, item);
                ShowSprite(diag1Sprite);

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
                ShowSprite(repeatSprite);
                s_state = TutorialState.ShowingRepeatItem;
                API.Log($"[TutorialManager] Repeat pickup #{pickupCount} for {item}. Showing repeat sprite.");
            }

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
            // Hide any currently visible sprites
            if (s_state != TutorialState.None)
            {
                HideCurrentSprite();
                // Also hide the base item sprite if it was a first-time sequence
                if (s_activeItem != default || s_state != TutorialState.None)
                {
                    HideSprite(GetEntityForState(TutorialState.ShowingFirstTimeItem, s_activeItem));
                }
            }

            s_state          = TutorialState.None;
            s_justDismissed  = false;
            s_enterWasDown   = false;
            s_aButtonWasDown = false;
            s_entitiesResolved = false;

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

            if (s_state == TutorialState.None) return;

            // --- Input edge detection: ENTER key (GLFW 257) and gamepad A button ---
            bool enterDown   = API.IsKeyDown(API.KEY_ENTER);
            bool aButtonDown = API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A);

            bool enterPressed   = enterDown   && !s_enterWasDown;
            bool aButtonPressed = aButtonDown && !s_aButtonWasDown;

            s_enterWasDown   = enterDown;
            s_aButtonWasDown = aButtonDown;

            if (!enterPressed && !aButtonPressed) return;

            // ENTER pressed — advance state machine
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
            // Hide current sprite
            HideCurrentSprite();

            // Show next sprite
            s_state = next;
            ulong nextEntity = GetEntityForState(next, s_activeItem);
            ShowSprite(nextEntity);

            API.Log($"[TutorialManager] Advancing to state {next} for {s_activeItem}.");
        }

        private static void CloseTutorial()
        {
            HideCurrentSprite();

            // Always ensure the base item sprite is hidden (it persists through first-time dialogues)
            ulong baseItemSprite = GetEntityForState(TutorialState.ShowingFirstTimeItem, s_activeItem);
            HideSprite(baseItemSprite);

            // Hide dimmed background
            if (s_eTutorialDimBG != 0 && API.HasSprite(s_eTutorialDimBG))
                API.SetSpriteAlpha(s_eTutorialDimBG, 0f);

            s_state = TutorialState.None;
            s_justDismissed = true;
            API.SetGameLogicPaused(false);
            API.Log($"[TutorialManager] Tutorial closed for {s_activeItem}. Game resumed.");
        }

        private static void HideCurrentSprite()
        {
            ulong entity = GetEntityForState(s_state, s_activeItem);
            HideSprite(entity);
        }

        private static void ShowSprite(ulong entity)
        {
            if (entity == 0) return;
            if (API.HasSprite(entity))
                API.SetSpriteAlpha(entity, 1f);
        }

        private static void HideSprite(ulong entity)
        {
            if (entity == 0) return;
            if (API.HasSprite(entity))
                API.SetSpriteAlpha(entity, 0f);
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
