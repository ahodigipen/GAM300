using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// PrisonBreak: When the player enters the trigger zone and presses E,
    /// the screen fades to black, two objects are moved (one with rigidbody,
    /// one without), an audio clip plays, then the screen fades back in.
    /// One-time use only.
    /// </summary>
    public class PrisonBreak
    {
        public ulong Entity;

        // ─── Inspector Fields ───────────────────────────────────────────────

        [Boom.EditorExposed("Non-RB Object Name", "Name of the object WITHOUT a rigidbody (teleported to target position)")]
        private string _nonRbObjectName = "PrisonDoor";

        [Boom.EditorExposed("RB Object Name", "Name of the object WITH a rigidbody (Y set to -20)")]
        private string _rbObjectName = "PrisonBars";

        [Boom.EditorExposed("Audio Clip Path", "Path to the audio file to play during the sequence")]
        private string _audioClipPath = "Resources/Audio/prison_break.wav";

        [Boom.EditorExposed("Fade Out Duration", "Seconds to fade to black", 0.1f, 5f, true)]
        private float _fadeOutDuration = 0.5f;

        [Boom.EditorExposed("Black Hold Duration", "Seconds to hold on the black screen before fading in", 0.1f, 5f, true)]
        private float _blackHoldDuration = 0.5f;

        [Boom.EditorExposed("Fade In Duration", "Seconds to fade back in from black", 0.1f, 5f, true)]
        private float _fadeInDuration = 0.75f;

        // ─── State ──────────────────────────────────────────────────────────

        private static readonly Dictionary<ulong, PrisonBreak> s_instances = new Dictionary<ulong, PrisonBreak>();

        private bool _playerInZone = false;
        private bool _activated    = false;
        private bool _wasEPressed  = false;

        private enum FadeState { None, FadingOut, BlackHold, FadingIn, Done }
        private FadeState _fadeState = FadeState.None;
        private float _fadeTimer = 0f;

        // ─── Lifecycle ──────────────────────────────────────────────────────

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;
            ScriptRegistry.ApplyParamsToExposedFields(this, jsonParams);

            if (!API.HasCollider(Entity))
            {
                API.Log("[PrisonBreak] WARNING: Entity has no collider – trigger will not work.");
                return;
            }

            if (!API.IsTrigger(Entity))
                API.SetTrigger(Entity, true);

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log("[PrisonBreak] Ready. Player must enter the zone and press E.");
        }

        public void OnUpdate(float dt)
        {
            // ── Fade / sequence state machine ────────────────────────────
            if (_fadeState != FadeState.None && _fadeState != FadeState.Done)
            {
                _fadeTimer += dt;

                switch (_fadeState)
                {
                    case FadeState.FadingOut:
                    {
                        float alpha = Clamp01(_fadeTimer / _fadeOutDuration);
                        API.SetScreenFadeAlpha(alpha);
                        if (_fadeTimer >= _fadeOutDuration)
                        {
                            API.SetScreenFadeAlpha(1f);
                            _fadeTimer = 0f;
                            _fadeState = FadeState.BlackHold;

                            // Move objects while screen is fully black
                            MoveObjects();

                            // Play audio at the moment of the blackout
                            API.PreloadSound("PrisonBreak_SFX", _audioClipPath);
                            API.PlaySound("PrisonBreak_SFX", _audioClipPath, false);
                        }
                        break;
                    }

                    case FadeState.BlackHold:
                    {
                        if (_fadeTimer >= _blackHoldDuration)
                        {
                            _fadeTimer = 0f;
                            _fadeState = FadeState.FadingIn;
                        }
                        break;
                    }

                    case FadeState.FadingIn:
                    {
                        float alpha = 1f - Clamp01(_fadeTimer / _fadeInDuration);
                        API.SetScreenFadeAlpha(alpha);
                        if (_fadeTimer >= _fadeInDuration)
                        {
                            API.SetScreenFadeAlpha(0f);
                            _fadeState = FadeState.Done;
                            API.Log("[PrisonBreak] Sequence complete.");
                            StoryDialogueManager.PlayGuardSequence();
                        }
                        break;
                    }
                }

                // Don't check input while the sequence is running
                return;
            }

            // ── Interaction detection (F key or Gamepad A) ───────────────────────────
            bool interactDown = API.IsKeyDown(API.KEY_F) || (API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A));

            if (_playerInZone && !_activated && interactDown && !_wasEPressed)
                Activate();

            _wasEPressed = interactDown;
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity))
                s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        // ─── Private Logic ───────────────────────────────────────────────────

        private void Activate()
        {
            _activated = true;
            ulong eBreakOutID = API.FindEntity("E_BreakOut");
            if (eBreakOutID != 0) API.SetSpriteAlpha(eBreakOutID, 0f);
            
            _fadeState  = FadeState.FadingOut;
            _fadeTimer  = 0f;
            API.Log("[PrisonBreak] Activated – starting fade-to-black sequence.");
        }

        private void MoveObjects()
        {
            // Object WITHOUT rigidbody: teleport directly to target world position
            ulong nonRbID = API.FindEntity(_nonRbObjectName);
            if (nonRbID != 0)
            {
                API.SetPosition(nonRbID, new Vec3(-54.3f, 0.63f, 12.7f));
                API.Log($"[PrisonBreak] Moved '{_nonRbObjectName}' to (-54.3, 0.63, 12.7).");
            }
            else
            {
                API.Log($"[PrisonBreak] WARNING: Could not find non-RB object '{_nonRbObjectName}'.");
            }

            // Object WITH rigidbody: keep current X/Z, set Y to -20
            ulong rbID = API.FindEntity(_rbObjectName);
            if (rbID != 0)
            {
                Vec3 rbPos = API.GetPosition(rbID);
                rbPos.Y = -20f;
                API.TeleportRigidBody(rbID, rbPos);
                API.Log($"[PrisonBreak] Teleported '{_rbObjectName}' Y to -20.");
            }
            else
            {
                API.Log($"[PrisonBreak] WARNING: Could not find RB object '{_rbObjectName}'.");
            }
        }

        private static float Clamp01(float v)
        {
            return v < 0f ? 0f : (v > 1f ? 1f : v);
        }

        // ─── Trigger Callbacks ───────────────────────────────────────────────

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            PrisonBreak inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInZone = true;
            ulong eBreakOutID = API.FindEntity("E_BreakOut");
            if (eBreakOutID != 0 && !inst._activated) API.SetSpriteAlpha(eBreakOutID, 1f);
            API.Log("[PrisonBreak] Player entered zone. Press E to trigger.");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            PrisonBreak inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInZone = false;
            ulong eBreakOutID = API.FindEntity("E_BreakOut");
            if (eBreakOutID != 0 && !inst._activated) API.SetSpriteAlpha(eBreakOutID, 0f);
            API.Log("[PrisonBreak] Player left zone.");
        }
    }
}
