using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Controls the cinematic letterbox effect (black bars)
    /// Slides bars in when a cutscene starts and out when it ends
    /// </summary>
    public class UILetterboxController
    {
        public ulong Entity;

        private ulong _topBar = 0;
        private ulong _bottomBar = 0;

        public float SlideSpeed = 5.0f;
        public float SqueezeAmount = 0.32f;
        public bool ManualTest = false;

        public float TopHiddenY = 1.165f;
        public float BottomHiddenY = -1.165f;

        public string TopBarName = "UI_LetterboxTop";
        public string BottomBarName = "UI_LetterboxBottom";
        public string Status = "Initializing...";

        private bool _shouldShow = false;
        private float _currentTopY = -9999f; // Special value for uninitialized
        private float _currentBottomY = -9999f;

        public void OnStart(string jsonParams)
        {
            // Do NOT initialize current positions here, wait for first Update to sync from Inspector
        }

        public void CapturePositions()
        {
            ulong top = API.FindEntity(TopBarName);
            ulong bot = API.FindEntity(BottomBarName);

            if (top != 0) TopHiddenY = API.GetPosition(top).Y;
            if (bot != 0) BottomHiddenY = API.GetPosition(bot).Y;
            
            Status = "Positions Captured!";
        }

        public void OnUpdate(float dt)
        {
            // Ensure UI animation continues even if game logic dt is 0 (Paused)
            float animDt = (dt <= 0f) ? 0.016f : dt;

            _topBar = API.FindEntity(TopBarName);
            _bottomBar = API.FindEntity(BottomBarName);

            // Sync targets
            bool active = _shouldShow || ManualTest;
            float targetTopY = active ? (TopHiddenY - SqueezeAmount) : TopHiddenY;
            float targetBottomY = active ? (BottomHiddenY + SqueezeAmount) : BottomHiddenY;

            // Handle independent updates for robustness
            if (_topBar != 0 && API.HasTransform(_topBar))
            {
                if (_currentTopY < -9000f) _currentTopY = TopHiddenY;
                _currentTopY = Lerp(_currentTopY, targetTopY, SlideSpeed * animDt);

                Vec3 p = API.GetPosition(_topBar);
                p.Y = _currentTopY;
                API.SetPosition(_topBar, p);
            }

            if (_bottomBar != 0 && API.HasTransform(_bottomBar))
            {
                if (_currentBottomY < -9000f) _currentBottomY = BottomHiddenY;
                _currentBottomY = Lerp(_currentBottomY, targetBottomY, SlideSpeed * animDt);

                Vec3 p = API.GetPosition(_bottomBar);
                p.Y = _currentBottomY;
                API.SetPosition(_bottomBar, p);
            }

            // Detailed status for user
            string mode = active ? (ManualTest ? "TEST" : "ACTIVE") : "OFF";
            
            // Auto-snap if very close
            string snapMsg = "";
            if (active && Math.Abs(_currentTopY - targetTopY) < 0.002f) 
            {
                _currentTopY = targetTopY;
                _currentBottomY = targetBottomY;
                snapMsg = " (LOCKED)";
            }
            else if (!active && Math.Abs(_currentTopY - TopHiddenY) < 0.002f)
            {
                _currentTopY = TopHiddenY;
                _currentBottomY = BottomHiddenY;
                snapMsg = " (HIDDEN)";
            }

            string masterTag = UIManager.IsMaster(this.ParentManager) ? "[MASTER]" : "[SLAVE]";
            if (!UIManager.IsMaster(this.ParentManager))
            {
                Status = $"{masterTag} (Inactive)";
                return;
            }

            Status = $"{masterTag} {mode}{snapMsg} | sh:{_shouldShow} | dt:{dt:F3}";
        }

        // Parent manager reference for Master check
        public UIManager ParentManager;

        public void SetShowState(bool show)
        {
            _shouldShow = show;
        }

        public void Show()
        {
            _shouldShow = true;
        }

        public void Hide()
        {
            _shouldShow = false;
        }

        private float Lerp(float a, float b, float t)
        {
            t = Math.Max(0f, Math.Min(1f, t));
            return a + (b - a) * t;
        }
    }
}
