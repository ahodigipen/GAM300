using System;
using Boom;

namespace GameScripts
{
    public class PlayerMovement 
    {
        // Tunables
        private float _speed = 6.0f;      // horizontal speed (m/s)
        private float _jump = 8.0f;      // jump impulse (m/s)

        public ulong Entity { get; set; }

        public void OnStart()
        {
            // Require a transform; don't force a ScriptComponent check on self.
            if (!API.HasTransform(Entity))
            {
                API.Log("[PlayerMovement] Entity has no Transform; disabling.");
            }
        }

        public void OnUpdate(float dt)
        {
            // If RMB is held (common for mouselook), we pause movement.
            // Remove this gate if you don't want it.
            if (API.IsMouseDown(API.MOUSE_RIGHT))
                return;

            // Read current linear velocity (Dynamic body friendly)
            var vel = API.GetLinearVelocity(Entity);

            // Build desired planar direction from WASD
            float dx = 0f, dz = 0f;
            if (API.IsKeyDown(API.KEY_A)) dx -= 1f;
            if (API.IsKeyDown(API.KEY_D)) dx += 1f;
            if (API.IsKeyDown(API.KEY_W)) dz -= 1f;   // -Z forward in your engine
            if (API.IsKeyDown(API.KEY_S)) dz += 1f;

            // Normalize input (so diagonals aren't faster)
            if (dx != 0f || dz != 0f)
            {
                float len = (float)Math.Sqrt(dx * dx + dz * dz);
                dx /= len; dz /= len;
                vel.X = dx * _speed;
                vel.Z = dz * _speed;
            }
            else
            {
                vel.X = 0f;
                vel.Z = 0f;
            }

            // Jump only when grounded
            if (API.IsKeyDown(API.KEY_SPACE))   // or IsKeyDown if you prefer
            {
                // If you have a better ground check, use it here.
                if (API.IsColliding(Entity))       // simple “touching something” ground check
                {
                    vel.Y = _jump;                 // upward impulse; let gravity handle the rest
                }
            }

            API.SetLinearVelocity(Entity, vel);

            // (Optional) Drive animator parameters if present
            // This won't throw if AnimatorComponent doesn't exist — native side should no-op.
            float speed = (float)Math.Sqrt(vel.X * vel.X + vel.Z * vel.Z);
            API.AnimatorSetFloat(Entity, "Speed", speed);
            API.AnimatorSetBool(Entity, "IsMoving", speed > 0.1f);
        }
    }
}
