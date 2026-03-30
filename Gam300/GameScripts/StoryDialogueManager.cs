using System;
using Boom;

namespace GameScripts
{
    public static class StoryDialogueManager
    {
        public enum SequenceType { None, StartCutscene, Checkpoint1, Checkpoint2, BossCheckpoint, BossTransition, TutorialZone, TutorialZoneDeath, Generic }
        
        private static SequenceType s_activeSequence = SequenceType.None;
        private static int s_dialogueIndex = 0;

        // Start Cutscene UI
        private static ulong s_startD1 = 0;
        private static ulong s_startD2 = 0;
        private static ulong s_startD3 = 0;
        private static ulong s_startD4 = 0;
        private static ulong s_startD5 = 0;
        private static ulong s_startD6 = 0;
        private static ulong s_breakOut = 0;
        private static ulong s_guardD1 = 0;
        private static ulong s_guardD2 = 0;

        // Checkpoint 1 UI
        private static ulong s_cp1D1 = 0;
        private static ulong s_cp1D2 = 0;

        // Checkpoint 2 UI
        private static ulong s_cp2D1 = 0;
        private static ulong s_cp2D2 = 0;
        private static ulong s_cp2D3 = 0;
        private static ulong s_cp2D4 = 0;
        private static ulong s_cp2D5 = 0;
        private static ulong s_redMeter = 0;
        private static ulong s_tutorialBlackBackground = 0;

        // Boss Checkpoint UI
        private static ulong s_bossCpD1 = 0;
        private static ulong s_bossCpD2 = 0;
        private static ulong s_bossCpD3 = 0;
        private static ulong s_bossCpD4 = 0;
        private static ulong s_bossCpD5 = 0;
        private static ulong s_tutorialWhiteBackground = 0;

        // Tutorial Zone UI
        private static ulong s_tzD1 = 0;
        private static ulong s_tzD2 = 0;
        private static ulong s_tzD3 = 0;
        private static ulong s_tzD4 = 0;
        private static ulong s_tzD5 = 0;

        // Tutorial Zone Death UI (shown after repeated catches)
        private static ulong s_tzDeathA1 = 0; // 3rd catch, panel 1
        private static ulong s_tzDeathA2 = 0; // 3rd catch, panel 2
        private static ulong s_tzDeathB1 = 0; // 6th catch, panel 1
        private static ulong s_tzDeathB2 = 0; // 6th catch, panel 2
        private static int s_tzDeathThreshold = 0;

        // Generic sequence (used by TutorialDialogueTrigger)
        private static ulong[] s_genericEntities = new ulong[0];
        private static int s_genericCount = 0;

        private static bool s_entitiesResolved = false;

        private enum FadeMode { None, FadeIn, FadeOut }
        private static FadeMode s_fadeMode = FadeMode.None;
        private static float s_fadeTimer = 0f;
        private const float FADE_DURATION = 0.15f;
        private static ulong s_fadingEntity = 0;
        
        private static Action s_pendingAction = null;

        private static bool s_enterWasDown = false;
        private static bool s_eWasDown = false;
        private static bool s_aButtonWasDown = false;
        private static bool s_justDismissed = false;
        private static Action s_onSequenceComplete = null;

        public static void Reset()
        {
            s_activeSequence = SequenceType.None;
            s_dialogueIndex = 0;
            s_entitiesResolved = false;
            
            s_fadeMode = FadeMode.None;
            s_fadeTimer = 0f;
            s_fadingEntity = 0;
            s_pendingAction = null;
            
            s_enterWasDown = false;
            s_eWasDown = false;
            s_aButtonWasDown = false;
            s_justDismissed = false;
            s_onSequenceComplete = null;

            s_startD1 = 0; s_startD2 = 0; s_startD3 = 0; s_startD4 = 0; s_startD5 = 0; s_startD6 = 0;
            s_breakOut = 0; s_guardD1 = 0; s_guardD2 = 0;
            s_cp1D1 = 0; s_cp1D2 = 0;
            s_cp2D1 = 0; s_cp2D2 = 0; s_cp2D3 = 0; s_cp2D4 = 0; s_cp2D5 = 0; s_redMeter = 0; s_tutorialBlackBackground = 0;
            s_bossCpD1 = 0; s_bossCpD2 = 0; s_bossCpD3 = 0; s_bossCpD4 = 0; s_bossCpD5 = 0; s_tutorialWhiteBackground = 0;
            s_tzD1 = 0; s_tzD2 = 0; s_tzD3 = 0; s_tzD4 = 0; s_tzD5 = 0;
            s_tzDeathA1 = 0; s_tzDeathA2 = 0; s_tzDeathB1 = 0; s_tzDeathB2 = 0;
            s_tzDeathThreshold = 0;
            s_genericEntities = new ulong[0];
            s_genericCount = 0;
        }

        public static void PlayStartSequence(Action onComplete = null)
        {
            if (s_activeSequence != SequenceType.None) return;
            ResolveEntities();
            s_activeSequence = SequenceType.StartCutscene;
            s_dialogueIndex = 1;
            s_onSequenceComplete = onComplete;

            s_enterWasDown = true;
            s_eWasDown = true;
            s_aButtonWasDown = true;

            FadeInEntity(s_startD1);
            API.SetGameLogicPaused(true);
            API.SetCutsceneMode(true);
            API.Log("[StoryDialogueManager] Playing Start Sequence");
        }

        public static void PlayCheckpoint1Sequence(Action onComplete = null)
        {
            if (s_activeSequence != SequenceType.None) return;
            ResolveEntities();
            s_activeSequence = SequenceType.Checkpoint1;
            s_dialogueIndex = 1;
            s_onSequenceComplete = onComplete;

            s_enterWasDown = true;
            s_aButtonWasDown = true;

            FadeInEntity(s_cp1D1);
            API.SetGameLogicPaused(true);
            API.SetCutsceneMode(true);
            API.Log("[StoryDialogueManager] Playing Checkpoint 1 Sequence");
        }

        public static void PlayCheckpoint2Sequence(Action onComplete = null)
        {
            if (s_activeSequence != SequenceType.None) return;
            ResolveEntities();
            s_activeSequence = SequenceType.Checkpoint2;
            s_dialogueIndex = 1;
            s_onSequenceComplete = onComplete;

            s_enterWasDown = true;
            s_aButtonWasDown = true;

            FadeInEntity(s_cp2D1);
            API.SetGameLogicPaused(true);
            API.SetCutsceneMode(true);
            API.Log("[StoryDialogueManager] Playing Checkpoint 2 Sequence");
        }

        public static void PlayBossCheckpointSequence(Action onComplete = null)
        {
            if (s_activeSequence != SequenceType.None) return;
            ResolveEntities();
            s_activeSequence = SequenceType.BossCheckpoint;
            s_dialogueIndex = 1;
            s_onSequenceComplete = onComplete;

            s_enterWasDown = true;
            s_aButtonWasDown = true;

            FadeInEntity(s_bossCpD1);
            API.SetGameLogicPaused(true);
            API.SetCutsceneMode(true);
            API.Log("[StoryDialogueManager] Playing Boss Checkpoint Sequence");
        }

        public static void PlayBossTransitionSequence(Action onComplete = null)
        {
            if (s_activeSequence != SequenceType.None) return;
            ResolveEntities();
            s_activeSequence = SequenceType.BossTransition;
            s_dialogueIndex = 4;
            s_onSequenceComplete = onComplete;

            s_enterWasDown = true;
            s_aButtonWasDown = true;

            SetAlpha(s_tutorialWhiteBackground, 1f);
            FadeInEntity(s_bossCpD4);
            API.SetGameLogicPaused(true);
            API.SetCutsceneMode(true);
            API.Log("[StoryDialogueManager] Playing Boss Transition Sequence");
        }

        public static void PlayTutorialZoneSequence(Action onComplete = null)
        {
            if (s_activeSequence != SequenceType.None) return;
            ResolveEntities();
            s_activeSequence = SequenceType.TutorialZone;
            s_dialogueIndex = 1;
            s_onSequenceComplete = onComplete;

            s_enterWasDown = true;
            s_eWasDown = true;
            s_aButtonWasDown = true;

            FadeInEntity(s_tzD1);
            API.SetGameLogicPaused(true);
            API.SetCutsceneMode(true);
            API.Log("[StoryDialogueManager] Playing Tutorial Zone Sequence");
        }

        public static void PlayTutorialZoneDeathSequence(int deathCount, Action onComplete = null)
        {
            if (s_activeSequence != SequenceType.None) return;
            ResolveEntities();
            s_tzDeathThreshold = deathCount;
            s_activeSequence = SequenceType.TutorialZoneDeath;
            s_dialogueIndex = 1;
            s_onSequenceComplete = onComplete;

            s_enterWasDown = true;
            s_eWasDown = true;
            s_aButtonWasDown = true;

            ulong first = (deathCount == 3) ? s_tzDeathA1 : s_tzDeathB1;
            FadeInEntity(first);
            API.SetGameLogicPaused(true);
            API.SetCutsceneMode(true);
            API.Log($"[StoryDialogueManager] Playing Tutorial Zone Death Sequence (count={deathCount})");
        }

        public static void PlayGenericSequence(string[] entityNames, Action onComplete = null)
        {
            if (s_activeSequence != SequenceType.None) return;
            if (entityNames == null || entityNames.Length == 0) { onComplete?.Invoke(); return; }

            s_genericCount = entityNames.Length;
            s_genericEntities = new ulong[s_genericCount];
            for (int i = 0; i < s_genericCount; i++)
            {
                s_genericEntities[i] = API.FindEntity(entityNames[i]);
                if (s_genericEntities[i] == 0)
                    API.Log($"[StoryDialogueManager] WARNING: Generic dialogue entity '{entityNames[i]}' not found");
                else
                    SetAlpha(s_genericEntities[i], 0f);
            }

            s_activeSequence = SequenceType.Generic;
            s_dialogueIndex = 0;
            s_onSequenceComplete = onComplete;

            s_enterWasDown = true;
            s_eWasDown = true;
            s_aButtonWasDown = true;

            FadeInEntity(s_genericEntities[0]);
            API.SetGameLogicPaused(true);
            API.SetCutsceneMode(true);
            API.Log($"[StoryDialogueManager] Playing Generic Sequence ({s_genericCount} panels)");
        }

        public static bool IsSequenceActive()
        {
            return s_activeSequence != SequenceType.None;
        }

        public static bool WasJustDismissed()
        {
            return s_justDismissed;
        }

        public static void Update(float dt)
        {
            s_justDismissed = false;
            if (s_fadeMode != FadeMode.None)
            {
                s_fadeTimer += dt;
                float t = Math.Min(1f, s_fadeTimer / FADE_DURATION);

                if (s_fadeMode == FadeMode.FadeIn)
                {
                    SetAlpha(s_fadingEntity, t);
                    if (t >= 1f)
                    {
                        s_fadeMode = FadeMode.None;
                        s_fadeTimer = 0f;
                        s_fadingEntity = 0;
                    }
                }
                else
                {
                    SetAlpha(s_fadingEntity, 1f - t);
                    if (t >= 1f)
                    {
                        SetAlpha(s_fadingEntity, 0f);
                        s_fadeMode = FadeMode.None;
                        s_fadeTimer = 0f;
                        s_fadingEntity = 0;
                        
                        Action pending = s_pendingAction;
                        s_pendingAction = null;
                        pending?.Invoke();
                    }
                }
            }

            if (s_activeSequence == SequenceType.None || s_fadeMode != FadeMode.None) return;

            bool enterDown = API.IsKeyDown(API.KEY_SPACE);
            bool eDown = API.IsKeyDown(API.KEY_E);
            bool aButtonDown = API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A);

            bool enterPressed = enterDown && !s_enterWasDown;
            bool ePressed = eDown && !s_eWasDown;
            bool aButtonPressed = aButtonDown && !s_aButtonWasDown;

            s_enterWasDown = enterDown;
            s_eWasDown = eDown;
            s_aButtonWasDown = aButtonDown;

            if (s_activeSequence == SequenceType.StartCutscene)
            {
                if (enterPressed || aButtonPressed)
                {
                    API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);
                    AdvanceStartSequence();
                }
            }
            else if (s_activeSequence == SequenceType.Checkpoint1)
            {
                if (enterPressed || aButtonPressed)
                {
                    API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);
                    AdvanceCheckpoint1Sequence();
                }
            }
            else if (s_activeSequence == SequenceType.Checkpoint2)
            {
                if (enterPressed || aButtonPressed)
                {
                    API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);
                    AdvanceCheckpoint2Sequence();
                }
            }
            else if (s_activeSequence == SequenceType.BossCheckpoint)
            {
                if (enterPressed || aButtonPressed)
                {
                    API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);
                    AdvanceBossCheckpointSequence();
                }
            }
            else if (s_activeSequence == SequenceType.BossTransition)
            {
                if (enterPressed || aButtonPressed)
                {
                    API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);
                    AdvanceBossTransitionSequence();
                }
            }
            else if (s_activeSequence == SequenceType.TutorialZone)
            {
                if (enterPressed || aButtonPressed)
                {
                    API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);
                    AdvanceTutorialZoneSequence();
                }
            }
            else if (s_activeSequence == SequenceType.TutorialZoneDeath)
            {
                if (enterPressed || aButtonPressed)
                {
                    API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);
                    AdvanceTutorialZoneDeathSequence();
                }
            }
            else if (s_activeSequence == SequenceType.Generic)
            {
                if (enterPressed || aButtonPressed)
                {
                    API.PlaySound("sfx_ui_click", "Resources/Audio/uiClick.wav", false);
                    AdvanceGenericSequence();
                }
            }
        }

        private static void AdvanceStartSequence()
        {
            ulong currentEntity = GetStartSequenceEntity(s_dialogueIndex);
            FadeOutEntity(currentEntity, () =>
            {
                s_dialogueIndex++;
                if (s_dialogueIndex == 7) // Cutscene part 1 is done
                {
                    CloseSequence();
                }
                else if (s_dialogueIndex > 9)
                {
                    CloseSequence();
                }
                else
                {
                    FadeInEntity(GetStartSequenceEntity(s_dialogueIndex));
                }
            });
        }

        public static void PlayGuardSequence()
        {
            if (s_activeSequence != SequenceType.None) return;
            ResolveEntities();
            s_activeSequence = SequenceType.StartCutscene;
            s_dialogueIndex = 8; // Start at GuardD1

            s_enterWasDown = true;
            s_aButtonWasDown = true;

            FadeInEntity(s_guardD1);
            API.SetGameLogicPaused(true);
            API.SetCutsceneMode(true);
            API.Log("[StoryDialogueManager] Playing Guard Sequence");
        }

        private static void AdvanceCheckpoint1Sequence()
        {
            ulong currentEntity = s_dialogueIndex == 1 ? s_cp1D1 : s_cp1D2;
            FadeOutEntity(currentEntity, () =>
            {
                s_dialogueIndex++;
                if (s_dialogueIndex > 2)
                {
                    CloseSequence();
                }
                else
                {
                    FadeInEntity(s_cp1D2);
                }
            });
        }

        private static void AdvanceCheckpoint2Sequence()
        {
            ulong currentEntity = 0;
            switch(s_dialogueIndex)
            {
                case 1: currentEntity = s_cp2D1; break;
                case 2: currentEntity = s_cp2D2; break;
                case 3: currentEntity = s_cp2D3; break;
                case 4: currentEntity = s_cp2D4; break;
                case 5: currentEntity = s_cp2D5; break;
            }

            if (s_dialogueIndex == 4)
            {
                SetAlpha(s_redMeter, 0f);
                SetAlpha(s_tutorialBlackBackground, 0f);
            }

            FadeOutEntity(currentEntity, () =>
            {
                s_dialogueIndex++;
                if (s_dialogueIndex > 5)
                {
                    CloseSequence();
                }
                else
                {
                    ulong nextEntity = 0;
                    switch(s_dialogueIndex)
                    {
                        case 2: nextEntity = s_cp2D2; break;
                        case 3: nextEntity = s_cp2D3; break;
                        case 4: nextEntity = s_cp2D4; break;
                        case 5: nextEntity = s_cp2D5; break;
                    }
                    if (s_dialogueIndex == 4)
                    {
                        SetAlpha(s_redMeter, 1f);
                        SetAlpha(s_tutorialBlackBackground, 1f);
                    }
                    FadeInEntity(nextEntity);
                }
            });
        }

        private static void AdvanceBossCheckpointSequence()
        {
            ulong currentEntity = 0;
            switch(s_dialogueIndex)
            {
                case 1: currentEntity = s_bossCpD1; break;
                case 2: currentEntity = s_bossCpD2; break;
                case 3: currentEntity = s_bossCpD3; break;
            }

            FadeOutEntity(currentEntity, () =>
            {
                s_dialogueIndex++;
                if (s_dialogueIndex > 3)
                {
                    CloseSequence();
                }
                else
                {
                    ulong nextEntity = 0;
                    switch(s_dialogueIndex)
                    {
                        case 2: nextEntity = s_bossCpD2; break;
                        case 3: nextEntity = s_bossCpD3; break;
                    }
                    FadeInEntity(nextEntity);
                }
            });
        }

        private static void AdvanceBossTransitionSequence()
        {
            ulong currentEntity = s_dialogueIndex == 4 ? s_bossCpD4 : s_bossCpD5;

            FadeOutEntity(currentEntity, () =>
            {
                s_dialogueIndex++;
                if (s_dialogueIndex > 5)
                {
                    SetAlpha(s_tutorialWhiteBackground, 0f);
                    CloseSequence();
                }
                else
                {
                    FadeInEntity(s_bossCpD5);
                }
            });
        }

        private static void AdvanceTutorialZoneSequence()
        {
            ulong currentEntity = 0;
            switch (s_dialogueIndex)
            {
                case 1: currentEntity = s_tzD1; break;
                case 2: currentEntity = s_tzD2; break;
                case 3: currentEntity = s_tzD3; break;
                case 4: currentEntity = s_tzD4; break;
                case 5: currentEntity = s_tzD5; break;
            }

            FadeOutEntity(currentEntity, () =>
            {
                s_dialogueIndex++;
                if (s_dialogueIndex > 5)
                {
                    CloseSequence();
                }
                else
                {
                    ulong nextEntity = 0;
                    switch (s_dialogueIndex)
                    {
                        case 2: nextEntity = s_tzD2; break;
                        case 3: nextEntity = s_tzD3; break;
                        case 4: nextEntity = s_tzD4; break;
                        case 5: nextEntity = s_tzD5; break;
                    }
                    FadeInEntity(nextEntity);
                }
            });
        }

        private static void AdvanceTutorialZoneDeathSequence()
        {
            ulong panel1 = (s_tzDeathThreshold == 3) ? s_tzDeathA1 : s_tzDeathB1;
            ulong panel2 = (s_tzDeathThreshold == 3) ? s_tzDeathA2 : s_tzDeathB2;
            ulong current = (s_dialogueIndex == 1) ? panel1 : panel2;

            FadeOutEntity(current, () =>
            {
                s_dialogueIndex++;
                if (s_dialogueIndex > 2)
                    CloseSequence();
                else
                    FadeInEntity(panel2);
            });
        }

        private static void AdvanceGenericSequence()
        {
            ulong current = s_genericEntities[s_dialogueIndex];
            FadeOutEntity(current, () =>
            {
                s_dialogueIndex++;
                if (s_dialogueIndex >= s_genericCount)
                    CloseSequence();
                else
                    FadeInEntity(s_genericEntities[s_dialogueIndex]);
            });
        }

        private static void CloseSequence()
        {
            s_activeSequence = SequenceType.None;
            s_justDismissed = true;
            API.SetGameLogicPaused(false);
            API.SetCutsceneMode(false);
            API.Log("[StoryDialogueManager] Sequence closed");

            Action cb = s_onSequenceComplete;
            s_onSequenceComplete = null;
            cb?.Invoke();
        }

        private static ulong GetStartSequenceEntity(int index)
        {
            switch (index)
            {
                case 1: return s_startD1;
                case 2: return s_startD2;
                case 3: return s_startD3;
                case 4: return s_startD4;
                case 5: return s_startD5;
                case 6: return s_startD6;
                case 7: return s_breakOut;
                case 8: return s_guardD1;
                case 9: return s_guardD2;
                default: return 0;
            }
        }

        private static void FadeInEntity(ulong entity)
        {
            if (entity == 0) return;
            SetAlpha(entity, 0f);
            s_fadingEntity = entity;
            s_fadeMode = FadeMode.FadeIn;
            s_fadeTimer = 0f;
        }

        private static void FadeOutEntity(ulong entity, Action onDone)
        {
            if (entity == 0)
            {
                onDone?.Invoke();
                return;
            }
            s_fadingEntity = entity;
            s_fadeMode = FadeMode.FadeOut;
            s_fadeTimer = 0f;
            s_pendingAction = onDone;
        }

        private static void SetAlpha(ulong entity, float alpha)
        {
            if (entity != 0 && API.HasSprite(entity))
            {
                API.SetSpriteAlpha(entity, alpha);
            }
        }

        private static void ResolveEntities()
        {
            if (s_entitiesResolved) return;

            s_startD1 = FindEntity("StartCutscene_Dialogue1");
            s_startD2 = FindEntity("StartCutscene_Dialogue2");
            s_startD3 = FindEntity("StartCutscene_Dialogue3");
            s_startD4 = FindEntity("StartCutscene_Dialogue4");
            s_startD5 = FindEntity("StartCutscene_Dialogue5");
            s_startD6 = FindEntity("StartCutscene_Dialogue6");
            s_breakOut = FindEntity("E_BreakOut");
            s_guardD1 = FindEntity("EncounterPrisonGuard_Dialogue1");
            s_guardD2 = FindEntity("EncounterPrisonGuard_Dialogue2");

            s_cp1D1 = FindEntity("Checkpoint1_Dialogue1");
            s_cp1D2 = FindEntity("Checkpoint1_Dialogue2");

            s_cp2D1 = FindEntity("Checkpoint2_Dialogue1");
            s_cp2D2 = FindEntity("Checkpoint2_Dialogue2");
            s_cp2D3 = FindEntity("Checkpoint2_Dialogue3");
            s_cp2D4 = FindEntity("Checkpoint2_Dialogue4");
            s_cp2D5 = FindEntity("Checkpoint2_Dialogue5");
            s_redMeter = FindEntity("RedMeter");
            s_tutorialBlackBackground = FindEntity("Tutorial_BlackBackground");
            s_bossCpD1 = FindEntity("BossCheckpoint_Dialogue1");
            s_bossCpD2 = FindEntity("BossCheckpoint_Dialogue2");
            s_bossCpD3 = FindEntity("BossCheckpoint_Dialogue3");
            s_bossCpD4 = FindEntity("BossCheckpoint_Dialogue4");
            s_bossCpD5 = FindEntity("BossCheckpoint_Dialogue5");
            s_tutorialWhiteBackground = FindEntity("Tutorial_WhiteBackground");

            s_tzD1 = FindEntity("TutorialZone_Dialogue1");
            s_tzD2 = FindEntity("TutorialZone_Dialogue2");
            s_tzD3 = FindEntity("TutorialZone_Dialogue3");
            s_tzD4 = FindEntity("TutorialZone_Dialogue4");
            s_tzD5 = FindEntity("TutorialZone_Dialogue5");

            s_tzDeathA1 = FindEntity("TutorialZone_Death3_Dialogue1");
            s_tzDeathA2 = FindEntity("TutorialZone_Death3_Dialogue2");
            s_tzDeathB1 = FindEntity("TutorialZone_Death6_Dialogue1");
            s_tzDeathB2 = FindEntity("TutorialZone_Death6_Dialogue2");

            // Hide all initially
            SetAlpha(s_startD1, 0f); SetAlpha(s_startD2, 0f); SetAlpha(s_startD3, 0f);
            SetAlpha(s_startD4, 0f); SetAlpha(s_startD5, 0f); SetAlpha(s_startD6, 0f);
            SetAlpha(s_breakOut, 0f); SetAlpha(s_guardD1, 0f); SetAlpha(s_guardD2, 0f);
            SetAlpha(s_cp1D1, 0f); SetAlpha(s_cp1D2, 0f);
            SetAlpha(s_cp2D1, 0f); SetAlpha(s_cp2D2, 0f); SetAlpha(s_cp2D3, 0f);
            SetAlpha(s_cp2D4, 0f); SetAlpha(s_cp2D5, 0f); SetAlpha(s_redMeter, 0f);
            SetAlpha(s_tutorialBlackBackground, 0f);
            SetAlpha(s_bossCpD1, 0f); SetAlpha(s_bossCpD2, 0f); SetAlpha(s_bossCpD3, 0f);
            SetAlpha(s_bossCpD4, 0f); SetAlpha(s_bossCpD5, 0f); SetAlpha(s_tutorialWhiteBackground, 0f);
            SetAlpha(s_tzD1, 0f); SetAlpha(s_tzD2, 0f); SetAlpha(s_tzD3, 0f); SetAlpha(s_tzD4, 0f); SetAlpha(s_tzD5, 0f);
            SetAlpha(s_tzDeathA1, 0f); SetAlpha(s_tzDeathA2, 0f);
            SetAlpha(s_tzDeathB1, 0f); SetAlpha(s_tzDeathB2, 0f);

            s_entitiesResolved = true;
        }

        private static ulong FindEntity(string name)
        {
            ulong id = API.FindEntity(name);
            if (id == 0) API.Log($"[StoryDialogueManager] WARNING: UI entity '{name}' not found");
            return id;
        }
    }
}
