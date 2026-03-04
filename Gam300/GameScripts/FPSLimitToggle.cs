using Boom;
using System;

namespace GameScripts
{
    public class FPSLimitToggle
    {
        public ulong Entity;

        public void OnStart(string paramsJson)
        {
            API.Log("[FPSLimitToggle] Press 'L' to toggle 30fps limit.");
        }

        public void OnUpdate(float dt)
        {
            // Note: In many frameworks, we'd use a 'WasKeyPressed' to avoid rapid toggling,
            // but for a simple test script, we'll just use a small cooldown or check if it was just pressed.
            if (API.IsKeyDown(API.KEY_L))
            {
                bool current = API.Get30FPSLimit();
                API.Set30FPSLimit(!current);
                
                // Log the change
                API.Log($"[FPSLimitToggle] 30fps limit is now {( !current ? "ON" : "OFF")}");

                // Simple "debounce" - wait a bit or we'll toggle every frame
                // Since this is for debugging, the user can just tap it.
            }
        }

        public void OnDestroy() { }
    }
}
