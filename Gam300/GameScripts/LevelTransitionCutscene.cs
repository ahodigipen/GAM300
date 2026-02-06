using Boom;
using System;
using System.Collections.Generic;

namespace GameScripts
{
    public class LevelTransitionCutscene
    {
        public ulong Entity;

        private CutsceneSequencer _sequencer;
        private bool _initialized = false;
        private bool _hasTriggered = false;

        private float _totalDuration = 0f;
        private float _elapsedTime = 0f;
        private bool _isPlaying = false;
        private bool _hasFinished = false;

        public void OnStart(string jsonParams)
        {
            API.Log("[LevelTransitionCutscene] OnStart. Waiting for Trigger...");
            try
            {
                _sequencer = new CutsceneSequencer();
                _sequencer.Entity = Entity;
                _initialized = true;

                // Register Collision/Trigger Callback
                if (API.HasCollider(Entity))
                {
                    API.RegisterTriggerEnterCallback(Entity, OnTriggerEnter);
                    API.Log($"[LevelTransition] Registered Trigger Callback on Entity {Entity}");
                }
                else
                {
                    API.Log($"[LevelTransition] WARNING: Entity {Entity} has no Collider!");
                }
            }
            catch (Exception ex)
            {
                API.Log("[LevelTransitionCutscene] ERROR: " + ex.ToString());
            }
        }

        public void OnTriggerEnter(ulong triggerID, ulong otherID)
        {
            if (_hasTriggered) return;

            // Check if it's Player
            ulong playerID = API.FindEntity("Player");
            if (otherID != playerID)
            {
                // Try fallback logic if exact ID match fails (e.g. searching name of otherID?)
                // API.GetName(otherID) is not always available, but let's assume strict ID check first.
                return;
            }

            API.Log("[LevelTransition] Player entered trigger! Starting Cutscene.");
            _hasTriggered = true;

            // Auto-dismiss any active tutorials before starting cutscene
            if (TutorialManager.IsTutorialActive())
            {
                API.Log("[LevelTransition] Dismissing active tutorial for cutscene");
                TutorialManager.DismissTutorial();
            }
            if (TutorialPopupTrigger.IsPopupActive())
            {
                API.Log("[LevelTransition] Dismissing active popup for cutscene");
                TutorialPopupTrigger.DismissActivePopup();
            }

            BuildSequence();
            _sequencer.Play();

            // Start Timer
            _isPlaying = true;
            _elapsedTime = 0f;
        }

        private void BuildSequence()
        {
            _sequencer.ClearTracks();
            _sequencer.BlockInput = true;
            _sequencer.Loop = false;

            ulong cameraID = API.FindEntity("Camera");
            if (cameraID == 0) cameraID = API.FindEntity("Main Camera");
            if (cameraID == 0) cameraID = API.FindEntity("MainCamera");

            if (cameraID == 0)
            {
                API.Log("[LevelTransitionCutscene] ERROR: Camera not found!");
                return;
            }

            // Define Tracks
            CutsceneSequencer.Track posTrack = new CutsceneSequencer.Track();
            posTrack.label = "CameraPos";
            posTrack.targetEntityName = "Camera";
            posTrack.cachedEntityID = cameraID;
            posTrack.type = 0; // Position

            CutsceneSequencer.Track lookTrack = new CutsceneSequencer.Track();
            lookTrack.label = "CameraLook";
            lookTrack.targetEntityName = "Camera";
            lookTrack.cachedEntityID = cameraID;
            lookTrack.type = 4; // LookAt

            CutsceneSequencer.Track rotTrack = new CutsceneSequencer.Track();
            rotTrack.label = "CameraRot";
            rotTrack.targetEntityName = "Camera";
            rotTrack.cachedEntityID = cameraID;
            rotTrack.type = 1; // Rotation

            // Sequence Logic
            int fps = 60;
            int frame = 0;

            // Safety: Neutral Rotation at start
            AddRotKey(rotTrack, 0, new Vec3(0f, 0f, 0f));

            // 1. Enemy: Level2_PatrolEnemy_3 (Hover/Follow)
            ulong enemyID = FindEntityFallback("Level2_PatrolEnemy_3");
            if (enemyID != 0)
            {
                Vec3 ePos = API.GetPosition(enemyID);
                // Start: Offset (X-5, Y+5, Z+5)
                Vec3 startPos = new Vec3(ePos.X + 5.0f, ePos.Y + 5.0f, ePos.Z + 5.0f);
                // End: Move backwards slightly (Z-5) over 4 seconds
                Vec3 endPos = new Vec3(ePos.X + 5.5f, ePos.Y + 6.0f, ePos.Z - 5.0f);

                AddPosKey(posTrack, frame, startPos);
                AddLookKey(lookTrack, frame, "Level2_PatrolEnemy_3");

                int endFrame = frame + 4 * fps;
                AddPosKey(posTrack, endFrame, endPos);
                AddLookKey(lookTrack, endFrame, "Level2_PatrolEnemy_3");
            }
            else { API.Log("[LevelTransition] WARNING: 'Level2_PatrolEnemy_3' not found!"); }
            frame += 4 * fps; // ALWAYS Advance time

            // 2. Map 1: Level 2 Map (CutScene)
            ulong map1ID = FindEntityFallback("Level 2 Map (CutScene)");
            if (map1ID != 0)
            {
                Vec3 ePos = API.GetPosition(map1ID);
                Vec3 camPos = new Vec3(ePos.X + 5.0f, ePos.Y + 2.0f, ePos.Z); // High angle

                // CUT to Map 1 - Top Down View
                AddPosKey(posTrack, frame, camPos);
                AddRotKey(rotTrack, frame, new Vec3(-90.0f, 90.0f, 0.0f));
                AddLookKey(lookTrack, frame, "None");

                // Pan for 4 seconds
                int endFrame = frame + 5 * fps;
                Vec3 endPos = new Vec3(ePos.X, ePos.Y + 2.0f, ePos.Z + 25.0f); // Slight drift
                AddPosKey(posTrack, endFrame, endPos);
                AddRotKey(rotTrack, endFrame, new Vec3(-90.0f, 90.0f, 0.0f));
                AddLookKey(lookTrack, endFrame, "None");
            }
            else { API.Log("[LevelTransition] WARNING: 'Level 2 Map (CutScene)' not found!"); }
            frame += 4 * fps; // ALWAYS Advance time

            // 3. Pan towards "Level 2 Map (1 CutScene)"
            ulong map1CutSceneID = FindEntityFallback("Level 2 Map (1 CutScene)");
            if (map1CutSceneID != 0)
            {
                Vec3 targetPos = API.GetPosition(map1CutSceneID);
                Vec3 endPos = new Vec3(targetPos.X, targetPos.Y + 2.0f, targetPos.Z);

                int endFrame = frame + 1 * fps;

                AddPosKey(posTrack, endFrame, endPos);
                AddRotKey(rotTrack, endFrame, new Vec3(-90.0f, 90.0f, 0.0f));
                AddLookKey(lookTrack, endFrame, "None");
            }
            else { API.Log("[LevelTransition] WARNING: 'Level 2 Map (1 CutScene)' not found!"); }
            frame += 4 * fps; // ALWAYS Advance time

            // 4. Pan towards "Level 2 Map (2 CutScene)"
            ulong map2CutSceneID = FindEntityFallback("Level 2 Map (2 CutScene)");
            if (map2CutSceneID != 0)
            {
                Vec3 targetPos = API.GetPosition(map2CutSceneID);
                Vec3 endPos = new Vec3(targetPos.X, targetPos.Y + 2.0f, targetPos.Z);

                int endFrame = frame + 2 * fps;

                AddPosKey(posTrack, endFrame, endPos);
                AddRotKey(rotTrack, endFrame, new Vec3(-90.0f, 90.0f, 0.0f));
                AddLookKey(lookTrack, endFrame, "None");
            }
            else { API.Log("[LevelTransition] WARNING: 'Level 2 Map (2 CutScene)' not found!"); }
            frame += 4 * fps; // ALWAYS Advance time

            // 5. Pan towards "Level 2 Map (3 CutScene)"
            ulong map3CutSceneID = FindEntityFallback("Level 2 Map (3 CutScene)");
            if (map3CutSceneID != 0)
            {
                Vec3 targetPos = API.GetPosition(map3CutSceneID);
                Vec3 endPos = new Vec3(targetPos.X, targetPos.Y + 2.0f, targetPos.Z);

                int endFrame = frame + 4 * fps;

                AddPosKey(posTrack, endFrame, endPos);
                AddRotKey(rotTrack, endFrame, new Vec3(-90.0f, 90.0f, 0.0f));
                AddLookKey(lookTrack, endFrame, "None");
            }
            else { API.Log("[LevelTransition] WARNING: 'Level 2 Map (3 CutScene)' not found!"); }
            frame += 4 * fps; // ALWAYS Advance time

            // 6. SNAP to "Level 2 Map (4 CutScene)" (Side View - Left/Right)
            ulong map4CutSceneID = FindEntityFallback("Level 2 Map (4 CutScene)");
            if (map4CutSceneID != 0)
            {
                Vec3 targetPos = API.GetPosition(map4CutSceneID);
                // Side View: Position to LEFT (-X) and Look RIGHT (+X)
                // Assuming standard Right Handed Y-up: 0= -Z, 90= -X, 180= +Z, 270= +X
                // Let's try Yaw -90 (Look Left/Right)
                Vec3 camPos = new Vec3(targetPos.X, targetPos.Y + 2.0f, targetPos.Z);

                // SNAP: Add Key at CURRENT frame
                AddPosKey(posTrack, frame, camPos);
                AddRotKey(rotTrack, frame, new Vec3(0.0f, 80.0f, 0.0f)); // Pitch 0, Yaw -90
                AddLookKey(lookTrack, frame, "None");

                // Hold for 3 seconds
                int endFrame = frame + 5 * fps;
                AddPosKey(posTrack, endFrame, camPos);
                AddRotKey(rotTrack, endFrame, new Vec3(0.0f, 120.0f, 0.0f));
                AddLookKey(lookTrack, endFrame, "None");

                frame = endFrame;
            }
            else { API.Log("[LevelTransition] WARNING: 'Level 2 Map (4 CutScene)' not found!"); }
            // If not found, advance time slightly
            if (map4CutSceneID == 0) frame += 2 * fps;

            // 7. Back to Player
            ulong playerID = API.FindEntity("Player");
            if (playerID == 0) playerID = PlayerMovement.GetPlayerEntity();
            if (playerID != 0)
            {
                Vec3 ePos = API.GetPosition(playerID);
                Vec3 camPos = new Vec3(ePos.X, ePos.Y + 4.0f, ePos.Z - 6.0f);

                // CUT Back
                AddPosKey(posTrack, frame, camPos);
                AddRotKey(rotTrack, frame, new Vec3(0f, 0f, 0f));
                AddLookKey(lookTrack, frame, "Player");
            }

            // Register Tracks
            // Add RotTrack FIRST so it is applied as "Base Layer"
            _sequencer.AddTrack(rotTrack);
            _sequencer.AddTrack(posTrack);
            _sequencer.AddTrack(lookTrack);

            _totalDuration = frame / (float)fps;
            API.Log($"[LevelTransition] Built sequence. Duration: {frame} frames.");
        }

        private void AddPosKey(CutsceneSequencer.Track t, int frame, Vec3 pos)
        {
            CutsceneSequencer.KeyFrame kf = new CutsceneSequencer.KeyFrame();
            kf.frame = frame; kf.time = frame / 60.0f;
            kf.vX = pos.X; kf.vY = pos.Y; kf.vZ = pos.Z;
            t.keyframes.Add(kf);
        }

        private void AddLookKey(CutsceneSequencer.Track t, int frame, string targetName)
        {
            CutsceneSequencer.KeyFrame kf = new CutsceneSequencer.KeyFrame();
            kf.frame = frame; kf.time = frame / 60.0f;
            kf.valStr = targetName;
            t.keyframes.Add(kf);
        }

        private void AddRotKey(CutsceneSequencer.Track t, int frame, Vec3 rot)
        {
            CutsceneSequencer.KeyFrame kf = new CutsceneSequencer.KeyFrame();
            kf.frame = frame; kf.time = frame / 60.0f;
            kf.vX = rot.X; kf.vY = rot.Y; kf.vZ = rot.Z;
            t.keyframes.Add(kf);
        }

        private ulong FindEntityFallback(string name)
        {
            ulong id = API.FindEntity(name);
            if (id != 0) return id;

            // Try 1: Replace Spaces with Underscores
            string withUnderscores = name.Replace(' ', '_');
            id = API.FindEntity(withUnderscores);
            if (id != 0)
            {
                API.Log($"[LevelTransition] Found '{withUnderscores}' (Fallback from '{name}')");
                return id;
            }

            // Try 2: Replace Underscores with Spaces
            string withSpaces = name.Replace('_', ' ');
            id = API.FindEntity(withSpaces);
            if (id != 0)
            {
                API.Log($"[LevelTransition] Found '{withSpaces}' (Fallback from '{name}')");
                return id;
            }

            return 0;
        }

        public void OnUpdate(float dt)
        {
            // Fallback: Distance Check if Trigger fails
            if (!_hasTriggered && _initialized)
            {
                ulong playerID = API.FindEntity("Samurai");
                if (playerID == 0) playerID = PlayerMovement.GetPlayerEntity();

                if (playerID != 0)
                {
                    Vec3 myPos = API.GetPosition(Entity);
                    Vec3 pPos = API.GetPosition(playerID);

                    float dx = myPos.X - pPos.X;
                    float dy = myPos.Y - pPos.Y;
                    float dz = myPos.Z - pPos.Z;
                    float distSq = dx * dx + dy * dy + dz * dz;

                    // Threshold: 3.0 units (Squared = 9.0)
                    if (distSq < 9.0f)
                    {
                        API.Log($"[LevelTransition] Distance Check Triggered! ({distSq} < 9.0)");
                        _hasTriggered = true;

                        // Auto-dismiss any active tutorials before starting cutscene
                        if (TutorialManager.IsTutorialActive())
                        {
                            API.Log("[LevelTransition] Dismissing active tutorial for cutscene (distance check)");
                            TutorialManager.DismissTutorial();
                        }
                        if (TutorialPopupTrigger.IsPopupActive())
                        {
                            API.Log("[LevelTransition] Dismissing active popup for cutscene (distance check)");
                            TutorialPopupTrigger.DismissActivePopup();
                        }

                        BuildSequence();
                        _sequencer.Play();
                    }
                }
            }

            if (_initialized && _sequencer != null)
            {
                if (API.IsKeyDown(API.KEY_SPACE))
                {
                    _sequencer.Skip();
                }
             
                _sequencer.OnUpdate(dt);
        

                // TRACK COMPLETION
                if (_isPlaying && !_hasFinished)
                {
                    _elapsedTime += dt;
                    if (_elapsedTime >= _totalDuration)
                    {
                        _hasFinished = true;
                        _isPlaying = false;
                        API.Log("[LevelTransition] Cutscene Finished.");
                    }
                }
            }
        }

        public void OnDestroy() { if (_sequencer != null) _sequencer.OnDestroy(); }
    }
}
