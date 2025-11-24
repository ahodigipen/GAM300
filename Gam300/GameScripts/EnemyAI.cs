using Boom;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using static Boom.API;

namespace GameScripts
{
    public class EnemyMovementAnimator
    {
        private const string IDLE_STATE = "Old Man Idle";
        private const string MOVE_STATE = "Fast Run";

        private const string TRIG_MOVE = "Move";
        private const string TRIG_IDLE = "Idle";

        private const float MOVE_DIST_EPS_SQ = 0.00005f;

        public ulong Entity;

        private bool _initialized = false;
        private string _currentState = null;
        private Vec3 _prevPos;

        public void OnStart(string _)
        {
            if (Entity == 0 || !API.HasTransform(Entity))
                return;

            _prevPos = API.GetPosition(Entity);
            _currentState = IDLE_STATE;
            _initialized = true;
        }

        public void OnUpdate(float dt)
        {
            if (!_initialized || Entity == 0)
                return;

            bool forceIdle = false;
            try
            {
                var mode = API.GetAIMode(Entity);
                if (mode == AIMode.Idle)
                    forceIdle = true;
            }
            catch { }

            Vec3 pos = API.GetPosition(Entity);
            float dx = pos.X - _prevPos.X;
            float dz = pos.Z - _prevPos.Z;
            float distSq = dx * dx + dz * dz;

            bool moving = !forceIdle && (distSq > MOVE_DIST_EPS_SQ);

            // --- Face movement direction if we are moving ---
            if (moving)
            {
                float lenSq = dx * dx + dz * dz;
                if (lenSq > 0.0000001f)
                {
                   
                    // If he faces sideways/backwards, we’ll tweak this line.
                    float yawRad = (float)Math.Atan2(dx, dz); // atan2(x, z)
                    float yawDeg = yawRad * (180.0f / (float)Math.PI);

                    Vec3 rot = API.GetRotation(Entity);
                    rot.Y = yawDeg;
                    API.SetRotation(Entity, rot);
                }
            }

            // --- Animator state switching (same as before) ---
            if (moving && _currentState != MOVE_STATE)
            {
                API.AnimatorSetTrigger(Entity, TRIG_MOVE);
                _currentState = MOVE_STATE;
                API.Log($"[EnemyMovementAnimator] Trigger Move (distSq={distSq:0.000000})");
            }
            else if (!moving && _currentState != IDLE_STATE)
            {
                API.AnimatorSetTrigger(Entity, TRIG_IDLE);
                _currentState = IDLE_STATE;
                API.Log($"[EnemyMovementAnimator] Trigger Idle (distSq={distSq:0.000000})");
            }

            _prevPos = pos;
        }


        public void OnDestroy() { }
    }
}