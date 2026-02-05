using System;
using Boom;

namespace GameScripts
{
    /// <summary>
    /// Automatically moves the player from start position to end position,
    /// then transitions to the main menu after a set duration.
    /// Replaces normal player movement controls during the cutscene.
    /// </summary>
    public class BossCutsceneMovement
    {
        // Entity handle - automatically populated by the engine
        public ulong Entity;

        // Movement parameters - configure these in inspector or set programmatically
        [EditorExposed("Start Position", "Position where player starts (top of steps)")]
        public Vec3 startPosition = new Vec3(0, 5, 0);

        [EditorExposed("End Position", "Position where player ends (bottom of steps)")]
        public Vec3 endPosition = new Vec3(0, 0, 10);

        [EditorExposed("Movement Duration", "How long the movement takes (seconds)")]
        public float movementDuration = 8.0f;

        [EditorExposed("Total Duration", "Total time before loading main menu (seconds)")]
        public float totalDuration = 10.0f;

        [EditorExposed("Fade Duration", "Duration of fade out before scene transition")]
        public float fadeDuration = 1.0f;

        [EditorExposed("Start Delay", "Delay before movement starts (allows scene to load)")]
        public float startDelay = 2.0f;

        [EditorExposed("Use Smooth Movement", "Use smooth easing for movement")]
        public bool useSmoothMovement = true;

        [EditorExposed("Enable Fade Effects", "Enable fade in/out effects (disable for easier editing)")]
        public bool enableFadeEffects = true;

        [EditorExposed("Maze Trigger Progress", "Progress % (0-1) when maze generation triggers")]
        public float mazeTriggerProgress = 0.5f;

        [EditorExposed("Maze Entity Name", "Name of the entity with MazeGeneration script")]
        public string mazeEntityName = "MazeTrigger";

        [EditorExposed("Spotlight Entity Name", "Name of spotlight to control (optional)")]
        public string spotlightEntityName = "";

        [EditorExposed("Enable Spotlight Control", "Control a red/green spotlight during cutscene")]
        public bool enableSpotlightControl = false;

        [EditorExposed("Spotlight Interval", "How often spotlight switches colors (seconds)")]
        public float spotlightInterval = 3.0f;

        [EditorExposed("Play Spotlight Audio", "Play red/green light audio")]
        public bool playSpotlightAudio = false;

        [EditorExposed("Red Light Audio Path", "Path to red light audio")]
        public string redLightAudioPath = "Resources/Audio/Redlight.wav";

        [EditorExposed("Green Light Audio Path", "Path to green light audio")]
        public string greenLightAudioPath = "Resources/Audio/Greenlight.wav";

        // Camera pan parameters
        [EditorExposed("Enable Camera Pan", "Enable slow upward camera rotation")]
        public bool enableCameraPan = false;

        [EditorExposed("Camera Entity Name", "Name of camera entity to rotate")]
        public string cameraEntityName = "Camera";

        [EditorExposed("Camera X Rotation Amount", "Total degrees to rotate camera X (upward)")]
        public float cameraXRotationAmount = 15.0f;

        [EditorExposed("Camera Pan Duration", "How long the camera rotation takes (seconds)")]
        public float cameraPanDuration = 8.0f;

        // Internal state
        private float _elapsedTime = 0f;
        private bool _movementComplete = false;
        private bool _isFading = false;
        private float _fadeTimer = 0f;
        private bool _transitionTriggered = false;
        private bool _mazeTriggered = false;

        // Fade in state
        private bool _isFadingIn = true;
        private float _fadeInTimer = 0f;

        // Start delay state
        private bool _isWaitingToStart = true;
        private float _startDelayTimer = 0f;

        // Store previous position for movement delta
        private Vec3 _previousPosition;

        // Spotlight control
        private ulong _spotlightEntity = 0;
        private bool _spotlightIsRed = true;
        private float _spotlightTimer = 0f;
        private Vec3 _redColor = new Vec3(1f, 0f, 0f);
        private Vec3 _greenColor = new Vec3(0f, 1f, 0f);

        // Camera pan control
        private ulong _cameraEntity = 0;
        private Vec3 _cameraStartRotation;
        private Vec3 _cameraTargetRotation;
        private float _cameraPanTimer = 0f;
        private bool _cameraPanComplete = false;

        public void OnStart(string paramsJson)
        {
            if (Entity == 0)
            {
                API.Log("[BossCutsceneMovement] Warning: Entity handle not set");
                return;
            }

            // Start faded to black (only if fade effects enabled)
            if (enableFadeEffects)
            {
                API.SetScreenFadeAlpha(1f);
                _isFadingIn = true;
            }
            else
            {
                API.SetScreenFadeAlpha(0f);
                _isFadingIn = false;
            }

            _fadeInTimer = 0f;
            _elapsedTime = 0f;
            _movementComplete = false;
            _isFading = false;
            _transitionTriggered = false;
            _mazeTriggered = false;
            _isWaitingToStart = true;
            _startDelayTimer = 0f;

            // Set player to start position
            if (API.HasTransform(Entity))
            {
                API.SetPosition(Entity, startPosition);
                _previousPosition = startPosition;
                API.Log($"[BossCutsceneMovement] Player set to start position: ({startPosition.X}, {startPosition.Y}, {startPosition.Z})");
            }

            // If the player has a character controller, use TeleportController for initial setup
            if (API.HasCollider(Entity))
            {
                API.TeleportController(Entity, startPosition);
            }

            // Don't play animation yet - wait for start delay
            if (API.HasAnimator(Entity))
            {
                API.AnimatorSetBool(Entity, "IsMoving", false);
                API.AnimatorSetFloat(Entity, "Speed", 0f);
                API.AnimatorSetBool(Entity, "Sprint", false);
                API.AnimatorSetBool(Entity, "IsSneaking", false);
            }

            // Initialize spotlight control if enabled
            if (enableSpotlightControl && !string.IsNullOrEmpty(spotlightEntityName))
            {
                _spotlightEntity = API.FindEntity(spotlightEntityName);
                if (_spotlightEntity != 0 && API.HasSpotLight(_spotlightEntity))
                {
                    API.Log($"[BossCutsceneMovement] Found spotlight: {spotlightEntityName}");
                    _spotlightIsRed = true;
                    _spotlightTimer = 0f;
                    API.SetSpotLightColor(_spotlightEntity, _redColor);
                }
                else
                {
                    API.Log($"[BossCutsceneMovement] WARNING: Could not find spotlight '{spotlightEntityName}' or it has no spotlight component");
                    _spotlightEntity = 0;
                }
            }

            // Initialize camera pan if enabled
            if (enableCameraPan && !string.IsNullOrEmpty(cameraEntityName))
            {
                _cameraEntity = API.FindEntity(cameraEntityName);
                if (_cameraEntity != 0 && API.HasTransform(_cameraEntity))
                {
                    _cameraStartRotation = API.GetRotation(_cameraEntity);
                    _cameraTargetRotation = new Vec3(
                        _cameraStartRotation.X + cameraXRotationAmount,
                        _cameraStartRotation.Y,
                        _cameraStartRotation.Z
                    );
                    _cameraPanTimer = 0f;
                    _cameraPanComplete = false;
                    API.Log($"[BossCutsceneMovement] Camera pan initialized: {cameraEntityName}, rotating X by {cameraXRotationAmount} degrees over {cameraPanDuration}s");
                }
                else
                {
                    API.Log($"[BossCutsceneMovement] WARNING: Could not find camera '{cameraEntityName}' or it has no transform");
                    _cameraEntity = 0;
                }
            }

            API.Log($"[BossCutsceneMovement] Cutscene initialized. Start delay: {startDelay}s, Movement: {movementDuration}s, Total: {totalDuration}s");
        }

        public void OnUpdate(float deltaTime)
        {
            if (Entity == 0 || _transitionTriggered) return;

            // Handle start delay - wait before beginning movement
            if (_isWaitingToStart)
            {
                _startDelayTimer += deltaTime;

                if (_startDelayTimer >= startDelay)
                {
                    _isWaitingToStart = false;
                    API.Log("[BossCutsceneMovement] Start delay complete, beginning movement");

                    // Now start walking animation
                    if (API.HasAnimator(Entity))
                    {
                        API.AnimatorSetBool(Entity, "IsMoving", true);
                        API.AnimatorSetFloat(Entity, "Speed", 3.0f);
                        API.AnimatorSetBool(Entity, "Sprint", false);
                        API.AnimatorSetBool(Entity, "IsSneaking", false);
                    }
                }
                // During start delay, just handle fade-in but don't move
            }

            // Handle fade-in from black (when scene first loads)
            if (_isFadingIn && enableFadeEffects)
            {
                _fadeInTimer += deltaTime;
                float alpha = 1f - Clamp01(_fadeInTimer / fadeDuration);
                API.SetScreenFadeAlpha(alpha);

                if (_fadeInTimer >= fadeDuration)
                {
                    API.SetScreenFadeAlpha(0f);
                    _isFadingIn = false;
                    API.Log("[BossCutsceneMovement] Fade-in complete");
                }
            }
            else if (_isFadingIn && !enableFadeEffects)
            {
                // Skip fade-in if disabled
                _isFadingIn = false;
            }

            // Handle fading out (before scene transition)
            if (_isFading)
            {
                if (enableFadeEffects)
                {
                    _fadeTimer += deltaTime;
                    float alpha = Clamp01(_fadeTimer / fadeDuration);
                    API.SetScreenFadeAlpha(alpha);

                    if (_fadeTimer >= fadeDuration)
                    {
                        // Transition to main menu
                        _transitionTriggered = true;
                        API.Log("[BossCutsceneMovement] Loading MainMenu scene");
                        API.LoadScene(Entry.MAIN_MENU_SCENE_NAME);
                    }
                }
                else
                {
                    // No fade, transition immediately
                    _transitionTriggered = true;
                    API.Log("[BossCutsceneMovement] Loading MainMenu scene (no fade)");
                    API.LoadScene(Entry.MAIN_MENU_SCENE_NAME);
                }
                return;
            }

            // Update spotlight cycling (happens even during start delay)
            if (enableSpotlightControl && _spotlightEntity != 0)
            {
                UpdateSpotlight(deltaTime);
            }

            // Update camera pan (happens even during start delay)
            if (enableCameraPan && _cameraEntity != 0 && !_cameraPanComplete)
            {
                UpdateCameraPan(deltaTime);
            }

            // Don't move during start delay
            if (_isWaitingToStart)
            {
                return;
            }

            // Update elapsed time
            _elapsedTime += deltaTime;

            // Move the player if movement is still active
            if (!_movementComplete && _elapsedTime < movementDuration)
            {
                // Calculate interpolation factor (0 to 1)
                float t = _elapsedTime / movementDuration;

                // Apply easing if smooth movement is enabled
                if (useSmoothMovement)
                {
                    // Smooth ease in-out (cubic)
                    t = t < 0.5f
                        ? 4f * t * t * t
                        : 1f - (float)Math.Pow(-2f * t + 2f, 3f) / 2f;
                }

                // Interpolate position
                Vec3 currentPosition = new Vec3(
                    Lerp(startPosition.X, endPosition.X, t),
                    Lerp(startPosition.Y, endPosition.Y, t),
                    Lerp(startPosition.Z, endPosition.Z, t)
                );

                // Calculate movement delta from previous position
                Vec3 movementDelta = new Vec3(
                    currentPosition.X - _previousPosition.X,
                    currentPosition.Y - _previousPosition.Y,
                    currentPosition.Z - _previousPosition.Z
                );

                // Use MoveController to move (this triggers collision callbacks)
                if (API.HasCollider(Entity))
                {
                    API.MoveController(Entity, movementDelta, 0.001f, deltaTime);
                }
                else if (API.HasTransform(Entity))
                {
                    // Fallback to direct position setting if no controller
                    API.SetPosition(Entity, currentPosition);
                }

                // Update previous position for next frame
                _previousPosition = currentPosition;

                // Check if we should trigger the maze generation
                if (!_mazeTriggered && t >= mazeTriggerProgress)
                {
                    TriggerMazeGeneration();
                    _mazeTriggered = true;
                }

                // Calculate direction for rotation (look towards movement direction)
                Vec3 direction = new Vec3(
                    endPosition.X - startPosition.X,
                    0,  // Keep Y flat for rotation
                    endPosition.Z - startPosition.Z
                );
                float dirLength = (float)Math.Sqrt(direction.X * direction.X + direction.Z * direction.Z);
                if (dirLength > 0.001f)
                {
                    float targetYaw = (float)(Math.Atan2(direction.X, direction.Z) * 180.0 / Math.PI);
                    API.SetRotationY(Entity, targetYaw);
                }
            }
            else if (!_movementComplete && _elapsedTime >= movementDuration)
            {
                // Movement complete - ensure we're at exact final position
                _movementComplete = true;

                // Calculate final movement delta to reach exact end position
                Vec3 currentPos = API.GetPosition(Entity);
                Vec3 finalDelta = new Vec3(
                    endPosition.X - currentPos.X,
                    endPosition.Y - currentPos.Y,
                    endPosition.Z - currentPos.Z
                );

                // Move to final position
                if (API.HasCollider(Entity))
                {
                    API.MoveController(Entity, finalDelta, 0.001f, deltaTime);
                }
                else if (API.HasTransform(Entity))
                {
                    API.SetPosition(Entity, endPosition);
                }

                _previousPosition = endPosition;

                // Stop walking animation
                if (API.HasAnimator(Entity))
                {
                    API.AnimatorSetBool(Entity, "IsMoving", false);
                    API.AnimatorSetFloat(Entity, "Speed", 0f);
                }

                API.Log("[BossCutsceneMovement] Movement complete");
            }

            // Check if it's time to transition to main menu
            if (_elapsedTime >= totalDuration && !_isFading)
            {
                API.Log("[BossCutsceneMovement] Starting transition to MainMenu");
                StartTransition();
            }
        }

        private void StartTransition()
        {
            if (_isFading) return;

            _isFading = true;
            _fadeTimer = 0f;
        }

        private static float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        private static float Clamp01(float v)
        {
            return v < 0f ? 0f : (v > 1f ? 1f : v);
        }

        /// <summary>
        /// Update the spotlight color cycling
        /// </summary>
        private void UpdateSpotlight(float deltaTime)
        {
            _spotlightTimer += deltaTime;

            if (_spotlightTimer >= spotlightInterval)
            {
                _spotlightTimer = 0f;
                _spotlightIsRed = !_spotlightIsRed;

                if (API.HasSpotLight(_spotlightEntity))
                {
                    if (_spotlightIsRed)
                    {
                        API.SetSpotLightColor(_spotlightEntity, _redColor);
                        API.Log("[BossCutsceneMovement] Spotlight -> RED");

                        if (playSpotlightAudio)
                        {
                            Vec3 pos = API.GetPosition(_spotlightEntity);
                            API.PlaySoundAt("cutscene_redlight", redLightAudioPath, pos, false);
                            API.SetSoundVolume("cutscene_redlight", 0.8f);
                        }
                    }
                    else
                    {
                        API.SetSpotLightColor(_spotlightEntity, _greenColor);
                        API.Log("[BossCutsceneMovement] Spotlight -> GREEN");

                        if (playSpotlightAudio)
                        {
                            Vec3 pos = API.GetPosition(_spotlightEntity);
                            API.PlaySoundAt("cutscene_greenlight", greenLightAudioPath, pos, false);
                            API.SetSoundVolume("cutscene_greenlight", 0.8f);
                        }
                    }
                }
            }
        }

        /// <summary>
        /// Update the camera pan rotation (slow upward tilt)
        /// </summary>
        private void UpdateCameraPan(float deltaTime)
        {
            if (_cameraPanComplete || _cameraEntity == 0 || !API.HasTransform(_cameraEntity))
                return;

            _cameraPanTimer += deltaTime;

            // Calculate interpolation factor (0 to 1)
            float t = Clamp01(_cameraPanTimer / cameraPanDuration);

            // Apply smooth easing for camera movement (ease in-out)
            float easedT = t < 0.5f
                ? 2f * t * t
                : 1f - (float)Math.Pow(-2f * t + 2f, 2f) / 2f;

            // Interpolate camera rotation
            Vec3 currentRotation = new Vec3(
                Lerp(_cameraStartRotation.X, _cameraTargetRotation.X, easedT),
                Lerp(_cameraStartRotation.Y, _cameraTargetRotation.Y, easedT),
                Lerp(_cameraStartRotation.Z, _cameraTargetRotation.Z, easedT)
            );

            API.SetRotation(_cameraEntity, currentRotation);

            // Mark as complete when finished
            if (_cameraPanTimer >= cameraPanDuration)
            {
                _cameraPanComplete = true;
                API.SetRotation(_cameraEntity, _cameraTargetRotation);
                API.Log("[BossCutsceneMovement] Camera pan complete");
            }
        }

        /// <summary>
        /// Triggers the maze generation via static method call
        /// </summary>
        private void TriggerMazeGeneration()
        {
            API.Log($"[BossCutsceneMovement] Triggering maze generation at progress {mazeTriggerProgress:F2}");

            try
            {
                // If a specific maze entity name is provided, use it; otherwise use the primary instance
                if (!string.IsNullOrEmpty(mazeEntityName) && mazeEntityName != "MazeTrigger")
                {
                    // Trigger specific maze by name
                    MazeGeneration.TriggerMazeByName(mazeEntityName);
                    API.Log($"[BossCutsceneMovement] Triggered maze by name: {mazeEntityName}");
                }
                else
                {
                    // Trigger the primary maze instance
                    MazeGeneration.TriggerMazeFromExternal();
                    API.Log($"[BossCutsceneMovement] Triggered primary maze instance");
                }
            }
            catch (Exception ex)
            {
                API.Log($"[BossCutsceneMovement] ERROR triggering maze: {ex.Message}");
            }
        }

        public void OnDestroy()
        {
            // Reset screen fade when destroyed
            API.SetScreenFadeAlpha(0f);

            // Reset camera rotation if it was modified
            if (enableCameraPan && _cameraEntity != 0 && API.HasTransform(_cameraEntity))
            {
                API.SetRotation(_cameraEntity, _cameraStartRotation);
                API.Log("[BossCutsceneMovement] Camera rotation reset");
            }
        }
    }
}
