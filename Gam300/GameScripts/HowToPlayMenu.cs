using System;
using Boom;

namespace GameScripts
{
    public class HowToPlayMenu
    {
        private const int MOUSE_LEFT = 0;
        private ulong _returnButtonID;

        public void OnStart(string jsonParams)
        {
            API.Log("MainMenu OnStart Running...");
            _returnButtonID = API.FindEntity("ReturnButton");

            API.Log("Return ID: " + _returnButtonID);

            if (_returnButtonID == 0) API.Log("Warning: Return Button not found!");
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
                    API.Log("Wanted Start ID: " + _returnButtonID);

                    if (hitID == _returnButtonID)
                    {
                        API.Log(">> Return Button Clicked! Returning to Main Menu...");
                        API.LoadScene(Entry.MAIN_MENU_SCENE_NAME);
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