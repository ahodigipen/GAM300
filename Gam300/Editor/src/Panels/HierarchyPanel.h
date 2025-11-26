#pragma once
#include <entt/entity/entity.hpp>
#include "Vendors/imgui/imgui.h"

namespace Boom { struct AppContext; struct AppInterface; }

namespace EditorUI {

    class Editor;

    class HierarchyPanel {
    public:
        explicit HierarchyPanel(Editor* owner);

        void Render();

        // Optional wiring from Editor
        void SetShowFlag(bool* flag) { m_ShowHierarchy = flag; }

    private:
        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;   // preferred
        Boom::AppContext* m_Ctx = nullptr;   // cached from m_App->GetContext()

        bool* m_ShowHierarchy = nullptr;

        //camera
        bool isTransitionCam{};
        glm::vec3 startingCamPos{};
        glm::vec3 targetPos{};
        void TransitionCam(glm::vec3& curCamPos);
    };

} // namespace EditorUI
