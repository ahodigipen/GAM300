#include "Panels/SkeletonTreePanel.h"
#include "Editor.h"
#include "Application/Interface.h"
#include "Application/Context.h"
#include "ECS/ECS.hpp"
#include "Graphics/Models/Animator.h"
#include "Vendors/imgui/imgui.h"

using namespace EditorUI;

SkeletonTreePanel::SkeletonTreePanel(Editor* owner)
    : m_Owner(owner)
    , m_App(owner ? static_cast<Boom::AppInterface*>(owner) : nullptr)
{
}

void SkeletonTreePanel::Render()
{
    if (!m_ShowPanel) return;
    if (!m_Owner || !m_App) return;

    ImGui::Begin("Skeleton Tree", &m_ShowPanel);

    // Get selected entity
    auto selectedID = m_App->SelectedEntity();
    if (selectedID == entt::null)
    {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    Boom::Entity selected(&m_App->GetContext()->scene, selectedID);

    // Check if entity has animator
    if (!selected.Has<Boom::AnimatorComponent>())
    {
        ImGui::TextDisabled("Selected entity has no AnimatorComponent");
        ImGui::End();
        return;
    }

    auto& animComp = selected.Get<Boom::AnimatorComponent>();
    if (!animComp.animator)
    {
        ImGui::TextDisabled("Animator is null");
        ImGui::End();
        return;
    }

    // Get the root joint
    const auto& animator = animComp.animator;

    ImGui::Text("Skeleton Hierarchy");
    ImGui::Separator();
    ImGui::Spacing();

    // Start recursive rendering from root
    if (ImGui::BeginChild("SkeletonTreeScroll", ImVec2(0, 0), false))
    {
        // Access the root joint (need to add getter to Animator)
        // For now, we'll add a public method to get the root
        RenderJointNode(animator->GetRoot());
    }
    ImGui::EndChild();

    ImGui::End();
}

void SkeletonTreePanel::RenderJointNode(const Boom::Joint& joint)
{
    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

    // If this joint is selected, highlight it
    if (joint.name == m_SelectedBoneName)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // If no children, make it a leaf node
    if (joint.children.empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Display joint name with index
    std::string label = joint.name + " [" + std::to_string(joint.index) + "]";
    bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

    // Handle selection
    if (ImGui::IsItemClicked())
    {
        m_SelectedBoneName = joint.name;
        // Update global context so viewport can highlight it
        if (m_App && m_App->GetContext())
        {
            m_App->GetContext()->SelectedBoneName = joint.name;
        }
    }

    // Tooltip with more info
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Bone: %s", joint.name.c_str());
        ImGui::Text("Index: %d", joint.index);
        ImGui::Text("Children: %zu", joint.children.size());
        ImGui::EndTooltip();
    }

    // Recurse to children
    if (nodeOpen && !joint.children.empty())
    {
        for (const auto& child : joint.children)
        {
            RenderJointNode(child);
        }
        ImGui::TreePop();
    }
}
