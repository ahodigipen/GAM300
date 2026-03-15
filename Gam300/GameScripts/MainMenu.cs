using System;
using Boom;

namespace GameScripts
{
    public class MainMenu
    {
        private const int MOUSE_LEFT = 0;

        private const string NEWGAME_TEX_NORMAL = "Resources/Textures/MenusUI/NewGameButton.png";
        private const string HOWTOPLAY_TEX_NORMAL = "Resources/Textures/MenusUI/HowToPlayButton.png";
        private const string SETTINGS_TEX_NORMAL = "Resources/Textures/MenusUI/Settings.png";
        private const string QUIT_TEX_NORMAL = "Resources/Textures/MenusUI/ExitButton.png";

        private const string NEWGAME_TEX_HOVER = "Resources/Textures/MenusUI/NewGameButton_hovered.png";
        private const string HOWTOPLAY_TEX_HOVER = "Resources/Textures/MenusUI/HowToPlayButton_hovered.png";
        private const string SETTINGS_TEX_HOVER = "Resources/Textures/MenusUI/Settings_hovered.png";
        private const string QUIT_TEX_HOVER = "Resources/Textures/MenusUI/ExitButton_hovered.png";

        private const string NEWGAME_TEX_CLICKED = "Resources/Textures/MenusUI/NewGameButton_Clicked.png";
        private const string HOWTOPLAY_TEX_CLICKED = "Resources/Textures/MenusUI/HowToPlayButton_Clicked.png";
        private const string SETTINGS_TEX_CLICKED = "Resources/Textures/MenusUI/Settings_Clicked.png";
        private const string QUIT_TEX_CLICKED = "Resources/Textures/MenusUI/ExitButton_Clicked.png";

        private ulong _newGameButtonID;
        private ulong _howToPlayButtonID;
        private ulong _settingsButtonID;
        private ulong _quitButtonID;

        private ulong _hoveredButtonID = 0;
        private bool _wasMouseDown = false;

        private ButtonFX _buttonFX;

        private enum MenuState
        {
            Idle,
            Hover,
            ButtonDelay,
            FadingOut
        }

        private MenuState _currentState = MenuState.Idle;
        private ulong _clickedButtonID = 0;

        // Controller navigation
        private int _selectedIndex = -1; // -1: Nothing Selected, 0: New Game, 1: How To Play, 2: Settings, 3: Quit
        private bool _wasDpadUp = false;
        private bool _wasDpadDown = false;
        private bool _wasStickUp = false;
        private bool _wasStickDown = false;
        private bool _wasAButtonPressed = false;

        // Fade transition state
        private float _fadeTimer = 0f;
        private float _fadeDuration = 1.0f;
        private string _sceneToLoad = "";

        public void OnStart(string jsonParams)
        {
            API.Log("MainMenu OnStart Running...");
            _newGameButtonID = API.FindEntity("NewGameButton");
            _howToPlayButtonID = API.FindEntity("HowToPlayButton");
            _settingsButtonID = API.FindEntity("SettingsButton");
            _quitButtonID = API.FindEntity("QuitButton");

            _buttonFX = new ButtonFX(_newGameButtonID, _howToPlayButtonID, _settingsButtonID, _quitButtonID);

            _currentState = MenuState.Idle;
            _clickedButtonID = 0;
            _selectedIndex = -1;

            // Fade in from black when menu loads
            API.SetScreenFadeAlpha(1f);
            StartFadeIn();
            UpdateVisuals();
        }

        public void OnUpdate(float dt)
        {
            // Always update hover effects (indent + sound)
            _buttonFX?.Update(dt);

            switch (_currentState)
            {
                case MenuState.Idle:
                case MenuState.Hover:
                    Update_Idle();
                    Update_ControllerNavigation();
                    UpdateFadeIn(dt);
                    break;

                case MenuState.ButtonDelay:
                    Update_ButtonDelay(dt);
                    break;

                case MenuState.FadingOut:
                    UpdateFadeOut(dt);
                    break;
            }
        }

        private void Update_ControllerNavigation()
        {
            if (!API.IsGamepadConnected()) return;

            bool dpadUp = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_UP);
            bool dpadDown = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_DOWN);
            float stickY = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_Y);
            bool stickUp = stickY < -0.5f;
            bool stickDown = stickY > 0.5f;
            bool aPressed = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A);

            if (_selectedIndex == -1)
            {
                // If any navigation button is pressed, select the first button and return
                if ((dpadUp && !_wasDpadUp) || (dpadDown && !_wasDpadDown) ||
                    (stickUp && !_wasStickUp) || (stickDown && !_wasStickDown))
                {
                    _selectedIndex = 0;
                    _buttonFX?.SetControllerSelection(_selectedIndex);
                    UpdateVisuals();

                    // Update "was" flags to prevent double-input this frame
                    _wasDpadUp = dpadUp;
                    _wasDpadDown = dpadDown;
                    _wasStickUp = stickUp;
                    _wasStickDown = stickDown;
                    _wasAButtonPressed = aPressed;
                    return;
                }

                // Keep updating tracking flags even if we didn't wake up
                _wasDpadUp = dpadUp;
                _wasDpadDown = dpadDown;
                _wasStickUp = stickUp;
                _wasStickDown = stickDown;
                _wasAButtonPressed = aPressed;
                return;
            }

            if ((dpadUp && !_wasDpadUp) || (stickUp && !_wasStickUp))
            {
                _selectedIndex = (_selectedIndex - 1 + 4) % 4;
                _buttonFX?.SetControllerSelection(_selectedIndex);
                UpdateVisuals();
            }
            if ((dpadDown && !_wasDpadDown) || (stickDown && !_wasStickDown))
            {
                _selectedIndex = (_selectedIndex + 1) % 4;
                _buttonFX?.SetControllerSelection(_selectedIndex);
                UpdateVisuals();
            }

            if (aPressed && !_wasAButtonPressed)
            {
                ulong buttonID = 0;
                if (_selectedIndex == 0) buttonID = _newGameButtonID;
                else if (_selectedIndex == 1) buttonID = _howToPlayButtonID;
                else if (_selectedIndex == 2) buttonID = _settingsButtonID;
                else if (_selectedIndex == 3) buttonID = _quitButtonID;

                if (buttonID != 0) StartClickDelay(buttonID);
            }

            _wasDpadUp = dpadUp;
            _wasDpadDown = dpadDown;
            _wasStickUp = stickUp;
            _wasStickDown = stickDown;
            _wasAButtonPressed = aPressed;
        }

        private void UpdateVisuals()
        {
            // Reset all to normal
            API.SetSpriteTexture(_newGameButtonID, NEWGAME_TEX_NORMAL);
            API.SetSpriteTexture(_howToPlayButtonID, HOWTOPLAY_TEX_NORMAL);
            API.SetSpriteTexture(_settingsButtonID, SETTINGS_TEX_NORMAL);
            API.SetSpriteTexture(_quitButtonID, QUIT_TEX_NORMAL);

            if (_selectedIndex == -1) return;

            // Highlight controller-selected button
            if (_selectedIndex == 0) API.SetSpriteTexture(_newGameButtonID, NEWGAME_TEX_CLICKED);
            else if (_selectedIndex == 1) API.SetSpriteTexture(_howToPlayButtonID, HOWTOPLAY_TEX_CLICKED);
            else if (_selectedIndex == 2) API.SetSpriteTexture(_settingsButtonID, SETTINGS_TEX_CLICKED);
            else if (_selectedIndex == 3) API.SetSpriteTexture(_quitButtonID, QUIT_TEX_CLICKED);
        }

        private void Update_Idle()
        {
            bool mouseDown = API.IsMouseDown(MOUSE_LEFT);

            if (!API.GetMousePosInViewport(out Vec2 mousePos))
            {
                if (_hoveredButtonID != 0)
                {
                    _hoveredButtonID = 0;
                    _currentState = MenuState.Idle;
                }
                _wasMouseDown = mouseDown;
                return;
            }

            // Detect which button (if any) is under the cursor this frame
            ulong hoveredNow = 0;
            if (API.Check2DViewportClick(_newGameButtonID, mousePos.X, mousePos.Y))
                hoveredNow = _newGameButtonID;
            else if (API.Check2DViewportClick(_howToPlayButtonID, mousePos.X, mousePos.Y))
                hoveredNow = _howToPlayButtonID;
            else if (API.Check2DViewportClick(_settingsButtonID, mousePos.X, mousePos.Y))
                hoveredNow = _settingsButtonID;
            else if (API.Check2DViewportClick(_quitButtonID, mousePos.X, mousePos.Y))
                hoveredNow = _quitButtonID;

            // Update hover state and swap textures when hover changes
            if (hoveredNow != _hoveredButtonID)
            {
                _hoveredButtonID = hoveredNow;
                _currentState = hoveredNow != 0 ? MenuState.Hover : MenuState.Idle;

                // Reset all to normal, then apply hover texture to the hovered button
                API.SetSpriteTexture(_newGameButtonID, hoveredNow == _newGameButtonID ? NEWGAME_TEX_HOVER : NEWGAME_TEX_NORMAL);
                API.SetSpriteTexture(_howToPlayButtonID, hoveredNow == _howToPlayButtonID ? HOWTOPLAY_TEX_HOVER : HOWTOPLAY_TEX_NORMAL);
                API.SetSpriteTexture(_settingsButtonID, hoveredNow == _settingsButtonID ? SETTINGS_TEX_HOVER : SETTINGS_TEX_NORMAL);
                API.SetSpriteTexture(_quitButtonID, hoveredNow == _quitButtonID ? QUIT_TEX_HOVER : QUIT_TEX_NORMAL);
            }

            // Fire click on fresh press over a button (edge-triggered to avoid repeat firing)
            bool justPressed = mouseDown && !_wasMouseDown;
            if (justPressed && hoveredNow != 0)
            {
                if (hoveredNow == _newGameButtonID) _selectedIndex = 0;
                else if (hoveredNow == _howToPlayButtonID) _selectedIndex = 1;
                else if (hoveredNow == _settingsButtonID) _selectedIndex = 2;
                else if (hoveredNow == _quitButtonID) _selectedIndex = 3;
                StartClickDelay(hoveredNow);
            }

            _wasMouseDown = mouseDown;
        }

        private void Update_ButtonDelay(float dt)
        {
            ExecuteClickAction();
        }

        private void StartClickDelay(ulong buttonID)
        {
            _currentState = MenuState.ButtonDelay;
            _clickedButtonID = buttonID;
            ButtonFX.PlayClickSound();

            // Set the texture
            if (buttonID == _newGameButtonID)
                API.SetSpriteTexture(buttonID, NEWGAME_TEX_CLICKED);
            else if (buttonID == _howToPlayButtonID)
                API.SetSpriteTexture(buttonID, HOWTOPLAY_TEX_CLICKED);
            else if (buttonID == _settingsButtonID)
                API.SetSpriteTexture(buttonID, SETTINGS_TEX_CLICKED);
            else if (buttonID == _quitButtonID)
                API.SetSpriteTexture(buttonID, QUIT_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            if (_clickedButtonID == _newGameButtonID)
            {
                API.Log(">> New Game Button Clicked! Fading to cutscene...");
                PlayerMovement.ResetPersistedHealth();
                PlayerInventory.Reset();
                _currentState = MenuState.FadingOut;
                _fadeTimer = 0f;
                _sceneToLoad = Entry.CUTSCENE_SCENE_NAME;
            }
            else if (_clickedButtonID == _howToPlayButtonID)
            {
                API.Log(">> How To Play Button Clicked! Loading HowToPlay...");
                _currentState = MenuState.Idle;
                API.LoadScene("HowToPlay");
            }
            else if (_clickedButtonID == _settingsButtonID)
            {
                API.Log(">> Settings Button Clicked! Loading Settings...");
                _currentState = MenuState.Idle;
                API.LoadScene("Settings");
            }
            else if (_clickedButtonID == _quitButtonID)
            {
                API.Log(">> Quit Button Clicked! Shutting down...");
                API.ShutdownApplication();
            }
            else
            {
                _currentState = MenuState.Idle;
            }

            _clickedButtonID = 0;
        }

        // Fade in from black (called when menu loads)
        private bool _isFadingIn = false;
        private void StartFadeIn()
        {
            _isFadingIn = true;
            _fadeTimer = 0f;
        }

        private void UpdateFadeIn(float dt)
        {
            if (!_isFadingIn) return;

            _fadeTimer += dt;
            float alpha = 1f - Clamp01(_fadeTimer / _fadeDuration);
            API.SetScreenFadeAlpha(alpha);

            if (_fadeTimer >= _fadeDuration)
            {
                API.SetScreenFadeAlpha(0f);
                _isFadingIn = false;
            }
        }

        // Fade out to black before loading scene
        private void UpdateFadeOut(float dt)
        {
            _fadeTimer += dt;
            float alpha = Clamp01(_fadeTimer / _fadeDuration);
            API.SetScreenFadeAlpha(alpha);

            if (_fadeTimer >= _fadeDuration)
            {
                API.SetScreenFadeAlpha(1f);
                API.Log($"[MainMenu] Loading scene: {_sceneToLoad}");
                API.LoadScene(_sceneToLoad);
            }
        }

        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);
    }
}
