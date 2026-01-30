#pragma once
#include "Vendors/imgui/imgui.h"
#include <string>

namespace Boom { struct AppContext; struct AppInterface; struct MaterialAsset; }

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

        // Resolves material texture IDs and renders a preview sphere
        // Returns the preview texture ID, or 0 if preview unavailable
        uint32_t GetMaterialPreviewTexture(Boom::MaterialAsset* mat);

    private:
        char const* NEW_MATERIAL_NAME{ "New Material" };

        Editor* m_Owner = nullptr; // non-owning
        Boom::AppInterface* m_App = nullptr;
        Boom::AppContext* m_Ctx = nullptr; // cached from Editor
        ImTextureID       m_Icon = {};

        uint64_t selected{};
        bool showNamePopup{};

        // Fixed camera angles for material thumbnail preview
        static constexpr float PREVIEW_YAW = 0.5f;
        static constexpr float PREVIEW_PITCH = 0.3f;
        static constexpr float PREVIEW_DISTANCE = 1.5f;
    };

} // namespace EditorUI
