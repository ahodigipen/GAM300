using Boom;

namespace GameScripts
{
    /// <summary>
    /// Lightweight animation driver for non-character entities (e.g. Hanging_Spawn, props).
    /// Attach to any entity that has a ModelComponent + AnimatorComponent.
    ///
    /// On Start it prints ALL clip names loaded in the AnimatorComponent to the console
    /// so you can see the exact spelling — copy-paste the name into DefaultClipName.
    /// </summary>
    public class SimpleAnimator
    {
        public ulong Entity;

        // ── Inspector-exposed properties ──────────────────────────────────────

        [Boom.EditorExposed("Default Clip Name",
            "Exact name of the clip to play on start (check console for available names).")]
        public string DefaultClipName = "";

        [Boom.EditorExposed("Play On Start",
            "Play DefaultClipName automatically when the scene starts.")]
        public bool PlayOnStart = true;

        [Boom.EditorExposed("Disable State Machine",
            "Disable Animator state-machine so this script has full control. " +
            "Recommended: true for cutscene-only objects.")]
        public bool DisableStateMachine = true;

        // ── Lifecycle ─────────────────────────────────────────────────────────

        public void OnStart(string _)
        {
            if (!API.HasAnimator(Entity))
            {
                API.Log($"[SimpleAnimator] Entity {Entity} has NO AnimatorComponent. " +
                        "Add one in the Inspector and make sure the skeletal FBX model is assigned.");
                return;
            }

            // ── List every clip so the user can verify the exact name ─────────
            int count = API.AnimatorGetClipCount(Entity);
            if (count == 0)
            {
                API.Log($"[SimpleAnimator] Entity {Entity}: AnimatorComponent has 0 clips. " +
                        "The FBX animation has not been imported into the AnimatorComponent yet. " +
                        "Load the model in the Animation Timeline panel, then Apply to entity.");
            }
            else
            {
                API.Log($"[SimpleAnimator] Entity {Entity}: Found {count} clip(s) in AnimatorComponent:");
                for (int i = 0; i < count; i++)
                {
                    string name = API.AnimatorGetClipName(Entity, i);
                    API.Log($"  [{i}] \"{name}\"");
                }
            }

            // ── Apply control settings ────────────────────────────────────────
            if (DisableStateMachine)
            {
                API.AnimatorSetStateMachineEnabled(Entity, false);
            }

            // ── Play the requested clip ───────────────────────────────────────
            if (PlayOnStart && !string.IsNullOrEmpty(DefaultClipName))
            {
                // Verify the clip exists before playing (helps catch typos)
                bool found = false;
                for (int i = 0; i < count; i++)
                {
                    if (API.AnimatorGetClipName(Entity, i) == DefaultClipName)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    API.AnimatorPlay(Entity, DefaultClipName);
                    API.Log($"[SimpleAnimator] Playing \"{DefaultClipName}\" on entity {Entity}.");
                }
                else
                {
                    API.Log($"[SimpleAnimator] WARNING: Clip \"{DefaultClipName}\" not found on entity {Entity}! " +
                            "Check the clip names printed above — watch for spaces and capitalisation.");
                }
            }
        }

        public void OnUpdate(float dt) { }   // sequencer drives us, nothing to tick

        public void OnDestroy()
        {
            if (API.HasAnimator(Entity))
            {
                API.AnimatorSetStateMachineEnabled(Entity, true);
            }
        }

        // ── Public helpers (callable from Cutscene Event Trigger tracks) ──────

        /// <summary>Play a clip by its exact name.</summary>
        public void PlayClip(string clipName)
        {
            if (!API.HasAnimator(Entity)) return;
            API.AnimatorSetStateMachineEnabled(Entity, false);
            API.AnimatorPlay(Entity, clipName);
            API.Log($"[SimpleAnimator] PlayClip(\"{clipName}\") on entity {Entity}.");
        }

        /// <summary>Stop animation (returns to bind/T-pose).</summary>
        public void StopClip()
        {
            if (!API.HasAnimator(Entity)) return;
            API.AnimatorPlay(Entity, "");
        }
    }
}
