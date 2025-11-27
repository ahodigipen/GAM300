using System;
using Boom;

namespace GameScripts
{
    public class HealthBarShrinkRightToLeft
    {
        public ulong Entity;

        // --- Controls ---
        private int _triggerKey = API.KEY_M;  // press M
        private float _shrinkTime = 3.0f;       // seconds to shrink to zero
        private bool _rightToLeft = true;       // requested: shrink right → left
        private float _anchorUnits = 3.0f;       // tweak if your quad width units differ
        private bool _forceHideIfUIScaleIgnored = true; // move off-screen at end for GUI

        // --- State ---
        private bool _animRunning, _keyWasDown;
        private float _t;
        private TransformData _start, _goal;

        public void OnStart(string _) { }

        public void OnUpdate(float dt)
        {
            bool down = API.IsKeyDown(_triggerKey);
            if (down && !_keyWasDown && !_animRunning)
            {
                if (!API.HasTransform(Entity)) return;
                BeginShrink();
            }
            _keyWasDown = down;

            if (_animRunning) Tick(dt);
        }

        private void BeginShrink()
        {
            _start = API.GetTransform(Entity);

            float s0 = _start.ScaleX;
            float s1 = 0f; // disappear

            _goal = _start;
            _goal.Scale = new Vec3(s1, _start.ScaleY, _start.ScaleZ);

            // Anchor math:
            // center shift = ±0.5 * (s0 - s1) * _anchorUnits
            // + for left-edge anchor (empties left→right)
            // - for right-edge anchor (empties right→left)
            float sign = _rightToLeft ? -1f : +1f;
            float dx = 0.5f * (s0 - s1) * _anchorUnits * sign;
            _goal.Position = new Vec3(_start.PositionX + dx, _start.PositionY, _start.PositionZ);

            _t = 0f;
            _animRunning = true;
        }

        private void Tick(float dt)
        {
            _t += dt / Math.Max(_shrinkTime, 1e-4f);
            float a = EaseOutCubic(Clamp01(_t));

            var cur = _start;

            // position X lerp for edge anchoring
            cur.Position = new Vec3(
                Lerp(_start.PositionX, _goal.PositionX, a),
                _start.PositionY,
                _start.PositionZ
            );

            // rotation unchanged
            cur.Rotation = _start.Rotation;

            // scale X only
            cur.Scale = new Vec3(
                Lerp(_start.ScaleX, _goal.ScaleX, a),
                _start.ScaleY,
                _start.ScaleZ
            );

            API.SetTransform(Entity, cur);

            if (_t >= 1f)
            {
                _animRunning = false;

                if (_forceHideIfUIScaleIgnored)
                {
                    // If GUI pass ignores scale, yeet off-screen to guarantee invisible
                    var hide = API.GetTransform(Entity);
                    hide.Position = new Vec3(hide.PositionX, hide.PositionY + 10000f, hide.PositionZ);
                    API.SetTransform(Entity, hide);
                }
            }
        }

        // helpers
        private static float Lerp(float a, float b, float t) => a + (b - a) * t;
        private static float Clamp01(float x) => x < 0 ? 0 : (x > 1 ? 1 : x);
        private static float EaseOutCubic(float t) { t = Clamp01(t) - 1f; return t * t * t + 1f; }
    }
}
