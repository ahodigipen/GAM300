// ===================================================================
// AnimationTimelinePanel_Timeline.cpp
// Timeline Ruler, Track List, and Keyframe Functions
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
#include "Vendors/imgui/imgui.h"
#include "common/Core.h"
#include <cmath>
#include <algorithm>

using namespace EditorUI;

// ========== Timeline Ruler ==========

void AnimationTimelinePanel::RenderTimelineRuler()
{
    ImGui::BeginGroup();

    // Get animation duration
    float duration = 1.0f;  // Default
    if (m_Animator && m_SelectedClipIndex >= 0 && static_cast<size_t>(m_SelectedClipIndex) < m_Animator->GetClipCount())
    {
        const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
        if (clip)
        {
            duration = clip->duration;
        }
    }

    // Timeline ruler dimensions
    const float rulerHeight = 50.0f;
    ImVec2 rulerSize = ImVec2(ImGui::GetContentRegionAvail().x, rulerHeight);
    ImVec2 rulerPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw ruler background with subtle gradient
    drawList->AddRectFilledMultiColor(
        rulerPos,
        ImVec2(rulerPos.x + rulerSize.x, rulerPos.y + rulerSize.y),
        IM_COL32(55, 55, 55, 255),  // Top-left
        IM_COL32(55, 55, 55, 255),  // Top-right
        IM_COL32(45, 45, 45, 255),  // Bottom-right
        IM_COL32(45, 45, 45, 255)   // Bottom-left
    );

    // Draw top border (separator from viewport)
    drawList->AddLine(
        rulerPos,
        ImVec2(rulerPos.x + rulerSize.x, rulerPos.y),
        IM_COL32(80, 80, 80, 255),
        1.0f
    );

    // Draw bottom border (separator from bone tracks)
    drawList->AddLine(
        ImVec2(rulerPos.x, rulerPos.y + rulerSize.y),
        ImVec2(rulerPos.x + rulerSize.x, rulerPos.y + rulerSize.y),
        IM_COL32(100, 100, 100, 255),
        2.0f
    );

    // Draw time markers with dynamic spacing based on available width
    if (duration > 0.0f && rulerSize.x > 0.0f)
    {
        // Calculate pixels per second
        float pixelsPerSecond = rulerSize.x / duration;

        // Choose marker interval based on zoom level
        // We want major markers roughly every 60-120 pixels
        float majorInterval = 1.0f;  // Default: 1 second
        float minorInterval = 0.5f;  // Default: 0.5 seconds

        if (pixelsPerSecond < 30.0f)  // Very zoomed out (< 30px per second)
        {
            majorInterval = 10.0f;
            minorInterval = 5.0f;
        }
        else if (pixelsPerSecond < 60.0f)  // Zoomed out (30-60px per second)
        {
            majorInterval = 5.0f;
            minorInterval = 1.0f;
        }
        else if (pixelsPerSecond < 100.0f)  // Medium (60-100px per second)
        {
            majorInterval = 2.0f;
            minorInterval = 1.0f;
        }
        else if (pixelsPerSecond < 150.0f)  // Medium-close (100-150px per second)
        {
            majorInterval = 1.0f;
            minorInterval = 0.5f;
        }
        else if (pixelsPerSecond < 300.0f)  // Close (150-300px per second)
        {
            majorInterval = 0.5f;
            minorInterval = 0.1f;
        }
        else  // Very zoomed in (> 300px per second)
        {
            majorInterval = 0.1f;
            minorInterval = 0.05f;
        }

        // Draw major markers
        for (float t = 0.0f; t <= duration; t += majorInterval)
        {
            float normalizedTime = t / duration;
            float x = rulerPos.x + normalizedTime * rulerSize.x;

            // Major marker (longer line + time label)
            drawList->AddLine(
                ImVec2(x, rulerPos.y + rulerHeight - 20.0f),
                ImVec2(x, rulerPos.y + rulerHeight),
                IM_COL32(200, 200, 200, 255),
                1.5f
            );

            // Time label with smart formatting
            char timeLabel[16];
            if (majorInterval >= 1.0f)
            {
                snprintf(timeLabel, sizeof(timeLabel), "%.0fs", t);  // No decimals for >= 1s intervals
            }
            else
            {
                snprintf(timeLabel, sizeof(timeLabel), "%.1fs", t);  // 1 decimal for < 1s intervals
            }

            // Calculate text size for proper centering and edge clamping
            ImVec2 textSize = ImGui::CalcTextSize(timeLabel);
            float textX = x - (textSize.x * 0.5f);  // Center text on tick mark

            // Clamp text to stay within ruler bounds
            textX = (textX < rulerPos.x) ? rulerPos.x : textX;  // Don't go off left edge
            textX = (textX + textSize.x > rulerPos.x + rulerSize.x) ? (rulerPos.x + rulerSize.x - textSize.x) : textX;  // Don't go off right edge

            drawList->AddText(
                ImVec2(textX, rulerPos.y + 5.0f),
                IM_COL32(220, 220, 220, 255),
                timeLabel
            );
        }

        // Draw minor markers (only if there's enough space)
        if (pixelsPerSecond > 40.0f)  // Only show minor markers when not too cramped
        {
            for (float t = 0.0f; t <= duration; t += minorInterval)
            {
                // Skip if this is already a major marker
                if (fmod(t, majorInterval) < 0.001f) continue;

                float normalizedTime = t / duration;
                float x = rulerPos.x + normalizedTime * rulerSize.x;

                // Minor marker (shorter line)
                drawList->AddLine(
                    ImVec2(x, rulerPos.y + rulerHeight - 10.0f),
                    ImVec2(x, rulerPos.y + rulerHeight),
                    IM_COL32(150, 150, 150, 255),
                    1.0f
                );
            }
        }
    }

    // Draw playhead (red vertical line)
    if (duration > 0.0f)
    {
        float normalizedTime = m_CurrentTime / duration;
        normalizedTime = (normalizedTime < 0.0f) ? 0.0f : (normalizedTime > 1.0f) ? 1.0f : normalizedTime;
        float playheadX = rulerPos.x + normalizedTime * rulerSize.x;

        // Playhead line
        drawList->AddLine(
            ImVec2(playheadX, rulerPos.y),
            ImVec2(playheadX, rulerPos.y + rulerHeight),
            IM_COL32(255, 80, 80, 255),
            3.0f
        );

        // Playhead triangle (at top)
        ImVec2 triangleTop(playheadX, rulerPos.y);
        ImVec2 triangleLeft(playheadX - 6.0f, rulerPos.y + 12.0f);
        ImVec2 triangleRight(playheadX + 6.0f, rulerPos.y + 12.0f);
        drawList->AddTriangleFilled(triangleTop, triangleLeft, triangleRight, IM_COL32(255, 80, 80, 255));

        // Current time display (Unity-style: frame number + seconds)
        const float fps = 30.0f;  // Standard animation framerate
        int currentFrame = (int)(m_CurrentTime * fps);
        int totalFrames = (int)(duration * fps);

        char currentTimeLabel[64];
        snprintf(currentTimeLabel, sizeof(currentTimeLabel), "Frame %d / %d  (%.2fs / %.2fs)",
                 currentFrame, totalFrames, m_CurrentTime, duration);

        ImVec2 timeDisplaySize = ImGui::CalcTextSize(currentTimeLabel);
        float timeDisplayX = rulerPos.x + rulerSize.x - timeDisplaySize.x - 5.0f;  // 5px padding from right edge

        // Draw semi-transparent background for readability
        drawList->AddRectFilled(
            ImVec2(timeDisplayX - 3.0f, rulerPos.y + rulerHeight - 20.0f),
            ImVec2(timeDisplayX + timeDisplaySize.x + 3.0f, rulerPos.y + rulerHeight - 2.0f),
            IM_COL32(0, 0, 0, 150)
        );

        drawList->AddText(
            ImVec2(timeDisplayX, rulerPos.y + rulerHeight - 18.0f),
            IM_COL32(255, 255, 255, 255),
            currentTimeLabel
        );
    }

    // Make the ruler interactive (invisible button over the entire area)
    ImGui::SetCursorScreenPos(rulerPos);
    ImGui::InvisibleButton("TimelineRuler", rulerSize);

    // Handle timeline scrubbing
    if (ImGui::IsItemHovered())
    {
        // Change cursor to indicate interactivity
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        // Click to jump to time
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_IsDraggingTimeline = true;
            m_IsPlaying = false;  // Pause playback when scrubbing
        }
    }

    // Handle dragging
    if (m_IsDraggingTimeline)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // Calculate time from mouse position
            ImVec2 mousePos = ImGui::GetMousePos();
            float normalizedTime = (mousePos.x - rulerPos.x) / rulerSize.x;
            normalizedTime = (normalizedTime < 0.0f) ? 0.0f : (normalizedTime > 1.0f) ? 1.0f : normalizedTime;

            m_CurrentTime = normalizedTime * duration;

            // Update animator to this time (if we have one)
            if (m_Animator)
            {
                m_Animator->SetTime(m_CurrentTime);
                m_Animator->Animate(0.0f);  // Update with 0 dt to just apply the time
            }
        }
        else
        {
            // Mouse released - stop dragging
            m_IsDraggingTimeline = false;
        }
    }

    ImGui::EndGroup();
}

// ========== Track List and Bone Tracks ==========

void AnimationTimelinePanel::RenderTrackList()
{
    ImGui::BeginGroup();

    ImGui::Text("BONE TRACKS");

    // Check if we have a valid animator with skeleton
    if (!m_Animator || !m_HasModel)
    {
        ImGui::TextDisabled("No model loaded");
        ImGui::EndGroup();
        return;
    }

    // Get animation duration for timeline scaling
    float duration = 1.0f;  // Default
    if (m_SelectedClipIndex >= 0 && static_cast<size_t>(m_SelectedClipIndex) < m_Animator->GetClipCount())
    {
        const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
        if (clip)
        {
            duration = clip->duration;
        }
    }

    if (ImGui::BeginChild("TrackListScroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        // Setup two columns: bone names (left) and timeline tracks (right)
        const float boneNameWidth = 250.0f;
        ImGui::Columns(2, "BoneTrackColumns", true);
        ImGui::SetColumnWidth(0, boneNameWidth);

        // Get the root joint from animator
        const Boom::Joint& root = m_Animator->GetRoot();

        // Render bone hierarchy starting from root
        RenderBoneTrack(root, duration);

        // End columns
        ImGui::Columns(1);
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

void AnimationTimelinePanel::RenderBoneTrack(const Boom::Joint& joint, float duration)
{
    // === COLUMN 0: Bone Name (with tree hierarchy) ===

    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    // Highlight if selected
    if (joint.name == m_SelectedBoneName)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // If no children, make it a leaf node
    if (joint.children.empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Default to open for root and first level
    if (joint.index == 0 || joint.index == 1)
    {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    }

    // Store row position for timeline drawing
    ImVec2 rowStartPos = ImGui::GetCursorScreenPos();
    float rowHeight = ImGui::GetTextLineHeightWithSpacing();

    // Display bone name with tree node in COLUMN 0
    std::string label = joint.name + " [" + std::to_string(joint.index) + "]";
    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

    // Handle selection
    if (ImGui::IsItemClicked())
    {
        m_SelectedBoneName = joint.name;
        // Sync with global context for viewport highlighting
        if (m_Ctx)
        {
            m_Ctx->SelectedBoneName = joint.name;
        }
    }

    // Tooltip with bone info
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Bone: %s", joint.name.c_str());
        ImGui::Text("Index: %d", joint.index);
        ImGui::Text("Children: %zu", joint.children.size());
        ImGui::EndTooltip();
    }

    // === COLUMN 1: Timeline Track (perfectly aligned) ===
    ImGui::NextColumn();

    // Get the timeline area dimensions
    ImVec2 timelineStartPos = ImGui::GetCursorScreenPos();
    float timelineWidth = ImGui::GetColumnWidth(1) - 10.0f; // Leave some padding

    // Adjust vertical position to match the tree node row
    timelineStartPos.y = rowStartPos.y;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Timeline background (dark gray)
    ImVec2 timelineMin = timelineStartPos;
    ImVec2 timelineMax(timelineMin.x + timelineWidth, timelineMin.y + rowHeight);
    drawList->AddRectFilled(timelineMin, timelineMax, IM_COL32(40, 40, 40, 255));

    // Draw grid lines for time markers (every second)
    if (duration > 0.0f)
    {
        for (float t = 0.0f; t <= duration; t += 1.0f)
        {
            float x = timelineMin.x + (t / duration) * timelineWidth;
            drawList->AddLine(
                ImVec2(x, timelineMin.y),
                ImVec2(x, timelineMax.y),
                IM_COL32(80, 80, 80, 255)
            );
        }
    }

    // Draw current time indicator (red line)
    if (duration > 0.0f && m_CurrentTime >= 0.0f)
    {
        float normalizedTime = m_CurrentTime / duration;
        normalizedTime = (normalizedTime < 0.0f) ? 0.0f : (normalizedTime > 1.0f) ? 1.0f : normalizedTime;
        float x = timelineMin.x + normalizedTime * timelineWidth;
        drawList->AddLine(
            ImVec2(x, timelineMin.y),
            ImVec2(x, timelineMax.y),
            IM_COL32(255, 0, 0, 255),
            2.0f
        );
    }

    // Draw and interact with keyframe diamonds
    if (m_Animator && m_SelectedClipIndex >= 0 && duration > 0.0f)
    {
        const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
        if (clip)
        {
            const auto* keyframes = clip->GetTrack(joint.name);
            if (keyframes && !keyframes->empty())
            {
                ImVec2 mousePos = ImGui::GetMousePos();

                // Track if we're hovering any keyframe on THIS bone (local to this iteration)
                bool hoveredAnyKeyframe = false;

                // Draw diamond at each keyframe timestamp
                for (size_t i = 0; i < keyframes->size(); ++i)
                {
                    const auto& kf = (*keyframes)[i];

                    // Calculate X position based on timestamp
                    float normalizedTime = kf.timeStamp / duration;
                    float x = timelineMin.x + normalizedTime * timelineWidth;

                    // If we're dragging this keyframe, use mouse position for X
                    if (m_IsDraggingKeyframe && m_DraggedBoneName == joint.name && m_DraggedKeyframeIndex == i)
                    {
                        x = mousePos.x;
                        x = std::max(timelineMin.x, std::min(x, timelineMin.x + timelineWidth)); // Clamp to timeline bounds
                    }

                    // Diamond center (vertically centered in row)
                    ImVec2 center(x, timelineMin.y + rowHeight * 0.5f);
                    float size = 3.0f;
                    float hitTestSize = 6.0f; // Larger hit area for easier clicking

                    // Diamond vertices (rotated square)
                    ImVec2 top(center.x, center.y - size);
                    ImVec2 right(center.x + size, center.y);
                    ImVec2 bottom(center.x, center.y + size);
                    ImVec2 left(center.x - size, center.y);

                    // Check if mouse is hovering over this keyframe
                    bool isHovered = (mousePos.x >= center.x - hitTestSize && mousePos.x <= center.x + hitTestSize &&
                                      mousePos.y >= center.y - hitTestSize && mousePos.y <= center.y + hitTestSize);

                    // Determine color based on state
                    ImU32 fillColor = IM_COL32(255, 200, 0, 255);  // Default gold
                    ImU32 outlineColor = IM_COL32(200, 150, 0, 255);

                    if (m_IsDraggingKeyframe && m_DraggedBoneName == joint.name && m_DraggedKeyframeIndex == i)
                    {
                        // Being dragged - bright cyan
                        fillColor = IM_COL32(0, 255, 255, 255);
                        outlineColor = IM_COL32(0, 200, 200, 255);
                        size = 4.0f; // Slightly larger when dragging
                    }
                    else if (isHovered)
                    {
                        // Hovered - brighter yellow
                        fillColor = IM_COL32(255, 255, 100, 255);
                        outlineColor = IM_COL32(255, 200, 0, 255);
                        size = 4.0f; // Slightly larger when hovered
                    }

                    // Recalculate vertices with potentially new size
                    top = ImVec2(center.x, center.y - size);
                    right = ImVec2(center.x + size, center.y);
                    bottom = ImVec2(center.x, center.y + size);
                    left = ImVec2(center.x - size, center.y);

                    // Draw filled diamond
                    drawList->AddQuadFilled(top, right, bottom, left, fillColor);

                    // Draw outline for better visibility
                    drawList->AddQuad(top, right, bottom, left, outlineColor, 1.5f);

                    // Handle mouse interactions
                    if (isHovered && !m_IsDraggingKeyframe)
                    {
                        // Mark that we're hovering a keyframe on this bone
                        hoveredAnyKeyframe = true;

                        // Set hover state
                        m_HoveredKeyframeIndex = (int)i;
                        m_HoveredBoneName = joint.name;

                        // Show tooltip
                        ImGui::SetTooltip("Keyframe at %.2fs\nLeft-click to drag\nRight-click to delete", kf.timeStamp);

                        // Start dragging on left-click
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            m_IsDraggingKeyframe = true;
                            m_DraggedBoneName = joint.name;
                            m_DraggedKeyframeIndex = i;
                        }

                        // Delete on right-click
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                        {
                            // Create and execute remove command
                            KeyframeCommand cmd;
                            cmd.type = KeyframeCommand::REMOVE;
                            cmd.boneName = joint.name;
                            cmd.keyframeIndex = i;
                            cmd.keyframe = kf; // Store the keyframe data for undo
                            ExecuteCommand(cmd);
                            break; // Exit loop since we modified the array
                        }
                    }
                }

                // Handle drag release
                if (m_IsDraggingKeyframe && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    // Calculate new timestamp from mouse position
                    float newTime = ((mousePos.x - timelineMin.x) / timelineWidth) * duration;
                    newTime = std::max(0.0f, std::min(newTime, duration)); // Clamp to clip duration

                    // Get the old timestamp before moving
                    auto* track = m_Animator->GetTrackMutable(m_SelectedClipIndex, m_DraggedBoneName);
                    if (track && m_DraggedKeyframeIndex < track->size())
                    {
                        float oldTime = (*track)[m_DraggedKeyframeIndex].timeStamp;

                        // Only create command if time actually changed
                        if (std::abs(oldTime - newTime) > 0.001f)
                        {
                            // Create and execute move command
                            KeyframeCommand cmd;
                            cmd.type = KeyframeCommand::MOVE;
                            cmd.boneName = m_DraggedBoneName;
                            cmd.keyframeIndex = m_DraggedKeyframeIndex;
                            cmd.oldTime = oldTime;
                            cmd.newTime = newTime;
                            ExecuteCommand(cmd);
                        }
                    }

                    m_IsDraggingKeyframe = false;
                }

                // NOTE: "Click to add keyframe" feature disabled
                // This is not Unity-like, and without bone manipulation capability,
                // adding keyframes with identity transforms would break animations.
                // TODO: Re-enable once we can capture actual bone transforms from 3D viewport
            }
        }
    }

    // Add invisible dummy item to properly extend window bounds
    ImGui::Dummy(ImVec2(timelineWidth, rowHeight));

    // Return to COLUMN 0 for next bone
    ImGui::NextColumn();

    // === Recurse to children if node is open ===
    if (nodeOpen && !joint.children.empty())
    {
        for (const auto& child : joint.children)
        {
            RenderBoneTrack(child, duration);
        }
        ImGui::TreePop();
    }
}
