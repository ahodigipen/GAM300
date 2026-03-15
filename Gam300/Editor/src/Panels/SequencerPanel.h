#pragma once
#include <memory>

namespace EditorUI {
    class Editor;
    class AnimationTimelinePanel;

    class SequencerPanel {
    public:
        explicit SequencerPanel(Editor* owner);
        ~SequencerPanel();

        void Render();

    private:
        Editor* m_Owner = nullptr;
        std::unique_ptr<AnimationTimelinePanel> m_AnimationTimeline;
    };
}
