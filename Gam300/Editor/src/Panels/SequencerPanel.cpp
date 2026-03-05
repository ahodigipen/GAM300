#include "Panels/SequencerPanel.h"
#include "Panels/AnimationTimelinePanel.h"
#include "Editor.h"
#include "Vendors/imgui/imgui.h"

namespace EditorUI {
    SequencerPanel::SequencerPanel(Editor* owner) : m_Owner(owner) {
        m_AnimationTimeline = std::make_unique<AnimationTimelinePanel>(owner);
    }

    SequencerPanel::~SequencerPanel() = default;

    void SequencerPanel::Render() {
        if (!m_Owner || !m_Owner->m_ShowSequencer) return;

        ImGui::SetNextWindowSize(ImVec2(1200, 800), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Sequencer", &m_Owner->m_ShowSequencer, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            m_AnimationTimeline->Render();
        }
        ImGui::End();
    }
}
