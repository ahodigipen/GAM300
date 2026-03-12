using Boom;

namespace GameScripts
{
    /// <summary>
    /// Attached to the "Outro Video" entity in OUTRO SCENE.
    /// Waits for the video to actually start playing before monitoring for end.
    /// Transitions to the Credits scene when the video finishes, or on Space skip.
    /// </summary>
    public class OutroScene
    {
        public ulong Entity;

        [EditorExposed(displayName: "Credits Scene Name", tooltip: "Scene to load after the outro video ends.")]
        public string creditsSceneName = "Credits";

        private bool _videoStarted = false;
        private bool _transitionStarted = false;
        private bool _spaceWasDown = false;

        public void OnStart(string jsonParams)
        {
            _videoStarted = false;
            _transitionStarted = false;
            _spaceWasDown = false;
            API.SetCutsceneMode(true);
        }

        public void OnUpdate(float dt)
        {
            if (_transitionStarted) return;

            // Wait until the video has actually started playing.
            // HasVideoEnded() returns true while the player is unloaded,
            // so we must confirm playback has begun before we trust it.
            if (!_videoStarted)
            {
                if (API.IsVideoPlaying(Entity))
                    _videoStarted = true;
                else
                    return;
            }

            // Edge-detect Space so a held key doesn't re-trigger.
            bool spaceDown = API.IsKeyDown(API.KEY_SPACE);
            bool spacePressed = spaceDown && !_spaceWasDown;
            _spaceWasDown = spaceDown;

            if (API.HasVideoEnded(Entity) || spacePressed)
            {
                _transitionStarted = true;
                SceneFader.FadeToScene(creditsSceneName);
            }
        }

        public void OnDestroy()
        {
            API.SetCutsceneMode(false);
        }
    }
}
