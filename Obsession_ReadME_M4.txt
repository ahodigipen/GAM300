=============================================================================
  ____                          _____             _
 | __ )  ___   ___  _ __ ___   | ____|_ __   __ _(_)_ __   ___
 |  _ \ / _ \ / _ \| '_ ` _ \  |  _| | '_ \ / _` | | '_ \ / _ \
 | |_) | (_) | (_) | | | | | | | |___| | | | (_| | | | | |  __/
 |____/ \___/ \___/|_| |_| |_| |_____|_| |_|\__, |_|_| |_|\___|
                                            |___/
=============================================================================
by Team Obsession - M4
  1. Darius Maximus Chan Wei Jie 			   d.chan@digipen.edu
  2. Muhammad Nur Aqif Bin Abdemanaf 		muhammadnuraqif.b@digipen.edu
  3. Christopher Lam Yit Shyong 			    c.lam@digipen.edu
  4. Adam Goh Zheng Shan 				    goh.a@digipen.edu
  5. Tan Guan Yew, Wesley 			  t.guanyewwesley@digipen.edu
  6. Amos Ho Hin Wai 					     a.ho@digipen.edu
  7. Titus Kwong Wen Shuen 		      tituswenshuen.kwong@digipen.edu
  8. Leon Pablo Dominguez Mayor Gregorio 	       l.gregorio@digipen.edu
  9. Jericho Lorenz Villegas Quimson 			j.quimson@digipen.edu
 10. Lewis Koa Yi Heng 					    l.koa@digipen.edu
 11. Chee Kar Yi 				       karyi.chee@digipen.edu
 12. Sarah Hung 					   hung.s@digipen.edu
 13. Ang Yu Shi Sebrena         			    ang.y@digipen.edu
=============================================================================
Controls

Keyboard & Mouse
WASD							   Player Movement
Left Shift						         Sprint
Left Ctrl						  Crouch / Roll
E							       Interact
Escape							    Pause Menu
Mouse							  Camera Look

Gamepad (Controller)
Left Stick						   Player Movement
Left Trigger						         Sprint
Right Thumb Stick Click					         Sneak
Button B						        Crouch
Button A						Confirm / Interact
Button X / Y						         Roll
LB + RB							   Freeze Time
D-Pad Up / Down						    Menu Navigate
D-Pad Left / Right					   Volume Adjust
Start							    Pause Menu

Editor Controls
IMGUIZMO
Key 1 / Numpad 1					     Translate View
Key 2 / Numpad 2					        Rotate View
Key 3 / Numpad 3					         Scale View

Scene Manager
Ctrl + N						       New Scene
Ctrl + S						      Save Scene
Ctrl + Shift + S					     Save Scene As
Ctrl + O						      Load Scene
Ctrl + Z						           Undo
Ctrl + Y						           Redo
Alt  + F4						  Exit Application

Scene/Viewport Camera Controls
Key W 							   Move Forwards
Key A 							       Move Left
Key S						          Move Backwards
Key D							      Move Right
Scroll-Wheel Up						         Zoom In
Scroll-Wheel Down					        Zoom Out
Hold Mouse Right Click					Rotate Camera View

Prefab Browser
Double Left Click on prefab				Load Existing Prefabs
=============================================================================
How to Use
1. Before running the engine, install the latest version of Python on Windows
Store.

2. Run the setup.bat to install all dependencies.

3. Set Editor as startup project.

4. Run the Engine.

=============================================================================
Project Structure

GAM300/
├── GameScripts/                          # C# gameplay scripts (.NET / Mono)
│   ├── API.cs                            # C# bridge layer (P/Invoke + Gamepad API)
│   ├── Entry.cs                          # Script entry point (Init / Update hooks)
│   ├── PlayerMovement.cs                 # Player controller (walk, sprint, crouch, roll)
│   ├── PlayerFootsteps.cs                # Speed-based footstep audio
│   ├── PlayerInventory.cs                # Key collection and item tracking
│   ├── PlayerManager.cs                  # Player state coordination
│   ├── CharacterAnimation.cs             # Animation state controller
│   ├── MovementAnimator.cs               # Locomotion animation blending
│   ├── EnemyController.cs                # Stationary enemy with rotation and detection
│   ├── EnemyAI.cs                        # AI behaviour logic
│   ├── PatrolEnemyController.cs          # NavMesh-based patrol enemy
│   ├── VisionComponent.cs                # FOV detection with alert states
│   ├── ProximityDetectionComponent.cs    # Radius-based proximity triggers
│   ├── FreezeManager.cs                  # Time freeze mechanic
│   ├── FreezeOverlayBehavior.cs          # Freeze visual overlay effect
│   ├── MazeGeneration.cs                 # Procedural maze with BSP algorithm
│   ├── CutsceneSequencer.cs              # Keyframe camera animation system
│   ├── CutsceneController.cs             # Cutscene playback controller
│   ├── IntroCutscene.cs                  # Intro sequence controller
│   ├── BossCutsceneMovement.cs           # Boss encounter camera movement
│   ├── LevelTransitionCutscene.cs        # Level transition animation
│   ├── SpotlightFollower.cs              # Enemy spotlight tracking
│   ├── PatrolSpotlightFollower.cs        # Patrol route spotlight
│   ├── PatrolMultiDirectionSpotlight.cs  # Multi-directional spotlight
│   ├── MainMenu.cs                       # Main menu with gamepad support
│   ├── PauseMenu.cs                      # Pause menu with gamepad support
│   ├── DeathMenu.cs                      # Death screen with gamepad support
│   ├── EndMenu.cs                        # End screen
│   ├── HowToPlayMenu.cs                  # Tutorial menu
│   ├── UIManager.cs                      # HUD management
│   ├── UIHeartController.cs              # Health display
│   ├── UIKeyController.cs                # Key counter display
│   ├── UIFreezeController.cs             # Freeze cooldown indicator
│   ├── UIStanceController.cs             # Stance display
│   ├── UILocationController.cs           # Location display
│   ├── UIHoldController.cs               # Interaction hold indicator
│   ├── UIEndController.cs                # End game UI
│   ├── UITutorial.cs                     # Tutorial prompts
│   ├── UITutorialController.cs           # Tutorial state controller
│   ├── TutorialManager.cs                # Tutorial trigger manager
│   ├── TutorialPopupTrigger.cs           # Tutorial popup zones
│   ├── BloodOverlayController.cs         # Damage visual overlay
│   ├── AttackSquash.cs                   # Attack animation squash effect
│   ├── CPTrigger.cs                      # Checkpoint triggers
│   ├── DoorTriggerLeft.cs                # Door interaction triggers
│   ├── EndZoneTrigger.cs                 # Level end zone detection
│   ├── SceneTransitionTrigger.cs         # Scene transition zones
│   ├── KeyPickup.cs                      # Key pickup interaction
│   ├── CrouchTriggerText.cs              # Crouch zone UI text
│   ├── HoverMotion.cs                    # Floating animation effect
│   ├── FootStepsMixer.cs                 # Footstep audio mixer
│   ├── VolumeSlider.cs                   # Volume control slider
│   ├── SettingsManager.cs                # Game settings persistence
│   └── TextTest.cs                       # Text rendering test

├── BoomEngine/                           # Native runtime engine (C++20)
│   ├── includes/
│   │   ├── AI/
│   │   │   ├── AIComponent.h             # AI component interface
│   │   │   ├── AISystem.h                # AI system manager
│   │   │   ├── BehaviourTree.h           # Hierarchical behaviour tree
│   │   │   ├── BehaviourTreeActions.h    # BT action nodes
│   │   │   ├── Actions.h                 # AI action definitions
│   │   │   ├── DetourNavSystem.h/.cpp    # Recast/Detour NavMesh pathfinding
│   │   │   ├── DetourBuildAPI.h/.cpp     # NavMesh building utilities
│   │   │   ├── NavAgent.h/.cpp           # NavMesh-based path following agent
│   │   │   ├── Grid.h                    # Grid data structure for AI
│   │   │   ├── GridAStar.h               # A* pathfinding on grids
│   │   │   ├── GridChaseAI.h             # Grid-based enemy chase logic
│   │   │   └── GridReverseDjik.h         # Reverse Dijkstra flow fields
│   │   ├── Application/
│   │   │   ├── Application.h             # Main app loop and ECS iteration
│   │   │   ├── Context.h                 # Global engine context
│   │   │   └── Interface.h               # Base AppInterface (OnStart, OnUpdate, OnStop)
│   │   ├── Audio/
│   │   │   ├── Audio.cpp/.hpp            # FMOD integration with 3D spatial audio
│   │   │   ├── SoundSystem.hpp           # Sound component system
│   │   │   └── TrackLibrary.h            # Audio track reference management
│   │   ├── Auxiliaries/
│   │   │   ├── Assets.h                  # Asset loading and reference management
│   │   │   ├── AsyncAssetLoader.h        # Multithreaded asset loading
│   │   │   ├── DataSerializer.h          # Scene serialization/deserialization
│   │   │   ├── PrefabUtility.h           # Prefab creation and instantiation
│   │   │   ├── PropertyAPI.h             # Runtime property access for editor
│   │   │   ├── Profiler.h                # Frame profiling utilities
│   │   │   └── SerializationRegistry.h   # Component serialization registry
│   │   ├── Common/
│   │   │   ├── BoomProperties.h          # Engine property declarations
│   │   │   └── xproperty.h               # C++ reflection system
│   │   ├── ECS/
│   │   │   └── ECS.hpp                   # entt ECS wrapper with all components
│   │   ├── Graphics/
│   │   │   ├── Buffers/
│   │   │   │   ├── Frame.h               # Framebuffer objects
│   │   │   │   ├── Mesh.h                # Mesh buffer management
│   │   │   │   └── Vertex.h              # Vertex layout definitions
│   │   │   ├── Instancing/
│   │   │   │   ├── InstanceManager.h     # GPU instancing manager
│   │   │   │   ├── InstanceBatch.h       # Static instance batching
│   │   │   │   └── AnimatedInstanceBatch.h # Animated instance batching
│   │   │   ├── Models/
│   │   │   │   ├── Animation.h           # Animation clip data
│   │   │   │   ├── AnimationIO.h         # Animation file I/O
│   │   │   │   ├── Animator.h            # State machine animator with blend trees
│   │   │   │   ├── Helper.h              # Model loading helpers
│   │   │   │   └── Model.h               # Model resource management
│   │   │   ├── Shaders/
│   │   │   │   ├── PBR.h                 # Physically Based Rendering shader
│   │   │   │   ├── Shadow.h              # Shadow mapping shader
│   │   │   │   ├── Bloom.h               # Bloom post-processing shader
│   │   │   │   ├── Skybox.h              # Skybox rendering shader
│   │   │   │   ├── SkyMap.h              # Sky map shader
│   │   │   │   ├── DebugLines.h          # Debug wireframe rendering
│   │   │   │   ├── PickingShader.h       # Entity picking for editor
│   │   │   │   ├── Color.h               # Color shaders (2D/3D)
│   │   │   │   ├── Final.h               # Final compositing shader
│   │   │   │   ├── LoadingShader.h       # Loading screen shader
│   │   │   │   ├── LoadingVideoShader.h  # Loading video overlay
│   │   │   │   └── Shader.h              # Base shader program class
│   │   │   ├── Text/
│   │   │   │   └── FontManager.h         # FreeType font rendering system
│   │   │   ├── Textures/
│   │   │   │   ├── Texture.h             # Texture loading and management
│   │   │   │   └── Compression.h         # BC7/BC1 texture compression (Compressonator)
│   │   │   ├── Utilities/
│   │   │   │   ├── Culling.h             # Frustum culling
│   │   │   │   ├── Data.h                # Rendering data structures
│   │   │   │   ├── Quad.h                # Screen quad utility
│   │   │   │   └── Skybox.h              # Skybox mesh utility
│   │   │   ├── Video/
│   │   │   │   ├── VideoPlayer.h         # MPEG-1 video decoding (pl_mpeg)
│   │   │   │   ├── VideoSystem.h         # Video component ECS integration
│   │   │   │   └── pl_mpeg.h             # MPEG-1 decoder library
│   │   │   └── Renderer.h               # Main rendering pipeline
│   │   ├── Input/
│   │   │   ├── InputHandler.h            # Keyboard, mouse, and gamepad input
│   │   │   ├── CameraManager.h           # Camera management
│   │   │   └── RayCast.h                # Physics raycasting
│   │   ├── Physics/
│   │   │   ├── Callback.h               # PhysX contact/trigger callbacks
│   │   │   ├── Context.h                # Physics world simulation
│   │   │   ├── Helpers.h                # Physics math utilities
│   │   │   └── Utilities.h              # Shapes, cooking, filtering
│   │   ├── Scripting/
│   │   │   ├── MonoRuntime.h            # Mono C# runtime embedding
│   │   │   ├── ScriptingSystem.h        # Script lifecycle management
│   │   │   ├── ScriptBinding.h          # C++ / C# interop bindings
│   │   │   └── FileWatcher.h            # Hot-reload file watcher
│   │   ├── AppWindow.h                  # Window creation and management
│   │   ├── BoomEngine.h                 # Main engine export header
│   │   └── GlobalConstants.h            # Engine constants
│   ├── src/                             # Engine source implementations
│   ├── Vendors/                         # Detour/DetourCrowd/DetourTileCache/DebugUtils
│   └── CMakeLists.txt                   # Engine build configuration

├── Editor/                              # ImGui-based in-engine editor
│   ├── Resources/
│   │   ├── Animations/                  # Animation clip files (.anim)
│   │   ├── Audio/                       # Audio assets
│   │   ├── Fonts/                       # Font resources
│   │   ├── Models/                      # 3D model assets
│   │   ├── Shaders/                     # GLSL shader programs
│   │   └── Textures/                    # Texture assets
│   ├── Scenes/                          # Scene YAML files
│   ├── src/
│   │   ├── Commands/
│   │   │   └── UndoRedo.h               # Undo/Redo command system
│   │   ├── Context/                     # Editor context, inputs, widgets
│   │   ├── Panels/
│   │   │   ├── AnimationTimelinePanel   # Animation timeline editor
│   │   │   ├── AnimatorGraphPanel       # Animator state machine editor
│   │   │   ├── AudioPanel               # Audio playback panel
│   │   │   ├── ConsolePanel             # Log output panel
│   │   │   ├── DirectoryPanel           # Asset directory browser
│   │   │   ├── HierarchyPanel           # Scene entity tree
│   │   │   ├── Inspector/               # Component property editor
│   │   │   ├── MenuBarPanel             # File/Edit/Tools menus
│   │   │   ├── ModelPreviewPanel        # 3D model preview
│   │   │   ├── NavMeshPanel             # NavMesh visualization and baking
│   │   │   ├── PerformancePanel         # Performance metrics
│   │   │   ├── PlaybackControlsPanel    # Play/Pause/Step controls
│   │   │   ├── PrefabBrowserPanel       # Prefab management
│   │   │   ├── ResourcePanel            # Resource browser
│   │   │   └── ViewportPanel            # 3D scene viewport
│   │   ├── Recast/                      # NavMesh baking (Recast)
│   │   └── Vendors/                     # ImGui, ImGuizmo, Recast
│   └── imgui.ini                        # Window layout

└── Gam300.sln                           # Visual Studio solution

=============================================================================
Features

AI and Navigation System (Recast / Detour)

- NavMesh pathfinding using Recast for baking and Detour for runtime queries.
- NavAgent component for entity-based path following with waypoint navigation.
- A* pathfinding and Reverse Dijkstra flow fields on grid-based maps.
- Behaviour Tree system for hierarchical AI decision-making.
- Vision component with configurable FOV, range, and alert states.
- Proximity detection for radius-based enemy awareness.
- Editor NavMesh Panel for visualization and rebaking.

Animation System

- State machine animator with transitions, conditions, and triggers.
- Blend tree support for smooth locomotion blending.
- Animation clips with playback speed, looping, and layer blending.
- Animation audio events triggered from keyframes.
- Animation I/O system for .anim file loading and saving.
- Editor Animation Timeline Panel for keyframe editing and preview.
- Editor Animator Graph Panel for visual state machine editing.

Rendering System (OpenGL / PBR)

- Physically Based Rendering (PBR) with albedo, normal, metallic,
  roughness, occlusion, and emissive maps.
- Support for up to 512 point lights, 512 directional lights, 512 spot lights.
- Shadow mapping for directional and spot lights (up to 25 shadow maps).
- Bloom post-processing with HDR brightness threshold and Gaussian blur.
- Skybox and Sky Map rendering with cubemap environment mapping.
- Frustum culling for visibility optimization.
- Debug line rendering for wireframe visualization.
- Entity picking shader for editor object selection.
- Final compositing pass for post-processing output.

GPU Instancing

- Instance Manager for batching thousands of static and animated objects.
- Per-material batch grouping with transform SSBOs.
- Animated instance batching with joint matrix SSBOs.
- Transparent instance sorting for correct blending order.

Font and Text Rendering

- FreeType-based TrueType font rendering.
- Font atlas generation with configurable scale and color.
- In-world 3D text component for entity-based text display.
- Dedicated font shader for text rendering.

Video Playback System

- MPEG-1 video decoding using pl_mpeg.
- Playback controls: play, pause, stop, seek, rewind, loop.
- Frame data to OpenGL texture conversion.
- Audio integration with FMOD for video sound.
- Video component for entity-based playback.
- Loading screen video support.

Texture Compression

- BC7/BC1 texture compression using AMD Compressonator.
- DDS format output with configurable quality settings.
- Batch compression for asset pipelines.

Audio System (FMOD)

- Full FMOD Studio API integration with 3D spatial audio.
- Channel groups: Master, Music, SFX, and custom groups.
- Audio properties: pitch, pan, priority, mute, spatial blend.
- 3D attenuation with min/max distance.
- Listener attributes for positional audio.
- Sound component for entity-based audio playback.
- Animation-triggered sound events.
- Editor Audio Panel for real-time testing.

Physics System (PhysX)

- Full NVIDIA PhysX integration for simulation.
- Rigid body support with velocity and force control.
- Contact and trigger callbacks.
- Character Controller (capsule-based) with grounded checks.
- Shapes, materials, and collision filtering.
- Physics raycasting for queries and selection.

Scripting System (Mono / C#)

- Mono runtime embedding for C# script execution.
- Script lifecycle: OnStart, OnUpdate, OnDestroy.
- Hot-reload with file watcher for rapid iteration.
- Editor-exposed fields for inspector editing.
- C++ / C# interop via P/Invoke bindings.
- Full Gamepad API exposed to scripts.

Input System (Keyboard, Mouse, Gamepad)

- Keyboard input with key down, pressed, and released states.
- Mouse tracking with position, delta, and scroll.
- Gamepad support: buttons, analog sticks, triggers.
- Gamepad API: IsGamepadButtonDown, GetGamepadAxis, IsGamepadConnected.
- All buttons mapped: A/B/X/Y, bumpers, D-Pad, thumbsticks, start/back.
- All axes mapped: left/right stick X/Y, left/right trigger.

Controller Support in Gameplay Scripts

- Player movement with left stick, sprint with left trigger, crouch with B.
- Roll with X/Y buttons, freeze time with LB+RB.
- All menus (Main, Pause, Death, End, HowToPlay) navigable with D-Pad and A.
- Volume adjustment with D-Pad left/right in menus.
- Cutscene skip with gamepad buttons.

Gameplay Systems

- Player Movement: walk, sprint, sneak, crouch, roll with cooldown.
- Health system with respawning, invulnerability frames, and fade transitions.
- Freeze Mechanic: time freeze with spatial activation zone and visual overlay.
- Maze Generation: procedural maze using BSP with animated rise/sink phases.
- Cutscene Sequencer: keyframe-based camera animation with .seq file parsing.
- Enemy Controllers: stationary (sentry) and patrolling (NavMesh-based).
- Spotlight System: dynamic enemy spotlights with patrol routes.
- Checkpoint system for save/respawn locations.
- Key pickup and door interaction system.
- Level transition triggers with cutscene support.
- Blood overlay and damage visual effects.
- Contextual footstep audio based on movement speed and stance.

UI and HUD Systems

- Health heart display.
- Key counter display.
- Freeze cooldown indicator.
- Stance indicator.
- Location display.
- Interaction hold indicator.
- Tutorial popup system with trigger zones.
- Blood overlay for damage feedback.

Menu Systems

- Main Menu with play, how-to-play, and exit.
- Pause Menu with resume, settings, and quit.
- Death Menu with retry and quit.
- End Menu for game completion.
- How To Play tutorial screen.
- Volume slider settings.
- All menus support both keyboard and gamepad navigation.

Serialization and Prefab System

- Scene and entity serialization using YAML.
- Component, asset, and property serializers.
- Prefab creation, saving, and instantiation.
- Async asset loader with thread pool for non-blocking loading.
- Property reflection system (xproperty) for runtime access.

Entity Component System (ECS)

- Based on entt with lightweight entity wrapper.
- Components: Transform, Camera, ThirdPersonCamera, Model, Animator,
  RigidBody, Collider, CharacterController, Light (Point/Spot/Directional),
  Skybox, NavAgent, AI, Script, Sprite, Text, Video, Menu, Sound.

Editor Features (ImGui + ImGuizmo)

- Animation Timeline Panel: keyframe editing and animation preview.
- Animator Graph Panel: visual state machine editing.
- NavMesh Panel: NavMesh visualization and rebaking.
- Model Preview Panel: 3D model preview with lighting.
- Playback Controls Panel: play, pause, step for runtime testing.
- Performance Panel: frame time and profiling metrics.
- Undo/Redo system with command pattern.
- Hierarchy Panel: scene entity tree with drag-and-drop.
- Inspector Panel: component property editing with script fields.
- Viewport Panel: 3D scene rendering with ImGuizmo gizmos.
- Console Panel: log output with filtering.
- Directory Panel: asset file browser.
- Resource Panel: resource management.
- Prefab Browser Panel: prefab management and instantiation.
- Menu Bar: scene management (new, save, load, exit).
- Docking layout system with customizable window arrangement.

Build and Toolchain

- CMake as primary build system.
- Dependencies managed through Conan.
- Visual Studio 2022 recommended.
- Mono runtime embedded for C# scripting.
- .NET assembly compilation for GameScripts.

Outputs

- BoomEngine.dll     - Core engine runtime.
- Editor.exe         - ImGui-based editor.
- GameScripts.dll    - Managed C# gameplay assembly.

=============================================================================
Additions from M1 to M4

Engine
- AI and Navigation system (Recast/Detour NavMesh, Behaviour Trees, Grid AI).
- Animation system with state machine, blend trees, and animation I/O.
- PBR rendering pipeline with shadow mapping and bloom post-processing.
- GPU instancing for static and animated models.
- Font rendering system with FreeType.
- Video playback system with MPEG-1 decoding.
- Texture compression with AMD Compressonator (BC7/BC1).
- Frustum culling for rendering optimization.
- Gamepad input system with full button and axis support.
- Async multithreaded asset loading.
- Character controller (PhysX capsule-based).
- Mono runtime for C# scripting (replacing HostFXR).
- Script hot-reload with file watcher.
- Skybox and sky map rendering.
- Loading screen and loading video systems.

Editor
- Animation Timeline Panel.
- Animator Graph Panel.
- NavMesh Panel with baking and visualization.
- Model Preview Panel.
- Playback Controls Panel (play/pause/step).
- Performance Panel.
- Undo/Redo command system.

Gameplay (C# Scripts)
- Full player controller with walk, sprint, crouch, roll, and health.
- Gamepad/controller support across all gameplay and menus.
- Enemy AI with vision cones, proximity detection, and patrol routes.
- Freeze time mechanic with visual overlay.
- Procedural maze generation with animated walls.
- Cutscene sequencer with keyframe camera animation.
- Spotlight system for enemy detection.
- Complete menu suite (Main, Pause, Death, End, HowToPlay).
- HUD system (health, keys, freeze, stance, location).
- Tutorial system with trigger zones and popups.
- Checkpoint and respawn system.
- Key pickup and door interaction.
- Level transitions with cutscene support.
- Footstep audio system with speed and stance variation.
- Blood overlay and damage feedback.
- Settings and volume management.
=============================================================================
