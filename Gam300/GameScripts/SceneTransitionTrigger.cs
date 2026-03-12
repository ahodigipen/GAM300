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

        // State
        private static readonly Dictionary<ulong, SceneTransitionTrigger> s_instances = new Dictionary<ulong, SceneTransitionTrigger>();
        private bool  _hasTriggered    = false;
        private bool  _isTransitioning = false;   // delay countdown
        private float _transitionTimer = 0f;
        private bool  _isFadingOut     = false;   // fade-to-black before load
        private float _fadeTimer       = 0f;
        private const float FADE_DURATION = 0.25f;

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
            // Step 1 — optional delay before starting the fade
            if (_isTransitioning)
            {
                _transitionTimer -= dt;
                if (_transitionTimer <= 0f)
                {
                    _isTransitioning = false;
                    StartFadeOut();
                }
                return;
            }

            // Step 2 — fade to black, then load
            if (_isFadingOut)
            {
                _fadeTimer += dt;
                float alpha = Math.Min(_fadeTimer / FADE_DURATION, 1f);
                API.SetScreenFadeAlpha(alpha);
                if (_fadeTimer >= FADE_DURATION)
                {
                    API.SetScreenFadeAlpha(1f);
                    _isFadingOut = false;
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
                API.Log("[SceneTransitionTrigger] Trigger already used (one-time use).");
                return;
            }

            // Check if scene name is valid
            if (string.IsNullOrWhiteSpace(inst._sceneName))
            {
                API.Log("[SceneTransitionTrigger] ERROR: Scene name is empty!");
                return;
            }

            inst._hasTriggered = true;

            // Start transition (with delay if configured)
            if (inst._transitionDelay > 0f)
            {
                inst._isTransitioning = true;
                inst._transitionTimer = inst._transitionDelay;
                API.Log($"[SceneTransitionTrigger] Transition to '{inst._sceneName}' in {inst._transitionDelay:F1}s then fade.");
            }
            else
            {
                inst.StartFadeOut();
            }
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            // Not needed for scene transition
        }

        private void StartFadeOut()
        {
            _isFadingOut = true;
            _fadeTimer   = 0f;
            API.Log($"[SceneTransitionTrigger] Fading out before loading: '{_sceneName}'");
        }

        private void DoTransition()
        {
            API.Log($"[SceneTransitionTrigger] Loading scene: '{_sceneName}'");
            API.LoadScene(_sceneName);
        }
    }
}
