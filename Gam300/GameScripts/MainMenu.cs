using System;
using Boom;

namespace GameScripts
{
    public class MainMenu
    {
        private const int MOUSE_LEFT = 0;

        private const string NEWGAME_TEX_NORMAL = "Resources/Textures/MenusUI/NewGameButton.png";
        private const string HOWTOPLAY_TEX_NORMAL = "Resources/Textures/MenusUI/HowToPlayButton.png";
        private const string QUIT_TEX_NORMAL = "Resources/Textures/MenusUI/ExitButton.png";

        private const string NEWGAME_TEX_CLICKED = "Resources/Textures/MenusUI/NewGameButton_Clicked.png";
        private const string HOWTOPLAY_TEX_CLICKED = "Resources/Textures/MenusUI/HowToPlayButton_Clicked.png";
        private const string QUIT_TEX_CLICKED = "Resources/Textures/MenusUI/ExitButton_Clicked.png";

        private ulong _newGameButtonID;
        private ulong _howToPlayButtonID;
        private ulong _quitButtonID;

        private ButtonFX _buttonFX;

        private enum MenuState
        {
            Idle,
            ButtonDelay,
            FadingOut
        }

        private MenuState _currentState = MenuState.Idle;
        private ulong _clickedButtonID = 0;

        // Controller navigation
        private int _selectedIndex = -1; // -1: Nothing Selected, 0: New Game, 1: How To Play, 2: Quit
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
            _quitButtonID = API.FindEntity("QuitButton");

            _buttonFX = new ButtonFX(_newGameButtonID, _howToPlayButtonID, _quitButtonID);

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
                _selectedIndex = (_selectedIndex - 1 + 3) % 3;
                _buttonFX?.SetControllerSelection(_selectedIndex);
                UpdateVisuals();
            }
            if ((dpadDown && !_wasDpadDown) || (stickDown && !_wasStickDown))
            {
                _selectedIndex = (_selectedIndex + 1) % 3;
                _buttonFX?.SetControllerSelection(_selectedIndex);
                UpdateVisuals();
            }

            if (aPressed && !_wasAButtonPressed)
            {
                ulong buttonID = 0;
                if (_selectedIndex == 0) buttonID = _newGameButtonID;
                else if (_selectedIndex == 1) buttonID = _howToPlayButtonID;
                else if (_selectedIndex == 2) buttonID = _quitButtonID;

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
            API.SetSpriteTexture(_quitButtonID, QUIT_TEX_NORMAL);

            if (_selectedIndex == -1)
            {
                return; // "continue" isn't valid here, so we use return to stop.
            }

            // Highlight selected (using clicked texture as highlight for now)
            if (_selectedIndex == 0) API.SetSpriteTexture(_newGameButtonID, NEWGAME_TEX_CLICKED);
            else if (_selectedIndex == 1) API.SetSpriteTexture(_howToPlayButtonID, HOWTOPLAY_TEX_CLICKED);
            else if (_selectedIndex == 2) API.SetSpriteTexture(_quitButtonID, QUIT_TEX_CLICKED);
        }

        private void Update_Idle()
        {
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                if (!API.GetMousePosInViewport(out Vec2 mousePos))
                {
                    return;
                }

                if (API.Check2DViewportClick(_newGameButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 0;
                    StartClickDelay(_newGameButtonID);
                }
                else if (API.Check2DViewportClick(_howToPlayButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 1;
                    StartClickDelay(_howToPlayButtonID);
                }
                else if (API.Check2DViewportClick(_quitButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 2;
                    StartClickDelay(_quitButtonID);
                }
            }
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
            else if (buttonID == _quitButtonID)
                API.SetSpriteTexture(buttonID, QUIT_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            if (_clickedButtonID == _newGameButtonID)
            {
                API.Log(">> New Game Button Clicked! Fading to cutscene...");
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
