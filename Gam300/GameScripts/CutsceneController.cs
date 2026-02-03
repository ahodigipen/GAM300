using Boom;

namespace GameScripts
{
    /// <summary>
    /// Controls cutscene playback and transitions to the next scene when video ends.
    /// Attach this script to an entity with a VideoComponent.
    /// </summary>
    public class CutsceneController
    {
        private ulong _entityHandle;
        private bool _videoStarted = false;
        private bool _transitionTriggered = false;

        // Configure these in the editor or via script params
        [EditorExposed("Next Scene", "Scene to load when cutscene ends")]
        public string nextSceneName = Entry.GAMEPLAY_SCENE_NAME;

        [EditorExposed("Skip Key", "GLFW key code to skip cutscene (default: Space=32, Escape=256)")]
        public int skipKey = 32; // Space bar

        [EditorExposed("Allow Skip", "Whether the player can skip the cutscene")]
        public bool allowSkip = true;

        [EditorExposed("Fade Duration", "Duration of fade out before scene transition")]
        public float fadeDuration = 1.0f;

        private float _fadeTimer = 0f;
        private bool _isFading = false;

        public void OnStart(string entityGuid)
        {
            _entityHandle = API.FindEntity(entityGuid);
            if (_entityHandle == 0)
            {
                API.Log("[CutsceneController] Warning: Could not find entity");
                return;
            }

            // Check if entity has VideoComponent
            if (!API.HasVideoComponent(_entityHandle))
            {
                API.Log("[CutsceneController] Warning: Entity does not have VideoComponent");
                return;
            }

            API.Log($"[CutsceneController] Initialized. Next scene: {nextSceneName}");
        }

        public void OnUpdate(float deltaTime)
        {
            if (_entityHandle == 0 || _transitionTriggered) return;

            // Handle fading
            if (_isFading)
            {
                _fadeTimer += deltaTime;
                float alpha = _fadeTimer / fadeDuration;
                API.SetScreenFadeAlpha(alpha);

                if (_fadeTimer >= fadeDuration)
                {
                    // Transition to next scene
                    _transitionTriggered = true;
                    API.Log($"[CutsceneController] Loading scene: {nextSceneName}");
                    API.LoadScene(nextSceneName);
                }
                return;
            }

            // Check if video has started playing
            if (!_videoStarted && API.IsVideoPlaying(_entityHandle))
            {
                _videoStarted = true;
                API.Log("[CutsceneController] Video started playing");
            }

            // Check for skip input
            if (allowSkip && API.IsKeyDown(skipKey))
            {
                API.Log("[CutsceneController] Cutscene skipped");
                StartTransition();
                return;
            }

            // Check if video has ended
            if (_videoStarted && API.HasVideoEnded(_entityHandle))
            {
                API.Log("[CutsceneController] Video ended");
                StartTransition();
            }
        }

        private void StartTransition()
        {
            if (_isFading) return;

            _isFading = true;
            _fadeTimer = 0f;

            // Stop the video
            API.StopVideo(_entityHandle);
        }

        public void OnDestroy()
        {
            // Reset screen fade when destroyed
            API.SetScreenFadeAlpha(0f);
        }
    }
}
