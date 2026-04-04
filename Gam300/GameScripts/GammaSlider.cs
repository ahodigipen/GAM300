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

        // --- Configuration ---
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

            // --- Calibration (position-invariant: works regardless of saved slider position) ---
            TransformData initBg     = API.GetTransform(_bgID);
            TransformData initFill   = API.GetTransform(_fillID);
            TransformData initHandle = API.GetTransform(_handleID);

            _fixedY = initBg.PositionY;
            _fixedZ = initBg.PositionZ;

            float centerX    = initBg.PositionX;
            float handleX    = initHandle.PositionX;
            float fillCenterX = initFill.PositionX;

            // The fill center and handle always satisfy:
            //   fillCenter = leftAnchor + (handle - leftAnchor) / 2
            //   => leftAnchor = 2 * fillCenter - handle
            // This holds at ANY saved t, not just t = 1.
            float leftAnchor  = 2f * fillCenterX - handleX;
            float halfWidth   = centerX - leftAnchor;

            // Fallback: if slider was saved at t ≈ 0, handle ≈ fill ≈ leftAnchor
            // so the formula degenerates. Use BG scale as an approximation instead.
            if (halfWidth < 0.001f)
                halfWidth = Math.Abs(initBg.ScaleX);

            _leftAnchorX     = centerX - halfWidth;
            _rightAnchorX    = centerX + halfWidth;
            _totalWorldWidth = _rightAnchorX - _leftAnchorX;

            if (_totalWorldWidth < 0.001f) _totalWorldWidth = 1.0f;

            // Derive fillVisualMultiplier from the current saved t.
            // At saved t: fill_ScaleX = (totalWidth * t) / fillVisualMultiplier
            //             t           = (handleX - leftAnchor) / totalWidth
            float savedT        = (handleX - _leftAnchorX) / _totalWorldWidth;
            float rawFillScale  = Math.Max(MIN_ENGINE_SCALE, Math.Abs(initFill.ScaleX));
            if (savedT > 0.001f)
                _fillVisualMultiplier = (_totalWorldWidth * savedT) / rawFillScale;
            else
                // t ≈ 0 fallback: totalWidth / bgScaleX ≈ 2.0 by design convention
                _fillVisualMultiplier = _totalWorldWidth / Math.Max(MIN_ENGINE_SCALE, Math.Abs(initBg.ScaleX));

            // Fix BG Z ordering
            initBg.PositionZ = _fixedZ + Z_BG;
            API.SetTransform(_bgID, initBg);

            // Read current gamma and drive the visuals to the correct position.
            // This overwrites wherever the handle/fill were saved in the editor.
            _currentNorm = GammaToNorm(API.GetGamma());
            API.Log($"[GammaSlider] Calibrated. leftAnchor={_leftAnchorX:0.000} rightAnchor={_rightAnchorX:0.000} fillMul={_fillVisualMultiplier:0.000} norm={_currentNorm:0.000}");

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
                }
            }

            if (_isDragging)
            {
                _currentNorm = NdcToNorm(mouseScreenPos.X);
                API.SetGamma(NormToGamma(_currentNorm));
                UpdateVisuals(_currentNorm);
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

        // Maps a screen-pixel X (from GetMousePosInViewport) to a [0,1] slider norm.
        // 2D entity world X is NDC (-1..+1); screen X = (worldX + 1) * 0.5 * viewportWidth.
        private float NdcToNorm(float screenX)
        {
            API.GetViewportSize(out float vW, out float _);
            float leftScreenX  = (_leftAnchorX  + 1f) * 0.5f * vW;
            float rightScreenX = (_rightAnchorX + 1f) * 0.5f * vW;
            float range = rightScreenX - leftScreenX;
            if (Math.Abs(range) < 0.0001f) return _currentNorm;
            return Clamp((screenX - leftScreenX) / range, 0f, 1f);
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
