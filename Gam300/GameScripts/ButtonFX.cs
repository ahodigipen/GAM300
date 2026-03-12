using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Reusable button hover/click effects: press-inward scale animation + sound.
    /// On hover, buttons shrink slightly to simulate being pressed into the screen.
    /// Attach to any menu that has sprite-based buttons.
    /// </summary>
    public class ButtonFX
    {
        // Sound paths (relative to Editor working directory)
        private const string HOVER_SOUND_PATH = "Resources/Audio/buttonHover.wav";
        private const string CLICK_SOUND_PATH = "Resources/Audio/buttonPressed.wav";

        // Scale multiplier when pressed inward (e.g. 0.90 = 10% smaller)
        private const float PRESSED_SCALE = 0.90f;
        private const float PRESS_SPEED   = 12.0f;  // lerp speed

        // Per-button state
        private readonly ulong[] _buttonIDs;
        private readonly Vec3[]  _baseScale;       // original scale per button
        private readonly float[] _currentT;        // current lerp t toward pressed (0=normal, 1=pressed)
        private readonly bool[]  _wasHovered;      // hover state last frame

        public ButtonFX(params ulong[] buttonIDs)
        {
            _buttonIDs  = buttonIDs;
            _baseScale  = new Vec3[buttonIDs.Length];
            _currentT   = new float[buttonIDs.Length];
            _wasHovered = new bool[buttonIDs.Length];

            // Cache original scales
            for (int i = 0; i < buttonIDs.Length; i++)
            {
                if (buttonIDs[i] != 0)
                    _baseScale[i] = API.GetScale(buttonIDs[i]);
                _currentT[i]   = 0f;
                _wasHovered[i] = false;
            }

            // Preload sounds so first play is instant
            API.PreloadSound("ui_hover", HOVER_SOUND_PATH);
            API.PreloadSound("ui_click", CLICK_SOUND_PATH);
        }

        /// <summary>
        /// Call every frame from OnUpdate. Handles hover detection, press-inward animation, and hover sound.
        /// </summary>
        public void Update(float dt)
        {
            bool hasMousePos = API.GetMousePosInViewport(out Vec2 mousePos);

            for (int i = 0; i < _buttonIDs.Length; i++)
            {
                if (_buttonIDs[i] == 0) continue;

                // Determine if hovered
                bool hovered = false;
                if (hasMousePos)
                    hovered = API.Check2DViewportClick(_buttonIDs[i], mousePos.X, mousePos.Y);

                // Play hover sound on enter
                if (hovered && !_wasHovered[i])
                    API.PlaySound("ui_hover", HOVER_SOUND_PATH);
                _wasHovered[i] = hovered;

                // Animate lerp t toward 1 (pressed) or back to 0 (normal)
                float targetT = hovered ? 1f : 0f;
                _currentT[i] = Lerp(_currentT[i], targetT, dt * PRESS_SPEED);

                // Snap if close enough to avoid jitter
                if (Math.Abs(_currentT[i] - targetT) < 0.001f)
                    _currentT[i] = targetT;

                // Apply scale: lerp from base scale to base * PRESSED_SCALE
                float s = 1f - (1f - PRESSED_SCALE) * _currentT[i];
                API.SetScale(_buttonIDs[i], new Vec3(
                    _baseScale[i].X * s,
                    _baseScale[i].Y * s,
                    _baseScale[i].Z * s
                ));
            }
        }

        /// <summary>
        /// Call when a controller changes the selected index. Sets hover state for the selected button.
        /// </summary>
        public void SetControllerSelection(int selectedIndex)
        {
            for (int i = 0; i < _buttonIDs.Length; i++)
            {
                bool selected = (i == selectedIndex);
                if (selected && !_wasHovered[i])
                    API.PlaySound("ui_hover", HOVER_SOUND_PATH);
                _wasHovered[i] = selected;
            }
        }

        /// <summary>
        /// Call when a button is clicked/pressed. Plays the click sound.
        /// </summary>
        public static void PlayClickSound()
        {
            API.PlaySound("ui_click", CLICK_SOUND_PATH);
        }

        /// <summary>
        /// Resets all buttons back to their original scale. Call in OnStart or ResetButtonState.
        /// </summary>
        public void Reset()
        {
            for (int i = 0; i < _buttonIDs.Length; i++)
            {
                _currentT[i]   = 0f;
                _wasHovered[i] = false;
                if (_buttonIDs[i] != 0)
                    API.SetScale(_buttonIDs[i], _baseScale[i]);
            }
        }

        private static float Lerp(float a, float b, float t)
        {
            if (t > 1f) t = 1f;
            if (t < 0f) t = 0f;
            return a + (b - a) * t;
        }
    }
}
