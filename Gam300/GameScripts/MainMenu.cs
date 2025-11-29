using System;
using Boom;

namespace GameScripts
{
    public class MainMenu
    {
        private const int MOUSE_LEFT = 0;
        private const string LEVEL_SCENE_NAME = "M2_Redesign_scaled";

        // Click sound
        private const string BUTTON_CLICK_SOUND_ID = "ui_button_start";
        private const string BUTTON_CLICK_SOUND_PATH = "Resources/Audio/buttonPressed.wav";

        // Hover sound
        private const string BUTTON_HOVER_SOUND_ID = "ui_button_hover";
        private const string BUTTON_HOVER_SOUND_PATH = "Resources/Audio/buttonHover.wav";

        // Store the unique IDs of our buttons
        private ulong _startButtonID;
        private ulong _quitButtonID;

        // Hover state to avoid spamming sound every frame
        private bool _wasHoveringStart = false;
        private bool _wasHoveringQuit = false;

        public void OnStart(string jsonParams)
        {
            API.Log("MainMenu OnStart Running...");

            // Preload sounds
            API.PreloadSound(BUTTON_CLICK_SOUND_ID, BUTTON_CLICK_SOUND_PATH);
            API.PreloadSound(BUTTON_HOVER_SOUND_ID, BUTTON_HOVER_SOUND_PATH);

            // Find the entities by name once at startup
            _startButtonID = API.FindEntity("StartButton");
            _quitButtonID = API.FindEntity("QuitButton");

            API.Log("Start ID: " + _startButtonID);
            API.Log("Quit ID: " + _quitButtonID);

            if (_startButtonID == 0) API.Log("Warning: StartButton not found!");
            if (_quitButtonID == 0) API.Log("Warning: QuitButton not found!");
        }

        public void OnUpdate(float dt)
        {
            // Raycast under mouse every frame for hover + click
            ulong hoverID = API.PickGameEntity();
            // API.Log(">> Hover Raycast returned ID: " + hoverID); // uncomment if you want spammy debug

            bool isHoveringStart = (hoverID == _startButtonID);
            bool isHoveringQuit = (hoverID == _quitButtonID);

            // --- HOVER SOUND LOGIC ---

            // Start button hover enter
            if (isHoveringStart && !_wasHoveringStart)
            {
                API.Log(">> Hovered Start Button - playing hover sound");
                Vec3 pos = new Vec3(0f, 0f, 0f);
                if (API.HasTransform(_startButtonID))
                    pos = API.GetPosition(_startButtonID);

                API.PlaySoundAt(BUTTON_HOVER_SOUND_ID, BUTTON_HOVER_SOUND_PATH, pos, false);
                API.SetSoundVolume(BUTTON_HOVER_SOUND_ID, 0.5f);
            }

            // Quit button hover enter (use same hover sound)
            if (isHoveringQuit && !_wasHoveringQuit)
            {
                API.Log(">> Hovered Quit Button - playing hover sound");
                Vec3 pos = new Vec3(0f, 0f, 0f);
                if (API.HasTransform(_quitButtonID))
                    pos = API.GetPosition(_quitButtonID);

                API.PlaySoundAt(BUTTON_HOVER_SOUND_ID, BUTTON_HOVER_SOUND_PATH, pos, false);
                API.SetSoundVolume(BUTTON_HOVER_SOUND_ID, 0.85f);
            }

            // Update hover state
            _wasHoveringStart = isHoveringStart;
            _wasHoveringQuit = isHoveringQuit;

            // --- CLICK LOGIC ---

            if (API.IsMouseDown(MOUSE_LEFT))
            {
                API.Log(">> Mouse Click Detected!");

                // Reuse hoverID as what we clicked on
                ulong hitID = hoverID;
                API.Log(">> Click Raycast returned ID: " + hitID);

                if (hitID != 0)
                {
                    API.Log("Hit ID: " + hitID);
                    API.Log("Wanted Start ID: " + _startButtonID);

                    if (hitID == _startButtonID)
                    {
                        API.Log(">> Start Button Clicked! Playing sound + Starting Game...");

                        Vec3 pos = new Vec3(0f, 0f, 0f);
                        if (API.HasTransform(hitID))
                            pos = API.GetPosition(hitID);

                        API.PlaySoundAt(BUTTON_CLICK_SOUND_ID, BUTTON_CLICK_SOUND_PATH, pos, false);
                        API.SetSoundVolume(BUTTON_CLICK_SOUND_ID, 1.0f);

                        API.LoadScene(LEVEL_SCENE_NAME);
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
