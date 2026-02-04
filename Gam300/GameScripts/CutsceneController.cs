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

        // Fade in state (from black when scene loads)
        private bool _isFadingIn = true;
        private float _fadeInTimer = 0f;

        // Track if we've tried to start the video (handles timing with VideoSystem)
        private bool _videoStartAttempted = false;

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

            // Start faded to black, then fade in (for smooth transition from previous scene)
            API.SetScreenFadeAlpha(1f);
            _isFadingIn = true;
            _fadeInTimer = 0f;
            _videoStartAttempted = false;

            API.Log($"[CutsceneController] Initialized. Next scene: {nextSceneName}");
        }

        public void OnUpdate(float deltaTime)
        {
            if (_entityHandle == 0 || _transitionTriggered) return;

            // Handle fade-in from black (when scene first loads)
            if (_isFadingIn)
            {
                _fadeInTimer += deltaTime;
                float alpha = 1f - Clamp01(_fadeInTimer / fadeDuration);
                API.SetScreenFadeAlpha(alpha);

                if (_fadeInTimer >= fadeDuration)
                {
                    API.SetScreenFadeAlpha(0f);
                    _isFadingIn = false;
                    API.Log("[CutsceneController] Fade-in complete");
                }
                // Continue updating while fading in (don't return)
            }

            // Handle fading out (before scene transition)
            if (_isFading)
            {
                _fadeTimer += deltaTime;
                float alpha = Clamp01(_fadeTimer / fadeDuration);
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

            // Try to start the video if it hasn't been started yet
            // (VideoSystem may not have loaded the video when OnStart ran)
            if (!_videoStartAttempted && !API.IsVideoPlaying(_entityHandle))
            {
                API.PlayVideo(_entityHandle);
                _videoStartAttempted = true;
                API.Log("[CutsceneController] Attempting to start video playback");
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

        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);

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
