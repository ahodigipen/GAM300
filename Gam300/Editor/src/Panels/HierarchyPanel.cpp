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

        static TransformComponent* camTPtr{ nullptr };

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
                    //set starting, target position and begin transition boolean
                    m_App->SelectedEntity(true) = e;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        auto entView = m_App->GetEntityRegistry().view<CameraComponent, TransformComponent>();
                        for (auto ent : entView) {
                            auto camPtr = &entView.get<CameraComponent>(ent);
                            if (camPtr) {
                                camTPtr = &entView.get<TransformComponent>(ent);
                                startingCamPos = camTPtr->transform.translate;
                                break; //only get first camera
                            }
                        }
                        targetPos = Boom::Entity{ &m_Ctx->scene, m_App->SelectedEntity() }.Get<TransformComponent>().transform.translate;

                        curTime = 0.f;
                        isTransitionCam = true;
                    }
                }
                ImGui::PopID();
            }

        }
        ImGui::End();

        if (isTransitionCam) TransitionCam(camTPtr->transform.translate);
    }


    void HierarchyPanel::TransitionCam(glm::vec3& curCamPos) {
        //init constants
        const float transtitionTime{ 0.5f };
        const float dt{ (float)m_App->GetDeltaTime() };


        //call glm::slerp here to transistion camera nicely
        //make sure that camera can see whole of object depending on its scale.

        if (curCamPos == targetPos) {
            isTransitionCam = false;
        }
    }

} // namespace EditorUI