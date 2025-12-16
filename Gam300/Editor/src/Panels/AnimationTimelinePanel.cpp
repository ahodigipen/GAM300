#include "Panels/AnimationTimelinePanel.h"
#include "Editor.h"
#include "Application/Interface.h"
#include "Application/Context.h"
#include "ECS/ECS.hpp"
#include "Graphics/Models/Model.h"
#include "Graphics/Models/Animator.h"
#include "Graphics/Shaders/DebugLines.h"
#include "Graphics/Shaders/PBR.h"
#include "Graphics/Utilities/Data.h"
#include "Auxiliaries/Assets.h"
#include "Vendors/imgui/imgui.h"
#include "common/Core.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cmath>

using namespace EditorUI;

AnimationTimelinePanel::AnimationTimelinePanel(Editor* owner)
    : m_Owner(owner)
    , m_App(owner ? static_cast<Boom::AppInterface*>(owner) : nullptr)
    , m_Ctx(m_App ? m_App->GetContext() : nullptr)
{
    // Create framebuffer for 3D viewport
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
        BOOM_ERROR("[AnimationTimelinePanel] Framebuffer is not complete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ResetCamera();
}

AnimationTimelinePanel::~AnimationTimelinePanel()
{
    if (m_FramebufferID) glDeleteFramebuffers(1, &m_FramebufferID);
    if (m_TextureID) glDeleteTextures(1, &m_TextureID);
    if (m_DepthBufferID) glDeleteRenderbuffers(1, &m_DepthBufferID);
}

void AnimationTimelinePanel::Render()
{
    if (!m_Owner || !m_App) return;

    // Create the animation editor window
    ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
    ImGui::Begin("Animation Timeline", &m_Owner->m_ShowAnimationTimeline);

    // Get selected entity and load model/animator (only if not in standalone mode)
    auto selectedID = m_App->SelectedEntity();

    // Only try to load from entity if we're not in standalone mode
    if (!m_StandaloneMode && selectedID != entt::null)
    {
        Boom::Entity selected(&m_App->GetContext()->scene, selectedID);

        // Get animator from AnimatorComponent
        if (selected.Has<Boom::AnimatorComponent>())
        {
            auto& animComp = selected.Get<Boom::AnimatorComponent>();
            m_Animator = animComp.animator;
            BOOM_INFO("[AnimationTimeline] Entity has AnimatorComponent - Animator: {}", m_Animator ? "Valid" : "Null");
        }
        else
        {
            m_Animator.reset();
        }

        // Get model from ModelComponent
        if (selected.Has<Boom::ModelComponent>())
        {
            auto& modelComp = selected.Get<Boom::ModelComponent>();
            BOOM_INFO("[AnimationTimeline] Entity has ModelComponent - ModelID: {}", modelComp.modelID);

            if (modelComp.modelID != Boom::EMPTY_ASSET && m_Ctx && m_Ctx->assets)
            {
                Boom::ModelAsset* modelAsset = m_Ctx->assets->TryGet<Boom::ModelAsset>(modelComp.modelID);
                if (modelAsset && modelAsset->data)
                {
                    m_Model = modelAsset->data;
                    m_HasModel = true;
                    BOOM_INFO("[AnimationTimeline] Model loaded from entity: {}", modelComp.modelName);
                }
                else
                {
                    if (!m_StandaloneMode)
                    {
                        m_Model.reset();
                        m_HasModel = false;
                    }
                    BOOM_ERROR("[AnimationTimeline] Failed to load model from asset registry");
                }
            }
            else
            {
                if (!m_StandaloneMode)
                {
                    m_Model.reset();
                    m_HasModel = false;
                }
            }
        }
        else
        {
            if (!m_StandaloneMode)
            {
                m_Model.reset();
                m_HasModel = false;
            }
        }
    }

    // CHUNK 2: Detect animator change and auto-select first clip
    if (m_Animator != m_PreviousAnimator)
    {
        m_PreviousAnimator = m_Animator;

        if (m_Animator && m_Animator->GetClipCount() > 0)
        {
            m_SelectedClipIndex = 0;
            m_Animator->PlayClip(0);
            m_CurrentTime = 0.0f;
            BOOM_INFO("[AnimationTimeline] Animator changed - Auto-selected first clip: {}",
                m_Animator->GetClip(0)->name);
        }
        else
        {
            m_SelectedClipIndex = -1;
            m_CurrentTime = 0.0f;
        }
    }

    // --- LAYOUT: Four sections ---

    // Section 1: Control Bar (top - fixed height)
    RenderControlBar();

    ImGui::Separator();

    // Section 2: 3D Viewport (middle-top - resizable)
    RenderViewport();

    ImGui::Separator();

    // Section 3: Timeline Ruler (middle-bottom - fixed height)
    RenderTimelineRuler();

    ImGui::Separator();

    // Section 4: Track List (bottom - remaining space)
    RenderTrackList();

    ImGui::End();
}

void AnimationTimelinePanel::RenderControlBar()
{
    // Top bar with model loading, playback controls, and visualization options
    ImGui::BeginGroup();

    // Model loading button
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
    ImGui::Separator();
    ImGui::SameLine();

    // Playback controls (will be functional in Chunk 4)
    ImGui::BeginDisabled(!m_HasModel);
    if (ImGui::Button(m_IsPlaying ? "Pause" : "Play")) {
        m_IsPlaying = !m_IsPlaying;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        m_IsPlaying = false;
        m_CurrentTime = 0.0f;
    }
    ImGui::EndDisabled();

    ImGui::SameLine(0, 20);
    ImGui::Separator();
    ImGui::SameLine(0, 20);

    // Visualization toggles
    ImGui::Checkbox("Show Skeleton", &m_ShowSkeleton);
    ImGui::SameLine();
    ImGui::Checkbox("Show Grid", &m_ShowGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Wireframe", &m_ShowWireframe);

    ImGui::SameLine();
    if (ImGui::Button("Reset Camera")) {
        ResetCamera();
    }

    if (m_HasModel) {
        ImGui::SameLine();
        if (ImGui::Button("Frame Model")) {
            FrameModel();
        }
    }

    // Scale controls (new row)
    if (m_HasModel)
    {
        ImGui::Text("Preview Scale:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);

        // Use logarithmic slider for better control at small values
        if (ImGui::SliderFloat("##ModelScale", &m_ModelScale, 0.001f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
        {
            // Scale changed - optionally re-frame
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset Scale"))
        {
            m_ModelScale = 1.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("0.01x"))
        {
            m_ModelScale = 0.01f;
        }
        ImGui::SameLine();
        if (ImGui::Button("0.1x"))
        {
            m_ModelScale = 0.1f;
        }

        // Show info
        ImGui::SameLine();
        if (m_Model)
        {
            ImGui::TextDisabled("| Asset Scale: (%.2f, %.2f, %.2f) | Camera Dist: %.2f",
                m_Model->modelTransform.scale.x,
                m_Model->modelTransform.scale.y,
                m_Model->modelTransform.scale.z,
                m_CameraDistance);
        }
    }

    // ===== CHUNK 2: Animation Clip Selection & Info Display =====
    if (m_Animator && m_Animator->GetClipCount() > 0)
    {
        ImGui::Separator();
        ImGui::Text("Animation Clip:");
        ImGui::SameLine();

        // Animation clip dropdown
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##AnimClip",
            m_SelectedClipIndex >= 0 ? m_Animator->GetClip(m_SelectedClipIndex)->name.c_str() : "Select clip..."))
        {
            for (size_t i = 0; i < m_Animator->GetClipCount(); ++i)
            {
                const auto* clip = m_Animator->GetClip(i);
                if (!clip) continue;

                bool isSelected = (m_SelectedClipIndex == (int)i);
                if (ImGui::Selectable(clip->name.c_str(), isSelected))
                {
                    m_SelectedClipIndex = (int)i;
                    m_Animator->PlayClip(i);  // Switch to this clip
                    m_CurrentTime = 0.0f;     // Reset time
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Display clip information
        if (m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
        {
            const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
            if (clip)
            {
                ImGui::SameLine();
                ImGui::Text("|");

                // Duration display
                ImGui::SameLine();
                ImGui::Text("Duration: %.2fs", clip->duration);

                // FPS / Ticks per second
                ImGui::SameLine();
                ImGui::Text("| FPS: %.1f", clip->ticksPerSecond);

                // Frame count (approximate)
                int frameCount = (int)(clip->duration * clip->ticksPerSecond);
                ImGui::SameLine();
                ImGui::Text("| Frames: %d", frameCount);

                // Current time / Total time
                ImGui::SameLine();
                ImGui::Text("| Time: %.2f / %.2f", m_CurrentTime, clip->duration);
            }
        }
    }
    else if (m_Animator)
    {
        ImGui::Separator();
        ImGui::TextDisabled("No animation clips available");
    }

    ImGui::EndGroup();
}

void AnimationTimelinePanel::RenderViewport()
{
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    // Take half of remaining space for viewport
    ImVec2 viewportSize = ImVec2(availableSize.x, availableSize.y * 0.5f);

    // Resize framebuffer if viewport size changed
    if (viewportSize.x != m_ViewportSize.x || viewportSize.y != m_ViewportSize.y)
    {
        if (viewportSize.x > 0 && viewportSize.y > 0)
        {
            m_ViewportSize = viewportSize;

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
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Set up lighting for preview
    if (m_HasModel && m_Ctx && m_Ctx->renderer)
    {
        std::vector<Boom::GPUDirLight> dirLights(1);
        dirLights[0].dir_intensity = glm::vec4(glm::normalize(glm::vec3(1.0f, -1.0f, 1.0f)), 1.0f);
        dirLights[0].radiance = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_Ctx->renderer->UploadDirLights(dirLights, 1);
        m_Ctx->renderer->AmbientStrength() = 0.3f;
    }

    // Set wireframe mode if requested
    if (m_ShowWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // Render scene
    if (m_ShowGrid) RenderGrid();
    if (m_HasModel) RenderModel();
    if (m_HasModel && m_ShowSkeleton) RenderSkeleton();

    // Reset polygon mode
    if (m_ShowWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Display framebuffer texture in ImGui
    ImVec2 viewportPos = ImGui::GetCursorScreenPos();
    ImGui::Image(
        (ImTextureID)(intptr_t)m_TextureID,
        m_ViewportSize,
        ImVec2(0, 1), // UV top-left
        ImVec2(1, 0)  // UV bottom-right (flip Y)
    );

    // Show message overlay if no model loaded
    if (!m_HasModel)
    {
        ImVec2 textPos = ImVec2(
            viewportPos.x + m_ViewportSize.x * 0.5f - 150,
            viewportPos.y + m_ViewportSize.y * 0.5f - 40
        );
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Draw semi-transparent background
        drawList->AddRectFilled(
            ImVec2(textPos.x - 10, textPos.y - 10),
            ImVec2(textPos.x + 310, textPos.y + 90),
            IM_COL32(0, 0, 0, 200)
        );

        // Draw text
        drawList->AddText(ImVec2(textPos.x, textPos.y),
                         IM_COL32(200, 200, 200, 255),
                         "No model loaded");
        drawList->AddText(ImVec2(textPos.x, textPos.y + 25),
                         IM_COL32(150, 150, 150, 255),
                         "Click 'Load Model' to select a model");
        drawList->AddText(ImVec2(textPos.x, textPos.y + 50),
                         IM_COL32(150, 150, 150, 255),
                         "OR select an entity with AnimatorComponent");
    }

    // Handle camera controls AFTER image is rendered
    HandleCameraControls();
}

void AnimationTimelinePanel::RenderTimelineRuler()
{
    // Middle section with time ruler and scrubber (will be implemented in Chunk 3)
    ImGui::BeginGroup();

    ImGui::Text("TIMELINE RULER");
    ImGui::SameLine();
    ImGui::TextDisabled("(Time scrubber will appear here in Chunk 3)");

    // Placeholder ruler visual
    ImVec2 rulerSize = ImVec2(ImGui::GetContentRegionAvail().x, 40);
    ImVec2 rulerPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw ruler background
    drawList->AddRectFilled(rulerPos,
                           ImVec2(rulerPos.x + rulerSize.x, rulerPos.y + rulerSize.y),
                           IM_COL32(40, 40, 40, 255));

    // Draw ruler border
    drawList->AddRect(rulerPos,
                     ImVec2(rulerPos.x + rulerSize.x, rulerPos.y + rulerSize.y),
                     IM_COL32(100, 100, 100, 255));

    ImGui::Dummy(rulerSize);

    ImGui::EndGroup();
}

void AnimationTimelinePanel::RenderTrackList()
{
    // Bottom section with bone tracks (will be populated in Chunk 5)
    ImGui::BeginGroup();

    ImGui::Text("TRACK LIST");
    ImGui::SameLine();
    ImGui::TextDisabled("(Bone tracks and keyframes will appear here in Chunk 5)");

    if (ImGui::BeginChild("TrackListScroll", ImVec2(0, 0), true))
    {
        ImGui::TextDisabled("Bone tracks will be displayed here...");
        ImGui::Spacing();
        ImGui::BulletText("Position tracks");
        ImGui::BulletText("Rotation tracks");
        ImGui::BulletText("Scale tracks");
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

// ========== Camera Functions ==========

void AnimationTimelinePanel::UpdateCamera()
{
    // Calculate camera position based on spherical coordinates
    float x = m_CameraDistance * cos(m_CameraPitch) * sin(m_CameraYaw);
    float y = m_CameraDistance * sin(m_CameraPitch);
    float z = m_CameraDistance * cos(m_CameraPitch) * cos(m_CameraYaw);

    m_CameraPosition = m_CameraTarget + glm::vec3(x, y, z);
}

void AnimationTimelinePanel::HandleCameraControls()
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
        m_CameraYaw -= delta.x * 0.005f;
        m_CameraPitch -= delta.y * 0.005f;

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
        m_CameraDistance -= io.MouseWheel * 0.5f;
        m_CameraDistance = glm::clamp(m_CameraDistance, 0.1f, 100.0f);
    }
}

void AnimationTimelinePanel::ResetCamera()
{
    m_CameraDistance = 3.0f;
    m_CameraYaw = 0.0f;
    m_CameraPitch = 0.3f;
    m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
    UpdateCamera();
}

void AnimationTimelinePanel::FrameModel()
{
    if (!m_Model) return;

    float radius = 2.0f * m_ModelScale;
    m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);

    float fovRadians = glm::radians(45.0f);
    m_CameraDistance = (radius * 2.5f) / std::tan(fovRadians * 0.5f);
    m_CameraDistance = glm::clamp(m_CameraDistance, 0.5f, 100.0f);

    UpdateCamera();
}

// ========== 3D Rendering Functions ==========

void AnimationTimelinePanel::RenderModel()
{
    if (!m_Ctx || !m_Ctx->renderer || !m_Model) return;

    // Set aspect ratio for viewport
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    m_Ctx->renderer->SetAspectOverride(aspect);

    // Set up camera
    Boom::Camera3D camera{};
    camera.FOV = 45.0f;
    camera.nearPlane = 0.01f;
    camera.farPlane = 1000.0f;

    // Compute camera transform
    glm::vec3 direction = glm::normalize(m_CameraTarget - m_CameraPosition);
    float yaw = atan2(-direction.x, -direction.z);
    float pitch = asin(direction.y);
    float roll = 0.0f;

    glm::vec3 eulerAngles = glm::vec3(glm::degrees(pitch), glm::degrees(yaw), glm::degrees(roll));

    Boom::Transform3D cameraTransform{};
    cameraTransform.translate = m_CameraPosition;
    cameraTransform.rotate = eulerAngles;
    cameraTransform.scale = glm::vec3(1.0f);

    m_Ctx->renderer->SetCamera(camera, cameraTransform);

    // Set joints if model has skeleton
    if (m_Animator)
    {
        auto transforms = m_Animator->Animate(m_CurrentTime);
        m_Ctx->renderer->SetJoints(transforms);
    }

    // CRITICAL: Call SetCamera again right before Draw in case something overwrote it
    m_Ctx->renderer->SetCamera(camera, cameraTransform);

    // Model transform (with preview scale)
    glm::mat4 desiredMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));
    glm::mat4 assetMatrix = m_Model->modelTransform.Matrix();
    glm::mat4 finalMatrix = desiredMatrix * glm::inverse(assetMatrix);

    glm::vec3 finalTranslate = glm::vec3(finalMatrix[3]);
    glm::vec3 finalScale;
    finalScale.x = glm::length(glm::vec3(finalMatrix[0]));
    finalScale.y = glm::length(glm::vec3(finalMatrix[1]));
    finalScale.z = glm::length(glm::vec3(finalMatrix[2]));

    glm::mat4 rotMatrix = finalMatrix;
    rotMatrix[0] /= finalScale.x;
    rotMatrix[1] /= finalScale.y;
    rotMatrix[2] /= finalScale.z;
    glm::quat modelRotQuat = glm::quat_cast(rotMatrix);
    glm::vec3 finalRotate = glm::degrees(glm::eulerAngles(modelRotQuat));

    Boom::Transform3D modelTransform{};
    modelTransform.translate = finalTranslate;
    modelTransform.rotate = finalRotate;
    modelTransform.scale = finalScale;

    // Default material
    Boom::PbrMaterial material{};
    material.albedo = glm::vec3(0.7f, 0.7f, 0.7f);
    material.roughness = 0.5f;
    material.metallic = 0.0f;

    // Draw model
    m_Ctx->renderer->Draw(m_Model, modelTransform, material);

    m_Ctx->renderer->ClearAspectOverride();
}

void AnimationTimelinePanel::RenderSkeleton()
{
    if (!m_Animator || !m_Ctx || !m_Owner) return;

    auto debugShader = m_Owner->GetDebugLinesShader();
    if (!debugShader) return;

    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    auto boneLines = m_Animator->GetSkeletonLines();
    if (boneLines.empty()) return;

    std::vector<Boom::LineVert> normalBones;
    std::vector<Boom::LineVert> selectedBones;
    normalBones.reserve(boneLines.size() * 2);

    glm::vec4 boneColor = m_Ctx->BoneColor;
    glm::vec4 selectedBoneColor = m_Ctx->SelectedBoneColor;

    glm::mat4 boneTransform = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));

    for (const auto& boneLine : boneLines)
    {
        bool isSelected = (!m_SelectedBoneName.empty() && boneLine.boneName == m_SelectedBoneName);

        glm::vec4 start4 = boneTransform * glm::vec4(boneLine.start, 1.0f);
        glm::vec4 end4 = boneTransform * glm::vec4(boneLine.end, 1.0f);
        glm::vec3 scaledStart = glm::vec3(start4);
        glm::vec3 scaledEnd = glm::vec3(end4);

        if (isSelected)
        {
            selectedBones.push_back({ scaledStart, selectedBoneColor });
            selectedBones.push_back({ scaledEnd, selectedBoneColor });
        }
        else
        {
            normalBones.push_back({ scaledStart, boneColor });
            normalBones.push_back({ scaledEnd, boneColor });
        }
    }

    if (!normalBones.empty())
    {
        debugShader->Draw(view, proj, normalBones, m_Ctx->BoneLineWidth, true);
    }

    if (!selectedBones.empty())
    {
        debugShader->Draw(view, proj, selectedBones, m_Ctx->BoneLineWidth * 3.0f, true);
    }
}

void AnimationTimelinePanel::RenderGrid()
{
    if (!m_Owner || !m_Ctx) return;

    auto debugShader = m_Owner->GetDebugLinesShader();
    if (!debugShader) return;

    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    std::vector<Boom::LineVert> gridLines;
    const int gridSize = 10;
    const float gridStep = 1.0f;
    const glm::vec4 gridColor(0.3f, 0.3f, 0.3f, 1.0f);
    const glm::vec4 axisXColor(0.6f, 0.2f, 0.2f, 1.0f);
    const glm::vec4 axisZColor(0.2f, 0.2f, 0.6f, 1.0f);

    // Grid lines parallel to Z axis
    for (int i = -gridSize; i <= gridSize; ++i)
    {
        float x = i * gridStep;
        glm::vec3 start(x, 0.0f, -gridSize * gridStep);
        glm::vec3 end(x, 0.0f, gridSize * gridStep);
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
        glm::vec4 color = (i == 0) ? axisXColor : gridColor;
        gridLines.push_back({ start, color });
        gridLines.push_back({ end, color });
    }

    if (!gridLines.empty())
    {
        debugShader->Draw(view, proj, gridLines, 1.0f, false);
    }
}

// ========== Model Loading (Standalone Mode) ==========

void AnimationTimelinePanel::LoadModel(const std::string& modelPath)
{
    if (!m_Ctx || !m_Ctx->assets) return;

    BOOM_INFO("[AnimationTimeline] Loading model in standalone mode: {}", modelPath);

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
        BOOM_ERROR("[AnimationTimeline] Model not found in asset registry: {}", modelPath);
        return;
    }

    // Load the model
    m_Model = foundAsset->data;
    m_LoadedModelPath = modelPath;
    m_HasModel = true;
    m_StandaloneMode = true;

    BOOM_INFO("[AnimationTimeline] Model loaded successfully in standalone mode: {} (HasJoints: {})",
              foundAsset->name, foundAsset->hasJoints);

    // Get animator from skeletal model if it has skeleton
    if (foundAsset->hasJoints && m_Model->HasJoint())
    {
        auto skeletalModel = std::dynamic_pointer_cast<Boom::SkeletalModel>(m_Model);
        if (skeletalModel)
        {
            m_Animator = skeletalModel->GetAnimator();
            if (m_Animator)
            {
                BOOM_INFO("[AnimationTimeline] Animator found for skeletal model");

                // CHUNK 2: Auto-select first animation clip if available
                if (m_Animator->GetClipCount() > 0)
                {
                    m_SelectedClipIndex = 0;
                    m_Animator->PlayClip(0);
                    m_CurrentTime = 0.0f;
                    BOOM_INFO("[AnimationTimeline] Auto-selected first clip: {}",
                        m_Animator->GetClip(0)->name);
                }
                else
                {
                    m_SelectedClipIndex = -1;
                    BOOM_WARN("[AnimationTimeline] Animator has no clips");
                }
            }
            else
            {
                BOOM_WARN("[AnimationTimeline] Model has joints but no animator");
                m_SelectedClipIndex = -1;
            }
        }
        else
        {
            BOOM_WARN("[AnimationTimeline] Model has joints but is not SkeletalModel");
            m_SelectedClipIndex = -1;
        }
    }
    else
    {
        m_Animator.reset();
        m_SelectedClipIndex = -1;
        BOOM_INFO("[AnimationTimeline] No animator needed (static model)");
    }

    // Reset preview scale when loading new model
    m_ModelScale = 1.0f;

    // Auto-frame the model to fit it in view
    FrameModel();
}

void AnimationTimelinePanel::ClearModel()
{
    m_Model.reset();
    m_Animator.reset();
    m_PreviousAnimator.reset();  // CHUNK 2: Reset previous animator
    m_HasModel = false;
    m_StandaloneMode = false;
    m_LoadedModelPath.clear();
    m_SelectedBoneName.clear();
    m_SelectedClipIndex = -1;  // CHUNK 2: Reset clip selection
    m_CurrentTime = 0.0f;

    BOOM_INFO("[AnimationTimeline] Model cleared");
}
