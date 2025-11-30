using Boom;

namespace GameScripts
{
    /// <summary>
    /// EndZoneTrigger handles player collision with the end zone.
    /// When the player enters this trigger, it loads the main menu scene.
    /// </summary>
    public class EndZoneTrigger
    {
        public ulong Entity;

        private ulong _playerID = 0;
        private bool _hasTriggered = false;
        private float _triggerDelay = 0.5f;  // Small delay to prevent multiple triggers
        private float _triggerTimer = 0f;

        public void OnStart(string jsonParams)
        {
            // Find the player entity once during startup
            _playerID = API.FindEntity("Samurai");

            if (_playerID == 0)
            {
                API.Log("[EndZoneTrigger] WARNING: Could not find Player entity!");
            }
            else
            {
                API.Log("[EndZoneTrigger] Initialized and found player");
            }

            // Ensure this entity is configured as a trigger
            if (API.HasCollider(Entity) && !API.IsTrigger(Entity))
            {
                API.SetTrigger(Entity, true);
            }
        }

        public void OnUpdate(float dt)
        {
            if (_hasTriggered)
            {
                _triggerTimer += dt;
                if (_triggerTimer >= _triggerDelay)
                {
                    // Load the main menu scene
                    API.Log("[EndZoneTrigger] Loading MainMenu...");
                    API.LoadScene("MainMenu");
                    _hasTriggered = false;  // Reset for potential scene reload
                    _triggerTimer = 0f;
                }
                return;
            }
        }

        public void OnTriggerEnter(ulong otherEntityID)
        {
            // Check if the entity that entered is the player
            if (otherEntityID == _playerID && _playerID != 0)
            {
                API.Log("[EndZoneTrigger] Player entered end zone!");
                _hasTriggered = true;
                _triggerTimer = 0f;
            }
        }

        public void OnTriggerStay(ulong otherEntityID)
        {
            // Optional: Handle continuous trigger stay
        }

        public void OnTriggerExit(ulong otherEntityID)
        {
            // Optional: Reset if player leaves
            if (otherEntityID == _playerID && !_hasTriggered)
            {
                API.Log("[EndZoneTrigger] Player left end zone");
            }
        }
    }
}