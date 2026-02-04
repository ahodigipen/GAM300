#pragma once
#pragma once
#include <functional>
#include <string>
#include <entt/entt.hpp>
#include "Vendors/imgui/imgui.h"
#include "GlobalConstants.h"
#include "ECS/ECS.hpp"
#include "Audio/TrackLibrary.h"
#include "AI/NavAgent.h"
namespace Boom { struct AppContext; struct AppInterface; }

namespace EditorUI {

    class Editor;  // forward declare the owner (your Editor class)

    class InspectorPanel
    {
    public:
        // Preferred: construct with the Editor owner; get context via owner->GetContext()
        explicit InspectorPanel(Editor* owner, bool* showFlag = nullptr);

        // Render entry point
        void Render();

        // Accessors
        Boom::AppContext* GetContext() const;   // implemented in .cpp using m_Owner->GetContext()
        Editor* GetOwner() const { return m_Owner; }
        void SetShowFlag(bool* flag) { m_ShowInspector = flag; }
    
    private:
        // helpers
        void EntityUpdate();
        void AssetUpdate();
        void DeleteUpdate();
        void ComponentSelector(Boom::Entity& selected);
        void SnapEntity(Boom::Entity& entity, glm::vec3 direction);
        template <class Type> void UpdateComponent(Boom::ComponentID id, Boom::Entity& selected);
        //to be placed right below collapsing header that has flag: ImGuiTreeNodeFlags_AllowItemOverlap
        template <class CType> bool ComponentSettings(Boom::AppContext* ctx);

        void AcceptIDDrop(uint64_t& data, char const* payloadType);
        template <std::string_view const& Payload>
        void InputAssetWidget(char const* label, uint64_t& data);

		void AnimatorComponentUI(Boom::Entity& selected);
		void SoundComponentUI(Boom::Entity& selected);
        void GetEntityAABB(Boom::Entity& entity, glm::vec3& outMin, glm::vec3& outMax);
        void GetEntityAABBForSnap(Boom::AppContext* ctx, entt::entity entity, glm::vec3& outMin, glm::vec3& outMax);
        bool RayAABBIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
            const glm::vec3& aabbMin, const glm::vec3& aabbMax, float& t);
        glm::vec3 CalculateAABBHitNormal(const glm::vec3& hitPoint, const glm::vec3& aabbMin, const glm::vec3& aabbMax);
		void VideoComponentUI(Boom::Entity& selected);

    private:
        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;
        Boom::AppContext* ctx = nullptr;
        bool* m_ShowInspector = nullptr;
        bool showDeletePopup{};
        bool           m_HasSelection{ false };
        char           m_NameBuffer[128]{};

        // Animator state editing
        int            m_EditingStateIndex = -1;
        bool m_OpenEditStatePopup = false;
        char           m_StateNameBuffer[128]{};

        // Animator parameters
        char           m_NewParamNameBuffer[128]{};
        int            m_NewParamType = 0; // 0=Float, 1=Bool, 2=Trigger

        // Animator transitions
        int            m_EditingTransitionStateIndex = -1;
        int            m_EditingTransitionIndex = -1;
        bool           m_OpenEditTransitionPopup = false;
        Boom::Animator::Transition m_TempTransition;
        char           m_TransitionParamNameBuffer[128]{};

        // Sprite component undo tracking
        bool           m_IsSpriteBeingEdited = false;
        Boom::SpriteComponent m_SpriteBeforeEdit;

        // Transform component undo tracking
        bool           m_IsTransformBeingEdited = false;
        Boom::Transform3D m_TransformBeforeEdit;

        // Asset picker popup state
        bool           m_AssetPickerOpen = false;
        std::string    m_AssetPickerLabel;
        char           m_AssetPickerSearch[128]{};
        uint64_t*      m_AssetPickerTarget = nullptr;
        std::string    m_AssetPickerPayload;

        // Asset picker icon textures
        ImTextureID    m_AssetIcon = {};
        ImTextureID    m_ModelIcon = {};
        ImTextureID    m_MaterialIcon = {};
        ImTextureID    m_ScriptIcon = {};

        // Material preview camera state
        float          m_MatPreviewYaw = 0.0f;
        float          m_MatPreviewPitch = 0.3f;
        float          m_MatPreviewDistance = 1.5f;

        // Grid Snap settings
        bool           m_UseGridSnap = false;
        glm::vec3      m_GridSnapValues = glm::vec3(1.0f, 15.0f, 0.5f); // Pos, Rot, Scale
        entt::entity   m_AlignTarget = entt::null; // Target entity for alignment tools
        char           m_AlignSearchBuffer[128]{}; // Search buffer for alignment target

        void RenderMaterialPreview(Boom::MaterialAsset* mat);

        template<typename TComponent, typename GetPropsFn>
        void DrawComponentSection(const char* title,
            TComponent* comp,
            GetPropsFn getProps,
            bool removable,
            const std::function<void()>& onRemove);
    };

}
