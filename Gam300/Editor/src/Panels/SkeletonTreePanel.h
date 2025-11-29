#pragma once
#include "Graphics/Models/Animation.h"
#include <string>

namespace Boom {
    struct AppInterface;
}

namespace EditorUI {
    class Editor;

    class SkeletonTreePanel {
    public:
        explicit SkeletonTreePanel(Editor* owner);
        ~SkeletonTreePanel() = default;

        void Render();
        void Show(bool v) { m_ShowPanel = v; }
        bool IsVisible() const { return m_ShowPanel; }

        // Bone selection
        const std::string& GetSelectedBoneName() const { return m_SelectedBoneName; }
        void ClearSelection() { m_SelectedBoneName.clear(); }

    private:
        void RenderJointNode(const Boom::Joint& joint);

        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;
        bool m_ShowPanel = true;
        std::string m_SelectedBoneName;
    };

} // namespace EditorUI
