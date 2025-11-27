using System;
using Boom;

namespace GameScripts
{
    public class MainMenu
    {
        private const int MOUSE_LEFT = 0;

        // Store the unique IDs of our buttons
        private ulong _startButtonID;
        private ulong _quitButtonID;

        public void OnStart(string jsonParams)
        {
            API.Log("MainMenu OnStart Running...");
            // 1. Find the entities by name once at startup
            // (Make sure your entities are named exactly this in the Editor!)
            _startButtonID = API.FindEntity("StartButton");
            _quitButtonID = API.FindEntity("QuitButton");

            // Add these logs to verify they were actually found
            API.Log("Start ID: " + _startButtonID);
            API.Log("Quit ID: " + _quitButtonID);

            if (_startButtonID == 0) API.Log("Warning: StartButton not found!");
        }

        public void OnUpdate(float dt)
        {
            // 2. Check for Click
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                // DEBUG 2: Confirm Input works
                API.Log(">> Mouse Click Detected!");

                ulong hitID = API.PickGameEntity();

                // DEBUG 3: See what the raycast actually returned
                API.Log(">> Raycast returned ID: " + hitID);

                if (hitID != 0)
                {
                    API.Log("Hit ID: " + hitID);
                    API.Log("Wanted Start ID: " + _startButtonID);

                    if (hitID == _startButtonID)
                    {
                        API.Log(">> Start Button Clicked! Starting Game...");
                        API.LoadScene("M2_Scene");
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
                    // DEBUG 4: Confirm we entered the hit block
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