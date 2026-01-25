// Panels/InspectorPanel.cpp
#include "Panels/Inspector/InspectorPanel.h"
#include "Editor.h"          // for Editor::GetContext()
#include "Context/Context.h"        // for Boom::AppContext + scene access
#include "Vendors/imgui/imgui.h"
#include "Auxiliaries/Assets.h"
#include "Context/DebugHelpers.h"
#include "Panels/PropertiesImgui.h"
#include"Physics/Context.h"
#include "Commands/UndoRedo.h"  // for ComponentPropertyCommand
#include "Audio/Audio.hpp"     // for SoundEngine (real-time audio preview)
#include <GLFW/glfw3.h>
#include <unordered_map>       // for audio preview tracking
//#include "BoomProperties.h"
using namespace EditorUI;

Boom::AppContext* InspectorPanel::GetContext() const {
    return m_Owner ? m_Owner->GetContext() : nullptr;
}

namespace EditorUI {

    InspectorPanel::InspectorPanel(Editor* owner, bool* showFlag)
        : m_Owner(owner)
        , m_ShowInspector(showFlag)
        , m_App(dynamic_cast<Boom::AppInterface*>(owner))
    {
        DEBUG_POINTER(m_App, "AppInterface");
    }

    // ---- templated section drawer ----
    template<typename TComponent, typename GetPropsFn>
    void InspectorPanel::DrawComponentSection(const char* title,
        TComponent* comp,
        GetPropsFn getProps,
        bool removable,
        const std::function<void()>& onRemove)
    {
        if (!comp) return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen
            | ImGuiTreeNodeFlags_Framed
            | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_AllowItemOverlap;

        bool open = ImGui::TreeNodeEx((void*)comp, flags, "%s", title);

        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        const float  lineH = ImGui::GetFrameHeight();

        // right-align the "..." inside the header, unique popup per component
        ImGui::PushID(comp);
        if (removable) {
            const float y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
            if (ImGui::Button("...", ImVec2(lineH, lineH)))
                ImGui::OpenPopup("ComponentSettings");

            if (ImGui::BeginPopup("ComponentSettings")) {
                if (ImGui::MenuItem("Remove Component")) {
                    if (onRemove) onRemove();
                    ImGui::EndPopup();
                    if (open) ImGui::TreePop();
                    ImGui::PopID();
                    return;
                }
                ImGui::EndPopup();
            }
        }
        ImGui::PopID();

        ImGui::SetCursorScreenPos(ImVec2(headerMin.x, headerMax.y + ImGui::GetStyle().ItemSpacing.y));

        if (open) {
            using SchemaT = const xproperty::type::object*;
            SchemaT schema = nullptr;

            // Try all reasonable call forms; your macro declares: SchemaT GetXxx(void*)
            if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, void*>) {
                schema = getProps(static_cast<void*>(comp));
            }
            else if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, TComponent*>) {
                schema = getProps(comp);
            }
            else if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, const TComponent*>) {
                schema = getProps(static_cast<const TComponent*>(comp));
            }
            else if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, TComponent&>) {
                schema = getProps(*comp);
            }
            else if constexpr (std::is_invocable_r_v<SchemaT, GetPropsFn, const TComponent&>) {
                schema = getProps(static_cast<const TComponent&>(*comp));
            }
            else {
                // (Fallback: if someone passes a void-returning drawer)
                if constexpr (std::is_invocable_v<GetPropsFn, void*>)               getProps(static_cast<void*>(comp));
                else if constexpr (std::is_invocable_v<GetPropsFn, TComponent*>)    getProps(comp);
                else if constexpr (std::is_invocable_v<GetPropsFn, const TComponent*>) getProps(static_cast<const TComponent*>(comp));
                else if constexpr (std::is_invocable_v<GetPropsFn, TComponent&>)    getProps(*comp);
                else if constexpr (std::is_invocable_v<GetPropsFn, const TComponent&>) getProps(static_cast<const TComponent&>(*comp));
            }

            // If we got a schema, render it with your UI bridge
            if (schema) {
                DrawPropertiesUI(schema, static_cast<void*>(comp));
            }

            ImGui::TreePop();
        }
    }
    void InspectorPanel::Render()
    {
        if (m_ShowInspector && !*m_ShowInspector) return;

        Boom::AppContext* ctx = GetContext();
        if (!ctx) return;

        ImGui::Begin("Inspector", m_ShowInspector);

        DeleteUpdate();
        if (m_App->SelectedEntity() != entt::null) {
            EntityUpdate();
        }
        else if (m_App->SelectedAsset().id != 0u) {
            AssetUpdate();
        }
        else {
            ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.5f - 20);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::TextWrapped("Select an entity in the hierarchy or an asset in resources to view its properties");
            ImGui::PopStyleColor();
        }

        ImGui::End();
    }

    void InspectorPanel::EntityUpdate() {
        Boom::AppContext* ctx = GetContext();
        // NOTE: adjust Entity wrapper to your real type/ctor signature
            // Assuming: Entity(Boom::Scene*, entt::entity)
        Boom::Entity selected{ &ctx->scene, m_App->SelectedEntity() };

        // ===== ENTITY NAME =====
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));

        if (selected.Has<Boom::InfoComponent>()) {
            auto& info = selected.Get<Boom::InfoComponent>();
            ImGui::TextUnformatted("Entity");
            ImGui::SameLine();
            ImGui::PushItemWidth(-1);
#ifdef _MSC_VER
            strncpy_s(m_NameBuffer, sizeof(m_NameBuffer), info.name.c_str(), sizeof(m_NameBuffer) - 1);
#else
            std::snprintf(m_NameBuffer, sizeof(m_NameBuffer), "%s", info.name.c_str());
#endif
            if (ImGui::InputText("##EntityName", m_NameBuffer, sizeof(m_NameBuffer))) {
                info.name = std::string(m_NameBuffer);
            }
            ImGui::PopItemWidth();

            // ===== PARENT FIELD =====
            ImGui::Spacing();
            ImGui::TextUnformatted("Parent");
            ImGui::SameLine();
            ImGui::PushItemWidth(-1);

            entt::entity currentParent = Boom::GetParentEntity(ctx->scene, m_App->SelectedEntity());
            std::string parentName = "None";
            if (currentParent != entt::null && ctx->scene.all_of<Boom::InfoComponent>(currentParent)) {
                parentName = ctx->scene.get<Boom::InfoComponent>(currentParent).name;
            }

            ImGui::Button(parentName.c_str(), ImVec2(-1, 0));

            // Drag-drop to set parent
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY")) {
                    entt::entity draggedEntity = *(const entt::entity*)payload->Data;
                    if (Boom::SetParent(ctx->scene, m_App->SelectedEntity(), draggedEntity)) {
                        BOOM_INFO("[Inspector] Set parent to '{}'",
                                 ctx->scene.get<Boom::InfoComponent>(draggedEntity).name);
                    } else {
                        BOOM_WARN("[Inspector] Failed to set parent (circular reference prevented)");
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Right-click to clear parent
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && currentParent != entt::null) {
                if (Boom::SetParent(ctx->scene, m_App->SelectedEntity(), entt::null)) {
                    BOOM_INFO("[Inspector] Cleared parent");
                }
            }

            ImGui::PopItemWidth();

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Drag an entity from Hierarchy to set parent\nRight-click to clear parent");
            }
        }

        ImGui::PopStyleVar();
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        // ===== COMPONENTS =====
        if (selected.Has<Boom::TransformComponent>()) {
            auto& tc = selected.Get<Boom::TransformComponent>();
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                //modified as dragging speed should vary between variables

                // Track when any transform drag starts
                ImGui::DragFloat3("Translate", &tc.transform.translate[0], 0.01f);
                if (ImGui::IsItemActivated()) {
                    m_TransformBeforeEdit = tc.transform;
                    m_IsTransformBeingEdited = true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    auto* history = m_Owner->GetCommandHistory();
                    if (history) {
                        auto command = std::make_unique<TransformCommand>(
                            &ctx->scene,
                            m_App->SelectedEntity(),
                            m_TransformBeforeEdit,
                            tc.transform,
                            "Change Position"
                        );
                        history->Execute(std::move(command));
                        BOOM_INFO("[Undo] Created command: Change Position");
                    }
                    m_IsTransformBeingEdited = false;
                }

                ImGui::DragFloat3("Rotation", &tc.transform.rotate[0], .3142f);
                if (ImGui::IsItemActivated()) {
                    m_TransformBeforeEdit = tc.transform;
                    m_IsTransformBeingEdited = true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    auto* history = m_Owner->GetCommandHistory();
                    if (history) {
                        auto command = std::make_unique<TransformCommand>(
                            &ctx->scene,
                            m_App->SelectedEntity(),
                            m_TransformBeforeEdit,
                            tc.transform,
                            "Change Rotation"
                        );
                        history->Execute(std::move(command));
                        BOOM_INFO("[Undo] Created command: Change Rotation");
                    }
                    m_IsTransformBeingEdited = false;
                }

                ImGui::DragFloat3("Scale", &tc.transform.scale[0], 0.01f);
                tc.transform.scale = glm::max(glm::vec3(0.01f), tc.transform.scale); //limit scale to positive
                if (ImGui::IsItemActivated()) {
                    m_TransformBeforeEdit = tc.transform;
                    m_IsTransformBeingEdited = true;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    auto* history = m_Owner->GetCommandHistory();
                    if (history) {
                        auto command = std::make_unique<TransformCommand>(
                            &ctx->scene,
                            m_App->SelectedEntity(),
                            m_TransformBeforeEdit,
                            tc.transform,
                            "Change Scale"
                        );
                        history->Execute(std::move(command));
                        BOOM_INFO("[Undo] Created command: Change Scale");
                    }
                    m_IsTransformBeingEdited = false;
                }
                ImGui::Spacing();
                ImGui::SeparatorText("Utilities");

                // We use -1 width to make the buttons span the whole panel
                if (ImGui::Button("Snap to Floor", ImVec2(-1, 0))) {
                    SnapEntity(selected, glm::vec3(0.0f, -1.0f, 0.0f));
                }

                if (ImGui::Button("Snap to Wall (Left)", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 4, 0))) {
                    SnapEntity(selected, glm::vec3(-1.0f, 0.0f, 0.0f));
                }
                ImGui::SameLine();
                if (ImGui::Button("Snap to Wall (Right)", ImVec2(-1, 0))) {
                    SnapEntity(selected, glm::vec3(1.0f, 0.0f, 0.0f));
                }
            }
        }

        if (selected.Has<Boom::CameraComponent>()) {
            auto& cc = selected.Get<Boom::CameraComponent>();
            DrawComponentSection("Camera", &cc, GetCameraComponentProperties, true,
                [&]() { ctx->scene.remove<Boom::CameraComponent>(m_App->SelectedEntity()); });
        }

        if (selected.Has<Boom::ThirdPersonCameraComponent>())
        {
            bool component_open = ImGui::CollapsingHeader("Third Person Camera", ImGuiTreeNodeFlags_DefaultOpen);

            // Add a "..." button to remove the component (optional but good to have)
            ComponentSettings<Boom::ThirdPersonCameraComponent>(ctx);

            if (component_open)
            {
                auto& tpc = selected.Get<Boom::ThirdPersonCameraComponent>();

                // --- BEGIN CUSTOM UI WIDGET ---
                ImGui::Text("Target Entity");
                ImGui::SameLine();

                // Find the name of the currently targeted entity
                const char* currentTargetName = "None";
                if (tpc.targetUID != 0) {
                    auto infoView = ctx->scene.view<Boom::InfoComponent>();
                    for (auto e : infoView) {
                        const auto& info = infoView.get<Boom::InfoComponent>(e);
                        if (info.uid == tpc.targetUID) {
                            currentTargetName = info.name.c_str();
                            break;
                        }
                    }
                }

                // Draw the dropdown menu
                if (ImGui::BeginCombo("##TargetEntity", currentTargetName))
                {
                    // Add a "None" option
                    if (ImGui::Selectable("None", tpc.targetUID == 0)) {
                        tpc.targetUID = 0;
                    }

                    // Loop through all entities with an InfoComponent to populate the list
                    auto infoView = ctx->scene.view<Boom::InfoComponent>();
                    for (auto e : infoView) {
                        const auto& info = infoView.get<Boom::InfoComponent>(e);
                        const bool isSelected = (tpc.targetUID == info.uid);
                        if (ImGui::Selectable(info.name.c_str(), isSelected)) {
                            tpc.targetUID = info.uid; // Set the UID when selected
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                // --- END CUSTOM UI WIDGET ---

                // Now draw the rest of the properties automatically using xproperty
                DrawPropertiesUI(Boom::GetThirdPersonCameraComponentProperties(&tpc), &tpc);
            }
        }

        // Model Component
        if (selected.Has<Boom::ModelComponent>()) {
            ImGui::PushID("Model Renderer");
            auto& mc = selected.Get<Boom::ModelComponent>();

            // Use CollapsingHeader to match the style
            if (ImGui::CollapsingHeader("Model Renderer", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap)) {
                if (ComponentSettings<Boom::ModelComponent>(ctx)) {
                    ImGui::PopID();
                    return; // Component was removed, exit early
                }

                // Track previous model per-entity to detect change
                static std::unordered_map<entt::entity, AssetID> previousModelIDs;
                AssetID& previousModelID = previousModelIDs[m_App->SelectedEntity()];
                const bool modelChanged = (mc.modelID != previousModelID);

                // --- UI for assigning model and material ---
                ImGui::BeginTable("##maps", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV);
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
                InputAssetWidget<CONSTANTS::DND_PAYLOAD_MODEL>("Model", mc.modelID);
                InputAssetWidget<CONSTANTS::DND_PAYLOAD_MATERIAL>("Material", mc.materialID);
                ImGui::EndTable();

                // ---- Handle animator add/remove on model change (Unity-like) ----
                if (modelChanged) {
                    previousModelID = mc.modelID;

                    if (mc.modelID != EMPTY_ASSET) {
                        // Resolve the chosen model
                        auto& modelAsset = m_App->GetAssetRegistry().Get<ModelAsset>(mc.modelID);

                        // If this model is skeletal, ensure we (re)provision an AnimatorComponent
                        if (modelAsset.hasJoints && modelAsset.data) {
                            // Try to fetch a skeletal interface / animator from the model
                            // (Your SkeletalModel should expose GetAnimator() or similar.)
                            auto skeletalModel = std::dynamic_pointer_cast<Boom::SkeletalModel>(modelAsset.data);
                            if (skeletalModel && skeletalModel->GetAnimator()) {
                                if (selected.Has<Boom::AnimatorComponent>()) {
                                    // Update skeleton but preserve states/clips/parameters
                                    auto& animComp = selected.Get<Boom::AnimatorComponent>();
                                    if (animComp.animator) {
                                        animComp.animator->UpdateSkeletonFrom(*skeletalModel->GetAnimator());
                                        BOOM_INFO("Updated skeleton (preserved states/clips).");
                                    } else {
                                        animComp.animator = skeletalModel->GetAnimator()->Clone();
                                        BOOM_INFO("Created new animator.");
                                    }
                                } else {
                                    auto& animComp = selected.Attach<Boom::AnimatorComponent>();
                                    animComp.animator = skeletalModel->GetAnimator()->Clone();
                                    BOOM_INFO("Auto-added AnimatorComponent for skeletal model.");
                                }
                            }
                        }
                        else {
                            // Non-skeletal model: remove AnimatorComponent if present (Unity behavior)
                            if (selected.Has<Boom::AnimatorComponent>()) {
                                ctx->scene.remove<Boom::AnimatorComponent>(m_App->SelectedEntity());
                                BOOM_INFO("Removed AnimatorComponent (model is non-skeletal).");
                            }
                        }
                    }
                    else {
                        // Cleared the model asset entirely; remove animator if present
                        if (selected.Has<Boom::AnimatorComponent>()) {
                            ctx->scene.remove<Boom::AnimatorComponent>(m_App->SelectedEntity());
                            BOOM_INFO("Removed AnimatorComponent (no model assigned).");
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::SeparatorText("Physics");

                // --- UI for cooking the mesh collider ---
                if (mc.modelID != EMPTY_ASSET) {
                    ModelAsset& modelAsset = m_App->GetAssetRegistry().Get<ModelAsset>(mc.modelID);

                    if (modelAsset.data) {
                        std::string saveDir = "Resources/Physics/";
                        if (!std::filesystem::exists(saveDir)) {
                            std::filesystem::create_directories(saveDir);
                        }

                        // ---------------------------------------------------------
                        // 1. CONVEX MESH BUTTON (For Dynamic Objects)
                        // ---------------------------------------------------------
                        std::string convexPath = saveDir + modelAsset.name + ".pxm";
                        auto* existingConvex = m_App->GetAssetRegistry().FindPhysicsMeshByPath(convexPath);
                        bool convexFileExists = std::filesystem::exists(convexPath);

                        if (existingConvex || convexFileExists) {
                            ImGui::BeginDisabled();
                            ImGui::Button("Convex Mesh Compiled", ImVec2(-1, 0));
                            ImGui::EndDisabled();
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                                ImGui::SetTooltip("Use for DYNAMIC objects.\nAsset: %s", convexPath.c_str());
                            }
                        }
                        else {
                            if (ImGui::Button("Compile Convex Mesh (Dynamic)", ImVec2(-1, 0))) {
                                bool success = m_App->GetPhysicsContext().CompileAndSavePhysicsMesh(modelAsset, convexPath);
                                if (success) {
                                    AssetID newID = RandomU64();
                                    m_App->GetAssetRegistry().AddPhysicsMesh(newID, convexPath)->name = modelAsset.name; // Name: "Stairs"
                                    BOOM_INFO("Successfully cooked Convex Mesh '{}'", modelAsset.name);
                                    m_App->SaveAssets();
                                }
                                else {
                                    BOOM_ERROR("Failed to cook convex mesh.");
                                }
                            }
                        }

                        ImGui::Spacing();

                        // ---------------------------------------------------------
                        // 2. TRIANGLE MESH BUTTON (For Static/Complex Objects)
                        // ---------------------------------------------------------
                        std::string triPath = saveDir + modelAsset.name + "_tri.pxm";
                        auto* existingTri = m_App->GetAssetRegistry().FindPhysicsMeshByPath(triPath);
                        bool triFileExists = std::filesystem::exists(triPath);

                        if (existingTri || triFileExists) {
                            ImGui::BeginDisabled();
                            ImGui::Button("Triangle Mesh Compiled", ImVec2(-1, 0));
                            ImGui::EndDisabled();
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                                ImGui::SetTooltip("Use for STATIC objects (Exact Geometry).\nAsset: %s", triPath.c_str());
                            }
                        }
                        else {
                            // Label this clearly so users know it's for static objects
                            if (ImGui::Button("Compile Exact Mesh (Static Only)", ImVec2(-1, 0))) {
                                bool success = m_App->GetPhysicsContext().CompileAndSaveTriangleMesh(modelAsset, triPath);
                                if (success) {
                                    AssetID newID = RandomU64();
                                    // Append suffix to name so you can tell them apart in the asset picker
                                    m_App->GetAssetRegistry().AddPhysicsMesh(newID, triPath)->name = modelAsset.name + " (Tri)";
                                    BOOM_INFO("Successfully cooked Triangle Mesh '{}'", modelAsset.name);
                                    m_App->SaveAssets();
                                }
                                else {
                                    BOOM_ERROR("Failed to cook triangle mesh.");
                                }
                            }
                        }

                    }
                    else {
                        ImGui::TextDisabled("Model data not yet loaded.");
                    }
                }
                else {
                    ImGui::TextDisabled("Assign a model to enable mesh cooking.");
                }
            }
            ImGui::PopID();
        }

        if (selected.Has<Boom::SpriteComponent>()) {
            ImGui::PushID("Sprite");
            if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap)) {
                if (ComponentSettings<Boom::SpriteComponent>(ctx)) {
                    ImGui::PopID();
                    return; // Component was removed, exit early
                }

                auto& q = selected.Get<Boom::SpriteComponent>();

                // Track sprite edits for undo/redo
                // Capture state before any changes this frame
                Boom::SpriteComponent spriteBeforeFrame = q;

                // GUI Overlay Checkbox
                bool oldUiOverlay = q.uiOverlay;
                if (ImGui::Checkbox("GUI", &q.uiOverlay)) {
                    // Checkbox was toggled - create undo command immediately
                    auto* history = m_Owner->GetCommandHistory();
                    if (history) {
                        Boom::SpriteComponent before = spriteBeforeFrame;
                        before.uiOverlay = oldUiOverlay;
                        auto command = std::make_unique<ComponentPropertyCommand<Boom::SpriteComponent>>(
                            &ctx->scene,
                            m_App->SelectedEntity(),
                            before,
                            q,
                            "Toggle Sprite GUI Overlay"
                        );
                        history->Execute(std::move(command));
                        BOOM_INFO("[Undo] Created command: Toggle Sprite GUI Overlay");
                    } else {
                        BOOM_WARN("[Undo] CommandHistory is null!");
                    }
                }

                // Texture Widget
                Boom::AssetID oldTextureID = q.textureID;
                ImGui::BeginTable("##maps", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV);
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
                InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("texture", q.textureID);
                ImGui::EndTable();

                // Check if texture changed (via drag-drop or other means)
                if (oldTextureID != q.textureID) {
                    auto* history = m_Owner->GetCommandHistory();
                    if (history) {
                        Boom::SpriteComponent before = spriteBeforeFrame;
                        before.textureID = oldTextureID;
                        auto command = std::make_unique<ComponentPropertyCommand<Boom::SpriteComponent>>(
                            &ctx->scene,
                            m_App->SelectedEntity(),
                            before,
                            q,
                            "Change Sprite Texture"
                        );
                        history->Execute(std::move(command));
                        BOOM_INFO("[Undo] Created command: Change Sprite Texture");
                    }
                }

                // Color Picker - track when editing finishes
                glm::vec4 oldColor = q.color;
                if (ImGui::ColorEdit4("color", &q.color[0])) {
                    // Color is being edited
                    if (!m_IsSpriteBeingEdited) {
                        m_SpriteBeforeEdit = spriteBeforeFrame;
                        m_IsSpriteBeingEdited = true;
                    }
                }

                // When color picker is closed/deactivated, create undo command
                if (m_IsSpriteBeingEdited && !ImGui::IsItemActive() && ImGui::IsItemEdited()) {
                    auto* history = m_Owner->GetCommandHistory();
                    if (history) {
                        auto command = std::make_unique<ComponentPropertyCommand<Boom::SpriteComponent>>(
                            &ctx->scene,
                            m_App->SelectedEntity(),
                            m_SpriteBeforeEdit,
                            q,
                            "Change Sprite Color"
                        );
                        history->Execute(std::move(command));
                        BOOM_INFO("[Undo] Created command: Change Sprite Color");
                    }
                    m_IsSpriteBeingEdited = false;
                }
            }
            ImGui::PopID();
        }

        // === CHARACTER CONTROLLER COMPONENT ===
        if (selected.Has<Boom::CharacterControllerComponent>()) {
            ImGui::PushID("CharacterController");

            bool isOpen = ImGui::CollapsingHeader("Character Controller", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);

            // Settings button
            const ImVec2 headerMin = ImGui::GetItemRectMin();
            const ImVec2 headerMax = ImGui::GetItemRectMax();
            const float lineH = ImGui::GetFrameHeight();
            const float y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
            if (ImGui::Button("...", ImVec2(lineH, lineH)))
                ImGui::OpenPopup("CharacterControllerSettings");

            bool removed = false;
            if (ImGui::BeginPopup("CharacterControllerSettings")) {
                if (ImGui::MenuItem("Remove Component")) {
                    removed = true;
                }
                ImGui::EndPopup();
            }

            ImGui::SetCursorScreenPos(ImVec2(headerMin.x, headerMax.y + ImGui::GetStyle().ItemSpacing.y));

            if (isOpen) {
                ImGui::Indent(12.0f);
                ImGui::Spacing();

                auto& cc = selected.Get<Boom::CharacterControllerComponent>();
                bool hasPhysicsController = m_App->GetPhysicsContext().HasController(selected);

                // Sync component values from actual PhysX controller if it exists
                if (hasPhysicsController) {
                    float actualRadius, actualHeight;
                    if (m_App->GetPhysicsContext().GetControllerDimensions(
                        static_cast<uint32_t>(selected.ID()), actualRadius, actualHeight)) {
                        // Update component to reflect actual PhysX state
                        cc.radius = actualRadius;
                        cc.height = actualHeight;
                    }
                }

                // Store old values for change detection
                float oldRadius = cc.radius;
                float oldHeight = cc.height;
                float oldStepOffset = cc.stepOffset;
                float oldContactOffset = cc.contactOffset;
                float oldSlopeLimit = cc.slopeLimit;
                glm::vec3 oldLocalOffset = cc.localOffset;

                ImGui::SeparatorText("Capsule Shape");
                ImGui::Spacing();

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Radius");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##CCRadius", &cc.radius, 0.01f, 0.1f, 10.0f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Height");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##CCHeight", &cc.height, 0.01f, 0.5f, 20.0f);

                ImGui::Spacing();
                ImGui::SeparatorText("Transform");
                ImGui::Spacing();

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Center Offset");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##CCLocalOffset", &cc.localOffset.x, 0.01f);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Offset of the capsule center from the entity's pivot.\nUse positive Y to raise the capsule (fixes floating).");
                }

                ImGui::Spacing();
                ImGui::SeparatorText("Movement");
                ImGui::Spacing();

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Step Offset");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##CCStepOffset", &cc.stepOffset, 0.01f, 0.0f, 2.0f);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Maximum height the controller can step up");
                }

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Contact Offset");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##CCContactOffset", &cc.contactOffset, 0.01f, 0.01f, 1.0f);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Skin width for collision detection (prevents tunneling)");
                }

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Slope Limit (deg)");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##CCSlopeLimit", &cc.slopeLimit, 0.5f, 0.0f, 90.0f);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Maximum walkable slope angle in degrees");
                }

                // Runtime status
                ImGui::Spacing();
                ImGui::SeparatorText("Runtime Status");
                ImGui::Spacing();

                if (hasPhysicsController) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "[OK] Physics controller active");
                }
                else {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "[!] Controller not created (enter Play mode)");
                }

                // Utility buttons
                ImGui::Spacing();
                if (hasPhysicsController) {
                    if (ImGui::Button("Reset to Transform Position", ImVec2(-1, 0))) {
                        // Teleport controller to entity's transform position + offset
                        if (selected.Has<Boom::TransformComponent>()) {
                            auto& tc = selected.Get<Boom::TransformComponent>();
                            glm::vec3 targetPos = tc.transform.translate + cc.localOffset;
                            m_App->GetPhysicsContext().SetControllerPosition(
                                static_cast<uint32_t>(selected.ID()),
                                targetPos
                            );
                        }
                    }
                }

                // Check if shape parameters changed - update PhysX controller immediately if it exists
                if (cc.radius != oldRadius || cc.height != oldHeight) {
                    // If controller exists at runtime, resize it immediately
                    if (hasPhysicsController) {
                        m_App->GetPhysicsContext().ResizeCapsuleController(
                            static_cast<uint32_t>(selected.ID()),
                            cc.radius,
                            cc.height
                        );
                        BOOM_INFO("[Inspector] Resized active controller to radius={}, height={}", cc.radius, cc.height);
                    }
                    else {
                        // Controller doesn't exist yet - mark for recreation on play
                        cc.isCreated = false;
                        BOOM_INFO("[Inspector] Character controller parameters changed - will recreate on play");
                    }
                }

                // Handle local offset changes - teleport controller if it exists
                if (cc.localOffset != oldLocalOffset && hasPhysicsController) {
                    if (selected.Has<Boom::TransformComponent>()) {
                        auto& tc = selected.Get<Boom::TransformComponent>();
                        glm::vec3 targetPos = tc.transform.translate + cc.localOffset;
                        m_App->GetPhysicsContext().SetControllerPosition(
                            static_cast<uint32_t>(selected.ID()),
                            targetPos
                        );
                        BOOM_INFO("[Inspector] Updated controller position with new offset");
                    }
                }

                // Also handle stepOffset, contactOffset, slopeLimit changes
                // These require controller recreation since PhysX doesn't allow runtime changes
                if (cc.stepOffset != oldStepOffset || cc.contactOffset != oldContactOffset ||
                    cc.slopeLimit != oldSlopeLimit) {
                    cc.isCreated = false;
                    if (hasPhysicsController) {
                        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1),
                            "Step/Contact/Slope changes require re-entering Play mode");
                    }
                }

                ImGui::Spacing();
                ImGui::Unindent(12.0f);
            }

            ImGui::PopID();

            if (removed) {
                // Destroy physics controller if exists
                m_App->GetPhysicsContext().DestroyController(static_cast<uint32_t>(m_App->SelectedEntity()));
                ctx->scene.remove<Boom::CharacterControllerComponent>(m_App->SelectedEntity());
                return;
            }
            ImGui::Spacing();
        }

        if (selected.Has<Boom::AnimatorComponent>()) {
            AnimatorComponentUI(selected);
        }

        // --- Sound Component UI ---
        if (selected.Has<Boom::SoundComponent>()) {
            ImGui::PushID("Sound");
            auto& sc = selected.Get<Boom::SoundComponent>();

            bool compRemoved = false;
            bool isOpen = ImGui::CollapsingHeader("Sound", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);

            // settings popup
            const ImVec2 shMin = ImGui::GetItemRectMin();
            const ImVec2 shMax = ImGui::GetItemRectMax();
            const float shLineH = ImGui::GetFrameHeight();
            const float shY = shMin.y + (shMax.y - shMin.y - shLineH) *0.5f;
            ImGui::SetCursorScreenPos(ImVec2(shMax.x - shLineH, shY));
            if (ImGui::Button("...", ImVec2(shLineH, shLineH))) ImGui::OpenPopup("SoundSettings");
            if (ImGui::BeginPopup("SoundSettings")) {
                if (ImGui::MenuItem("Remove Component")) compRemoved = true;
                ImGui::EndPopup();
            }

            ImGui::SetCursorScreenPos(ImVec2(shMin.x, shMax.y + ImGui::GetStyle().ItemSpacing.y));

            if (isOpen) {
                ImGui::Indent(12.0f);
                ImGui::Spacing();

                ImGui::Text("Entries: %zu", sc.entries.size());
                ImGui::SameLine();
                if (ImGui::Button("+ Add Entry")) {
                    Boom::SoundComponent::Entry e{};
                    e.name = "NewSound";
                    sc.entries.push_back(std::move(e));
                }

                ImGui::Spacing();

                for (size_t i =0; i < sc.entries.size(); ++i) {
                    auto& entry = sc.entries[i];
                    ImGui::PushID(static_cast<int>(i));

                    // header for entry
                    bool openEntry = ImGui::TreeNodeEx((void*)(intptr_t)i, ImGuiTreeNodeFlags_DefaultOpen, "%s", entry.name.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove")) {
                        sc.entries.erase(sc.entries.begin() + i);
                        ImGui::PopID();
                        break; // indices changed; break out to avoid iterator invalidation
                    }

                    if (openEntry) {
                        // Name
                        char nameBuf[128];
#ifdef _MSC_VER
                        strncpy_s(nameBuf, sizeof(nameBuf), entry.name.c_str(), sizeof(nameBuf)-1);
#else
                        std::snprintf(nameBuf, sizeof(nameBuf), "%s", entry.name.c_str());
#endif
                        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) entry.name = std::string(nameBuf);

                        // Variant files list (filePaths)
                        ImGui::Text("Variants");
                        ImGui::SameLine();
                        if (ImGui::SmallButton("+ Add Variant")) {
                            entry.filePaths.push_back(entry.filePath.empty() ? std::string("") : entry.filePath);
                        }

                        for (size_t v =0; v < entry.filePaths.size(); ++v) {
                            ImGui::PushID(static_cast<int>(v));
                            char pathBuf[512];
#ifdef _MSC_VER
                            strncpy_s(pathBuf, sizeof(pathBuf), entry.filePaths[v].c_str(), sizeof(pathBuf)-1);
#else
 std::snprintf(pathBuf, sizeof(pathBuf), "%s", entry.filePaths[v].c_str());
#endif
 // Manual edit field
 if (ImGui::InputText("File", pathBuf, sizeof(pathBuf))) {
 entry.filePaths[v] = std::string(pathBuf);
 // keep legacy single filePath in sync for older systems
 if (v ==0) entry.filePath = entry.filePaths[0];
 }

 ImGui::SameLine();

 // --- Asset picker dropdown for audio assets ---
 {
 // Build current label (show filename if available)
 std::string curLabel = entry.filePaths[v].empty() ? "Select Audio..." : std::filesystem::path(entry.filePaths[v]).filename().string();
 if (ImGui::BeginCombo("##AudioPicker", curLabel.c_str())) {
 auto& audioMap = m_App->GetAssetRegistry().GetMap<AudioAsset>();
 // Allow clearing
 bool noneSel = entry.filePaths[v].empty();
 if (ImGui::Selectable("None", noneSel)) {
 entry.filePaths[v].clear();
 if (v ==0) entry.filePath.clear();
 }
 if (noneSel) ImGui::SetItemDefaultFocus();

 for (auto& [uid, asset] : audioMap) {
 if (uid == EMPTY_ASSET) continue;
 std::string name = asset->name;
 bool isSel = (entry.filePaths[v] == asset->source);
 if (ImGui::Selectable(name.c_str(), isSel)) {
 entry.filePaths[v] = asset->source;
 if (v ==0) entry.filePath = entry.filePaths[0];
 }
 if (isSel) ImGui::SetItemDefaultFocus();
 }
 ImGui::EndCombo();
 }
 }

 ImGui::SameLine();
 if (ImGui::SmallButton("Remove")) {
 entry.filePaths.erase(entry.filePaths.begin() + v);
 ImGui::PopID();
 break;
 }
 ImGui::PopID();
 }

 // Legacy single filePath (shows only if no variants present)
 if (entry.filePaths.empty()) {
 char legacyBuf[512];
#ifdef _MSC_VER
 strncpy_s(legacyBuf, sizeof(legacyBuf), entry.filePath.c_str(), sizeof(legacyBuf)-1);
#else
 std::snprintf(legacyBuf, sizeof(legacyBuf), "%s", entry.filePath.c_str());
#endif
 if (ImGui::InputText("File Path", legacyBuf, sizeof(legacyBuf))) entry.filePath = std::string(legacyBuf);

 ImGui::SameLine();
 // Legacy asset picker
 {
 std::string cur = entry.filePath.empty() ? "Select Audio..." : std::filesystem::path(entry.filePath).filename().string();
 if (ImGui::BeginCombo("##AudioPickerLegacy", cur.c_str())) {
 auto& audioMap = m_App->GetAssetRegistry().GetMap<AudioAsset>();
 bool noneSel = entry.filePath.empty();
 if (ImGui::Selectable("None", noneSel)) {
 entry.filePath.clear();
 }
 if (noneSel) ImGui::SetItemDefaultFocus();
 for (auto& [uid, asset] : audioMap) {
 if (uid == EMPTY_ASSET) continue;
 bool isSel = (entry.filePath == asset->source);
 if (ImGui::Selectable(asset->name.c_str(), isSel)) {
 entry.filePath = asset->source;
 }
 if (isSel) ImGui::SetItemDefaultFocus();
 }
 ImGui::EndCombo();
 }
 }
 }

                        // loop and playOnStart
                        ImGui::Checkbox("Loop", &entry.loop);
                        ImGui::SameLine();
                        ImGui::Checkbox("Play On Start", &entry.playOnStart);
                        ImGui::SameLine();
                        ImGui::Checkbox("Mute", &entry.mute);

                        // Volume
                        ImGui::SliderFloat("Volume", &entry.volume, 0.0f, 1.0f);

                        // Pitch
                        ImGui::SliderFloat("Pitch", &entry.pitch, 0.5f, 2.0f, "%.2f");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Playback speed: 0.5 = half speed, 1.0 = normal, 2.0 = double speed");
                        }

                        // Priority
                        ImGui::SliderInt("Priority", &entry.priority, 0, 256);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Channel priority: 0 = highest, 256 = lowest (128 = default)");
                        }

                        // Stereo Pan
                        ImGui::SliderFloat("Stereo Pan", &entry.stereoPan, -1.0f, 1.0f, "%.2f");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("-1.0 = full left, 0.0 = center, 1.0 = full right");
                        }

                        // Spatial Blend
                        ImGui::SliderFloat("Spatial Blend", &entry.spatialBlend, 0.0f, 1.0f, "%.2f");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("0.0 = fully 2D (no positional audio), 1.0 = fully 3D (positional)");
                        }

                        ImGui::Separator();
                        ImGui::Text("Triggers");

                        // triggerKey
                        ImGui::InputInt("Trigger Key (GLFW)", &entry.triggerKey);
                        ImGui::TextDisabled("Use GLFW key codes (e.g. %d = Space)", GLFW_KEY_SPACE);

                        // Play on move
                        ImGui::Checkbox("Play On Move", &entry.playOnMove);
                        if (entry.playOnMove) {
                            ImGui::InputFloat("Move Threshold (m/s)", &entry.moveThreshold);
                        }

                        // Repeat interval
                        ImGui::InputFloat("Repeat Interval (s)", &entry.repeatInterval);

                        // Animation trigger name
                        char animBuf[128];
#ifdef _MSC_VER
                        strncpy_s(animBuf, sizeof(animBuf), entry.animTrigger.c_str(), sizeof(animBuf)-1);
#else
                        std::snprintf(animBuf, sizeof(animBuf), "%s", entry.animTrigger.c_str());
#endif
                        if (ImGui::InputText("Anim Trigger", animBuf, sizeof(animBuf))) {
                            entry.animTrigger = std::string(animBuf);
                        }

                        ImGui::Separator();
                        ImGui::Text("3D Audio Settings");

                        // Min/Max Distance sliders with tooltips
                        ImGui::SliderFloat("Min Distance", &entry.minDistance, 0.1f, 100.0f, "%.1f");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Distance at which sound is at full volume (in world units)");
                        }

                        ImGui::SliderFloat("Max Distance", &entry.maxDistance, 1.0f, 200.0f, "%.1f");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Distance at which sound becomes silent (in world units)");
                        }

                        // Validation: ensure min < max
                        if (entry.minDistance >= entry.maxDistance) {
                            entry.minDistance = entry.maxDistance - 0.1f;
                        }

                        // Quick presets
                        ImGui::Text("Quick Presets:");
                        if (ImGui::SmallButton("Footsteps")) {
                            entry.minDistance = 0.5f;
                            entry.maxDistance = 10.0f;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Dialogue")) {
                            entry.minDistance = 1.0f;
                            entry.maxDistance = 30.0f;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Environment")) {
                            entry.minDistance = 2.0f;
                            entry.maxDistance = 100.0f;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Ambient")) {
                            entry.minDistance = 5.0f;
                            entry.maxDistance = 200.0f;
                        }

                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }

                ImGui::Unindent(12.0f);
            }

            ImGui::PopID();

            if (compRemoved) {
                ctx->scene.remove<Boom::SoundComponent>(m_App->SelectedEntity());
            }
        }

        if (selected.Has<Boom::RigidBodyComponent>()) {
            ImGui::PushID("Rigid Body");

            // 1. Draw Header
            bool isOpen = ImGui::CollapsingHeader("Rigidbody", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);

            // 2. Draw "..." Button (to match photo)
            const ImVec2 headerMin = ImGui::GetItemRectMin();
            const ImVec2 headerMax = ImGui::GetItemRectMax();
            const float  lineH = ImGui::GetFrameHeight();
            const float y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
            if (ImGui::Button("...", ImVec2(lineH, lineH)))
                ImGui::OpenPopup("RigidBodySettings");

            bool removed = false;
            if (ImGui::BeginPopup("RigidBodySettings")) {
                if (ImGui::MenuItem("Remove Component")) {
                    removed = true; // Set flag to remove later
                }
                ImGui::EndPopup();
            }

            // 3. Reset cursor
            ImGui::SetCursorScreenPos(ImVec2(headerMin.x, headerMax.y + ImGui::GetStyle().ItemSpacing.y));

            // 4. Draw Contents
            if (isOpen) {
                ImGui::Indent(12.0f);
                ImGui::Spacing();

                auto& rc = selected.Get<Boom::RigidBodyComponent>();

                RigidBody3D::Type currentType = rc.RigidBody.type;
                const char* currentTypeName;
                switch (currentType)
                {
                case RigidBody3D::Type::STATIC:  currentTypeName = "Static";  break;
                case RigidBody3D::Type::DYNAMIC: currentTypeName = "Dynamic"; break;
                default:                         currentTypeName = "Unknown"; break;
                }

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Body Type");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);

                if (ImGui::BeginCombo("##BodyType", currentTypeName))
                {
                    bool isStaticSelected = (currentType == RigidBody3D::Type::STATIC);
                    if (ImGui::Selectable("Static", isStaticSelected))
                    {
                        m_App->GetPhysicsContext().SetRigidBodyType(selected, RigidBody3D::Type::STATIC);
                    }
                    if (isStaticSelected) ImGui::SetItemDefaultFocus();

                    bool isDynamicSelected = (currentType == RigidBody3D::Type::DYNAMIC);
                    if (ImGui::Selectable("Dynamic", isDynamicSelected))
                    {
                        m_App->GetPhysicsContext().SetRigidBodyType(selected, RigidBody3D::Type::DYNAMIC);
                    }
                    if (isDynamicSelected) ImGui::SetItemDefaultFocus();

                    ImGui::EndCombo();
                }

                auto* rigidBody = &rc.RigidBody;

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Density");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##Density", &rigidBody->density, 0.01f, 0.0f, 1000.0f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Mass");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##Mass", &rigidBody->mass, 0.1f, 0.0f, 1000.0f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Initial Velocity");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##InitialVelocity", &rigidBody->initialVelocity.x, 0.01f);

                // --- ADD THIS NEW SECTION ---
                ImGui::Spacing();
                ImGui::SeparatorText("Constraints"); // Uses a nice separator
                ImGui::Spacing();

                // Store old values to detect changes
                bool oldFreezeX = rigidBody->freezeRotationX;
                bool oldFreezeY = rigidBody->freezeRotationY;
                bool oldFreezeZ = rigidBody->freezeRotationZ;

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Freeze Rotation");
                ImGui::SameLine(150);

                // We use Push/PopID to make the labels unique for ImGui
                ImGui::PushID("FreezeRot");
                ImGui::Checkbox("X", &rigidBody->freezeRotationX);
                ImGui::SameLine();
                ImGui::Checkbox("Y", &rigidBody->freezeRotationY);
                ImGui::SameLine();
                ImGui::Checkbox("Z", &rigidBody->freezeRotationZ);
                ImGui::PopID();

                // If any value changed, notify the physics context
                if (rigidBody->freezeRotationX != oldFreezeX ||
                    rigidBody->freezeRotationY != oldFreezeY ||
                    rigidBody->freezeRotationZ != oldFreezeZ)
                {
                    // This is a new function we will need to create in PhysicsContext 
                    m_App->GetPhysicsContext().SetRotationLock(
                        selected,
                        rigidBody->freezeRotationX,
                        rigidBody->freezeRotationY,
                        rigidBody->freezeRotationZ
                    );
                }

                ImGui::Spacing();
                ImGui::Unindent(12.0f);
            }

            ImGui::PopID();

            if (removed) {
                // Destroy physics controller if exists
                m_App->GetPhysicsContext().DestroyController(static_cast<uint32_t>(m_App->SelectedEntity()));
                ctx->scene.remove<Boom::RigidBodyComponent>(m_App->SelectedEntity());
                return;
            }
            ImGui::Spacing();
        }
        // ----- AI (Behaviour Tree) -----
        if (selected.Has<Boom::AIComponent>()) {
            auto& ai = selected.Get<Boom::AIComponent>();
            DrawComponentSection(
                "AI (Behaviour Tree)", &ai,
                [&](void* p) -> const xproperty::type::object* {
                    auto* a = static_cast<Boom::AIComponent*>(p);
                    auto& reg = GetContext()->scene;

                    // --- MODE (this is what you’re missing) --------------------------------
                    ImGui::SeparatorText("Mode");
                    {
                        static const char* kModes[] = { "Auto", "Idle", "Patrol", "Seek" };
                        int idx = static_cast<int>(a->mode);
                        if (ImGui::Combo("Mode", &idx, kModes, IM_ARRAYSIZE(kModes))) {
                            auto newMode = static_cast<Boom::AIComponent::AIMode>(idx);
                            if (a->mode != newMode) {
                                a->mode = newMode;
                                // No more a->root.reset(); AISystem will see mode change and rebuild.
                            }
                        }
                    }

                    // --- PLAYER PICKER ------------------------------------------------------
                    const char* cur = a->playerName.empty() ? "None" : a->playerName.c_str();
                    if (ImGui::BeginCombo("Player (by name)", cur)) {
                        bool isNone = a->playerName.empty();
                        if (ImGui::Selectable("None", isNone)) { a->playerName.clear(); a->player = entt::null; }
                        if (isNone) ImGui::SetItemDefaultFocus();

                        auto view = reg.view<Boom::InfoComponent>();
                        for (auto e : view) {
                            const auto& info = view.get<Boom::InfoComponent>(e);
                            bool sel = (a->playerName == info.name);
                            if (ImGui::Selectable(info.name.c_str(), sel)) {
                                a->playerName = info.name; a->player = entt::null;
                            }
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    // --- TUNING -------------------------------------------------------------
                    ImGui::SeparatorText("Tuning");
                    ImGui::DragFloat("Detect Radius", &a->detectRadius, 0.05f, 0.0f, 200.0f);
                    ImGui::DragFloat("Lose Radius", &a->loseRadius, 0.05f, 0.0f, 200.0f);
                    ImGui::DragFloat("Idle Wait (s)", &a->idleWait, 0.01f, 0.0f, 10.0f);
                    ImGui::InputFloat("Idle Timer (runtime)", &a->idleTimer, 0, 0, "%.3f",
                        ImGuiInputTextFlags_ReadOnly);

                    // --- PATROL -------------------------------------------------------------
                    ImGui::SeparatorText("Patrol");
                    if (selected.Has<Boom::TransformComponent>()) {
                        if (ImGui::Button("Add Point From Entity Pos", ImVec2(-1, 0))) {
                            auto& tc = selected.Get<Boom::TransformComponent>();
                            a->patrolPoints.push_back(tc.transform.translate);
                        }
                    }
                    ImGui::Text("Points: %zu", a->patrolPoints.size());
                    if (ImGui::BeginListBox("##patrol_pts", ImVec2(-1, 160))) {
                        for (int i = 0; i < (int)a->patrolPoints.size(); ++i) {
                            const auto& p3 = a->patrolPoints[i];
                            char lbl[96]; std::snprintf(lbl, sizeof(lbl), "%02d: (%.2f, %.2f, %.2f)", i, p3.x, p3.y, p3.z);
                            bool sel = (a->patrolIndex == i);
                            if (ImGui::Selectable(lbl, sel)) a->patrolIndex = i;
                            if (sel) ImGui::SetItemDefaultFocus();

                            if (ImGui::BeginPopupContextItem(lbl)) {
                                if (ImGui::MenuItem("Remove")) {
                                    a->patrolPoints.erase(a->patrolPoints.begin() + i);
                                    if (a->patrolIndex >= (int)a->patrolPoints.size())
                                        a->patrolIndex = std::max(0, (int)a->patrolPoints.size() - 1);
                                    ImGui::EndPopup(); break;
                                }
                                if (ImGui::MenuItem("Insert After (Here)")) {
                                    glm::vec3 p2 = p3;
                                    if (selected.Has<Boom::TransformComponent>())
                                        p2 = selected.Get<Boom::TransformComponent>().transform.translate;
                                    a->patrolPoints.insert(a->patrolPoints.begin() + i + 1, p2);
                                    ImGui::EndPopup(); break;
                                }
                                ImGui::EndPopup();
                            }
                        }
                        ImGui::EndListBox();
                    }
                    if (a->patrolIndex >= 0 && a->patrolIndex < (int)a->patrolPoints.size()) {
                        auto edit = a->patrolPoints[a->patrolIndex];
                        if (ImGui::DragFloat3("Edit Selected Point", &edit.x, 0.01f))
                            a->patrolPoints[a->patrolIndex] = edit;
                    }

                 
                    return nullptr;

               
                },
                /*removable=*/true,
                [&]() { GetContext()->scene.remove<Boom::AIComponent>(m_App->SelectedEntity()); }
            );
        }



        // ----- Nav Agent -----
        if (selected.Has<Boom::NavAgentComponent>()) {
            auto& ag = selected.Get<Boom::NavAgentComponent>();
            DrawComponentSection(
                "Nav Agent", &ag,
                [&](void* p) -> const xproperty::type::object* {
                    auto* a = static_cast<Boom::NavAgentComponent*>(p);
                    auto& reg = GetContext()->scene;

                    // --- UTILITIES TABLE -----------------------------------------------------
                    ImGui::BeginTable("##navtools", 2, ImGuiTableFlags_SizingStretchProp);
                    ImGui::TableSetupColumn("l", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("r", ImGuiTableColumnFlags_WidthFixed, 140.0f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextDisabled("Utilities");
                    ImGui::TableSetColumnIndex(1);

                    if (ImGui::Button("Target = Player##btn", ImVec2(-1, 0))) {
                        if (selected.Has<Boom::AIComponent>()) {
                            auto& ai = selected.Get<Boom::AIComponent>();
                            if (ai.player != entt::null && reg.all_of<Boom::TransformComponent>(ai.player)) {
                                a->target = reg.get<Boom::TransformComponent>(ai.player).transform.translate;
                                a->dirty = true; a->repathTimer = 0.f;
                            }
                        }
                    }
                    if (ImGui::Button("Target = Here##btn", ImVec2(-1, 0))) {
                        if (selected.Has<Boom::TransformComponent>()) {
                            a->target = selected.Get<Boom::TransformComponent>().transform.translate;
                            a->dirty = true; a->repathTimer = 0.f;
                        }
                    }
                    ImGui::EndTable();
                    ImGui::Separator();

                    // --- BASIC PROPERTIES ----------------------------------------------------
                    bool changed = false;

                    // Target
                    {
                        glm::vec3 t = a->target;
                        if (ImGui::DragFloat3("Target", &t.x, 0.01f)) { a->target = t; changed = true; }
                        if (ImGui::IsItemDeactivatedAfterEdit()) { a->dirty = true; a->repathTimer = 0.f; }
                    }

                    // Speed
                    {
                        float sp = a->speed;
                        if (ImGui::DragFloat("Speed (m/s)", &sp, 0.05f, 0.0f, 100.0f)) { a->speed = sp; }
                    }

                    // Arrive radius
                    {
                        float ar = a->arrive;
                        if (ImGui::DragFloat("Arrive Radius (m)", &ar, 0.01f, 0.01f, 5.0f)) { a->arrive = ar; }
                    }

                    // Active
                    ImGui::Checkbox("Active", &a->active);

                    // Repath tuning
                    {
                        float cd = a->repathCooldown;
                        float rd = a->retargetDist;
                        bool c1 = ImGui::DragFloat("Repath Cooldown (s)", &cd, 0.01f, 0.01f, 10.0f);
                        bool c2 = ImGui::DragFloat("Retarget Distance (m)", &rd, 0.01f, 0.0f, 10.0f);
                        if (c1) a->repathCooldown = cd;
                        if (c2) a->retargetDist = rd;
                        if (c1 || c2) { a->dirty = true; a->repathTimer = 0.f; }
                    }

                    // --- FOLLOW ENTITY PICKER -----------------------------------------------
                    ImGui::SeparatorText("Follow");
                    {
                        // Current label
                        std::string current = "None";
                        if (a->follow != entt::null && reg.all_of<Boom::InfoComponent>(a->follow)) {
                            current = reg.get<Boom::InfoComponent>(a->follow).name;
                        }

                        if (ImGui::BeginCombo("Follow Entity", current.c_str())) {
                            // None option
                            bool isNone = (a->follow == entt::null);
                            if (ImGui::Selectable("None", isNone)) { a->follow = entt::null; a->dirty = true; a->repathTimer = 0.f; }
                            if (isNone) ImGui::SetItemDefaultFocus();

                            // List all entities with InfoComponent
                            auto view = reg.view<Boom::InfoComponent>();
                            for (auto e : view) {
                                const auto& info = view.get<Boom::InfoComponent>(e);
                                bool sel = (a->follow == e);
                                if (ImGui::Selectable(info.name.c_str(), sel)) {
                                    a->follow = e; a->dirty = true; a->repathTimer = 0.f;  a->followName = info.name;
                                }
                                if (sel) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        // Quick actions
                        ImGui::SameLine();
                        if (ImGui::Button("Rebuild Path")) { a->dirty = true; a->repathTimer = 0.f; }
                        ImGui::SameLine();
                        if (ImGui::Button("Clear Follow")) {
                            a->follow = entt::null; a->dirty = true; a->repathTimer = 0.f; a->followName.clear();
                        }
                    }

                    // --- PATH / WAYPOINT TOOLS ----------------------------------------------
                    ImGui::SeparatorText("Path");
                    ImGui::Text("Waypoints: %d / %zu", a->waypoint, a->path.size());
                    ImGui::SameLine();
                    if (ImGui::Button("Clear Path")) { a->path.clear(); a->waypoint = 0; } // view-only for path unless edited below

                    if (!a->path.empty()) {
                        // Select current waypoint
                        if (ImGui::BeginListBox("##pathbox", ImVec2(-1, 140))) {
                            for (int i = 0; i < static_cast<int>(a->path.size()); ++i) {
                                char label[64];
                                std::snprintf(label, sizeof(label), "%02d: (%.2f, %.2f, %.2f)", i, a->path[i].x, a->path[i].y, a->path[i].z);
                                bool selectedRow = (a->waypoint == i);
                                if (ImGui::Selectable(label, selectedRow)) { a->waypoint = i; }
                                if (selectedRow) ImGui::SetItemDefaultFocus();

                                // Context menu per waypoint
                                if (ImGui::BeginPopupContextItem(label)) {
                                    if (ImGui::MenuItem("Remove")) {
                                        a->path.erase(a->path.begin() + i);
                                        if (a->waypoint >= static_cast<int>(a->path.size()))
                                            a->waypoint = (int)std::max<size_t>(0, a->path.size() ? a->path.size() - 1 : 0);
                                        ImGui::EndPopup();
                                        break;
                                    }
                                    if (ImGui::MenuItem("Insert After (use Selected Transform if any)")) {
                                        glm::vec3 insertPos = a->path[i];
                                        if (selected.Has<Boom::TransformComponent>())
                                            insertPos = selected.Get<Boom::TransformComponent>().transform.translate;
                                        a->path.insert(a->path.begin() + i + 1, insertPos);
                                        ImGui::EndPopup();
                                        break;
                                    }
                                    ImGui::EndPopup();
                                }
                            }
                            ImGui::EndListBox();
                        }

                        // Edit currently selected waypoint
                        if (a->waypoint >= 0 && a->waypoint < (int)a->path.size()) {
                            glm::vec3 wp = a->path[a->waypoint];
                            if (ImGui::DragFloat3("Edit Selected Waypoint", &wp.x, 0.01f)) {
                                a->path[a->waypoint] = wp;
                                // editing path does not need immediate rebuild unless you want:
                                // a->dirty = true; a->repathTimer = 0.f;
                            }
                            if (ImGui::Button("Snap Selected to This Entity")) {
                                if (selected.Has<Boom::TransformComponent>()) {
                                    a->path[a->waypoint] = selected.Get<Boom::TransformComponent>().transform.translate;
                                }
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Reverse Path")) {
                                std::reverse(a->path.begin(), a->path.end());
                                a->waypoint = (int)a->path.size() - 1 - a->waypoint;
                            }
                        }
                    }
                    else {
                        ImGui::TextDisabled("No path computed.");
                    }

                    // --- RUNTIME / DEBUG -----------------------------------------------------
                    ImGui::SeparatorText("Runtime");
                    {
                        float frac = 0.f;
                        if (a->repathCooldown > 0.f) frac = std::clamp(a->repathTimer / a->repathCooldown, 0.f, 1.f);
                        ImGui::ProgressBar(frac, ImVec2(-1, 0), "Repath Timer");

                        bool dirty = a->dirty;
                        if (ImGui::Checkbox("Dirty (force rebuild)", &dirty)) {
                            a->dirty = dirty;
                            if (dirty) a->repathTimer = 0.f;
                        }
                        int wp = a->waypoint;
                        if (ImGui::DragInt("Current Waypoint Index", &wp, 1, 0, (int)std::max<size_t>(1, a->path.size()) - 1)) {
                            a->waypoint = std::clamp(wp, 0, (int)std::max<size_t>(0, a->path.size() ? a->path.size() - 1 : 0));
                        }
                    }

                    // If you want your xproperty-driven editor to also show (for the subset you declared),
                    // return its meta-object here instead of nullptr. If your XPROPERTY_DEF exposes a getter like `xmeta()`,
                    // do: `return &NavAgentComponent::xmeta();`. Otherwise, keep nullptr and rely on the manual UI above.
                    return nullptr;
                },
                /*removable=*/true,
                [&]() { GetContext()->scene.remove<Boom::NavAgentComponent>(m_App->SelectedEntity()); }
            );
        }

        if (selected.Has<Boom::ColliderComponent>()) {
            ImGui::PushID("Collider");

            // 1. Draw Header
            bool isOpen = ImGui::CollapsingHeader("Collider", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);

            // 2. Draw "..." Button
            const ImVec2 headerMin = ImGui::GetItemRectMin();
            const ImVec2 headerMax = ImGui::GetItemRectMax();
            const float  lineH = ImGui::GetFrameHeight();
            const float y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
            if (ImGui::Button("...", ImVec2(lineH, lineH)))
                ImGui::OpenPopup("ColliderSettings");

            bool removed = false;
            if (ImGui::BeginPopup("ColliderSettings")) {
                if (ImGui::MenuItem("Remove Component")) {
                    removed = true;
                }
                ImGui::EndPopup();
            }

            // 3. Reset cursor
            ImGui::SetCursorScreenPos(ImVec2(headerMin.x, headerMax.y + ImGui::GetStyle().ItemSpacing.y));

            // 4. Draw Contents
            if (isOpen) {
                ImGui::Indent(12.0f);
                ImGui::Spacing();

                auto& col = selected.Get<Boom::ColliderComponent>();
                auto* collider = &col.Collider;

                // Store old values for change detection
                float oldDynamicFriction = col.Collider.dynamicFriction;
                float oldStaticFriction = col.Collider.staticFriction;
                float oldRestitution = col.Collider.restitution;
                glm::vec3 oldPos = collider->localPosition;
                glm::vec3 oldRot = collider->localRotation;
                glm::vec3 oldScale = collider->localScale;
                //bool oldIsTrigger = collider->isTrigger; // NEW

                // --- NEW: IS TRIGGER CHECKBOX ---
                ImGui::Spacing();
                ImGui::SeparatorText("Behavior");
                ImGui::Spacing();

                bool isTrigger = collider->isTrigger;
                if (ImGui::Checkbox("Is Trigger", &isTrigger)) {
                    collider->isTrigger = isTrigger;

                    // Update the physics shape immediately
                    if (collider->Shape) {
                        if (isTrigger) {
                            collider->Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                            collider->Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                        }
                        else {
                            collider->Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
                            collider->Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                        }
                    }
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Triggers do not produce collision response.\nThey only fire collision events.");
                }

                ImGui::Spacing();
                ImGui::SeparatorText("Shape");
                ImGui::Spacing();
                // --- END NEW SECTION ---

                Collider3D::Type currentType = col.Collider.type;
                const char* currentTypeName = "Unknown";
                switch (currentType)
                {
                case Collider3D::Type::BOX:     currentTypeName = "Box";     break;
                case Collider3D::Type::SPHERE:  currentTypeName = "Sphere";  break;
                case Collider3D::Type::CAPSULE: currentTypeName = "Capsule"; break;
                case Collider3D::Type::CONVEX_MESH:    currentTypeName = "Convex Mesh";    break;
				case Collider3D::Type::TRIANGLE_MESH:  currentTypeName = "Triangle Mesh";  break;
				case Collider3D::Type::CYLINDER:currentTypeName = "Cylinder";break;
				case Collider3D::Type::TRIANGLE:currentTypeName = "Triangle";break;
                case Collider3D::Type::PLANE:   currentTypeName = "Plane";   break;
                }

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Shape Type");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);

                if (ImGui::BeginCombo("##ColliderType", currentTypeName))
                {
                    const char* types[] = { "Box", "Sphere", "Capsule", "Convex Mesh", "Triangle Mesh", "Plane", "Cylinder", "Triangle" };                    for (int i = 0; i < IM_ARRAYSIZE(types); ++i) {
                        bool isSelected = (currentType == static_cast<Collider3D::Type>(i));
                        if (ImGui::Selectable(types[i], isSelected)) {
                            m_App->GetPhysicsContext().SetColliderType(selected, static_cast<Collider3D::Type>(i), m_App->GetAssetRegistry());
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                // Mesh asset picker (only for MESH type)
                if (currentType == Collider3D::Type::CONVEX_MESH || currentType == Collider3D::Type::TRIANGLE_MESH) {
                    ImGui::Spacing();
                    ImGui::Separator();

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Physics Mesh");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-1);

                    auto& assetRegistry = m_App->GetAssetRegistry();
                    auto& currentAsset = assetRegistry.Get<PhysicsMeshAsset>(col.Collider.physicsMeshID);
                    const char* currentName = (currentAsset.uid != EMPTY_ASSET) ? currentAsset.name.c_str() : "Select a mesh...";

                    if (ImGui::BeginCombo("##PhysicsMesh", currentName))
                    {
                        auto& map = assetRegistry.GetMap<PhysicsMeshAsset>();

                        bool isNoneSelected = (col.Collider.physicsMeshID == EMPTY_ASSET);
                        if (ImGui::Selectable("None", isNoneSelected))
                        {
                            col.Collider.physicsMeshID = EMPTY_ASSET;
                            m_App->GetPhysicsContext().UpdateColliderShape(selected, m_App->GetAssetRegistry());
                        }
                        if (isNoneSelected) ImGui::SetItemDefaultFocus();

                        // --- FIX START: Create a set to track displayed names ---
                        std::unordered_set<std::string> displayedNames;

                        for (auto& [uid, asset] : map)
                        {
                            if (uid == EMPTY_ASSET) continue;

                            // 1. Check if we have already displayed a mesh with this name
                            if (displayedNames.find(asset->name) != displayedNames.end()) {
                                continue; // Skip duplicates
                            }

                            // 2. Mark this name as displayed
                            displayedNames.insert(asset->name);

                            // 3. PushID ensures ImGui differentiates items even if names were identical 
                            // (though the set check above prevents that, this is good safety)
                            ImGui::PushID(static_cast<int>(uid));

                            bool isSelected = (col.Collider.physicsMeshID == uid);
                            if (ImGui::Selectable(asset->name.c_str(), isSelected))
                            {
                                col.Collider.physicsMeshID = uid;
                                m_App->GetPhysicsContext().UpdateColliderShape(selected, m_App->GetAssetRegistry());
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();

                            ImGui::PopID();
                        }
                        // --- FIX END ---

                        ImGui::EndCombo();
                    }
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                // Transform section
                ImGui::SeparatorText("Transform");
                ImGui::Spacing();

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Local Position");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##LocalPosition", &collider->localPosition.x, 0.01f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Local Rotation");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##LocalRotation", &collider->localRotation.x, 0.1f);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Local Scale");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat3("##LocalScale", &collider->localScale.x, 0.01f);
                collider->localScale = glm::max(collider->localScale, glm::vec3(0.01f));

                // Material section (only if NOT a trigger)
                if (!collider->isTrigger) {
                    ImGui::Spacing();
                    ImGui::SeparatorText("Material");
                    ImGui::Spacing();

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Dynamic Friction");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragFloat("##DynamicFriction", &collider->dynamicFriction, 0.01f, 0.0f, 1000.0f);

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Static Friction");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragFloat("##StaticFriction", &collider->staticFriction, 0.01f, 0.0f, 1000.0f);

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Restitution");
                    ImGui::SameLine(150);
                    ImGui::SetNextItemWidth(-1);
                    ImGui::DragFloat("##Restitution", &collider->restitution, 0.01f, 0.0f, 1000.0f);
                }
                else {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Material properties not used for triggers");
                    ImGui::Spacing();
                }

                // Apply changes if transform changed
                if (collider->localPosition != oldPos ||
                    collider->localRotation != oldRot ||
                    collider->localScale != oldScale)
                {
                    m_App->GetPhysicsContext().UpdateColliderShape(selected, m_App->GetAssetRegistry());
                }

                // Apply material changes if not a trigger
                if (!collider->isTrigger &&
                    (col.Collider.dynamicFriction != oldDynamicFriction ||
                        col.Collider.staticFriction != oldStaticFriction ||
                        col.Collider.restitution != oldRestitution))
                {
                    m_App->GetPhysicsContext().UpdatePhysicsMaterial(selected);
                }

                // Fit to Transform button
                ImGui::Spacing();
                if (ImGui::Button("Fit to Transform")) {
                    //auto& transform = selected.Get<TransformComponent>().transform;
                    auto& colli = selected.Get<ColliderComponent>().Collider;
                    
                    // Reset to match transform exactly
                    colli.localScale = glm::vec3(1.0f, 1.0f, 1.0f);
                    
                    // Update the physics shape
                    m_App->GetPhysicsContext().UpdateColliderShape(selected, 
                                                                     m_App->GetAssetRegistry());
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Reset collider to match the entity's transform scale");
                }

                ImGui::Spacing();
                ImGui::Unindent(12.0f);
            }
            ImGui::PopID();

            // CORRECTED
            if (removed) {
                // 1. Use the new smart cleanup function
                // This finds the actor attached to this specific collider shape and destroys it
                m_App->GetPhysicsContext().RemoveColliderActor(selected);

                // 2. Remove the component from ECS
                ctx->scene.remove<Boom::ColliderComponent>(m_App->SelectedEntity());
                return;
            }
            ImGui::Spacing();
        }

        if (selected.Has<DirectLightComponent>()) {
            auto& dl = selected.Get<DirectLightComponent>();
            DrawComponentSection(
                "Direct Light",
                &dl,
                [&](void* p) -> const xproperty::type::object*
                {
                    auto* comp = static_cast<DirectLightComponent*>(p);
                    ImGui::ColorEdit3("Irradiance", &comp->light.radiance[0]);

                    return GetDirectLightComponentProperties(p);
                },
                true,
                [&]() { ctx->scene.remove<DirectLightComponent>(m_App->SelectedEntity()); }
            );
        }

        if (selected.Has<PointLightComponent>()) {
            auto& pl = selected.Get<PointLightComponent>();

            DrawComponentSection(
                "Point Light",
                &pl,
                [&](void* p) -> const xproperty::type::object*
                {
                    auto* comp = static_cast<PointLightComponent*>(p);
                    ImGui::ColorEdit3("Irradiance", &comp->light.radiance[0]);

                    return GetPointLightComponentProperties(p);
                },
                true,
                [&]() { ctx->scene.remove<PointLightComponent>(m_App->SelectedEntity()); }
            );
        }

        if (selected.Has<SpotLightComponent>()) {
            auto& sl = selected.Get<SpotLightComponent>();
            DrawComponentSection(
                "Spot Light",
                &sl,
                [&](void* p) -> const xproperty::type::object*
                {
                    auto* comp = static_cast<SpotLightComponent*>(p);
                    ImGui::ColorEdit3("Irradiance", &comp->light.radiance[0]);

                    return GetSpotLightComponentProperties(p);
                },
                true,
                [&]() { ctx->scene.remove<SpotLightComponent>(m_App->SelectedEntity()); }
            );
        }

        if (selected.Has<SkyboxComponent>()) {
            ImGui::PushID("Skybox");
            if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap)) {
                if (ComponentSettings<Boom::SkyboxComponent>(ctx)) {
                    ImGui::PopID();
                    return; // Component was removed, exit early
                }
                auto& sky = selected.Get<SkyboxComponent>();

                ImGui::BeginTable("##maps", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV);
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
                InputAssetWidget<CONSTANTS::DND_PAYLOAD_SKYBOX>("skybox", sky.skyboxID);
                ImGui::EndTable();
            }
            ImGui::PopID();
        }

        if (selected.Has<Boom::MenuComponent>())
        {
            // 1. Get the ACTUAL component from the selected entity
            auto& realComp = selected.Get<Boom::MenuComponent>();

            DrawComponentSection(
                "Menu Tag",
                &realComp, // Pass the address of the REAL component

                // 2. The Draw Function (The 'void*' is the pointer we just passed)
                [](void* componentData)
                {
                    // Cast the generic pointer back to our component type
                    auto* comp = static_cast<Boom::MenuComponent*>(componentData);

                    // 3. Define the Enum Names for the Dropdown
                    // These must match the order of your 'enum class MenuType'
                    // Pause=0, Death=1, Settings=2, Main=3
                    const char* menuTypeNames[] = { "Pause", "Death", "Settings", "Main" };

                    // Convert current enum value to int for ImGui
                    int currentSelection = (int)comp->menuType;

                    // 4. Draw the Combo Box
                    // "Menu Type" is the label. 
                    // 'currentSelection' holds the index. 
                    // IM_ARRAYSIZE calculates the count (4).
                    if (ImGui::Combo("Menu Type", &currentSelection, menuTypeNames, IM_ARRAYSIZE(menuTypeNames)))
                    {
                        // If changed, cast the int back to the Enum and update the component
                        comp->menuType = (Boom::MenuType)currentSelection;
                    }
                },

                true, // Can remove?
                [&]() { ctx->scene.remove<Boom::MenuComponent>(m_App->SelectedEntity()); } // Remove Callback
            );
        }

        if (selected.Has<Boom::DeactivatedComponent>()) {
            static Boom::DeactivatedComponent fakeTagInstance;

            DrawComponentSection("Deactivated Tag", &fakeTagInstance, [](void*) { return nullptr; }, true,
                [&]() { ctx->scene.remove<Boom::DeactivatedComponent>(m_App->SelectedEntity()); });
        }

        if (selected.Has<Boom::ScriptComponent>()) {
            ImGui::PushID("Script");
            auto& sc = selected.Get<Boom::ScriptComponent>();

            // Collapsing header + settings
            bool isOpen = ImGui::CollapsingHeader("Script",
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap);

            // Settings button (remove)
            const ImVec2 headerMin = ImGui::GetItemRectMin();
            const ImVec2 headerMax = ImGui::GetItemRectMax();
            const float  lineH = ImGui::GetFrameHeight();
            const float  y = headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, y));
            if (ImGui::Button("...", ImVec2(lineH, lineH)))
                ImGui::OpenPopup("ScriptSettings");

            bool removed = false;
            if (ImGui::BeginPopup("ScriptSettings")) {
                if (ImGui::MenuItem("Remove Component"))
                    removed = true;
                ImGui::EndPopup();
            }

            // Reset cursor for contents
            ImGui::SetCursorScreenPos(ImVec2(headerMin.x,
                headerMax.y + ImGui::GetStyle().ItemSpacing.y));

            if (isOpen) {
                ImGui::Indent(12.0f);

                // ===== Context / scripting system pointer =====
                auto* appCtx = m_Owner ? m_Owner->GetContext() : nullptr;
                auto* scripting = (appCtx && appCtx->scriptingSystem)
                    ? appCtx->scriptingSystem.get()
                    : nullptr;
                entt::entity currentEntity = m_App->SelectedEntity();

                // *** AUTO-FIX: Automatically recreate dead script instances ***
                bool needsRecreation = false;
                if (sc.InstanceId == 0 && sc.Enabled && !sc.TypeName.empty() && scripting && scripting->IsAlive()) {
                    // Script should be alive but isn't - auto-recreate it
                    needsRecreation = true;
                    BOOM_INFO("[Inspector] Auto-recreating missing script instance for type: {}", sc.TypeName);
                }

                // Track if we need to recreate the instance this frame
                bool enabledChanged = false;
                bool typeNameChanged = false;

                // ----- Enabled toggle -----
                if (ImGui::Checkbox("Enabled", &sc.Enabled)) {
                    enabledChanged = true;
                }

                // ----- Type name DROPDOWN -----
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Type");
                ImGui::SameLine(150);
                ImGui::SetNextItemWidth(-1);

                std::vector<std::string> availableTypes;
                if (scripting && scripting->IsAlive()) {
                    availableTypes = scripting->GetAvailableScriptTypes();
                }

                const char* currentPreview = sc.TypeName.empty()
                    ? "None"
                    : sc.TypeName.c_str();

                if (ImGui::BeginCombo("##ScriptTypeDropdown", currentPreview)) {
                    // "None" option
                    bool isNoneSelected = sc.TypeName.empty();
                    if (ImGui::Selectable("None", isNoneSelected)) {
                        if (!sc.TypeName.empty()) {
                            sc.TypeName.clear();
                            typeNameChanged = true;
                        }
                    }
                    if (isNoneSelected)
                        ImGui::SetItemDefaultFocus();

                    // All available script types
                    for (const auto& typeName : availableTypes) {
                        bool isSelected = (sc.TypeName == typeName);
                        if (ImGui::Selectable(typeName.c_str(), isSelected)) {
                            if (sc.TypeName != typeName) {
                                sc.TypeName = typeName;
                                typeNameChanged = true;
                            }
                        }
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }

                // Auto-recreate instance if TypeName, Enabled changed, or manual fix requested
                if ((typeNameChanged || enabledChanged || needsRecreation) && scripting) {
                    scripting->RecreateForEntity(currentEntity, sc);
                    BOOM_INFO("[Inspector] Recreated script instance (Enabled={}, TypeName={})",
                        sc.Enabled, sc.TypeName);
                }

                // ----- Exposed Script Fields -----
                if (scripting && scripting->IsAlive() && sc.InstanceId != 0 && !sc.TypeName.empty()) {
                    auto exposedFields = scripting->GetExposedFields(sc.TypeName);

                    if (!exposedFields.empty()) {
                        ImGui::Separator();
                        ImGui::Text("Script Properties");
                        ImGui::Spacing();

                        for (const auto& field : exposedFields) {
                            ImGui::PushID(field.fieldName.c_str());

                            // Get current value
                            std::string valueJson = scripting->GetFieldValue(sc.InstanceId, field.fieldName);
                            bool valueChanged = false;

                            // Label
                            ImGui::AlignTextToFramePadding();
                            ImGui::Text("%s", field.displayName.c_str());
                            if (!field.tooltip.empty() && ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("%s", field.tooltip.c_str());
                            }
                            ImGui::SameLine(150);
                            ImGui::SetNextItemWidth(-1);

                            // Helper lambda to update both live instance AND Params for serialization
                            auto updateFieldValue = [&](const std::string& fieldName, const nlohmann::json& jsonValue) {
                                // Update Params for serialization (scene save/load) - always do this
                                sc.Params[fieldName] = jsonValue;

                                // Update live instance if it exists
                                if (sc.InstanceId != 0) {
                                    bool success = scripting->SetFieldValue(sc.InstanceId, fieldName, jsonValue.dump());
                                    if (!success) {
                                        BOOM_WARN("[Inspector] Failed to set field '{}' on live instance", fieldName);
                                    }
                                }
                            };

                            // Type-specific widget
                            if (field.typeName == "float") {
                                float val = 0.0f;
                                try { val = std::stof(valueJson); } catch (...) {}

                                if (field.useSlider && field.minValue > -FLT_MAX && field.maxValue < FLT_MAX) {
                                    if (ImGui::SliderFloat("##val", &val, field.minValue, field.maxValue)) {
                                        valueChanged = true;
                                    }
                                } else {
                                    if (ImGui::DragFloat("##val", &val, 0.1f, field.minValue, field.maxValue)) {
                                        valueChanged = true;
                                    }
                                }

                                if (valueChanged) {
                                    updateFieldValue(field.fieldName, val);
                                }
                            }
                            else if (field.typeName == "int") {
                                int val = 0;
                                try { val = std::stoi(valueJson); } catch (...) {}

                                if (field.useSlider && field.minValue > -FLT_MAX && field.maxValue < FLT_MAX) {
                                    if (ImGui::SliderInt("##val", &val, (int)field.minValue, (int)field.maxValue)) {
                                        valueChanged = true;
                                    }
                                } else {
                                    if (ImGui::DragInt("##val", &val)) {
                                        valueChanged = true;
                                    }
                                }

                                if (valueChanged) {
                                    updateFieldValue(field.fieldName, val);
                                }
                            }
                            else if (field.typeName == "bool") {
                                bool val = (valueJson == "true");
                                if (ImGui::Checkbox("##val", &val)) {
                                    updateFieldValue(field.fieldName, val);
                                }
                            }
                            else if (field.typeName == "string") {
                                // Parse string (remove quotes)
                                std::string val = valueJson;
                                if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
                                    val = val.substr(1, val.size() - 2);
                                }

                                // Check if this is an audio/sound field - use SoundComponent integration
                                bool isAudioField = (field.displayName.find("Sound") != std::string::npos ||
                                                    field.displayName.find("Audio") != std::string::npos ||
                                                    field.fieldName.find("Sound") != std::string::npos ||
                                                    field.fieldName.find("Audio") != std::string::npos ||
                                                    field.fieldName.find("sound") != std::string::npos ||
                                                    field.fieldName.find("audio") != std::string::npos);

                                if (isAudioField) {
                                    // Auto-create SoundComponent if it doesn't exist
                                    if (!selected.Has<Boom::SoundComponent>()) {
                                        ctx->scene.emplace<Boom::SoundComponent>(currentEntity);
                                    }
                                    auto& soundComp = selected.Get<Boom::SoundComponent>();

                                    // Find or create entry with the field name
                                    Boom::SoundComponent::Entry* audioEntry = nullptr;
                                    for (auto& entry : soundComp.entries) {
                                        if (entry.name == field.fieldName) {
                                            audioEntry = &entry;
                                            break;
                                        }
                                    }

                                    // Create new entry if not found
                                    if (!audioEntry) {
                                        Boom::SoundComponent::Entry newEntry{};
                                        newEntry.name = field.fieldName;
                                        soundComp.entries.push_back(std::move(newEntry));
                                        audioEntry = &soundComp.entries.back();
                                    }

                                    // Script's Params is the source of truth for the file path
                                    // Get the current value from Params (serialized) rather than live instance
                                    std::string currentAudioPath = val; // Default to live instance value
                                    if (sc.Params.contains(field.fieldName)) {
                                        auto& paramVal = sc.Params[field.fieldName];
                                        if (paramVal.is_string()) {
                                            currentAudioPath = paramVal.get<std::string>();
                                        }
                                    }

                                    // Sync SoundComponent entry FROM script Params (entry mirrors script)
                                    if (audioEntry->filePath != currentAudioPath) {
                                        audioEntry->filePath = currentAudioPath;
                                        if (audioEntry->filePaths.empty()) {
                                            if (!currentAudioPath.empty()) {
                                                audioEntry->filePaths.push_back(currentAudioPath);
                                            }
                                        } else {
                                            audioEntry->filePaths[0] = currentAudioPath;
                                        }
                                    }

                                    // ===== Display full SoundComponent Entry UI =====
                                    ImGui::PushID(field.fieldName.c_str());

                                    // Static map to track preview sounds and their file paths
                                    static std::unordered_map<std::string, std::string> s_previewFilePaths;

                                    // Generate unique preview instance name
                                    uint64_t entityUid = static_cast<uint64_t>(static_cast<uint32_t>(currentEntity));
                                    std::string previewName = "preview_" + std::to_string(entityUid) + "_" + field.fieldName;

                                    // Audio asset dropdown - use currentAudioPath (from Params) as the display value
                                    std::string curLabel = currentAudioPath.empty() ? "Select Audio..." : std::filesystem::path(currentAudioPath).filename().string();

                                    // Track if file changed for real-time update
                                    std::string previousFilePath = s_previewFilePaths[previewName];
                                    bool fileChanged = false;

                                    if (ImGui::BeginCombo("##audioSelect", curLabel.c_str())) {
                                        auto& audioMap = m_App->GetAssetRegistry().GetMap<AudioAsset>();

                                        // Allow clearing
                                        bool noneSel = currentAudioPath.empty();
                                        if (ImGui::Selectable("None", noneSel)) {
                                            // Update Params first (source of truth)
                                            sc.Params[field.fieldName] = "";
                                            // Then update entry to match
                                            audioEntry->filePath.clear();
                                            audioEntry->filePaths.clear();
                                            // Try to update live instance (may fail, but Params is already updated)
                                            if (sc.InstanceId != 0) {
                                                scripting->SetFieldValue(sc.InstanceId, field.fieldName, "\"\"");
                                            }
                                            fileChanged = true;
                                        }
                                        if (noneSel) ImGui::SetItemDefaultFocus();

                                        // List all audio assets
                                        for (auto& [uid, asset] : audioMap) {
                                            if (uid == EMPTY_ASSET) continue;
                                            bool isSel = (currentAudioPath == asset->source);
                                            if (ImGui::Selectable(asset->name.c_str(), isSel)) {
                                                // Update Params first (source of truth)
                                                sc.Params[field.fieldName] = asset->source;
                                                // Then update entry to match
                                                audioEntry->filePath = asset->source;
                                                if (audioEntry->filePaths.empty()) {
                                                    audioEntry->filePaths.push_back(asset->source);
                                                } else {
                                                    audioEntry->filePaths[0] = asset->source;
                                                }
                                                // Try to update live instance (may fail, but Params is already updated)
                                                if (sc.InstanceId != 0) {
                                                    std::string jsonStr = "\"" + asset->source + "\"";
                                                    scripting->SetFieldValue(sc.InstanceId, field.fieldName, jsonStr);
                                                }
                                                fileChanged = true;
                                            }
                                            if (isSel) ImGui::SetItemDefaultFocus();
                                        }
                                        ImGui::EndCombo();
                                    }

                                    // If file changed while preview is playing, restart with new file
                                    if (fileChanged && SoundEngine::Instance().IsPlaying(previewName)) {
                                        SoundEngine::Instance().StopSound(previewName);
                                        if (!audioEntry->filePath.empty()) {
                                            SoundEngine::Instance().PlaySound(previewName, audioEntry->filePath, audioEntry->loop);
                                            // Apply all current settings
                                            SoundEngine::Instance().SetVolume(previewName, audioEntry->mute ? 0.0f : audioEntry->volume);
                                            SoundEngine::Instance().SetPitch(previewName, audioEntry->pitch);
                                            SoundEngine::Instance().SetPan(previewName, audioEntry->stereoPan);
                                            SoundEngine::Instance().SetPriority(previewName, audioEntry->priority);
                                            SoundEngine::Instance().SetMute(previewName, audioEntry->mute);
                                        }
                                    }
                                    s_previewFilePaths[previewName] = audioEntry->filePath;

                                    // Show audio settings indented
                                    ImGui::Indent(10.0f);

                                    // ===== Preview Play/Stop buttons =====
                                    bool isPlaying = SoundEngine::Instance().IsPlaying(previewName);

                                    if (!audioEntry->filePath.empty()) {
                                        if (isPlaying) {
                                            if (ImGui::Button("Stop##preview")) {
                                                SoundEngine::Instance().StopSound(previewName);
                                            }
                                        } else {
                                            if (ImGui::Button("Play##preview")) {
                                                SoundEngine::Instance().PlaySound(previewName, audioEntry->filePath, audioEntry->loop);
                                                // Apply all current settings immediately
                                                SoundEngine::Instance().SetVolume(previewName, audioEntry->mute ? 0.0f : audioEntry->volume);
                                                SoundEngine::Instance().SetPitch(previewName, audioEntry->pitch);
                                                SoundEngine::Instance().SetPan(previewName, audioEntry->stereoPan);
                                                SoundEngine::Instance().SetPriority(previewName, audioEntry->priority);
                                                SoundEngine::Instance().SetMute(previewName, audioEntry->mute);
                                            }
                                        }
                                        ImGui::SameLine();
                                        if (isPlaying) {
                                            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Playing");
                                        }
                                    } else {
                                        ImGui::TextDisabled("Select audio to preview");
                                    }

                                    ImGui::Spacing();

                                    // Loop, Play On Start, Mute checkboxes
                                    bool loopChanged = ImGui::Checkbox("Loop##scriptAudio", &audioEntry->loop);
                                    ImGui::SameLine();
                                    ImGui::Checkbox("Play On Start##scriptAudio", &audioEntry->playOnStart);
                                    ImGui::SameLine();
                                    bool muteChanged = ImGui::Checkbox("Mute##scriptAudio", &audioEntry->mute);

                                    // Volume slider - apply in real-time
                                    bool volumeChanged = ImGui::SliderFloat("Volume##scriptAudio", &audioEntry->volume, 0.0f, 1.0f);

                                    // Pitch slider - apply in real-time
                                    bool pitchChanged = ImGui::SliderFloat("Pitch##scriptAudio", &audioEntry->pitch, 0.5f, 2.0f, "%.2f");
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::SetTooltip("Playback speed: 0.5 = half speed, 1.0 = normal, 2.0 = double speed");
                                    }

                                    // Priority slider - apply in real-time
                                    bool priorityChanged = ImGui::SliderInt("Priority##scriptAudio", &audioEntry->priority, 0, 256);
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::SetTooltip("Channel priority: 0 = highest, 256 = lowest (128 = default)");
                                    }

                                    // Stereo Pan slider - apply in real-time
                                    bool panChanged = ImGui::SliderFloat("Stereo Pan##scriptAudio", &audioEntry->stereoPan, -1.0f, 1.0f, "%.2f");
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::SetTooltip("-1.0 = full left, 0.0 = center, 1.0 = full right");
                                    }

                                    // Spatial Blend slider - apply in real-time
                                    bool spatialChanged = ImGui::SliderFloat("Spatial Blend##scriptAudio", &audioEntry->spatialBlend, 0.0f, 1.0f, "%.2f");
                                    if (ImGui::IsItemHovered()) {
                                        ImGui::SetTooltip("0.0 = fully 2D (no positional audio), 1.0 = fully 3D (positional)");
                                    }

                                    // Apply real-time changes to preview if playing
                                    if (isPlaying) {
                                        if (volumeChanged || muteChanged) {
                                            SoundEngine::Instance().SetVolume(previewName, audioEntry->mute ? 0.0f : audioEntry->volume);
                                        }
                                        if (pitchChanged) {
                                            SoundEngine::Instance().SetPitch(previewName, audioEntry->pitch);
                                        }
                                        if (priorityChanged) {
                                            SoundEngine::Instance().SetPriority(previewName, audioEntry->priority);
                                        }
                                        if (panChanged) {
                                            SoundEngine::Instance().SetPan(previewName, audioEntry->stereoPan);
                                        }
                                        if (spatialChanged) {
                                            SoundEngine::Instance().SetSpatialBlend(previewName, audioEntry->spatialBlend);
                                        }
                                        if (muteChanged) {
                                            SoundEngine::Instance().SetMute(previewName, audioEntry->mute);
                                        }
                                        if (loopChanged) {
                                            SoundEngine::Instance().SetLooping(previewName, audioEntry->loop);
                                        }
                                    }

                                    // 3D Audio Settings in collapsible section
                                    bool minDistChanged = false, maxDistChanged = false;
                                    if (ImGui::TreeNode("3D Audio##scriptAudio")) {
                                        minDistChanged = ImGui::SliderFloat("Min Distance##scriptAudio", &audioEntry->minDistance, 0.1f, 100.0f, "%.1f");
                                        if (ImGui::IsItemHovered()) {
                                            ImGui::SetTooltip("Distance at which sound is at full volume");
                                        }

                                        maxDistChanged = ImGui::SliderFloat("Max Distance##scriptAudio", &audioEntry->maxDistance, 1.0f, 200.0f, "%.1f");
                                        if (ImGui::IsItemHovered()) {
                                            ImGui::SetTooltip("Distance at which sound becomes silent");
                                        }

                                        // Validation
                                        if (audioEntry->minDistance >= audioEntry->maxDistance) {
                                            audioEntry->minDistance = audioEntry->maxDistance - 0.1f;
                                        }

                                        // Apply 3D distance changes in real-time
                                        if (isPlaying && (minDistChanged || maxDistChanged)) {
                                            SoundEngine::Instance().Set3DMinMaxDistance(previewName, audioEntry->minDistance, audioEntry->maxDistance);
                                        }

                                        // Quick presets
                                        ImGui::Text("Presets:");
                                        if (ImGui::SmallButton("Footsteps##scriptAudio")) {
                                            audioEntry->minDistance = 0.5f;
                                            audioEntry->maxDistance = 10.0f;
                                            if (isPlaying) {
                                                SoundEngine::Instance().Set3DMinMaxDistance(previewName, audioEntry->minDistance, audioEntry->maxDistance);
                                            }
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::SmallButton("Dialogue##scriptAudio")) {
                                            audioEntry->minDistance = 1.0f;
                                            audioEntry->maxDistance = 30.0f;
                                            if (isPlaying) {
                                                SoundEngine::Instance().Set3DMinMaxDistance(previewName, audioEntry->minDistance, audioEntry->maxDistance);
                                            }
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::SmallButton("Environment##scriptAudio")) {
                                            audioEntry->minDistance = 2.0f;
                                            audioEntry->maxDistance = 100.0f;
                                            if (isPlaying) {
                                                SoundEngine::Instance().Set3DMinMaxDistance(previewName, audioEntry->minDistance, audioEntry->maxDistance);
                                            }
                                        }

                                        ImGui::TreePop();
                                    }

                                    ImGui::Unindent(10.0f);
                                    ImGui::PopID();

                                } else {
                                    // Regular string input
                                    char buf[256];
#ifdef _MSC_VER
                                    strncpy_s(buf, sizeof(buf), val.c_str(), sizeof(buf) - 1);
#else
                                    std::snprintf(buf, sizeof(buf), "%s", val.c_str());
#endif
                                    if (ImGui::InputText("##val", buf, sizeof(buf))) {
                                        updateFieldValue(field.fieldName, std::string(buf));
                                    }
                                }
                            }
                            else if (field.typeName == "Vec3") {
                                float vals[3] = {0, 0, 0};
                                try {
                                    auto j = nlohmann::json::parse(valueJson);
                                    vals[0] = j.value("X", 0.0f);
                                    vals[1] = j.value("Y", 0.0f);
                                    vals[2] = j.value("Z", 0.0f);
                                } catch (...) {}

                                if (ImGui::DragFloat3("##val", vals, 0.1f)) {
                                    nlohmann::json vecJson = {{"X", vals[0]}, {"Y", vals[1]}, {"Z", vals[2]}};
                                    updateFieldValue(field.fieldName, vecJson);
                                }
                            }
                            else if (field.typeName == "Vec2") {
                                float vals[2] = {0, 0};
                                try {
                                    auto j = nlohmann::json::parse(valueJson);
                                    vals[0] = j.value("X", 0.0f);
                                    vals[1] = j.value("Y", 0.0f);
                                } catch (...) {}

                                if (ImGui::DragFloat2("##val", vals, 0.1f)) {
                                    nlohmann::json vecJson = {{"X", vals[0]}, {"Y", vals[1]}};
                                    updateFieldValue(field.fieldName, vecJson);
                                }
                            }
                            else if (field.typeName == "Vec4") {
                                float vals[4] = {0, 0, 0, 0};
                                try {
                                    auto j = nlohmann::json::parse(valueJson);
                                    vals[0] = j.value("X", 0.0f);
                                    vals[1] = j.value("Y", 0.0f);
                                    vals[2] = j.value("Z", 0.0f);
                                    vals[3] = j.value("W", 0.0f);
                                } catch (...) {}

                                if (ImGui::DragFloat4("##val", vals, 0.1f)) {
                                    nlohmann::json vecJson = {{"X", vals[0]}, {"Y", vals[1]}, {"Z", vals[2]}, {"W", vals[3]}};
                                    updateFieldValue(field.fieldName, vecJson);
                                }
                            }
                            else if (field.typeName == "ulong") {
                                // Entity reference - show as text for now
                                ImGui::TextDisabled("Entity: %s", valueJson.c_str());
                            }
                            else {
                                // Unknown type - show as read-only text
                                ImGui::TextDisabled("%s: %s", field.typeName.c_str(), valueJson.c_str());
                            }

                            ImGui::PopID();
                        }
                    }
                }

                // ----- Raw Params (JSON) - Collapsible for Advanced Users -----
                ImGui::Spacing();
                if (ImGui::TreeNode("Advanced: Raw Params (JSON)")) {
                    static char paramsBuf[2048];
                    static entt::entity lastJsonEntity = entt::null;

                    if (currentEntity != lastJsonEntity) {
                        std::string initial = sc.Params.dump(2);
#ifdef _MSC_VER
                        strncpy_s(paramsBuf, sizeof(paramsBuf), initial.c_str(), sizeof(paramsBuf) - 1);
#else
                        std::snprintf(paramsBuf, sizeof(paramsBuf), "%s", initial.c_str());
#endif
                        lastJsonEntity = currentEntity;
                    }

                    if (ImGui::InputTextMultiline(
                        "##ScriptParams",
                        paramsBuf,
                        IM_ARRAYSIZE(paramsBuf),
                        ImVec2(-1, 80),
                        ImGuiInputTextFlags_AllowTabInput))
                    {
                        try {
                            sc.Params = nlohmann::json::parse(paramsBuf);
                        }
                        catch (...) {
                            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Invalid JSON");
                        }
                    }
                    ImGui::TreePop();
                }

                // ----- Runtime info -----
                ImGui::Separator();
                ImGui::TextDisabled("Runtime Info");
                ImGui::Text("Instance ID: %llu", (unsigned long long)sc.InstanceId);

                // Enhanced status display
                if (sc.InstanceId != 0 && sc.Enabled) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "[OK] Active");
                }
                else if (sc.InstanceId == 0 && sc.Enabled && !sc.TypeName.empty()) {
                    // This is the problematic state - should be active but isn't
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "[!] Instance Missing");
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Script is enabled but has no instance.\n"
                            "This happens after exiting play mode.\n"
                            "Click 'Fix: Recreate Instance' or enter play mode.");
                    }
                }
                else if (sc.InstanceId == 0 && sc.Enabled && sc.TypeName.empty()) {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "[!] No Type Selected");
                }
                else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "[ ] Disabled");
                }

                // ----- Reload buttons -----
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Show scripting system status
                if (!scripting) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Scripting system not available");
                } else if (!scripting->IsAlive()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Scripting system not alive");
                    if (ImGui::Button("Initialize Scripting System", ImVec2(-1, 0))) {
                        // Try to reinitialize
                        BOOM_INFO("[Inspector] Attempting to reinitialize scripting system...");
                    }
                } else {
                    if (ImGui::Button("Reload This Script", ImVec2(-1, 0))) {
                        bool success = scripting->RecreateForEntity(currentEntity, sc);
                        if (success) {
                            BOOM_INFO("[Inspector] Manually reloaded script instance (InstanceId={})", sc.InstanceId);
                        } else {
                            BOOM_ERROR("[Inspector] Failed to reload script instance");
                        }
                    }

                    if (ImGui::Button("Hot Reload All Scripts (DLL)", ImVec2(-1, 0))) {
                        bool success = scripting->ReloadScripts();
                        if (success) {
                            BOOM_INFO("[Inspector] Hot reloaded all scripts from DLL!");
                        } else {
                            BOOM_ERROR("[Inspector] Hot reload failed!");
                        }
                    }
                }

                ImGui::Unindent(12.0f);
            }

            ImGui::PopID();

            if (removed) {
                auto* appCtx = m_Owner ? m_Owner->GetContext() : nullptr;
                auto* scripting = (appCtx && appCtx->scriptingSystem)
                    ? appCtx->scriptingSystem.get()
                    : nullptr;
                if (scripting) {
                    scripting->DestroyForEntity(m_App->SelectedEntity(), sc);
                }
                ctx->scene.remove<Boom::ScriptComponent>(m_App->SelectedEntity());
                return;
            }

            ImGui::Spacing();
        }

        // ===== Add Component =====
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
            ImGui::OpenPopup("AddComponentPopup");
        }
        ComponentSelector(selected);
    }

    void InspectorPanel::AssetUpdate() {
        m_App->ModifyAsset([&](auto* asset) {
            ImGui::Text("Modifying: %s (%d)", asset->name.c_str(), asset->uid);
            if (asset->type == AssetType::MATERIAL) {
                MaterialAsset* mat{ dynamic_cast<MaterialAsset*>(asset) };

                //TODO: showcase material as textured sphere
                //data variables (showcase texture name instead of map id)
                // toggle between mapID and standard slider (vec3/float)

                if (ImGui::CollapsingHeader("Maps", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::BeginTable("##maps", 6, ImGuiTableFlags_SizingFixedFit);
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("albedo map", mat->albedoMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("normal map", mat->normalMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("roughness map", mat->roughnessMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("metallic map", mat->metallicMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("occlusion map", mat->occlusionMapID);
                    InputAssetWidget<CONSTANTS::DND_PAYLOAD_TEXTURE>("emissive map", mat->emissiveMapID);
                    ImGui::EndTable();
                }

                if (ImGui::CollapsingHeader("Variables", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::DragFloat3("albedo", &mat->data.albedo[0], 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat3("emissive", &mat->data.emissive[0], 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("roughness", &mat->data.roughness, 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("metallic", &mat->data.metallic, 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::DragFloat("occlusion", &mat->data.occlusion, 0.01f, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                }
            }
            else if (asset->type == AssetType::TEXTURE) {
                TextureAsset* tex{ dynamic_cast<TextureAsset*>(asset) };
                ImGui::Image((ImTextureID)(*tex->data.get()), { 256, 256 });

                //data variables (these are settings for compression)
                if (ImGui::CollapsingHeader("Compression Settings:", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("Will Compress?", &tex->data->isCompileAsCompressed);
                    if (tex->data->isCompileAsCompressed) {
                        ImGui::SliderFloat("Quality", &tex->data->quality, 0.f, 1.f);
                        ImGui::SliderInt("Alpha Threshold", &tex->data->alphaThreshold, 0, 255);
                        ImGui::SliderInt("Mip Level", &tex->data->mipLevel, 1, 24);
                        ImGui::Checkbox("Gamma", &tex->data->isGamma);
                    }
                }
            }
            else if (asset->type == AssetType::MODEL) {
                ModelAsset* m{ dynamic_cast<ModelAsset*>(asset) };
                //TODO: showcase model without texture

                if (ImGui::CollapsingHeader("Model Offset", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::DragFloat3("Translate", &m->data->modelTransform.translate[0], 0.01f);
                    ImGui::DragFloat3("Rotation", &m->data->modelTransform.rotate[0], 1.f, 0.f, 360.f);
                    ImGui::DragFloat3("Scale", &m->data->modelTransform.scale[0], 0.01f, 0.01f);
                }
            }
            else {
                ImGui::Button("nothing here!");
            }
            });
    }

    void InspectorPanel::DeleteUpdate()
    {
        // 1. Only care if something is actually selected
        const bool hasSelection =
            (m_App->SelectedEntity() != entt::null) ||
            (m_App->SelectedAsset().id != 0u);

        if (!hasSelection)
            return;

        // 2. When Delete is pressed this frame, open the popup
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            ImGui::OpenPopup("Confirm Delete");
        }

        // 3. Center the popup when it *appears*
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Appearing,
            ImVec2(0.5f, 0.5f)
        );

        // 4. Draw the popup if it's open
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            AppInterface::AssetInfo info{};

            if (m_App->SelectedEntity() != entt::null)
            {
                Boom::Entity selectedEntity{ &m_App->GetEntityRegistry(), m_App->SelectedEntity() };
                info.name = selectedEntity.Get<Boom::InfoComponent>().name;
                info.id = selectedEntity.Get<Boom::InfoComponent>().uid;
                ImGui::Text("Are you sure you want to delete:\n%s?", info.name.c_str());
                ImGui::Separator();
                if (ImGui::Button("Yes", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
                    if (m_App->SelectedEntity() != entt::null) {

                        // === BEGIN PHYSICS CLEANUP ===

                        // Use ForceRemoveActor instead of RemoveRigidBody
                        m_App->GetPhysicsContext().ForceRemoveActor(static_cast<uint32_t>(m_App->SelectedEntity()));
                        // =======================

                        // === END PHYSICS CLEANUP ===

                        m_App->GetEntityRegistry().destroy(m_App->SelectedEntity());
                        m_App->ResetAllSelected();
                    }
                    else if (m_App->SelectedAsset().id != 0u) {
                        m_App->DeleteAsset(info.id, info.type);
                        m_App->ResetAllSelected();
                    }
                    showDeletePopup = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    showDeletePopup = false;
                    ImGui::CloseCurrentPopup();
                }
                
            }
            else if (m_App->SelectedAsset().id != 0u)
            {
                info = m_App->SelectedAsset();
            }

            ImGui::Text("Are you sure you want to delete:\n%s?", info.name.c_str());
            ImGui::Separator();

            // Yes / Enter
            if (ImGui::Button("Yes", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter, false))
            {
                if (m_App->SelectedEntity() != entt::null)
                {
                    // === PHYSICS CLEANUP ===
                    Boom::Entity entity{ &m_App->GetEntityRegistry(), m_App->SelectedEntity() };
                    if (auto* physicsCtx = &m_App->GetPhysicsContext())
                    {
                        physicsCtx->RemoveRigidBody(entity);
                    }

                    m_App->GetEntityRegistry().destroy(m_App->SelectedEntity());
                    m_App->ResetAllSelected();
                }
                else if (m_App->SelectedAsset().id != 0u)
                {
                    m_App->DeleteAsset(info.id, info.type);
                    m_App->ResetAllSelected();
                }

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            // No / Escape
            if (ImGui::Button("No", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }


    // === Animator-specific UpdateComponent specialization ===
    template<>
    void InspectorPanel::UpdateComponent<Boom::AnimatorComponent>(
        Boom::ComponentID id,
        Boom::Entity& selected
    )
    {
        if (!selected.Has<Boom::AnimatorComponent>() &&
            selected.Has<Boom::ModelComponent>())
        {
            auto& modelComp = selected.Get<Boom::ModelComponent>();
            auto& assets = m_App->GetAssetRegistry();

            if (modelComp.modelID != 0) {
                auto& modelAsset = assets.Get<Boom::ModelAsset>(modelComp.modelID);

                if (modelAsset.hasJoints) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID(static_cast<int>(id));

                    if (ImGui::Selectable(COMPONENT_NAMES[static_cast<size_t>(id)].data())) {
                        auto skeletalModel =
                            std::dynamic_pointer_cast<Boom::SkeletalModel>(modelAsset.data);
                        if (skeletalModel && skeletalModel->GetAnimator()) {
                            auto& animComp = selected.Attach<Boom::AnimatorComponent>();
                            animComp.animator = skeletalModel->GetAnimator()->Clone();
                            BOOM_INFO("Added AnimatorComponent");
                        }
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::PopID();
                }
            }
        }
    }

    template <>
    void InspectorPanel::UpdateComponent<Boom::ThirdPersonCameraComponent>(Boom::ComponentID id, Boom::Entity& selected) {

        // --- OUR CUSTOM LOGIC ---
        // Only show this component in the list if the entity
        // 1. Has a CameraComponent
        // 2. Does NOT already have a ThirdPersonCameraComponent
        //
        if (selected.Has<Boom::CameraComponent>() && !selected.Has<Boom::ThirdPersonCameraComponent>()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(id));
            if (ImGui::Selectable(COMPONENT_NAMES[static_cast<size_t>(id)].data())) {
                selected.Attach<Boom::ThirdPersonCameraComponent>();
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
    }

    void InspectorPanel::ComponentSelector(Boom::Entity& selected) {
        if (ImGui::BeginPopup("AddComponentPopup")) {
            ImGui::SetNextWindowSizeConstraints(ImVec2(300, 200), ImVec2(500, 600));

            ImGui::Text("Select component to add:");
            ImGui::Separator();
            if (ImGui::BeginChild("ComponentScrollArea", ImVec2(0, 250), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                if (ImGui::BeginTable("Component Table", 1, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
                    //commented out code are components that are incomplete (will crash when trying to add them/nothing to show in inspector)
                    UpdateComponent<Boom::InfoComponent>(Boom::ComponentID::INFO, selected);
                    UpdateComponent<Boom::TransformComponent>(Boom::ComponentID::TRANSFORM, selected);
                    UpdateComponent<Boom::CameraComponent>(Boom::ComponentID::CAMERA, selected);
                    UpdateComponent<Boom::RigidBodyComponent>(Boom::ComponentID::RIGIDBODY, selected);
                    UpdateComponent<Boom::ColliderComponent>(Boom::ComponentID::COLLIDER, selected);
                    UpdateComponent<Boom::ModelComponent>(Boom::ComponentID::MODEL, selected);
                    UpdateComponent<Boom::AnimatorComponent>(Boom::ComponentID::ANIMATOR, selected);
                    UpdateComponent<Boom::DirectLightComponent>(Boom::ComponentID::DIRECT_LIGHT, selected);
                    UpdateComponent<Boom::PointLightComponent>(Boom::ComponentID::POINT_LIGHT, selected);
                    UpdateComponent<Boom::SpotLightComponent>(Boom::ComponentID::SPOT_LIGHT, selected);
                    UpdateComponent<Boom::SoundComponent>(Boom::ComponentID::SOUND, selected);
                    UpdateComponent<Boom::ScriptComponent>(Boom::ComponentID::SCRIPT, selected);
                    UpdateComponent<Boom::NavAgentComponent>(Boom::ComponentID::NAV_AGENT_COMPONENT, selected);
                    UpdateComponent<Boom::AIComponent>(Boom::ComponentID::AI_COMPONENT, selected);
                    UpdateComponent<Boom::ThirdPersonCameraComponent>(Boom::ComponentID::THIRD_PERSON_CAMERA, selected);
					UpdateComponent<Boom::SpriteComponent>(Boom::ComponentID::SPRITE, selected);
                    UpdateComponent<Boom::MenuComponent>(Boom::ComponentID::MENU_COMPONENT, selected);
                    UpdateComponent<Boom::DeactivatedComponent>(Boom::ComponentID::DEACTIVATED_TAG, selected);
                    UpdateComponent<Boom::CharacterControllerComponent>(Boom::ComponentID::CHARACTER_CONTROLLER, selected);
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }

    void InspectorPanel::SnapEntity(Boom::Entity& entity, glm::vec3 direction)
    {
        Boom::AppContext* ctx = GetContext();
        if (!ctx || !entity.Has<Boom::TransformComponent>()) return;

        auto& tc = entity.Get<Boom::TransformComponent>();
        const float maxDistance = 100.0f;

        // Get entity's current world position
        glm::mat4 worldMatrix = Boom::GetWorldMatrix(ctx->scene, entity.ID());
        glm::vec3 entityWorldPos;
        glm::vec3 unused1, unused2;
        Boom::DecomposeMatrix(worldMatrix, entityWorldPos, unused1, unused2);

        // Calculate ray origin - start from entity center, offset slightly in opposite direction
        // to avoid self-intersection
        glm::vec3 rayOrigin = entityWorldPos - direction * 0.1f;
        glm::vec3 rayDir = glm::normalize(direction);

        // Get entity's own AABB for offset calculation
        glm::vec3 entityAABBMin, entityAABBMax;
        GetEntityAABB(entity, entityAABBMin, entityAABBMax);

        // Calculate entity half-size in the snap direction
        glm::vec3 entityHalfSize = (entityAABBMax - entityAABBMin) * 0.5f;
        float entityOffset = glm::abs(glm::dot(entityHalfSize, rayDir));

        // Try physics raycast first (works if actors exist)
        auto physResult = ctx->physics->Raycast(rayOrigin, rayDir, maxDistance);

        bool hitFound = false;
        glm::vec3 hitPoint;
        glm::vec3 hitNormal;
        entt::entity hitEntity = entt::null;

        if (physResult.hitFound && physResult.hitEntity != entity.ID()) {
            hitFound = true;
            hitPoint = physResult.position;
            hitNormal = physResult.normal;
            hitEntity = physResult.hitEntity;
        }

        // Fallback: Check against all entities with models or colliders (for edit mode)
        if (!hitFound) {
            float closestDist = maxDistance;

            auto view = ctx->scene.view<Boom::TransformComponent>();
            for (auto e : view) {
                if (e == entity.ID()) continue; // Skip self

                bool hasModel = ctx->scene.any_of<Boom::ModelComponent>(e);
                bool hasCollider = ctx->scene.any_of<Boom::ColliderComponent>(e);
                if (!hasModel && !hasCollider) continue;

                // Get target entity's AABB
                glm::vec3 targetMin, targetMax;
                GetEntityAABBForSnap(ctx, e, targetMin, targetMax);

                // Ray-AABB intersection
                float t;
                if (RayAABBIntersection(rayOrigin, rayDir, targetMin, targetMax, t)) {
                    if (t > 0.0f && t < closestDist) {
                        closestDist = t;
                        hitFound = true;
                        hitPoint = rayOrigin + rayDir * t;
                        hitEntity = e;

                        // Calculate approximate normal based on which face was hit
                        hitNormal = CalculateAABBHitNormal(hitPoint, targetMin, targetMax);
                    }
                }
            }
        }

        if (hitFound) {
            // Calculate new position: hit point + offset so entity sits on surface
            glm::vec3 newWorldPos = hitPoint - rayDir * entityOffset;

            // If entity has a parent, convert world position to local
            entt::entity parent = Boom::GetParentEntity(ctx->scene, entity.ID());
            if (parent != entt::null) {
                glm::mat4 parentWorld = Boom::GetWorldMatrix(ctx->scene, parent);
                glm::mat4 parentInverse = glm::inverse(parentWorld);
                glm::vec4 localPos = parentInverse * glm::vec4(newWorldPos, 1.0f);
                tc.transform.translate = glm::vec3(localPos);
            }
            else {
                tc.transform.translate = newWorldPos;
            }

            // Get hit entity name for logging
            std::string hitName = "Unknown";
            if (hitEntity != entt::null && ctx->scene.all_of<Boom::InfoComponent>(hitEntity)) {
                hitName = ctx->scene.get<Boom::InfoComponent>(hitEntity).name;
            }

            BOOM_INFO("[Snap] Snapped entity to '{}' (distance: {:.2f})", hitName, glm::distance(entityWorldPos, newWorldPos));
        }
        else {
            BOOM_WARN("[Snap] No surface found in direction ({:.1f}, {:.1f}, {:.1f})", direction.x, direction.y, direction.z);
        }
    }

    // Helper: Get AABB for any entity (model or collider)
    void InspectorPanel::GetEntityAABBForSnap(Boom::AppContext* ctx, entt::entity entity, glm::vec3& outMin, glm::vec3& outMax)
    {
        auto& tc = ctx->scene.get<Boom::TransformComponent>(entity);
        glm::mat4 worldMatrix = Boom::GetWorldMatrix(ctx->scene, entity);

        glm::vec3 localMin(-0.5f), localMax(0.5f); // Default 1x1x1 box

        // Try to get model bounds
        if (ctx->scene.any_of<Boom::ModelComponent>(entity)) {
            auto& mc = ctx->scene.get<Boom::ModelComponent>(entity);
            if (mc.modelID != EMPTY_ASSET) {
                auto* modelAsset = ctx->assets->TryGet<ModelAsset>(mc.modelID);
                if (modelAsset && modelAsset->data) {
                    auto staticModel = std::dynamic_pointer_cast<Boom::StaticModel>(modelAsset->data);
                    if (staticModel) {
                        const auto& meshData = staticModel->GetMeshData();
                        if (!meshData.empty()) {
                            localMin = glm::vec3(FLT_MAX);
                            localMax = glm::vec3(-FLT_MAX);
                            for (const auto& mesh : meshData) {
                                for (const auto& vertex : mesh.vtx) {
                                    localMin = glm::min(localMin, vertex.pos);
                                    localMax = glm::max(localMax, vertex.pos);
                                }
                            }
                        }
                    }
                }
            }
        }
        // Or use collider bounds
        else if (ctx->scene.any_of<Boom::ColliderComponent>(entity)) {
            auto& cc = ctx->scene.get<Boom::ColliderComponent>(entity);
            glm::vec3 halfSize = cc.Collider.localScale * 0.5f;
            localMin = cc.Collider.localPosition - halfSize;
            localMax = cc.Collider.localPosition + halfSize;
        }

        // Transform corners to world space and find AABB
        std::vector<glm::vec3> corners = {
            glm::vec3(localMin.x, localMin.y, localMin.z),
            glm::vec3(localMax.x, localMin.y, localMin.z),
            glm::vec3(localMin.x, localMax.y, localMin.z),
            glm::vec3(localMax.x, localMax.y, localMin.z),
            glm::vec3(localMin.x, localMin.y, localMax.z),
            glm::vec3(localMax.x, localMin.y, localMax.z),
            glm::vec3(localMin.x, localMax.y, localMax.z),
            glm::vec3(localMax.x, localMax.y, localMax.z)
        };

        outMin = glm::vec3(FLT_MAX);
        outMax = glm::vec3(-FLT_MAX);
        for (const auto& corner : corners) {
            glm::vec3 worldCorner = glm::vec3(worldMatrix * glm::vec4(corner, 1.0f));
            outMin = glm::min(outMin, worldCorner);
            outMax = glm::max(outMax, worldCorner);
        }
    }

    void InspectorPanel::GetEntityAABB(Boom::Entity& entity, glm::vec3& outMin, glm::vec3& outMax)
    {
        Boom::AppContext* ctx = GetContext();
        if (!ctx) {
            outMin = outMax = glm::vec3(0.0f);
            return;
        }
        GetEntityAABBForSnap(ctx, entity.ID(), outMin, outMax);
    }

    bool InspectorPanel::RayAABBIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
        const glm::vec3& aabbMin, const glm::vec3& aabbMax, float& t)
    {
        glm::vec3 invDir = 1.0f / rayDir;
        glm::vec3 t1 = (aabbMin - rayOrigin) * invDir;
        glm::vec3 t2 = (aabbMax - rayOrigin) * invDir;

        glm::vec3 tmin = glm::min(t1, t2);
        glm::vec3 tmax = glm::max(t1, t2);

        float tmin_val = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
        float tmax_val = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

        if (tmax_val >= tmin_val && tmax_val >= 0) {
            t = tmin_val > 0 ? tmin_val : tmax_val;
            return true;
        }
        return false;
    }

    glm::vec3 InspectorPanel::CalculateAABBHitNormal(const glm::vec3& hitPoint, const glm::vec3& aabbMin, const glm::vec3& aabbMax)
    {
        const float epsilon = 0.001f;
        glm::vec3 center = (aabbMin + aabbMax) * 0.5f;
        glm::vec3 halfSize = (aabbMax - aabbMin) * 0.5f;
        glm::vec3 localHit = hitPoint - center;

        // Find which face the hit is closest to
        glm::vec3 d = glm::abs(localHit) - halfSize;

        if (d.x > d.y && d.x > d.z) {
            return glm::vec3(localHit.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f);
        }
        else if (d.y > d.z) {
            return glm::vec3(0.0f, localHit.y > 0 ? 1.0f : -1.0f, 0.0f);
        }
        else {
            return glm::vec3(0.0f, 0.0f, localHit.z > 0 ? 1.0f : -1.0f);
        }
    }


    template <class Type>
    void InspectorPanel::UpdateComponent(Boom::ComponentID id, Boom::Entity& selected) {
        if (!selected.Has<Type>()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(static_cast<int>(id));
            if (ImGui::Selectable(COMPONENT_NAMES[static_cast<size_t>(id)].data())) {

                if constexpr (std::is_same_v<Type, Boom::ColliderComponent>) {
                    // ADD THE COLLIDER (works with or without rigidbody now)
                    selected.Attach<Type>();

                    // If there's a rigidbody, use the existing path
                    if (selected.Has<Boom::RigidBodyComponent>()) {
                        m_App->GetPhysicsContext().AddRigidBody(selected, m_App->GetAssetRegistry());
                    }
                    else {
                        // No rigidbody - create a collider-only entity
                        m_App->GetPhysicsContext().AddColliderOnly(selected, m_App->GetAssetRegistry());
                    }

                    ImGui::CloseCurrentPopup();
                }
                else {
                    // This is not a collider, add it normally
                    selected.Attach<Type>();
                    if constexpr (std::is_same_v<Type, Boom::RigidBodyComponent>) {
                        m_App->GetPhysicsContext().AddRigidBody(selected, m_App->GetAssetRegistry());
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::PopID();
        }
    }

    template <class CType>
    bool InspectorPanel::ComponentSettings(Boom::AppContext* ctx) {
        const ImVec2 headerMin = ImGui::GetItemRectMin();
        const ImVec2 headerMax = ImGui::GetItemRectMax();
        const float  lineH = ImGui::GetFrameHeight();
        ImGui::SetCursorScreenPos(ImVec2(headerMax.x - lineH, headerMin.y + (headerMax.y - headerMin.y - lineH) * 0.5f));
        if (ImGui::Button("...", ImVec2(lineH, lineH)))
            ImGui::OpenPopup("ComponentSettings");
        bool removed = false;
        if (ImGui::BeginPopup("ComponentSettings")) {
            if (ImGui::MenuItem("Remove Component")) {
                ctx->scene.remove<CType>(m_App->SelectedEntity());
                removed = true;
            }
            ImGui::EndPopup();
        }
        return removed;
    }

    void InspectorPanel::AcceptIDDrop(uint64_t& data, char const* payloadType) {
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType))
            {
                IM_ASSERT(payload->DataSize == sizeof(AssetID));
                data = *(AssetID const*)payload->Data;
                ImGui::Text("Dropped ID: %llu", data);
            }
            ImGui::EndDragDropTarget();
        }
    }

    template <std::string_view const& Payload>
    void InspectorPanel::InputAssetWidget(char const* label, uint64_t& data) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        ImGui::TableSetColumnIndex(1);
        ImVec2 const fieldSize{ ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() };
        ImGui::PushID(label);

        using AssetType = typename PayloadToType<Payload>::Type;
        if (ImGui::Button(m_App->GetAssetName<AssetType>(data).data(), fieldSize)) {
            //TODO: clicking button opens asset picker window
        }
        AcceptIDDrop(data, Payload.data());
        ImGui::PopID();
    }

} // namespace EditorUI
