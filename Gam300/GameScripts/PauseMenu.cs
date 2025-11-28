using System;
using Boom;

namespace GameScripts
{
    public class PauseMenu
    {
        private const int MOUSE_LEFT = 0;

        // Store the unique IDs of our buttons
        // (Make sure your entities are named exactly this in the Editor!)
        private ulong _resumeButtonID;
        private ulong _restartButtonID;
        private ulong _mainMenuButtonID;
        private ulong _quitButtonID;

        public void OnStart(string jsonParams)
        {
            API.Log("PauseMenu OnStart Running...");
            Entry.s_ActivePauseMenuInstance = this;

            // 1. Find the entities by name once at startup
            _resumeButtonID = API.FindEntity("ResumeButton");
            _restartButtonID = API.FindEntity("RestartButton");
            _mainMenuButtonID = API.FindEntity("ReturnButton");
            _quitButtonID = API.FindEntity("QuitButton");

            // Add logs to verify they were found
            API.Log("Resume ID: " + _resumeButtonID);
            API.Log("Restart ID: " + _restartButtonID);
            API.Log("Return to Main Menu ID: " + _mainMenuButtonID);
            API.Log("Quit ID: " + _quitButtonID);
        }

        public void OnUpdate(float dt)
        {
            // 2. Check for Click
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                ulong hitID = API.PickGameEntity();

                if (hitID != 0)
                {
                    // --- Resume Button ---
                    if (hitID == _resumeButtonID)
                    {
                        API.Log(">> Resume Button Clicked! Resuming Game...");
                        // This logic is copied from Entry.cs's 'R' key logic
                        Entry.ResumeGame();
                    }
                    // --- Main Menu Button ---
                    else if (hitID == _mainMenuButtonID)
                    {
                        API.Log(">> Main Menu Button Clicked! Returning to Menu...");
                        // This logic is copied from Entry.cs's 'M' key logic
                        Entry.ResumeGame();
                        API.LoadScene(Entry.MAIN_MENU_SCENE_NAME); // 2. ...THEN load the new scene
                    }
                    // --- Restart Button ---
                    else if (hitID == _restartButtonID)
                    {
                        API.Log(">> Restart Button Clicked! Restarting Scene...");
                        // This logic is copied from Entry.cs's 'Y' key logic
                        Entry.ResumeGame();
                        API.LoadScene(Entry.LEVEL_SCENE_NAME); // 2. Reload the current game scene
                    }
                    // --- Quit Button ---
                    else if (hitID == _quitButtonID)
                    {
                        API.Log(">> Quit Button Clicked! (Temporary: reloading scene)...");
                        // This logic is copied from Entry.cs's 'Q' key logic
                        API.QuitGame();
                        // API.LoadScene(Entry._currentSceneName);
                    }
                }
            }
        }
    }
}