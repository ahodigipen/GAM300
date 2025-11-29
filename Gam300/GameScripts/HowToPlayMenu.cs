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
            API.Log("HowToPlayMenu OnStart Running...");
            _returnButtonID = API.FindEntity("ReturnButton");

            API.Log("Return ID: " + _returnButtonID);

            if (_returnButtonID == 0) API.Log("Warning: Return Button not found!");
        }

        public void OnUpdate(float dt)
        {
            if (API.IsMouseDown(MOUSE_LEFT))
            {
                if (!API.GetMousePosInViewport(out Vec2 mousePos))
                {
                    return; 
                }

                API.Log($"[Debug] Mouse Click at: X={mousePos.X}, Y={mousePos.Y}");
                if (_returnButtonID != 0 && API.ProjectWorldToViewport(API.GetTransform(_returnButtonID).Position, out Vec2 returnPos))
                {
                    API.Log($"[Debug] ReturnButton 2D Pos: X={returnPos.X}, Y={returnPos.Y}");
                }


                if (API.Check2DViewportClick(_returnButtonID, mousePos.X, mousePos.Y))
                {
                    API.Log(">> Return Button Clicked! Returning to Main Menu...");
                    API.LoadScene(Entry.MAIN_MENU_SCENE_NAME);
                }
            }
        }
    }
}