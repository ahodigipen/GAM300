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
        private const string HOVER_SOUND_PATH = "Resources/Audio/buttonHovered.wav";
        private const string CLICK_SOUND_PATH = "Resources/Audio/buttonPress.wav";

        private readonly ulong[] _buttonIDs;
        private readonly bool[]  _wasHovered;

        public ButtonFX(params ulong[] buttonIDs)
        {
            _buttonIDs  = buttonIDs;
            _wasHovered = new bool[buttonIDs.Length];

            API.PreloadSound("ui_hover", HOVER_SOUND_PATH);
            API.PreloadSound("ui_click", CLICK_SOUND_PATH);
            API.SetSoundVolume("ui_hover", 0.5f);
        }

        /// <summary>
        /// Call every frame from OnUpdate. Handles hover detection and hover sound.
        /// </summary>
        public void Update(float dt)
        {
            bool hasMousePos = API.GetMousePosInViewport(out Vec2 mousePos);

            for (int i = 0; i < _buttonIDs.Length; i++)
            {
                if (_buttonIDs[i] == 0 || !hasMousePos) continue;

                bool hovered = API.Check2DViewportClick(_buttonIDs[i], mousePos.X, mousePos.Y);
                if (hovered && !_wasHovered[i])
                    API.PlaySound("ui_hover", HOVER_SOUND_PATH);
                _wasHovered[i] = hovered;
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

        public void Reset()
        {
            for (int i = 0; i < _buttonIDs.Length; i++)
                _wasHovered[i] = false;
        }
    }
}
