using System;
using Boom;

namespace GameScripts
{
    public class DeathMenu
    {
        private const int MOUSE_LEFT = 0;

        // --- Texture Constants ---
        private const string RESTART_TEX_NORMAL = "Resources/Textures/MenusUI/RestartButton.png";
        private const string MAINMENU_TEX_NORMAL = "Resources/Textures/MenusUI/ReturnMenuButton.png";

        private const string RESTART_TEX_CLICKED = "Resources/Textures/MenusUI/RestartButton_Clicked.png";
        private const string MAINMENU_TEX_CLICKED = "Resources/Textures/MenusUI/ReturnMenuButton_Clicked.png";

        private ulong _restartButtonID;
        private ulong _mainMenuButtonID;
        private ulong _backgroundID;

        private enum DeathMenuState
        {
            Idle,
            ButtonDelay,
            WaitingForMouseUp
        }

        private DeathMenuState _currentState = DeathMenuState.Idle;
        private ulong _clickedButtonID = 0;
        private bool _wasDeadLastFrame = false;

        // Controller navigation
        private int _selectedIndex = 0; // 0: Restart, 1: Main Menu
        private bool _wasDpadUp = false;
        private bool _wasDpadDown = false;
        private bool _wasStickUp = false;
        private bool _wasStickDown = false;
        private bool _wasAButtonPressed = false;

        private float _buttonDelayTimer = 0.0f;
        private const float CLICK_DELAY_DURATION = 0.1f;

        public void OnStart(string jsonParams)
        {
            API.Log("DeathMenu OnStart Running...");
            Entry.s_ActiveDeathMenuInstance = this;

            _restartButtonID = API.FindEntity("Death_RestartButton");
            _mainMenuButtonID = API.FindEntity("Death_ReturnButton");
            _backgroundID = API.FindEntity("Death_Background");

            _selectedIndex = -1;
            ResetButtonState();
        }

        public void OnUpdate(float dt)
        {
            if (Entry.s_ActiveDeathMenuInstance != this)
            {
                Entry.s_ActiveDeathMenuInstance = this;
            }

            if (Entry.IsPlayerDead && !_wasDeadLastFrame)
            {
                ResetButtonState();
            }
            _wasDeadLastFrame = Entry.IsPlayerDead;

            if (!Entry.IsPlayerDead) return;
            if (Entry.s_RequestedDeathAction != Entry.DeathMenuAction.None) return;

            Update_ControllerNavigation();

            switch (_currentState)
            {
                case DeathMenuState.WaitingForMouseUp:
                    if (!API.IsMouseDown(MOUSE_LEFT))
                    {
                        _currentState = DeathMenuState.Idle;
                    }
                    break;

                case DeathMenuState.Idle:
                    Update_Idle();
                    break;

                case DeathMenuState.ButtonDelay:
                    Update_ButtonDelay(dt);
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

            if ((dpadUp && !_wasDpadUp) || (stickUp && !_wasStickUp))
            {
                if (_selectedIndex == -1) _selectedIndex = 1; // Highlight Main Menu if coming from nothing
                else _selectedIndex = (_selectedIndex - 1 + 2) % 2;
                UpdateVisuals();
            }
            if ((dpadDown && !_wasDpadDown) || (stickDown && !_wasStickDown))
            {
                if (_selectedIndex == -1) _selectedIndex = 0; // Highlight Restart if coming from nothing
                else _selectedIndex = (_selectedIndex + 1) % 2;
                UpdateVisuals();
            }

            if (aPressed && !_wasAButtonPressed)
            {
                ulong buttonID = 0;
                if (_selectedIndex == 0) buttonID = _restartButtonID;
                else if (_selectedIndex == 1) buttonID = _mainMenuButtonID;

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
            // Reset all to normal (Texture and full brightness)
            if (_restartButtonID != 0)
            {
                API.SetSpriteTexture(_restartButtonID, RESTART_TEX_NORMAL);
                API.SetSpriteColor(_restartButtonID, new Vec4(1.0f, 1.0f, 1.0f, 1.0f));
            }
            if (_mainMenuButtonID != 0)
            {
                API.SetSpriteTexture(_mainMenuButtonID, MAINMENU_TEX_NORMAL);
                API.SetSpriteColor(_mainMenuButtonID, new Vec4(1.0f, 1.0f, 1.0f, 1.0f));
            }

            // Highlight selected by darkening
            if (_selectedIndex == -1) return;

            if (_selectedIndex == 0 && _restartButtonID != 0)
            {
                API.SetSpriteColor(_restartButtonID, new Vec4(0.5f, 0.5f, 0.5f, 1.0f));
            }
            else if (_selectedIndex == 1 && _mainMenuButtonID != 0)
            {
                API.SetSpriteColor(_mainMenuButtonID, new Vec4(0.5f, 0.5f, 0.5f, 1.0f));
            }
        }

        public void ResetButtonState()
        {
            _selectedIndex = -1;
            _currentState = DeathMenuState.WaitingForMouseUp;
            _clickedButtonID = 0;
            _buttonDelayTimer = 0.0f;

            UpdateVisuals();
        }

        private void Update_Idle()
        {
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                if (!API.GetMousePosInViewport(out Vec2 mousePos)) { return; }

                if (API.Check2DViewportClick(_restartButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 0;
                    StartClickDelay(_restartButtonID);
                }
                else if (API.Check2DViewportClick(_mainMenuButtonID, mousePos.X, mousePos.Y))
                {
                    _selectedIndex = 1;
                    StartClickDelay(_mainMenuButtonID);
                }
            }
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
            _currentState = DeathMenuState.ButtonDelay;
            _clickedButtonID = buttonID;
            _buttonDelayTimer = 0.0f;

            if (buttonID != 0)
                API.SetSpriteColor(buttonID, new Vec4(0.5f, 0.5f, 0.5f, 1.0f));
        }

        private void ExecuteClickAction()
        {
            _currentState = DeathMenuState.Idle;

            if (_clickedButtonID == _mainMenuButtonID)
            {
                Entry.s_RequestedDeathAction = Entry.DeathMenuAction.MainMenu;
            }
            else if (_clickedButtonID == _restartButtonID)
            {
                Entry.s_RequestedDeathAction = Entry.DeathMenuAction.Restart;
            }
        }
    }
}
