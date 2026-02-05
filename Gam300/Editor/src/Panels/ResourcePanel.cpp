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
