using System;
using System.Collections.Generic;
using Boom;

namespace GameScripts
{
    // Attach to a trigger volume that transitions the player to another scene.
    // Set the scene name in the inspector.
    // When the player enters the trigger, load the specified scene.
    public class SceneTransitionTrigger
    {
        public ulong Entity;

        // Config
        [Boom.EditorExposed("Scene Name", "Name of the scene to load (e.g., 'Level2', 'MainMenu')")]
        private string _sceneName = "";

        [Boom.EditorExposed("Transition Delay", "Delay before transitioning in seconds", 0f, 5f, true)]
        private float _transitionDelay = 0f;

        [Boom.EditorExposed("One Time Use", "If true, trigger only works once")]
        private bool _oneTimeUse = false;

        [Boom.EditorExposed("Play Boss Transition Dialogue", "If true, plays boss transition dialogue before loading scene")]
        private bool _playBossTransitionDialogue = false;

        // State
        private static readonly Dictionary<ulong, SceneTransitionTrigger> s_instances = new Dictionary<ulong, SceneTransitionTrigger>();
        private bool _hasTriggered = false;
        private bool _isTransitioning = false;
        private float _transitionTimer = 0f;

        public void OnStart(string jsonParams)
        {
            s_instances[Entity] = this;

            if (string.IsNullOrWhiteSpace(_sceneName))
            {
                API.Log("[SceneTransitionTrigger] WARNING: Scene name is empty! Please set a scene name.");
            }

            // Ensure trigger is configured
            if (!API.HasCollider(Entity))
            {
                API.Log("[SceneTransitionTrigger] WARNING: Entity has no collider. Trigger will not work.");
                return;
            }

            // REMOVED: API.SetTrigger(true) call here to avoid PhysX state conflicts.
            // Ensure "Is Trigger" is checked in the Editor for this entity.

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log($"[SceneTransitionTrigger] Registered trigger callbacks. Will transition to: '{_sceneName}'");
        }

        public void OnUpdate(float dt)
        {
            // Handle transition logic in Update instead of Callback
            if (_isTransitioning)
            {
                _transitionTimer -= dt;
                if (_transitionTimer <= 0f)
                {
                    _isTransitioning = false;
                    DoTransition();
                }
            }
        }

        public void OnDestroy()
        {
            if (s_instances.ContainsKey(Entity)) s_instances.Remove(Entity);
            API.UnregisterTriggerCallbacks(Entity);
        }

        private static void OnTriggerEnter(ulong triggerEntity, ulong otherEntity)
        {
            SceneTransitionTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            // Only react when the player enters this trigger
            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            // Check if already triggered and one-time use
            if (inst._hasTriggered && inst._oneTimeUse)
            {
                return;
            }

            // Check if scene name is valid
            if (string.IsNullOrWhiteSpace(inst._sceneName))
            {
                API.Log("[SceneTransitionTrigger] ERROR: Scene name is empty!");
                return;
            }

            inst._hasTriggered = true;

            // ALWAYS defer the transition to OnUpdate to avoid loading scenes during a Physics Callback.
            // This prevents the "PxScene::simulate: Simulation is still processing" crash.
            if (inst._playBossTransitionDialogue)
            {
                API.Log($"[SceneTransitionTrigger] Starting sequence for '{inst._sceneName}'.");
                StoryDialogueManager.PlayBossTransitionSequence(() =>
                {
                    inst._isTransitioning = true;
                    inst._transitionTimer = Math.Max(0.01f, inst._transitionDelay);
                });
            }
            else
            {
                inst._isTransitioning = true;
                // Even with 0 delay, we use a tiny 0.01s delay to push it to the next frame.
                inst._transitionTimer = Math.Max(0.01f, inst._transitionDelay);
                API.Log($"[SceneTransitionTrigger] Deferring transition to '{inst._sceneName}' to next frame.");
            }
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
        }

        private void DoTransition()
        {
            API.Log($"[SceneTransitionTrigger] Executing deferred load for: '{_sceneName}'");
            API.LoadScene(_sceneName);
        }
    }
}
