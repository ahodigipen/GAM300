using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// A UI slider that controls the renderer's gamma / tone-map setting.
    /// Behaves identically to VolumeSlider but drives API.SetGamma / API.GetGamma
    /// instead of audio group volume.
    ///
    /// Gamma is stored in the engine as a raw float (default 2.2).
    /// We map [MIN_GAMMA, MAX_GAMMA] linearly onto the slider's [0, 1] visual range.
    /// </summary>
    public class GammaSlider
    {
        // -----------------------------------------------------------
        // Gamma range exposed to the player
        // -----------------------------------------------------------
        private const float MIN_GAMMA = 0.5f;
        private const float MAX_GAMMA = 4.0f;

        // -----------------------------------------------------------
        // Internal slider state (mirrors VolumeSlider layout)
        // -----------------------------------------------------------
        private ulong _bgID;
        private ulong _fillID;
        private ulong _handleID;

        private bool _isDragging = false;
        public bool IsDragging => _isDragging;

        private float _currentNorm = 0.0f;   // normalised [0, 1] position
        private float _lastMouseX;

        // --- Configuration ---
        private const float DRAG_SENSITIVITY = 0.002f;
        private const float MIN_ENGINE_SCALE = 0.0001f;

        // Stacking order (Z-axis)
        private const float Z_BG = 0.00f;
        private const float Z_FILL = 0.01f;
        private const float Z_HANDLE = 0.02f;

        // Geometry data (calibrated once in constructor)
        private float _leftAnchorX;
        private float _rightAnchorX;
        private float _fixedY;
        private float _fixedZ;
        private float _totalWorldWidth;
        private float _fillVisualMultiplier;

        // -----------------------------------------------------------
        // Constructor
        // -----------------------------------------------------------
        public GammaSlider(string bgEntityName, string fillEntityName, string handleEntityName)
        {
            _bgID = API.FindEntity(bgEntityName);
            _fillID = API.FindEntity(fillEntityName);
            _handleID = API.FindEntity(handleEntityName);

            if (_bgID == 0 || _fillID == 0 || _handleID == 0)
            {
                API.Log($"[GammaSlider] ERROR: Entities not found (bg={bgEntityName}, fill={fillEntityName}, handle={handleEntityName})");
                return;
            }

            // --- Calibration (same algorithm as VolumeSlider) ---
            TransformData initBg = API.GetTransform(_bgID);
            TransformData initFill = API.GetTransform(_fillID);
            TransformData initHandle = API.GetTransform(_handleID);

            _fixedY = initBg.PositionY;
            _fixedZ = initBg.PositionZ;

            // The handle must be placed at the far-right (100%) in the editor
            float centerPos = initBg.PositionX;
            float rightPos = initHandle.PositionX;
            float halfWidth = Math.Abs(rightPos - centerPos);

            _leftAnchorX = centerPos - halfWidth;
            _rightAnchorX = centerPos + halfWidth;
            _totalWorldWidth = _rightAnchorX - _leftAnchorX;

            if (_totalWorldWidth < 0.001f) _totalWorldWidth = 1.0f;

            float rawFillScale = Math.Max(MIN_ENGINE_SCALE, Math.Abs(initFill.ScaleX));
            _fillVisualMultiplier = _totalWorldWidth / rawFillScale;

            // Z ordering
            initBg.PositionZ = _fixedZ + Z_BG;
            API.SetTransform(_bgID, initBg);

            // Read current gamma and set slider position accordingly
            _currentNorm = GammaToNorm(API.GetGamma());
            API.Log($"[GammaSlider] Initialised. Current gamma: {API.GetGamma():0.00}, norm: {_currentNorm:0.00}");

            UpdateVisuals(_currentNorm);
        }

        // -----------------------------------------------------------
        // Public API
        // -----------------------------------------------------------

        /// <summary>Set the gamma value directly (gamma in [MIN_GAMMA, MAX_GAMMA]).</summary>
        public void SetValue(float gammaValue)
        {
            _currentNorm = GammaToNorm(Clamp(gammaValue, MIN_GAMMA, MAX_GAMMA));
            API.SetGamma(NormToGamma(_currentNorm));
            UpdateVisuals(_currentNorm);
        }

        /// <summary>Set the slider via a normalised [0, 1] position delta (for gamepad).</summary>
        public void SetNormDelta(float delta)
        {
            _currentNorm = Clamp(_currentNorm + delta, 0f, 1f);
            API.SetGamma(NormToGamma(_currentNorm));
            UpdateVisuals(_currentNorm);
        }

        public void Update()
        {
            if (!API.GetMousePosInViewport(out Vec2 mouseScreenPos)) return;

            // ------- Mouse released -------
            if (!API.IsMouseDown(0))
            {
                if (_isDragging)
                {
                    SettingsManager.SaveSettings();
                    _isDragging = false;
                }
                return;
            }

            // ------- Begin drag -------
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
                // ------- Continue drag -------
                float deltaX = mouseScreenPos.X - _lastMouseX;
                if (Math.Abs(deltaX) > 0.0001f)
                {
                    _currentNorm = Clamp(_currentNorm + deltaX * DRAG_SENSITIVITY, 0f, 1f);
                    float gamma = NormToGamma(_currentNorm);
                    API.SetGamma(gamma);
                    API.Log($"[GammaSlider] Gamma: {gamma:0.00}");
                    UpdateVisuals(_currentNorm);
                    _lastMouseX = mouseScreenPos.X;
                }
            }
        }

        // -----------------------------------------------------------
        // Private helpers
        // -----------------------------------------------------------

        private void UpdateVisuals(float t)
        {
            // 1. Position handle
            float handleX = _leftAnchorX + (_totalWorldWidth * t);

            TransformData handleTrans = API.GetTransform(_handleID);
            handleTrans.PositionX = handleX;
            handleTrans.PositionY = _fixedY;
            handleTrans.PositionZ = _fixedZ + Z_HANDLE;
            API.SetTransform(_handleID, handleTrans);

            // 2. Scale & position fill bar
            float currentFillWidth = handleX - _leftAnchorX;
            float newScale = Math.Max(MIN_ENGINE_SCALE, currentFillWidth / _fillVisualMultiplier);

            TransformData fillTrans = API.GetTransform(_fillID);
            fillTrans.ScaleX = newScale;
            fillTrans.PositionX = _leftAnchorX + (currentFillWidth * 0.5f);
            fillTrans.PositionY = _fixedY;
            fillTrans.PositionZ = _fixedZ + Z_FILL;
            API.SetTransform(_fillID, fillTrans);
        }

        // Linear mapping helpers
        private static float GammaToNorm(float gamma)
            => (Clamp(gamma, MIN_GAMMA, MAX_GAMMA) - MIN_GAMMA) / (MAX_GAMMA - MIN_GAMMA);

        private static float NormToGamma(float norm)
            => Clamp(MIN_GAMMA + norm * (MAX_GAMMA - MIN_GAMMA), MIN_GAMMA, MAX_GAMMA);

        private static float Clamp(float x, float lo, float hi)
            => x < lo ? lo : (x > hi ? hi : x);
    }
}
