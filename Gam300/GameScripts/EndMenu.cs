using System;
using Boom;

namespace GameScripts
{
    public class EndMenu
    {
        private const int MOUSE_LEFT = 0;

        // --- Texture Constants ---
        private const string MAINMENU_TEX_NORMAL = "Resources/Textures/MenusUI/ReturnMenuButton.png";
        private const string MAINMENU_TEX_CLICKED = "Resources/Textures/MenusUI/ReturnMenuButton_Clicked.png";

        private ulong _mainMenuButtonID;
        private ulong _backgroundID;

        private enum EndMenuState
        {
            Idle,
            ButtonDelay,
            WaitingForMouseUp
        }

        private EndMenuState _currentState = EndMenuState.Idle;
        private ulong _clickedButtonID = 0;
        private bool _wasEndedLastFrame = false;

        private float _buttonDelayTimer = 0.0f;
        private const float CLICK_DELAY_DURATION = 0.1f;

        public void OnStart(string jsonParams)
        {
            API.Log("EndMenu OnStart Running...");
            Entry.s_ActiveEndMenuInstance = this; 

            _mainMenuButtonID = API.FindEntity("End_ReturnButton");
            _backgroundID = API.FindEntity("End_Background");

            ResetButtonState();
        }

        public void OnUpdate(float dt)
        {
            if (Entry.s_ActiveEndMenuInstance != this)
            {
                Entry.s_ActiveEndMenuInstance = this;
            }

            if (Entry.IsGameEnded && !_wasEndedLastFrame)
            {
                ResetButtonState();
            }
            _wasEndedLastFrame = Entry.IsGameEnded;

            // 2. Only update if the game has ended
            if (!Entry.IsGameEnded) return;
            if (Entry.s_RequestedEndAction != Entry.EndMenuAction.None) return;


            // 3. State Machine
            switch (_currentState)
            {
                case EndMenuState.WaitingForMouseUp:
                    if (!API.IsMouseDown(MOUSE_LEFT))
                    {
                        _currentState = EndMenuState.Idle;
                    }
                    break;

                case EndMenuState.Idle:
                    Update_Idle();
                    break;

                case EndMenuState.ButtonDelay:
                    Update_ButtonDelay(dt);
                    break;
            }
        }

        public void ResetButtonState()
        {
            _currentState = EndMenuState.WaitingForMouseUp;
            _clickedButtonID = 0;
            _buttonDelayTimer = 0.0f;

            if (_mainMenuButtonID != 0)
                API.SetSpriteTexture(_mainMenuButtonID, MAINMENU_TEX_NORMAL);
        }

        private void Update_Idle()
        {
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                if (!API.GetMousePosInViewport(out Vec2 mousePos)) { return; }

                if (API.Check2DViewportClick(_mainMenuButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_mainMenuButtonID);
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
            _currentState = EndMenuState.ButtonDelay;
            _clickedButtonID = buttonID;
            _buttonDelayTimer = 0.0f;

            if (buttonID == _mainMenuButtonID)
                API.SetSpriteTexture(buttonID, MAINMENU_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            _currentState = EndMenuState.Idle;

            // Perform the action directly via Entry helpers
            if (_clickedButtonID == _mainMenuButtonID)
            {
                Entry.s_RequestedEndAction = Entry.EndMenuAction.MainMenu;

            }
        }
    }
}