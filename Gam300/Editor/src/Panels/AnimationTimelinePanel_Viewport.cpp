// ===================================================================
// AnimationTimelinePanel_Viewport.cpp
// 3D Viewport, Camera, Bone Picking, Gizmo, and Rendering Functions
// ===================================================================

#include "Panels/AnimationTimelinePanel.h"
#include "Editor.h"
#include "Application/Interface.h"
#include "Application/Context.h"
#include "Commands/UndoRedo.h"
#include "ECS/ECS.hpp"
#include "Graphics/Models/Model.h"
#include "Graphics/Models/Animator.h"
#include "Graphics/Models/Animation.h"
#include "Graphics/Shaders/DebugLines.h"
#include "Graphics/Shaders/PBR.h"
#include "Graphics/Utilities/Data.h"
#include "Auxiliaries/Assets.h"
#include "Vendors/imgui/imgui.h"
#include "Vendors/imGuizmo/ImGuizmo.h"
#include "common/Core.h"
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <cmath>
#include <functional>

using namespace EditorUI;

// ========== 3D Viewport Rendering ==========

void AnimationTimelinePanel::RenderViewport()
{
    ImVec2 availableSize = ImGui::GetContentRegionAvail();

    // Use user-adjustable viewport ratio (stored in m_ViewportHeightRatio)
    // Clamp to ensure both viewport and timeline have minimum usable space
    float viewportHeight = availableSize.y * m_ViewportHeightRatio;
    viewportHeight = (viewportHeight < 200.0f) ? 200.0f : viewportHeight;
    viewportHeight = (viewportHeight > availableSize.y - 150.0f) ? availableSize.y - 150.0f : viewportHeight;
    ImVec2 viewportSize = ImVec2(availableSize.x, viewportHeight);

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

    // ===== SAVE RENDERER STATE (to prevent timeline from affecting game scene) =====
    // CRITICAL: The renderer is SHARED between timeline and game scene
    // Render order: Game scene FIRST, then Timeline SECOND
    // We must save and restore state to prevent cross-contamination
    float savedAmbient = 0.0f;
    if (m_Ctx && m_Ctx->renderer)
    {
        savedAmbient = m_Ctx->renderer->AmbientStrength();
    }

    // Render to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);

    // CRITICAL: Check if framebuffer is still valid after scene reload
    GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        BOOM_ERROR("[AnimationTimeline] Framebuffer invalid (status: 0x{:X}) - recreating!", fbStatus);

        // Unbind before deleting
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Delete old framebuffer resources
        if (m_FramebufferID) glDeleteFramebuffers(1, &m_FramebufferID);
        if (m_TextureID) glDeleteTextures(1, &m_TextureID);
        if (m_DepthBufferID) glDeleteRenderbuffers(1, &m_DepthBufferID);

        // Recreate framebuffer
        glGenFramebuffers(1, &m_FramebufferID);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FramebufferID);

        // Recreate color texture
        glGenTextures(1, &m_TextureID);
        glBindTexture(GL_TEXTURE_2D, m_TextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureID, 0);

        // Recreate depth buffer
        glGenRenderbuffers(1, &m_DepthBufferID);
        glBindRenderbuffer(GL_RENDERBUFFER, m_DepthBufferID);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthBufferID);

        // Verify new framebuffer
        GLenum newStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (newStatus == GL_FRAMEBUFFER_COMPLETE)
        {
            BOOM_INFO("[AnimationTimeline] Framebuffer recreated successfully!");
        }
        else
        {
            BOOM_ERROR("[AnimationTimeline] Failed to recreate framebuffer (status: 0x{:X})", newStatus);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;  // Abort rendering if framebuffer is broken
        }
    }

    glViewport(0, 0, (GLsizei)m_ViewportSize.x, (GLsizei)m_ViewportSize.y);

    // Clear
    glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Set up lighting for preview (temporary - will restore after)
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

    // ===== RESTORE RENDERER STATE (critical to prevent leaking to game scene) =====
    if (m_Ctx && m_Ctx->renderer)
    {
        // CRITICAL: Clear joint transforms to prevent affecting game scene models
        // Must use identity matrices (like Application.cpp does), NOT empty vector!
        // Empty vector doesn't upload to GPU, leaving stale timeline joints
        // NOTE: NOT static - create fresh each time to force GPU upload
        std::vector<glm::mat4> identityPalette(100, glm::mat4(1.0f));
        m_Ctx->renderer->SetJoints(identityPalette);

        // Restore ambient strength
        m_Ctx->renderer->AmbientStrength() = savedAmbient;

        // Note: Directional lights will be re-uploaded by game scene on next frame
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

    // Handle bone picking for selection in 3D viewport
    HandleBonePicking();

    // Render transform gizmo at selected bone
    // NOTE: Gizmo draws on top of the image, but we already rendered with updated poses
    HandleGizmo(viewportPos, m_ViewportSize);

    // ========== RESIZABLE SPLITTER BAR ==========
    // Add a draggable splitter between viewport and timeline
    // IMPORTANT: This must come AFTER all viewport interactions to avoid stealing mouse events
    const float splitterHeight = 6.0f;  // Thickness of draggable area
    ImVec2 splitterPos = ImGui::GetCursorScreenPos();
    ImVec2 splitterSize = ImVec2(ImGui::GetContentRegionAvail().x, splitterHeight);

    // Invisible button for interaction
    ImGui::InvisibleButton("##ViewportSplitter", splitterSize);
    bool isSplitterHovered = ImGui::IsItemHovered();
    bool isSplitterActive = ImGui::IsItemActive();

    // Change cursor to resize when hovering
    if (isSplitterHovered || m_IsDraggingSplitter)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    // Handle dragging
    if (isSplitterActive)
    {
        if (!m_IsDraggingSplitter)
        {
            m_IsDraggingSplitter = true;
        }

        // Update ratio based on mouse drag
        ImGuiIO& io = ImGui::GetIO();
        if (io.MouseDelta.y != 0.0f)
        {
            // Calculate new ratio based on total available height
            ImVec2 totalAvailable = ImGui::GetContentRegionAvail();
            totalAvailable.y += viewportHeight;  // Add back the viewport height we already used

            float newHeight = viewportHeight + io.MouseDelta.y;
            m_ViewportHeightRatio = newHeight / totalAvailable.y;

            // Clamp ratio to reasonable bounds (20% to 80%)
            m_ViewportHeightRatio = glm::clamp(m_ViewportHeightRatio, 0.2f, 0.8f);
        }
    }
    else
    {
        m_IsDraggingSplitter = false;
    }

    // Draw the visual splitter bar with appropriate color
    ImDrawList* splitterDrawList = ImGui::GetWindowDrawList();
    ImU32 splitterColor = IM_COL32(80, 80, 80, 255);  // Default gray
    ImU32 splitterHoverColor = IM_COL32(120, 150, 200, 255);  // Blue when hovered
    ImU32 splitterActiveColor = IM_COL32(150, 180, 230, 255);  // Bright blue when dragging

    ImU32 currentSplitterColor = isSplitterActive ? splitterActiveColor : (isSplitterHovered ? splitterHoverColor : splitterColor);
    splitterDrawList->AddRectFilled(
        splitterPos,
        ImVec2(splitterPos.x + splitterSize.x, splitterPos.y + splitterHeight),
        currentSplitterColor
    );

    // Add subtle shadow/border for depth
    splitterDrawList->AddLine(
        ImVec2(splitterPos.x, splitterPos.y),
        ImVec2(splitterPos.x + splitterSize.x, splitterPos.y),
        IM_COL32(60, 60, 60, 255),
        1.0f
    );
    splitterDrawList->AddLine(
        ImVec2(splitterPos.x, splitterPos.y + splitterHeight),
        ImVec2(splitterPos.x + splitterSize.x, splitterPos.y + splitterHeight),
        IM_COL32(100, 100, 100, 255),
        1.0f
    );
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
    // Only process input if the viewport is actually hovered
    if (!ImGui::IsItemHovered()) return;

    // Check if Animation Timeline window is focused (prevents input bleed-through)
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        // Another window is focused on top - don't process input
        m_IsOrbitingCamera = false;
        return;
    }

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

// ========== Bone Picking and Manipulation ==========

void AnimationTimelinePanel::HandleBonePicking()
{
    // CRITICAL: Don't pick bones while gizmo is being used OR hovered (prevents click-through)
    if (ImGuizmo::IsUsing()) return;
    if (ImGuizmo::IsOver()) return;  // Also block when mouse is over gizmo (even if not dragging yet)

    // Only process if viewport is hovered and window is focused
    if (!ImGui::IsItemHovered()) return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
    if (!m_Animator || !m_HasModel) return;

    // Get mouse position relative to viewport
    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 viewportScreenPos = ImGui::GetItemRectMin();  // Top-left corner of viewport
    m_ViewportMousePos = ImVec2(
        mousePos.x - viewportScreenPos.x,
        mousePos.y - viewportScreenPos.y
    );

    // Convert to normalized device coordinates [-1, 1]
    float ndcX = (m_ViewportMousePos.x / m_ViewportSize.x) * 2.0f - 1.0f;
    float ndcY = 1.0f - (m_ViewportMousePos.y / m_ViewportSize.y) * 2.0f;  // Flip Y

    // Compute ray in world space
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    glm::mat4 invProjView = glm::inverse(proj * view);

    // Near and far points in world space
    glm::vec4 rayStartNDC(ndcX, ndcY, -1.0f, 1.0f);  // Near plane
    glm::vec4 rayEndNDC(ndcX, ndcY, 1.0f, 1.0f);     // Far plane

    glm::vec4 rayStartWorld = invProjView * rayStartNDC;
    glm::vec4 rayEndWorld = invProjView * rayEndNDC;

    rayStartWorld /= rayStartWorld.w;
    rayEndWorld /= rayEndWorld.w;

    glm::vec3 rayOrigin = glm::vec3(rayStartWorld);
    glm::vec3 rayDir = glm::normalize(glm::vec3(rayEndWorld) - glm::vec3(rayStartWorld));

    // Get bone positions - use manual poses if available for accurate picking during gizmo manipulation
    auto boneLines = m_Animator->GetSkeletonLines();  // Get default lines first

    if (m_HasManualPoses)
    {
        // Rebuild bone lines with manual poses (same logic as RenderSkeleton)
        boneLines.clear();  // Clear and rebuild with manual poses

        const Boom::Joint& root = m_Animator->GetRoot();

        std::function<void(const Boom::Joint&, const glm::vec3&)> buildBoneLinesFunc;
        buildBoneLinesFunc = [&](const Boom::Joint& joint, const glm::vec3& parentPos)
        {
            glm::mat4 worldTransform = GetBoneWorldTransform(joint.name);
            glm::vec3 bonePos = glm::vec3(worldTransform[3]);

            Boom::Animator::BoneLine boneLine;  // Correct type
            boneLine.start = parentPos;
            boneLine.end = bonePos;
            boneLine.boneName = joint.name;
            boneLines.push_back(boneLine);

            for (const auto& child : joint.children)
            {
                buildBoneLinesFunc(child, bonePos);
            }
        };

        glm::mat4 rootTransform = GetBoneWorldTransform(root.name);
        glm::vec3 rootPos = glm::vec3(rootTransform[3]);

        for (const auto& child : root.children)
        {
            buildBoneLinesFunc(child, rootPos);
        }
    }
    // else: keep the default boneLines from GetSkeletonLines()

    if (boneLines.empty()) return;

    // Apply same transform as skeleton rendering
    glm::mat4 boneTransform = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));

    // Find closest bone to ray
    float closestDist = FLT_MAX;
    std::string closestBone;

    // IMPROVED: Smaller, more precise selection radius
    // Use a fixed screen-space equivalent radius that scales with camera distance
    // But clamp it to prevent huge selection areas when far away
    float baseRadius = m_CameraDistance * 0.03f;  // Reduced from 0.08 for better precision
    const float selectionRadius = glm::clamp(baseRadius, 0.05f, 0.3f);  // Clamp between 5cm and 30cm

    for (const auto& boneLine : boneLines)
    {
        // Transform bone positions (same as RenderSkeleton)
        glm::vec4 start4 = boneTransform * glm::vec4(boneLine.start, 1.0f);
        glm::vec4 end4 = boneTransform * glm::vec4(boneLine.end, 1.0f);
        glm::vec3 boneStart = glm::vec3(start4);
        glm::vec3 boneEnd = glm::vec3(end4);

        // Compute distance from ray to bone LINE SEGMENT (not just start point)
        // This makes selection work for entire bone length, not just the joint
        glm::vec3 boneDir = boneEnd - boneStart;
        float boneLength = glm::length(boneDir);

        // Handle zero-length bones (shouldn't happen but be safe)
        if (boneLength < 0.001f)
        {
            boneDir = glm::vec3(0, 1, 0);
            boneLength = 0.001f;
        }
        else
        {
            boneDir /= boneLength;  // Normalize
        }

        // Line-segment to line distance (closest approach between ray and bone segment)
        // Based on: http://geomalgorithms.com/a07-_distance.html
        glm::vec3 w0 = rayOrigin - boneStart;
        float a = glm::dot(rayDir, rayDir);        // Always 1 (ray is normalized)
        float b = glm::dot(rayDir, boneDir);
        float c = glm::dot(boneDir, boneDir);      // Always 1 (bone dir normalized)
        float d = glm::dot(rayDir, w0);
        float e = glm::dot(boneDir, w0);

        float denom = a * c - b * b;
        float sc, tc;

        // Compute parameters for closest points
        if (denom < 0.001f)
        {
            // Lines are parallel
            sc = 0.0f;
            tc = (b > c ? d / b : e / c);
        }
        else
        {
            sc = (b * e - c * d) / denom;
            tc = (a * e - b * d) / denom;
        }

        // Clamp tc to bone segment [0, boneLength]
        tc = glm::clamp(tc, 0.0f, boneLength);

        // Compute closest points
        glm::vec3 closestOnRay = rayOrigin + sc * rayDir;
        glm::vec3 closestOnBone = boneStart + tc * boneDir;

        float dist = glm::length(closestOnRay - closestOnBone);

        // Check if within selection radius and closer than previous bones
        if (dist < selectionRadius && dist < closestDist)
        {
            closestDist = dist;
            closestBone = boneLine.boneName;
        }
    }

    // Update hover state
    m_HoveredBoneNameViewport = closestBone;

    // Handle mouse click for selection
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !closestBone.empty())
    {
        m_SelectedBoneName = closestBone;

        // Sync with global context for track list highlighting (same as RenderBoneTrack does)
        if (m_Ctx)
        {
            m_Ctx->SelectedBoneName = closestBone;
        }
    }
}

glm::mat4 AnimationTimelinePanel::GetBoneWorldTransform(const std::string& boneName)
{
    // Walk the bone hierarchy and accumulate transforms to get world matrix
    // This matches how GetSkeletonLines() computes bone positions

    if (!m_Animator) return glm::mat4(1.0f);

    // Helper lambda to recursively find bone and compute world transform
    std::function<bool(const Boom::Joint&, const glm::mat4&, glm::mat4&)> findBone;
    findBone = [&](const Boom::Joint& joint, const glm::mat4& parentWorld, glm::mat4& outWorld) -> bool
    {
        // Compute local transform for this bone
        glm::mat4 localTransform = glm::mat4(1.0f);

        // Check if we have manual override for this bone
        auto it = m_ManualBonePoses.find(joint.name);
        if (it != m_ManualBonePoses.end())
        {
            // Use manual pose
            const BonePose& pose = it->second;
            localTransform = glm::translate(glm::mat4(1.0f), pose.position);
            localTransform *= glm::mat4_cast(pose.rotation);
            localTransform = glm::scale(localTransform, pose.scale);
        }
        else
        {
            // Get transform from animation
            if (m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
            {
                const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
                const auto* keys = clip->GetTrack(joint.name);

                if (keys && keys->size() >= 2)
                {
                    // Find previous and next keyframes manually (GetPreviousAndNextFrames is private)
                    Boom::KeyFrame prev = (*keys)[0];
                    Boom::KeyFrame next = (*keys)[keys->size() - 1];

                    for (size_t i = 0; i < keys->size() - 1; ++i)
                    {
                        if ((*keys)[i].timeStamp <= m_CurrentTime && (*keys)[i + 1].timeStamp >= m_CurrentTime)
                        {
                            prev = (*keys)[i];
                            next = (*keys)[i + 1];
                            break;
                        }
                    }

                    float progression = 0.0f;
                    float dt = next.timeStamp - prev.timeStamp;
                    if (dt > 0.0f)
                    {
                        progression = (m_CurrentTime - prev.timeStamp) / dt;
                    }

                    // Interpolate between keyframes
                    glm::vec3 pos = glm::mix(prev.position, next.position, progression);
                    glm::quat rot = glm::slerp(prev.rotation, next.rotation, progression);
                    glm::vec3 scl = glm::mix(prev.scale, next.scale, progression);

                    localTransform = glm::translate(glm::mat4(1.0f), pos);
                    localTransform *= glm::mat4_cast(rot);
                    localTransform = glm::scale(localTransform, scl);
                }
            }
        }

        // Compute world transform
        glm::mat4 worldTransform = parentWorld * localTransform;

        // Is this the bone we're looking for?
        if (joint.name == boneName)
        {
            outWorld = worldTransform;
            return true;
        }

        // Search children
        for (const auto& child : joint.children)
        {
            if (findBone(child, worldTransform, outWorld))
                return true;
        }

        return false;
    };

    glm::mat4 result;
    const Boom::Joint& root = m_Animator->GetRoot();
    if (findBone(root, glm::mat4(1.0f), result))
    {
        return result;
    }

    return glm::mat4(1.0f);
}

std::string AnimationTimelinePanel::GetParentBoneName(const std::string& boneName)
{
    if (!m_Animator) return "";

    const Boom::Joint& root = m_Animator->GetRoot();

    // Lambda to search for parent
    std::function<bool(const Boom::Joint&, std::string&)> findParent;
    findParent = [&](const Boom::Joint& joint, std::string& parentName) -> bool
    {
        // Check if any direct child matches our bone
        for (const auto& child : joint.children)
        {
            if (child.name == boneName)
            {
                parentName = joint.name;
                return true;
            }

            // Recurse into children
            if (findParent(child, parentName))
                return true;
        }
        return false;
    };

    std::string parent;
    if (findParent(root, parent))
        return parent;

    return "";  // No parent (root bone)
}

Boom::KeyFrame AnimationTimelinePanel::CaptureCurrentBoneTransform(const std::string& boneName)
{
    Boom::KeyFrame kf;
    kf.timeStamp = -1.0f;  // Invalid by default

    if (!m_Animator || boneName.empty() || m_SelectedClipIndex < 0)
    {
        return kf;  // Invalid
    }

    kf.timeStamp = m_CurrentTime;  // Capture at current timeline time

    // Priority 1: Check if we have a manual override from gizmo manipulation
    auto it = m_ManualBonePoses.find(boneName);
    if (it != m_ManualBonePoses.end())
    {
        // Use the manually posed transform (already in LOCAL space)
        const BonePose& pose = it->second;
        kf.position = pose.position;
        kf.rotation = pose.rotation;
        kf.scale = pose.scale;

        BOOM_INFO("[Keyframe Capture] Using manual pose for bone '{}'", boneName.c_str());
        return kf;
    }

    // Priority 2: Get current bone transform from animation at current time
    // We need to extract LOCAL space transform (relative to parent)

    // Get the bone's world transform at current time
    glm::mat4 boneWorld = GetBoneWorldTransform(boneName);

    // Get parent's world transform
    std::string parentName = GetParentBoneName(boneName);
    glm::mat4 parentWorld = glm::mat4(1.0f);

    if (!parentName.empty())
    {
        parentWorld = GetBoneWorldTransform(parentName);
    }

    // Convert to local space: local = inverse(parent) * world
    glm::mat4 localTransform = glm::inverse(parentWorld) * boneWorld;

    // Decompose local transform into position/rotation/scale
    glm::vec3 scale, translation, skew;
    glm::quat rotation;
    glm::vec4 perspective;
    glm::decompose(localTransform, scale, rotation, translation, skew, perspective);

    kf.position = translation;
    kf.rotation = rotation;
    kf.scale = scale;

    BOOM_INFO("[Keyframe Capture] Captured animated pose for bone '{}' - pos:({:.2f}, {:.2f}, {:.2f})",
              boneName.c_str(), translation.x, translation.y, translation.z);

    return kf;
}

// ========== Bone Pose Manipulation ==========

void AnimationTimelinePanel::SetBonePose(const std::string& boneName, const BonePose& pose)
{
    m_ManualBonePoses[boneName] = pose;
    m_HasManualPoses = !m_ManualBonePoses.empty();
}

void AnimationTimelinePanel::ClearBonePose(const std::string& boneName)
{
    m_ManualBonePoses.erase(boneName);
    m_HasManualPoses = !m_ManualBonePoses.empty();
}

void AnimationTimelinePanel::ClearAllBonePoses()
{
    m_ManualBonePoses.clear();
    m_HasManualPoses = false;
}

void AnimationTimelinePanel::ApplyManualBonePosesToTransforms(std::vector<glm::mat4>& transforms)
{
    // Apply manual bone poses to the transform matrices for skinning
    // This makes the model mesh move with the manually posed bones
    // CRITICAL: This must match GetBoneWorldTransform() logic exactly for consistency

    if (!m_Animator || m_ManualBonePoses.empty()) return;

    const Boom::Joint& root = m_Animator->GetRoot();
    const auto* clip = (m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
                       ? m_Animator->GetClip(m_SelectedClipIndex)
                       : nullptr;

    // Recursively rebuild all transforms using manual poses where applicable
    std::function<void(const Boom::Joint&, const glm::mat4&, bool)> rebuild;
    rebuild = [&](const Boom::Joint& joint, const glm::mat4& parentWorld, bool parentHadManualPose)
    {
        glm::mat4 localTransform = glm::mat4(1.0f);
        bool hasManualPose = false;

        // Check if this bone has a manual pose override
        auto it = m_ManualBonePoses.find(joint.name);
        if (it != m_ManualBonePoses.end())
        {
            // Use manual local transform (from gizmo manipulation)
            const BonePose& pose = it->second;
            localTransform = glm::translate(glm::mat4(1.0f), pose.position);
            localTransform *= glm::mat4_cast(pose.rotation);
            localTransform = glm::scale(localTransform, pose.scale);
            hasManualPose = true;
        }
        else
        {
            // Get transform from animation at current time
            if (clip)
            {
                const auto* keys = clip->GetTrack(joint.name);
                if (keys && keys->size() >= 2)
                {
                    // Find keyframes for interpolation
                    Boom::KeyFrame prev = (*keys)[0];
                    Boom::KeyFrame next = (*keys)[keys->size() - 1];

                    for (size_t i = 0; i < keys->size() - 1; ++i)
                    {
                        if ((*keys)[i].timeStamp <= m_CurrentTime && (*keys)[i + 1].timeStamp >= m_CurrentTime)
                        {
                            prev = (*keys)[i];
                            next = (*keys)[i + 1];
                            break;
                        }
                    }

                    float progression = 0.0f;
                    float dt = next.timeStamp - prev.timeStamp;
                    if (dt > 0.0f)
                    {
                        progression = (m_CurrentTime - prev.timeStamp) / dt;
                    }

                    // Interpolate
                    glm::vec3 pos = glm::mix(prev.position, next.position, progression);
                    glm::quat rot = glm::slerp(prev.rotation, next.rotation, progression);
                    glm::vec3 scl = glm::mix(prev.scale, next.scale, progression);

                    localTransform = glm::translate(glm::mat4(1.0f), pos);
                    localTransform *= glm::mat4_cast(rot);
                    localTransform = glm::scale(localTransform, scl);
                }
            }
        }

        // Compute world transform
        glm::mat4 worldTransform = parentWorld * localTransform;

        // Update skinning transform for GPU: skinning = world * offset
        transforms[joint.index] = worldTransform * joint.offset;

        // Recurse to children
        for (const auto& child : joint.children)
        {
            rebuild(child, worldTransform, hasManualPose || parentHadManualPose);
        }
    };

    rebuild(root, glm::mat4(1.0f), false);  // false = root has no parent with manual pose
}

void AnimationTimelinePanel::HandleGizmo(const ImVec2& viewportMin, const ImVec2& viewportSize)
{
    // Only show gizmo if we have a selected bone
    if (m_SelectedBoneName.empty() || !m_Animator || !m_HasModel) return;

    // Get current bone world transform (includes animation + manual overrides)
    glm::mat4 boneWorldMatrix = GetBoneWorldTransform(m_SelectedBoneName);

    // Apply model scale
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));
    boneWorldMatrix = scaleMatrix * boneWorldMatrix;

    // Setup ImGuizmo
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportSize.x, viewportSize.y);
    ImGuizmo::SetGizmoSizeClipSpace(0.15f);

    // Enable ImGuizmo to receive input
    ImGuizmo::Enable(true);
    ImGuizmo::AllowAxisFlip(true);

    // Setup view and projection matrices
    glm::mat4 view = glm::lookAt(m_CameraPosition, m_CameraTarget, glm::vec3(0, 1, 0));
    float aspect = m_ViewportSize.x / m_ViewportSize.y;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    // Store the original matrix before manipulation
    glm::mat4 originalMatrix = boneWorldMatrix;

    // Render the gizmo (modifies boneWorldMatrix if user drags it)
    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        (ImGuizmo::OPERATION)m_GizmoOperation,
        (ImGuizmo::MODE)m_GizmoMode,
        glm::value_ptr(boneWorldMatrix),
        nullptr,
        m_UseSnap ? m_SnapValues : nullptr
    );

    // Check if user is manipulating the gizmo
    // IMPORTANT: Only process gizmo input if our viewport is hovered/focused
    // This prevents the main viewport from also responding to the same gizmo
    bool isUsing = ImGuizmo::IsUsing();
    bool isOver = ImGuizmo::IsOver();
    bool isOurGizmo = isOver || m_GizmoWasUsing;  // IsOver or we started the drag

    // UNITY-STYLE BONE MANIPULATION
    // Key difference: Apply changes in REAL-TIME during drag, not just on release

    // Capture pose when starting manipulation (for undo)
    if (isUsing && !m_GizmoWasUsing && isOurGizmo)
    {
        // User just started manipulating - capture the BEFORE state
        m_BoneBeingManipulated = m_SelectedBoneName;
        m_HasPoseBeforeManipulation = false;

        // Try to get existing manual pose
        auto it = m_ManualBonePoses.find(m_SelectedBoneName);
        if (it != m_ManualBonePoses.end())
        {
            // Bone already has manual pose - use that as the before state
            m_BonePoseBeforeManipulation = it->second;
            m_HasPoseBeforeManipulation = true;
        }
        else
        {
            // No manual pose yet - capture current animated pose as before state
            glm::mat4 boneWorld = GetBoneWorldTransform(m_SelectedBoneName);
            std::string parentName = GetParentBoneName(m_SelectedBoneName);
            glm::mat4 parentWorld = glm::mat4(1.0f);
            if (!parentName.empty())
            {
                parentWorld = GetBoneWorldTransform(parentName);
            }

            glm::mat4 localTransform = glm::inverse(parentWorld) * boneWorld;
            glm::vec3 scale, translation, skew;
            glm::quat rotation;
            glm::vec4 perspective;
            glm::decompose(localTransform, scale, rotation, translation, skew, perspective);

            m_BonePoseBeforeManipulation.position = translation;
            m_BonePoseBeforeManipulation.rotation = rotation;
            m_BonePoseBeforeManipulation.scale = scale;
            m_HasPoseBeforeManipulation = true;
        }
    }

    // REAL-TIME UPDATE: Apply transformation during drag (UNITY BEHAVIOR)
    if (isUsing && isOurGizmo)
    {
        // Check if the matrix actually changed - check BOTH position AND rotation
        glm::vec3 oldPos = glm::vec3(originalMatrix[3]);
        glm::vec3 newPos = glm::vec3(boneWorldMatrix[3]);
        float posDiff = glm::distance(oldPos, newPos);

        // Extract rotation from matrices
        glm::quat oldRot = glm::quat_cast(originalMatrix);
        glm::quat newRot = glm::quat_cast(boneWorldMatrix);
        float rotDot = glm::abs(glm::dot(oldRot, newRot));
        bool rotChanged = (rotDot < 0.9999f);  // Quaternions differ if dot product isn't ~1.0

        float detDiff = glm::abs(glm::determinant(originalMatrix) - glm::determinant(boneWorldMatrix));

        bool matrixChanged = (posDiff > 0.00001f || rotChanged || detDiff > 0.00001f);

        if (matrixChanged)
        {
            // CRITICAL FIX: Remove model scale to get actual bone world transform
            glm::mat4 boneWorld = glm::inverse(scaleMatrix) * boneWorldMatrix;

            // Get parent bone's world transform
            std::string parentName = GetParentBoneName(m_SelectedBoneName);
            glm::mat4 parentWorld = glm::mat4(1.0f);

            if (!parentName.empty())
            {
                // Temporarily remove our bone's manual pose to get parent's clean transform
                auto it = m_ManualBonePoses.find(m_SelectedBoneName);
                BonePose tempPose = {};  // Initialize to avoid warning
                bool hadPose = false;
                if (it != m_ManualBonePoses.end())
                {
                    tempPose = it->second;
                    hadPose = true;
                    m_ManualBonePoses.erase(it);
                }

                // Get parent's world transform
                parentWorld = GetBoneWorldTransform(parentName);

                // Restore our pose
                if (hadPose)
                {
                    m_ManualBonePoses[m_SelectedBoneName] = tempPose;
                }
            }

            // Convert to local space: local = inverse(parent) * world
            glm::mat4 localTransform = glm::inverse(parentWorld) * boneWorld;

            // Extract transform components
            glm::vec3 scale, translation, skew;
            glm::quat rotation;
            glm::vec4 perspective;
            bool decomposed = glm::decompose(localTransform, scale, rotation, translation, skew, perspective);

            if (decomposed)
            {
                // Normalize rotation to prevent drift and ensure valid quaternion
                rotation = glm::normalize(rotation);

                // Ensure scale is reasonable (prevent collapse or explosion)
                for (int i = 0; i < 3; i++)
                {
                    if (scale[i] < 0.001f) scale[i] = 0.001f;
                    if (scale[i] > 1000.0f) scale[i] = 1000.0f;
                }

                // Create new pose and apply immediately for real-time feedback
                BonePose newPose;
                newPose.position = translation;
                newPose.rotation = rotation;
                newPose.scale = scale;

                m_ManualBonePoses[m_SelectedBoneName] = newPose;
                m_HasManualPoses = true;
            }
        }
    }

    // On release: Create undo command
    if (!isUsing && m_GizmoWasUsing && isOurGizmo)
    {
        BOOM_INFO("[AnimTimeline Gizmo] User released gizmo on bone: {}", m_SelectedBoneName);

        // Get the final pose that was applied
        auto it = m_ManualBonePoses.find(m_SelectedBoneName);
        if (it != m_ManualBonePoses.end() && m_HasPoseBeforeManipulation)
        {
            BonePose finalPose = it->second;

            // Check if pose actually changed
            const float posThreshold = 0.0001f;
            const float rotThreshold = 0.0001f;
            const float scaleThreshold = 0.0001f;

            bool poseChanged =
                glm::distance(m_BonePoseBeforeManipulation.position, finalPose.position) > posThreshold ||
                glm::abs(glm::dot(m_BonePoseBeforeManipulation.rotation, finalPose.rotation)) < (1.0f - rotThreshold) ||
                glm::distance(m_BonePoseBeforeManipulation.scale, finalPose.scale) > scaleThreshold;

            if (poseChanged)
            {
                // Record the bone pose change for undo/redo using the new method
                BonePose oldPose = m_BonePoseBeforeManipulation;
                BonePose newPose = finalPose;
                RecordBonePoseChange(m_SelectedBoneName, oldPose, newPose);
            }
        }
    }

    m_GizmoWasUsing = isUsing;
}

// ========== 3D Rendering Functions ==========

void AnimationTimelinePanel::RenderModel()
{
    if (!m_Ctx || !m_Ctx->renderer || !m_Model)
    {
        return;
    }

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
        // Sync animator to timeline time and update joint transforms
        m_Animator->SetTime(m_CurrentTime);
        m_Animator->UpdateJointsFromCurrentTime();

        // Get a COPY of the transforms (not a reference) to avoid modifying animator's internal state
        // This prevents timeline bone manipulation from affecting the game scene entity
        std::vector<glm::mat4> transforms = m_Animator->GetJoints();

        // Apply manual bone poses if we're manipulating bones with gizmo
        if (m_HasManualPoses)
        {
            ApplyManualBonePosesToTransforms(transforms);
        }

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

    // If we have manual poses, rebuild bone lines to show real-time updates during gizmo drag
    auto boneLines = m_Animator->GetSkeletonLines();  // Get default lines first

    if (m_HasManualPoses)
    {
        // REAL-TIME UPDATE: Rebuild bone lines using manual poses
        boneLines.clear();  // Clear and rebuild with manual poses

        const Boom::Joint& root = m_Animator->GetRoot();

        // Lambda to recursively build bone lines with manual pose overrides
        std::function<void(const Boom::Joint&, const glm::vec3&)> buildBoneLinesFunc;
        buildBoneLinesFunc = [&](const Boom::Joint& joint, const glm::vec3& parentPos)
        {
            // Get bone's world position (respects manual overrides)
            glm::mat4 worldTransform = GetBoneWorldTransform(joint.name);
            glm::vec3 bonePos = glm::vec3(worldTransform[3]);

            // Create line from parent to this bone
            Boom::Animator::BoneLine boneLine;  // Correct type
            boneLine.start = parentPos;
            boneLine.end = bonePos;
            boneLine.boneName = joint.name;
            boneLines.push_back(boneLine);

            // Recurse to children
            for (const auto& child : joint.children)
            {
                buildBoneLinesFunc(child, bonePos);
            }
        };

        // Start from root
        glm::mat4 rootTransform = GetBoneWorldTransform(root.name);
        glm::vec3 rootPos = glm::vec3(rootTransform[3]);

        for (const auto& child : root.children)
        {
            buildBoneLinesFunc(child, rootPos);
        }
    }
    // else: keep the default boneLines from GetSkeletonLines()

    if (boneLines.empty()) return;

    std::vector<Boom::LineVert> normalBones;
    std::vector<Boom::LineVert> hoveredBones;
    std::vector<Boom::LineVert> selectedBones;
    normalBones.reserve(boneLines.size() * 2);

    glm::vec4 boneColor = m_Ctx->BoneColor;
    glm::vec4 selectedBoneColor = m_Ctx->SelectedBoneColor;
    glm::vec4 hoveredBoneColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for hover

    glm::mat4 boneTransform = glm::scale(glm::mat4(1.0f), glm::vec3(m_ModelScale));

    for (const auto& boneLine : boneLines)
    {
        bool isSelected = (!m_SelectedBoneName.empty() && boneLine.boneName == m_SelectedBoneName);
        bool isHovered = (!m_HoveredBoneNameViewport.empty() && boneLine.boneName == m_HoveredBoneNameViewport);

        glm::vec4 start4 = boneTransform * glm::vec4(boneLine.start, 1.0f);
        glm::vec4 end4 = boneTransform * glm::vec4(boneLine.end, 1.0f);
        glm::vec3 scaledStart = glm::vec3(start4);
        glm::vec3 scaledEnd = glm::vec3(end4);

        if (isSelected)
        {
            selectedBones.push_back({ scaledStart, selectedBoneColor });
            selectedBones.push_back({ scaledEnd, selectedBoneColor });
        }
        else if (isHovered)
        {
            hoveredBones.push_back({ scaledStart, hoveredBoneColor });
            hoveredBones.push_back({ scaledEnd, hoveredBoneColor });
        }
        else
        {
            normalBones.push_back({ scaledStart, boneColor });
            normalBones.push_back({ scaledEnd, boneColor });
        }
    }

    // Render in order: normal first, then hovered (thicker), then selected (thickest)
    if (!normalBones.empty())
    {
        debugShader->Draw(view, proj, normalBones, m_Ctx->BoneLineWidth, true);
    }

    if (!hoveredBones.empty())
    {
        debugShader->Draw(view, proj, hoveredBones, m_Ctx->BoneLineWidth * 2.0f, true);
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

    // Get animator from skeletal model if it has skeleton (CLONE for independence)
    if (foundAsset->hasJoints && m_Model->HasJoint())
    {
        auto skeletalModel = std::dynamic_pointer_cast<Boom::SkeletalModel>(m_Model);
        if (skeletalModel)
        {
            auto sourceAnimator = skeletalModel->GetAnimator();
            if (sourceAnimator)
            {
                // CRITICAL: Clone the animator for independent timeline
                m_SourceAnimator = sourceAnimator;
                m_Animator = sourceAnimator->Clone();

                // CRITICAL: Clear states to force clip mode (timeline doesn't use state machine)
                m_Animator->GetStates().clear();

                BOOM_INFO("[AnimationTimeline] Cloned animator from skeletal model for independent preview (states cleared)");

                // Auto-select first animation clip if available
                if (m_Animator->GetClipCount() > 0)
                {
                    m_SelectedClipIndex = 0;
                    m_Animator->PlayClip(0);
                    m_CurrentTime = 0.0f;
                    m_IsPlaying = false;  // Reset playback state
                    BOOM_INFO("[AnimationTimeline] Auto-selected first clip: {}",
                        m_Animator->GetClip(0)->name);
                }
                else
                {
                    m_SelectedClipIndex = -1;
                    m_IsPlaying = false;
                    BOOM_WARN("[AnimationTimeline] Animator has no clips");
                }
            }
            else
            {
                BOOM_WARN("[AnimationTimeline] Model has joints but no animator");
                m_Animator.reset();
                m_SourceAnimator.reset();
                m_SelectedClipIndex = -1;
            }
        }
        else
        {
            BOOM_WARN("[AnimationTimeline] Model has joints but is not SkeletalModel");
            m_Animator.reset();
            m_SourceAnimator.reset();
            m_SelectedClipIndex = -1;
        }
    }
    else
    {
        m_Animator.reset();
        m_SourceAnimator.reset();
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
    m_SourceAnimator.reset();
    m_SourceEntityID = entt::null;
    m_HasModel = false;
    m_StandaloneMode = false;
    m_LoadedModelPath.clear();
    m_SelectedBoneName.clear();
    m_SelectedClipIndex = -1;
    m_CurrentTime = 0.0f;
    m_IsPlaying = false;

    BOOM_INFO("[AnimationTimeline] Model cleared");
}
