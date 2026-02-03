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
        private bool _pendingStart = false;
        private int _startDelayFrames = 0;

        public void OnStart(string jsonParams)
        {
            API.Log("[IntroCutscene] OnStart CALL RECEIVED.");
            API.Log($"[IntroCutscene] Attached to Entity ID: {Entity}");
            
            // Force log to console specifically to be sure
            Console.WriteLine("[IntroCutscene] CONSOLE LOG - OnStart Running");

            try {
                 _sequencer = new CutsceneSequencer();
                 _sequencer.Entity = Entity; 
                 // Delay Start to ensure scene is loaded
                 _initialized = true; 
                 _pendingStart = true;
                 _startDelayFrames = 30; // Wait 30 frames (~0.5s at 60fps)

                 API.Log("[IntroCutscene] OnStart: Deferred Play by 30 frames...");
            }
            catch (Exception ex) {
                 API.Log("[IntroCutscene] CRITICAL ERROR in OnStart: " + ex.ToString());
            }
        }
        
        private void BuildSequence()
        {
            _sequencer.ClearTracks();
            _sequencer.BlockInput = true;
            _sequencer.Loop = false;
            
            ulong cameraID = API.FindEntity("Camera");
            if (cameraID == 0) cameraID = API.FindEntity("Main Camera");
            if (cameraID == 0) cameraID = API.FindEntity("MainCamera");
            
            if (cameraID == 0) {
                 API.Log("[IntroCutscene] ERROR: Clound not find Camera! sequence will fail.");
            }
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
            if (enemy3ID == 0) enemy3ID = API.FindEntity("Patrol enemy 3"); // Try Capitalized
            
            if (enemy3ID != 0) {
                 API.Log("[IntroCutscene] Found 'patrol enemy 3'. Adding keys.");
                 Vec3 ePos = API.GetPosition(enemy3ID);
                 // Camera Position relative to enemy
                 // Front View for Enemy 3
                 // Raised Y to look over rocks, modified Z to be closer
                 Vec3 camPos = new Vec3(ePos.X + 5.0f, ePos.Y + 5.0f, ePos.Z + 5.0f );
                 
                 // Move Start
                 AddPosKey(posTrack, frame, camPos); 
                 AddLookKey(lookTrack, frame, "patrol enemy 3");
                 
                 // Wait 5 seconds & Pull Back
                 frame += 5 * fps;
                 // End Pos: Move BACK (Z - 12.0f) to create a dolly-out effect
                 Vec3 camPosEnd = new Vec3(ePos.X, ePos.Y + 5.0f, ePos.Z + 13.0f);
                 AddPosKey(posTrack, frame, camPosEnd); 
                 AddLookKey(lookTrack, frame, "patrol enemy 3");
            } else { API.Log("[IntroCutscene] WARNING: 'patrol enemy 3' not found!"); }
            
            // 2. Sentry 2
            ulong sentry2ID = API.FindEntity("Sentry_2");
            if (sentry2ID != 0) {
                 Vec3 ePos = API.GetPosition(sentry2ID);
                 // Side View (Left) - "Rotated Left"
                 // User requested X=0 (Center). Z+5 is the "perfect" distance.
                 Vec3 camPos = new Vec3(ePos.X, ePos.Y + 1.0f , ePos.Z + 5.0f);

                 
                 // Ensure we look at Sentry IMMEDIATELY
                 AddLookKey(lookTrack, frame, "Sentry_2"); 
                 AddPosKey(posTrack, frame, camPos);
                 
                 // Wait 5 seconds
                 frame += 3 * fps;
                 AddPosKey(posTrack, frame, camPos); // Hold Pos
                 AddLookKey(lookTrack, frame, "Sentry_2");
            } else { API.Log("[IntroCutscene] WARNING: 'Sentry_2' not found!"); }
            
            // 3. Key
            ulong keyID = API.FindEntity("Key");
            if (keyID != 0) {
                 Vec3 ePos = API.GetPosition(keyID);
                 // Diagonal Front View for Key - Flipping to opposite side to see "Front"
                 Vec3 camPos = new Vec3(ePos.X, ePos.Y, ePos.Z + 5.0f); 
                 
                 // CUT to Key (No Pan)
                 AddLookKey(lookTrack, frame, "Key");
                 AddPosKey(posTrack, frame, camPos);
                 
                 // Wait 3 seconds
                 frame += 3 * fps;
                 AddPosKey(posTrack, frame, camPos);
                 AddLookKey(lookTrack, frame, "Key");
                 
                 // Wait 5 seconds
                 frame += 5 * fps;
                 AddPosKey(posTrack, frame, camPos);
                 AddLookKey(lookTrack, frame, "Key");
            } else { API.Log("[IntroCutscene] WARNING: 'Key' not found!"); }
            
            // 4. Back to Player
            ulong playerID = API.FindEntity("Player");
             if (playerID == 0) playerID = PlayerMovement.GetPlayerEntity();
            
            if (playerID != 0) {
                 // Restore original camera offset? Or just a nice view?
                 // Let's use a standard offset
                 Vec3 ePos = API.GetPosition(playerID);
                 Vec3 camPos = new Vec3(ePos.X, ePos.Y + 4.0f, ePos.Z - 6.0f); // Back up a bit
                 
                 int panFrames = 2 * fps;
                 frame += panFrames;
                 
                 AddPosKey(posTrack, frame, camPos);
                 AddPosKey(posTrack, frame, camPos);
                 AddLookKey(lookTrack, frame, "Player");
            } else { API.Log("[IntroCutscene] WARNING: Player not found!"); }
            
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
            if (_pendingStart) {
                _startDelayFrames--;

                if (_startDelayFrames <= 0) {
                     _pendingStart = false;
                     _pendingStart = false;
                     BuildSequence();
                     _sequencer.Play();
                     API.Log("[IntroCutscene] Delayed Play() executed.");
                }
                return;
            }

            if (_initialized && _sequencer != null)
            {
                _sequencer.OnUpdate(dt);
                // Debug log every second
                // static float logTimer = 0; logTimer += dt; if(logTimer > 1.0f) { API.Log("[IntroCutscene] Tick..."); logTimer=0; }
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
