using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Reusable button hover/click effects: indent animation + sound.
    /// Attach to any menu that has sprite-based buttons.
    /// </summary>
    public class ButtonFX
    {
        // Sound paths (relative to Editor working directory)
        private const string HOVER_SOUND_PATH = "Resources/Audio/buttonHover.wav";
        private const string CLICK_SOUND_PATH = "Resources/Audio/buttonPressed.wav";

        // Indent amount (world units — positive X = move right)
        private const float INDENT_AMOUNT = 0.15f;
        private const float INDENT_SPEED  = 10.0f;  // lerp speed

        // Per-button state
        private readonly ulong[] _buttonIDs;
        private readonly float[] _baseX;       // original X position per button
        private readonly float[] _currentIndent; // current indent offset
        private readonly bool[]  _wasHovered;  // hover state last frame

        public ButtonFX(params ulong[] buttonIDs)
        {
            _buttonIDs    = buttonIDs;
            _baseX        = new float[buttonIDs.Length];
            _currentIndent = new float[buttonIDs.Length];
            _wasHovered   = new bool[buttonIDs.Length];

            // Cache original X positions
            for (int i = 0; i < buttonIDs.Length; i++)
            {
                if (buttonIDs[i] != 0)
                {
                    Vec3 pos = API.GetPosition(buttonIDs[i]);
                    _baseX[i] = pos.X;
                }
                _currentIndent[i] = 0f;
                _wasHovered[i] = false;
            }

            // Preload sounds so first play is instant
            API.PreloadSound("ui_hover", HOVER_SOUND_PATH);
            API.PreloadSound("ui_click", CLICK_SOUND_PATH);
        }

        /// <summary>
        /// Call every frame from OnUpdate. Handles hover detection, indent animation, and hover sound.
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
                {
                    hovered = API.Check2DViewportClick(_buttonIDs[i], mousePos.X, mousePos.Y);
                }

                // Play hover sound on enter
                if (hovered && !_wasHovered[i])
                {
                    API.PlaySound("ui_hover", HOVER_SOUND_PATH);
                }
                _wasHovered[i] = hovered;

                // Animate indent
                float target = hovered ? INDENT_AMOUNT : 0f;
                _currentIndent[i] = Lerp(_currentIndent[i], target, dt * INDENT_SPEED);

                // Snap if close enough to avoid jitter
                if (Math.Abs(_currentIndent[i] - target) < 0.001f)
                    _currentIndent[i] = target;

                // Apply position
                Vec3 pos = API.GetPosition(_buttonIDs[i]);
                pos.X = _baseX[i] + _currentIndent[i];
                API.SetPosition(_buttonIDs[i], pos);
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
                {
                    API.PlaySound("ui_hover", HOVER_SOUND_PATH);
                }
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
        /// Resets all buttons back to their original X position. Call in OnStart or ResetButtonState.
        /// </summary>
        public void Reset()
        {
            for (int i = 0; i < _buttonIDs.Length; i++)
            {
                _currentIndent[i] = 0f;
                _wasHovered[i] = false;
                if (_buttonIDs[i] != 0)
                {
                    Vec3 pos = API.GetPosition(_buttonIDs[i]);
                    pos.X = _baseX[i];
                    API.SetPosition(_buttonIDs[i], pos);
                }
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
