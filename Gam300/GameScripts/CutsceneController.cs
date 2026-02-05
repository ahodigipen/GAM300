using Boom;

namespace GameScripts
{
    /// <summary>
    /// Controls cutscene playback and transitions to the next scene when video ends.
    /// Attach this script to an entity with a VideoComponent.
    /// </summary>
    public class CutsceneController
    {
        // Entity handle - automatically populated by the engine
        public ulong Entity;

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

        // Video duration cache
        private double _videoDuration = 0.0;
        private bool _durationCached = false;

        public void OnStart(string paramsJson)
        {
            if (Entity == 0)
            {
                API.Log("[CutsceneController] Warning: Entity handle not set by engine");
                return;
            }

            // Check if entity has VideoComponent
            if (!API.HasVideoComponent(Entity))
            {
                API.Log("[CutsceneController] Warning: Entity does not have VideoComponent");
                return;
            }

            // Start faded to black, then fade in (for smooth transition from previous scene)
            API.SetScreenFadeAlpha(1f);
            _isFadingIn = true;
            _fadeInTimer = 0f;
            _videoStartAttempted = false;
            _durationCached = false;

            API.Log($"[CutsceneController] Initialized. Next scene: {nextSceneName}");
        }

        public void OnUpdate(float deltaTime)
        {
            if (Entity == 0 || _transitionTriggered) return;

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
            if (!_videoStartAttempted && !API.IsVideoPlaying(Entity))
            {
                API.PlayVideo(Entity);
                _videoStartAttempted = true;
                API.Log("[CutsceneController] Attempting to start video playback");
            }

            // Cache video duration once available
            if (!_durationCached)
            {
                _videoDuration = API.GetVideoDuration(Entity);
                if (_videoDuration > 0.0)
                {
                    _durationCached = true;
                    API.Log($"[CutsceneController] Video duration: {_videoDuration:F2} seconds");
                }
            }

            // Check if video has started playing (via API or by checking current time > 0)
            double currentTime = API.GetVideoCurrentTime(Entity);
            if (!_videoStarted)
            {
                if (API.IsVideoPlaying(Entity) || currentTime > 0.1)
                {
                    _videoStarted = true;
                    API.Log("[CutsceneController] Video started playing");
                }
            }

            // Check for skip input
            bool skipKeyPressed = allowSkip && (API.IsKeyDown(skipKey) || API.IsKeyDown(256)); // Support Escape too
            bool skipControllerPressed = allowSkip && API.IsGamepadConnected() && 
                                        (API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A) || 
                                         API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_START));

            if (skipKeyPressed || skipControllerPressed)
            {
                API.Log("[CutsceneController] Cutscene skipped");
                StartTransition();
                return;
            }

            // Check if video has ended using multiple methods:
            // 1. API.HasVideoEnded() - primary method
            // 2. Time-based check as fallback (currentTime >= duration - 0.1)
            bool videoEnded = API.HasVideoEnded(Entity);
            bool timeBasedEnd = _durationCached && _videoDuration > 0 && currentTime >= (_videoDuration - 0.1);

            if (_videoStarted && (videoEnded || timeBasedEnd))
            {
                API.Log($"[CutsceneController] Video ended (API: {videoEnded}, Time: {currentTime:F2}/{_videoDuration:F2})");
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
            API.StopVideo(Entity);
        }

        public void OnDestroy()
        {
            // Reset screen fade when destroyed
            API.SetScreenFadeAlpha(0f);
        }
    }
}
