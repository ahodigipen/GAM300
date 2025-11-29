using System;
using Boom;

namespace GameScripts
{
    public class MainMenu
    {
        private const int MOUSE_LEFT = 0;

        private const string NEWGAME_TEX_CLICKED = "Resources/Textures/MainMenu/NewGameButton_Clicked.png";
        private const string HOWTOPLAY_TEX_CLICKED = "Resources/Textures/MainMenu/HowToPlayButton_Clicked.png";
        private const string QUIT_TEX_CLICKED = "Resources/Textures/MainMenu/ExitButton_Clicked.png";

        private ulong _newGameButtonID;
        private ulong _howToPlayButtonID;
        private ulong _quitButtonID;

        private enum MenuState
        {
            Idle,
            ButtonDelay
        }

        private MenuState _currentState = MenuState.Idle;
        private ulong _clickedButtonID = 0;

        public void OnStart(string jsonParams)
        {
            API.Log("MainMenu OnStart Running...");
            _newGameButtonID = API.FindEntity("NewGameButton");
            _howToPlayButtonID = API.FindEntity("HowToPlayButton");
            _quitButtonID = API.FindEntity("QuitButton");

            _currentState = MenuState.Idle;
            _clickedButtonID = 0;
        }

        public void OnUpdate(float dt)
        {
            switch (_currentState)
            {
                case MenuState.Idle:
                    Update_Idle();
                    break;

                case MenuState.ButtonDelay:
                    Update_ButtonDelay(dt);
                    break;
            }
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
                    StartClickDelay(_newGameButtonID);
                else if (API.Check2DViewportClick(_howToPlayButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_howToPlayButtonID);
                else if (API.Check2DViewportClick(_quitButtonID, mousePos.X, mousePos.Y))
                    StartClickDelay(_quitButtonID);
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

            // 3. Set the texture
            if (buttonID == _newGameButtonID)
                API.SetSpriteTexture(buttonID, NEWGAME_TEX_CLICKED);
            else if (buttonID == _howToPlayButtonID)
                API.SetSpriteTexture(buttonID, HOWTOPLAY_TEX_CLICKED);
            else if (buttonID == _quitButtonID)
                API.SetSpriteTexture(buttonID, QUIT_TEX_CLICKED);
        }

        private void ExecuteClickAction()
        {
            _currentState = MenuState.Idle;

            if (_clickedButtonID == _newGameButtonID)
            {
                API.Log(">> New Game Button Clicked! Starting Game...");
                API.LoadScene(Entry.LEVEL_SCENE_NAME);
            }
            else if (_clickedButtonID == _howToPlayButtonID)
            {
                API.Log(">> How To Play Button Clicked! Loading HowToPlay...");
                API.LoadScene(Entry.HOW_TO_PLAY_SCENE_NAME);
            }
            else if (_clickedButtonID == _quitButtonID)
            {
                API.Log(">> Quit Button Clicked! Exiting Game...");
                API.QuitGame();
            }
        }
    }
}