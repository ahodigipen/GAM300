using Boom;

namespace GameScripts
{
    /// <summary>
    /// Singleton UI manager that coordinates all UI controllers
    /// Attach this to the PlayerUI empty game object
    /// Other scripts can call UIManager.ShowKeyPickup(), etc.
    /// </summary>
    public class UIManager
    {
        public ulong Entity;

        private static UIManager s_instance = null;

        private UIHoldController _holdUI;
        private UIEndController _endUI;
        private UILocationController _locationUI;
        private UIHeartController _heartUI;  // *** NEW: Heart UI controller ***
        private BloodOverlayController _bloodUI; // *** NEW: Blood overlay controller ***
        private UITutorialController _tutorialUI; // *** NEW: Tutorial UI controller ***
        private UIStanceController _stanceUI; // *** NEW: Crouch/Run UI controller ***
        private WaypointIndicator _waypointUI; // Waypoint arrow indicator for keys
        private UILetterboxController _letterboxUI; // Cinematic letterbox bars

        [EditorExposed("Letterbox Status", "Current status of the letterbox system")]
        private string _lbStatus = "Initializing...";

        // Global state to persist across scene loads / instance changes
        private static bool s_lbActiveGlobal = false;
        private string _topBarName = "UI_LetterboxTop";

        [EditorExposed("Bottom Bar Name", "Name of the bottom letterbox entity")]
        private string _bottomBarName = "UI_LetterboxBottom";

        [EditorExposed("Capture Positions", "Toggle to capture current Y positions as 'Hidden' values")]
        private bool _lbCapture = false;

        [EditorExposed("Top Hidden Y", "Initial Y position when hidden (Top Bar)")]
        private float _topHiddenY = 1.165f;

        [EditorExposed("Bottom Hidden Y", "Initial Y position when hidden (Bottom Bar)")]
        private float _bottomHiddenY = -1.165f;

        [EditorExposed("Letterbox Speed", "How fast the bars slide in/out")]
        private float _lbSpeed = 5.0f;
        
        [EditorExposed("Letterbox Squeeze", "How many units the bars slide INTO the screen (distance)")]
        private float _lbSqueeze = 0.32f;

        [EditorExposed("Letterbox Test", "Toggle to manually test the slide")]
        private bool _lbTest = false;

        public void OnStart(string jsonParams)
        {
            // Singleton management: Newer instances always take over 
            // to ensure the UI manager of the CURRENT scene is the one in control.
            if (s_instance != null && s_instance != this)
            {
                API.Log($"[UIManager] ID:{Entity} taking over from old Master ID:{s_instance.Entity}");
            }
            s_instance = this;

            // Initialize all UI controllers
            _holdUI = new UIHoldController { Entity = Entity };
            _holdUI.OnStart(jsonParams);

            _endUI = new UIEndController { Entity = Entity };
            _endUI.OnStart(jsonParams);

            _locationUI = new UILocationController { Entity = Entity };
            _locationUI.OnStart(jsonParams);

            // *** NEW: Initialize blood UI ***
            _bloodUI = new BloodOverlayController { Entity = Entity };
            _bloodUI.OnStart(jsonParams);

            // *** NEW: Initialize heart UI ***
            _heartUI = new UIHeartController { Entity = Entity };
            _heartUI.OnStart(jsonParams);

            // *** NEW: Initialize stance UI (crouch/run) ***
            _stanceUI = new UIStanceController { Entity = Entity };
            _stanceUI.OnStart(jsonParams);

            // *** NEW: Initialize tutorial UI ***
            _tutorialUI = new UITutorialController { Entity = Entity };
            _tutorialUI.OnStart(jsonParams);

            // Initialize waypoint indicator for key navigation
            _waypointUI = new WaypointIndicator { Entity = Entity };
            _waypointUI.OnStart(jsonParams);

            // Initialize letterbox
            _letterboxUI = new UILetterboxController { Entity = Entity };
            _letterboxUI.ParentManager = this;
            _letterboxUI.OnStart(jsonParams);

            API.Log("[UIManager] All UI systems initialized (including hearts, tutorials, and waypoints)");

        }

        public static void Update(float dt)
        {
            s_instance?.UpdateInstance(dt);
        }

        public void OnUpdate(float dt)
        {
            // Note: Manual update handled by Entry.cs hook to ensure UI updates during pauses/cutscenes
        }

        private void UpdateInstance(float dt)
        {
            // Update all UI controllers
            _holdUI?.OnUpdate(dt);
            _endUI?.OnUpdate(dt);
            _locationUI?.OnUpdate(dt);
            _heartUI?.OnUpdate(dt);  // *** NEW: Update heart UI ***
            _bloodUI?.OnUpdate(dt);  // *** NEW: Update blood UI ***
            _stanceUI?.OnUpdate(dt); // *** NEW: Update crouch/run UI ***
            _tutorialUI?.OnUpdate(dt);  // *** NEW: Update tutorial UI ***
            _waypointUI?.OnUpdate(dt);  // Waypoint arrow indicator
            
            // Sync settings to letterbox controller
            if (_letterboxUI != null)
            {
                _letterboxUI.TopBarName = _topBarName;
                _letterboxUI.BottomBarName = _bottomBarName;
                _letterboxUI.SlideSpeed = _lbSpeed;
                _letterboxUI.SqueezeAmount = _lbSqueeze;
                _letterboxUI.ManualTest = _lbTest;
                _letterboxUI.TopHiddenY = _topHiddenY;
                _letterboxUI.BottomHiddenY = _bottomHiddenY;
                
                // Force sync with global state
                _letterboxUI.SetShowState(s_lbActiveGlobal);

                if (_lbCapture)
                {
                    _letterboxUI.CapturePositions();
                    _topHiddenY = _letterboxUI.TopHiddenY;
                    _bottomHiddenY = _letterboxUI.BottomHiddenY;
                    _lbCapture = false;
                }

                _letterboxUI.OnUpdate(dt);
                _lbStatus = _letterboxUI.Status;
            }
        }


        public void OnDestroy()
        {
            if (s_instance == this)
            {
                s_instance = null;
                API.Log("[UIManager] Singleton unregistered");
            }
        }

        // ===== Static Public API =====



        /// <summary>
        /// Show the "Hold to Crouch" prompt (backward compatible)
        /// </summary>
        public static void ShowHoldPrompt()
        {
            if (s_instance != null)
            {
                s_instance._holdUI?.Show();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot show hold prompt");
            }
        }

        /// <summary>
        /// Hide the "Hold to Crouch" prompt (backward compatible)
        /// </summary>
        public static void HideHoldPrompt()
        {
            if (s_instance != null)
            {
                s_instance._holdUI?.Hide();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot hide hold prompt");
            }
        }

        /// <summary>
        /// Trigger the end game sequence
        /// </summary>
        public static void TriggerEndGame()
        {
            if (s_instance != null)
            {
                s_instance._endUI?.TriggerEnd();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot trigger end game");
            }
        }

        /// <summary>
        /// Show the Garden location indicator
        /// </summary>
        public static void ShowGardenLocation()
        {
            if (s_instance != null)
            {
                s_instance._locationUI?.ShowGarden();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot show garden location");
            }
        }

        /// <summary>
        /// Show the Beginning location indicator
        /// </summary>
        public static void ShowBeginningLocation()
        {
            if (s_instance != null)
            {
                s_instance._locationUI?.ShowBeginning();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot show beginning location");
            }
        }

        /// <summary>
        /// Check if UIManager is initialized
        /// </summary>
        public static bool IsInitialized()
        {
            return s_instance != null;
        }

        public static bool IsMaster(UIManager instance)
        {
            return s_instance == instance;
        }

        /// <summary>
        /// Show a tutorial popup sprite
        /// </summary>
        public static void ShowTutorialPopup(ulong spriteEntity)
        {
            if (s_instance != null)
            {
                s_instance._tutorialUI?.Show(spriteEntity);
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot show tutorial popup");
            }
        }

        /// <summary>
        /// Hide the tutorial popup sprite
        /// </summary>
        public static void HideTutorialPopup()
        {
            if (s_instance != null)
            {
                s_instance._tutorialUI?.Hide();
            }
            else
            {
                API.Log("[UIManager] No instance registered - cannot hide tutorial popup");
            }
        }

        /// <summary>
        /// Show cinematic letterbox bars
        /// </summary>
        public static void ShowLetterbox()
        {
            s_lbActiveGlobal = true;
            if (s_instance != null)
            {
                API.Log($"[UIManager] ShowLetterbox (Global=ON) -> Master:{s_instance.Entity}");
            }
        }

        /// <summary>
        /// Hide cinematic letterbox bars
        /// </summary>
        public static void HideLetterbox()
        {
            s_lbActiveGlobal = false;
            if (s_instance != null)
            {
                API.Log($"[UIManager] HideLetterbox (Global=OFF) -> Master:{s_instance.Entity}");
            }
        }
    }
}


