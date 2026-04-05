using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Centralized screen-fade helper.
    ///
    /// Usage:
    ///   - Call SceneFader.FadeToScene(name) instead of API.LoadScene(name) for any
    ///     scene load that needs a fade-out transition.
    ///   - Call SceneFader.StartFadeIn() from a scene's start to fade in from black.
    ///   - Call SceneFader.Update(dt) every frame (done inside Entry.Update).
    ///   - Check SceneFader.IsFadingOut to block input during outgoing transitions.
    /// </summary>
    public static class SceneFader
    {
        private enum State { Idle, FadingIn, FadingOut }

        private static State _state    = State.Idle;
        private static float _timer    = 0f;
        private static float _duration = 0.5f;
        private static string _pendingScene = "";
        private static float _initialMasterVolume = 1.0f;

        // True while fading to black before a scene load — callers should block input.
        public static bool IsFadingOut => _state == State.FadingOut;
        public static bool IsBusy      => _state != State.Idle;

        // ── Public API ─────────────────────────────────────────────────────────

        /// <summary>
        /// Fade the screen to black over <paramref name="duration"/> seconds,
        /// then load <paramref name="sceneName"/>.
        /// Ignored if a fade-out is already in progress.
        /// </summary>
        public static void FadeToScene(string sceneName, float duration = 0.5f)
        {
            if (_state == State.FadingOut) return;
            _pendingScene = sceneName;
            _duration     = duration;
            _timer        = 0f;
            _state        = State.FadingOut;

            // Capture the current master volume as the starting point for the fade-out
            _initialMasterVolume = API.GetGroupVolume("Master");
        }

        /// <summary>
        /// Start a fade-in from black (alpha 1 → 0).
        /// Call this once at the start of any scene that doesn't manage its own fade-in.
        /// Safe to call even if the scene handles fading itself — both are additive toward 0.
        /// </summary>
        public static void StartFadeIn(float duration = 0.6f)
        {
            if (_state == State.FadingOut) return; // don't interrupt an outgoing transition
            _duration = duration;
            _timer    = 0f;
            _state    = State.FadingIn;
            API.SetScreenFadeAlpha(1f);

            // Capture the current master volume (set by SettingsManager.LoadSettings) as the target for the fade-in
            _initialMasterVolume = API.GetGroupVolume("Master");
            // Start silent
            API.SetGroupVolume("Master", 0f);
        }

        /// <summary>
        /// Must be called every frame. Placed at the top of Entry.Update(dt).
        /// </summary>
        public static void Update(float dt)
        {
            if (_state == State.Idle) return;

            _timer += dt;
            float t = Math.Min(_timer / _duration, 1f);

            switch (_state)
            {
                case State.FadingIn:
                    API.SetScreenFadeAlpha(1f - t);
                    
                    // Fade audio in from 0 to its initial volume
                    API.SetGroupVolume("Master", _initialMasterVolume * t);

                    if (_timer >= _duration)
                    {
                        API.SetScreenFadeAlpha(0f);
                        API.SetGroupVolume("Master", _initialMasterVolume);
                        _state = State.Idle;
                    }
                    break;

                case State.FadingOut:
                    API.SetScreenFadeAlpha(t);

                    // Fade audio out from its initial volume to 0
                    API.SetGroupVolume("Master", _initialMasterVolume * (1f - t));

                    if (_timer >= _duration)
                    {
                        API.SetScreenFadeAlpha(1f);
                        API.SetGroupVolume("Master", 0f);

                        // CRITICAL: Stop the looping boss turn sound. 
                        // If a scene transition occurs mid-turn, this sound might otherwise persist 
                        // as a global sound and roll over into the next scene.
                        API.StopSound("BossTurn");

                        string scene = _pendingScene;
                        _state = State.Idle;
                        _timer = 0f;
                        API.LoadScene(scene);
                    }
                    break;
            }
        }
    }
}
