#include "Panels/ResourcePanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "Vendors/imgui/imgui.h"
#include "Auxiliaries/Assets.h"
#include "Graphics/Textures/Texture.h"
#include "Graphics/Textures/Compression.h"

#include <filesystem>
#include <future>

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
					if (asset->name.find(searchBuff) == std::string::npos) 
						return;

					ImGui::TableNextColumn();
					ImTextureID texid{ m_Icon }; //default file icon
					TextureAsset* tex{ dynamic_cast<TextureAsset*>(asset) };
					if (tex) texid = *tex->data.get();

					ImGui::PushID((int)asset->uid);
					bool isClicked = ImGui::ImageButton("##thumb", texid, ImVec2(ASSET_SIZE, ASSET_SIZE),
						ImVec2(0, 0), ImVec2(1, 1),
						ImVec4(0, 0, 0, 1),
						ImVec4(1, 1, 1, 1));

					if (tex && ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_TEXTURE.data(), &asset->uid, sizeof(AssetID));
						ImGui::Text("Dragging Texture: %s", asset->name.c_str());
						ImGui::EndDragDropSource();
					}
					else if (dynamic_cast<MaterialAsset*>(asset) && ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_MATERIAL.data(), &asset->uid, sizeof(AssetID));
						ImGui::Text("Dragging Material: %s", asset->name.c_str());
						ImGui::EndDragDropSource();
					}
					else if (dynamic_cast<ModelAsset*>(asset) && ImGui::BeginDragDropSource()) {
						ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_MODEL.data(), &asset->uid, sizeof(AssetID));
						ImGui::Text("Dragging Model: %s", asset->name.c_str());
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

} // namespace EditorUI
