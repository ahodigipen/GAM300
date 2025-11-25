#include "Panels/HierarchyPanel.h"

// keep header light; pull real types here
#include "Editor.h"
#include "Application/Interface.h"   // AppInterface (GetContext, etc.)
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "BoomEngine.h"              // InfoComponent (adjust include if needed)

#include <entt/entt.hpp>

namespace EditorUI {

    HierarchyPanel::HierarchyPanel(Editor* owner)
        : m_Owner(owner)
    {
        DEBUG_DLL_BOUNDARY("HierarchyPanel::Constructor");

        if (!m_Owner) { BOOM_ERROR("HierarchyPanel - Null owner!"); return; }

        // FIXED: Since Editor now inherits from AppInterface, cast works
        m_App = static_cast<Boom::AppInterface*>(m_Owner);
        DEBUG_POINTER(m_App, "AppInterface");

        // Get context through the AppInterface
        if (m_App) {
            m_Ctx = owner->GetContext();
            DEBUG_POINTER(m_Ctx, "AppContext");
        }


        
    }

    void HierarchyPanel::Render()
    {
        if (!m_Ctx) return;

        bool open_local = true;
        bool* p_open = m_ShowHierarchy ? m_ShowHierarchy : &open_local;

        if (ImGui::Begin("Hierarchy", p_open))
        {
            ImGui::TextUnformatted("Scene Hierarchy");
            ImGui::Separator();

            // Your AppContext exposes 'scene'
            auto& registry = m_Ctx->scene;

            auto view = registry.view<Boom::InfoComponent>();
            for (entt::entity e : view)
            {
                const auto& info = view.get<Boom::InfoComponent>(e);
                const bool isSelected = (m_App->SelectedEntity() == e);

                ImGui::PushID(static_cast<int>(entt::to_integral(e)));
                if (ImGui::Selectable(info.name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    m_App->SelectedEntity(true) = e;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        static TransformComponent* camTPtr{nullptr};
                        auto entView = m_App->GetEntityRegistry().view<CameraComponent, TransformComponent>();
                        for (auto ent : entView) {
                            auto camPtr = &entView.get<CameraComponent>(ent);
                            if (camPtr) {
                                camTPtr = &entView.get<TransformComponent>(ent);
                            }
                        }
                        Boom::Entity selected{ &m_Ctx->scene, m_App->SelectedEntity() };
                        

                        Transform3D const& tRef = selected.Get<TransformComponent>().transform;
                        camTPtr->transform.translate = tRef.translate;
                        camTPtr->transform.translate += 2.f;
                        glm::vec3 forward = glm::normalize(camTPtr->transform.translate - tRef.translate);
                        float yaw = std::atan2(forward.x, forward.z);
                        float pitch = std::asin(-forward.y);
                        camTPtr->transform.rotate = { glm::degrees(pitch), glm::degrees(yaw), 0.f };
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::End();
    }

} // namespace EditorUI