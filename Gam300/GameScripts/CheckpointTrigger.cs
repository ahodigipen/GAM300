using Boom;
using System;

namespace GameScripts
{
    public class CheckpointTrigger
    {
        public ulong Entity;
        public string TargetCutsceneEntity = "CutsceneManager"; 
        
        // You can set this in the editor if supported, or hardcode/find it
        private bool _triggered = false;

        public void OnStart(string params)
        {
            // Optional: Register trigger callback if using physics triggers
            if (API.HasCollider(Entity))
            {
               API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
            }
        }

        public void OnTriggerEnter(ulong trigger, ulong other)
        {
            if (_triggered) return;

            // Check if it's the player
            // ulong playerID = API.FindEntity("Player");
            // if (other != playerID) return;

            API.Log("Checkpoint Reached! Triggering Cutscene...");
            
            // Call the static method we will add to CutsceneSequencer
            CutsceneSequencer.PlayCutscene(TargetCutsceneEntity);
            
            _triggered = true;
        }

        public void OnUpdate(float dt) 
        { 
           // Alternatively, check distance manually if no physics
        }
    }
}
