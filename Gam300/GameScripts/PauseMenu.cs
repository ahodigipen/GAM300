using System;
using Boom;

namespace GameScripts
{
    public class PauseMenu
    {
        private const int MOUSE_LEFT = 0;
        private ulong _resumeButtonID;
        private ulong _restartButtonID;
        private ulong _mainMenuButtonID;
        private ulong _quitButtonID;

        public void OnStart(string jsonParams)
        {
            API.Log("PauseMenu OnStart Running...");
            Entry.s_ActivePauseMenuInstance = this;

            _resumeButtonID = API.FindEntity("ResumeButton");
            _restartButtonID = API.FindEntity("RestartButton");
            _mainMenuButtonID = API.FindEntity("ReturnButton");
            _quitButtonID = API.FindEntity("QuitButton");

            API.Log("Resume ID: " + _resumeButtonID);
            API.Log("Restart ID: " + _restartButtonID);
            API.Log("Return to Main Menu ID: " + _mainMenuButtonID);
            API.Log("Quit ID: " + _quitButtonID);
        }

        public void OnUpdate(float dt)
        {
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                ulong hitID = API.PickGameEntity();

                if (hitID != 0)
                {
                    // --- Resume Button ---
                    if (hitID == _resumeButtonID)
                    {
                        API.Log(">> Resume Button Clicked! Resuming Game...");
                        Entry.ResumeGame();
                    }
                    // --- Main Menu Button ---
                    else if (hitID == _mainMenuButtonID)
                    {
                        API.Log(">> Main Menu Button Clicked! Returning to Menu...");
                        Entry.ResumeGame();
                        API.LoadScene(Entry.MAIN_MENU_SCENE_NAME);
                    }
                    // --- Restart Button ---
                    else if (hitID == _restartButtonID)
                    {
                        API.Log(">> Restart Button Clicked! Restarting Scene...");
                        Entry.ResumeGame();
                        API.LoadScene(Entry.LEVEL_SCENE_NAME);
                    }
                    // --- Quit Button ---
                    else if (hitID == _quitButtonID)
                    {
                        API.Log(">> Quit Button Clicked! (Temporary: reloading scene)...");
                        API.QuitGame();
                    }
                }
            }
        }
    }
}