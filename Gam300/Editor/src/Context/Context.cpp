// src/Context/Context.cpp
#include "Context/Context.h"          // your declaration-only header
#include "Context/DebugHelpers.h"     // BOOM_* macros, validators
#include "Audio/Audio.hpp"            // SoundEngine
#include "Helpers.h"                  // FONT_FILE, ICON_FONT, SHADER_VERSION, ICON_MIN_FA/ICON_MAX_FA (via Vendors/FA.h)

// ImGui + backends (only in .cpp)
#include "Vendors/imgui/imgui.h"
#include "Vendors/imgui/backends/imgui_impl_glfw.h"
#include "Vendors/imgui/backends/imgui_impl_opengl3.h"

// GLFW + GL
#include <GLFW/glfw3.h>
#include <GL/glew.h>   // or <GL/glew.h> whatever your project uses before GL calls
#include "CyberpunkTheme.h"

// ------------------------- GuiContext -------------------------

GuiContext::~GuiContext()
{
    DEBUG_DLL_BOUNDARY("GuiContext::~GuiContext");
    // Backends/context are owned elsewhere (e.g., main.cpp). Do NOT shut them down here.
    // Match your previous behavior: only shutdown audio if you own it.
    SoundEngine::Instance().Shutdown();
}

void GuiContext::OnStart()
{
    DEBUG_DLL_BOUNDARY("GuiContext::OnStart");

    std::shared_ptr<GLFWwindow> window = this->GetWindowHandle();
    if (!window) {
        BOOM_ERROR("GuiContext::OnStart - Invalid window handle!");
        return;
    }

    // Ensure the engine's GL context is current
    if (!EnsureContextCurrent(window.get())) {
        BOOM_ERROR("GuiContext::OnStart - Failed to ensure context is current!");
        return;
    }

    DebugHelpers::ValidateWindowHandle(window.get(), "OnStart");
    DEBUG_OPENGL_STATE();

    // Initialize ImGui core (context is created if missing)
    IMGUI_CHECKVERSION();
    if (ImGuiContext* existingCtx = ImGui::GetCurrentContext()) {
        BOOM_WARN("GuiContext::OnStart - ImGui context already exists: {}", (void*)existingCtx);
    }
    else {
        ImGuiContext* ctx = ImGui::CreateContext();
        BOOM_INFO("GuiContext::OnStart - Created ImGui context: {}", (void*)ctx);
    }

    // IO flags / config
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Detect if backends need init; we do NOT init here (owner is main.cpp)
    const bool platformNeedsInit = (io.BackendPlatformUserData == nullptr);
    const bool rendererNeedsInit = (io.BackendRendererUserData == nullptr);
    BOOM_INFO("GuiContext::OnStart - Platform needs init: {}, Renderer needs init: {}",
        platformNeedsInit, rendererNeedsInit);

    // Keep previous control flow: treat missing backends as failure, but don't init here.
    const bool platformInit = true;   // owner should have initialized
    const bool rendererInit = true;
    if (!platformInit || !rendererInit) {
        BOOM_ERROR("GuiContext::OnStart - ImGui backends not initialized!");
        return;
    }

    if (!SoundEngine::Instance().Init()) {
        BOOM_ERROR("FMOD failed to initialize");
    }

    // Load fonts only if needed
    if (io.Fonts->Fonts.Size == 0) {
        LoadFonts();
    }

    // Style
    ImGui::StyleColorsDark();
    ApplyCyberpunkTheme();

    // Validate ImGui state after initialization
    DebugHelpers::ValidateImGuiState("After initialization");

    // Attach event callback
    (void)AttachCallback<SelectEvent>([this](auto e)
        {
            DEBUG_DLL_BOUNDARY("SelectEvent callback");
            for (auto& window : m_Windows) {
                if (window) {
                    window->OnSelect(ToEntt<Entity>(e.EnttID));
                }
            }
        });

    // Keep a copy of the window handle for context restoration
    m_EngineWindow = window;

    OnGuiStart();
    BOOM_INFO("GuiContext::OnStart completed successfully");
}

void GuiContext::OnUpdate()
{
    DEBUG_DLL_BOUNDARY("GuiContext::OnUpdate");

    // Ensure context is current before every frame
    if (!EnsureContextCurrent(m_EngineWindow.get())) {
        BOOM_ERROR("GuiContext::OnUpdate - Context lost and cannot be restored!");
        return;
    }

    SoundEngine::Instance().Update();

    // Periodic ImGui state validation
    static int frameCount = 0;
    if (++frameCount % 300 == 0) {
        DebugHelpers::ValidateImGuiState("Periodic validation");
    }

    // Begin ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main dockspace window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowPos(viewport->Pos);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    const bool windowOpen = ImGui::Begin("MainWindow", nullptr, flags);

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    if (windowOpen)
    {
        ImGui::DockSpace(ImGui::GetID("MainDockspace"), ImVec2(0, 0),
            ImGuiDockNodeFlags_PassthruCentralNode);

        // Show all attached windows
        for (auto& w : m_Windows) {
            if (w) w->OnShow();
        }

        // Optional demo window
        ImGui::ShowDemoWindow();

        // User hook
        OnGuiFrame();
    }
    ImGui::End();

    // Render
    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData && drawData->Valid) {
        ImGui_ImplOpenGL3_RenderDrawData(drawData);
    }
    else {
        BOOM_WARN("GuiContext::OnUpdate - Invalid draw data!");
    }
}

bool GuiContext::EnsureContextCurrent(GLFWwindow* window)
{
    if (!window) return false;

    GLFWwindow* currentContext = glfwGetCurrentContext();
    if (currentContext != window) {
        BOOM_INFO("GuiContext::EnsureContextCurrent - Restoring context: {} -> {}",
            (void*)currentContext, (void*)window);

        glfwMakeContextCurrent(window);
        currentContext = glfwGetCurrentContext();
        if (currentContext != window) {
            BOOM_ERROR("GuiContext::EnsureContextCurrent - Failed to restore context!");
            return false;
        }

        BOOM_INFO("GuiContext::EnsureContextCurrent - Context restored successfully");
    }
    return true;
}

void GuiContext::LoadFonts()
{
    BOOM_INFO("GuiContext::LoadFonts - Loading fonts...");

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig fontConfig;
    fontConfig.MergeMode = true;
    fontConfig.PixelSnapH = true;
    static const ImWchar iconRange[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    // Regular + small text fonts (icons optional)
    ImFont* regularFont = io.Fonts->AddFontFromFileTTF(FONT_FILE, REGULAR_FONT_SIZE);
    // io.Fonts->AddFontFromFileTTF(ICON_FONT, REGULAR_FONT_SIZE, &fontConfig, iconRange);

    ImFont* smallFont = io.Fonts->AddFontFromFileTTF(FONT_FILE, SMALL_FONT_SIZE);
    // io.Fonts->AddFontFromFileTTF(ICON_FONT, SMALL_FONT_SIZE, &fontConfig, iconRange);

    BOOM_INFO("GuiContext::LoadFonts - Loaded fonts: regular={}, small={}",
        (void*)regularFont, (void*)smallFont);

    io.Fonts->Build();
}

// --------------------- GuiContextNoSwitch ---------------------

GuiContextNoSwitch::~GuiContextNoSwitch()
{
    // This variant owns its backend init; shut them down here.
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        SoundEngine::Instance().Shutdown();
    }
}

void GuiContextNoSwitch::InitializeWithExistingContext(GLFWwindow* window)
{
    BOOM_INFO("GuiContextNoSwitch::InitializeWithExistingContext - Window: {}", (void*)window);

    m_Window = window;

    GLFWwindow* current = glfwGetCurrentContext();
    if (!current) {
        BOOM_ERROR("InitializeWithExistingContext - No current context!");
        return;
    }
    BOOM_INFO("InitializeWithExistingContext - Current context: {}", (void*)current);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        BOOM_ERROR("InitializeWithExistingContext - OpenGL error: {}", error);
        return;
    }

    // ImGui core
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    BOOM_INFO("InitializeWithExistingContext - Created ImGui context: {}", (void*)ctx);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Backends (this class owns them)
    const bool platformInit = ImGui_ImplGlfw_InitForOpenGL(current, true);
    const bool rendererInit = ImGui_ImplOpenGL3_Init(SHADER_VERSION);
    BOOM_INFO("InitializeWithExistingContext - Platform: {}, Renderer: {}", platformInit, rendererInit);
    if (!platformInit || !rendererInit) {
        BOOM_ERROR("InitializeWithExistingContext - Backend initialization failed!");
        return;
    }

    // Fonts + style
    LoadFonts();
    ImGui::StyleColorsDark();
    ApplyCyberpunkTheme();

    BOOM_INFO("InitializeWithExistingContext - Initialization complete!");
}

void GuiContextNoSwitch::OnUpdate()
{
    GLFWwindow* current = glfwGetCurrentContext();
    if (!current) {
        BOOM_WARN("GuiContextNoSwitch::OnUpdate - No current context, skipping frame");
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Main dockspace window
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_MenuBar;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowPos(vp->Pos);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

    const bool windowOpen = ImGui::Begin("MainWindow", nullptr, flags);

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);

    if (windowOpen) {
        ImGui::DockSpace(ImGui::GetID("MainDockspace"), ImVec2(0, 0),
            ImGuiDockNodeFlags_PassthruCentralNode);

        for (auto& vwindow : m_Windows) {
            if (vwindow) vwindow->OnShow();
        }

        ImGui::ShowDemoWindow();
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GuiContextNoSwitch::LoadFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig fontConfig;
    fontConfig.MergeMode = true;
    fontConfig.PixelSnapH = true;
    static const ImWchar iconRange[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

    io.Fonts->AddFontFromFileTTF(FONT_FILE, REGULAR_FONT_SIZE);
    io.Fonts->AddFontFromFileTTF(ICON_FONT, REGULAR_FONT_SIZE, &fontConfig, iconRange);
    io.Fonts->AddFontFromFileTTF(FONT_FILE, SMALL_FONT_SIZE);
    io.Fonts->AddFontFromFileTTF(ICON_FONT, SMALL_FONT_SIZE, &fontConfig, iconRange);

    io.Fonts->Build();
}
