#pragma once
#include "Vendors/imgui/imgui.h"
#include <string>

namespace Boom { struct AppContext; struct AppInterface; }

namespace EditorUI {

    class Editor;

    class ResourcePanel {
    public:
        explicit ResourcePanel(Editor* owner);
        void OnShow();                 // Editor.cpp calls this
        void Render() { OnShow(); }    // optional wrapper

    private:
        void CreateEmptyMaterial();
        void HandleConflictName(std::string& name);

    private:
        char const* NEW_MATERIAL_NAME{ "New Material" };

        Editor* m_Owner = nullptr; // non-owning
        Boom::AppInterface* m_App = nullptr;
        Boom::AppContext* m_Ctx = nullptr; // cached from Editor
        ImTextureID       m_Icon = {};

        uint64_t selected{};
        bool showNamePopup{};
    };

} // namespace EditorUI
