#include "Panels/ModelPreviewPanel.h"
#include "Editor.h"
#include "Application/Interface.h"
#include "Application/Context.h"
#include "Graphics/Models/Model.h"
#include "Graphics/Models/Animator.h"
#include "Graphics/Shaders/DebugLines.h"
#include "Graphics/Utilities/Data.h"
#include "Auxiliaries/Assets.h"
#include "Vendors/imgui/imgui.h"
#include "common/Core.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

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

    // Model selection dropdown
    if (ImGui::Button("Load Model", ImVec2(100, 0)))
    {
        ImGui::OpenPopup("SelectModelPopup");
    }

    if (ImGui::BeginPopup("SelectModelPopup"))
    {
        ImGui::Text("Select a model:");
        ImGui::Separator();

        if (m_Ctx && m_Ctx->assets)
        {
            auto& modelMap = m_Ctx->assets->GetMap<Boom::ModelAsset>();

            for (auto& [assetID, assetPtr] : modelMap)
            {
                if (assetID == Boom::EMPTY_ASSET) continue;

                auto* modelAsset = dynamic_cast<Boom::ModelAsset*>(assetPtr.get());
                if (modelAsset && modelAsset->data)
                {
                    std::string displayName = modelAsset->name;
                    if (modelAsset->hasJoints)
                    {
                        displayName += " (Skeletal)";
                    }

                    if (ImGui::Selectable(displayName.c_str()))
                    {
                        LoadModel(modelAsset->name);
                        ImGui::CloseCurrentPopup();
                    }

                    // Tooltip with full path
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        ImGui::Text("Source: %s", modelAsset->source.c_str());
                        ImGui::Text("Has Joints: %s", modelAsset->hasJoints ? "Yes" : "No");
                        ImGui::EndTooltip();
                    }
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No models available");
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(60, 0)) && m_HasModel)
    {
        ClearModel();
    }

    ImGui::SameLine();
    if (ImGui::Button("Frame Model", ImVec2(100, 0)) && m_HasModel)
    {
        FrameModel();
    }

    // Current model info
    if (m_HasModel)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("| Model: %s", m_LoadedModelPath.c_str());
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

    // Camera controls info
    ImGui::SameLine();
    ImGui::TextDisabled("| Right-Click+Drag: Orbit | Scroll: Zoom");

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

    // Update camera
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

    // Handle camera controls AFTER image is rendered (so IsItemHovered works)
    HandleCameraControls();
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

    // Set up camera (creates frustum matrix internally)
    Boom::Camera3D camera{};
    camera.FOV = 45.0f;
    camera.nearPlane = 0.1f;
    camera.farPlane = 100.0f;

    Boom::Transform3D cameraTransform{};
    cameraTransform.translate = m_CameraPosition;

    m_Ctx->renderer->SetCamera(camera, cameraTransform);

    // Set joints if model has skeleton
    if (m_Animator)
    {
        auto transforms = m_Animator->Animate(0.0f); // Get current transforms
        m_Ctx->renderer->SetJoints(transforms);
    }

    // Draw the model (identity transform since we're in model space)
    Boom::Transform3D modelTransform{};
    modelTransform.translate = glm::vec3(0.0f);
    modelTransform.rotate = glm::vec3(0.0f);
    modelTransform.scale = glm::vec3(1.0f);

    // Use a default material (gray)
    Boom::PbrMaterial material{};
    material.albedo = glm::vec3(0.7f, 0.7f, 0.7f);
    material.roughness = 0.5f;
    material.metallic = 0.0f;

    // Draw takes Model3D which is shared_ptr<Model>
    m_Ctx->renderer->Draw(m_Model, modelTransform, material);
}

void ModelPreviewPanel::RenderSkeleton()
{
    if (!m_Animator || !m_Ctx || !m_Owner) return;

    // Get the debug lines shader from the main application
    auto debugShader = m_Owner->GetDebugLinesShader();
    if (!debugShader) return;

    // Get view and projection matrices
    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // Get skeleton lines from animator (in model space)
    auto boneLines = m_Animator->GetSkeletonLines();
    if (boneLines.empty()) return;

    // Convert bone lines to LineVert format
    std::vector<Boom::LineVert> normalBones;
    std::vector<Boom::LineVert> selectedBones;
    normalBones.reserve(boneLines.size() * 2);
    selectedBones.reserve(10);

    glm::vec4 boneColor = m_Ctx->BoneColor;
    glm::vec4 selectedBoneColor = m_Ctx->SelectedBoneColor;

    for (const auto& boneLine : boneLines)
    {
        bool isSelected = (!m_SelectedBoneName.empty() && boneLine.boneName == m_SelectedBoneName);

        if (isSelected)
        {
            selectedBones.push_back({ boneLine.start, selectedBoneColor });
            selectedBones.push_back({ boneLine.end, selectedBoneColor });
        }
        else
        {
            normalBones.push_back({ boneLine.start, boneColor });
            normalBones.push_back({ boneLine.end, boneColor });
        }
    }

    // Draw normal bones first (with X-ray mode - depth test disabled)
    if (!normalBones.empty())
    {
        debugShader->Draw(view, proj, normalBones, m_Ctx->BoneLineWidth, true);
    }

    // Draw selected bone on top with thicker line
    if (!selectedBones.empty())
    {
        debugShader->Draw(view, proj, selectedBones, m_Ctx->BoneLineWidth * 3.0f, true);
    }
}

void ModelPreviewPanel::RenderGrid()
{
    if (!m_Owner || !m_Ctx) return;

    auto debugShader = m_Owner->GetDebugLinesShader();
    if (!debugShader) return;

    // Get view and projection matrices
    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // Create grid lines on XZ plane (Y=0)
    std::vector<Boom::LineVert> gridLines;
    const int gridSize = 10;
    const float gridStep = 1.0f;
    const glm::vec4 gridColor(0.3f, 0.3f, 0.3f, 1.0f);
    const glm::vec4 axisXColor(0.6f, 0.2f, 0.2f, 1.0f); // Red for X axis
    const glm::vec4 axisZColor(0.2f, 0.2f, 0.6f, 1.0f); // Blue for Z axis

    // Grid lines parallel to Z axis
    for (int i = -gridSize; i <= gridSize; ++i)
    {
        float x = i * gridStep;
        glm::vec3 start(x, 0.0f, -gridSize * gridStep);
        glm::vec3 end(x, 0.0f, gridSize * gridStep);

        // Use axis color for center lines, otherwise grid color
        glm::vec4 color = (i == 0) ? axisZColor : gridColor;

        gridLines.push_back({ start, color });
        gridLines.push_back({ end, color });
    }

    // Grid lines parallel to X axis
    for (int i = -gridSize; i <= gridSize; ++i)
    {
        float z = i * gridStep;
        glm::vec3 start(-gridSize * gridStep, 0.0f, z);
        glm::vec3 end(gridSize * gridStep, 0.0f, z);

        // Use axis color for center lines, otherwise grid color
        glm::vec4 color = (i == 0) ? axisXColor : gridColor;

        gridLines.push_back({ start, color });
        gridLines.push_back({ end, color });
    }

    // Draw grid lines
    if (!gridLines.empty())
    {
        debugShader->Draw(view, proj, gridLines, 1.0f, false);
    }
}

void ModelPreviewPanel::LoadModel(const std::string& modelPath)
{
    if (!m_Ctx || !m_Ctx->assets) return;

    BOOM_INFO("[ModelPreviewPanel] Loading model: {}", modelPath);

    // Clear previous model
    ClearModel();

    // Search asset registry for model matching the path
    auto& modelMap = m_Ctx->assets->GetMap<Boom::ModelAsset>();

    Boom::ModelAsset* foundAsset = nullptr;
    Boom::AssetID foundID = Boom::EMPTY_ASSET;

    for (auto& [assetID, assetPtr] : modelMap)
    {
        if (assetID == Boom::EMPTY_ASSET) continue;

        auto* modelAsset = dynamic_cast<Boom::ModelAsset*>(assetPtr.get());
        if (modelAsset && (modelAsset->source == modelPath || modelAsset->name == modelPath))
        {
            foundAsset = modelAsset;
            foundID = assetID;
            break;
        }
    }

    if (!foundAsset || !foundAsset->data)
    {
        BOOM_ERROR("[ModelPreviewPanel] Model not found in asset registry: {}", modelPath);
        BOOM_INFO("[ModelPreviewPanel] Available models:");
        for (auto& [assetID, assetPtr] : modelMap)
        {
            if (assetID == Boom::EMPTY_ASSET) continue;
            auto* modelAsset = dynamic_cast<Boom::ModelAsset*>(assetPtr.get());
            if (modelAsset)
            {
                BOOM_INFO("  - {} (source: {})", modelAsset->name, modelAsset->source);
            }
        }
        return;
    }

    // Load the model
    m_Model = foundAsset->data;
    m_LoadedModelPath = modelPath;
    m_HasModel = true;

    BOOM_INFO("[ModelPreviewPanel] Model loaded successfully: {} (HasJoints: {})",
              foundAsset->name, foundAsset->hasJoints);

    // Get animator from skeletal model if it has skeleton
    if (foundAsset->hasJoints && m_Model->HasJoint())
    {
        // Cast to SkeletalModel to access GetAnimator()
        auto skeletalModel = std::dynamic_pointer_cast<Boom::SkeletalModel>(m_Model);
        if (skeletalModel)
        {
            m_Animator = skeletalModel->GetAnimator();
            if (m_Animator)
            {
                BOOM_INFO("[ModelPreviewPanel] Animator found for skeletal model");
            }
            else
            {
                BOOM_WARN("[ModelPreviewPanel] Model has joints but no animator");
            }
        }
        else
        {
            BOOM_WARN("[ModelPreviewPanel] Model has joints but is not SkeletalModel");
        }
    }
    else
    {
        m_Animator.reset();
        BOOM_INFO("[ModelPreviewPanel] No animator needed (static model)");
    }

    // Auto-frame the model to fit it in view
    FrameModel();
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

void ModelPreviewPanel::FrameModel()
{
    if (!m_Model) return;

    // Calculate model bounding box
    // For simplicity, estimate based on model transform and a default size
    // A more accurate approach would iterate through all vertices
    glm::vec3 modelMin = m_Model->modelTransform.translate - m_Model->modelTransform.scale;
    glm::vec3 modelMax = m_Model->modelTransform.translate + m_Model->modelTransform.scale;

    // Calculate bounding sphere radius
    glm::vec3 center = (modelMin + modelMax) * 0.5f;
    glm::vec3 extent = modelMax - modelMin;
    float radius = glm::length(extent) * 0.5f;

    // If radius is too small (default transform), use a reasonable default
    if (radius < 0.1f)
    {
        radius = 2.0f;
    }

    // Set camera target to model center
    m_CameraTarget = center;

    // Calculate camera distance to fit model in view (45 degree FOV)
    // Distance = radius / tan(FOV/2)
    float fovRadians = glm::radians(45.0f);
    m_CameraDistance = (radius * 1.5f) / std::tan(fovRadians * 0.5f);

    // Clamp to reasonable range
    m_CameraDistance = glm::clamp(m_CameraDistance, 1.0f, 50.0f);

    UpdateCamera();

    BOOM_INFO("[ModelPreviewPanel] Framed model - Distance: {:.2f}, Center: ({:.2f}, {:.2f}, {:.2f})",
              m_CameraDistance, center.x, center.y, center.z);
}
