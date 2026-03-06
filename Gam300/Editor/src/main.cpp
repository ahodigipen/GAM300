// src/Editor/main.cpp
#include <cstdint>
#include <memory>

#include "BoomEngine.h"                 // defines MyEngineClass, Application, etc.
#include "Context/DebugHelpers.h"       // BOOM_INFO / BOOM_ERROR
#include "Editor.h"              // your Editor layer

// ImGui + backends
#include "Vendors/imgui/imgui.h"
#include "Vendors/imgui/backends/imgui_impl_glfw.h"
#include "Vendors/imgui/backends/imgui_impl_opengl3.h"
#include "Context/Context.h"
#include "Context/CyberpunkTheme.h"
// GLFW (needed for window/context handle)
#include <GLFW/glfw3.h>
#include "../src/Context/InputBridge.h"            // EditorUI::InstallEditorInputCallbacks
int32_t main()
{
    try
    {
        // Boot the engine
        MyEngineClass engine;
        engine.whatup();
        BOOM_INFO("Editor Started");

        // Init audio (FMOD)
        if (!SoundEngine::Instance().Init())
        {
            BOOM_ERROR("FMOD init failed");
            return -1;
        }

        // Create the engine Application (owns the GLFW window + run loop)
        auto app = engine.CreateApp();
        app->PostEvent<WindowTitleRenameEvent>(
            "Boom Editor - Press 'Esc' to quit. 'WASD' to pan camera");

        // Registry shared by editor panels / runtime
        entt::registry mainRegistry;

        // Acquire window/context from the engine
        std::shared_ptr<GLFWwindow> engineWindow = app->GetWindowHandle();

        ImGuiContext* imguiContext = nullptr;

        if (engineWindow)
        {
            // Ensure GL context is current on this thread
            glfwMakeContextCurrent(engineWindow.get());
            GLFWwindow* current = glfwGetCurrentContext();

            if (current == engineWindow.get())
            {
                BOOM_INFO("Context is current, initializing ImGui...");

                // --- ImGui core ---
                IMGUI_CHECKVERSION();
                imguiContext = ImGui::CreateContext();

                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
                io.ConfigWindowsMoveFromTitleBarOnly = true;

                // --- ImGui backends ---
                const bool platformOk = ImGui_ImplGlfw_InitForOpenGL(engineWindow.get(), false);
                const bool rendererOk = ImGui_ImplOpenGL3_Init("#version 450");

                if (platformOk && rendererOk)
                {
                    BOOM_INFO("ImGui initialized successfully!");
                    ImGui::StyleColorsDark();
                    ApplyCyberpunkTheme();
                    EditorUI::InstallEditorInputCallbacks(engineWindow.get());
                }
                else
                {
                    BOOM_ERROR("ImGui backend initialization failed");
                    ImGui::DestroyContext(imguiContext);
                    imguiContext = nullptr;
                }
            }
            else
            {
                BOOM_ERROR("Failed to make GLFW context current.");
            }
        }
        else
        {
            BOOM_ERROR("Engine window handle is null.");
        }

        // Attach the Editor layer only if ImGui is ready   
        if (imguiContext)
        {
            // Your Editor layer expects (ImGuiContext*, entt::registry*, Application*)
            app->AttachLayer<EditorUI::Editor>(imguiContext, &mainRegistry, app.get());
        }
        else
        {
            BOOM_ERROR("Failed to initialize ImGui, running without editor.");
        }

        // --- Main run loop ---
        app->RunContext(true);

        // --- Shutdown / Cleanup ---
        if (imguiContext)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext(imguiContext);
        }

        SoundEngine::Instance().Shutdown();
    }
    catch (const std::exception& e)
    {
#ifdef DEBUG
        BOOM_ERROR("Application failed: {}", e.what());
#endif // DEBUG
		std::cerr << "Application failed: " << e.what() << std::endl;   
        return -1;
    }

    return 0;
}
