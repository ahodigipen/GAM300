using System;
using Boom;

namespace GameScripts
{
    public static class CrouchTutorialManager
    {
        private enum TutorialState
        {
            None,
            ShowingDialogue1,
            ShowingDialogue2,
            ShowingDialogue3,
            ShowingDialogue4
        }

        private static TutorialState s_state = TutorialState.None;
        private static bool s_hasCompletedTutorial = false;

        // Cached sprite entity IDs
        private static ulong s_eDialogue1 = 0;
        private static ulong s_eDialogue2 = 0;
        private static ulong s_eDialogue3 = 0;
        private static ulong s_eDialogue4 = 0;

        // Background dimming (reused from main tutorial manager but tracked separately if needed)
        private static ulong s_eTutorialDimBG = 0;
        private const float DIM_BG_ALPHA = 1.0f;

        private static bool s_entitiesResolved = false;

        // Fade state
        private enum FadeMode { None, FadeIn, FadeOut }
        private static FadeMode s_fadeMode = FadeMode.None;
        private static float s_fadeTimer = 0f;
        private const float FADE_DURATION = 0.25f;
        private static ulong s_fadingEntity = 0;
        private static System.Action s_pendingAfterFadeOut = null;

        // Frame-level flags
        private static bool s_justDismissed = false;
        private static bool s_enterWasDown = false;
        private static bool s_aButtonWasDown = false;

        public static bool HasCompletedTutorial() => s_hasCompletedTutorial;
        public static bool IsTutorialActive() => s_state != TutorialState.None;
        public static bool WasJustDismissed() => s_justDismissed;

        public static void ShowTutorial()
        {
            if (s_hasCompletedTutorial || s_state != TutorialState.None) return;

            ResolveEntities();

            s_enterWasDown = true;
            s_aButtonWasDown = true;

            FadeInEntity(s_eDialogue1);

            if (s_eTutorialDimBG != 0 && API.HasSprite(s_eTutorialDimBG))
                API.SetSpriteAlpha(s_eTutorialDimBG, DIM_BG_ALPHA);

            s_state = TutorialState.ShowingDialogue1;
            API.Log("[CrouchTutorialManager] First-time crouch tutorial started. Showing Dialogue 1.");

            API.SetGameLogicPaused(true);
        }

        public static void Reset()
        {
            if (s_state != TutorialState.None)
            {
                SetEntityAlpha(s_eDialogue1, 0f);
                SetEntityAlpha(s_eDialogue2, 0f);
                SetEntityAlpha(s_eDialogue3, 0f);
                SetEntityAlpha(s_eDialogue4, 0f);
            }
            if (s_fadingEntity != 0) SetEntityAlpha(s_fadingEntity, 0f);

            s_state = TutorialState.None;
            s_justDismissed = false;
            s_enterWasDown = false;
            s_aButtonWasDown = false;
            s_entitiesResolved = false;
            s_fadeMode = FadeMode.None;
            s_fadeTimer = 0f;
            s_fadingEntity = 0;
            s_pendingAfterFadeOut = null;

            // Keep s_hasCompletedTutorial between resets if we don't want the player to see it again after dying
            // but for safety/consistency with the current game structure, we might want to let them see it again if they restarted the level.
            s_hasCompletedTutorial = false;

            s_eDialogue1 = 0;
            s_eDialogue2 = 0;
            s_eDialogue3 = 0;
            s_eDialogue4 = 0;
            s_eTutorialDimBG = 0;

            API.Log("[CrouchTutorialManager] Reset");
        }

        public static void Update(float dt)
        {
            s_justDismissed = false;

            // Tick fade
            if (s_fadeMode != FadeMode.None)
            {
                s_fadeTimer += dt;
                float t = Math.Min(1f, s_fadeTimer / FADE_DURATION);

                if (s_fadeMode == FadeMode.FadeIn)
                {
                    SetEntityAlpha(s_fadingEntity, t);
                    if (t >= 1f)
                    {
                        s_fadeMode = FadeMode.None;
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
                        s_fadeMode = FadeMode.None;
                        s_fadeTimer = 0f;
                        s_fadingEntity = 0;

                        System.Action pending = s_pendingAfterFadeOut;
                        s_pendingAfterFadeOut = null;
                        pending?.Invoke();
                    }
                }
            }

            if (s_state == TutorialState.None) return;
            if (s_fadeMode != FadeMode.None) return; // Block input while fading

            // Input edge detection
            bool enterDown = API.IsKeyDown(API.KEY_ENTER);
            bool aButtonDown = API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A);

            bool enterPressed = enterDown && !s_enterWasDown;
            bool aButtonPressed = aButtonDown && !s_aButtonWasDown;

            s_enterWasDown = enterDown;
            s_aButtonWasDown = aButtonDown;

            if (!enterPressed && !aButtonPressed) return;

            AdvanceState();
        }

        private static void AdvanceState()
        {
            switch (s_state)
            {
                case TutorialState.ShowingDialogue1: TransitionTo(TutorialState.ShowingDialogue2); break;
                case TutorialState.ShowingDialogue2: TransitionTo(TutorialState.ShowingDialogue3); break;
                case TutorialState.ShowingDialogue3: TransitionTo(TutorialState.ShowingDialogue4); break;
                case TutorialState.ShowingDialogue4: CloseTutorial(); break;
            }
        }

        private static void TransitionTo(TutorialState next)
        {
            ulong currentEntity = GetEntityForState(s_state);
            TutorialState capturedNext = next;

            FadeOutEntity(currentEntity, () =>
            {
                s_state = capturedNext;
                ulong nextEntity = GetEntityForState(capturedNext);
                FadeInEntity(nextEntity);
                API.Log($"[CrouchTutorialManager] Advancing to state {capturedNext}.");
            });
        }

        private static void CloseTutorial()
        {
            ulong currentEntity = GetEntityForState(s_state);

            FadeOutEntity(currentEntity, () =>
            {
                if (s_eTutorialDimBG != 0 && API.HasSprite(s_eTutorialDimBG))
                    API.SetSpriteAlpha(s_eTutorialDimBG, 0f);

                s_state = TutorialState.None;
                s_justDismissed = true;
                s_hasCompletedTutorial = true;

                // Unpause the game
                API.SetGameLogicPaused(false);

                // Automatically show the "CTRL_Crouch" prompt right after closing 
                // since they are still standing in the trigger zone.
                UIManager.ShowHoldPrompt();

                API.Log($"[CrouchTutorialManager] Tutorial closed. Game resumed.");
            });
        }

        private static void FadeInEntity(ulong entity)
        {
            if (entity == 0) return;
            SetEntityAlpha(entity, 0f);
            s_fadingEntity = entity;
            s_fadeMode = FadeMode.FadeIn;
            s_fadeTimer = 0f;
        }

        private static void FadeOutEntity(ulong entity, System.Action onDone)
        {
            if (entity == 0)
            {
                onDone?.Invoke();
                return;
            }
            s_fadingEntity = entity;
            s_fadeMode = FadeMode.FadeOut;
            s_fadeTimer = 0f;
            s_pendingAfterFadeOut = onDone;
        }

        private static void SetEntityAlpha(ulong entity, float alpha)
        {
            if (entity == 0) return;
            if (API.HasSprite(entity))
                API.SetSpriteAlpha(entity, alpha);
        }

        private static ulong GetEntityForState(TutorialState state)
        {
            switch (state)
            {
                case TutorialState.ShowingDialogue1: return s_eDialogue1;
                case TutorialState.ShowingDialogue2: return s_eDialogue2;
                case TutorialState.ShowingDialogue3: return s_eDialogue3;
                case TutorialState.ShowingDialogue4: return s_eDialogue4;
            }
            return 0;
        }

        private static void ResolveEntities()
        {
            if (s_entitiesResolved) return;

            s_eDialogue1 = FindAndLog("Crouch_FirstTime_Dialogue1");
            s_eDialogue2 = FindAndLog("Crouch_FirstTime_Dialogue2");
            s_eDialogue3 = FindAndLog("Crouch_FirstTime_Dialogue3");
            s_eDialogue4 = FindAndLog("Crouch_FirstTime_Dialogue4");

            // Re-using the same darkened background sprite from the standard tutorials
            s_eTutorialDimBG = FindAndLog("Tutorial_BlackBackground");

            s_entitiesResolved = true;
        }

        private static ulong FindAndLog(string entityName)
        {
            ulong id = API.FindEntity(entityName);
            if (id == 0)
                API.Log($"[CrouchTutorialManager] WARNING: Could not find sprite entity '{entityName}'");
            else
                API.Log($"[CrouchTutorialManager] Resolved '{entityName}' -> ID {id}");
            return id;
        }
    }
}
