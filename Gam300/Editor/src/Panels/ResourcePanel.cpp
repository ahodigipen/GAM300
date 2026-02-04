#include "Panels/ResourcePanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "Vendors/imgui/imgui.h"
#include "Auxiliaries/Assets.h"
#include "Graphics/Textures/Texture.h"
#include "Graphics/Textures/Compression.h"
#include "Graphics/Renderer.h"

#include <filesystem>
#include <future>
#include <algorithm>
#include <cctype>

#ifndef ICON_FA_IMAGE
#define ICON_FA_IMAGE ""
#endif

namespace EditorUI {

    ResourcePanel::ResourcePanel(Editor* owner)
        : m_Owner(owner)
    {
        DEBUG_DLL_BOUNDARY("ResourcePanel::Ctor");
        if (!m_Owner) { BOOM_ERROR("ResourcePanel - null owner"); return; }
        m_Ctx = m_Owner->GetContext();
        DEBUG_POINTER(m_Ctx, "AppContext");
		m_App = dynamic_cast<Boom::AppInterface*>(owner);
		DEBUG_POINTER(m_App, "AppInterface");
		m_Icon = m_App->GetTexIDFromPath("Resources/Textures/Icons/asset.png");
		m_ModelIcon = m_App->GetTexIDFromPath("Resources/Textures/Icons/model.png");
		m_MaterialIcon = m_App->GetTexIDFromPath("Resources/Textures/Icons/material.png");
		m_ScriptIcon = m_App->GetTexIDFromPath("Resources/Textures/Icons/script.png");
    }

    void ResourcePanel::OnShow()
    {
        if (!ImGui::Begin("Resources")) { ImGui::End(); return; }

		if (ImGui::Button("Save All Assets", { 128, 20 })) {
			m_App->SaveAssets();
		}

		ImGui::SameLine();
		if (ImGui::Button("Create Empty Material", { 160, 20 })) {
			showNamePopup = true;
		}
		if (showNamePopup) {
			ImGui::OpenPopup("Input Material Name");
			ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			CreateEmptyMaterial();
		}

		ImGui::SameLine();
		static bool isCompressionStarted{};
		static std::future<void> g_CompressFuture;
		static float compressionTimeElapsed{};
		if (ImGui::Button("Compress Textures", { 160, 20 }) && !isCompressionStarted) {
			auto textureMap = m_App->GetAssetRegistry().GetMap<TextureAsset>();
			g_CompressFuture = std::async(std::launch::async, [copy = std::move(textureMap)]() mutable {
				CompressAllTextures(copy, CONSTANTS::COMPRESSED_TEXTURE_OUTPUT_PATH);
			});
			isCompressionStarted = true;
			compressionTimeElapsed = 0.f;
		}
		if (isCompressionStarted) {
			if (g_CompressFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
				try { g_CompressFuture.get(); }
				catch (std::exception const& e) { 
					char const* dodo{ e.what() }; 
					BOOM_ERROR("{}", dodo);
				}
				isCompressionStarted = false;
			}
			else {
				compressionTimeElapsed += (float)m_App->GetDeltaTime();
				ImGui::Text("Time elapsed: %.3f", compressionTimeElapsed);
			}
		}
		

		static int currentType{ static_cast<int>(AssetType::UNKNOWN) }; //unknown will show all assets
		ImGui::Combo("Filter", &currentType, TYPE_NAMES, IM_ARRAYSIZE(TYPE_NAMES));

		ImGui::SameLine();
		static char searchBuff[CONSTANTS::CHAR_BUFFER_SIZE] = "";
		ImGui::InputTextWithHint("##", "search", searchBuff, CONSTANTS::CHAR_BUFFER_SIZE);

		// Delete hotkey - only when popup is NOT open, window is focused, and no text input is active
		if (!m_ShowDeletePopup && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
				auto selectedAsset = m_App->SelectedAsset();
				if (selectedAsset.id != EMPTY_ASSET) {
					// Find the asset to get its path
					m_App->AssetView([&](Asset* asset) {
						if (asset->uid == selectedAsset.id) {
							m_AssetToDeleteUID = asset->uid;
							m_AssetToDeleteType = static_cast<uint8_t>(asset->type);
							m_AssetToDeleteName = asset->name;
							m_AssetToDeletePath = asset->source;
							m_ShowDeletePopup = true;
						}
					});
				}
			}
		}

		int32_t colNo{ (int32_t)((ImGui::GetContentRegionAvail().x) / (ASSET_SIZE + ImGui::GetStyle().ItemSpacing.x)) };
		colNo = glm::max(1, colNo);

		ImGuiTableFlags flags{
			ImGuiTableFlags_SizingFixedSame |
			ImGuiTableFlags_NoHostExtendX
		};
		if (ImGui::BeginTable("", colNo, flags)) {
			// set column sizes according to paddings
			for (int i{}; i < colNo; ++i) {
				ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ASSET_SIZE);
			}

			m_App->AssetView(
				[&](Asset* asset) {
					// filters
					if (static_cast<AssetType>(currentType) != AssetType::UNKNOWN && asset->type != static_cast<AssetType>(currentType))
						return;

					// Case-insensitive search comparison
					std::string searchLower(searchBuff);
					std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(),
						[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					std::string nameLower = asset->name;
					std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
						[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
					if (nameLower.find(searchLower) == std::string::npos)
						return;

					ImGui::TableNextColumn();
					ImTextureID texid{ m_Icon }; //default file icon

					// change icon based on asset type
					MaterialAsset* matAsset = dynamic_cast<MaterialAsset*>(asset);
					if (matAsset) {
						// Try to get material preview, fall back to icon if unavailable
						uint32_t previewTex = GetMaterialPreviewTexture(matAsset);
						texid = (previewTex != 0) ? (ImTextureID)(intptr_t)previewTex : m_MaterialIcon;
					}
					else if (dynamic_cast<ModelAsset*>(asset)) texid = m_ModelIcon;
					else if (dynamic_cast<ScriptAsset*>(asset)) texid = m_ScriptIcon;

					TextureAsset* tex{ dynamic_cast<TextureAsset*>(asset) };
					if (tex) texid = *tex->data.get();

					ImGui::PushID((int)asset->uid);
					// Material previews need flipped UVs (OpenGL origin is bottom-left, ImGui expects top-left)
					ImVec2 uv0 = (matAsset && texid != m_MaterialIcon) ? ImVec2(0, 1) : ImVec2(0, 0);
					ImVec2 uv1 = (matAsset && texid != m_MaterialIcon) ? ImVec2(1, 0) : ImVec2(1, 1);
					bool isClicked = ImGui::ImageButton("##thumb", texid, ImVec2(ASSET_SIZE, ASSET_SIZE),
						uv0, uv1,
						ImVec4(0, 0, 0, 1),
						ImVec4(1, 1, 1, 1));

					if (tex && ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_TEXTURE.data(), &asset->uid, sizeof(AssetID));
						ImGui::Text("Dragging Texture: %s", asset->name.c_str());
						ImGui::EndDragDropSource();
					}
					else if (matAsset && ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_MATERIAL.data(), &asset->uid, sizeof(AssetID));
						ImGui::Text("Dragging Material: %s", asset->name.c_str());
						ImGui::EndDragDropSource();
					}
					else if (dynamic_cast<ModelAsset*>(asset) && ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_MODEL.data(), &asset->uid, sizeof(AssetID));
						ImGui::Text("Dragging Model: %s", asset->name.c_str());
						ImGui::EndDragDropSource();
					}
					else if (dynamic_cast<SkyboxAsset*>(asset) && ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_SKYBOX.data(), &asset->uid, sizeof(AssetID));
						ImGui::Text("Dragging Skybox: %s", asset->name.c_str());
						ImGui::EndDragDropSource();
					}
					else if (dynamic_cast<AnimationAsset*>(asset) && ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_ANIMATION.data(), &asset->uid, sizeof(AssetID));
						ImGui::Text("Dragging Animation: %s", asset->name.c_str());
						ImGui::EndDragDropSource();
					}

					// Right-click context menu for asset deletion
					if (ImGui::BeginPopupContextItem()) {
						if (ImGui::MenuItem("Delete")) {
							m_AssetToDeleteUID = asset->uid;
							m_AssetToDeleteType = static_cast<uint8_t>(asset->type);
							m_AssetToDeleteName = asset->name;
							m_AssetToDeletePath = asset->source;
							m_ShowDeletePopup = true;
						}
						ImGui::EndPopup();
					}

					ImGui::PopID();

					std::filesystem::path aPath{ asset->source };
					ImGui::TextWrapped(aPath.filename().string().c_str());

					//show modifyable properties in inspector when selected
					if (isClicked) {
						m_App->SelectedAsset(true) = { asset->uid, asset->type, asset->name };
					}
				}
			);
			ImGui::EndTable();
		}

		// Delete confirmation popup
		if (m_ShowDeletePopup) {
			ImGui::OpenPopup("Delete Asset?");
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

		if (ImGui::BeginPopupModal("Delete Asset?", &m_ShowDeletePopup,
								   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
			ImGui::Text("Are you sure you want to delete:");
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "  %s", m_AssetToDeleteName.c_str());
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Path: %s", m_AssetToDeletePath.c_str());
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "This will permanently delete the file from disk!");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
				// Clear selection if deleted asset was selected
				if (m_App->SelectedAsset().id == m_AssetToDeleteUID) {
					m_App->SelectedAsset(true) = {};
				}

				// Delete the physical file from disk
				std::error_code ec;
				if (std::filesystem::exists(m_AssetToDeletePath)) {
					if (std::filesystem::remove(m_AssetToDeletePath, ec)) {
						BOOM_INFO("[ResourcePanel] Deleted file from disk: {}", m_AssetToDeletePath);
					} else {
						BOOM_ERROR("[ResourcePanel] Failed to delete file: {} - {}", m_AssetToDeletePath, ec.message());
					}
				}

				// Remove from asset registry based on type
				AssetType assetType = static_cast<AssetType>(m_AssetToDeleteType);
				auto& assetRegistry = m_App->GetAssetRegistry();
				bool removed = false;

				switch (assetType) {
					case AssetType::MATERIAL:
						removed = assetRegistry.Remove<MaterialAsset>(m_AssetToDeleteUID);
						break;
					case AssetType::TEXTURE:
						removed = assetRegistry.Remove<TextureAsset>(m_AssetToDeleteUID);
						break;
					case AssetType::SKYBOX:
						removed = assetRegistry.Remove<SkyboxAsset>(m_AssetToDeleteUID);
						break;
					case AssetType::SCRIPT:
						removed = assetRegistry.Remove<ScriptAsset>(m_AssetToDeleteUID);
						break;
					case AssetType::SCENE:
						removed = assetRegistry.Remove<SceneAsset>(m_AssetToDeleteUID);
						break;
					case AssetType::MODEL:
						removed = assetRegistry.Remove<ModelAsset>(m_AssetToDeleteUID);
						break;
					case AssetType::PHYSICS_MESH:
						removed = assetRegistry.Remove<PhysicsMeshAsset>(m_AssetToDeleteUID);
						break;
					case AssetType::PREFAB:
						removed = assetRegistry.Remove<PrefabAsset>(m_AssetToDeleteUID);
						break;
					case AssetType::AUDIO:
						removed = assetRegistry.Remove<AudioAsset>(m_AssetToDeleteUID);
						break;
					case AssetType::ANIMATION:
						removed = assetRegistry.Remove<AnimationAsset>(m_AssetToDeleteUID);
						break;
					default:
						BOOM_WARN("[ResourcePanel] Unknown asset type for deletion");
						break;
				}

				if (removed) {
					BOOM_INFO("[ResourcePanel] Removed asset '{}' from registry", m_AssetToDeleteName);
				}

				m_ShowDeletePopup = false;
				m_AssetToDeleteUID = 0;
				m_AssetToDeleteType = 0;
				m_AssetToDeleteName.clear();
				m_AssetToDeletePath.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
				m_ShowDeletePopup = false;
				m_AssetToDeleteUID = 0;
				m_AssetToDeleteType = 0;
				m_AssetToDeleteName.clear();
				m_AssetToDeletePath.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// Reset state if popup was closed via X button
		if (!m_ShowDeletePopup && m_AssetToDeleteUID != 0) {
			m_AssetToDeleteUID = 0;
			m_AssetToDeleteType = 0;
			m_AssetToDeleteName.clear();
			m_AssetToDeletePath.clear();
		}

        ImGui::End();
    }

	void ResourcePanel::CreateEmptyMaterial() {
		static char buff[CONSTANTS::CHAR_BUFFER_SIZE] = "";

		if (ImGui::BeginPopupModal("Input Material Name", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::InputTextWithHint("##", NEW_MATERIAL_NAME, buff, CONSTANTS::CHAR_BUFFER_SIZE);
			ImGui::Separator();

			if (ImGui::Button("OK", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter, false)) { //create material operation
				std::string name{ buff };
				if (name.empty()) name = NEW_MATERIAL_NAME;
				HandleConflictName(name);
				m_App->GetAssetRegistry().AddMaterial(RandomU64(), name);
				showNamePopup = false;
				ImGui::CloseCurrentPopup();
				memset(buff, 0, sizeof(buff));
			}
			ImGui::SameLine();
			if (ImGui::Button("Close", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) { //cancel operation
				showNamePopup = false;
				ImGui::CloseCurrentPopup();
				memset(buff, 0, sizeof(buff));
			}
			ImGui::EndPopup();
		}
	}

	void ResourcePanel::HandleConflictName(std::string& name) {
		int counter{ 1 };
		bool duplicateName{ true };
		std::string baseName{ name };
		while (duplicateName) {
			duplicateName = false;
			m_App->AssetTypeView<MaterialAsset>([&baseName, &name, &counter, &duplicateName](MaterialAsset* mat) {
				if (mat->name == name) {
					baseName = name + " (" + std::to_string(counter) + ")";
					++counter;
					duplicateName = true;
					return;
				}
				});
		}
		name = baseName;
	}

	uint32_t ResourcePanel::GetMaterialPreviewTexture(Boom::MaterialAsset* mat) {
		if (!m_Ctx || !m_Ctx->renderer || !m_Ctx->assets) return 0;

		// Initialize material preview system if not already done
		if (!m_Ctx->renderer->IsMaterialPreviewInitialized()) {
			// Find sphere model in assets
			Boom::Model3D sphereModel;
			auto& modelMap = m_Ctx->assets->GetMap<Boom::ModelAsset>();
			for (auto& [assetID, assetPtr] : modelMap) {
				auto* modelAsset = dynamic_cast<Boom::ModelAsset*>(assetPtr.get());
				if (modelAsset && modelAsset->source.find("sphere.fbx") != std::string::npos) {
					sphereModel = modelAsset->data;
					break;
				}
			}
			if (sphereModel) {
				m_Ctx->renderer->InitMaterialPreview(sphereModel);
			} else {
				return 0; // Can't initialize without sphere model
			}
		}

		// Resolve texture IDs to actual texture pointers
		m_Ctx->assets->ResolveMaterialTextures(mat);

		// Render and return the cached preview texture
		return m_Ctx->renderer->RenderMaterialPreviewCached(
			mat->uid, mat->data, PREVIEW_YAW, PREVIEW_PITCH, PREVIEW_DISTANCE);
	}
} // namespace EditorUI
