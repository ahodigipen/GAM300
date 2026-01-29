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
        private const float DRAG_SENSITIVITY = 0.002f;
        private const float MIN_ENGINE_SCALE = 0.0001f;
        private const float Z_PLANE_BARS = 0.00f;
        private const float Z_PLANE_HANDLE = 0.02f;

        // --- Geometry & Calibration ---
        private float _visualMultiplier;
        private float _leftAnchorX;
        private float _rightAnchorX;
        private float _fixedY;
        private float _fixedZ;
        private float _totalWorldWidth;

        public VolumeSlider(string bgEntityName, string fillEntityName, string handleEntityName, string audioGroup, string emptyEntityName = null)
        {
            _bgID = API.FindEntity(bgEntityName);
            _fillID = API.FindEntity(fillEntityName);
            _handleID = API.FindEntity(handleEntityName);
            _audioGroup = audioGroup;

            // Handle optional placeholder entity
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

            // --- Auto-Calibration ---
            TransformData initBg = API.GetTransform(_bgID);
            TransformData initHandle = API.GetTransform(_handleID);

            float distBgToHandle = Math.Abs(initBg.PositionX - initHandle.PositionX);
            float currentBgVisualWidth = distBgToHandle * 2.0f;

            float bgScale = Math.Abs(initBg.ScaleX);
            if (bgScale < 0.001f) bgScale = 1.0f;

            _visualMultiplier = currentBgVisualWidth / bgScale;

            // Calculate anchor points
            _rightAnchorX = initBg.PositionX + (currentBgVisualWidth * 0.5f);

            TransformData initFill = API.GetTransform(_fillID);
            float distFillToHandle = Math.Abs(initHandle.PositionX - initFill.PositionX);
            float currentFillWidth = distFillToHandle * 2.0f;

            _leftAnchorX = initFill.PositionX - (currentFillWidth * 0.5f);
            _totalWorldWidth = _rightAnchorX - _leftAnchorX;

            if (_totalWorldWidth < 0.001f) _totalWorldWidth = 1.0f;

            _fixedY = initBg.PositionY;
            _fixedZ = initBg.PositionZ;

            _currentValue = API.GetGroupVolume(_audioGroup);
            UpdateVisuals(_currentValue);
        }

        public void Update()
        {
            if (!API.GetMousePosInViewport(out Vec2 mouseScreenPos)) return;

            // Handle Mouse Up / End Drag
            if (!API.IsMouseDown(0))
            {
                if (_isDragging)
                {
                    SettingsManager.SaveSettings();
                    _isDragging = false;
                }
                return;
            }

            // Handle Input Detection
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
                // Process Dragging
                float deltaX = mouseScreenPos.X - _lastMouseX;
                if (Math.Abs(deltaX) > 0.0001f)
                {
                    _currentValue += deltaX * DRAG_SENSITIVITY;
                    _currentValue = Clamp(_currentValue, 0f, 1f);

                    API.SetGroupVolume(_audioGroup, _currentValue);
                    UpdateVisuals(_currentValue);

                    API.Log($"[Slider] {_audioGroup} volume set to: {_currentValue:P0}");

                    _lastMouseX = mouseScreenPos.X;
                }
            }
        }

        private void UpdateVisuals(float t)
        {
            float safeMultiplier = Math.Max(0.0001f, _visualMultiplier);

            // 1. Update Handle Position
            float handleX = _leftAnchorX + (_totalWorldWidth * t);
            TransformData handleTrans = API.GetTransform(_handleID);
            handleTrans.PositionX = handleX;
            handleTrans.PositionY = _fixedY;
            handleTrans.PositionZ = _fixedZ + Z_PLANE_HANDLE;
            API.SetTransform(_handleID, handleTrans);

            // 2. Update Fill Bar (Left side)
            float fillWorldWidth = handleX - _leftAnchorX;
            float fillScale = Math.Max(MIN_ENGINE_SCALE, fillWorldWidth / safeMultiplier);

            TransformData fillTrans = API.GetTransform(_fillID);
            fillTrans.ScaleX = fillScale;
            fillTrans.PositionX = _leftAnchorX + (fillWorldWidth * 0.5f);
            fillTrans.PositionY = _fixedY;
            fillTrans.PositionZ = _fixedZ + Z_PLANE_BARS;
            API.SetTransform(_fillID, fillTrans);

            // 3. Update Background Bar (Right side)
            float bgWorldWidth = _rightAnchorX - handleX;
            float bgScale = Math.Max(MIN_ENGINE_SCALE, bgWorldWidth / safeMultiplier);

            TransformData bgTrans = API.GetTransform(_bgID);
            bgTrans.ScaleX = bgScale;
            bgTrans.PositionX = _rightAnchorX - (bgWorldWidth * 0.5f);
            bgTrans.PositionY = _fixedY;
            bgTrans.PositionZ = _fixedZ + Z_PLANE_BARS;

            API.SetTransform(_bgID, bgTrans);
        }

        private static float Clamp(float x, float lo, float hi)
        {
            return x < lo ? lo : (x > hi ? hi : x);
        }
    }
}