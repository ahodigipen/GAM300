using Boom;
using System;

namespace GameScripts
{
    public class HealthBarShrinkRightToLeft
    {
        public ulong Entity;

        private bool _rightToLeft = true;
        private float _anchorUnits = 2.5f;
        private float _animTime = 0.25f;

        private TransformData _start;
        private float _srcScaleX;
        private float _dstScaleX;

        private float _srcPosX;
        private float _dstPosX;

        private float _currentRatio = 1f;
        private float _t = 1f;



        public void OnStart(string _)
        {
            if (!API.HasTransform(Entity)) return;

            _start = API.GetTransform(Entity);

            _srcScaleX = _dstScaleX = _start.ScaleX;
            _srcPosX = _dstPosX = _start.PositionX;

            _currentRatio = 1f;
        }



        public void OnUpdate(float dt)
        {
            if (!API.HasTransform(Entity)) return;

            float target = HUD.HealthRatio;

            // Detect change in ratio
            if (Math.Abs(target - _currentRatio) > 0.0001f)
            {
                TransformData cur = API.GetTransform(Entity);

                _srcScaleX = cur.ScaleX;
                _srcPosX = cur.PositionX;

                float full = _start.ScaleX;
                float s1 = full * target;     // new target width
                float s0 = cur.ScaleX;        // current width

                // compute anchor shift
                float sign = _rightToLeft ? -1f : +1f;
                float dx = 0.5f * (s0 - s1) * _anchorUnits * sign;

                _dstScaleX = s1;
                _dstPosX = cur.PositionX + dx;

                _t = 0f;
            }

            // Tween
            if (_t < 1f)
            {
                float dur = Math.Max(0.0001f, _animTime);
                _t += dt / dur;
                if (_t > 1f) _t = 1f;

                float ease = EaseOutCubic(_t);

                float newScaleX = Lerp(_srcScaleX, _dstScaleX, ease);
                float newPosX = Lerp(_srcPosX, _dstPosX, ease);

                TransformData updated = API.GetTransform(Entity);
                updated.Scale = new Vec3(newScaleX, updated.ScaleY, updated.ScaleZ);
                updated.Position = new Vec3(newPosX, updated.PositionY, updated.PositionZ);

                API.SetTransform(Entity, updated);

                _currentRatio = newScaleX / _start.ScaleX;
            }
        }



        private static float Lerp(float a, float b, float t) => a + (b - a) * t;
        private static float EaseOutCubic(float t)
        {
            t = Clamp(t, 0f, 1f) - 1f;
            return t * t * t + 1f;
        }

        private static float Clamp(float x, float min, float max)
            => x < min ? min : (x > max ? max : x);
    }
}
