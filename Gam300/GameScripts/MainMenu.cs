using System;
using Boom;

namespace GameScripts
{
    public class MainMenu
    {
        private const int MOUSE_LEFT = 0;
        private ulong _newGameButtonID;
        private ulong _howToPlayButtonID;
        private ulong _quitButtonID;

        public void OnStart(string jsonParams)
        {
            API.Log("MainMenu OnStart Running...");
            _newGameButtonID = API.FindEntity("NewGameButtonTest");
            _howToPlayButtonID = API.FindEntity("HowToPlayButton");
            _quitButtonID = API.FindEntity("QuitButton");

            API.Log("Start ID: " + _newGameButtonID);
            API.Log("How To Play ID: " + _howToPlayButtonID);
            API.Log("Quit ID: " + _quitButtonID);

            if (_newGameButtonID == 0) API.Log("Warning: NewGameButton not found!");
        }

        public void OnUpdate(float dt)
        {
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                // 1. Get mouse position in viewport pixels
                if (!API.GetMousePosInViewport(out Vec2 mousePos))
                {
                    return; // Mouse is outside the viewport
                }

                // --- NEW DEBUG LINE ---
                API.Log($"[Debug] Mouse Click at: X={mousePos.X}, Y={mousePos.Y}");
                // --- END DEBUG LINE ---


                // --- NEW DEBUG LINES for Button Positions ---
                // We project each button's 3D position to 2D screen space to compare
                if (_newGameButtonID != 0 && API.ProjectWorldToViewport(API.GetTransform(_newGameButtonID).Position, out Vec2 newGamePos))
                {
                    API.Log($"[Debug] NewGameButton 2D Pos: X={newGamePos.X}, Y={newGamePos.Y}");
                }
                //if (_howToPlayButtonID != 0 && API.ProjectWorldToViewport(API.GetTransform(_howToPlayButtonID).Position, out Vec2 howToPlayPos))
                //{
                //    API.Log($"[Debug] HowToPlayButton 2D Pos: X={howToPlayPos.X}, Y={howToPlayPos.Y}");
                //}
                //if (_quitButtonID != 0 && API.ProjectWorldToViewport(API.GetTransform(_quitButtonID).Position, out Vec2 quitPos))
                //{
                //    API.Log($"[Debug] QuitButton 2D Pos: X={quitPos.X}, Y={quitPos.Y}");
                //}
                // --- END DEBUG LINES ---


                // 2. Check each button using the accurate API call
                if (API.Check2DViewportClick(_newGameButtonID, mousePos.X, mousePos.Y))
                {
                    API.Log(">> New Game Button Clicked! Starting Game...");
                    //API.LoadScene(Entry.LEVEL_SCENE_NAME);
                }
                else if (API.Check2DViewportClick(_howToPlayButtonID, mousePos.X, mousePos.Y))
                {
                    API.Log(">> How To Play Button Clicked! Loading HowToPlay...");
                    API.LoadScene(Entry.HOW_TO_PLAY_SCENE_NAME);
                }
                else if (API.Check2DViewportClick(_quitButtonID, mousePos.X, mousePos.Y))
                {
                    API.Log(">> Quit Button Clicked! Exiting Game...");
                    API.QuitGame();
                }
            }
        }




    }
}