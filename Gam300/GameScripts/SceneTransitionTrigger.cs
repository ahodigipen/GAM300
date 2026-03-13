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

        [Boom.EditorExposed("Interaction Prompt Name", "Name of the UI entity for interaction (e.g. 'E to interact')")]
        private string _promptName = "UI_E_OpenDoor";
        [Boom.EditorExposed("Play Boss Transition Dialogue", "If true, plays boss transition dialogue before loading scene")]
        private bool _playBossTransitionDialogue = false;

        // State
        private static readonly Dictionary<ulong, SceneTransitionTrigger> s_instances = new Dictionary<ulong, SceneTransitionTrigger>();
        private bool  _hasTriggered    = false;
        private bool  _isTransitioning = false;   // delay countdown
        private float _transitionTimer = 0f;
        private bool  _isFadingOut     = false;   // fade-to-black before load
        private float _fadeTimer       = 0f;
        private const float FADE_DURATION = 0.25f;

        private bool _playerInRange = false;
        private bool _interactWasDown = false;
        private ulong _promptEntity = 0;

        private const int KEY_E = 69; // Changed back to E for interaction

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

            _promptEntity = API.FindEntity(_promptName);
            if (_promptEntity != 0 && API.HasSprite(_promptEntity))
            {
                API.SetSpriteAlpha(_promptEntity, 0f);
            }

            API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            API.RegisterTriggerExitCallback(Entity, OnTriggerExit);
            API.Log($"[SceneTransitionTrigger] Registered trigger callbacks. Will transition to: '{_sceneName}'");
        }

        public void OnUpdate(float dt)
        {
            // Handle fade and transition
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
                return;
            }

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

            if (_hasTriggered && _oneTimeUse) return;

            // Handle manual interaction
            bool interactDown = API.IsKeyDown(KEY_E) || (API.IsGamepadConnected() && API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A));
            bool interactPressed = interactDown && !_interactWasDown;
            _interactWasDown = interactDown;

            if (_playerInRange && interactPressed)
            {
                TriggerTransition();
            }
        }

        private void TriggerTransition()
        {
            // Check if scene name is valid
            if (string.IsNullOrWhiteSpace(_sceneName))
            {
                API.Log("[SceneTransitionTrigger] ERROR: Scene name is empty!");
                return;
            }

            _hasTriggered = true;
            if (_promptEntity != 0) API.SetSpriteAlpha(_promptEntity, 0f);

            // Start transition (with delay if configured)
            if (_playBossTransitionDialogue)
            {
                API.Log($"[SceneTransitionTrigger] Playing Boss Transition Dialogue before transitioning to '{_sceneName}'.");
                StoryDialogueManager.PlayBossTransitionSequence(() =>
                {
                    if (_transitionDelay > 0f)
                    {
                        _isTransitioning = true;
                        _transitionTimer = _transitionDelay;
                        API.Log($"[SceneTransitionTrigger] Transition to '{_sceneName}' in {_transitionDelay:F1}s then fade.");
                    }
                    else
                    {
                        StartFadeOut();
                    }
                });
            }
            else if (_transitionDelay > 0f)
            {
                _isTransitioning = true;
                _transitionTimer = _transitionDelay;
                API.Log($"[SceneTransitionTrigger] Transition to '{_sceneName}' in {_transitionDelay:F1}s then fade.");
            }
            else
            {
                StartFadeOut();
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

            inst._playerInRange = true;
            if (inst._promptEntity != 0 && (!inst._hasTriggered || !inst._oneTimeUse))
            {
                API.SetSpriteAlpha(inst._promptEntity, 1f);
            }
            
            API.Log("[SceneTransitionTrigger] Player in range. Press E or Gamepad A to transition.");
        }

        private static void OnTriggerExit(ulong triggerEntity, ulong otherEntity)
        {
            SceneTransitionTrigger inst;
            if (!s_instances.TryGetValue(triggerEntity, out inst)) return;

            if (otherEntity != PlayerMovement.GetPlayerEntity()) return;

            inst._playerInRange = false;
            if (inst._promptEntity != 0) API.SetSpriteAlpha(inst._promptEntity, 0f);
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
