using System;
using Boom;

namespace GameScripts
{
    public class SettingsMenu
    {
        private const int MOUSE_LEFT = 0;

        // --- Texture Constants ---
        private const string MAINMENU_TEX_NORMAL  = "Resources/Textures/MenusUI/ReturnMenuButton.png";
        private const string MAINMENU_TEX_CLICKED = "Resources/Textures/MenusUI/ReturnMenuButton_Clicked.png";
        private const string GAMMA_BTN_TEX_NORMAL  = "Resources/Textures/MenusUI/GammaButton.png";
        private const string GAMMA_BTN_TEX_CLICKED = "Resources/Textures/MenusUI/GammaButton_Clicked.png";

        private ulong _mainMenuButtonID;
        private ulong _gammaButtonID;

        private ButtonFX _buttonFX;

        private VolumeSlider _masterSlider;
        private VolumeSlider _bgmSlider;
        private VolumeSlider _sfxSlider;

        private bool _fadingToGamma = false;

        private enum SettingsState
        {
            Idle,
            ButtonDelay,
            WaitingForMouseUp,
            FadingOut
        }

        private SettingsState _currentState = SettingsState.Idle;
        private ulong _clickedButtonID = 0;

        // Controller navigation
        // 0: Return to Main Menu, 1: Master, 2: BGM, 3: SFX, 4: Gamma
        private int _selectedIndex = 0;
        private bool _wasDpadUp = false;
        private bool _wasDpadDown = false;
        private bool _wasStickUp = false;
        private bool _wasStickDown = false;
        private bool _wasAButtonPressed = false;

        private float _buttonDelayTimer = 0.0f;
        private const float CLICK_DELAY_DURATION = 0.1f;

        // Fade
        private float _fadeTimer = 0f;
        private const float FADE_DURATION = 1.0f;
        private bool _isFadingIn = false;

        // Visual constants for slider highlighting
        private const float SLIDER_HIGHLIGHT_ALPHA = 1.0f;
        private const float SLIDER_NORMAL_ALPHA    = 0.6f;

        public void OnStart(string jsonParams)
        {
            API.Log("SettingsMenu OnStart Running...");

            SettingsManager.LoadSettings();

            _mainMenuButtonID = API.FindEntity("Settings_ReturnButton");
            _gammaButtonID    = API.FindEntity("Settings_GammaButton");

            _buttonFX = new ButtonFX(_mainMenuButtonID);

            _masterSlider = new VolumeSlider("Settings_Master_BG", "Settings_Master_Fill", "Settings_Master_Handle", "Master");
            _bgmSlider    = new VolumeSlider("Settings_BGM_BG",    "Settings_BGM_Fill",    "Settings_BGM_Handle",    "Music");
            _sfxSlider    = new VolumeSlider("Settings_SFX_BG",    "Settings_SFX_Fill",    "Settings_SFX_Handle",    "SFX");

            _selectedIndex = 0;
            _currentState  = SettingsState.WaitingForMouseUp;

            // Fade in from black
            API.SetScreenFadeAlpha(1f);
            _isFadingIn = true;
            _fadeTimer  = 0f;

            UpdateVisuals();
            API.SetSpriteTexture(_mainMenuButtonID, MAINMENU_TEX_NORMAL);
        }

        public void OnUpdate(float dt)
        {
            _buttonFX?.Update(dt);

            if (_isFadingIn)
            {
                _fadeTimer += dt;
                float alpha = 1f - Clamp01(_fadeTimer / FADE_DURATION);
                API.SetScreenFadeAlpha(alpha);
                if (_fadeTimer >= FADE_DURATION)
                {
                    API.SetScreenFadeAlpha(0f);
                    _isFadingIn = false;
                }
            }

            if (_currentState == SettingsState.FadingOut)
            {
                UpdateFadeOut(dt);
                return;
            }

            bool isAnyDragging = false;

            if (_masterSlider != null && _masterSlider.IsDragging)
            {
                _masterSlider.Update();
                isAnyDragging = true;
            }
            else if (_bgmSlider != null && _bgmSlider.IsDragging)
            {
                _bgmSlider.Update();
                isAnyDragging = true;
            }
            else if (_sfxSlider != null && _sfxSlider.IsDragging)
            {
                _sfxSlider.Update();
                isAnyDragging = true;
            }
            else
            {
                if (_masterSlider != null)
                {
                    _masterSlider.Update();
                    if (_masterSlider.IsDragging) isAnyDragging = true;
                }
                if (!isAnyDragging && _bgmSlider != null)
                {
                    _bgmSlider.Update();
                    if (_bgmSlider.IsDragging) isAnyDragging = true;
                }
                if (!isAnyDragging && _sfxSlider != null)
                {
                    _sfxSlider.Update();
                    if (_sfxSlider.IsDragging) isAnyDragging = true;
                }
            }

            if (isAnyDragging) return;

            Update_ControllerNavigation(dt);

            switch (_currentState)
            {
                case SettingsState.WaitingForMouseUp:
                    if (!API.IsMouseDown(MOUSE_LEFT))
                        _currentState = SettingsState.Idle;
                    break;

                case SettingsState.Idle:
                    Update_Idle();
                    break;

                case SettingsState.ButtonDelay:
                    Update_ButtonDelay(dt);
                    break;
            }
        }

        private void Update_ControllerNavigation(float dt)
        {
            if (!API.IsGamepadConnected()) return;

            bool dpadUp   = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_UP);
            bool dpadDown = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_DOWN);
            float stickY  = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_Y);
            bool stickUp   = stickY < -0.5f;
            bool stickDown = stickY >  0.5f;
            bool aPressed  = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A);

            // Vertical navigation (5 items: 0 button + 4 sliders)
            if ((dpadUp && !_wasDpadUp) || (stickUp && !_wasStickUp))
            {
                _selectedIndex = (_selectedIndex - 1 + 5) % 5;
                UpdateVisuals();
            }
            if ((dpadDown && !_wasDpadDown) || (stickDown && !_wasStickDown))
            {
                _selectedIndex = (_selectedIndex + 1) % 5;
                UpdateVisuals();
            }

            // Horizontal navigation for sliders (indices 1–3 only)
            if (_selectedIndex >= 1 && _selectedIndex <= 3)
            {
                float stickX  = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_X);
                bool dpadLeft  = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_LEFT);
                bool dpadRight = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_RIGHT);

                float moveAmount = 0f;
                if (Math.Abs(stickX) > 0.2f) moveAmount = stickX * dt * 0.5f;
                else if (dpadLeft)  moveAmount = -dt * 0.5f;
                else if (dpadRight) moveAmount =  dt * 0.5f;

                if (Math.Abs(moveAmount) > 0.0001f)
                {
                    if (_selectedIndex == 1 && _masterSlider != null)
                        _masterSlider.SetValue(API.GetGroupVolume("Master") + moveAmount);
                    else if (_selectedIndex == 2 && _bgmSlider != null)
                        _bgmSlider.SetValue(API.GetGroupVolume("Music") + moveAmount);
                    else if (_selectedIndex == 3 && _sfxSlider != null)
                        _sfxSlider.SetValue(API.GetGroupVolume("SFX") + moveAmount);
                }
            }

            if (aPressed && !_wasAButtonPressed)
            {
                if (_selectedIndex == 0) StartClickDelay(_mainMenuButtonID);
                else if (_selectedIndex == 4) StartClickDelay(_gammaButtonID);
            }

            _wasDpadUp         = dpadUp;
            _wasDpadDown       = dpadDown;
            _wasStickUp        = stickUp;
            _wasStickDown      = stickDown;
            _wasAButtonPressed = aPressed;
        }

        private void UpdateVisuals()
        {
            API.SetSpriteTexture(_mainMenuButtonID, _selectedIndex == 0 ? MAINMENU_TEX_CLICKED : MAINMENU_TEX_NORMAL);
            API.SetSpriteTexture(_gammaButtonID,    _selectedIndex == 4 ? GAMMA_BTN_TEX_CLICKED : GAMMA_BTN_TEX_NORMAL);

            SetSliderHighlight("Settings_Master_BG", _selectedIndex == 1);
            SetSliderHighlight("Settings_BGM_BG",    _selectedIndex == 2);
            SetSliderHighlight("Settings_SFX_BG",    _selectedIndex == 3);
        }

        private void SetSliderHighlight(string bgEntityName, bool highlight)
        {
            ulong id = API.FindEntity(bgEntityName);
            if (id != 0)
                API.SetSpriteAlpha(id, highlight ? SLIDER_HIGHLIGHT_ALPHA : SLIDER_NORMAL_ALPHA);
        }

        private void Update_Idle()
        {
            if (!API.IsMouseDown(MOUSE_LEFT)) return;
            if (!API.GetMousePosInViewport(out Vec2 mousePos)) return;

            if (API.Check2DViewportClick(_mainMenuButtonID, mousePos.X, mousePos.Y))
            {
                _selectedIndex = 0;
                StartClickDelay(_mainMenuButtonID);
            }
            else if (API.Check2DViewportClick(_gammaButtonID, mousePos.X, mousePos.Y))
            {
                _selectedIndex = 4;
                StartClickDelay(_gammaButtonID);
            }
            else if (IsSliderClicked("Settings_Master_BG", mousePos)) _selectedIndex = 1;
            else if (IsSliderClicked("Settings_BGM_BG",    mousePos)) _selectedIndex = 2;
            else if (IsSliderClicked("Settings_SFX_BG",    mousePos)) _selectedIndex = 3;
        }

        private bool IsSliderClicked(string bgEntityName, Vec2 mousePos)
        {
            ulong id = API.FindEntity(bgEntityName);
            return id != 0 && API.Check2DViewportClick(id, mousePos.X, mousePos.Y);
        }

        private void Update_ButtonDelay(float dt)
        {
            _buttonDelayTimer += dt;
            if (_buttonDelayTimer >= CLICK_DELAY_DURATION)
                ExecuteClickAction();
        }

        private void StartClickDelay(ulong buttonID)
        {
            _currentState     = SettingsState.ButtonDelay;
            _clickedButtonID  = buttonID;
            _buttonDelayTimer = 0.0f;
            ButtonFX.PlayClickSound();

            if (buttonID == _mainMenuButtonID)
                API.SetSpriteTexture(buttonID, MAINMENU_TEX_CLICKED);
            else if (buttonID == _gammaButtonID)
                API.SetSpriteTexture(buttonID, GAMMA_BTN_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            if (_clickedButtonID == _mainMenuButtonID)
            {
                API.Log(">> Settings: Return to Main Menu");
                _fadingToGamma = false;
                _currentState  = SettingsState.FadingOut;
                _fadeTimer     = 0f;
            }
            else if (_clickedButtonID == _gammaButtonID)
            {
                API.Log(">> Settings: Open Gamma Adjust");
                _fadingToGamma = true;
                _currentState  = SettingsState.FadingOut;
                _fadeTimer     = 0f;
            }
            else
            {
                _currentState = SettingsState.Idle;
            }

            _clickedButtonID = 0;
        }

        private void UpdateFadeOut(float dt)
        {
            _fadeTimer += dt;
            float alpha = Clamp01(_fadeTimer / FADE_DURATION);
            API.SetScreenFadeAlpha(alpha);

            if (_fadeTimer >= FADE_DURATION)
            {
                API.SetScreenFadeAlpha(1f);
                API.LoadScene(_fadingToGamma ? "GammaAdjust" : "MainMenu");
            }
        }

        private static float Clamp01(float v) => v < 0f ? 0f : (v > 1f ? 1f : v);
    }
}
