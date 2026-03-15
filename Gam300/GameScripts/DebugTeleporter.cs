using Boom;
using System;

namespace GameScripts
{
    /// <summary>
    /// Specialized debug script for quick level navigation.
    /// Attach this to a separate entity (e.g., "DebugManager") to avoid 
    /// scrolling through PlayerMovement settings in the inspector.
    /// </summary>
    public class DebugTeleporter
    {
        public ulong Entity;

#pragma warning disable CS0414
        [Boom.EditorExposed("--- LEVEL NAVIGATION ---", "Visual separator", 0, 0, false)]
        private bool _header = false;
#pragma warning restore CS0414

        [Boom.EditorExposed("GO: Level Start", "Shortcut: F1", 0, 0, false)]
        private bool _teleportToStart = false;

        [Boom.EditorExposed("GO: Last Checkpoint", "Shortcut: F2", 0, 0, false)]
        private bool _teleportToCP = false;

        [Boom.EditorExposed("GO: Level 2 Area", "Shortcut: F3", 0, 0, false)]
        private bool _teleportToLevel2 = false;

        [Boom.EditorExposed("GO: Custom Position", "Shortcut: F4", 0, 0, false)]
        private bool _teleportToCustom = false;

        public void OnUpdate(float dt)
        {
            // Only handle teleports if the application is not paused
            if (API.GetApplicationState() == API.APP_STATE_PAUSED) return;

            PlayerMovement player = PlayerManager.GetPlayer();
            if (player == null) return;

            // Trigger on button press in inspector OR keyboard shortcut
            if (_teleportToStart || API.IsKeyDown(API.KEY_F1))
            {
                _teleportToStart = false;
                player.TeleportToStart();
                API.Log("[DebugTeleporter] F1: Teleporting to Start");
            }

            if (_teleportToCP || API.IsKeyDown(API.KEY_F2))
            {
                _teleportToCP = false;
                player.TeleportToLastCheckpoint();
                API.Log("[DebugTeleporter] F2: Teleporting to Checkpoint");
            }

            if (_teleportToLevel2 || API.IsKeyDown(API.KEY_F3))
            {
                _teleportToLevel2 = false;
                player.TeleportToLevel2();
                API.Log("[DebugTeleporter] F3: Teleporting to Level 2");
            }

            if (_teleportToCustom || API.IsKeyDown(API.KEY_F4))
            {
                _teleportToCustom = false;
                player.TeleportTo(new Vec3(-40.770f, 26.960f, -31.0f));
                API.Log("[DebugTeleporter] F4: Teleporting to Custom Position (-40.77, 26.96, -31)");
            }
        }
    }
}