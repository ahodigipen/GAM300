using System;
using Boom;

namespace GameScripts
{
    public class PauseMenu
    {
        private const int MOUSE_LEFT = 0;

        // --- Texture Constants ---
        private const string RESUME_TEX_NORMAL = "Resources/Textures/MenusUI/ResumeButton.png";
        private const string RESTART_TEX_NORMAL = "Resources/Textures/MenusUI/RestartButton.png";
        private const string MAINMENU_TEX_NORMAL = "Resources/Textures/MenusUI/ReturnMenuButton.png";
        private const string CREDITS_TEX_NORMAL = "Resources/Textures/MenusUI/CreditsButton.png";
        private const string QUIT_TEX_NORMAL = "Resources/Textures/MenusUI/QuitButton.png";

        private const string RESUME_TEX_CLICKED = "Resources/Textures/MenusUI/ResumeButton_Clicked.png";
        private const string RESTART_TEX_CLICKED = "Resources/Textures/MenusUI/RestartButton_Clicked.png";
        private const string MAINMENU_TEX_CLICKED = "Resources/Textures/MenusUI/ReturnMenuButton_Clicked.png";
        private const string CREDITS_TEX_CLICKED = "Resources/Textures/MenusUI/CreditsButton_Clicked.png";
        private const string QUIT_TEX_CLICKED = "Resources/Textures/MenusUI/QuitButton_Clicked.png";

        private ulong _resumeButtonID;
        private ulong _restartButtonID;
        private ulong _mainMenuButtonID;
        private ulong _creditsButtonID;
        private ulong _quitButtonID;

        private ButtonFX _buttonFX;

        private VolumeSlider _masterSlider;
        private VolumeSlider _bgmSlider;
        private VolumeSlider _sfxSlider;
        private GammaSlider  _gammaSlider;

        private enum PauseMenuState
        {
            Idle,
            ButtonDelay,
            WaitingForMouseUp
        }

        private PauseMenuState _currentState = PauseMenuState.Idle;
        private ulong _clickedButtonID = 0;
        private bool _wasPausedLastFrame = false;

        // Controller navigation
        // -1: None, 0: Resume, 1: Restart, 2: Main Menu, 3: Credits, 4: Quit, 5: Master, 6: BGM, 7: SFX, 8: Gamma
        private int _selectedIndex = -1; 

        private bool _wasDpadUp = false;
        private bool _wasDpadDown = false;
        private bool _wasStickUp = false;
        private bool _wasStickDown = false;
        private bool _wasAButtonPressed = false;

        private float _buttonDelayTimer = 0.0f;
        private const float CLICK_DELAY_DURATION = 0.1f;

        // Visual constants for slider highlighting
        private const float SLIDER_HIGHLIGHT_ALPHA = 1.0f;
        private const float SLIDER_NORMAL_ALPHA = 0.6f;

        public void OnStart(string jsonParams)
        {
            API.Log("PauseMenu OnStart Running...");
            Entry.s_ActivePauseMenuInstance = this;

            // Load saved settings on start
            SettingsManager.LoadSettings();

            _resumeButtonID = API.FindEntity("Pause_ResumeButton");
            _restartButtonID = API.FindEntity("Pause_RestartButton");
            _mainMenuButtonID = API.FindEntity("Pause_ReturnButton");
            _creditsButtonID = API.FindEntity("Pause_CreditsButton");
            _quitButtonID = API.FindEntity("Pause_QuitButton");

            _buttonFX = new ButtonFX(_resumeButtonID, _restartButtonID, _mainMenuButtonID, _creditsButtonID, _quitButtonID);

            _masterSlider = new VolumeSlider("Pause_Master_BG", "Pause_Master_Fill", "Pause_Master_Handle", "Master");
            _bgmSlider = new VolumeSlider("Pause_BGM_BG", "Pause_BGM_Fill", "Pause_BGM_Handle", "Music");
            _sfxSlider = new VolumeSlider("Pause_SFX_BG", "Pause_SFX_Fill", "Pause_SFX_Handle", "SFX");
            _gammaSlider = new GammaSlider("Pause_Gamma_BG", "Pause_Gamma_Fill", "Pause_Gamma_Handle");

            _selectedIndex = -1;
            ResetButtonState();
        }

        public void OnUpdate(float dt)
        {
            if (Entry.s_ActivePauseMenuInstance != this)
            {
                Entry.s_ActivePauseMenuInstance = this;
            }

            if (Entry.IsGamePaused && !_wasPausedLastFrame)
            {
                ResetButtonState();
            }
            _wasPausedLastFrame = Entry.IsGamePaused;

            if (!Entry.IsGamePaused) return;
            if (Entry.s_RequestedPauseAction != Entry.PauseMenuAction.None) return;

            // Always update hover effects
            _buttonFX?.Update(dt);

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
            else if (_gammaSlider != null && _gammaSlider.IsDragging)
            {
                _gammaSlider.Update();
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

                if (!isAnyDragging && _gammaSlider != null)
                {
                    _gammaSlider.Update();
                    if (_gammaSlider.IsDragging) isAnyDragging = true;
                }
            }

            if (isAnyDragging) return;

            Update_ControllerNavigation(dt);

            switch (_currentState)
            {
                case PauseMenuState.WaitingForMouseUp:
                    if (!API.IsMouseDown(MOUSE_LEFT))
                    {
                        _currentState = PauseMenuState.Idle;
                    }
                    break;

                case PauseMenuState.Idle:
                    Update_Idle();
                    break;

                case PauseMenuState.ButtonDelay:
                    Update_ButtonDelay(dt);
                    break;
            }
        }

        private void Update_ControllerNavigation(float dt)
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
                if (dpadUp || dpadDown || stickUp || stickDown)
                {
                    _selectedIndex = 0;
                    _buttonFX?.SetControllerSelection(_selectedIndex);
                    UpdateVisuals();
                }

                _wasDpadUp = dpadUp;
                _wasDpadDown = dpadDown;
                _wasStickUp = stickUp;
                _wasStickDown = stickDown;
                _wasAButtonPressed = aPressed;
                return;
            }

            // Vertical Navigation
            if ((dpadUp && !_wasDpadUp) || (stickUp && !_wasStickUp))
            {
                _selectedIndex = (_selectedIndex - 1 + 9) % 9;
                _buttonFX?.SetControllerSelection(_selectedIndex);
                UpdateVisuals();
            }
            if ((dpadDown && !_wasDpadDown) || (stickDown && !_wasStickDown))
            {
                _selectedIndex = (_selectedIndex + 1) % 9;
                _buttonFX?.SetControllerSelection(_selectedIndex);
                UpdateVisuals();
            }

            // Horizontal Navigation (Slider Control)
            if (_selectedIndex >= 5)
            {
                float stickX = API.GetGamepadAxis(API.GAMEPAD_AXIS_LEFT_X);
                bool dpadLeft = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_LEFT);
                bool dpadRight = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_DPAD_RIGHT);

                float moveAmount = 0f;
                if (Math.Abs(stickX) > 0.2f) moveAmount = stickX * dt * 0.5f;
                else if (dpadLeft) moveAmount = -dt * 0.5f;
                else if (dpadRight) moveAmount = dt * 0.5f;

                if (Math.Abs(moveAmount) > 0.0001f)
                {
                    if (_selectedIndex == 5 && _masterSlider != null)
                    {
                        _masterSlider.SetValue(API.GetGroupVolume("Master") + moveAmount);
                    }
                    else if (_selectedIndex == 6 && _bgmSlider != null)
                    {
                        _bgmSlider.SetValue(API.GetGroupVolume("Music") + moveAmount);
                    }
                    else if (_selectedIndex == 7 && _sfxSlider != null)
                    {
                        _sfxSlider.SetValue(API.GetGroupVolume("SFX") + moveAmount);
                    }
                    else if (_selectedIndex == 8 && _gammaSlider != null)
                    {
                        _gammaSlider.SetNormDelta(moveAmount);
                    }
                }
            }

            if (aPressed && !_wasAButtonPressed)
            {
                ulong buttonID = 0;
                if (_selectedIndex == 0) buttonID = _resumeButtonID;
                else if (_selectedIndex == 1) buttonID = _restartButtonID;
                else if (_selectedIndex == 2) buttonID = _mainMenuButtonID;
                else if (_selectedIndex == 3) buttonID = _creditsButtonID;
                else if (_selectedIndex == 4) buttonID = _quitButtonID;

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
            // Reset buttons to normal
            API.SetSpriteTexture(_resumeButtonID, RESUME_TEX_NORMAL);
            API.SetSpriteTexture(_restartButtonID, RESTART_TEX_NORMAL);
            API.SetSpriteTexture(_mainMenuButtonID, MAINMENU_TEX_NORMAL);
            API.SetSpriteTexture(_creditsButtonID, CREDITS_TEX_NORMAL);
            API.SetSpriteTexture(_quitButtonID, QUIT_TEX_NORMAL);

            // Reset sliders visuals (using alpha for highlight for now)
            SetSliderHighlight("Pause_Master_BG", _selectedIndex == 5);
            SetSliderHighlight("Pause_BGM_BG", _selectedIndex == 6);
            SetSliderHighlight("Pause_SFX_BG", _selectedIndex == 7);
            SetSliderHighlight("Pause_Gamma_BG", _selectedIndex == 8);

            if (_selectedIndex == -1) return;

            // Highlight selected button
            if (_selectedIndex == 0) API.SetSpriteTexture(_resumeButtonID, RESUME_TEX_CLICKED);
            else if (_selectedIndex == 1) API.SetSpriteTexture(_restartButtonID, RESTART_TEX_CLICKED);
            else if (_selectedIndex == 2) API.SetSpriteTexture(_mainMenuButtonID, MAINMENU_TEX_CLICKED);
            else if (_selectedIndex == 3) API.SetSpriteTexture(_creditsButtonID, CREDITS_TEX_CLICKED);
            else if (_selectedIndex == 4) API.SetSpriteTexture(_quitButtonID, QUIT_TEX_CLICKED);
        }

        private void SetSliderHighlight(string bgEntityName, bool highlight)
        {
            ulong id = API.FindEntity(bgEntityName);
            if (id != 0)
            {
                API.SetSpriteAlpha(id, highlight ? SLIDER_HIGHLIGHT_ALPHA : SLIDER_NORMAL_ALPHA);
            }
        }

        public void ResetButtonState()
        {
            _selectedIndex = -1;
            _currentState = PauseMenuState.WaitingForMouseUp;
            _clickedButtonID = 0;
            _buttonDelayTimer = 0.0f;
            _buttonFX?.Reset();

            UpdateVisuals();
        }

        private void Update_Idle()
        {
            bool isAnySliderDragging = (_masterSlider != null && _masterSlider.IsDragging) ||
                                       (_bgmSlider != null && _bgmSlider.IsDragging) ||
                                       (_sfxSlider != null && _sfxSlider.IsDragging) ||
                                       (_gammaSlider != null && _gammaSlider.IsDragging);

            if (isAnySliderDragging) return;

            if (API.IsMouseDown(MOUSE_LEFT))
            {
                if (!API.GetMousePosInViewport(out Vec2 mousePos)) { return; }

                if (API.Check2DViewportClick(_resumeButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 0;
                    StartClickDelay(_resumeButtonID);
                }
                else if (API.Check2DViewportClick(_restartButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 1;
                    StartClickDelay(_restartButtonID);
                }
                else if (API.Check2DViewportClick(_mainMenuButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 2;
                    StartClickDelay(_mainMenuButtonID);
                }
                else if (API.Check2DViewportClick(_creditsButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 3;
                    StartClickDelay(_creditsButtonID);
                }
                else if (API.Check2DViewportClick(_quitButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 4;
                    StartClickDelay(_quitButtonID);
                }
                else if (IsSliderClicked("Pause_Master_BG", mousePos)) _selectedIndex = 5;
                else if (IsSliderClicked("Pause_BGM_BG", mousePos)) _selectedIndex = 6;
                else if (IsSliderClicked("Pause_SFX_BG", mousePos)) _selectedIndex = 7;
                else if (IsSliderClicked("Pause_Gamma_BG", mousePos)) _selectedIndex = 8;
            }
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
            {
                ExecuteClickAction();
            }
        }

        private void StartClickDelay(ulong buttonID)
        {
            _currentState = PauseMenuState.ButtonDelay;
            _clickedButtonID = buttonID;
            _buttonDelayTimer = 0.0f;
            ButtonFX.PlayClickSound();

            if (buttonID == _resumeButtonID)
                API.SetSpriteTexture(buttonID, RESUME_TEX_CLICKED);
            else if (buttonID == _restartButtonID)
                API.SetSpriteTexture(buttonID, RESTART_TEX_CLICKED);
            else if (buttonID == _mainMenuButtonID)
                API.SetSpriteTexture(buttonID, MAINMENU_TEX_CLICKED);
            else if (buttonID == _creditsButtonID)
                API.SetSpriteTexture(buttonID, CREDITS_TEX_CLICKED);
            else if (buttonID == _quitButtonID)
                API.SetSpriteTexture(buttonID, QUIT_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            _currentState = PauseMenuState.Idle;

            if (_clickedButtonID == _resumeButtonID)
            {
                Entry.s_RequestedPauseAction = Entry.PauseMenuAction.Resume;
            }
            else if (_clickedButtonID == _restartButtonID)
            {
                Entry.s_RequestedPauseAction = Entry.PauseMenuAction.Restart;
            }
            else if (_clickedButtonID == _mainMenuButtonID)
            {
                Entry.s_RequestedPauseAction = Entry.PauseMenuAction.MainMenu;
            }
            else if (_clickedButtonID == _creditsButtonID)
            {
                Entry.s_RequestedPauseAction = Entry.PauseMenuAction.Credits;
            }
            else if (_clickedButtonID == _quitButtonID)
            {
                Entry.s_RequestedPauseAction = Entry.PauseMenuAction.Quit;
            }
        }
    }
}
