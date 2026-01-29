#include "Panels/CutsceneSequencerPanel.h"
#include "Editor.h"
#include "Context/DebugHelpers.h"
#include "ECS/ECS.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include "Graphics/Models/Animator.h"

namespace EditorUI
{
    static const char* SequencerItemTypeNames[] = { "Position", "Rotation", "Scale", "Animation Slot" };

    CutsceneSequencerPanel::CutsceneSequencerPanel(Editor* owner)
        : m_Owner(owner)
    {
        m_FrameMin = 0;
        m_FrameMax = 600; // 10 seconds at 60fps
    }

    const char* CutsceneSequencerPanel::GetItemLabel(int index) const
    {
        if (index >= 0 && index < m_Tracks.size())
            return m_Tracks[index].label.c_str();
        return "";
    }

    const char* CutsceneSequencerPanel::GetItemTypeName(int typeIndex) const
    {
        // FIX: Return valid name for each type
        if (typeIndex >= 0 && typeIndex < 4)
            return SequencerItemTypeNames[typeIndex];
        return "Unknown";
    }

    void CutsceneSequencerPanel::Get(int index, int** start, int** end, int* type, unsigned int* color)
    {
        if (index >= 0 && index < m_Tracks.size())
        {
            auto& track = m_Tracks[index];
            if (track.keyFrameTimes.empty())
            {
                // Fix: Return pointer to dummy if empty to avoid any null deref issues in ImSequencer
                static int dummy = 0;
                if (start) *start = &dummy;
                if (end) *end = &dummy;
            }
            else
            {
                if (start) *start = track.keyFrameTimes.data();
                if (end) *end = track.keyFrameTimes.data() + track.keyFrameTimes.size();
            }
            
            if (type) *type = track.type;
            
            if (color)
            {
                // Assign colors based on type for visual distinction
                switch (track.type) {
                    case 0: *color = 0xFFAA8080; break; // Pos - Reddish
                    case 1: *color = 0xFF80AA80; break; // Rot - Greenish
                    case 2: *color = 0xFF8080AA; break; // Scale - Blueish
                    case 3: *color = 0xFFAAAA55; break; // Anim - Yellow/Gold
                    default: *color = 0xFFCCCCCC; break;
                }
            }
        }
    }

    void CutsceneSequencerPanel::Add(int type)
    {
        // FIX: Implemented to use the currently selected entity
        if (m_Owner)
        {
            entt::entity selected = m_Owner->SelectedEntity();
            if (m_Owner->GetContext()->scene.valid(selected))
            {
                 // Get Name
                 std::string name = "Entity";
                 if (m_Owner->GetContext()->scene.any_of<Boom::InfoComponent>(selected)) {
                     name = m_Owner->GetContext()->scene.get<Boom::InfoComponent>(selected).name;
                 }
                 AddTrack(name, type);
            }
            else
            {
                 BOOM_WARN("No entity selected! Please select an entity to add a track.");
            }
        }
    }

    void CutsceneSequencerPanel::Del(int index)
    {
        if (index >= 0 && index < m_Tracks.size())
        {
            m_Tracks.erase(m_Tracks.begin() + index);
        }
    }

    void CutsceneSequencerPanel::Duplicate(int index)
    {
        // Optional
    }

    void CutsceneSequencerPanel::DoubleClick(int index)
    {
        if (index >= 0 && index < m_Tracks.size())
        {
            auto& track = m_Tracks[index];
            m_SelectedTrack = index; // FIX: Ensure we select the track on double click
            
            // 1. Capture current values from entity FIRST
            float vX = 0.0f, vY = 0.0f, vZ = 0.0f;
            float vW = 0.0f;
            // Safe defaults for Scale
            if (track.type == 2) { vX = 1.0f; vY = 1.0f; vZ = 1.0f; }

            bool captured = false;
            if (m_Owner && m_Owner->GetContext())
            {
                auto& reg = m_Owner->GetContext()->scene;
                entt::entity e = Boom::FindEntityByName(reg, track.entityName);
                if (reg.valid(e) && reg.all_of<Boom::TransformComponent>(e))
                {
                    const auto& tc = reg.get<Boom::TransformComponent>(e);
                    if (track.type == 0) { // Position
                        vX = tc.transform.translate.x; vY = tc.transform.translate.y; vZ = tc.transform.translate.z;
                    }
                    else if (track.type == 1) { // Rotation
                        vX = tc.transform.rotate.x; vY = tc.transform.rotate.y; vZ = tc.transform.rotate.z;
                    }
                    else if (track.type == 2) { // Scale
                        vX = tc.transform.scale.x; vY = tc.transform.scale.y; vZ = tc.transform.scale.z;
                    }
                    captured = true;
                }
                else {
                    // Only warn if we really expected to find it
                    BOOM_WARN("Could not find entity '{}' to capture!", track.entityName);
                }
            }

            // 2. Check for existing keyframe to UPDATE
            bool found = false;
            for (auto& kf : track.keyFrames) {
                if (kf.frame == m_CurrentFrame) {
                    // Update existing
                    if (captured) {
                        kf.valueX = vX; kf.valueY = vY; kf.valueZ = vZ;
                        BOOM_INFO("Keyframe UPDATED [{}]: ({:.2f}, {:.2f}, {:.2f})", m_CurrentFrame, vX, vY, vZ);
                    }
                    ImGui::OpenPopup("Edit Keyframe");
                    found = true;
                    break;
                }
            }

            // 3. If not found, ADD new
            if (!found) {
                track.keyFrameTimes.push_back(m_CurrentFrame);
                if (captured) {
                     BOOM_INFO("Keyframe ADD [{}]: ({:.2f}, {:.2f}, {:.2f})", m_CurrentFrame, vX, vY, vZ);
                }
                track.keyFrames.push_back({ m_CurrentFrame, vX, vY, vZ, vW });

                std::sort(track.keyFrameTimes.begin(), track.keyFrameTimes.end());
                std::sort(track.keyFrames.begin(), track.keyFrames.end(), [](const auto& a, const auto& b) { return a.frame < b.frame; });
                
                // FIX: Open popup immediately on creation
                ImGui::OpenPopup("Edit Keyframe");
            }
        }
    }

    void CutsceneSequencerPanel::CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect)
    {
        // We could draw property curves here later
    }

    void CutsceneSequencerPanel::Render()
    {
        if (!m_Owner) return;

        // Process Deferred Actions
        if (!m_DeferredTracks.empty())
        {
            BOOM_INFO("Processing {} deferred tracks...", m_DeferredTracks.size());
            for (const auto& dt : m_DeferredTracks)
            {
                SequenceTrack newTrack;
                newTrack.entityName = dt.entityName;
                newTrack.type = dt.type;
                newTrack.label = dt.entityName + " : " + SequencerItemTypeNames[dt.type];
                
                BOOM_INFO("Adding track: {}", newTrack.label);
                m_Tracks.push_back(newTrack);
            }
            m_DeferredTracks.clear();
        }

        bool open = m_Owner->m_ShowCutsceneSequencer;
        if (!ImGui::Begin("Cutscene Sequencer", &m_Owner->m_ShowCutsceneSequencer))
        {
            ImGui::End();
            return;
        }

        RenderMenuBar();
        
        // Playback Logic
        bool isPlayingFrameUpdate = false;
        if (m_IsPlaying)
        {
            m_TimeAccumulator += ImGui::GetIO().DeltaTime;
            float frameTime = 1.0f / 60.0f;
            while (m_TimeAccumulator >= frameTime)
            {
                m_CurrentFrame++;
                m_TimeAccumulator -= frameTime;
                isPlayingFrameUpdate = true;
                
                if (m_CurrentFrame > m_FrameMax) {
                    m_CurrentFrame = m_FrameMin; 
                }
            }
        }

        RenderTransportControls();

        // Ensure we fit inside window
        int sequencerFlags = ImSequencer::SEQUENCER_EDIT_STARTEND | ImSequencer::SEQUENCER_ADD | ImSequencer::SEQUENCER_DEL | ImSequencer::SEQUENCER_COPYPASTE | ImSequencer::SEQUENCER_CHANGE_FRAME;
        int changes = ImSequencer::Sequencer(this, &m_CurrentFrame, &m_Expanded, &m_SelectedTrack, &m_FrameMin, sequencerFlags);
        
        bool manualScrub = (changes & ImSequencer::SEQUENCER_CHANGE_FRAME);

        // Apply Preview IF Playing OR Scrubbing (Change Frame)
        if (m_PreviewIndex && (isPlayingFrameUpdate || manualScrub))
        {
            ApplyFrame(m_CurrentFrame);
        }

        // Edit Keyframe Popup (Model)
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Edit Keyframe", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (m_SelectedTrack >= 0 && m_SelectedTrack < m_Tracks.size())
            {
                auto& track = m_Tracks[m_SelectedTrack];
                SerializedKeyframe* kf_data = nullptr;
                for(auto& kf : track.keyFrames) {
                    if (kf.frame == m_CurrentFrame) {
                        kf_data = &kf;
                        break;
                    }
                }
                
                if (kf_data) {
                    ImGui::Text("%s", track.label.c_str());
                    ImGui::Text("Frame: %d (%.2fs)", m_CurrentFrame, m_CurrentFrame / 60.0f);
                    ImGui::Separator();
                    
                    if (track.type <= 2) { // Pos/Rot/Scale
                        ImGui::DragFloat("X", &kf_data->valueX, 0.1f);
                        ImGui::DragFloat("Y", &kf_data->valueY, 0.1f);
                        ImGui::DragFloat("Z", &kf_data->valueZ, 0.1f);
                    }


                    else if (track.type == 3) // Animation
                    {
                        // 1. Collect available animation names
                        std::vector<std::string> animNames;
                        animNames.push_back("None");

                        if (m_Owner && m_Owner->GetContext())
                        {
                            auto& reg = m_Owner->GetContext()->scene;
                            entt::entity e = Boom::FindEntityByName(reg, track.entityName);
                            if (reg.valid(e) && reg.all_of<Boom::AnimatorComponent>(e))
                            {
                                 const auto& ac = reg.get<Boom::AnimatorComponent>(e);
                                 if (ac.animator)
                                 {
                                     for(size_t i=0; i < ac.animator->GetClipCount(); ++i)
                                     {
                                         const auto* clip = ac.animator->GetClip(i);
                                         if(clip) animNames.push_back(clip->name);
                                     }
                                 }
                            }
                        }

                        // 2. Combo Box
                        const char* preview = kf_data->valueStr.empty() ? "None" : kf_data->valueStr.c_str();
                        if (ImGui::BeginCombo("Animation Name", preview))
                        {
                            for (const auto& name : animNames)
                            {
                                bool is_selected = (kf_data->valueStr == name);
                                if (ImGui::Selectable(name.c_str(), is_selected))
                                {
                                    kf_data->valueStr = name;
                                }
                                if (is_selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        RenderBottomBar();

        ImGui::End();
    }

    void CutsceneSequencerPanel::RenderMenuBar()
    {
        if (ImGui::Button("Save Sequence"))
        {
             SaveSequence("Resources/Cutscenes/Test.seq");
             ImGui::OpenPopup("SaveConfirm");
        }
        
        if (ImGui::BeginPopup("SaveConfirm"))
        {
            ImGui::Text("Sequence Saved Successfully!");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Sequence"))
        {
             LoadSequence("Resources/Cutscenes/Test.seq");
        }
    }

    void CutsceneSequencerPanel::RenderTransportControls()
    {
        ImGui::Separator();
        
        // Play/Pause
        if (m_IsPlaying) {
            if (ImGui::Button("Stop")) m_IsPlaying = false;
        } else {
            if (ImGui::Button("Play")) m_IsPlaying = true;
        }

        ImGui::SameLine();
        // Simple Transport UI
        if (ImGui::Button("|<")) m_CurrentFrame = m_FrameMin;
        ImGui::SameLine();
        if (ImGui::Button("<")) m_CurrentFrame--;
        ImGui::SameLine();
        if (ImGui::Button(">")) m_CurrentFrame++;
        ImGui::SameLine();
        if (ImGui::Button(">|")) m_CurrentFrame = m_FrameMax;
        
        ImGui::SameLine();
        
        // Allow typing the frame number directly (easier than dragging)
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputInt("Frame", &m_CurrentFrame)) {
            // Clamp if typed manually
            if (m_CurrentFrame < m_FrameMin) m_CurrentFrame = m_FrameMin;
            if (m_CurrentFrame > m_FrameMax) m_CurrentFrame = m_FrameMax;
        }
        ImGui::SameLine();
        ImGui::Text("(%.2fs)", m_CurrentFrame / 60.0f);
        
        ImGui::SameLine();
        ImGui::Checkbox("Preview", &m_PreviewIndex);

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // New Explicit Controls
        if (ImGui::Button("[+] Snapshot Key"))
        {
            if (m_SelectedTrack >= 0 && m_SelectedTrack < m_Tracks.size()) {
                DoubleClick(m_SelectedTrack); // Re-uses the robust capture logic
                BOOM_INFO("Snapshot taken for track {}", m_Tracks[m_SelectedTrack].label);
            } else {
                BOOM_WARN("Please select a track (click its name) to add a keyframe.");
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save the entity's CURRENT transform to the CURRENT frame.");

        ImGui::SameLine();
        if (ImGui::Button("[?] Debug"))
        {
            BOOM_INFO("--- SEQUENCER DEBUG DUMP ---");
            BOOM_INFO("Total Tracks: {}", m_Tracks.size());
            for (size_t i = 0; i < m_Tracks.size(); i++) {
                const auto& t = m_Tracks[i];
                BOOM_INFO("Track {}: '{}' (Entity: {})", i, t.label, t.entityName);
                BOOM_INFO("  Keyframes: {}", t.keyFrames.size());
                for (const auto& kf : t.keyFrames) {
                    BOOM_INFO("    Frame {}: ({:.2f}, {:.2f}, {:.2f})", kf.frame, kf.valueX, kf.valueY, kf.valueZ);
                }
            }
            BOOM_INFO("------------------------------");
        }

        ImGui::Separator();
    }
    
    void CutsceneSequencerPanel::RenderBottomBar()
    {
        ImGui::Separator();
        
        // UNITY-STYLE: "Add Track" uses selection
        // FIX: Access Editor's selection, not Application's
        bool hasSelection = (m_Owner->SelectedEntity() != entt::null);
        
        if (hasSelection)
        {
             // Get Entity Name
             auto entityID = m_Owner->SelectedEntity();
             auto& scene = m_Owner->GetContext()->scene;
             std::string name = "Entity";
             if (scene.valid(entityID) && scene.any_of<Boom::InfoComponent>(entityID)) {
                  name = scene.get<Boom::InfoComponent>(entityID).name;
             }

             if (ImGui::Button(("Add Track for '" + name + "'").c_str()))
             {
                 ImGui::OpenPopup("AddTrackPropPopup");
                 m_PendingEntityName = name;
             }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("Select an Entity to Add Track");
            ImGui::EndDisabled();
        }

        if (ImGui::BeginPopup("AddTrackPropPopup"))
        {
            ImGui::Text("Property:");
            ImGui::Separator();
            // Only allow Position (0), Rotation (1), Scale (2)
            for (int i = 0; i < 4; i++)
            {
                if (ImGui::Selectable(SequencerItemTypeNames[i]))
                {
                    AddTrack(m_PendingEntityName, i);
                }
            }
            ImGui::EndPopup();
        }
    }
    
    void CutsceneSequencerPanel::ApplyFrame(int frame)
    {
        if (!m_Owner || !m_Owner->GetContext()) return;
        auto& reg = m_Owner->GetContext()->scene;

        for (const auto& track : m_Tracks)
        {
            if (track.keyFrames.size() < 2) 
            {
                // Warn user if trying to play with insufficient data
                static int kfWarn = 0;
                if (frame % 60 == 0 && kfWarn++ % 30 == 0) // Throttle
                    BOOM_WARN("Track '{}' has only {} Keyframe(s). Need at least 2 to animate.", track.entityName, track.keyFrames.size());
                continue;
            }

            // 1. Find Entity
            entt::entity e = Boom::FindEntityByName(reg, track.entityName);
            if (!reg.valid(e)) {
                // Throttle warning
                static int warnTimer = 0;
                if (warnTimer++ % 120 == 0) BOOM_WARN("ApplyFrame: Entity '{}' not found!", track.entityName);
                continue;
            }
            if (!reg.all_of<Boom::TransformComponent>(e)) continue;

            auto& tc = reg.get<Boom::TransformComponent>(e);

            // 2. Find Keyframes
            const SerializedKeyframe* k1 = nullptr;
            const SerializedKeyframe* k2 = nullptr;

            // Simple search
            for (size_t i = 0; i < track.keyFrames.size() - 1; i++)
            {
                 if (frame >= track.keyFrames[i].frame && frame <= track.keyFrames[i+1].frame)
                 {
                     k1 = &track.keyFrames[i];
                     k2 = &track.keyFrames[i+1];
                     break;
                 }
            }

            if (k1 && k2)
            {
                // Animation Track (Type 3) - Trigger on Keyframe
                if (track.type == 3) // Animation
                {
                     // Find active keyframe (step function)
                     const SerializedKeyframe* activeKF = nullptr;
                     for(auto& kf : track.keyFrames) {
                         if (frame >= kf.frame) activeKF = &kf;
                         else break;
                     }
                     
                     if (activeKF && !activeKF->valueStr.empty() && activeKF->valueStr != "None")
                     {
                         if (reg.all_of<Boom::AnimatorComponent>(e))
                         {
                             auto& ac = reg.get<Boom::AnimatorComponent>(e);
                             if (ac.animator) {
                                  // Ensure we are playing the right clip
                                  std::string clipName = activeKF->valueStr;
                                  
                                  // Find the clip index
                                  int clipIndex = -1;
                                  for (size_t i = 0; i < ac.animator->GetClipCount(); ++i) {
                                      const auto* c = ac.animator->GetClip(i);
                                      if (c && c->name == clipName) {
                                          clipIndex = (int)i;
                                          break;
                                      }
                                  }

                                  if (clipIndex != -1)
                                  {
                                      // FIX: Disable State Machine to force manual clip playback
                                      ac.animator->SetStateMachineEnabled(false);
                                      
                                      // If switched, play it
                                      if (ac.animator->GetCurrentClip() != clipIndex) {
                                          ac.animator->PlayClip(clipIndex);
                                      }
                                      
                                      // SYNC TIME precisely for scrubbing
                                      // Frame difference / 60.0f = seconds elapsed
                                      float timeInSeconds = (float)(frame - activeKF->frame) / 60.0f;
                                      
                                      // Animator uses TICKS, not Seconds
                                      // Time (Ticks) = Seconds * TicksPerSecond
                                      const auto* clip = ac.animator->GetClip(clipIndex);
                                      float tps = clip ? clip->ticksPerSecond : 25.0f;
                                      if (tps <= 0.0f) tps = 25.0f; 

                                      ac.animator->SetTime(timeInSeconds * tps);
                                      ac.animator->UpdateJointsFromCurrentTime();
                                  }
                             }
                         }
                     }
                     // Continue to next track (don't do transform lerp)
                     continue; 
                }

                float range = (float)(k2->frame - k1->frame);
                float t = 0.0f;
                if (range > 0.0001f) t = (frame - k1->frame) / range;
                
                // Clamp
                if (t < 0.0f) t = 0.0f;
                if (t > 1.0f) t = 1.0f;

                float valX = k1->valueX + (k2->valueX - k1->valueX) * t;
                float valY = k1->valueY + (k2->valueY - k1->valueY) * t;
                float valZ = k1->valueZ + (k2->valueZ - k1->valueZ) * t;

                if (track.type == 0) // Position
                {
                    tc.transform.translate = { valX, valY, valZ };
                }
                else if (track.type == 1) // Rotation
                {
                    tc.transform.rotate = { valX, valY, valZ };
                }
                else if (track.type == 2) // Scale
                {
                    tc.transform.scale = { valX, valY, valZ };
                }
            }
        }
    }

    void CutsceneSequencerPanel::AddTrack(const std::string& entityName, int propertyType)
    {
        BOOM_INFO("Queueing track add: {} type {}", entityName, propertyType);
        // Fix: Defer addition to start of next frame
        m_DeferredTracks.push_back({ entityName, propertyType });
    }

    void CutsceneSequencerPanel::SaveSequence(const std::string& path)
    {
        std::filesystem::path fsPath(path);
        if (fsPath.has_parent_path()) {
            std::filesystem::create_directories(fsPath.parent_path());
        }

        std::ofstream out(path);
        if (!out.is_open()) {
            BOOM_ERROR("Failed to save cutscene: {}", path);
            return;
        }

        out << "DURATION " << m_FrameMax << "\n";
        for (const auto& track : m_Tracks)
        {
            // Use quotes to handle spaces safely
            out << "TRACK \"" << track.entityName << "\" " << track.type << "\n";
            for (const auto& kf : track.keyFrames)
            {
                out << "KEY " << kf.frame << " " << kf.valueX << " " << kf.valueY << " " << kf.valueZ << " " << kf.valueW;
                if (track.type == 3 && !kf.valueStr.empty()) {
                    out << " \"" << kf.valueStr << "\"";
                }
                out << "\n";
            }
        }
        out.close();
        BOOM_INFO("Saved cutscene to {}", path);
    }

    void CutsceneSequencerPanel::LoadSequence(const std::string& path)
    {
        std::ifstream in(path);
        if (!in.is_open()) {
            BOOM_ERROR("Failed to load cutscene: {}", path);
            return;
        }

        m_Tracks.clear();
        std::string line, token;
        SequenceTrack* currentTrack = nullptr;

        while (std::getline(in, line))
        {
            if (line.empty()) continue;
            std::stringstream ss(line);
            ss >> token;

            if (token == "DURATION")
            {
                ss >> m_FrameMax;
            }
            else if (token == "TRACK")
            {
                // Format: TRACK "EntityName" Type
                size_t firstQuote = line.find('"');
                size_t lastQuote = line.rfind('"');
                if (firstQuote != std::string::npos && lastQuote != std::string::npos)
                {
                     std::string entityName = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                     std::string typeStr = line.substr(lastQuote + 2);
                     int type = std::stoi(typeStr);
                     
                     // LoadSequence needs immediate addition, not deferred
                     SequenceTrack newTrack;
                     newTrack.entityName = entityName;
                     newTrack.type = type;
                     newTrack.label = entityName + " : " + SequencerItemTypeNames[type];
                     
                     m_Tracks.push_back(newTrack);
                     currentTrack = &m_Tracks.back(); // Now safe
                     
                     // AddTrack(entityName, type); // deferred, causes crash here
                     // currentTrack = &m_Tracks.back();
                }
            }
            else if (token == "KEY" && currentTrack)
            {
                SerializedKeyframe kf;
                ss >> kf.frame >> kf.valueX >> kf.valueY >> kf.valueZ >> kf.valueW;
                
                // Try reading string if present
                if (currentTrack->type == 3)
                {
                    // Rest of line might be "AnimName"
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"');
                    size_t q2 = rest.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                        kf.valueStr = rest.substr(q1 + 1, q2 - q1 - 1);
                    }
                }

                currentTrack->keyFrames.push_back(kf);
                currentTrack->keyFrameTimes.push_back(kf.frame);
            }
        }
        in.close();
        BOOM_INFO("Loaded cutscene from {}", path);
    }
}
