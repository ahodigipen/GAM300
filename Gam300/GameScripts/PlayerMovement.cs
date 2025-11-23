using System;
using Boom;

namespace GameScripts
{
    public class PlayerMovement 
    {
        private float _moveSpeed = 6.0f;
        private float _jumpSpeed = 8.0f;
        private bool _gateRmbToPauseMove = true;

        public ulong Entity;
        private bool _wasSpaceDown = false;

        public void OnStart(string jsonParams)
        {
            // Not using JSON params yet — safe to ignore.
            if (!API.HasTransform(Entity))
                API.Log("[PlayerMovement] Entity has no TransformComponent.");
        }

        public void OnUpdate(float dt)
        {
            if (_gateRmbToPauseMove && API.IsMouseDown(API.MOUSE_RIGHT))
                return;

            float dx = 0f, dz = 0f;
            if (API.IsKeyDown(API.KEY_A)) dx -= 1f;
            if (API.IsKeyDown(API.KEY_D)) dx += 1f;
            if (API.IsKeyDown(API.KEY_W)) dz -= 1f;
            if (API.IsKeyDown(API.KEY_S)) dz += 1f;

            float len = (float)Math.Sqrt(dx * dx + dz * dz);
            if (len > 0f) { dx /= len; dz /= len; }

            var vel = API.GetLinearVelocity(Entity);
            vel.X = dx * _moveSpeed;
            vel.Z = dz * _moveSpeed;

            bool grounded = API.IsColliding(Entity);
            bool spaceDown = API.IsKeyDown(API.KEY_SPACE);

            if (spaceDown && !_wasSpaceDown && grounded)
                vel.Y = _jumpSpeed;

            _wasSpaceDown = spaceDown;

            API.SetLinearVelocity(Entity, vel);

            float planarSpeed = (float)Math.Sqrt(vel.X * vel.X + vel.Z * vel.Z);
            API.AnimatorSetFloat(Entity, "Speed", planarSpeed);
            API.AnimatorSetBool(Entity, "IsMoving", planarSpeed > 0.1f);
        }

        public void OnDestroy() { }
    }
}
