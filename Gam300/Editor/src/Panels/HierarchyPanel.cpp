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
        , m_ShowDeletePopup(false)
        , m_EntityToDelete(entt::null)
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

    // Helper to render entity tree recursively
    void HierarchyPanel::RenderEntityNode(entt::registry& registry, entt::entity entity, Boom::AppInterface* app, Boom::AppContext* ctx,
                          bool& showDeletePopup, entt::entity& entityToDelete)
    {
        const auto& info = registry.get<Boom::InfoComponent>(entity);
        const bool isSelected = (app->SelectedEntity() == entity);

        // Get children to determine if this is a leaf node
        auto children = Boom::GetChildren(registry, entity);
        bool hasChildren = !children.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isSelected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        ImGui::PushID(static_cast<int>(entt::to_integral(entity)));

        // Tree node or leaf
        bool nodeOpen = false;
        if (hasChildren) {
            nodeOpen = ImGui::TreeNodeEx(info.name.c_str(), flags);
        } else {
            ImGui::TreeNodeEx(info.name.c_str(), flags);
        }

        // Handle selection
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            app->SelectedEntity(true) = entity;

            // Double-click to focus camera
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                auto entView = registry.view<CameraComponent, TransformComponent>();
                for (auto ent : entView) {
                    auto* camTPtr = &entView.get<TransformComponent>(ent);
                    if (camTPtr && registry.all_of<TransformComponent>(entity)) {
                        const Transform3D& tRef = registry.get<TransformComponent>(entity).transform;
                        camTPtr->transform.translate = tRef.translate;
                        camTPtr->transform.translate += 2.f;
                        glm::vec3 forward = glm::normalize(camTPtr->transform.translate - tRef.translate);
                        float yaw = std::atan2(forward.x, forward.z);
                        float pitch = std::asin(-forward.y);
                        camTPtr->transform.rotate = { glm::degrees(pitch), glm::degrees(yaw), 0.f };
                        break;
                    }
                }
            }
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            entt::entity currentParent = Boom::GetParentEntity(registry, entity);

            if (currentParent != entt::null) {
                if (ImGui::MenuItem("Unparent (Clear Parent)")) {
                    if (Boom::SetParent(registry, entity, entt::null)) {
                        BOOM_INFO("[Hierarchy] Unparented '{}'", info.name);
                    }
                }
                ImGui::Separator();
            }

            if (ImGui::MenuItem("Delete")) {
                // Mark for deletion (will show confirmation popup)
                entityToDelete = entity;
                showDeletePopup = true;
            }

            if (ImGui::MenuItem("Duplicate")) {
                entt::entity duplicated = Boom::DuplicateEntity(registry, entity, true);
                if (duplicated != entt::null) {
                    app->SelectedEntity(true) = duplicated;
                    BOOM_INFO("[Hierarchy] Duplicated '{}' (including {} children)",
                             info.name, Boom::GetChildren(registry, entity).size());
                }
            }

            ImGui::Separator();

            // Debug: Show parent info
            if (currentParent != entt::null && registry.all_of<Boom::InfoComponent>(currentParent)) {
                ImGui::TextDisabled("Parent: %s", registry.get<Boom::InfoComponent>(currentParent).name.c_str());
            } else {
                ImGui::TextDisabled("Parent: None (Root)");
            }

            auto& infoComp = registry.get<Boom::InfoComponent>(entity);
            ImGui::TextDisabled("UID: %llu", infoComp.uid);
            ImGui::TextDisabled("Parent UID: %llu", infoComp.parent);

            ImGui::EndPopup();
        }

        // Drag source (for reparenting)
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("ENTITY_HIERARCHY", &entity, sizeof(entt::entity));
            ImGui::Text("Move: %s", info.name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target (for reparenting)
       // Drop target (for reparenting)
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY")) {
                entt::entity draggedEntity = *(const entt::entity*)payload->Data;
                // Preserve WORLD transform; local will adjust
                if (Boom::SetParent(registry, draggedEntity, entity /* preserveWorldTransform = default true */)) {
                    BOOM_INFO("[Hierarchy] Reparented '{}' to '{}'",
                        registry.get<Boom::InfoComponent>(draggedEntity).name, info.name);
                }
                else {
                    BOOM_WARN("[Hierarchy] Failed to reparent (circular reference prevented)");
                }
            }
            ImGui::EndDragDropTarget();
        }



        // Render children recursively
        if (hasChildren && nodeOpen) {
            for (entt::entity child : children) {
                RenderEntityNode(registry, child, app, ctx, showDeletePopup, entityToDelete);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
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

            auto& registry = m_Ctx->scene;

            // Keyboard shortcuts (only when popup is NOT open)
            if (!m_ShowDeletePopup && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                entt::entity selected = m_App->SelectedEntity();

                // Ctrl+D: Duplicate selected entity
                if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
                    if (selected != entt::null && registry.valid(selected)) {
                        entt::entity duplicated = Boom::DuplicateEntity(registry, selected, true);
                        if (duplicated != entt::null) {
                            m_App->SelectedEntity(true) = duplicated;
                            if (registry.all_of<Boom::InfoComponent>(selected)) {
                                BOOM_INFO("[Hierarchy] Duplicated '{}' with Ctrl+D",
                                         registry.get<Boom::InfoComponent>(selected).name);
                            }
                        }
                    }
                }

                // Delete: Delete selected entity
                if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                    if (selected != entt::null && registry.valid(selected)) {
                        m_EntityToDelete = selected;
                        m_ShowDeletePopup = true;
                    }
                }
            }

            // Drop target for root (to unparent entities)
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY")) {
                    entt::entity draggedEntity = *(const entt::entity*)payload->Data;
                    // Set dragged entity's parent to null (make it root)
                    if (Boom::SetParent(registry, draggedEntity, entt::null)) {
                        BOOM_INFO("[Hierarchy] Unparented '{}'",
                                 registry.get<Boom::InfoComponent>(draggedEntity).name);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Render only root entities (those with no parent)
            auto view = registry.view<Boom::InfoComponent>();
            for (entt::entity e : view)
            {
                const auto& info = view.get<Boom::InfoComponent>(e);

                // Only render if this entity has no parent (is a root)
                if (info.parent == EMPTY_ASSET) {
                    RenderEntityNode(registry, e, m_App, m_Ctx, m_ShowDeletePopup, m_EntityToDelete);
                }
            }

            // Delete confirmation popup
            if (m_ShowDeletePopup) {
                ImGui::OpenPopup("Delete Entity?");
            }

            // Center the popup before opening
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing); // Minimum width

            if (ImGui::BeginPopupModal("Delete Entity?", &m_ShowDeletePopup,
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
                if (registry.valid(m_EntityToDelete) && registry.all_of<Boom::InfoComponent>(m_EntityToDelete)) {
                    const auto& info = registry.get<Boom::InfoComponent>(m_EntityToDelete);
                    auto children = Boom::GetChildren(registry, m_EntityToDelete);

                    ImGui::Text("Are you sure you want to delete:");
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "  %s", info.name.c_str());

                    if (!children.empty()) {
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "This will also delete %zu child entities:", children.size());
                        int displayed = 0;
                        for (entt::entity child : children) {
                            if (registry.all_of<Boom::InfoComponent>(child)) {
                                ImGui::BulletText("%s", registry.get<Boom::InfoComponent>(child).name.c_str());
                                if (++displayed >= 5 && children.size() > 5) {
                                    ImGui::BulletText("... and %zu more", children.size() - 5);
                                    break;
                                }
                            }
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                        // Delete entity and all children
                        Boom::DeleteEntityRecursive(registry, m_EntityToDelete, nullptr);

                        // Clear selection if deleted entity was selected
                        if (m_App->SelectedEntity() == m_EntityToDelete) {
                            m_App->SelectedEntity(true) = entt::null;
                        }

                        m_ShowDeletePopup = false;
                        m_EntityToDelete = entt::null;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                        m_ShowDeletePopup = false;
                        m_EntityToDelete = entt::null;
                        ImGui::CloseCurrentPopup();
                    }
                } else {
                    // Entity became invalid
                    m_ShowDeletePopup = false;
                    m_EntityToDelete = entt::null;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            // If popup was closed by clicking X button, reset state
            if (!m_ShowDeletePopup && m_EntityToDelete != entt::null) {
                m_EntityToDelete = entt::null;
            }
        }
        ImGui::End();
    }

} // namespace EditorUI