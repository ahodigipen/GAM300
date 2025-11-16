#pragma once
#ifndef APPLICATION_H
#define APPLICATION_H

#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#undef APIENTRY
#include <Windows.h>

#endif

#include "Interface.h"
#include "ECS/ECS.hpp"
#include "Physics/Context.h"
#include "Audio/Audio.hpp"   
#include "Auxiliaries/DataSerializer.h"
#include "Auxiliaries/PrefabUtility.h"
#include "../Graphics/Utilities/Culling.h"
#include "Input/CameraManager.h"
#include "Graphics/Shaders/DebugLines.h"
//#include "../../../Editor/src/Vendors/imgui/imgui.h"
//#include "../../../Editor/src/Vendors/imGuizmo/ImGuizmo.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "Scripting/MonoRuntime.h"
#include "Scripting/ScriptingSystem.h"
#include "Scripting/ScriptBinding.h"

#include "AI/GridChaseAI.h"
#include "AI/DetourNavSystem.h"
#include "AI/NavAgent.h"
namespace Boom {
    BOOM_INLINE entt::entity CreateEnemySphere(entt::registry& reg, const std::string& name,
        const glm::vec3& pos, float radius)
    {
        Entity e{ &reg };
        // Name + transform
        auto& info = e.Attach<InfoComponent>(); info.name = name;
        auto& tr = e.Attach<TransformComponent>().transform;
        tr.translate = pos;
        tr.scale = glm::vec3(radius);   // 1:1 radius if your collider reads scale

        // Visuals (optional if you rely on PhysX debug viz):
        // If you have a sphere model/material, attach ModelComponent here.

        // Physics
        auto& rb = e.Attach<RigidBodyComponent>().RigidBody;
        rb.type = RigidBody3D::STATIC;    // your code already toggles this type elsewhere

        e.Attach<ColliderComponent>();     // collider details are handled by physics on Add/Update
        // (Your Application calls m_Context->physics->AddRigidBody(...) elsewhere when (re)initializing scene.)

        // AI
		e.Attach<NavAgentComponent>().speed = 2.0f;

        return e.ID();
    }
    inline void DecomposeMatrix(const glm::mat4& matrix,
        glm::vec3& translation,
        glm::vec3& rotation,
        glm::vec3& scale)
    {
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::quat orientation;

        glm::decompose(matrix, scale, orientation, translation, skew, perspective);
        rotation = glm::degrees(glm::eulerAngles(orientation));
    }

    inline glm::mat4 RecomposeMatrix(const glm::vec3& translation,
        const glm::vec3& rotation,
        const glm::vec3& scale)
    {
        glm::mat4 matrix = glm::translate(glm::mat4(1.0f), translation);
        matrix *= glm::mat4_cast(glm::quat(glm::radians(rotation)));
        matrix = glm::scale(matrix, scale);
        return matrix;
    }
   
}

namespace Boom
{
    /**
     * @enum ApplicationState
     * @brief Defines the current state of the application
     */
    enum class ApplicationState
    {
        RUNNING,
        PAUSED,
        STOPPED
    };
   
    /**
     * @class Application
     * @brief Core application that owns the context and drives all layers.
     *
     * Inherits from AppInterface to receive the same lifecycle hooks
     * and gain access to the shared AppContext.
     */
    struct Application : AppInterface
    {
        template<typename EntityType, typename... Components, typename Fn>
        BOOM_INLINE void EnttView(Fn&& fn) {
            auto view = m_Context->scene.view<Components...>();
            for (auto e : view) {
                fn(EntityType{ &m_Context->scene, e }, m_Context->scene.get<Components>(e)...);
            }
        }


        // Application state management
        ApplicationState m_AppState = ApplicationState::RUNNING;
        double m_PausedTime = 0.0;  // Track time spent paused
        double m_LastPauseTime = 0.0;  // When the last pause started
        bool m_ShouldExit = false;  // Flag for graceful shutdown
        float m_TestRot = 0.0f;

        bool m_PhysDebugViz = true;

        // --- Mono State ---
        MonoDomain* m_MonoRootDomain = nullptr;
        MonoDomain* m_MonoAppDomain = nullptr;
        MonoAssembly* m_GameAssembly = nullptr;
        MonoImage* m_GameImage = nullptr;
        std::string      m_MonoBase;       // e.g. "<EditorRoot>/Mono"
        std::string      m_AssembliesPath; // e.g. "<EditorRoot>/Scripts/bin/x64/Debug"


        // Temporary for showing physics
        double m_SphereTimer = 0.0;
        double m_SphereResetInterval = 5.0;
        glm::vec3 m_SphereInitialPosition = { 2.5f, 1.2f, 0.0f };

        /**
         * @brief Constructs the Application, assigns its unique ID, and allocates the AppContext.
         *
         * BOOM_INLINE hints to the compiler to inline this small constructor
         * to avoid function-call overhead during startup.
         */
        BOOM_INLINE Application()
        {
            m_LayerID = TypeID<Application>();
            m_Context = new AppContext();
            RegisterEventCallbacks();

            AttachCallback<WindowResizeEvent>([this](auto e) {
                m_Context->renderer->Resize(e.width, e.height);
                }
            );

            AttachCallback<WindowTitleRenameEvent>([this](auto e) {
                m_Context->window->SetWindowTitle(e.title);
                }
            );

        }

        /**
         * @brief Destructor that frees the AppContext.
         *
         * BOOM_INLINE here helps keep the size of the vtable cleanup small
         * by inlining the delete call.
         */
        BOOM_INLINE ~Application()
        {
            DestroyPhysicsActors();
            ShutdownMonoRuntime();
            BOOM_DELETE(m_Context);
            //called here in case of the need of multiple windows
            glfwTerminate();
        }

        /**
        * @brief Pauses the application (stops updates but continues rendering)
        */
        BOOM_INLINE void Pause()
        {
            if (m_AppState == ApplicationState::RUNNING) {
                m_AppState = ApplicationState::PAUSED;
                m_LastPauseTime = glfwGetTime();
                BOOM_INFO("[Application] Paused");

                // Pause audio if available
            }
        }

        /**
         * @brief Resumes the application from pause
         */
        BOOM_INLINE void Resume()
        {
            if (m_AppState == ApplicationState::PAUSED) {
                m_AppState = ApplicationState::RUNNING;

                // Add paused time to total paused time
                m_PausedTime += (glfwGetTime() - m_LastPauseTime);

                BOOM_INFO("[Application] Resumed");

                // Resume audio if available
            }
        }

        /**
         * @brief Stops the application gracefully
         */
        BOOM_INLINE void Stop()
        {
            m_AppState = ApplicationState::STOPPED;
            m_ShouldExit = true;
            BOOM_INFO("[Application] Stopping application...");

            // Stop audio if available
        }

        /**
         * @brief Toggles between pause and resume
         */
        BOOM_INLINE void TogglePause()
        {
            if (m_AppState == ApplicationState::RUNNING) {
                Pause();
            }
            else if (m_AppState == ApplicationState::PAUSED) {
                Resume();
            }
        }

        /**
         * @brief Gets the current application state
         */
        BOOM_INLINE ApplicationState GetState() const { return m_AppState; }

        /**
         * @brief Gets the adjusted time (excluding paused time)
         */
        BOOM_INLINE double GetAdjustedTime() const
        {
            double currentTime = glfwGetTime();
            double adjustedPausedTime = m_PausedTime;

            // If currently paused, add the current pause duration
            if (m_AppState == ApplicationState::PAUSED) {
                adjustedPausedTime += (currentTime - m_LastPauseTime);
            }

            return currentTime - adjustedPausedTime;
        }


        /**
         * @brief Runs the main loop, calling OnUpdate() on every attached layer.
         *
         * BOOM_INLINE suggests inlining this hot-path entry point so
         * the call to RunContext itself adds minimal overhead.
         */
        BOOM_INLINE void RunContext(bool showFrame = false)
        {
            BOOM_INFO("[Application] RunContext started");
            LoadScene("level");
            

            // -- LOADING in MONO --
            const std::string exeDir = GetExeDir();
            

            std::filesystem::path repoRoot = std::filesystem::path(exeDir)
                .parent_path()  // Debug -> x64
                .parent_path()  // x64 -> Gam300
                .parent_path(); // Gam300 -> GAM300

            const std::string monoBase = (repoRoot / "mono").string();
#if defined(_DEBUG)
            
            const std::string asmDir = (repoRoot / "Gam300" / "GameScripts" / "bin" / "x64" / "Debug").string();
#else
            const std::string asmDir = (repoRoot / "Gam300" / "GameScripts" / "bin" / "x64" / "Release").string();
#endif

            if (!InitMonoRuntime(monoBase, asmDir, "BoomDomain"))
            {
#ifdef _DEBUG
                BOOM_ERROR("[Scripting] Failed to initialize Mono runtime!");
#endif // DEBUG
            }

            else
            {
                RegisterScriptInternalCalls(m_Context);
                if (!LoadGameAssembly("GameScripts.dll"))
                {
#ifdef _DEBUG
                    BOOM_ERROR("[Scripting] Failed to load GameScripts.dll");

#endif // DEBUG

                }
                else
                {
  
                    InvokeStaticVoid("GameScripts", "Entry", "Start", nullptr);
#ifdef _DEBUG
                    BOOM_INFO("[Scripting] GameScripts entry invoked.");
#endif // DEBUG

                }
            }
            // --- END MONO INITIALIZE ---

            InitNavRuntime();
			EnsureNinjaSeeksSamurai();
            CameraController camera(
                m_Context->window.get()
            );

            ////init skybox
            EnttView<Entity, SkyboxComponent>([this](auto, auto& comp) {
                SkyboxAsset& skybox{ m_Context->assets->Get<SkyboxAsset>(comp.skyboxID) };
                m_Context->renderer->InitSkybox(skybox.data, skybox.envMap, skybox.size);
                });

            m_DebugLinesShader = std::make_unique<Boom::DebugLinesShader>("debug_lines.glsl");
            m_Context->physics->EnableDebugVisualization(m_PhysDebugViz, 1.0f);

            //temp input for mouse motion
            glm::dvec2 curMP{};
            glm::dvec2 prevMP{};
            while (m_Context->window->PollEvents() && !m_ShouldExit)
            {
                std::shared_ptr<GLFWwindow> engineWindow = m_Context->window->Handle();
                SoundEngine::Instance().Update();
                Camera3D* activeCam = nullptr;
                EnttView<Entity, CameraComponent>([&](auto, CameraComponent& comp) {
                    if (!activeCam && comp.camera.cameraType == Camera3D::CameraType::Main)
                        activeCam = &comp.camera;
                    });
                if (!activeCam) { // fallback: first camera
                    EnttView<Entity, CameraComponent>([&](auto, CameraComponent& comp) {
                        if (!activeCam) activeCam = &comp.camera;
                        });
                }

                // 2) Attach BEFORE update so scroll/pan use this camera's FOV in this frame
                if (activeCam) camera.attachCamera(activeCam);
                glfwMakeContextCurrent(engineWindow.get());

                // NEW: runtime toggle with F9
                {
                    static bool prevF9 = false;
                    bool f9Pressed = glfwGetKey(engineWindow.get(), GLFW_KEY_F9) == GLFW_PRESS;
                    if (f9Pressed && !prevF9)   
                    {
                        m_PhysDebugViz = !m_PhysDebugViz;
                        m_Context->physics->EnableDebugVisualization(m_PhysDebugViz, 1.0f);
                        BOOM_INFO("[PhysX] Debug visualization: {}", m_PhysDebugViz ? "ON" : "OFF");
                    }
                    prevF9 = f9Pressed;
                }

                //   F11 for testing change of rigid body type
                {
                    static bool prevF11 = false;
                    bool f11Pressed = glfwGetKey(engineWindow.get(), GLFW_KEY_F11) == GLFW_PRESS;
                    if (f11Pressed && !prevF11)
                    {
                        // Find the "Sphere" entity to toggle its type
                        EnttView<Entity, InfoComponent, RigidBodyComponent>([this](auto entity, InfoComponent& info, RigidBodyComponent& rb) {
                            if (info.name == "Sphere") {
                                // Determine the new type by flipping the current one
                                RigidBody3D::Type currentType = rb.RigidBody.type;
                                RigidBody3D::Type newType = (currentType == RigidBody3D::DYNAMIC)
                                    ? RigidBody3D::STATIC
                                    : RigidBody3D::DYNAMIC;

                                // Call the function to perform the switch!
                                m_Context->physics->SetRigidBodyType(entity, newType);

                                // Log the change to the console for confirmation
                                BOOM_INFO("[Test F11] Toggled Sphere rigid body to: {}", (newType == RigidBody3D::DYNAMIC) ? "DYNAMIC" : "STATIC");
                            }
                            });
                    }
                    prevF11 = f11Pressed;
                }


                // Always update delta time, but adjust for pause state
                ComputeFrameDeltaTime();
                InvokeStatic1Float("GameScripts", "Entry", "Update", static_cast<float>(m_Context->DeltaTime));

                if (m_Nav) {
                    m_NavAgents.update(m_Context->scene, static_cast<float>(m_Context->DeltaTime), *m_Nav);
                }
                // Animation testing controls
                {
                    // Press L to load additional animations
                    static bool lastLPressed = false;
                    bool lPressed = glfwGetKey(engineWindow.get(), GLFW_KEY_L) == GLFW_PRESS;
                    if (lPressed && !lastLPressed) {
                        EnttView<Entity, AnimatorComponent>([this]([[maybe_unused]] auto entity, auto& animComp) {

                            auto& animator = animComp.animator;


                            BOOM_INFO("=== Loading additional animations ===");
                            size_t beforeCount = animator->GetClipCount();

                            // Try to load these - they need to exist in your Models folder!
                            animator->LoadAnimationFromFile("Resources/Models/idle.fbx", "Idle");
                            animator->LoadAnimationFromFile("Resources/Models/walking.fbx", "Walk");
                            animator->LoadAnimationFromFile("Resources/Models/run.fbx", "Run");

                            size_t afterCount = animator->GetClipCount();
                            BOOM_WARN("Loaded {} new animations (total: {})", afterCount - beforeCount, afterCount);

                            // List all animations
                            for (size_t i = 0; i < animator->GetClipCount(); ++i) {
                                const auto* clip = animator->GetClip(i);
                                if (clip) {
                                    BOOM_INFO("  [{}] '{}' - {:.2f}s", i, clip->name, clip->duration);
                                }
                            }
                            });
                    }
                    lastLPressed = lPressed;

                    // Press 1-9 to switch animations
                    EnttView<Entity, AnimatorComponent>([this, &engineWindow]([[maybe_unused]]auto entity, auto& animComp) {

                        auto& animator = animComp.animator;

                        if (glfwGetKey(engineWindow.get(), GLFW_KEY_1) == GLFW_PRESS && animator->GetClipCount() > 0) {
                            animator->PlayClip(0);
#ifdef DEBUG
                            const auto* clip = animator->GetClip(0);
                            if (clip) BOOM_INFO("Switched to [0]: '{}'", clip->name);
#endif // DEBUG

                        }
                        if (glfwGetKey(engineWindow.get(), GLFW_KEY_2) == GLFW_PRESS && animator->GetClipCount() > 1) {
                            animator->PlayClip(1);

#ifdef DEBUG
                            const auto* clip = animator->GetClip(1);
                            if (clip) BOOM_INFO("Switched to [1]: '{}'", clip->name);
#endif // DEBUG

                        }
                        if (glfwGetKey(engineWindow.get(), GLFW_KEY_3) == GLFW_PRESS && animator->GetClipCount() > 2) {
                            animator->PlayClip(2);

#ifdef DEBUG
                            const auto* clip = animator->GetClip(2);
                            if (clip) BOOM_INFO("Switched to [2]: '{}'", clip->name);
#endif // DEBUG
                        }
                        if (glfwGetKey(engineWindow.get(), GLFW_KEY_4) == GLFW_PRESS && animator->GetClipCount() > 3) {
                            animator->PlayClip(3);
#ifdef DEBUG
                            const auto* clip = animator->GetClip(3);
                            if (clip) BOOM_INFO("Switched to [3]: '{}'", clip->name);
#endif // DEBUG
                        }

                        // Press I for info about current animation
                        static bool lastIPressed = false;
                        bool iPressed = glfwGetKey(engineWindow.get(), GLFW_KEY_I) == GLFW_PRESS;
                        if (iPressed && !lastIPressed) {
                            BOOM_INFO("=== Current Animation Info ===");
                            BOOM_INFO("Current Clip: {} / {}", animator->GetCurrentClip(), animator->GetClipCount() - 1);
                            const auto* clip = animator->GetClip(animator->GetCurrentClip());
                            if (clip) {
#ifdef DEBUG

                                BOOM_INFO("  Name: '{}'", clip->name);
                                BOOM_INFO("  Duration: {:.2f}s", clip->duration);
                                BOOM_INFO("  Current Time: {:.2f}s", animator->GetTime());
                                BOOM_INFO("  Tracks: {}", clip->tracks.size());
#endif // DEBUG

                            }
                        }
                        lastIPressed = iPressed;
                        });
                }


                // ============ END NEW SECTION ============
                m_Context->profiler.BeginFrame();
                m_Context->profiler.Start("Total Frame");
                m_Context->profiler.Start("Renderer Start Frame");
                std::apply(glClearColor, CONSTANTS::DEFAULT_BACKGROUND_COLOR);
                RenderShadowScene();
                m_Context->renderer->NewFrame();
                m_Context->profiler.End("Renderer Start Frame");

                // Only update rotation when running
                if (m_AppState == ApplicationState::RUNNING) {
                    EnttView<Entity, RigidBodyComponent>([](auto, RigidBodyComponent& rb) {
                        rb.RigidBody.isColliding = false;
                        });
                    UpdateStaticTransforms();
                    RunPhysicsSimulation();
                    InitNavRuntime();
                    UpdateThirdPersonCameras();
                }
                
                m_SphereTimer += m_Context->DeltaTime;
                if (m_SphereTimer >= m_SphereResetInterval) {
                    ResetAllSpheres();
                    ResetSphere();
                    m_SphereTimer = 0.0;
                }

                LightsUpdate();

                //temp input for mouse motion
                glfwGetCursorPos(m_Context->window->Handle().get(), &curMP.x, &curMP.y);
                // ONLY update the flycam controller if the game is PAUSED
                if (m_AppState != ApplicationState::RUNNING) {
                    camera.update(static_cast<float>(m_Context->DeltaTime));
                }

                glm::mat4 dbgView(1.0f);
                glm::mat4 dbgProj(1.0f);
                glm::vec3 dbgCamPos(0.0f);

                EnttView<Entity, CameraComponent>([this, &curMP, &prevMP, &dbgView, &dbgProj, &dbgCamPos](auto entity, CameraComponent& comp) {
                    Transform3D& transform{ entity.template Get<TransformComponent>().transform };

                    // ONLY apply flycam logic if the game is PAUSED
                    if (m_AppState != ApplicationState::RUNNING)
                    {
                        // This is the flycam logic, only run when not playing
                        transform.rotate.x += m_Context->window->camRot.x;
                        transform.rotate.y += m_Context->window->camRot.y;
                        glm::quat quat{ glm::radians(transform.rotate) };
                        glm::vec3 dir{ quat * m_Context->window->camMoveDir };
                        transform.translate += dir;

                        if (curMP == prevMP) {
                            m_Context->window->camRot = {};
                            if (m_Context->window->isMiddleClickDown)
                                m_Context->window->camMoveDir = {};
                        }
                    }

                    // This part is needed by BOTH cameras, so leave it outside the 'if'
                    m_Context->renderer->SetCamera(comp.camera, transform);
                    dbgView = comp.camera.View(transform);
                    dbgProj = comp.camera.Projection(m_Context->renderer->Aspect());
                    dbgCamPos = transform.translate;
                    });
                {
                    prevMP = curMP;

                    EnttView<Entity, TransformComponent, RigidBodyComponent>([this](auto entity, TransformComponent& tc, RigidBodyComponent& rbc) {
                        // Check if the current scale is different from the stored scale
                        if (tc.transform.scale != rbc.RigidBody.previousScale)
                        {
                            // If it changed, update the collider shape
                            m_Context->physics->UpdateColliderShape(entity, GetAssetRegistry());

                            // Then, update the stored scale to the new value for the next frame
                            rbc.RigidBody.previousScale = tc.transform.scale;
                        }
                        });
                }
               
                RenderScene();
                if (m_PhysDebugViz && m_DebugLinesShader)
                {
                    m_Context->physics->CollectDebugLines(m_PhysLinesCPU);
                    if (!m_PhysLinesCPU.empty())
                    {
                        // Build CPU line list
                        std::vector<Boom::LineVert> lineVerts;
                        lineVerts.reserve(m_PhysLinesCPU.size() * 2);
                        for (const auto& l : m_PhysLinesCPU)
                        {
                            lineVerts.push_back(Boom::LineVert{ l.p0, l.c0 });
                            lineVerts.push_back(Boom::LineVert{ l.p1, l.c1 });
                        }

                        // Cull any segments within a small radius of the camera (fix “ball in face”)
                        std::vector<Boom::LineVert> filtered;
                        filtered.reserve(lineVerts.size());
                        const float camCullRadius = 0.6f;

                        for (size_t i = 0; i + 1 < lineVerts.size(); i += 2)
                        {
                            const auto& a = lineVerts[i + 0];
                            const auto& b = lineVerts[i + 1];

                            const float segDist = DistancePointSegment(dbgCamPos, a.pos, b.pos);
                            const float endA = glm::distance(a.pos, dbgCamPos);
                            const float endB = glm::distance(b.pos, dbgCamPos);

                            // Keep only segments not near the camera (endpoints and segment body)
                            if (segDist >= camCullRadius && endA >= camCullRadius && endB >= camCullRadius)
                            {
                                filtered.push_back(a);
                                filtered.push_back(b);
                            }
                        }

                        if (!filtered.empty())
                            m_DebugLinesShader->Draw(dbgView, dbgProj, filtered, 50.5f);
                    }
                }
                if (m_PhysDebugViz && m_DebugLinesShader)
                {
                    DrawRigidBodiesDebugOnly(dbgView, dbgProj);
                }
                if (m_Context->ShowNavDebug && m_DebugLinesShader && m_Nav) {
                    // Draw navmesh edges & centroids near the camera. Tweak radius to taste.
                    const float navDrawRadius = 60.0f; // try 40–100 to see more/less
                    m_Nav->DrawDetourNavMesh_Query(*m_DebugLinesShader, dbgView, dbgProj, dbgCamPos, navDrawRadius);
                }
                
                //skybox ecs (should be drawn at the end)
                EnttView<Entity, SkyboxComponent>([this](auto entity, SkyboxComponent& comp) {
                    Transform3D& transform{ entity.template Get<TransformComponent>().transform };
                    SkyboxAsset& skybox{ m_Context->assets->Get<SkyboxAsset>(comp.skyboxID) };
                    m_Context->renderer->DrawSkybox(skybox.data, transform);
                    });
                
                m_Context->profiler.Start("Renderer End Frame");
                m_Context->renderer->EndFrame();
                m_Context->profiler.End("Renderer End Frame");

                //draw the updated frame
                m_Context->renderer->ShowFrame(showFrame);


                for (auto layer : m_Context->layers)
                {
                    layer->OnUpdate();
                }

                m_Context->profiler.End("Total Frame");
                m_Context->profiler.EndFrame();
            }
        }

        /**
        * 
         * @brief Saves the current scene and assets to files
         * @param sceneName The name of the scene (without extension)
         * @param scenePath Optional custom path for scene files (defaults to "Scenes/")
         * @return true if save was successful, false otherwise
         */
        BOOM_INLINE bool SaveScene(const std::string& sceneName, const std::string& scenePath = "Scenes/")
        {
            //Try blocks cause crashed in release mode. Need to find new alternative
            DataSerializer serializer;

            const std::string sceneFilePath = scenePath + sceneName + ".yaml";
            //const std::string assetsFilePath = scenePath + sceneName + "_assets.yaml";

            BOOM_INFO("[Scene] Saving scene '{}' to '{}'", sceneName, sceneFilePath);

            // Serialize scene and assets
            serializer.Serialize(m_Context->scene, sceneFilePath);
            //serializer.Serialize(*m_Context->assets, assetsFilePath);

            // Update current scene tracking
            strncpy_s(m_CurrentScenePath, sizeof(m_CurrentScenePath), sceneFilePath.c_str(), _TRUNCATE);

            BOOM_INFO("[Scene] Successfully saved scene '{}' and assets", sceneName);
            return true;
        }

        /**
         * @brief Loads a scene from files, replacing the current scene
         * @param sceneName The name of the scene (without extension)
         * @param scenePath Optional custom path for scene files (defaults to "Scenes/")
         * @return true if load was successful, false otherwise
         */
        BOOM_INLINE bool LoadScene(const std::string& sceneName, const std::string& scenePath = "Scenes/")
        {
            DataSerializer serializer;

            const std::string sceneFilePath = scenePath + sceneName + ".yaml";
            //const std::string assetsFilePath = scenePath + sceneName + "_assets.yaml";

            BOOM_INFO("[Scene] Loading scene '{}' from '{}'", sceneName, sceneFilePath);

            // Clean up current scene
            CleanupCurrentScene();

            // Load assets first
            //BOOM_INFO("[Scene] Loading assets...");
            //serializer.Deserialize(*m_Context->assets, assetsFilePath);

            // Then load scene
            BOOM_INFO("[Scene] Loading scene data...");
            serializer.Deserialize(m_Context->scene, *m_Context->assets, sceneFilePath);

            // Update tracking
            strncpy_s(m_CurrentScenePath, sizeof(m_CurrentScenePath), sceneFilePath.c_str(), _TRUNCATE);
            m_SceneLoaded = true;

            // Reinitialize systems that need it
            ReinitializeSceneSystems();

            BOOM_INFO("[Scene] Successfully loaded scene '{}'", sceneName);
            return true;
        }

        BOOM_INLINE void LightsUpdate() {
            {
                int points = 0;
                EnttView<Entity, PointLightComponent, TransformComponent>(
                    [this, &points](auto, PointLightComponent& plc, TransformComponent& tc)
                    {
                        m_Context->renderer->SetLight(plc.light, tc.transform, points++);
                    });
                m_Context->renderer->SetPointLightCount(points);
            }
            {
                int directs = 0;
                EnttView<Entity, DirectLightComponent, TransformComponent>(
                    [this, &directs](auto, DirectLightComponent& dlc, TransformComponent& tc)
                    {
                        m_Context->renderer->SetLight(dlc.light, tc.transform, directs++);
                    });
                m_Context->renderer->SetDirectionalLightCount(directs);
            }
            {
                int spots = 0;
                EnttView<Entity, SpotLightComponent, TransformComponent>(
                    [this, &spots](auto, SpotLightComponent& slc, TransformComponent& tc)
                    {
                        m_Context->renderer->SetLight(slc.light, tc.transform, spots++);
                    });
                m_Context->renderer->SetSpotLightCount(spots);
            }
        }
        BOOM_INLINE void RenderShadowScene() {
            //building shadows
            EnttView<Entity, DirectLightComponent, TransformComponent>(
                [this](auto, DirectLightComponent&, TransformComponent& tc)
                {
                    // light direction
                    auto& lightDir = tc.transform.rotate;
                    m_Context->renderer->BeginShadowPass(lightDir);

                    EnttView<Entity, ModelComponent>([this](auto entity, ModelComponent& comp) {
                        //ignore lights and non initialized models
                        if (!entity.Has<ModelComponent>() || comp.modelID == EMPTY_ASSET) return;
                        if (entity.Has<DirectLightComponent>() || entity.Has<PointLightComponent>() || entity.Has<SpotLightComponent>()) return;

                        ModelAsset& model{ m_Context->assets->Get<ModelAsset>(comp.modelID) };
                        std::vector<glm::mat4> joints;
                        if (entity.Has<AnimatorComponent>()) {
                            auto& an = entity.Get<AnimatorComponent>();
                            joints = an.animator->Animate(0); //dont update animation here
                        }
                        else if (model.hasJoints) {
                            static std::vector<glm::mat4> identityPalette(100, glm::mat4(1.0f));
                            joints = identityPalette;
                        }

                        glm::mat4 worldMatrix = GetWorldMatrix(entity);
                        Transform3D worldTransform;
                        DecomposeMatrix(worldMatrix, worldTransform.translate, worldTransform.rotate, worldTransform.scale);

                        m_Context->renderer->DrawShadow(model.data, worldTransform, joints);
                    });
            
                    m_Context->renderer->EndShadowPass();
                });
        }
        
        BOOM_INLINE void RenderScene() {
            //pbr ecs (always render)
            EnttView<Entity, ModelComponent>([this](auto entity, ModelComponent& comp) {
                if (!entity.Has<ModelComponent>() || comp.modelID == EMPTY_ASSET) {
                    //BOOM_ERROR("[Render] Model data is null for ModelID: {} ({})", comp.modelID, comp.modelName);
                    return; // Skip rendering this model
                }

                ModelAsset& model{ m_Context->assets->Get<ModelAsset>(comp.modelID) };
                if (entity.Has<AnimatorComponent>()) {
                    auto& an = entity.Get<AnimatorComponent>();
                    float dt = (m_AppState == ApplicationState::RUNNING) ? (float)m_Context->DeltaTime : 0.0f;
                    auto& joints = an.animator->Animate(dt);
                    m_Context->renderer->SetJoints(joints);           // existing
                }
                else {
                    // NEW: ensure no stale palette leaks into this draw
                    if (model.hasJoints)
                    {
                        static std::vector<glm::mat4> identityPalette(100, glm::mat4(1.0f));
                        m_Context->renderer->SetJoints(identityPalette);
                    }
                }

                glm::mat4 worldMatrix = GetWorldMatrix(entity);
                Transform3D worldTransform;
                DecomposeMatrix(worldMatrix, worldTransform.translate, worldTransform.rotate, worldTransform.scale);

                //draw model with material if it has one otherwise draw default material
                if (comp.materialID != EMPTY_ASSET) {
                    auto& material{ m_Context->assets->Get<MaterialAsset>(comp.materialID) };

                    // Only assign textures if they exist and are valid
                    if (material.albedoMapID != EMPTY_ASSET) {
                        auto& albedoTex = m_Context->assets->Get<TextureAsset>(material.albedoMapID);
                        if (albedoTex.data) {
                            material.data.albedoMap = albedoTex.data;
                        }
                    }
                    if (material.normalMapID != EMPTY_ASSET) {
                        auto& normalTex = m_Context->assets->Get<TextureAsset>(material.normalMapID);
                        if (normalTex.data) {
                            material.data.normalMap = normalTex.data;
                        }
                    }
                    if (material.roughnessMapID != EMPTY_ASSET) {
                        auto& roughnessTex = m_Context->assets->Get<TextureAsset>(material.roughnessMapID);
                        if (roughnessTex.data) {
                            material.data.roughnessMap = roughnessTex.data;
                        }
                    }

                    m_Context->renderer->Draw(model.data, worldTransform, material.data);
                }
                else {
                    m_Context->renderer->Draw(model.data, worldTransform);
                }
                });
        }

        /**
         * @brief Creates a new empty scene
         * @param sceneName Optional name for the new scene
         */
        BOOM_INLINE void NewScene(const std::string& sceneName = "NewScene")
        {
            BOOM_INFO("[Scene] Creating new scene '{}'", sceneName);

            // Clean up current scene
            //CleanupCurrentScene();

            // Create basic scene with camera
            //CreateDefaultScene();

			LoadScene("templateScene");

            m_CurrentScenePath[0] = '\0'; // Clear the path
            m_SceneLoaded = false;

            BOOM_INFO("[Scene] New scene '{}' created", sceneName);
        }

        /**
         * @brief Gets the current scene file path
         */
        BOOM_INLINE std::string GetCurrentScenePath() const { return std::string(m_CurrentScenePath); }

        /**
         * @brief Checks if a scene is currently loaded
         */
        BOOM_INLINE bool IsSceneLoaded() const { return m_SceneLoaded; }

        BOOM_INLINE glm::mat4 GetWorldMatrix(Entity& entity)
        {
            // 1. Get this entity's local matrix (e.g., the visual model's transform)
            glm::mat4 localMatrix(1.0f);
            if (entity.Has<TransformComponent>()) {
                localMatrix = entity.Get<TransformComponent>().transform.Matrix();
            }

            // 2. Get the parent's world matrix (e.g., the physics body's transform)
            glm::mat4 pMatrix(1.0f);
            if (entity.Has<InfoComponent>()) {
                uint64_t parentUID = entity.Get<InfoComponent>().parent;

                if (parentUID != 0) // 0 means root/no parent
                {
                    entt::entity parentEnttID = entt::null;
                    auto view = m_Context->scene.view<InfoComponent>();
                    for (auto e : view) {
                        if (view.get<InfoComponent>(e).uid == parentUID) {
                            parentEnttID = e;
                            break;
                        }
                    }

                    if (parentEnttID != entt::null) {
                        Entity parentEntity{ &m_Context->scene, parentEnttID };
                        pMatrix = GetWorldMatrix(parentEntity); // Recurse
                    }
                }
            }

            // 3. Decompose the parent's matrix into T, R, and S
            glm::vec3 pTranslate, pRotate, pScale;
            DecomposeMatrix(pMatrix, pTranslate, pRotate, pScale);


            // 4. Recompose the parent's matrix *without* its scale
            glm::mat4 pMatrix_NoScale;
            pMatrix_NoScale = RecomposeMatrix(pTranslate, pRotate, glm::vec3(1.0f));

            // 5. Return the parent's (T*R) multiplied by the child's (T*R*S)
            return pMatrix_NoScale * localMatrix;
        }

        BOOM_INLINE void UpdateStaticTransforms()
        {
            EnttView<Entity, RigidBodyComponent>(
                [this](auto entity, RigidBodyComponent& rb)
                {
                    if (rb.RigidBody.type == RigidBody3D::Type::STATIC)
                    {
                        auto* actor = rb.RigidBody.actor;
                        if (!actor) return;

                        // Get the FINAL world matrix of the physics body,
                        // no matter how deep it is in the hierarchy.
                        glm::mat4 worldMatrix = GetWorldMatrix(entity);

                        // Decompose the world matrix to get the final T and R
                        glm::vec3 worldTranslate, worldRotate, worldScale;
                        DecomposeMatrix(worldMatrix, worldTranslate, worldRotate, worldScale);
                        // --- END FIX ---

                        physx::PxTransform currentPose = actor->getGlobalPose();

                        // Convert new world transform to PhysX
                        glm::quat rotQuat = glm::quat(glm::radians(worldRotate));

                        physx::PxVec3 newPos(worldTranslate.x, worldTranslate.y, worldTranslate.z);
                        physx::PxQuat newRot(rotQuat.x, rotQuat.y, rotQuat.z, rotQuat.w);

                        if (currentPose.p != newPos || currentPose.q != newRot)
                        {
                            actor->setGlobalPose(physx::PxTransform(newPos, newRot));
                        }
                    }
                });
        }
        public:
        BOOM_INLINE static void AppendLine(std::vector<Boom::LineVert>& out,
            const glm::vec3& a, const glm::vec3& b,
            const glm::vec4& cA, const glm::vec4& cB)
        {
            out.push_back(Boom::LineVert{ a, cA });
            out.push_back(Boom::LineVert{ b, cB });
        }
        BOOM_INLINE DetourNavSystem* GetNavSystem() override { return m_Nav.get(); }
        BOOM_INLINE const DetourNavSystem* GetNavSystem() const override { return m_Nav.get(); }
    private:
        std::unordered_map<std::string, std::pair<glm::vec3, glm::vec3>> m_SphereInitialStates;

        BOOM_INLINE void RegisterSphereInitialState(const std::string& name,
            const glm::vec3& pos,
            const glm::vec3& vel = glm::vec3(0.0f)) {
            m_SphereInitialStates[name] = { pos, vel };
        }
		bool m_NavInitialized = false;
        std::unique_ptr<Boom::DebugLinesShader> m_DebugLinesShader;
        std::vector<Boom::PhysicsContext::DebugLine> m_PhysLinesCPU;
        char m_CurrentScenePath[512] = "\0";
        bool m_SceneLoaded = false;
        std::unique_ptr<DetourNavSystem> m_Nav;
        Boom::NavAgentSystem                   m_NavAgents;
        entt::entity                           m_PlayerE = entt::null;
        entt::entity                           m_AgentE = entt::null;
        BOOM_INLINE void EnsureNinjaSeeksSamurai()
        {
            auto& reg = m_Context->scene;

            // Find target (player) named "Samurai"
            entt::entity samurai = Boom::FindEntityByName(reg, "Samurai");
            if (samurai == entt::null) {
                BOOM_WARN("[Nav] 'Samurai' not found in scene; Ninja will idle.");
                return;
            }

            // Find or create the seeker named "Ninja"
            entt::entity ninja = Boom::FindEntityByName(reg, "Ninja");
       

            // Ensure Ninja has a NavAgentComponent and configure it to follow Samurai
            auto& nac = reg.get_or_emplace<Boom::NavAgentComponent>(ninja);

            // NOTE: NavAgentComponent wraps NavAgent as 'agent'
            nac.follow = samurai;
            nac.active = true;
            nac.dirty = true;   // force first path build on the next update
            nac.speed = 2.5f;   // tune as you like
            nac.arrive = 0.15f;

            BOOM_INFO("[Nav] 'Ninja' will seek 'Samurai'.");
        }
        BOOM_INLINE void InitNavRuntime()
        {
            if (m_NavInitialized) return;

            auto& reg = m_Context->scene;

            // 1) Build / load navmesh only once
            if (!m_Nav) {
                const char* kNavPath = "Resources/NavData/level1.bin";
                m_Nav = std::make_unique<Boom::DetourNavSystem>();
                if (!m_Nav || !m_Nav->initFromFile(kNavPath)) {
                    BOOM_ERROR("[Nav] Failed to load navmesh: {}", kNavPath);
                    m_Nav.reset();
                    return;
                }
                BOOM_INFO("[Nav] Loaded navmesh.");
            }

            // 2) Cache the player (if any)
            m_PlayerE = Boom::FindEntityByName(reg, "Player");
            if (m_PlayerE == entt::null) {
                BOOM_WARN("[Nav] 'Player' not found; agent will idle until one exists.");
            }

            // 3) Find an existing agent FIRST (prefer 'NavAgent', then allow 'Enemy' to be used as agent)
            if (m_AgentE == entt::null || !reg.valid(m_AgentE)) {
                entt::entity byName = Boom::FindEntityByName(reg, "NavAgent");
                if (byName == entt::null) {
                    byName = Boom::FindEntityByName(reg, "Enemy"); // optional reuse
                }

                if (byName != entt::null) {
                    m_AgentE = byName;
                }
                else {
                    // Create a single agent as a sphere
                    glm::vec3 spawnPos{ 2.f, 1.f, 2.f }; // default
                    if (auto ground = Boom::FindEntityByName(reg, "ground");
                        ground != entt::null && reg.all_of<TransformComponent>(ground)) {
                        const auto& gt = reg.get<TransformComponent>(ground).transform;
                        spawnPos.y = gt.translate.y + 1.0f; // float above ground
                    }

                    // Uses your helper (speed = 2.0f set inside NavAgentComponent)
                    m_AgentE = CreateEnemySphere(reg, "NavAgent", spawnPos, /*radius*/0.5f);
                    RegisterSphereInitialState("NavAgent", spawnPos /*, glm::vec3(0)*/);
                  
                    BOOM_INFO("[Nav] Spawned 'NavAgent' sphere at ({}, {}, {}), r = {}",
                        spawnPos.x, spawnPos.y, spawnPos.z, 0.5f);
                }
            }

            // 4) Ensure required components exist on the chosen agent
            if (!reg.all_of<Boom::TransformComponent>(m_AgentE))
                reg.emplace<Boom::TransformComponent>(m_AgentE);
            if (!reg.all_of<Boom::NavAgentComponent>(m_AgentE))
                reg.emplace<Boom::NavAgentComponent>(m_AgentE);

            // 5) Configure follow target once
            if (m_PlayerE != entt::null) {
                auto& ag = reg.get<Boom::NavAgentComponent>(m_AgentE);
                ag.follow = m_PlayerE;
                ag.dirty = true; // force first path build
                BOOM_INFO("[Nav] Agent now follows 'Player'.");
            }

            m_NavInitialized = true;  // ← prevents re-entering
        }



        /**
        * @brief Cleans up the current scene and physics actors
        */
        BOOM_INLINE void CleanupCurrentScene()
        {
            BOOM_INFO("[Scene] Cleaning up current scene...");

            // Destroy physics actors before clearing scene
            DestroyPhysicsActors();

            // Clear the ECS scene
            m_Context->scene.clear();

            // PRESERVE PREFABS - but only those that exist on disk
            std::unordered_map<AssetID, std::shared_ptr<Asset>> savedPrefabs;
            auto& prefabMap = m_Context->assets->GetMap<PrefabAsset>();
            for (auto& [uid, asset] : prefabMap) {
                if (uid != EMPTY_ASSET) {
                    // Check if the prefab file exists on disk
                    std::string filepath = "Prefabs/" + asset->name + ".prefab";
                    if (std::filesystem::exists(filepath)) {
                        savedPrefabs[uid] = asset;
                    }
                    else {
                        BOOM_INFO("[Scene] Skipping prefab '{}' - file not found on disk", asset->name);
                    }
                }
            }
#if defined(_DEBUG)
            BOOM_INFO("[Scene] Preserved {} prefabs", savedPrefabs.size());
#endif

            // Reset asset registry (keeping EMPTY_ASSET sentinels)
            //* m_Context->assets = AssetRegistry();


            // RESTORE PREFABS after registry reset
            for (auto& [uid, asset] : savedPrefabs) {
                m_Context->assets->GetMap<PrefabAsset>()[uid] = std::static_pointer_cast<PrefabAsset>(asset);
            }
#if defined(_DEBUG)
            BOOM_INFO("[Scene] Restored {} prefabs", savedPrefabs.size());
#endif

            // Reset any scene-specific state



            BOOM_INFO("[Scene] Scene cleanup complete");
        }

        /**
         * @brief Reinitializes systems after loading a scene
         */
        BOOM_INLINE void ReinitializeSceneSystems()
        {
            BOOM_INFO("[Scene] Reinitializing scene systems...");

            EnttView<Entity, SkyboxComponent>([this](auto, auto& comp) {
                SkyboxAsset& skybox{ m_Context->assets->Get<SkyboxAsset>(comp.skyboxID) };
                m_Context->renderer->InitSkybox(skybox.data, skybox.envMap, skybox.size);
                BOOM_INFO("[Scene] Reinitialized skybox");
                });

            // Only reinitialize physics
            EnttView<Entity, RigidBodyComponent>([this](auto entity, auto&) {
                m_Context->physics->AddRigidBody(entity, *m_Context->assets);
                });


            BOOM_INFO("[Scene] Scene systems reinitialization complete");
        }

        /**
         * @brief Creates a minimal default scene with camera
         */
        BOOM_INLINE void CreateDefaultScene()
        {
            BOOM_INFO("[Scene] Creating default scene...");

            // Create basic camera entity
            Entity camera{ &m_Context->scene };
            camera.Attach<InfoComponent>();
            camera.Attach<TransformComponent>();
            camera.Attach<CameraComponent>();

            BOOM_INFO("[Scene] Default scene created with camera");
        }

        BOOM_INLINE void RegisterEventCallbacks()
        {
            // Set physics event callback (mark unused param to avoid warnings)
            m_Context->physics->SetEventCallback([this](auto e)
                {
                    // Check if this is a contact event
                    if (e.Event == PxEvent::CONTACT)
                    {
                        // Get both entities from the event payload
                        entt::entity ent1 = (entt::entity)e.Entity1;
                        entt::entity ent2 = (entt::entity)e.Entity2;

                        // Safely get the RigidBodyComponent for entity 1 and set its flag
                        if (m_Context->scene.valid(ent1) && m_Context->scene.all_of<RigidBodyComponent>(ent1))
                        {
                            m_Context->scene.get<RigidBodyComponent>(ent1).RigidBody.isColliding = true;
                        }

                        // Safely get the RigidBodyComponent for entity 2 and set its flag
                        if (m_Context->scene.valid(ent2) && m_Context->scene.all_of<RigidBodyComponent>(ent2))
                        {
                            m_Context->scene.get<RigidBodyComponent>(ent2).RigidBody.isColliding = true;
                        }
                    }
                });

            // Attach window resize event callback
            AttachCallback<WindowResizeEvent>([this](auto e)
                {
                    m_Context->renderer->Resize(e.width, e.height);
                });
        }
        BOOM_INLINE void ComputeFrameDeltaTime()
        {
            static double sLastTime = glfwGetTime();
            double currentTime = glfwGetTime();

            // Calculate raw delta time
            double rawDelta = (currentTime - sLastTime);

            // Only update delta time if not paused
            if (m_AppState == ApplicationState::RUNNING) {
                m_Context->DeltaTime = rawDelta;
            }
            else {
                m_Context->DeltaTime = 0.0; // No time progression when paused
            }

            sLastTime = currentTime;
        }

        BOOM_INLINE void ResetSphere()
        {
            EnttView<Entity, InfoComponent, TransformComponent, RigidBodyComponent>(
                [this](auto entity, InfoComponent& info, TransformComponent& transform, RigidBodyComponent& rb)
                {
                    (void)entity;
                    if (info.name != "Sphere") return;

                    auto* dyn = rb.RigidBody.actor->is<physx::PxRigidDynamic>();
                    if (!dyn) return;

                    const physx::PxVec3 p(
                        m_SphereInitialPosition.x,
                        m_SphereInitialPosition.y,
                        m_SphereInitialPosition.z
                    );

                    const physx::PxQuat q(0.f, 0.f, 0.f, 1.f); // identity quaternion

                    const physx::PxTransform pose(p, q);
                    dyn->setGlobalPose(pose);
                    dyn->setLinearVelocity(physx::PxVec3(0.f, 0.f, 0.f));
                    dyn->setAngularVelocity(physx::PxVec3(0.f, 0.f, 0.f));

                    // Mirror to ECS transform immediately (prevents 1-frame hitch)
                    transform.transform.translate = m_SphereInitialPosition;
                    transform.transform.rotate = glm::vec3(0.0f);
                });
        }


        BOOM_INLINE void ResetAllSpheres()
        {
            EnttView<Entity, InfoComponent, TransformComponent, RigidBodyComponent>(
                [this](auto entity, InfoComponent& info, TransformComponent& transform, RigidBodyComponent& rb)
                {
                    (void)entity; // Silence the "unused parameter" warning

                    // 1. Check if this entity is one of the spheres we want to reset
                    auto it = m_SphereInitialStates.find(info.name);
                    if (it == m_SphereInitialStates.end()) {
                        return; // Not a sphere we care about, skip it
                    }

                    // 2. Get the dynamic actor
                    auto* dyn = rb.RigidBody.actor->is<physx::PxRigidDynamic>();
                    if (!dyn) return; // Skip if it's not a dynamic body

                    // 3. Get the initial state from our map
                    const glm::vec3& initialPos = it->second.first;
                    const glm::vec3& initialVel = it->second.second;

                    // 4. Create the PhysX pose and velocity
                    const physx::PxVec3 p(initialPos.x, initialPos.y, initialPos.z);
                    const physx::PxVec3 v(initialVel.x, initialVel.y, initialVel.z);
                    const physx::PxQuat q(0.f, 0.f, 0.f, 1.f); // Identity rotation

                    // 5. Teleport the PhysX actor and reset its velocity
                    const physx::PxTransform pose(p, q);
                    dyn->setGlobalPose(pose);
                    dyn->setLinearVelocity(v);
                    dyn->setAngularVelocity(physx::PxVec3(0.f, 0.f, 0.f));

                    // 6. Update the ECS transform to match (so it's correct next frame)
                    transform.transform.translate = initialPos;
                    transform.transform.rotate = glm::vec3(0.0f);
                });
        }

        BOOM_INLINE void DestroyPhysicsActors()
        {
            // Get the scene from the physics context *once* outside the loop
            auto* pxScene = m_Context->physics->GetPxScene();
            if (!pxScene) {
                BOOM_ERROR("DestroyPhysicsActors failed: No PxScene available.");
                return;
            }

            // Iterate over all entities with a RigidBodyComponent
            EnttView<Entity, RigidBodyComponent>([this, pxScene](auto entity, auto& comp)
                {
                    auto* actor = comp.RigidBody.actor;
                    if (!actor) return; // Skip if no actor

                    // 1. Clean up Collider Pointers (if they exist)
                    if (entity.template Has<ColliderComponent>())
                    {
                        auto& collider = entity.template Get<ColliderComponent>().Collider;
                        if (collider.material) {
                            collider.material->release();
                            collider.material = nullptr;
                        }
                        if (collider.Shape) {
                            collider.Shape->release();
                            collider.Shape = nullptr;
                        }
                    }

                    // 2. Destroy actor user data
                    if (actor->userData) {
                        EntityID* owner = static_cast<EntityID*>(actor->userData);
                        BOOM_DELETE(owner);
                        actor->userData = nullptr;
                    }

                    // 3. (THE FIX) Remove from scene, THEN release memory
                    pxScene->removeActor(*actor);
                    actor->release();
                    comp.RigidBody.actor = nullptr;
                });
        }

        BOOM_INLINE void RunPhysicsSimulation()
        {
            // Only simulate physics if running
            if (m_AppState == ApplicationState::RUNNING)
            {

                m_Context->physics->Simulate(1, static_cast<float>(m_Context->DeltaTime));
                EnttView<Entity, RigidBodyComponent>([this](auto entity, auto& comp)
                    {
                        auto& transform = entity.template Get<TransformComponent>().transform;

                        // --- guard / lazy create ---
                        if (!comp.RigidBody.actor) {
                            // optional: try to create now
                            if (m_Context->physics)
                                m_Context->physics->AddRigidBody(entity, *m_Context->assets);

                            if (!comp.RigidBody.actor) {
                                // still null -> skip this entity this frame
                                return;
                            }
                        }

                        const auto pose = comp.RigidBody.actor->getGlobalPose();
                        if (comp.RigidBody.actor->is<physx::PxRigidDynamic>()) {
                            glm::quat rot(pose.q.w, pose.q.x, pose.q.y, pose.q.z);
                            transform.rotate = glm::degrees(glm::eulerAngles(rot));
                            transform.translate = PxToVec3(pose.p);
                        }
                    });

            }
        }

        BOOM_INLINE static glm::mat4 PxToGlm(const physx::PxTransform& t)
        {
            // GLM expects (w,x,y,z) ctor, PhysX stores (x,y,z,w)
            glm::quat q(t.q.w, t.q.x, t.q.y, t.q.z);
            glm::mat4 m = glm::mat4_cast(q);
            m[3] = glm::vec4(t.p.x, t.p.y, t.p.z, 1.0f);
            return m;
        }

       
       
        BOOM_INLINE static void AppendBoxWire(const physx::PxBoxGeometry& g,
            const physx::PxTransform& world,
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
        {
            const glm::vec3 he(g.halfExtents.x, g.halfExtents.y, g.halfExtents.z);
            const glm::mat4 M = PxToGlm(world);

            const glm::vec3 c[8] = {
                {-he.x, -he.y, -he.z}, { he.x, -he.y, -he.z},
                { he.x,  he.y, -he.z}, {-he.x,  he.y, -he.z},
                {-he.x, -he.y,  he.z}, { he.x, -he.y,  he.z},
                { he.x,  he.y,  he.z}, {-he.x,  he.y,  he.z}
            };
            auto X = [&](glm::vec3 p) { return glm::vec3(M * glm::vec4(p, 1)); };

            const int e[12][2] = {
                {0,1},{1,2},{2,3},{3,0},
                {4,5},{5,6},{6,7},{7,4},
                {0,4},{1,5},{2,6},{3,7}
            };
            for (auto& pair : e)
                AppendLine(out, X(c[pair[0]]), X(c[pair[1]]), color, color);
        }

        BOOM_INLINE static void AppendCircle(const glm::mat4& M, float r,
            int segments, int axis, // 0=X,1=Y,2=Z
            float yOffset,
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
        {
            auto P = [&](float a)->glm::vec3 {
                float s = sinf(a), c = cosf(a);
                glm::vec3 p;
                if (axis == 0)      p = glm::vec3(0, r * c, r * s);
                else if (axis == 1) p = glm::vec3(r * c, 0, r * s);
                else                p = glm::vec3(r * c, r * s, 0);
                p.y += (axis == 1 ? 0.0f : yOffset);
                return glm::vec3(M * glm::vec4(p, 1));
                };
            const float step = glm::two_pi<float>() / (float)segments;
            for (int i = 0; i < segments; ++i) {
                glm::vec3 a = P(i * step);
                glm::vec3 b = P((i + 1) * step);
                AppendLine(out, a, b, color, color);
            }
        }

        BOOM_INLINE static void AppendSphereWire(float radius,
            const physx::PxTransform& world,
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
        {
            const glm::mat4 M = PxToGlm(world);
            const int seg = 24;
            // 3 great circles
            AppendCircle(M, radius, seg, 0, 0.0f, out, color); // YZ plane
            AppendCircle(M, radius, seg, 1, 0.0f, out, color); // XZ plane
            AppendCircle(M, radius, seg, 2, 0.0f, out, color); // XY plane
        }

        BOOM_INLINE static void AppendSemiCircle(const glm::mat4& M, float r,
            int segments, int axis, // 0=X,1=Y,2=Z
            bool positiveHalf,      // which half to draw
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
        {
            auto P = [&](float a)->glm::vec3 {
                float s = sinf(a), c = cosf(a);
                glm::vec3 p;
                if (axis == 0)      p = glm::vec3(0, r * c, r * s); // YZ plane circle
                else if (axis == 1) p = glm::vec3(r * c, 0, r * s); // XZ plane circle
                else                p = glm::vec3(r * c, r * s, 0); // XY plane circle
                return glm::vec3(M * glm::vec4(p, 1));
                };

            const float step = glm::pi<float>() / (float)segments; // Step over 180 degrees
            const float offset = positiveHalf ? -glm::half_pi<float>() : glm::half_pi<float>();

            for (int i = 0; i < segments; ++i) {
                glm::vec3 a = P(offset + i * step);
                glm::vec3 b = P(offset + (i + 1) * step);
                AppendLine(out, a, b, color, color);
            }
        }

        BOOM_INLINE static void AppendCapsuleWire(float radius, float halfHeight,
            const physx::PxTransform& world,
            std::vector<Boom::LineVert>& out,
            const glm::vec4& color)
        {
            const glm::mat4 M = PxToGlm(world);
            const int seg = 12; // Segments for a semi-circle

            // --- Create transforms for the two hemisphere centers by translating in LOCAL space ---
            // A PhysX capsule's length is ALWAYS along its local X-axis.
            glm::mat4 M_end_pos_x = M * glm::translate(glm::mat4(1.0f), glm::vec3(halfHeight, 0, 0));
            glm::mat4 M_end_neg_x = M * glm::translate(glm::mat4(1.0f), glm::vec3(-halfHeight, 0, 0));

            // --- Draw the two hemispheres ---
            // For an X-aligned capsule, the "equator" is a circle in the YZ plane.
            // The other two semi-circles provide the 3D volume.

            // Positive X Hemisphere
            AppendCircle(M_end_pos_x, radius, seg * 2, 0, 0.0f, out, color);     // Equator circle (YZ plane)
            AppendSemiCircle(M_end_pos_x, radius, seg, 1, true, out, color);  // Top arc (XZ plane)
            AppendSemiCircle(M_end_pos_x, radius, seg, 2, true, out, color);  // Side arc (XY plane)

            // Negative X Hemisphere
            AppendCircle(M_end_neg_x, radius, seg * 2, 0, 0.0f, out, color);     // Equator circle (YZ plane)
            AppendSemiCircle(M_end_neg_x, radius, seg, 1, false, out, color); // Bottom arc (XZ plane)
            AppendSemiCircle(M_end_neg_x, radius, seg, 2, false, out, color); // Side arc (XY plane)

            // --- Draw four side rails connecting the equators ---
            for (int i = 0; i < 4; ++i) {
                float angle = glm::half_pi<float>() * i;

                // Points on the circumference of the capsule's equator (in the YZ plane)
                glm::vec3 p_on_circ(0, radius * cos(angle), radius * sin(angle));

                // Define the local start and end points of the rail along the X-axis
                glm::vec3 rail_start_local = glm::vec3(-halfHeight, 0, 0) + p_on_circ;
                glm::vec3 rail_end_local = glm::vec3(halfHeight, 0, 0) + p_on_circ;

                // Transform these local points to the final world space using the main transform M
                glm::vec3 A = glm::vec3(M * glm::vec4(rail_start_local, 1.0f));
                glm::vec3 B = glm::vec3(M * glm::vec4(rail_end_local, 1.0f));

                AppendLine(out, A, B, color, color);
            }
        }

        BOOM_INLINE void DrawRigidBodiesDebugOnly(const glm::mat4& view, const glm::mat4& proj)
        {
            if (!m_DebugLinesShader) return;

            std::vector<Boom::LineVert> verts;
            verts.reserve(1024);
            //const glm::vec4 color(0.1f, 1.0f, 0.1f, 1.0f);

            EnttView<Entity, RigidBodyComponent>([&](auto entity, RigidBodyComponent& rb) {
                if (!entity.template Has<ColliderComponent>()) return;

                const glm::vec4 color = rb.RigidBody.isColliding
                    ? glm::vec4(1.0f, 0.0f, 0.0f, 1.0f) // Bright Red when colliding
                    : glm::vec4(1.1f, 0.0f, 1.0f, 1.0f);

                physx::PxRigidActor* actor = rb.RigidBody.actor;
                if (!actor) return;

                PxU32 n = actor->getNbShapes();
                if (!n) return;
                std::vector<physx::PxShape*> shapes(n);
                actor->getShapes(shapes.data(), n);

                const physx::PxTransform actorPose = actor->getGlobalPose();
                for (auto* shape : shapes)
                {
                    const physx::PxTransform world = actorPose * shape->getLocalPose();
                    physx::PxGeometryHolder gh = shape->getGeometry();
                    switch (gh.getType())
                    {
                    case physx::PxGeometryType::eBOX:
                        AppendBoxWire(gh.box(), world, verts, color);
                        break;
                    case physx::PxGeometryType::eSPHERE:
                        AppendSphereWire(gh.sphere().radius, world, verts, color);
                        break;
                    case physx::PxGeometryType::eCAPSULE:
                        AppendCapsuleWire(gh.capsule().radius, gh.capsule().halfHeight, world, verts, color);
                        break;
                    default:
                        // For mesh/convex/etc. you can add more if needed; skip for now.
                        break;
                    }
                }
                });

            if (!verts.empty())
                m_DebugLinesShader->Draw(view, proj, verts, 10.5f);
        }

        BOOM_INLINE static float DistancePointSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
        {
            const glm::vec3 ab = b - a;
            const float ab2 = glm::dot(ab, ab);
            if (ab2 <= 1e-6f) return glm::distance(p, a);
            const float t = glm::clamp(glm::dot(p - a, ab) / ab2, 0.0f, 1.0f);
            const glm::vec3 closest = a + t * ab;
            return glm::distance(p, closest);
        }


        // -- MONO functions -- 
        BOOM_INLINE static std::string GetExeDir()
        {
#ifdef _WIN32
            char buf[MAX_PATH]{};
            DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
            if (n == 0 || n == MAX_PATH) return std::filesystem::current_path().string();
            std::filesystem::path p(buf);
            return p.parent_path().string();
#else
            // Fallback for non-Windows platforms
            return std::filesystem::current_path().string();
#endif
        }

        BOOM_INLINE bool InitMonoRuntime(const std::string& monoBaseDir,
            const std::string& assembliesDir,
            const char* domainName = "BoomDomain")
        {
            // 0) sanity on folders
            if (!std::filesystem::exists(monoBaseDir) ||
                !std::filesystem::exists(monoBaseDir + "/lib") ||
                !std::filesystem::exists(monoBaseDir + "/etc"))
            {
                
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Invalid mono base folder: '{}'", monoBaseDir);

#endif // DEBUG
                return false;
            }
            if (!std::filesystem::exists(assembliesDir))
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Assemblies folder not found: '{}'", assembliesDir);

#endif // DEBUG
                
                return false;
            }

            m_MonoBase = monoBaseDir;
            m_AssembliesPath = assembliesDir;

            // 1) point Mono at its runtime folders
            //    On Windows, make sure the Mono DLLs (e.g., mono-2.0-sgen.dll) are next to the EXE or in PATH.
            mono_set_dirs((m_MonoBase + "/lib").c_str(),
                (m_MonoBase + "/etc").c_str());

            // 2) config (enables machine.config etc.)
            //mono_config_parse(nullptr);

            // 3) assembly search paths (so "GameScripts.dll" can be found by name)
            mono_set_assemblies_path(m_AssembliesPath.c_str());

            // 4) root domain
            m_MonoRootDomain = mono_jit_init_version(domainName, "v4.0.30319");
            if (!m_MonoRootDomain)
            {
                BOOM_ERROR("[Mono] mono_jit_init_version failed.");
                return false;
            }

            // 5) create a child/app domain (optional but recommended for unload/reload patterns)
            m_MonoAppDomain = mono_domain_create_appdomain(const_cast<char*>("BoomAppDomain"), nullptr);
            if (!m_MonoAppDomain)
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] mono_domain_create_appdomain failed.");

#endif // DEBUG
                return false;
            }
            mono_domain_set(m_MonoAppDomain, /* force */ false);
#ifdef _DEBUG
            BOOM_INFO("[Mono] Initialized. Base='{}', Assemblies='{}'", m_MonoBase, m_AssembliesPath);
#endif // DEBUG
            return true;
        }

        BOOM_INLINE void ShutdownMonoRuntime()
        {
            if (m_MonoAppDomain)
            {
                // Switch back to root to safely unload app domain
                mono_domain_set(m_MonoRootDomain, false);
                mono_domain_unload(m_MonoAppDomain);
                m_MonoAppDomain = nullptr;
            }
            if (m_MonoRootDomain)
            {
                mono_jit_cleanup(m_MonoRootDomain);
                m_MonoRootDomain = nullptr;
            }
            m_GameAssembly = nullptr;
            m_GameImage = nullptr;

#ifdef _DEBUG
            BOOM_INFO("[Mono] Shutdown complete.");
#endif // DEBUG

        }

        BOOM_INLINE bool LoadGameAssembly(const std::string& dllName /* e.g., "GameScripts.dll" */)
        {
            if (!m_MonoAppDomain)
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] App domain not initialized.");
#endif // DEBUG

                return false;
            }

            const std::string full = (std::filesystem::path(m_AssembliesPath) / dllName).string();
            if (!std::filesystem::exists(full))
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Assembly not found: {}", full);

#endif // DEBUG
                return false;
            }

            m_GameAssembly = mono_domain_assembly_open(m_MonoAppDomain, full.c_str());
            if (!m_GameAssembly)
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Failed to load assembly: {}", full);
#endif // DEBUG
                return false;
            }

            m_GameImage = mono_assembly_get_image(m_GameAssembly);
            if (!m_GameImage)
            {
#ifdef _DEBUG
                BOOM_ERROR("[Mono] mono_assembly_get_image failed.");
#endif // DEBUG
                return false;
            }
#ifdef _DEBUG
            BOOM_INFO("[Mono] Loaded assembly: {}", full);
#endif // DEBUG
            return true;
        }

        BOOM_INLINE bool InvokeStaticVoid(const char* nsName,
            const char* className,
            const char* methodName,
            void** args = nullptr)
        {
            if (!m_GameImage) { BOOM_ERROR("[Mono] No assembly image loaded."); return false; }

            MonoClass* klass = mono_class_from_name(m_GameImage, nsName, className);
            if (!klass) { BOOM_ERROR("[Mono] Class not found: {}.{}", nsName, className); return false; }

            MonoMethod* method = mono_class_get_method_from_name(klass, methodName, /*param_count*/ 0);
            if (!method) { BOOM_ERROR("[Mono] Method not found: {}.{}", className, methodName); return false; }

            MonoObject* exc = nullptr;
            mono_runtime_invoke(method, /*this*/ nullptr, args, &exc);
            if (exc)
            {
                // print exception
                MonoString* s = mono_object_to_string(exc, nullptr);
                char* utf8 = mono_string_to_utf8(s);
#ifdef _DEBUG
                BOOM_ERROR("[Mono] Exception: {}", utf8 ? utf8 : "(null)");
#endif // DEBUG
                if (utf8) mono_free(utf8);
                return false;
            }
            return true;
        }

        BOOM_INLINE bool InvokeStatic1Float(const char* nsName,
            const char* className,
            const char* methodName,
            float value)
        {
            if (!m_GameImage) { BOOM_ERROR("[Mono] No assembly image loaded."); return false; }

            MonoClass* klass = mono_class_from_name(m_GameImage, nsName, className);
            if (!klass) { BOOM_ERROR("[Mono] Class not found: {}.{}", nsName, className); return false; }

            // method with 1 parameter
            MonoMethod* method = mono_class_get_method_from_name(klass, methodName, 1);
            if (!method) { BOOM_ERROR("[Mono] Method not found: {}.{}", className, methodName); return false; }

            void* args[1];
            args[0] = &value;

            MonoObject* exc = nullptr;
            mono_runtime_invoke(method, nullptr, args, &exc);
            if (exc)
            {
                MonoString* s = mono_object_to_string(exc, nullptr);
                char* utf8 = mono_string_to_utf8(s);
                BOOM_ERROR("[Mono] Exception: {}", utf8 ? utf8 : "(null)");
                if (utf8) mono_free(utf8);
                return false;
            }
            return true;
        }


        BOOM_INLINE void UpdateThirdPersonCameras()
        {
            // 1. Get input
            glm::vec2 mouseDelta = m_Context->window->input.mouseDeltaLast();
            glm::vec2 scrollDelta = m_Context->window->input.scrollDelta();

            // 2. Iterate over all third-person cameras
            EnttView<Entity, ThirdPersonCameraComponent, TransformComponent>(
                [this, &mouseDelta, &scrollDelta](Entity entity, ThirdPersonCameraComponent& cam, TransformComponent& tc)
                {

#define UNUSED(x) (void)(x)
                    UNUSED(entity);



                    // 3. Find the target entity by its UID
                    if (cam.targetUID == 0) return; // No target UID set

                    entt::entity targetEnttID = entt::null;
                    auto infoView = m_Context->scene.view<InfoComponent>();
                    for (auto e : infoView) {
                        if (infoView.get<InfoComponent>(e).uid == cam.targetUID) {
                            targetEnttID = e;
                            break;
                        }
                    }

                    if (targetEnttID == entt::null) return; // Target not found

                    Entity target{ &m_Context->scene, targetEnttID };
                    if (!target.Has<TransformComponent>()) return; // Target has no position

                    //
                    // === NEW LOGIC STARTS HERE ===
                    //

                    // 4. Get the target's full transform
                    Transform3D& targetTransform = target.Get<TransformComponent>().transform;
                    glm::vec3 targetPosition = targetTransform.translate;
                    float targetYaw = targetTransform.rotate.y; // Get the player's Y rotation

                    // 5. Update Pitch (up/down) from the mouse
                   // cam.currentPitch -= mouseDelta.y * cam.mouseSensitivity;

                    // 6. Apply new Pitch Limits
                    //    We clamp the pitch from 5 (slightly looking down) to 40 (about 45 degrees)
                    //    This prevents the camera from going "below the plane".
                    cam.currentPitch = glm::clamp(cam.currentPitch, 2.0f, 40.0f);

                    // 7. Lock Yaw (left/right) to the target's yaw
                    //    This keeps the camera locked behind the player.
                    cam.currentYaw = targetYaw + 180.0f;

                    // 8. Update distance (zoom) from the scroll wheel
                    cam.currentDistance -= scrollDelta.y * cam.scrollSensitivity;
                    cam.currentDistance = glm::clamp(cam.currentDistance, cam.minDistance, cam.maxDistance);

                    // 9. Calculate the camera's final orientation
                    glm::quat orientation = glm::quat(glm::vec3(glm::radians(cam.currentPitch),
                        glm::radians(cam.currentYaw),
                        0.0f));

                    // 10. Define the pivot point (e.g., 5 units above the player's origin)
                    glm::vec3 pivotPosition = targetPosition + glm::vec3(0.0f, cam.offset.y, 0.0f);

                    // 11. Calculate the final camera position
                    //     Start with a vector pointing "back" by the zoom distance
                    glm::vec3 offsetVector = glm::vec3(0.0f, 0.0f, -cam.currentDistance);
                    //     Rotate that vector by the final orientation
                    glm::vec3 rotatedOffset = orientation * offsetVector;
                    //     Add it to the pivot point
                    glm::vec3 desiredPosition = pivotPosition + rotatedOffset;

                    // 12. Update the camera's actual transform
                    tc.transform.translate = desiredPosition;

                    // 13. Make the camera look at the pivot point
                    tc.transform.rotate = glm::degrees(glm::eulerAngles(
                        glm::quatLookAt(glm::normalize(pivotPosition - desiredPosition), glm::vec3(0, 1, 0))
                    ));
                }
            );
        }
    };



}

#endif // !APPLICATION_H
