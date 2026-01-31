using Boom;
using System;
using System.Collections.Generic;

namespace GameScripts
{
    public class IntroCutscene
    {
        public ulong Entity; 

        private CutsceneSequencer _sequencer;
        private bool _initialized = false;

        public void OnStart(string jsonParams)
        {
            API.Log("[IntroCutscene] OnStart CALL RECEIVED.");
            API.Log($"[IntroCutscene] Attached to Entity ID: {Entity}");

            // Create/Get Sequencer
            // Since we can't easily AttachComponent at runtime (maybe we can?), 
            // we will create a standalone instance logic-wise, but we need it to update.
            // Best approach: IntroCutscene IS A WRAPPER that runs the sequencer logic.
            
            _sequencer = new CutsceneSequencer();
            _sequencer.Entity = Entity; // Use our entity ID for logging/registration
            
            // Build the Sequence Programmatically
            BuildSequence();
            
            // Start Playing
            _sequencer.Play();
            _initialized = true;
        }
        
        private void BuildSequence()
        {
            _sequencer.ClearTracks();
            _sequencer.BlockInput = true;
            _sequencer.Loop = false;
            
            ulong cameraID = API.FindEntity("Camera");
            // Assuming Camera is what moves. 
            // Track 1: Camera Position (Type 0)
            // Track 2: Camera LookAt (Type 4)
            
            CutsceneSequencer.Track posTrack = new CutsceneSequencer.Track();
            posTrack.label = "CameraPos";
            posTrack.targetEntityName = "Camera";
            posTrack.cachedEntityID = cameraID;
            posTrack.type = 0; // Position
            
            API.Log($"[IntroCutscene] PosTrack Target: '{posTrack.targetEntityName}' ID: {posTrack.cachedEntityID}");
            
            CutsceneSequencer.Track lookTrack = new CutsceneSequencer.Track();
            lookTrack.label = "CameraLook";
            lookTrack.targetEntityName = "Camera";
            lookTrack.cachedEntityID = cameraID;
            lookTrack.type = 4; // LookAt
            
            API.Log($"[IntroCutscene] LookTrack Target: '{lookTrack.targetEntityName}' ID: {lookTrack.cachedEntityID}");
            
            // Define Keyframes
            int fps = 60;
            int frame = 0;
            
            // 1. Enemy 3
            ulong enemy3ID = API.FindEntity("patrol enemy 3");
            if (enemy3ID != 0) {
                 API.Log("[IntroCutscene] Found 'patrol enemy 3'. Adding keys.");
                 Vec3 ePos = API.GetPosition(enemy3ID);
                 // Camera Position relative to enemy
                 Vec3 camPos = new Vec3(ePos.X, ePos.Y + 5.0f, ePos.Z + 8.0f);
                 
                 // Move
                 AddPosKey(posTrack, frame, camPos); // Start at this pos
                 // Look
                 AddLookKey(lookTrack, frame, "patrol enemy 3");
                 
                 // Wait 5 seconds
                 frame += 5 * fps;
                 AddPosKey(posTrack, frame, camPos); // Hold pos
                 AddLookKey(lookTrack, frame, "patrol enemy 3"); // Hold look
            }
            
            // 2. Sentry 2
            ulong sentry2ID = API.FindEntity("Sentry_2");
            if (sentry2ID != 0) {
                 Vec3 ePos = API.GetPosition(sentry2ID);
                 Vec3 camPos = new Vec3(ePos.X, ePos.Y + 5.0f, ePos.Z + 8.0f);
                 
                 // Move (Pan takes 2 seconds?)
                 int panFrames = 2 * fps;
                 frame += panFrames;
                 
                 AddPosKey(posTrack, frame, camPos);
                 AddLookKey(lookTrack, frame, "Sentry_2");
                 
                 // Wait 5 seconds
                 frame += 5 * fps;
                 AddPosKey(posTrack, frame, camPos);
                 AddLookKey(lookTrack, frame, "Sentry_2");
            }

            // 3. Key
            ulong keyID = API.FindEntity("Key");
            if (keyID != 0) {
                 Vec3 ePos = API.GetPosition(keyID);
                 Vec3 camPos = new Vec3(ePos.X, ePos.Y + 3.0f, ePos.Z + 5.0f); // Closer for key
                 
                 int panFrames = 2 * fps;
                 frame += panFrames;
                 
                 AddPosKey(posTrack, frame, camPos);
                 AddLookKey(lookTrack, frame, "Key");
                 
                 // Wait 5 seconds
                 frame += 5 * fps;
                 AddPosKey(posTrack, frame, camPos);
                 AddLookKey(lookTrack, frame, "Key");
            }
            
            // 4. Back to Player
            ulong playerID = API.FindEntity("Player");
             if (playerID == 0) playerID = PlayerMovement.GetPlayerEntity();
            
            if (playerID != 0) {
                 // Restore original camera offset? Or just a nice view?
                 // Let's use a standard offset
                 Vec3 ePos = API.GetPosition(playerID);
                 Vec3 camPos = new Vec3(ePos.X, ePos.Y + 4.0f, ePos.Z + 8.0f);
                 
                 int panFrames = 2 * fps;
                 frame += panFrames;
                 
                 AddPosKey(posTrack, frame, camPos);
                 AddLookKey(lookTrack, frame, "Player");
            }
            
            _sequencer.AddTrack(posTrack);
            _sequencer.AddTrack(lookTrack);
            
            API.Log($"[IntroCutscene] Built sequence. Duration: {frame} frames. Tracks Added: Pos & Look.");
        }
        
        private void AddPosKey(CutsceneSequencer.Track t, int frame, Vec3 pos)
        {
            CutsceneSequencer.KeyFrame kf = new CutsceneSequencer.KeyFrame();
            kf.frame = frame;
            kf.time = frame / 60.0f;
            kf.vX = pos.X;
            kf.vY = pos.Y;
            kf.vZ = pos.Z;
            t.keyframes.Add(kf);
        }

        private void AddLookKey(CutsceneSequencer.Track t, int frame, string targetName)
        {
            CutsceneSequencer.KeyFrame kf = new CutsceneSequencer.KeyFrame();
            kf.frame = frame;
            kf.time = frame / 60.0f;
            kf.valStr = targetName;
            t.keyframes.Add(kf);
        }

        public void OnUpdate(float dt)
        {
            // API.Log("[IntroCutscene] OnUpdate tick..."); 
            if (_initialized && _sequencer != null)
            {
                _sequencer.OnUpdate(dt);
            }
            else {
                 if (!_initialized) API.Log("[IntroCutscene] WARNING: Update called but NOT Initialized!");
            }
        }
        
        public void OnDestroy()
        {
            if (_sequencer != null) _sequencer.OnDestroy();
        }
    }
}
