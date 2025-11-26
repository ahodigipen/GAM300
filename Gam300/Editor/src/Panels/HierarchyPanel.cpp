#include "Panels/HierarchyPanel.h"

// keep header light; pull real types here
#include "Editor.h"
#include "Application/Interface.h"   // AppInterface (GetContext, etc.)
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "BoomEngine.h"              // InfoComponent (adjust include if needed)

#include <entt/entt.hpp>

namespace {
    glm::vec3 EulerToDirection(glm::vec3 eulerDegrees)
    {
        // Create rotation matrix from Euler angles (yaw-pitch-roll order = Y * X * Z)
        glm::mat4 rotation = glm::eulerAngleYXZ(
            glm::radians(eulerDegrees.y),  // yaw   (around Y)
            glm::radians(eulerDegrees.x),  // pitch (around X)
            glm::radians(eulerDegrees.z)   // roll  (around Z)
        );

        // Forward direction in GLM is +Z by default, but most games use -Z or +X
        // Here we assume typical FPS/game convention: forward = +X axis in local space
        glm::vec3 forward = glm::vec3(rotation[0]); // X axis of the matrix = right
        glm::vec3 up = glm::vec3(rotation[1]); // Y axis = up
        glm::vec3 right = glm::vec3(rotation[2]); // Z axis = forward? Wait!

        // Better: define local +X as forward (common in games)
        glm::vec3 localForward(0.0f, 0.0f, -1.0f);
        return glm::normalize(glm::vec3(rotation * glm::vec4(localForward, 0.0f)));
    }
}

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

    TransformComponent* StartTransitionCam(Boom::AppInterface* m_App, Boom::AppContext* m_Ctx, float& curTime, bool& isTransitionCam, glm::vec3& startingCamPos, glm::vec3& targetPos) {
        if (!ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) return nullptr;

        //set starting, target position and begin transition boolean
        TransformComponent* camTPtr{nullptr};
        auto entView = m_App->GetEntityRegistry().view<CameraComponent, TransformComponent>();
        CameraComponent* camPtr{};
        for (auto ent : entView) {
            camPtr = &entView.get<CameraComponent>(ent);
            if (camPtr) {
                camTPtr = &entView.get<TransformComponent>(ent);
                startingCamPos = camTPtr->transform.translate;
            }
        }

        Transform3D const& t{ Boom::Entity{ &m_Ctx->scene, m_App->SelectedEntity() }.Get<TransformComponent>().transform };
        targetPos = t.translate;

        //offset camera to encompass whole object in view
        targetPos.y += .25f;
        targetPos -= EulerToDirection(camTPtr->transform.rotate) * glm::max(t.scale.x, t.scale.y, t.scale.z);

        curTime = 0.f;
        isTransitionCam = true;

        return camTPtr;
    }

    void TransitionCam(glm::vec3& curCamPos, Boom::AppInterface* m_App, float& curTime, bool& isTransitionCam, glm::vec3& startingCamPos, glm::vec3& targetPos) {
        //init constants
        const float transtitionTime{ 0.2f };
        const float dt{ (float)m_App->GetDeltaTime() };

        //call glm::slerp here to transistion camera nicely
        curTime += dt;
        float t = glm::smoothstep(0.f, 1.f, curTime / transtitionTime);
        if (t >= 1.f) {
            t = 1.f;
            isTransitionCam = false;
        }

        curCamPos = glm::lerp(startingCamPos, targetPos, t);
    }


    void HierarchyPanel::Render()
    {
        if (!m_Ctx) return;

        bool open_local = true;
        bool* p_open = m_ShowHierarchy ? m_ShowHierarchy : &open_local;

        static TransformComponent* camTPtr{ nullptr };

        static glm::vec3 startingCamPos{};
        static glm::vec3 targetPos{};

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
                    camTPtr = StartTransitionCam(m_App, m_Ctx, curTime, isTransitionCam, startingCamPos, targetPos);
                }
                ImGui::PopID();
            }

        }
        ImGui::End();

        if (isTransitionCam) TransitionCam(camTPtr->transform.translate, m_App, curTime, isTransitionCam, startingCamPos, targetPos);
    }

    

} // namespace EditorUI