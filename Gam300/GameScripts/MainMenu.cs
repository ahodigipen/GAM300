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
            _newGameButtonID = API.FindEntity("NewGameButton");
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
                API.Log(">> Mouse Click Detected!");

                ulong hitID = API.PickGameEntity();

                API.Log(">> Raycast returned ID: " + hitID);

                if (hitID != 0)
                {
                    API.Log("Hit ID: " + hitID);
                    API.Log("Wanted Start ID: " + _newGameButtonID);

                    if (hitID == _newGameButtonID)
                    {
                        API.Log(">> New Game Button Clicked! Starting Game...");
                        API.LoadScene(Entry.LEVEL_SCENE_NAME);
                    }
                    else if (hitID == _howToPlayButtonID)
                    {
                        API.Log(">> How To Play Button Clicked! Exiting Game...");
                        API.LoadScene(Entry.HOW_TO_PLAY_SCENE_NAME);
                    }
                    else if (hitID == _quitButtonID)
                    {
                        API.Log(">> Quit Button Clicked! Exiting Game...");
                        API.QuitGame();
                    }
                    else
                    {
                        API.Log(">> Clicked on an unrecognized entity.");
                    }
                    API.Log(">> HIT VALID ENTITY!");
                }
                else
                {
                    API.Log(">> Raycast Missed (ID was 0)");
                }
            }
        }
    }
}