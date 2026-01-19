using System;
using Boom;

namespace GameScripts
{
    public class DeathMenu
    {
        private const int MOUSE_LEFT = 0;

        // --- Texture Constants ---
        private const string RESTART_TEX_NORMAL = "Resources/Textures/PauseMenu/RestartButton.png";
        private const string MAINMENU_TEX_NORMAL = "Resources/Textures/PauseMenu/ReturnMenuButton.png";

        private const string RESTART_TEX_CLICKED = "Resources/Textures/PauseMenu/RestartButton_Clicked.png";
        private const string MAINMENU_TEX_CLICKED = "Resources/Textures/PauseMenu/ReturnMenuButton_Clicked.png";

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

        public void OnStart(string jsonParams)
        {
            API.Log("DeathMenu OnStart Running...");
            Entry.s_ActiveDeathMenuInstance = this;

            _restartButtonID = API.FindEntity("Death_RestartButton");
            _mainMenuButtonID = API.FindEntity("Death_ReturnButton");
            _backgroundID = API.FindEntity("Death_Background");

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

        public void ResetButtonState()
        {
            _currentState = DeathMenuState.WaitingForMouseUp;
            _clickedButtonID = 0;

            if (_restartButtonID != 0)
                API.SetSpriteTexture(_restartButtonID, RESTART_TEX_NORMAL);
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
                else if (API.Check2DViewportClick(_restartButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_restartButtonID);
            }
        }

        private void Update_ButtonDelay(float dt)
        {
            ExecuteClickAction();
        }

        private void StartClickDelay(ulong buttonID)
        {
            _currentState = DeathMenuState.ButtonDelay;
            _clickedButtonID = buttonID;

            if (buttonID == _mainMenuButtonID)
                API.SetSpriteTexture(buttonID, MAINMENU_TEX_CLICKED);
            else if (buttonID == _restartButtonID)
                API.SetSpriteTexture(buttonID, RESTART_TEX_CLICKED);
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
