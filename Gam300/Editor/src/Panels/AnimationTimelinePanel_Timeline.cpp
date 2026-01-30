// ===================================================================
// AnimationTimelinePanel_Timeline.cpp
// Timeline Ruler, Track List, and Keyframe Functions
// ===================================================================

#include "Panels/AnimationTimelinePanel.h"
#include "Editor.h"

// Static variable for new audio event timestamp (set during right-click)
static float s_NewEventTimestamp = 0.0f;
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
#include <filesystem>

using namespace EditorUI;

// ========== Audio File Browser Helper ==========

// Get list of audio files from Resources/Audio directory
static std::vector<std::string> GetAvailableAudioFiles()
{
    std::vector<std::string> audioFiles;
    const std::string audioDir = "Resources/Audio";

    try
    {
        if (std::filesystem::exists(audioDir))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(audioDir))
            {
                if (entry.is_regular_file())
                {
                    std::string ext = entry.path().extension().string();
                    // Convert to lowercase for comparison
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
                    {
                        // Store path relative to Resources/Audio
                        std::string relativePath = entry.path().string();
                        // Normalize path separators
                        std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
                        audioFiles.push_back(relativePath);
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        BOOM_WARN("[AudioBrowser] Failed to scan audio directory: {}", e.what());
    }

    // Sort alphabetically
    std::sort(audioFiles.begin(), audioFiles.end());
    return audioFiles;
}

// ========== Helper Functions ==========

// Check if a joint or any of its descendants has the given name
static bool JointContainsDescendant(const Boom::Joint& joint, const std::string& targetName)
{
    if (joint.name == targetName) return true;

    for (const auto& child : joint.children)
    {
        if (JointContainsDescendant(child, targetName)) return true;
    }
    return false;
}

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

            // Disable audio events while scrubbing to prevent spam
            if (m_Animator)
            {
                m_Animator->SetAudioEventsEnabled(false);
            }
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

            // Re-enable audio events and reset triggers after scrubbing
            if (m_Animator)
            {
                m_Animator->ResetAudioEventTriggers();
                m_Animator->SetAudioEventsEnabled(true);
            }
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

    // Clear keyframe and audio marker screen positions for this frame
    m_KeyframeScreenPositions.clear();
    m_AudioMarkerScreenPositions.clear();

    // Reset hover state each frame (important for selection detection)
    m_HoveredKeyframeIndex = -1;
    m_HoveredBoneName.clear();
    m_HoveredAudioEventIndex = -1;

    if (ImGui::BeginChild("TrackListScroll", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
    {
        // Setup two columns: bone names (left) and timeline tracks (right)
        // Use proportional width: 25% for bone names (min 150px, max 300px) and 75% for timeline
        float availWidth = ImGui::GetContentRegionAvail().x;
        float boneNameWidth = availWidth * 0.25f;
        boneNameWidth = (boneNameWidth < 150.0f) ? 150.0f : (boneNameWidth > 300.0f) ? 300.0f : boneNameWidth;
        ImGui::Columns(2, "BoneTrackColumns", true);
        ImGui::SetColumnWidth(0, boneNameWidth);

        // ========== AUDIO EVENTS TRACK (before bone tracks) ==========
        RenderAudioTrack(duration);

        // Reduce indentation spacing to show more of bone names
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 10.0f);  // Default is ~21

        // Get the root joint from animator
        const Boom::Joint& root = m_Animator->GetRoot();

        // Render bone hierarchy starting from root
        RenderBoneTrack(root, duration);

        ImGui::PopStyleVar();  // IndentSpacing

        // End columns
        ImGui::Columns(1);

        // === Box Selection Handling ===
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 mousePos = ImGui::GetMousePos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Get the child window bounds for clipping
        ImVec2 childMin = ImGui::GetWindowPos();
        ImVec2 childMax = ImVec2(childMin.x + ImGui::GetWindowSize().x, childMin.y + ImGui::GetWindowSize().y);

        // Check if mouse is in the track list area and window is hovered
        bool mouseInTrackArea = (mousePos.x >= childMin.x && mousePos.x <= childMax.x &&
                                 mousePos.y >= childMin.y && mousePos.y <= childMax.y);
        bool windowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        // Start box selection on left-click in empty space (not on keyframe, not dragging)
        if (mouseInTrackArea && windowHovered && !m_IsDraggingKeyframe && !m_IsBoxSelecting &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_HoveredKeyframeIndex < 0)
        {
            m_IsBoxSelecting = true;
            m_BoxSelectStart = mousePos;
            m_BoxSelectEnd = mousePos;
            m_BoxSelectAdditive = io.KeyCtrl;  // Ctrl = add to selection
        }

        // Update box selection during drag
        if (m_IsBoxSelecting)
        {
            m_BoxSelectEnd = mousePos;

            // Cancel box selection on Escape
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                m_IsBoxSelecting = false;
            }
            else
            {
                // Calculate box bounds (handle any drag direction)
                float minX = std::min(m_BoxSelectStart.x, m_BoxSelectEnd.x);
                float maxX = std::max(m_BoxSelectStart.x, m_BoxSelectEnd.x);
                float minY = std::min(m_BoxSelectStart.y, m_BoxSelectEnd.y);
                float maxY = std::max(m_BoxSelectStart.y, m_BoxSelectEnd.y);

                // Draw selection rectangle
                ImU32 fillColor = IM_COL32(100, 150, 255, 50);    // Semi-transparent blue
                ImU32 borderColor = IM_COL32(100, 150, 255, 200); // Blue border
                drawList->AddRectFilled(ImVec2(minX, minY), ImVec2(maxX, maxY), fillColor);
                drawList->AddRect(ImVec2(minX, minY), ImVec2(maxX, maxY), borderColor, 0.0f, 0, 1.5f);

                // End box selection on mouse release
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    // Select all keyframes within the box
                    if (!m_BoxSelectAdditive)
                    {
                        ClearKeyframeSelection();
                    }

                    for (const auto& kfPos : m_KeyframeScreenPositions)
                    {
                        // Check if keyframe center is within the box
                        if (kfPos.screenPos.x >= minX && kfPos.screenPos.x <= maxX &&
                            kfPos.screenPos.y >= minY && kfPos.screenPos.y <= maxY)
                        {
                            SelectKeyframe(kfPos.boneName, kfPos.keyframeIndex, true);
                        }
                    }

                    m_IsBoxSelecting = false;
                }
            }
        }

        // ========== AUDIO EVENT POPUPS ==========

        // Add Audio Event Popup
        if (ImGui::BeginPopup("AddAudioEventPopup"))
        {
            static char soundFile[256] = "";
            static char eventName[128] = "";
            static float volume = 1.0f;
            static float pitch = 1.0f;
            static bool is3D = false;
            static bool loop = false;
            static int groupIndex = 0;
            const char* groups[] = { "SFX", "Music", "Ambience", "Voice" };
            static std::vector<std::string> cachedAudioFiles;
            static bool audioFilesLoaded = false;

            ImGui::Text("Add Audio Event");
            ImGui::Separator();

            ImGui::InputText("Event Name", eventName, sizeof(eventName));

            // Sound file input with Browse button
            ImGui::InputText("Sound File", soundFile, sizeof(soundFile));
            ImGui::SameLine();
            if (ImGui::Button("Browse...##AddBrowse"))
            {
                // Cache audio files on first browse
                if (!audioFilesLoaded)
                {
                    cachedAudioFiles = GetAvailableAudioFiles();
                    audioFilesLoaded = true;
                }
                ImGui::OpenPopup("AudioFileBrowser##Add");
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Browse audio files from Resources/Audio");
            }

            // Audio file browser popup
            if (ImGui::BeginPopup("AudioFileBrowser##Add"))
            {
                ImGui::Text("Select Audio File");
                ImGui::Separator();

                // Refresh button
                if (ImGui::Button("Refresh"))
                {
                    cachedAudioFiles = GetAvailableAudioFiles();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(%zu files)", cachedAudioFiles.size());

                ImGui::BeginChild("AudioFileList", ImVec2(350, 200), true);
                for (const auto& file : cachedAudioFiles)
                {
                    // Show just the filename, with full path on hover
                    std::string filename = std::filesystem::path(file).filename().string();
                    if (ImGui::Selectable(filename.c_str()))
                    {
                        strncpy_s(soundFile, sizeof(soundFile), file.c_str(), _TRUNCATE);
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", file.c_str());
                    }
                }
                ImGui::EndChild();

                ImGui::EndPopup();
            }

            ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Pitch", &pitch, 0.5f, 2.0f, "%.2f");

            ImGui::Checkbox("3D Sound", &is3D);
            ImGui::SameLine();
            ImGui::Checkbox("Loop", &loop);

            ImGui::Combo("Group", &groupIndex, groups, IM_ARRAYSIZE(groups));

            ImGui::Spacing();

            if (ImGui::Button("Add", ImVec2(120, 0)))
            {
                if (m_Animator && m_SelectedClipIndex >= 0)
                {
                    // Create new audio event with timestamp from right-click
                    Boom::AudioEventMarker newEvent;
                    newEvent.timeStamp = s_NewEventTimestamp;
                    newEvent.soundFile = std::string(soundFile);
                    newEvent.eventName = std::string(eventName);
                    newEvent.volume = volume;
                    newEvent.pitch = pitch;
                    newEvent.is3D = is3D;
                    newEvent.loop = loop;
                    newEvent.groupName = std::string(groups[groupIndex]);

                    // Use command system for undo/redo support
                    KeyframeCommand cmd;
                    cmd.type = KeyframeCommand::AUDIO_ADD;
                    cmd.audioEvent = newEvent;
                    ExecuteCommand(cmd);

                    // Reset form
                    soundFile[0] = '\0';
                    eventName[0] = '\0';
                    volume = 1.0f;
                    pitch = 1.0f;
                    is3D = false;
                    loop = false;
                    groupIndex = 0;
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

        // Edit Audio Event Popup
        if (ImGui::BeginPopup("EditAudioEventPopup"))
        {
            if (m_Animator && m_SelectedClipIndex >= 0 && m_SelectedAudioEventIndex >= 0)
            {
                auto* clip = m_Animator->GetClipMutable(m_SelectedClipIndex);
                if (clip && m_SelectedAudioEventIndex < (int)clip->audioEvents.size())
                {
                    auto& audioEvent = clip->audioEvents[m_SelectedAudioEventIndex];

                    static char soundFile[256] = "";
                    static char eventName[128] = "";
                    static float volume = 1.0f;
                    static float pitch = 1.0f;
                    static bool is3D = false;
                    static bool loop = false;
                    static int groupIndex = 0;
                    const char* groups[] = { "SFX", "Music", "Ambience", "Voice" };
                    static bool initialized = false;
                    static Boom::AudioEventMarker originalEvent;  // Store original for undo
                    static std::vector<std::string> cachedAudioFiles;
                    static bool audioFilesLoaded = false;

                    // Initialize form with current event data on first open
                    if (!initialized || ImGui::IsWindowAppearing())
                    {
                        // Capture original state for undo
                        originalEvent = audioEvent;

                        strncpy_s(soundFile, sizeof(soundFile), audioEvent.soundFile.c_str(), _TRUNCATE);
                        strncpy_s(eventName, sizeof(eventName), audioEvent.eventName.c_str(), _TRUNCATE);
                        volume = audioEvent.volume;
                        pitch = audioEvent.pitch;
                        is3D = audioEvent.is3D;
                        loop = audioEvent.loop;

                        // Find group index
                        groupIndex = 0;
                        for (int i = 0; i < IM_ARRAYSIZE(groups); i++)
                        {
                            if (audioEvent.groupName == groups[i])
                            {
                                groupIndex = i;
                                break;
                            }
                        }
                        initialized = true;
                    }

                    ImGui::Text("Edit Audio Event");
                    ImGui::Separator();

                    ImGui::Text("Time: %.2fs", audioEvent.timeStamp);

                    ImGui::InputText("Event Name", eventName, sizeof(eventName));

                    // Sound file input with Browse button
                    ImGui::InputText("Sound File", soundFile, sizeof(soundFile));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse...##EditBrowse"))
                    {
                        // Cache audio files on first browse
                        if (!audioFilesLoaded)
                        {
                            cachedAudioFiles = GetAvailableAudioFiles();
                            audioFilesLoaded = true;
                        }
                        ImGui::OpenPopup("AudioFileBrowser##Edit");
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("Browse audio files from Resources/Audio");
                    }

                    // Audio file browser popup
                    if (ImGui::BeginPopup("AudioFileBrowser##Edit"))
                    {
                        ImGui::Text("Select Audio File");
                        ImGui::Separator();

                        // Refresh button
                        if (ImGui::Button("Refresh"))
                        {
                            cachedAudioFiles = GetAvailableAudioFiles();
                        }
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%zu files)", cachedAudioFiles.size());

                        ImGui::BeginChild("AudioFileListEdit", ImVec2(350, 200), true);
                        for (const auto& file : cachedAudioFiles)
                        {
                            // Show just the filename, with full path on hover
                            std::string filename = std::filesystem::path(file).filename().string();
                            if (ImGui::Selectable(filename.c_str()))
                            {
                                strncpy_s(soundFile, sizeof(soundFile), file.c_str(), _TRUNCATE);
                                ImGui::CloseCurrentPopup();
                            }
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("%s", file.c_str());
                            }
                        }
                        ImGui::EndChild();

                        ImGui::EndPopup();
                    }

                    ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f");
                    ImGui::SliderFloat("Pitch", &pitch, 0.5f, 2.0f, "%.2f");

                    ImGui::Checkbox("3D Sound", &is3D);
                    ImGui::SameLine();
                    ImGui::Checkbox("Loop", &loop);

                    ImGui::Combo("Group", &groupIndex, groups, IM_ARRAYSIZE(groups));

                    ImGui::Spacing();

                    if (ImGui::Button("Save", ImVec2(120, 0)))
                    {
                        // Create new event with updated values
                        Boom::AudioEventMarker newEvent;
                        newEvent.timeStamp = audioEvent.timeStamp;  // Keep original timestamp
                        newEvent.soundFile = std::string(soundFile);
                        newEvent.eventName = std::string(eventName);
                        newEvent.volume = volume;
                        newEvent.pitch = pitch;
                        newEvent.is3D = is3D;
                        newEvent.loop = loop;
                        newEvent.groupName = std::string(groups[groupIndex]);

                        // Use command system for undo/redo support
                        KeyframeCommand cmd;
                        cmd.type = KeyframeCommand::AUDIO_EDIT;
                        cmd.audioEventIndex = static_cast<size_t>(m_SelectedAudioEventIndex);
                        cmd.oldAudioEvent = originalEvent;
                        cmd.audioEvent = newEvent;
                        ExecuteCommand(cmd);

                        initialized = false;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Delete", ImVec2(120, 0)))
                    {
                        // Use command system for undo/redo support
                        KeyframeCommand cmd;
                        cmd.type = KeyframeCommand::AUDIO_REMOVE;
                        cmd.audioEventIndex = static_cast<size_t>(m_SelectedAudioEventIndex);
                        cmd.audioEvent = audioEvent;  // Store for undo
                        ExecuteCommand(cmd);

                        m_SelectedAudioEventIndex = -1;
                        initialized = false;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    {
                        initialized = false;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }

            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();

    ImGui::EndGroup();
}

// ========== Audio Events Track ==========

void AnimationTimelinePanel::RenderAudioTrack(float duration)
{
    // Skip if no clip selected
    if (m_SelectedClipIndex < 0 || !m_Animator) return;

    const auto* clip = m_Animator->GetClip(m_SelectedClipIndex);
    if (!clip) return;

    // === COLUMN 0: Track Label ===
    ImVec2 rowStartPos = ImGui::GetCursorScreenPos();
    float rowHeight = ImGui::GetTextLineHeightWithSpacing();

    // Audio track label (not a tree node, just text)
    ImGui::Text("Audio Events");

    // Show count of audio events
    if (!clip->audioEvents.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(%zu)", clip->audioEvents.size());
    }

    // === COLUMN 1: Timeline Track ===
    ImGui::NextColumn();

    // Get the timeline area dimensions
    ImVec2 timelineStartPos = ImGui::GetCursorScreenPos();
    float timelineWidth = ImGui::GetColumnWidth(1) - 10.0f; // Leave some padding

    // Adjust vertical position to match the label row
    timelineStartPos.y = rowStartPos.y;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Timeline background - slightly different color to differentiate from bone tracks
    ImVec2 timelineMin = timelineStartPos;
    ImVec2 timelineMax(timelineMin.x + timelineWidth, timelineMin.y + rowHeight);

    // Audio track background (darker purple/blue tint)
    drawList->AddRectFilled(timelineMin, timelineMax, IM_COL32(50, 40, 60, 255));

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

    // Draw and interact with audio event markers
    if (duration > 0.0f && !clip->audioEvents.empty())
    {
        ImVec2 mousePos = ImGui::GetMousePos();
        ImGuiIO& io = ImGui::GetIO();

        for (size_t i = 0; i < clip->audioEvents.size(); ++i)
        {
            const auto& audioEvent = clip->audioEvents[i];

            // Calculate X position based on timestamp
            float normalizedTime = audioEvent.timeStamp / duration;
            float x = timelineMin.x + normalizedTime * timelineWidth;

            // Marker center (vertically centered in row)
            ImVec2 center(x, timelineMin.y + rowHeight * 0.5f);
            float size = 5.0f; // Larger than keyframe diamonds
            float hitTestSize = 8.0f; // Larger hit area for easier clicking

            // Store marker position for click detection
            AudioMarkerScreenPos markerScreenPos;
            markerScreenPos.eventIndex = i;
            markerScreenPos.screenPos = center;
            m_AudioMarkerScreenPositions.push_back(markerScreenPos);

            // Circle shape for audio markers (different from keyframe diamonds)
            bool isSelected = (m_SelectedAudioEventIndex == (int)i);
            bool isHovered = (mousePos.x >= center.x - hitTestSize && mousePos.x <= center.x + hitTestSize &&
                             mousePos.y >= center.y - hitTestSize && mousePos.y <= center.y + hitTestSize);

            if (isHovered)
            {
                m_HoveredAudioEventIndex = (int)i;
            }

            // Determine color based on state
            ImU32 fillColor = IM_COL32(255, 165, 0, 255);  // Orange default
            ImU32 outlineColor = IM_COL32(200, 130, 0, 255);

            if (isSelected)
            {
                // Selected - bright cyan
                fillColor = IM_COL32(0, 255, 255, 255);
                outlineColor = IM_COL32(0, 200, 200, 255);
                size = 6.0f; // Slightly larger when selected
            }
            else if (isHovered)
            {
                // Hovered - bright yellow
                fillColor = IM_COL32(255, 255, 0, 255);
                outlineColor = IM_COL32(200, 200, 0, 255);
                size = 5.5f; // Slightly larger when hovered
            }

            // Color coding by type
            if (audioEvent.is3D)
            {
                // 3D sounds - green tint
                fillColor = IM_COL32(100, 255, 100, 255);
                outlineColor = IM_COL32(50, 200, 50, 255);
            }

            // Draw circle marker
            drawList->AddCircleFilled(center, size, fillColor);
            drawList->AddCircle(center, size, outlineColor, 12, 2.0f);

            // Show event name on hover (tooltip)
            if (isHovered)
            {
                ImGui::BeginTooltip();
                ImGui::Text("Audio Event: %s", audioEvent.eventName.empty() ? "Unnamed" : audioEvent.eventName.c_str());
                ImGui::Text("Time: %.2fs", audioEvent.timeStamp);
                ImGui::Text("Sound: %s", audioEvent.soundFile.c_str());
                ImGui::Text("Volume: %.0f%%", audioEvent.volume * 100.0f);
                ImGui::Text("Type: %s", audioEvent.is3D ? "3D" : "2D");
                ImGui::EndTooltip();
            }

            // Handle selection
            if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_SelectedAudioEventIndex = (int)i;
            }

            // Double-click to edit
            if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_SelectedAudioEventIndex = (int)i;
                ImGui::OpenPopup("EditAudioEventPopup");
            }
        }
    }

    // Right-click on audio track to add new event
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.x >= timelineMin.x && mousePos.x <= timelineMax.x &&
            mousePos.y >= timelineMin.y && mousePos.y <= timelineMax.y)
        {
            // Calculate timestamp based on click position
            float clickX = mousePos.x - timelineMin.x;
            float newTimestamp = (clickX / timelineWidth) * duration;
            newTimestamp = glm::clamp(newTimestamp, 0.0f, duration);

            // Store the timestamp for the popup (uses file-scope static variable)
            s_NewEventTimestamp = newTimestamp;

            ImGui::OpenPopup("AddAudioEventPopup");
        }
    }

    // Back to column 0 for next track
    ImGui::NextColumn();
}

void AnimationTimelinePanel::RenderBoneTrack(const Boom::Joint& joint, float duration)
{
    // === COLUMN 0: Bone Name (with tree hierarchy) ===

    // Tree node flags - SpanAvailWidth prevents text from overflowing column
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                             | ImGuiTreeNodeFlags_OpenOnDoubleClick
                             | ImGuiTreeNodeFlags_SpanAvailWidth;

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

    // Auto-expand if this joint contains the selected bone as a descendant
    // (but not if this IS the selected bone - we only expand parents)
    bool shouldAutoExpand = false;
    if (!m_SelectedBoneName.empty() && joint.name != m_SelectedBoneName && !joint.children.empty())
    {
        // Check if any child (recursively) is the selected bone
        for (const auto& child : joint.children)
        {
            if (JointContainsDescendant(child, m_SelectedBoneName))
            {
                shouldAutoExpand = true;
                break;
            }
        }
    }

    if (shouldAutoExpand)
    {
        ImGui::SetNextItemOpen(true);
    }

    // Store row position for timeline drawing
    ImVec2 rowStartPos = ImGui::GetCursorScreenPos();
    float rowHeight = ImGui::GetTextLineHeightWithSpacing();

    // Draw highlight background for selected bone (more visible than default)
    bool isSelected = (joint.name == m_SelectedBoneName);
    if (isSelected)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float fullRowWidth = ImGui::GetColumnWidth(0) + ImGui::GetColumnWidth(1);
        ImVec2 highlightMin = ImVec2(rowStartPos.x - ImGui::GetCursorPosX(), rowStartPos.y);
        ImVec2 highlightMax = ImVec2(highlightMin.x + fullRowWidth, rowStartPos.y + rowHeight);

        // Bright highlight color (orange/yellow tint)
        ImU32 highlightColor = IM_COL32(255, 180, 50, 60);  // RGBA with alpha
        drawList->AddRectFilled(highlightMin, highlightMax, highlightColor);

        // Add a left border for extra visibility
        ImU32 borderColor = IM_COL32(255, 180, 50, 200);
        drawList->AddLine(
            ImVec2(highlightMin.x, highlightMin.y),
            ImVec2(highlightMin.x, highlightMax.y),
            borderColor, 3.0f);
    }

    // Calculate available width for bone name (accounting for tree indentation)
    float cursorX = ImGui::GetCursorPosX();
    float columnWidth = ImGui::GetColumnWidth(0);
    float availableWidth = columnWidth - cursorX - 5.0f;  // 5px right margin

    // Build full label
    std::string fullLabel = joint.name + " [" + std::to_string(joint.index) + "]";
    std::string displayLabel = fullLabel;

    // Truncate if label is too wide for available space
    if (availableWidth > 40.0f)  // Minimum width to show anything meaningful
    {
        ImVec2 textSize = ImGui::CalcTextSize(fullLabel.c_str());
        if (textSize.x > availableWidth)
        {
            // Truncate bone name and add ellipsis
            std::string truncated = joint.name;
            std::string suffix = "... [" + std::to_string(joint.index) + "]";

            while (!truncated.empty() &&
                   ImGui::CalcTextSize((truncated + suffix).c_str()).x > availableWidth)
            {
                truncated.pop_back();
            }

            if (!truncated.empty())
            {
                displayLabel = truncated + suffix;
            }
            else
            {
                // If even truncated name doesn't fit, just show index
                displayLabel = "..." + std::to_string(joint.index);
            }
        }
    }

    // Use unique ID (joint index) but display potentially truncated label
    ImGui::PushID(joint.index);
    bool nodeOpen = ImGui::TreeNodeEx(displayLabel.c_str(), flags);
    ImGui::PopID();

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

    // Tooltip with full bone info (especially useful for truncated names)
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

    // Timeline background - highlighted if bone is selected
    ImVec2 timelineMin = timelineStartPos;
    ImVec2 timelineMax(timelineMin.x + timelineWidth, timelineMin.y + rowHeight);

    if (isSelected)
    {
        // Highlighted background for selected bone (orange tint over dark)
        drawList->AddRectFilled(timelineMin, timelineMax, IM_COL32(80, 60, 30, 255));
        // Add top/bottom border for emphasis
        drawList->AddLine(timelineMin, ImVec2(timelineMax.x, timelineMin.y), IM_COL32(255, 180, 50, 150), 1.0f);
        drawList->AddLine(ImVec2(timelineMin.x, timelineMax.y), timelineMax, IM_COL32(255, 180, 50, 150), 1.0f);
    }
    else
    {
        // Normal dark gray background
        drawList->AddRectFilled(timelineMin, timelineMax, IM_COL32(40, 40, 40, 255));
    }

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
                ImGuiIO& io = ImGui::GetIO();

                // Track if we're hovering any keyframe on THIS bone (local to this iteration)
                bool hoveredAnyKeyframe = false;

                // Draw diamond at each keyframe timestamp
                for (size_t i = 0; i < keyframes->size(); ++i)
                {
                    const auto& kf = (*keyframes)[i];

                    // Calculate X position based on timestamp
                    float normalizedTime = kf.timeStamp / duration;
                    float x = timelineMin.x + normalizedTime * timelineWidth;

                    // Check if this keyframe is selected (for multiselect)
                    bool isKeyframeSelected = IsKeyframeSelected(joint.name, i);

                    // If we're dragging and this keyframe is selected, apply drag offset
                    if (m_IsDraggingKeyframe && isKeyframeSelected && !m_SelectedKeyframeOriginalTimes.empty())
                    {
                        // Calculate time delta from the dragged keyframe
                        float draggedNewTime = ((mousePos.x - timelineMin.x) / timelineWidth) * duration;
                        draggedNewTime = std::max(0.0f, std::min(draggedNewTime, duration));
                        float timeDelta = draggedNewTime - m_MultiDragStartTime;

                        // Apply delta to this keyframe's original time
                        SelectedKeyframe selKf;
                        selKf.boneName = joint.name;
                        selKf.keyframeIndex = i;
                        auto it = m_SelectedKeyframeOriginalTimes.find(selKf);
                        if (it != m_SelectedKeyframeOriginalTimes.end())
                        {
                            float newTime = it->second + timeDelta;
                            newTime = std::max(0.0f, std::min(newTime, duration));
                            x = timelineMin.x + (newTime / duration) * timelineWidth;
                        }
                    }

                    // Diamond center (vertically centered in row)
                    ImVec2 center(x, timelineMin.y + rowHeight * 0.5f);
                    float size = 3.0f;
                    float hitTestSize = 6.0f; // Larger hit area for easier clicking

                    // Store keyframe position for box selection
                    KeyframeScreenPos kfScreenPos;
                    kfScreenPos.boneName = joint.name;
                    kfScreenPos.keyframeIndex = i;
                    kfScreenPos.screenPos = center;
                    m_KeyframeScreenPositions.push_back(kfScreenPos);

                    // Diamond vertices (rotated square)
                    ImVec2 top(center.x, center.y - size);
                    ImVec2 right(center.x + size, center.y);
                    ImVec2 bottom(center.x, center.y + size);
                    ImVec2 left(center.x - size, center.y);

                    // Check if mouse is hovering over this keyframe
                    bool isHovered = (mousePos.x >= center.x - hitTestSize && mousePos.x <= center.x + hitTestSize &&
                                      mousePos.y >= center.y - hitTestSize && mousePos.y <= center.y + hitTestSize);

                    // Determine color based on state (priority: dragging > selected > hovered > default)
                    ImU32 fillColor = IM_COL32(255, 200, 0, 255);  // Default gold
                    ImU32 outlineColor = IM_COL32(200, 150, 0, 255);

                    if (m_IsDraggingKeyframe && isKeyframeSelected)
                    {
                        // Being dragged (selected keyframes during drag) - bright cyan
                        fillColor = IM_COL32(0, 255, 255, 255);
                        outlineColor = IM_COL32(0, 200, 200, 255);
                        size = 4.0f; // Slightly larger when dragging
                    }
                    else if (isKeyframeSelected)
                    {
                        // Selected - bright blue
                        fillColor = IM_COL32(100, 150, 255, 255);
                        outlineColor = IM_COL32(50, 100, 200, 255);
                        size = 4.0f; // Slightly larger when selected
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

                        // Show tooltip with selection info
                        if (m_SelectedKeyframes.size() > 1 && isKeyframeSelected)
                        {
                            ImGui::SetTooltip("Keyframe at %.2fs\n%zu keyframes selected\nDrag to move all\nDel to delete all\nCtrl+Click to deselect",
                                              kf.timeStamp, m_SelectedKeyframes.size());
                        }
                        else
                        {
                            ImGui::SetTooltip("Keyframe at %.2fs\nClick to select\nCtrl+Click to add to selection\nRight-click to delete", kf.timeStamp);
                        }

                        // Handle left-click for selection and dragging
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            if (io.KeyCtrl)
                            {
                                // Ctrl+Click: Toggle selection
                                ToggleKeyframeSelection(joint.name, i);
                            }
                            else
                            {
                                // Regular click: Select this keyframe
                                if (!isKeyframeSelected)
                                {
                                    // Not selected - select it (and clear others)
                                    SelectKeyframe(joint.name, i, false);
                                }
                                // If already selected, keep selection for potential drag

                                // Start dragging
                                m_IsDraggingKeyframe = true;
                                m_DraggedBoneName = joint.name;
                                m_DraggedKeyframeIndex = i;
                                m_MultiDragStartTime = kf.timeStamp;

                                // Store original times of all selected keyframes for multi-drag
                                m_SelectedKeyframeOriginalTimes.clear();
                                for (const auto& sel : m_SelectedKeyframes)
                                {
                                    auto* selTrack = m_Animator->GetTrackMutable(m_SelectedClipIndex, sel.boneName);
                                    if (selTrack && sel.keyframeIndex < selTrack->size())
                                    {
                                        m_SelectedKeyframeOriginalTimes[sel] = (*selTrack)[sel.keyframeIndex].timeStamp;
                                    }
                                }
                            }
                        }

                        // Delete on right-click (delete all selected if this is selected, otherwise just this one)
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                        {
                            if (isKeyframeSelected && m_SelectedKeyframes.size() > 1)
                            {
                                // Delete all selected keyframes
                                DeleteSelectedKeyframes();
                            }
                            else
                            {
                                // Delete just this keyframe
                                KeyframeCommand cmd;
                                cmd.type = KeyframeCommand::REMOVE;
                                cmd.boneName = joint.name;
                                cmd.keyframeIndex = i;
                                cmd.keyframe = kf;
                                ExecuteCommand(cmd);
                            }
                            break; // Exit loop since we modified the array
                        }
                    }
                }

                // Handle drag release for multi-drag
                if (m_IsDraggingKeyframe && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                {
                    // Calculate time delta from the dragged keyframe
                    float draggedNewTime = ((mousePos.x - timelineMin.x) / timelineWidth) * duration;
                    draggedNewTime = std::max(0.0f, std::min(draggedNewTime, duration));
                    float timeDelta = draggedNewTime - m_MultiDragStartTime;

                    // Only move if there was actual movement
                    if (std::abs(timeDelta) > 0.001f)
                    {
                        // Create a BATCH command for multi-drag (single undo operation)
                        KeyframeCommand batchCmd;
                        batchCmd.type = KeyframeCommand::BATCH;

                        // Track the new times for updating selection after move
                        std::vector<std::pair<std::string, float>> movedKeyframeNewTimes;

                        // Build batch of move commands directly from stored original times
                        for (const auto& [selKf, originalTime] : m_SelectedKeyframeOriginalTimes)
                        {
                            float newTime = originalTime + timeDelta;
                            newTime = std::max(0.0f, std::min(newTime, duration));

                            if (std::abs(originalTime - newTime) > 0.001f)
                            {
                                KeyframeCommand cmd;
                                cmd.type = KeyframeCommand::MOVE;
                                cmd.boneName = selKf.boneName;
                                cmd.oldTime = originalTime;  // Use stored original time
                                cmd.newTime = newTime;

                                batchCmd.batchCommands.push_back(cmd);
                                movedKeyframeNewTimes.push_back({selKf.boneName, newTime});
                            }
                        }

                        // Execute the batch (single undo entry)
                        if (!batchCmd.batchCommands.empty())
                        {
                            ExecuteCommand(batchCmd);

                            // Update selection to reflect new keyframe indices after move
                            // (indices change because tracks are sorted by timestamp)
                            ClearKeyframeSelection();
                            for (const auto& [boneName, newTime] : movedKeyframeNewTimes)
                            {
                                int newIdx = FindKeyframeByTimestamp(boneName, newTime);
                                if (newIdx >= 0)
                                {
                                    SelectKeyframe(boneName, static_cast<size_t>(newIdx), true);
                                }
                            }
                        }
                    }

                    m_IsDraggingKeyframe = false;
                    m_SelectedKeyframeOriginalTimes.clear();
                }

                // NOTE: "Click to add keyframe" feature disabled
                // This is not Unity-like, and without bone manipulation capability,
                // adding keyframes with identity transforms would break animations.
                // TODO: Re-enable once we can capture actual bone transforms from 3D viewport

                // NOTE: Click on empty space is now handled by box selection in RenderTrackList
                // A click without drag creates a zero-size box that clears selection
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

// ========== Keyframe Multiselect Helper Functions ==========

bool AnimationTimelinePanel::IsKeyframeSelected(const std::string& boneName, size_t index) const
{
    SelectedKeyframe kf;
    kf.boneName = boneName;
    kf.keyframeIndex = index;
    return m_SelectedKeyframes.find(kf) != m_SelectedKeyframes.end();
}

void AnimationTimelinePanel::SelectKeyframe(const std::string& boneName, size_t index, bool addToSelection)
{
    if (!addToSelection)
    {
        m_SelectedKeyframes.clear();
    }

    SelectedKeyframe kf;
    kf.boneName = boneName;
    kf.keyframeIndex = index;
    m_SelectedKeyframes.insert(kf);

    // Set as selection anchor for shift-click range selection
    m_SelectionAnchor = kf;
    m_HasSelectionAnchor = true;
}

void AnimationTimelinePanel::DeselectKeyframe(const std::string& boneName, size_t index)
{
    SelectedKeyframe kf;
    kf.boneName = boneName;
    kf.keyframeIndex = index;
    m_SelectedKeyframes.erase(kf);
}

void AnimationTimelinePanel::ToggleKeyframeSelection(const std::string& boneName, size_t index)
{
    SelectedKeyframe kf;
    kf.boneName = boneName;
    kf.keyframeIndex = index;

    auto it = m_SelectedKeyframes.find(kf);
    if (it != m_SelectedKeyframes.end())
    {
        m_SelectedKeyframes.erase(it);
    }
    else
    {
        m_SelectedKeyframes.insert(kf);
        // Update anchor
        m_SelectionAnchor = kf;
        m_HasSelectionAnchor = true;
    }
}

void AnimationTimelinePanel::ClearKeyframeSelection()
{
    m_SelectedKeyframes.clear();
    m_HasSelectionAnchor = false;
}

void AnimationTimelinePanel::DeleteSelectedKeyframes()
{
    if (m_SelectedKeyframes.empty() || !m_Animator || m_SelectedClipIndex < 0)
        return;

    size_t deleteCount = m_SelectedKeyframes.size();

    // Create a BATCH command for multi-delete (single undo operation)
    KeyframeCommand batchCmd;
    batchCmd.type = KeyframeCommand::BATCH;

    // Delete in reverse order (highest index first) to avoid index shifting issues
    // First, group by bone and sort by index descending
    std::map<std::string, std::vector<size_t>> keyframesByBone;
    for (const auto& sel : m_SelectedKeyframes)
    {
        keyframesByBone[sel.boneName].push_back(sel.keyframeIndex);
    }

    // Sort each bone's keyframes in descending order
    for (auto& [boneName, indices] : keyframesByBone)
    {
        std::sort(indices.begin(), indices.end(), std::greater<size_t>());
    }

    // Build batch of remove commands
    for (const auto& [boneName, indices] : keyframesByBone)
    {
        for (size_t idx : indices)
        {
            // Get keyframe data for undo before deleting
            auto* track = m_Animator->GetTrackMutable(m_SelectedClipIndex, boneName);
            if (track && idx < track->size())
            {
                // Create remove command
                KeyframeCommand cmd;
                cmd.type = KeyframeCommand::REMOVE;
                cmd.boneName = boneName;
                cmd.keyframeIndex = idx;
                cmd.keyframe = (*track)[idx];

                // Add to batch
                batchCmd.batchCommands.push_back(cmd);
            }
        }
    }

    // Execute the batch (single undo entry)
    if (!batchCmd.batchCommands.empty())
    {
        ExecuteCommand(batchCmd);
    }

    // Clear selection after deletion
    ClearKeyframeSelection();

    BOOM_INFO("[Multiselect] Deleted {} keyframes", deleteCount);
}
