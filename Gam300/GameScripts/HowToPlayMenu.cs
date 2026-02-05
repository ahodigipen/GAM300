using System;
using Boom;

namespace GameScripts
{
    public class HowToPlayMenu
    {
        private const int MOUSE_LEFT = 0;
        private const string RETURN_TEX_NORMAL = "Resources/Textures/MenusUI/ReturnMenuButton.png";
        private const string RETURN_TEX_CLICKED = "Resources/Textures/MenusUI/ReturnMenuButton_Clicked.png";

        private ulong _returnButtonID;

        private enum MenuState
        {
            Idle,
            ButtonDelay
        }

        private MenuState _currentState = MenuState.Idle;
        private ulong _clickedButtonID = 0;

        // Controller input tracking
        private bool _wasAButtonPressed = false;
        private bool _wasBButtonPressed = false;
        private bool _wasStartButtonPressed = false;

        public void OnStart(string jsonParams)
        {
            API.Log("HowToPlayMenu OnStart Running...");
            _returnButtonID = API.FindEntity("ReturnButton");

            API.Log("Return ID: " + _returnButtonID);

            if (_returnButtonID == 0) API.Log("Warning: Return Button not found!");

            _currentState = MenuState.Idle;
            _clickedButtonID = 0;

            UpdateVisuals();
        }

        public void OnUpdate(float dt)
        {
            switch (_currentState)
            {
                case MenuState.Idle:
                    Update_Idle();
                    Update_ControllerInput();
                    break;

                case MenuState.ButtonDelay:
                    Update_ButtonDelay(dt);
                    break;
            }
        }

        private void Update_ControllerInput()
        {
            if (!API.IsGamepadConnected() || !Entry.CanProcessInput) return;

            bool aPressed = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_A);
            bool bPressed = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_B);
            bool startPressed = API.IsGamepadButtonDown(API.GAMEPAD_BUTTON_START);

            if ((aPressed && !_wasAButtonPressed) || 
                (bPressed && !_wasBButtonPressed) || 
                (startPressed && !_wasStartButtonPressed))
            {
                if (_returnButtonID != 0) StartClickDelay(_returnButtonID);
            }

            _wasAButtonPressed = aPressed;
            _wasBButtonPressed = bPressed;
            _wasStartButtonPressed = startPressed;
        }

        private void UpdateVisuals()
        {
            if (_returnButtonID == 0) return;

            // Since there's only one button, we'll highlight it if a controller is connected
            if (API.IsGamepadConnected())
                API.SetSpriteTexture(_returnButtonID, RETURN_TEX_CLICKED);
            else
                API.SetSpriteTexture(_returnButtonID, RETURN_TEX_NORMAL);
        }

        private void Update_Idle()
        {
            if (!Entry.CanProcessInput) return;

            if (API.IsMouseDown(MOUSE_LEFT))
            {
                if (!API.GetMousePosInViewport(out Vec2 mousePos))
                {
                    return;
                }

                if (API.Check2DViewportClick(_returnButtonID, mousePos.X, mousePos.Y))
                {
                    StartClickDelay(_returnButtonID);
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

            if (buttonID == _returnButtonID)
                API.SetSpriteTexture(buttonID, RETURN_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            _currentState = MenuState.Idle;

            if (_clickedButtonID == _returnButtonID)
            {
                API.Log(">> Return Button Clicked! Returning to Main Menu...");
                API.LoadScene(Entry.MAIN_MENU_SCENE_NAME);
            }
        }
    }
}