using System;
using Boom;

namespace GameScripts
{
    public class VolumeSlider
    {
        private ulong _bgID;
        private ulong _fillID;
        private ulong _handleID;
        private string _audioGroup;

        private bool _isDragging = false;
        public bool IsDragging => _isDragging;

        private float _currentValue = 1.0f;
        private float _lastMouseX;

        // --- Configuration ---
        private const float DRAG_SENSITIVITY = 0.004f;
        private const float MIN_ENGINE_SCALE = 0.0001f;

        // Stacking order (Z-axis)
        private const float Z_BG = 0.00f;       // Bottom
        private const float Z_FILL = 0.01f;     // Middle
        private const float Z_HANDLE = 0.02f;   // Top

        // Geometry Data
        private float _leftAnchorX;
        private float _rightAnchorX;
        private float _fixedY;
        private float _fixedZ;
        private float _totalWorldWidth;

        // Auto-Calibration
        private float _fillVisualMultiplier;

        public VolumeSlider(string bgEntityName, string fillEntityName, string handleEntityName, string audioGroup, string emptyEntityName = null)
        {
            _bgID = API.FindEntity(bgEntityName);
            _fillID = API.FindEntity(fillEntityName);
            _handleID = API.FindEntity(handleEntityName);
            _audioGroup = audioGroup;

            // Optional: Deactivate legacy "empty" bar entities
            if (!string.IsNullOrEmpty(emptyEntityName))
            {
                ulong extraID = API.FindEntity(emptyEntityName);
                if (extraID != 0)
                {
                    TransformData t = API.GetTransform(extraID);
                    t.ScaleX = MIN_ENGINE_SCALE;
                    t.PositionY = -9999f;
                    API.SetTransform(extraID, t);
                }
            }

            if (_bgID == 0 || _fillID == 0 || _handleID == 0)
            {
                API.Log($"[Slider] ERROR: Entities not found for {audioGroup}");
                return;
            }

            // --- Calibration ---
            // Logic assumes Handle is positioned at the maximum right (100%) in the Editor
            TransformData initBg = API.GetTransform(_bgID);
            TransformData initFill = API.GetTransform(_fillID);
            TransformData initHandle = API.GetTransform(_handleID);

            _fixedY = initBg.PositionY;
            _fixedZ = initBg.PositionZ;

            // Calculate horizontal boundaries
            float centerPos = initBg.PositionX;
            float rightPos = initHandle.PositionX;
            float halfWidth = Math.Abs(rightPos - centerPos);

            _leftAnchorX = centerPos - halfWidth;
            _rightAnchorX = centerPos + halfWidth;
            _totalWorldWidth = _rightAnchorX - _leftAnchorX;

            if (_totalWorldWidth < 0.001f) _totalWorldWidth = 1.0f;

            // Calibrate Fill Bar scaling multiplier based on Editor setup
            float rawFillScale = Math.Max(MIN_ENGINE_SCALE, Math.Abs(initFill.ScaleX));
            _fillVisualMultiplier = _totalWorldWidth / rawFillScale;

            // --- Z-Order Stacking ---
            initBg.PositionZ = _fixedZ + Z_BG;
            API.SetTransform(_bgID, initBg);

            _currentValue = API.GetGroupVolume(_audioGroup);
            API.Log($"[Slider] Initialized for {_audioGroup}. Width: {_totalWorldWidth:0.00}");

            UpdateVisuals(_currentValue);
        }

        public void SetValue(float value)
        {
            _currentValue = Clamp(value, 0f, 1f);
            API.SetGroupVolume(_audioGroup, _currentValue);
            UpdateVisuals(_currentValue);
        }

        public void Update()
        {
            if (!API.GetMousePosInViewport(out Vec2 mouseScreenPos)) return;

            // Handle Mouse Up
            if (!API.IsMouseDown(0))
            {
                if (_isDragging)
                {
                    SettingsManager.SaveSettings();
                    _isDragging = false;
                }
                return;
            }

            // Handle Click/Drag Detection
            if (!_isDragging)
            {
                if (API.Check2DViewportClick(_bgID, mouseScreenPos.X, mouseScreenPos.Y) ||
                    API.Check2DViewportClick(_fillID, mouseScreenPos.X, mouseScreenPos.Y) ||
                    API.Check2DViewportClick(_handleID, mouseScreenPos.X, mouseScreenPos.Y))
                {
                    _isDragging = true;
                    _lastMouseX = mouseScreenPos.X;
                }
            }
            else
            {
                // Handle Drag Movement
                float deltaX = mouseScreenPos.X - _lastMouseX;
                if (Math.Abs(deltaX) > 0.0001f)
                {
                    _currentValue += deltaX * DRAG_SENSITIVITY;
                    _currentValue = Clamp(_currentValue, 0f, 1f);

                    API.SetGroupVolume(_audioGroup, _currentValue);
                    API.Log($"[Slider] {_audioGroup} Volume: {_currentValue:0.00}");

                    UpdateVisuals(_currentValue);
                    _lastMouseX = mouseScreenPos.X;
                }
            }
        }

        private void UpdateVisuals(float t)
        {
            // 1. Position Handle
            float handleX = _leftAnchorX + (_totalWorldWidth * t);

            TransformData handleTrans = API.GetTransform(_handleID);
            handleTrans.PositionX = handleX;
            handleTrans.PositionY = _fixedY;
            handleTrans.PositionZ = _fixedZ + Z_HANDLE;
            API.SetTransform(_handleID, handleTrans);

            // 2. Scale and Position Fill Bar
            float currentFillWidth = handleX - _leftAnchorX;
            float newScale = Math.Max(MIN_ENGINE_SCALE, currentFillWidth / _fillVisualMultiplier);

            TransformData fillTrans = API.GetTransform(_fillID);
            fillTrans.ScaleX = newScale;

            // Adjust X position to account for center-pivot scaling
            fillTrans.PositionX = _leftAnchorX + (currentFillWidth * 0.5f);
            fillTrans.PositionY = _fixedY;
            fillTrans.PositionZ = _fixedZ + Z_FILL;
            API.SetTransform(_fillID, fillTrans);
        }

        private static float Clamp(float x, float lo, float hi)
        {
            return x < lo ? lo : (x > hi ? hi : x);
        }
    }
}