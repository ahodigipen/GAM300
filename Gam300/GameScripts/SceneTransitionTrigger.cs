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
        private bool  _hasTriggered    = false;
        private bool  _isTransitioning = false;   // delay countdown
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

            if (!API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
                API.Log("[SceneTransitionTrigger] Collider set to IsTrigger = true.");
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log($"[SceneTransitionTrigger] Registered trigger callbacks. Will transition to: '{_sceneName}'");
        }

        public void OnUpdate(float dt)
        {
            if (_isTransitioning)
            {
                _transitionTimer -= dt;
                if (_transitionTimer <= 0f)
                {
                    _isTransitioning = false;
                    ExecuteTransition();
                }
            }
        }

        private void TriggerTransition()
        {
            if (_hasTriggered) return;

            // Check if scene name is valid
            if (string.IsNullOrWhiteSpace(_sceneName))
            {
                API.Log("[SceneTransitionTrigger] ERROR: Scene name is empty!");
                return;
            }

            _hasTriggered = true;

            // Start transition (with dialogue if configured)
            if (_playBossTransitionDialogue)
            {
                API.Log($"[SceneTransitionTrigger] Playing Boss Transition Dialogue before transitioning to '{_sceneName}'.");
                StoryDialogueManager.PlayBossTransitionSequence(() =>
                {
                    StartTransitionCountdown();
                });
            }
            else
            {
                StartTransitionCountdown();
            }
        }

        private void StartTransitionCountdown()
        {
            if (_transitionDelay > 0f)
            {
                _isTransitioning = true;
                _transitionTimer = _transitionDelay;
                API.Log($"[SceneTransitionTrigger] Transition to '{_sceneName}' in {_transitionDelay:F1}s.");
            }
            else
            {
                ExecuteTransition();
            }
        }

        private void ExecuteTransition()
        {
            API.Log($"[SceneTransitionTrigger] Initiating fade to scene: '{_sceneName}'");
            SceneFader.FadeToScene(_sceneName, 0.25f);
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

            if (inst._hasTriggered) return;

            API.Log("[SceneTransitionTrigger] Player entered trigger. Starting automatic transition.");
            inst.TriggerTransition();
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
        }
    }
}
