#include "Panels/ModelPreviewPanel.h"
#include "Editor.h"
#include "Application/Interface.h"
#include "Application/Context.h"
#include "Graphics/Models/Model.h"
#include "Graphics/Models/Animator.h"
#include "Auxiliaries/Assets.h"
#include "Vendors/imgui/imgui.h"
#include "common/Core.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace EditorUI;

ModelPreviewPanel::ModelPreviewPanel(Editor* owner)
    : m_Owner(owner)
    , m_App(owner ? static_cast<Boom::AppInterface*>(owner) : nullptr)
    , m_Ctx(m_App ? m_App->GetContext() : nullptr)
{
    // Create framebuffer for independent rendering
    glGenFramebuffers(1, &m_FramebufferID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);

    // Create color texture
    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureID, 0);

    // Create depth buffer
    glGenRenderbuffers(1, &m_DepthBufferID);
    glBindRenderbuffer(GL_RENDERBUFFER, m_DepthBufferID);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthBufferID);

    // Check framebuffer status
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        BOOM_ERROR("[ModelPreviewPanel] Framebuffer is not complete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ResetCamera();
}

ModelPreviewPanel::~ModelPreviewPanel()
{
    if (m_FramebufferID) glDeleteFramebuffers(1, &m_FramebufferID);
    if (m_TextureID) glDeleteTextures(1, &m_TextureID);
    if (m_DepthBufferID) glDeleteRenderbuffers(1, &m_DepthBufferID);
}

void ModelPreviewPanel::Render()
{
    if (!m_Owner) return;

    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    ImGui::Begin("Model Preview", &m_Owner->m_ShowModelPreview);

    RenderToolbar();
    RenderViewport();

    ImGui::End();
}

void ModelPreviewPanel::RenderToolbar()
{
    ImGui::Text("Model Preview Window");
    ImGui::SameLine(ImGui::GetWindowWidth() - 120);

    if (ImGui::Button("Load Model"))
    {
        // TODO: Open file dialog to select model
        // For now, just show a placeholder
        ImGui::OpenPopup("LoadModelPopup");
    }

    if (ImGui::BeginPopup("LoadModelPopup"))
    {
        ImGui::Text("Enter model path:");
        static char modelPath[256] = "Resources/Models/";
        if (ImGui::InputText("##modelpath", modelPath, sizeof(modelPath), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            LoadModel(modelPath);
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear") && m_HasModel)
    {
        ClearModel();
    }

    ImGui::Separator();

    // Visualization options
    ImGui::Checkbox("Show Skeleton", &m_ShowSkeleton);
    ImGui::SameLine();
    ImGui::Checkbox("Show Grid", &m_ShowGrid);
    ImGui::SameLine();
    if (ImGui::Button("Reset Camera"))
    {
        ResetCamera();
    }

    ImGui::Separator();
}

void ModelPreviewPanel::RenderViewport()
{
    ImVec2 availableSize = ImGui::GetContentRegionAvail();

    // Resize framebuffer if viewport size changed
    if (availableSize.x != m_ViewportSize.x || availableSize.y != m_ViewportSize.y)
    {
        if (availableSize.x > 0 && availableSize.y > 0)
        {
            m_ViewportSize = availableSize;

            // Resize color texture
            glBindTexture(GL_TEXTURE_2D, m_TextureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

            // Resize depth buffer
            glBindRenderbuffer(GL_RENDERBUFFER, m_DepthBufferID);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y);
        }
    }

    // Handle camera controls
    HandleCameraControls();
    UpdateCamera();

    // Render to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);
    glViewport(0, 0, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y);

    // Clear
    glClearColor(0.2f, 0.2f, 0.25f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Render scene
    if (m_ShowGrid) RenderGrid();
    if (m_HasModel) RenderModel();
    if (m_HasModel && m_ShowSkeleton) RenderSkeleton();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Display framebuffer texture in ImGui
    ImGui::Image(
        (ImTextureID)(intptr_t)m_TextureID,
        m_ViewportSize,
        ImVec2(0, 1), // UV top-left
        ImVec2(1, 0)  // UV bottom-right (flip Y)
    );
}

void ModelPreviewPanel::HandleCameraControls()
{
    if (!ImGui::IsItemHovered()) return;

    ImGuiIO& io = ImGui::GetIO();

    // Right mouse button: orbit camera
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        if (!m_IsOrbitingCamera)
        {
            m_IsOrbitingCamera = true;
            m_LastMousePos = ImGui::GetMousePos();
        }

        ImVec2 currentMousePos = ImGui::GetMousePos();
        ImVec2 delta = ImVec2(
            currentMousePos.x - m_LastMousePos.x,
            currentMousePos.y - m_LastMousePos.y
        );

        // Update camera angles
        m_CameraYaw -= delta.x * 0.005f;   // Horizontal rotation
        m_CameraPitch -= delta.y * 0.005f; // Vertical rotation

        // Clamp pitch to avoid flipping
        m_CameraPitch = glm::clamp(m_CameraPitch, -1.5f, 1.5f);

        m_LastMousePos = currentMousePos;
    }
    else
    {
        m_IsOrbitingCamera = false;
    }

    // Mouse wheel: zoom
    if (io.MouseWheel != 0.0f)
    {
        m_CameraDistance -= io.MouseWheel * 0.3f;
        m_CameraDistance = glm::clamp(m_CameraDistance, 0.5f, 20.0f);
    }
}

void ModelPreviewPanel::UpdateCamera()
{
    // Calculate camera position based on spherical coordinates
    float x = m_CameraDistance * cos(m_CameraPitch) * sin(m_CameraYaw);
    float y = m_CameraDistance * sin(m_CameraPitch);
    float z = m_CameraDistance * cos(m_CameraPitch) * cos(m_CameraYaw);

    m_CameraPosition = m_CameraTarget + glm::vec3(x, y, z);
}

void ModelPreviewPanel::RenderModel()
{
    if (!m_Ctx || !m_Ctx->renderer || !m_Model) return;

    // TODO: Render the loaded model using the renderer
    // For now, we'll just render a placeholder

    // Get view and projection matrices
    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // TODO: Draw model with renderer->DrawModel() or similar
    // This will require accessing the PBR shader and drawing the model mesh
}

void ModelPreviewPanel::RenderSkeleton()
{
    if (!m_Animator || !m_Ctx) return;

    // TODO: Render skeleton bones similar to Application.cpp skeleton visualization
    // We'll reuse the DebugLinesShader and GetSkeletonLines() method
}

void ModelPreviewPanel::RenderGrid()
{
    // TODO: Render a grid on the floor for reference
    // Simple lines in XZ plane
}

void ModelPreviewPanel::LoadModel(const std::string& modelPath)
{
    if (!m_Ctx || !m_Ctx->assets) return;

    BOOM_INFO("[ModelPreviewPanel] Loading model: {}", modelPath);

    // Try to load model from asset registry
    // TODO: Implement model loading from file path
    // For now, just set flags

    m_LoadedModelPath = modelPath;
    m_HasModel = false; // Will be true once we implement loading

    BOOM_WARN("[ModelPreviewPanel] Model loading not yet implemented");
}

void ModelPreviewPanel::ClearModel()
{
    m_Model.reset();
    m_Animator.reset();
    m_HasModel = false;
    m_LoadedModelPath.clear();
    m_SelectedBoneName.clear();

    BOOM_INFO("[ModelPreviewPanel] Model cleared");
}

void ModelPreviewPanel::ResetCamera()
{
    m_CameraDistance = 3.0f;
    m_CameraYaw = 0.0f;
    m_CameraPitch = 0.3f; // Slightly above horizontal
    m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
    UpdateCamera();
}
