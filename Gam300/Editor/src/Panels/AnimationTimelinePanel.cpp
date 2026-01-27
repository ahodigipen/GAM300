#include "Panels/AnimationTimelinePanel.h"
#include "Editor.h"
#include "Application/Interface.h"
#include "Application/Context.h"
#include "Commands/UndoRedo.h"
#include "ECS/ECS.hpp"
#include "Graphics/Models/Model.h"
#include "Graphics/Models/Animator.h"
#include "Graphics/Models/Animation.h"
#include "Graphics/Models/AnimationIO.h"
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
#include <chrono>

using namespace EditorUI;

// ==================== BONE POSE UNDO/REDO COMMAND ====================

namespace EditorUI {
    // Command for bone pose manipulation with undo/redo support
    class BonePoseCommand : public ICommand {
    public:
        BonePoseCommand(AnimationTimelinePanel* panel,
                       const std::string& boneName,
                       const BonePose& oldPose,
                       const BonePose& newPose)
            : m_Panel(panel)
            , m_BoneName(boneName)
            , m_OldPose(oldPose)
            , m_NewPose(newPose)
        {
        }

        void Execute() override {
            if (!m_Panel) return;
            m_Panel->SetBonePose(m_BoneName, m_NewPose);
        }

        void Undo() override {
            if (!m_Panel) return;
            m_Panel->SetBonePose(m_BoneName, m_OldPose);
        }

        std::string GetDescription() const override {
            return "Bone Pose: " + m_BoneName;
        }

        // Public method to set the panel (for when panel needs to be updated)
        void SetPanel(AnimationTimelinePanel* panel) { m_Panel = panel; }

    private:
        AnimationTimelinePanel* m_Panel;
        std::string m_BoneName;
        BonePose m_OldPose;
        BonePose m_NewPose;
    };
}

AnimationTimelinePanel::AnimationTimelinePanel(Editor* owner)
    : m_Owner(owner)
    , m_App(owner ? static_cast<Boom::AppInterface*>(owner) : nullptr)
    , m_Ctx(m_App ? m_App->GetContext() : nullptr)
{
    // Initialize playback time
    m_LastFrameTime = (float)ImGui::GetTime();

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

    // ===== PLAYBACK UPDATE: Independent timeline =====
    float currentFrameTime = (float)ImGui::GetTime();
    float deltaTime = currentFrameTime - m_LastFrameTime;

    // Clamp deltaTime to prevent huge first-frame jumps (e.g., window opened late)
    // Max 100ms (0.1s) per frame to avoid time spikes
    if (deltaTime > 0.1f || deltaTime < 0.0f)
    {
        deltaTime = 0.0f;  // Skip this frame's time update
    }

    m_LastFrameTime = currentFrameTime;

    // Update playback time (independent from game scene)
    if (m_Animator && m_SelectedClipIndex >= 0)
    {
        const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
        if (clip && clip->duration > 0.0f)
        {
            if (m_IsPlaying)
            {
                // CRITICAL: Manually advance time to control looping
                // The animator always loops in clip mode (Animator.h:128), so we control it here
                float newTime = m_CurrentTime + (deltaTime * m_PlaybackSpeed * clip->ticksPerSecond);

                if (m_Loop)
                {
                    // Loop enabled: wrap time
                    if (newTime >= clip->duration)
                    {
                        newTime = fmod(newTime, clip->duration);
                    }
                    m_CurrentTime = newTime;
                }
                else
                {
                    // Loop disabled: clamp and stop
                    if (newTime >= clip->duration)
                    {
                        m_CurrentTime = clip->duration;
                        m_IsPlaying = false;
                    }
                    else
                    {
                        m_CurrentTime = newTime;
                    }
                }

                // Update animator to match our timeline time
                m_Animator->SetTime(m_CurrentTime);
                m_Animator->UpdateJointsFromCurrentTime();
            }
            else
            {
                // When paused, ensure animator is synced to our scrubber time
                m_Animator->SetTime(m_CurrentTime);
                m_Animator->UpdateJointsFromCurrentTime();
            }
        }
    }

    // Get selected entity and load model/animator (only if not in standalone mode)
    auto selectedID = m_App->SelectedEntity();

    // Early validation: Check if selectedID is actually valid before doing anything
    // Some systems return entity 0 instead of entt::null when nothing is selected
    bool hasValidSelection = (selectedID != entt::null);
    if (hasValidSelection && m_Ctx)
    {
        hasValidSelection = m_Ctx->scene.valid(selectedID);
    }

    // Handle entity deselection (user clicked away in hierarchy, or entity became invalid)
    if (!m_StandaloneMode && !hasValidSelection && m_SourceEntityID != entt::null)
    {
        // Entity was deselected - clear timeline to avoid using stale data
        BOOM_INFO("[AnimationTimeline] Entity deselected, clearing timeline");
        m_Animator.reset();
        m_SourceAnimator.reset();
        m_Model.reset();
        m_SourceEntityID = entt::null;
        m_HasModel = false;
        m_SelectedBoneName.clear();
        m_ManualBonePoses.clear();
        m_HasManualPoses = false;
        m_SelectedClipIndex = -1;
        m_CurrentTime = 0.0f;
        m_IsPlaying = false;
        m_UndoStack.clear();
        m_RedoStack.clear();
        ImGui::End();  // Must end the window before returning
        return;
    }

    // Only try to load from entity if we're not in standalone mode and have valid selection
    if (!m_StandaloneMode && hasValidSelection)
    {
        auto& scene = m_App->GetContext()->scene;
        Boom::Entity selected(&scene, selectedID);

        // Get animator from AnimatorComponent (CLONE IT for independent timeline)
        if (selected.Has<Boom::AnimatorComponent>())
        {
            auto& animComp = selected.Get<Boom::AnimatorComponent>();
            if (animComp.animator)
            {
                // CRITICAL: Only clone when ENTITY changes, not when animator pointer might change
                // This prevents constant re-cloning during gameplay
                bool entityChanged = (m_SourceEntityID != selectedID);
                bool animatorPtrChanged = (m_SourceAnimator != animComp.animator);

                if (entityChanged)
                {
                    BOOM_INFO("[AnimationTimeline] Entity changed (old={}, new={}), cloning animator",
                        (uint32_t)m_SourceEntityID, (uint32_t)selectedID);

                    m_SourceEntityID = selectedID;
                    m_SourceAnimator = animComp.animator;

                    // Capture the entity's current clip BEFORE cloning
                    size_t entityCurrentClip = animComp.animator->GetCurrentClip();

                    m_Animator = animComp.animator->Clone();

                    // CRITICAL: Clear states to force clip mode (timeline doesn't use state machine)
                    m_Animator->GetStates().clear();

                    BOOM_INFO("[AnimationTimeline] Clone created: Original={}, Clone={} (states cleared for clip mode)",
                        (void*)animComp.animator.get(), (void*)m_Animator.get());

                    // Default to the entity's current clip (better UX)
                    if (entityCurrentClip < m_Animator->GetClipCount())
                    {
                        m_SelectedClipIndex = (int)entityCurrentClip;
                        m_Animator->PlayClip(entityCurrentClip);
                        BOOM_INFO("[AnimationTimeline] Defaulting to entity's current clip: {} (index {})",
                            m_Animator->GetClip(entityCurrentClip)->name, entityCurrentClip);
                    }
                    else
                    {
                        m_SelectedClipIndex = -1;
                    }

                    m_CurrentTime = 0.0f;
                    m_IsPlaying = false;

                    // Clear manual poses when switching entities
                    m_ManualBonePoses.clear();
                    m_HasManualPoses = false;
                    m_SelectedBoneName.clear();

                    // Clear undo/redo stacks - old commands reference old animator
                    m_UndoStack.clear();
                    m_RedoStack.clear();
                }
                else if (animatorPtrChanged)
                {
                    // FIX 2: Animator pointer changed (scene reload) - re-clone immediately
                    BOOM_INFO("[AnimationTimeline] Animator pointer changed (scene reload), re-cloning from entity");

                    // Update source reference
                    m_SourceAnimator = animComp.animator;

                    // Re-clone the animator
                    m_Animator = animComp.animator->Clone();
                    m_Animator->GetStates().clear();  // Force clip mode

                    // Restore playback state
                    if (m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
                    {
                        m_Animator->PlayClip(m_SelectedClipIndex);
                        m_Animator->SetTime(m_CurrentTime);
                        BOOM_INFO("[AnimationTimeline] Restored clip {} at time {:.2f}", m_SelectedClipIndex, m_CurrentTime);
                    }

                    BOOM_INFO("[AnimationTimeline] Re-clone complete: Original={}, New Clone={}",
                        (void*)animComp.animator.get(), (void*)m_Animator.get());

                    // Clear undo/redo stacks - old commands reference old animator
                    m_UndoStack.clear();
                    m_RedoStack.clear();
                }
                // If entity AND animator didn't change, keep using our existing clone (don't re-clone!)
            }
            else
            {
                m_Animator.reset();
                m_SourceAnimator.reset();
                m_SourceEntityID = entt::null;
            }
        }
        else
        {
            m_Animator.reset();
            m_SourceAnimator.reset();
            m_SourceEntityID = entt::null;
        }

        // Get model from ModelComponent
        if (selected.Has<Boom::ModelComponent>())
        {
            auto& modelComp = selected.Get<Boom::ModelComponent>();

            if (modelComp.modelID != Boom::EMPTY_ASSET && m_Ctx && m_Ctx->assets)
            {
                Boom::ModelAsset* modelAsset = m_Ctx->assets->TryGet<Boom::ModelAsset>(modelComp.modelID);
                if (modelAsset && modelAsset->data)
                {
                    m_Model = modelAsset->data;
                    m_HasModel = true;
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

    // NOTE: Animator change detection is now handled in the clone logic above
    // Auto-select first clip if we have an animator but no clip selected
    if (m_Animator && m_Animator->GetClipCount() > 0 && m_SelectedClipIndex < 0)
    {
        m_SelectedClipIndex = 0;
        m_Animator->PlayClip(0);
        m_CurrentTime = 0.0f;
        m_IsPlaying = false;  // Reset playback state
        BOOM_INFO("[AnimationTimeline] Auto-selected first clip: {}",
            m_Animator->GetClip(0)->name);
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
    // Top bar with collapsible sections for better organization
    ImGui::BeginGroup();

    // ========== SECTION 1: MODEL & PLAYBACK ==========
    // Blue tint for model/playback section
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.3f, 0.5f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.35f, 0.55f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.4f, 0.6f, 1.0f));

    if (ImGui::CollapsingHeader("Model & Playback", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent(10.0f);

        // Model loading
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

        ImGui::SameLine(0, 20);
        ImGui::Text("|");
        ImGui::SameLine(0, 20);

        // Playback controls (Unity-style layout)
        ImGui::BeginDisabled(!m_HasModel || !m_Animator || m_SelectedClipIndex < 0);

        // First Frame button
        if (ImGui::Button("|<")) {
            m_CurrentTime = 0.0f;
            m_IsPlaying = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("First Frame");

        // Previous Keyframe button
        ImGui::SameLine();
        if (ImGui::Button("<K")) {
            // Jump to previous keyframe
            if (m_Animator && m_SelectedClipIndex >= 0) {
                float prevTime = 0.0f;
                const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
                if (clip) {
                    // Find previous keyframe across all bones
                    for (const auto& [boneName, track] : clip->tracks) {
                        for (const auto& kf : track) {
                            if (kf.timeStamp < m_CurrentTime - 0.001f && kf.timeStamp > prevTime) {
                                prevTime = kf.timeStamp;
                            }
                        }
                    }
                    if (prevTime > 0.0f) m_CurrentTime = prevTime;
                }
            }
            m_IsPlaying = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Previous Keyframe");

        ImGui::SameLine();
        if (ImGui::Button(m_IsPlaying ? "Pause" : "Play")) {
            m_IsPlaying = !m_IsPlaying;
        }

        // Next Keyframe button
        ImGui::SameLine();
        if (ImGui::Button("K>")) {
            // Jump to next keyframe
            if (m_Animator && m_SelectedClipIndex >= 0) {
                const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
                if (clip) {
                    float nextTime = clip->duration;
                    // Find next keyframe across all bones
                    for (const auto& [boneName, track] : clip->tracks) {
                        for (const auto& kf : track) {
                            if (kf.timeStamp > m_CurrentTime + 0.001f && kf.timeStamp < nextTime) {
                                nextTime = kf.timeStamp;
                            }
                        }
                    }
                    if (nextTime < clip->duration) m_CurrentTime = nextTime;
                }
            }
            m_IsPlaying = false;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Next Keyframe");

        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            m_IsPlaying = false;
            m_CurrentTime = 0.0f;
        }

        ImGui::SameLine();
        ImGui::Checkbox("Loop", &m_Loop);

        // Playback speed control
        ImGui::SameLine();
        ImGui::Text("Speed:");
        ImGui::SameLine();
        // Proportional width (6% of window, min 60, max 100)
        float speedSliderWidth = ImGui::GetWindowWidth() * 0.06f;
        speedSliderWidth = (speedSliderWidth < 60.0f) ? 60.0f : (speedSliderWidth > 100.0f) ? 100.0f : speedSliderWidth;
        ImGui::SetNextItemWidth(speedSliderWidth);
        ImGui::SliderFloat("##PlaybackSpeed", &m_PlaybackSpeed, 0.1f, 3.0f, "%.1fx");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Playback speed multiplier");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("1x")) {
            m_PlaybackSpeed = 1.0f;
        }
        ImGui::EndDisabled();

        ImGui::Unindent(10.0f);
        ImGui::Spacing();
    }
    ImGui::PopStyleColor(3); // Pop Model & Playback colors

    // ========== SECTION 2: EDITING TOOLS ==========
    // Green tint for editing tools section
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.3f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.55f, 0.35f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.6f, 0.4f, 1.0f));

    if (ImGui::CollapsingHeader("Editing Tools", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent(10.0f);

        // Undo/Redo buttons
        ImGui::BeginDisabled(m_UndoStack.empty());
        if (ImGui::Button("Undo"))
        {
            Undo();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Undo last keyframe edit (Ctrl+Z)");
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_RedoStack.empty());
        if (ImGui::Button("Redo"))
        {
            Redo();
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Redo keyframe edit (Ctrl+Y)");
        }
        ImGui::EndDisabled();

    // Keyboard shortcuts - only process when Animation Timeline window is focused
    // The Editor's global undo/redo is skipped when this window is focused (see Editor.cpp)
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift)
        {
            Undo();
        }
        if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyShift)))
        {
            Redo();
        }
    }

    // Gizmo mode keyboard shortcuts (W/E/R/T/K) - only when viewport is focused
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
            // Only allow translate if not in rotation-only mode
            if (!m_RotationOnlyMode) {
                m_GizmoOperation = 7;  // ImGuizmo::TRANSLATE
            }
            else {
                BOOM_WARN("[Gizmo] Translation disabled in Rotation-Only mode. Uncheck 'Rotation Only' to enable.");
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
            m_GizmoOperation = 120;  // ImGuizmo::ROTATE
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            // Only allow scale if not in rotation-only mode
            if (!m_RotationOnlyMode) {
                m_GizmoOperation = 896;  // ImGuizmo::SCALE
            }
            else {
                BOOM_WARN("[Gizmo] Scaling disabled in Rotation-Only mode. Uncheck 'Rotation Only' to enable.");
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_T, false)) {
            m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;  // Toggle LOCAL (0) / WORLD (1)
            BOOM_INFO("[Gizmo] Toggled to {} space", m_GizmoMode == 1 ? "WORLD" : "LOCAL");
        }

        // K key - Add keyframe at current time for selected bone
        if (ImGui::IsKeyPressed(ImGuiKey_K, false))
        {
            if (!m_SelectedBoneName.empty() && m_Animator && m_SelectedClipIndex >= 0)
            {
                // Capture current bone transform
                Boom::KeyFrame kf = CaptureCurrentBoneTransform(m_SelectedBoneName);

                if (kf.timeStamp >= 0.0f)  // Valid capture
                {
                    // Create and execute ADD command
                    KeyframeCommand cmd;
                    cmd.type = KeyframeCommand::ADD;
                    cmd.boneName = m_SelectedBoneName;
                    cmd.keyframe = kf;
                    ExecuteCommand(cmd);

                    BOOM_INFO("[Keyframe Record] Captured pose for bone '{}' at time {:.2f}s",
                              m_SelectedBoneName.c_str(), (double)kf.timeStamp);
                }
                else
                {
                    BOOM_WARN("[Keyframe Record] Failed to capture bone transform for '{}'", m_SelectedBoneName.c_str());
                }
            }
            else if (m_SelectedBoneName.empty())
            {
                BOOM_WARN("[Keyframe Record] No bone selected - select a bone first!");
            }
            else if (!m_Animator)
            {
                BOOM_WARN("[Keyframe Record] No animator loaded");
            }
            else if (m_SelectedClipIndex < 0)
            {
                BOOM_WARN("[Keyframe Record] No clip selected");
            }
        }

        // Delete key - Delete selected keyframes OR audio events
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            if (!m_SelectedKeyframes.empty())
            {
                DeleteSelectedKeyframes();
            }
            else if (m_SelectedAudioEventIndex >= 0)
            {
                // Delete selected audio event
                if (m_Animator && m_SelectedClipIndex >= 0)
                {
                    auto* clip = m_Animator->GetClipMutable(m_SelectedClipIndex);
                    if (clip && m_SelectedAudioEventIndex < (int)clip->audioEvents.size())
                    {
                        BOOM_INFO("[AudioEvent] Deleted audio event '{}' at {:.2f}s",
                                  clip->audioEvents[m_SelectedAudioEventIndex].eventName,
                                  clip->audioEvents[m_SelectedAudioEventIndex].timeStamp);
                        clip->audioEvents.erase(clip->audioEvents.begin() + m_SelectedAudioEventIndex);
                        m_SelectedAudioEventIndex = -1;
                    }
                }
            }
        }

        // Escape key - Clear keyframe selection or audio event selection
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        {
            if (!m_SelectedKeyframes.empty())
            {
                ClearKeyframeSelection();
            }
            else if (m_SelectedAudioEventIndex >= 0)
            {
                m_SelectedAudioEventIndex = -1;
            }
        }

        // Ctrl+A - Select all keyframes in current clip
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false))
        {
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
                if (clip)
                {
                    ClearKeyframeSelection();
                    // Iterate all tracks and select all keyframes
                    for (const auto& [boneName, track] : clip->tracks)
                    {
                        for (size_t i = 0; i < track.size(); ++i)
                        {
                            SelectKeyframe(boneName, i, true);  // Add to selection
                        }
                    }
                    BOOM_INFO("[Multiselect] Selected {} keyframes", m_SelectedKeyframes.size());
                }
            }
        }
    }

        ImGui::SameLine(0, 20);
        ImGui::Text("|");
        ImGui::SameLine(0, 20);

        // Gizmo mode buttons
        const char* gizmoModeText = "";
        if (m_GizmoOperation == 7) gizmoModeText = "Move (W)";
        else if (m_GizmoOperation == 120) gizmoModeText = "Rotate (E)";
        else if (m_GizmoOperation == 896) gizmoModeText = "Scale (R)";

        const char* gizmoSpaceText = (m_GizmoMode == 1) ? "World" : "Local";

        if (!m_CompactMode)
        {
            // Full gizmo controls
            ImGui::Text("Gizmo:");
            ImGui::SameLine();
            if (ImGui::Button("Move (W)")) m_GizmoOperation = 7;
            ImGui::SameLine();
            if (ImGui::Button("Rotate (E)")) m_GizmoOperation = 120;
            ImGui::SameLine();
            if (ImGui::Button("Scale (R)")) m_GizmoOperation = 896;
            ImGui::SameLine();
            ImGui::Text("|");
            ImGui::SameLine();
            if (ImGui::Button("Toggle Space (T)"))
            {
                m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;
            }
            ImGui::SameLine();
            ImGui::Text("[%s - %s]", gizmoModeText, gizmoSpaceText);
        }
        else
        {
            // Compact mode - smaller buttons without labels
            if (ImGui::SmallButton("W")) m_GizmoOperation = 7;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move");
            ImGui::SameLine();
            if (ImGui::SmallButton("E")) m_GizmoOperation = 120;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate");
            ImGui::SameLine();
            if (ImGui::SmallButton("R")) m_GizmoOperation = 896;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale");
            ImGui::SameLine();
            if (ImGui::SmallButton("T"))
            {
                m_GizmoMode = (m_GizmoMode == 0) ? 1 : 0;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle %s", gizmoSpaceText);
        }

        ImGui::SameLine();
        // Gizmo manipulation mode toggle
        if (ImGui::Checkbox("Rotation Only", &m_RotationOnlyMode))
        {
            // If switching to rotation-only mode, force rotate gizmo
            if (m_RotationOnlyMode)
            {
                m_GizmoOperation = 120;  // ImGuizmo::ROTATE
                BOOM_INFO("[Gizmo] Rotation-Only Mode ENABLED - translation disabled to prevent bone stretching");
            }
            else
            {
                BOOM_INFO("[Gizmo] Rotation-Only Mode DISABLED - translation allowed (may cause bone stretching)");
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("When enabled, only rotation gizmo is allowed.\nPrevents bone length stretching from translation.");
        }

        // Keyframe recording controls (new row)
        ImGui::Text("Keyframe:");
        ImGui::SameLine();
        if (!m_SelectedBoneName.empty())
        {
            if (ImGui::Button("Add (K)"))
            {
                // Trigger K key action manually via button
                if (m_Animator && m_SelectedClipIndex >= 0)
                {
                    Boom::KeyFrame kf = CaptureCurrentBoneTransform(m_SelectedBoneName);
                    if (kf.timeStamp >= 0.0f)
                    {
                        KeyframeCommand cmd;
                        cmd.type = KeyframeCommand::ADD;
                        cmd.boneName = m_SelectedBoneName;
                        cmd.keyframe = kf;
                        ExecuteCommand(cmd);
                    }
                }
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Add keyframe for '%s' at current time (%.2fs)\nOr press K key",
                                m_SelectedBoneName.c_str(), m_CurrentTime);
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("Add (K)");
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Select a bone first, then press K to add a keyframe");
            }
        }

        // Show selected keyframes count (multiselect status)
        ImGui::SameLine();
        if (!m_SelectedKeyframes.empty())
        {
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "| %zu keyframe%s selected",
                              m_SelectedKeyframes.size(),
                              m_SelectedKeyframes.size() == 1 ? "" : "s");
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Multiselect shortcuts:\n"
                                 "  Ctrl+Click: Add/remove from selection\n"
                                 "  Ctrl+A: Select all keyframes\n"
                                 "  Delete: Delete selected keyframes\n"
                                 "  Escape: Clear selection\n"
                                 "  Drag: Move all selected together");
            }
        }

        ImGui::Unindent(10.0f);
        ImGui::Spacing();
    }
    ImGui::PopStyleColor(3); // Pop Editing Tools colors

    // ========== SECTION 3: VIEW OPTIONS ==========
    // Purple tint for view options section
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.2f, 0.5f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.45f, 0.25f, 0.55f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.5f, 0.3f, 0.6f, 1.0f));

    if (ImGui::CollapsingHeader("View Options", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent(10.0f);

        // Compact mode toggle (first item for easy access)
        if (ImGui::Checkbox("Compact Mode", &m_CompactMode))
        {
            // Optional: log state change
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Hide less-frequently-used controls for smaller screens");
        }

        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();

        // Visualization toggles
        ImGui::Checkbox("Show Skeleton", &m_ShowSkeleton);
        ImGui::SameLine();
        ImGui::Checkbox("Show Grid", &m_ShowGrid);
        ImGui::SameLine();
        ImGui::Checkbox("Wireframe", &m_ShowWireframe);

        // Camera controls
        if (ImGui::Button("Reset Camera")) {
            ResetCamera();
        }

        if (m_HasModel) {
            ImGui::SameLine();
            if (ImGui::Button("Frame Model")) {
                FrameModel();
            }

            // Clear bone poses button (only show if we have manual poses)
            if (m_HasManualPoses && !m_SelectedBoneName.empty())
            {
                ImGui::SameLine();
                if (ImGui::Button("Clear Bone Pose"))
                {
                    ClearBonePose(m_SelectedBoneName);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Clear manual pose for selected bone '%s'\n(Ctrl+Z to undo)",
                                    m_SelectedBoneName.c_str());
                }
            }

            if (m_HasManualPoses)
            {
                ImGui::SameLine();
                if (ImGui::Button("Clear All Poses"))
                {
                    ClearAllBonePoses();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("Clear all manual bone poses\n(Returns to animation keyframes)");
                }
            }
        }

        // Scale controls (new row)
        if (m_HasModel)
        {
            if (!m_CompactMode)
            {
                // Full scale controls (only in non-compact mode)
                ImGui::Text("Preview Scale:");
                ImGui::SameLine();
                // Use proportional width: 15% of window width (min 100, max 200)
                float scaleSliderWidth = ImGui::GetWindowWidth() * 0.15f;
                scaleSliderWidth = (scaleSliderWidth < 100.0f) ? 100.0f : (scaleSliderWidth > 200.0f) ? 200.0f : scaleSliderWidth;
                ImGui::SetNextItemWidth(scaleSliderWidth);

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
            else
            {
                // Compact mode - just show reset button
                ImGui::Text("Scale:");
                ImGui::SameLine();
                ImGui::Text("%.2fx", m_ModelScale);
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset"))
                {
                    m_ModelScale = 1.0f;
                }
            }
        }

        ImGui::Unindent(10.0f);
        ImGui::Spacing();
    }
    ImGui::PopStyleColor(3); // Pop View Options colors

    // ========== SECTION 4: ANIMATION CLIP ==========
    if (m_Animator && m_Animator->GetClipCount() > 0)
    {
        // Orange tint for animation clip section
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.6f, 0.4f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.65f, 0.45f, 0.25f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.7f, 0.5f, 0.3f, 1.0f));

        if (ImGui::CollapsingHeader("Animation Clip", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(10.0f);

            ImGui::Text("Clip:");
            ImGui::SameLine();

        // Animation clip dropdown - proportional width (15% of window, min 120, max 250)
        float clipDropdownWidth = ImGui::GetWindowWidth() * 0.15f;
        clipDropdownWidth = (clipDropdownWidth < 120.0f) ? 120.0f : (clipDropdownWidth > 250.0f) ? 250.0f : clipDropdownWidth;
        ImGui::SetNextItemWidth(clipDropdownWidth);
        std::string clipDisplayName = "Select clip...";
        if (m_SelectedClipIndex >= 0)
        {
            clipDisplayName = m_Animator->GetClip(m_SelectedClipIndex)->name;
            if (m_ClipModified)
            {
                clipDisplayName += " *";  // Asterisk indicates unsaved changes
            }
        }
        if (ImGui::BeginCombo("##AnimClip", clipDisplayName.c_str()))
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
                    m_IsPlaying = false;      // Stop playback when switching clips
                    m_ClipModified = false;   // Reset dirty flag for new clip
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Clip management buttons
        ImGui::SameLine();
        if (ImGui::Button("New"))
        {
            ImGui::OpenPopup("CreateClipPopup");
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Create a new animation clip");
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedClipIndex < 0);
        if (ImGui::Button("Duplicate"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
            {
                auto* sourceClip = m_Animator->GetClipMutable(m_SelectedClipIndex);
                if (sourceClip)
                {
                    // Create a deep copy of the clip
                    auto newClip = std::make_shared<Boom::AnimationClip>();
                    newClip->name = sourceClip->name + " Copy";
                    newClip->duration = sourceClip->duration;
                    newClip->ticksPerSecond = sourceClip->ticksPerSecond;
                    newClip->filePath = sourceClip->filePath;
                    newClip->tracks = sourceClip->tracks;  // Deep copy tracks

                    // Add to animator
                    m_Animator->AddClip(newClip);
                    m_SelectedClipIndex = (int)m_Animator->GetClipCount() - 1;
                    m_Animator->PlayClip(m_SelectedClipIndex);
                    m_CurrentTime = 0.0f;
                    m_IsPlaying = false;

                    BOOM_INFO("[AnimTimeline] Duplicated clip '{}' as '{}'",
                              sourceClip->name, newClip->name);
                }
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Duplicate selected animation clip");
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedClipIndex < 0);
        if (ImGui::Button("Rename"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                ImGui::OpenPopup("RenameClipPopup");
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Rename selected animation clip");
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedClipIndex < 0 || (m_Animator && m_Animator->GetClipCount() <= 1));
        if (ImGui::Button("Delete"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                ImGui::OpenPopup("DeleteClipConfirm");
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Delete selected animation clip");
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedClipIndex < 0);
        if (ImGui::Button("Save"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                ImGui::OpenPopup("SaveClipPopup");
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Save animation clip to .anim file");
        }
        ImGui::EndDisabled();

        // Apply to Entity button - syncs changes without saving to file
        ImGui::SameLine();
        bool canApply = (m_SelectedClipIndex >= 0 && m_SourceEntityID != entt::null && !m_StandaloneMode);
        ImGui::BeginDisabled(!canApply);
        if (ImGui::Button("Apply"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0 && m_Ctx && m_SourceEntityID != entt::null)
            {
                if (m_Ctx->scene.valid(m_SourceEntityID))
                {
                    Boom::Entity entity(&m_Ctx->scene, static_cast<Boom::EntityID>(m_SourceEntityID));
                    if (entity.Has<Boom::AnimatorComponent>())
                    {
                        auto& animComp = entity.Get<Boom::AnimatorComponent>();
                        if (animComp.animator)
                        {
                            // Get the clip from timeline and COPY its data to entity
                            // We copy instead of sharing to keep timeline and entity clips independent
                            // This prevents gizmo manipulations from affecting the entity directly
                            const auto* timelineClip = m_Animator->GetClip(m_SelectedClipIndex);
                            if (timelineClip)
                            {
                                // Create a copy of the clip data for the entity
                                auto entityClip = std::make_shared<Boom::AnimationClip>();
                                entityClip->name = timelineClip->name;
                                entityClip->duration = timelineClip->duration;
                                entityClip->ticksPerSecond = timelineClip->ticksPerSecond;
                                entityClip->filePath = timelineClip->filePath;
                                entityClip->tracks = timelineClip->tracks;  // Deep copy tracks

                                if (m_SelectedClipIndex < (int)animComp.animator->GetClipCount())
                                {
                                    // Replace existing clip with copied data
                                    animComp.animator->SetClip(m_SelectedClipIndex, entityClip);
                                    BOOM_INFO("[AnimTimeline] Copied clip data to entity (index {})", m_SelectedClipIndex);
                                }
                                else
                                {
                                    // Add the copied clip to entity
                                    animComp.animator->AddClip(entityClip);
                                    BOOM_INFO("[AnimTimeline] Added copied clip '{}' to entity", entityClip->name);
                                }

                                // Force entity to play this clip
                                // Clear states to force legacy clip-based mode (simpler for editor preview)
                                auto& states = animComp.animator->GetStates();
                                states.clear();

                                // Set the clip and time
                                animComp.animator->PlayClip(m_SelectedClipIndex);
                                animComp.animator->SetTime(m_CurrentTime);

                                // Force immediate joint transform update
                                animComp.animator->UpdateJointsFromCurrentTime();

                                // Clear manual bone poses so animation plays cleanly
                                m_ManualBonePoses.clear();
                                m_HasManualPoses = false;

                                BOOM_INFO("[AnimTimeline] Applied - entity now has copy of clip (clip: '{}', time: {:.2f})",
                                    entityClip->name, m_CurrentTime);
                            }
                        }
                    }
                }
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Apply changes to entity and clear manual poses\n(Preview animation in real-time)");
        }
        ImGui::EndDisabled();

        // Create clip popup
        if (ImGui::BeginPopup("CreateClipPopup"))
        {
            static char clipName[128] = "New Clip";
            static float clipDuration = 1.0f;
            static float clipFPS = 30.0f;

            ImGui::Text("Create New Animation Clip");
            ImGui::Separator();

            ImGui::InputText("Name", clipName, sizeof(clipName));
            ImGui::InputFloat("Duration (s)", &clipDuration, 0.1f, 1.0f, "%.2f");
            ImGui::InputFloat("FPS", &clipFPS, 1.0f, 10.0f, "%.1f");

            if (clipDuration < 0.01f) clipDuration = 0.01f;
            if (clipFPS < 1.0f) clipFPS = 1.0f;

            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
                if (m_Animator)
                {
                    auto newClip = std::make_shared<Boom::AnimationClip>();
                    newClip->name = std::string(clipName);
                    newClip->duration = clipDuration;
                    newClip->ticksPerSecond = clipFPS;
                    newClip->filePath = "";  // No source file

                    // Add to animator
                    m_Animator->AddClip(newClip);
                    m_SelectedClipIndex = (int)m_Animator->GetClipCount() - 1;
                    m_Animator->PlayClip(m_SelectedClipIndex);
                    m_CurrentTime = 0.0f;
                    m_IsPlaying = false;

                    BOOM_INFO("[AnimTimeline] Created new clip '{}'", newClip->name);

                    // Reset form
                    strcpy_s(clipName, sizeof(clipName), "New Clip");
                    clipDuration = 1.0f;
                    clipFPS = 30.0f;
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Rename clip popup
        if (ImGui::BeginPopup("RenameClipPopup"))
        {
            static char newName[128] = "";
            static bool firstOpen = true;

            if (firstOpen)
            {
                if (m_Animator && m_SelectedClipIndex >= 0)
                {
                    auto* clip = m_Animator->GetClipMutable(m_SelectedClipIndex);
                    if (clip)
                    {
                        strncpy_s(newName, sizeof(newName), clip->name.c_str(), _TRUNCATE);
                    }
                }
                firstOpen = false;
            }

            ImGui::Text("Rename Animation Clip");
            ImGui::Separator();

            ImGui::InputText("New Name", newName, sizeof(newName));

            if (ImGui::Button("Rename", ImVec2(120, 0)))
            {
                if (m_Animator && m_SelectedClipIndex >= 0)
                {
                    auto* clip = m_Animator->GetClipMutable(m_SelectedClipIndex);
                    if (clip && strlen(newName) > 0)
                    {
                        std::string oldName = clip->name;
                        clip->name = std::string(newName);
                        BOOM_INFO("[AnimTimeline] Renamed clip '{}' to '{}'", oldName, clip->name);
                    }
                }
                firstOpen = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                firstOpen = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Delete clip confirmation popup
        if (ImGui::BeginPopupModal("DeleteClipConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
                if (clip)
                {
                    ImGui::Text("Are you sure you want to delete clip:");
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "  %s", clip->name.c_str());
                    ImGui::Text("This action cannot be undone!");
                }
            }

            ImGui::Separator();

            if (ImGui::Button("Delete", ImVec2(120, 0)))
            {
                if (m_Animator && m_SelectedClipIndex >= 0 && m_Animator->GetClipCount() > 1)
                {
                    const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
                    std::string clipName = clip ? clip->name : "Unknown";

                    m_Animator->RemoveClip(m_SelectedClipIndex);
                    BOOM_INFO("[AnimTimeline] Deleted clip '{}'", clipName);

                    // Select the first remaining clip
                    if (m_Animator->GetClipCount() > 0)
                    {
                        m_SelectedClipIndex = 0;
                        m_Animator->PlayClip(0);
                        m_CurrentTime = 0.0f;
                        m_IsPlaying = false;
                    }
                    else
                    {
                        m_SelectedClipIndex = -1;
                    }
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Save clip popup
        if (ImGui::BeginPopup("SaveClipPopup"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0)
            {
                auto* clip = m_Animator->GetClipMutable(m_SelectedClipIndex);
                if (clip)
                {
                    static char saveNameBuffer[128] = "";
                    static bool initBuffer = true;

                    if (initBuffer || ImGui::IsWindowAppearing())
                    {
                        // Pre-fill with clip name (sanitized for filename)
                        std::string safeName = clip->name;
                        std::replace(safeName.begin(), safeName.end(), ' ', '_');
                        strncpy_s(saveNameBuffer, sizeof(saveNameBuffer), safeName.c_str(), _TRUNCATE);
                        initBuffer = false;
                    }

                    ImGui::Text("Save Animation Clip to .anim File");
                    ImGui::Separator();

                    ImGui::Text("Filename:");
                    ImGui::SetNextItemWidth(250.0f);
                    ImGui::InputText("##SaveFileName", saveNameBuffer, sizeof(saveNameBuffer));
                    ImGui::SameLine();
                    ImGui::TextDisabled(".anim");

                    // Show preview path
                    std::string previewPath = "Resources/Animations/" + std::string(saveNameBuffer) + ".anim";
                    ImGui::TextDisabled("Path: %s", previewPath.c_str());

                    ImGui::Spacing();

                    if (ImGui::Button("Save", ImVec2(120, 0)))
                    {
                        std::string filename = saveNameBuffer;
                        if (!filename.empty())
                        {
                            std::string filepath = "Resources/Animations/" + filename + ".anim";

                            if (Boom::SaveAnimationClip(*clip, filepath))
                            {
                                // Update clip's filePath to point to the saved .anim file
                                clip->filePath = filepath;
                                m_ClipModified = false;  // Clear dirty flag
                                BOOM_INFO("[AnimTimeline] Saved clip '{}' to {}", clip->name, filepath);

                                // Sync changes back to the entity's animator
                                if (m_SourceEntityID != entt::null && m_Ctx)
                                {
                                    if (m_Ctx->scene.valid(m_SourceEntityID))
                                    {
                                        Boom::Entity entity(&m_Ctx->scene, static_cast<Boom::EntityID>(m_SourceEntityID));
                                        if (entity.Has<Boom::AnimatorComponent>())
                                        {
                                            auto& animComp = entity.Get<Boom::AnimatorComponent>();
                                            if (animComp.animator && m_SelectedClipIndex >= 0)
                                            {
                                                // Get shared clip pointer from timeline
                                                auto timelineClipPtr = m_Animator->GetClipShared(m_SelectedClipIndex);
                                                if (timelineClipPtr)
                                                {
                                                    // Share clip pointer with entity
                                                    if (m_SelectedClipIndex < (int)animComp.animator->GetClipCount())
                                                    {
                                                        animComp.animator->SetClip(m_SelectedClipIndex, timelineClipPtr);
                                                    }
                                                    else
                                                    {
                                                        animComp.animator->AddClip(timelineClipPtr);
                                                    }

                                                    // Clear states and force clip playback
                                                    auto& states = animComp.animator->GetStates();
                                                    states.clear();

                                                    animComp.animator->PlayClip(m_SelectedClipIndex);
                                                    animComp.animator->SetTime(m_CurrentTime);
                                                    animComp.animator->UpdateJointsFromCurrentTime();

                                                    // Clear manual poses
                                                    m_ManualBonePoses.clear();
                                                    m_HasManualPoses = false;

                                                    BOOM_INFO("[AnimTimeline] Synced - entity now shares clip pointer");
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            else
                            {
                                BOOM_ERROR("[AnimTimeline] Failed to save clip to {}", filepath);
                            }

                            initBuffer = true;
                            ImGui::CloseCurrentPopup();
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    {
                        initBuffer = true;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndPopup();
        }

        // Display clip information
        if (m_SelectedClipIndex >= 0 && m_SelectedClipIndex < (int)m_Animator->GetClipCount())
        {
            const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
            if (clip)
            {
                ImGui::SameLine();
                ImGui::Text("|");

                if (!m_CompactMode)
                {
                    // Full info display
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

                    // DEBUG: Playback state
                    ImGui::SameLine();
                    ImGui::TextColored(m_IsPlaying ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                        "| %s", m_IsPlaying ? "PLAYING" : "PAUSED");

                    ImGui::SameLine();
                    ImGui::TextColored(m_Loop ? ImVec4(0, 1, 1, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1),
                        "| Loop: %s", m_Loop ? "ON" : "OFF");
                }
                else
                {
                    // Compact mode - just essentials
                    ImGui::SameLine();
                    ImGui::Text("%.2fs", clip->duration);

                    ImGui::SameLine();
                    ImGui::TextColored(m_IsPlaying ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                        "| %s", m_IsPlaying ? "►" : "■");
                }
            }
        }

            ImGui::Unindent(10.0f);
            ImGui::Spacing();
        }
        ImGui::PopStyleColor(3); // Pop Animation Clip colors
    }
    else if (m_Animator)
    {
        ImGui::Separator();
        ImGui::TextDisabled("No animation clips available");
    }

    ImGui::EndGroup();
}

// Helper: Find keyframe index by timestamp
int AnimationTimelinePanel::FindKeyframeByTimestamp(const std::string& boneName, float timestamp, float tolerance)
{
    if (!m_Animator || m_SelectedClipIndex < 0) return -1;

    auto* track = m_Animator->GetTrackMutable(m_SelectedClipIndex, boneName);
    if (!track) return -1;

    for (size_t i = 0; i < track->size(); ++i)
    {
        if (std::abs((*track)[i].timeStamp - timestamp) < tolerance)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Helper: Execute a single command (does NOT modify undo/redo stacks)
void AnimationTimelinePanel::ExecuteSingleCommand(const KeyframeCommand& cmd)
{
    if (!m_Animator || m_SelectedClipIndex < 0) return;

    switch (cmd.type)
    {
    case KeyframeCommand::ADD:
        m_Animator->AddKeyframe(m_SelectedClipIndex, cmd.boneName, cmd.keyframe);
        break;

    case KeyframeCommand::REMOVE:
        {
            // Find by timestamp, not index (index may be stale)
            int idx = FindKeyframeByTimestamp(cmd.boneName, cmd.keyframe.timeStamp);
            if (idx >= 0)
            {
                m_Animator->RemoveKeyframe(m_SelectedClipIndex, cmd.boneName, static_cast<size_t>(idx));
            }
        }
        break;

    case KeyframeCommand::MOVE:
        {
            // Find by old timestamp
            int idx = FindKeyframeByTimestamp(cmd.boneName, cmd.oldTime);
            if (idx >= 0)
            {
                m_Animator->UpdateKeyframeTime(m_SelectedClipIndex, cmd.boneName, static_cast<size_t>(idx), cmd.newTime);
            }
        }
        break;

    case KeyframeCommand::BONE_POSE:
        {
            BonePose newPose;
            newPose.position = cmd.newPosition;
            newPose.rotation = cmd.newRotation;
            newPose.scale = cmd.newScale;
            SetBonePose(cmd.boneName, newPose);
        }
        break;

    case KeyframeCommand::BATCH:
        // Execute all sub-commands in order
        for (const auto& subCmd : cmd.batchCommands)
        {
            ExecuteSingleCommand(subCmd);
        }
        break;
    }
}

// Helper: Undo a single command
void AnimationTimelinePanel::UndoSingleCommand(const KeyframeCommand& cmd)
{
    if (!m_Animator || m_SelectedClipIndex < 0) return;

    switch (cmd.type)
    {
    case KeyframeCommand::ADD:
        {
            // Undo ADD = remove the keyframe (find by timestamp)
            int idx = FindKeyframeByTimestamp(cmd.boneName, cmd.keyframe.timeStamp);
            if (idx >= 0)
            {
                m_Animator->RemoveKeyframe(m_SelectedClipIndex, cmd.boneName, static_cast<size_t>(idx));
            }
        }
        break;

    case KeyframeCommand::REMOVE:
        // Undo REMOVE = add it back
        m_Animator->AddKeyframe(m_SelectedClipIndex, cmd.boneName, cmd.keyframe);
        break;

    case KeyframeCommand::MOVE:
        {
            // Undo MOVE = find by newTime and set back to oldTime
            int idx = FindKeyframeByTimestamp(cmd.boneName, cmd.newTime);
            if (idx >= 0)
            {
                m_Animator->UpdateKeyframeTime(m_SelectedClipIndex, cmd.boneName, static_cast<size_t>(idx), cmd.oldTime);
            }
        }
        break;

    case KeyframeCommand::BONE_POSE:
        {
            // Undo BONE_POSE = restore old pose
            BonePose oldPose;
            oldPose.position = cmd.oldPosition;
            oldPose.rotation = cmd.oldRotation;
            oldPose.scale = cmd.oldScale;
            SetBonePose(cmd.boneName, oldPose);
        }
        break;

    case KeyframeCommand::BATCH:
        // Undo batch = undo all sub-commands in REVERSE order
        for (auto it = cmd.batchCommands.rbegin(); it != cmd.batchCommands.rend(); ++it)
        {
            UndoSingleCommand(*it);
        }
        break;
    }
}

void AnimationTimelinePanel::ExecuteCommand(const KeyframeCommand& cmd)
{
    if (!m_Animator || m_SelectedClipIndex < 0) return;

    // Execute the command
    ExecuteSingleCommand(cmd);

    // Push to undo stack
    m_UndoStack.push_back(cmd);
    if (m_UndoStack.size() > MAX_UNDO_HISTORY)
    {
        m_UndoStack.erase(m_UndoStack.begin());
    }

    // Clear redo stack (new action invalidates redo history)
    m_RedoStack.clear();

    // Mark clip as modified
    m_ClipModified = true;

    // Log
    if (cmd.type == KeyframeCommand::BATCH)
    {
        BOOM_INFO("[Keyframes] Executed batch of {} commands", cmd.batchCommands.size());
    }
}

void AnimationTimelinePanel::Undo()
{
    if (m_UndoStack.empty() || !m_Animator || m_SelectedClipIndex < 0) return;

    KeyframeCommand cmd = m_UndoStack.back();
    m_UndoStack.pop_back();

    // Undo the command using helper
    UndoSingleCommand(cmd);

    // Push to redo stack
    m_RedoStack.push_back(cmd);

    // Update selection for BATCH operations (keyframe indices change after move/restore)
    if (cmd.type == KeyframeCommand::BATCH)
    {
        ClearKeyframeSelection();
        for (const auto& subCmd : cmd.batchCommands)
        {
            if (subCmd.type == KeyframeCommand::MOVE)
            {
                // After undo, keyframe is back at oldTime
                int newIdx = FindKeyframeByTimestamp(subCmd.boneName, subCmd.oldTime);
                if (newIdx >= 0)
                {
                    SelectKeyframe(subCmd.boneName, static_cast<size_t>(newIdx), true);
                }
            }
            else if (subCmd.type == KeyframeCommand::REMOVE)
            {
                // After undo of REMOVE, keyframe is restored - select it
                int newIdx = FindKeyframeByTimestamp(subCmd.boneName, subCmd.keyframe.timeStamp);
                if (newIdx >= 0)
                {
                    SelectKeyframe(subCmd.boneName, static_cast<size_t>(newIdx), true);
                }
            }
        }
        BOOM_INFO("[Undo] Reverted batch of {} commands", cmd.batchCommands.size());
    }

    // Mark clip as modified
    m_ClipModified = true;
}

void AnimationTimelinePanel::Redo()
{
    if (m_RedoStack.empty() || !m_Animator || m_SelectedClipIndex < 0) return;

    KeyframeCommand cmd = m_RedoStack.back();
    m_RedoStack.pop_back();

    // Re-execute the command using helper
    ExecuteSingleCommand(cmd);

    // Push back to undo stack
    m_UndoStack.push_back(cmd);

    // Update selection for BATCH operations (keyframe indices change after move)
    if (cmd.type == KeyframeCommand::BATCH)
    {
        ClearKeyframeSelection();
        for (const auto& subCmd : cmd.batchCommands)
        {
            if (subCmd.type == KeyframeCommand::MOVE)
            {
                // After redo, keyframe is at newTime
                int newIdx = FindKeyframeByTimestamp(subCmd.boneName, subCmd.newTime);
                if (newIdx >= 0)
                {
                    SelectKeyframe(subCmd.boneName, static_cast<size_t>(newIdx), true);
                }
            }
            // For REMOVE redo, keyframes are deleted so don't select
        }
        BOOM_INFO("[Redo] Re-executed batch of {} commands", cmd.batchCommands.size());
    }

    // Mark clip as modified
    m_ClipModified = true;
}

void AnimationTimelinePanel::RecordBonePoseChange(const std::string& boneName, const BonePose& oldPose, const BonePose& newPose)
{
    // Create a BONE_POSE command and add to undo stack (don't execute, pose is already applied)
    KeyframeCommand cmd;
    cmd.type = KeyframeCommand::BONE_POSE;
    cmd.boneName = boneName;
    cmd.oldPosition = oldPose.position;
    cmd.oldRotation = oldPose.rotation;
    cmd.oldScale = oldPose.scale;
    cmd.newPosition = newPose.position;
    cmd.newRotation = newPose.rotation;
    cmd.newScale = newPose.scale;

    // Push to undo stack
    m_UndoStack.push_back(cmd);
    if (m_UndoStack.size() > MAX_UNDO_HISTORY)
    {
        m_UndoStack.erase(m_UndoStack.begin()); // Remove oldest
    }

    // Clear redo stack (new action invalidates redo history)
    m_RedoStack.clear();

    BOOM_INFO("[BonePose] Recorded pose change for bone '{}' (undoable)", boneName.c_str());
}
