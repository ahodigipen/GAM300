#include "Panels/DirectoryPanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "Vendors/imgui/imgui.h"
#include "Graphics/Textures/Texture.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <future>

// pull Interface so we can call AppInterface methods
#include "Application/Interface.h"   // include path per your include dirs

namespace EditorUI {

    struct DirectoryPanel::FileNode {
        std::string name;
        bool isDirectory;
        std::vector<std::unique_ptr<FileNode>> children;
        std::filesystem::path fullPath;
        unsigned int texId{};
        bool isHovered{};
        FileNode(const std::string& n, bool dir, const std::filesystem::path& p, unsigned id = 0)
            : name(n), isDirectory(dir), fullPath(p), texId(id) {
        }
    };
    DirectoryPanel::~DirectoryPanel() = default;
    DirectoryPanel::DirectoryPanel(Editor* owner)
        : m_Owner(owner)
    {
        DEBUG_DLL_BOUNDARY("DirectoryPanel::Ctor");
        if (!m_Owner) { BOOM_ERROR("DirectoryPanel - null owner"); return; }

        m_App = dynamic_cast<Boom::AppInterface*>(owner);
        DEBUG_POINTER(m_App, "AppInterface");

        // Get context through the AppInterface
        if (m_App) {
            m_Ctx = owner->GetContext();
            DEBUG_POINTER(m_Ctx, "AppContext");
        }

        folderIcon = m_App->GetTexIDFromPath("Resources/Textures/Icons/folder.png");
        assetIcon = m_App->GetTexIDFromPath("Resources/Textures/Icons/asset.png");
    }

    void DirectoryPanel::Init()
    {
        rootNode = BuildDirectoryTree();

        // Use the Interface API to wire the drop callback
        if (m_App) {
            if (auto wh = m_App->GetWindowHandle())
                glfwSetDropCallback(wh.get(), OnDrop);
        }
        else if (m_Ctx && m_Ctx->window) { // fallback
            glfwSetDropCallback(m_Ctx->window->Handle().get(), OnDrop);
        }

        treeNodeOpenStatus[ROOT_PATH.string()] = true;
    }

    void DirectoryPanel::OnShow()
    {
        dropTargetPath.clear();

        if (ImGui::Begin("Project", nullptr, ImGuiWindowFlags_HorizontalScrollbar))
        {
            ImGui::Separator();

            if (rootNode) RenderDirectoryTree(rootNode);

            // Right-click on empty space in the panel
            if (ImGui::BeginPopupContextWindow("##DirPanelContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
                if (ImGui::MenuItem("New Folder")) {
                    newFolderParentPath = ROOT_PATH.string();
                    memset(newFolderNameBuf, 0, sizeof(newFolderNameBuf));
                    showNewFolderPopup = true;
                }
                ImGui::EndPopup();
            }

            RefreshUpdate();
            PrintSelectedInfo();
            DeleteUpdate();
            NewFolderUpdate();
            RenameUpdate();
        }
        ImGui::End();

        if (filesDropped && !droppedFiles.empty())
        {
            std::filesystem::path targetDir{ dropTargetPath.empty() ? ROOT_PATH : dropTargetPath };
            CopyFilesToDirectory(droppedFiles, targetDir);
            droppedFiles.clear();
            dropTargetPath.clear();
            filesDropped = false;
        }
    }

    void DirectoryPanel::RefreshUpdate()
    {
        const double dt =
            m_App ? m_App->GetDeltaTime()
            : (m_Ctx ? m_Ctx->DeltaTime : 0.0);

        static std::future<std::unique_ptr<FileNode>> refreshFuture;

        // Pause refresh while any popup/context menu is active to avoid invalidating ImGui IDs
        bool popupActive = showDeleteConfirm || showDeleteError || showNewFolderPopup || showRenamePopup
            || ImGui::IsPopupOpen("Confirm Delete##Dir")
            || ImGui::IsPopupOpen("New Folder##Dir")
            || ImGui::IsPopupOpen("Rename##Dir")
            || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

        if (((rTimer += dt) > AUTO_REFRESH_SEC) && !refreshFuture.valid() && !popupActive)
        {
            refreshFuture = std::async(std::launch::async, [this]() {return BuildDirectoryTree(); });
            rTimer = 0.0;
        }

        if (refreshFuture.valid() && refreshFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            if (popupActive) return;  // defer applying refresh until popup closes
            rootNode = refreshFuture.get();
            UpdateAssetRegistry();
        }
    }

    void DirectoryPanel::ForceRefresh()
    {
        // Synchronously rebuild the directory tree and update asset registry
        rootNode = BuildDirectoryTree();
        UpdateAssetRegistry();
        rTimer = 0.0;  // Reset the auto-refresh timer
    }

    void DirectoryPanel::DeleteUpdate()
    {
        if (!selectedPath.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            showDeleteConfirm = true;

        if (showDeleteConfirm) {
            ImGui::OpenPopup("Confirm Delete##Dir");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Confirm Delete##Dir", &showDeleteConfirm,
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
            std::filesystem::path pathToDelete(selectedPath);
            std::string displayName = pathToDelete.filename().string();
            bool isDir = std::filesystem::is_directory(pathToDelete);

            ImGui::Text("Are you sure you want to delete:");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "  %s", displayName.c_str());
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Path: %s", selectedPath.c_str());

            if (isDir) {
                // Count and list directory contents
                int fileCount = 0;
                std::vector<std::string> contents;
                std::error_code ec;
                for (auto& entry : std::filesystem::recursive_directory_iterator(pathToDelete, ec)) {
                    if (!entry.is_directory()) {
                        fileCount++;
                        if (contents.size() < 5) {
                            contents.push_back(entry.path().filename().string());
                        }
                    }
                }
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "This directory contains %d file(s):", fileCount);
                for (const auto& name : contents) {
                    ImGui::BulletText("%s", name.c_str());
                }
                if (fileCount > 5) {
                    ImGui::BulletText("... and %d more", fileCount - 5);
                }
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "This will permanently delete %s from disk!",
                               isDir ? "the directory and all its contents" : "the file");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                if (DeletePath(selectedPath)) {
                    UpdateAssetRegistry();
                    selectedPath.clear();
                }
                else {
                    showDeleteError = true;
                    deleteErrorMessage = "Failed to delete: " + selectedPath;
                }
                showDeleteConfirm = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showDeleteConfirm = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (showDeleteError) {
            ImGui::OpenPopup("Delete Error");
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        }

        if (ImGui::BeginPopupModal("Delete Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", deleteErrorMessage.c_str());
            ImGui::Separator();

            if (ImGui::Button("OK", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showDeleteError = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void DirectoryPanel::NewFolderUpdate()
    {
        if (showNewFolderPopup) {
            ImGui::OpenPopup("New Folder##Dir");
            showNewFolderPopup = false;  // one-shot trigger
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("New Folder##Dir", nullptr,
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("Create new folder in:");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "  %s", newFolderParentPath.c_str());
            ImGui::Spacing();

            // Auto-focus on first appearance
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }

            bool enterPressed = ImGui::InputText("Folder Name", newFolderNameBuf, sizeof(newFolderNameBuf),
                                                  ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();

            std::string folderName(newFolderNameBuf);
            bool nameValid = !folderName.empty();
            std::filesystem::path newPath = std::filesystem::path(newFolderParentPath) / folderName;
            bool alreadyExists = nameValid && std::filesystem::exists(newPath);

            if (alreadyExists) {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "A folder with this name already exists!");
            }

            if (!nameValid) ImGui::BeginDisabled();
            if (alreadyExists) ImGui::BeginDisabled();

            if (ImGui::Button("Create", ImVec2(120, 0)) || (enterPressed && nameValid && !alreadyExists)) {
                std::error_code ec;
                std::filesystem::create_directory(newPath, ec);
                if (!ec) {
                    rootNode = BuildDirectoryTree();
                    treeNodeOpenStatus[newPath.string()] = false;
                }
                ImGui::CloseCurrentPopup();
            }

            if (alreadyExists) ImGui::EndDisabled();
            if (!nameValid) ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void DirectoryPanel::RenameUpdate()
    {
        if (showRenamePopup) {
            ImGui::OpenPopup("Rename##Dir");
            showRenamePopup = false;  // one-shot trigger
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Rename##Dir", nullptr,
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
            std::filesystem::path oldPath(renameTargetPath);
            std::string oldName = oldPath.filename().string();
            bool isDir = std::filesystem::is_directory(oldPath);

            ImGui::Text("Rename %s:", isDir ? "folder" : "file");
            ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "  %s", oldName.c_str());
            ImGui::Spacing();

            // Auto-focus on first appearance
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }

            bool enterPressed = ImGui::InputText("New Name", renameNameBuf, sizeof(renameNameBuf),
                                                  ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();

            std::string newName(renameNameBuf);
            bool nameValid = !newName.empty() && newName != oldName;
            std::filesystem::path newPath = oldPath.parent_path() / newName;
            bool alreadyExists = nameValid && std::filesystem::exists(newPath);

            if (alreadyExists) {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "An item with this name already exists!");
            }

            if (!nameValid) ImGui::BeginDisabled();
            if (alreadyExists) ImGui::BeginDisabled();

            if (ImGui::Button("Rename", ImVec2(120, 0)) || (enterPressed && nameValid && !alreadyExists)) {
                std::error_code ec;
                std::filesystem::rename(oldPath, newPath, ec);
                if (!ec) {
                    // Update asset registry source paths
                    std::string oldPathStr = oldPath.generic_string();
                    std::string newPathStr = newPath.generic_string();

                    for (auto& [type, map] : m_App->GetAssetRegistry().GetAll()) {
                        for (auto& [id, asset] : map) {
                            if (isDir) {
                                // For directories: update all assets whose source starts with old path prefix
                                std::string prefix = oldPathStr + "/";
                                if (asset->source.compare(0, prefix.size(), prefix) == 0) {
                                    asset->source = newPathStr + "/" + asset->source.substr(prefix.size());
                                }
                            }
                            else {
                                // For files: exact match
                                if (asset->source == oldPathStr) {
                                    asset->source = newPathStr;
                                }
                            }
                        }
                    }

                    // Update selected path if it was the renamed item
                    if (selectedPath == renameTargetPath) {
                        selectedPath = newPath.string();
                    }

                    // Transfer open status for directories
                    if (isDir) {
                        auto it = treeNodeOpenStatus.find(oldPath.string());
                        if (it != treeNodeOpenStatus.end()) {
                            treeNodeOpenStatus[newPath.string()] = it->second;
                            treeNodeOpenStatus.erase(it);
                        }
                    }

                    rootNode = BuildDirectoryTree();
                    UpdateAssetRegistry();
                }
                ImGui::CloseCurrentPopup();
            }

            if (alreadyExists) ImGui::EndDisabled();
            if (!nameValid) ImGui::EndDisabled();

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void DirectoryPanel::PrintSelectedInfo()
    {
        ImGui::Separator();
        ImGui::Text("Selected: %s", selectedPath.empty() ? "None" : selectedPath.c_str());
        if (!selectedPath.empty() && std::filesystem::exists(selectedPath))
        {
            if (!std::filesystem::is_directory(selectedPath))
                ImGui::Text("Size: %llu bytes",
                    static_cast<unsigned long long>(std::filesystem::file_size(selectedPath)));
        }
    }

    std::unique_ptr<DirectoryPanel::FileNode> DirectoryPanel::BuildDirectoryTree()
    {
        auto root = std::make_unique<FileNode>(ROOT_PATH.filename().string(), true, ROOT_PATH);

        std::function<void(FileNode&, uint32_t)> scanDir = [&](FileNode& node, uint32_t depth)
            {
                if (depth > MAX_DEPTH || !std::filesystem::exists(node.fullPath)) return;

                for (const auto& entry : std::filesystem::directory_iterator(node.fullPath))
                {
                    const auto& path = entry.path();
                    bool isDir = entry.is_directory();

                    unsigned id{ isDir ? (unsigned)folderIcon : (unsigned)assetIcon }; // Assign icon based on type

                    //custom sprites
                    if (!isDir) {
                        std::string ext{ path.extension().string() };
                        if (ext == ".dds" || ext == ".png") {
                            //finds the texture id to draw
                            m_App->AssetTextureView([&path, &id](TextureAsset* tex) {
                                if (tex->source == path.generic_string()) {
                                    id = static_cast<unsigned>(*tex->data.get());
                                    return;
                                }
                                });
                        }
                    }

                    auto child = std::make_unique<FileNode>(path.filename().string(), isDir, path, id);
                    if (isDir) scanDir(*child, depth + 1);
                    node.children.push_back(std::move(child));
                }
            };

        scanDir(*root, 0);
        return root;
    }

    void DirectoryPanel::RenderDirectoryTree(std::unique_ptr<FileNode> const& root)
    {
        std::stable_sort(root->children.begin(), root->children.end(),
            [](const auto& a, const auto& b)
            {
                return a->isDirectory > b->isDirectory ||
                    (a->isDirectory == b->isDirectory && a->name < b->name);
            });

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
        if (root->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        bool isSelected = (selectedPath == root->fullPath.string());
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::PushID(root->fullPath.string().c_str());

        //keep memory of open states for auto refreshing
        bool isOpen{ treeNodeOpenStatus[root->fullPath.string()] };
        //new directories
        if (root->isDirectory && treeNodeOpenStatus.find(root->fullPath.string()) == treeNodeOpenStatus.end()) {
            isOpen = (root->fullPath == ROOT_PATH);
        }
        //assets
        if (!root->isDirectory) {
            isOpen = false;
        }
        if (isOpen) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        root->isHovered = false;

        if (root->texId) {
            ImGui::Image((void*)(intptr_t)root->texId, ImVec2(32, 32));
            ImGui::SameLine();
        }

        bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)root.get(), flags, root->isDirectory ? "%s/" : "%s", root->name.c_str());

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly | ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
            root->isHovered = true;
            if (root->isDirectory) {
                dropTargetPath = root->fullPath;
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.6f, 0.4f));
            }
            else {
                dropTargetPath = root->fullPath.parent_path(); //output to same directory
            }
        }

        if (ImGui::IsItemClicked()) {
            m_App->ResetAllSelected();
            selectedPath = root->fullPath.string();
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem()) {
            if (root->isDirectory) {
                if (ImGui::MenuItem("New Folder")) {
                    newFolderParentPath = root->fullPath.string();
                    memset(newFolderNameBuf, 0, sizeof(newFolderNameBuf));
                    showNewFolderPopup = true;
                }
                ImGui::Separator();
            }
            if (root->fullPath != ROOT_PATH) {
                if (ImGui::MenuItem("Rename")) {
                    renameTargetPath = root->fullPath.string();
                    memset(renameNameBuf, 0, sizeof(renameNameBuf));
                    std::string currentName = root->fullPath.filename().string();
                    strncpy_s(renameNameBuf, currentName.c_str(), sizeof(renameNameBuf) - 1);
                    showRenamePopup = true;
                }
                if (ImGui::MenuItem("Delete")) {
                    selectedPath = root->fullPath.string();
                    showDeleteConfirm = true;
                }
            }
            ImGui::EndPopup();
        }

        // Drag source for animation files
        if (!root->isDirectory) {
            std::string ext = root->fullPath.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae" || ext == ".anim") {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    std::string pathStr = root->fullPath.string();
                    ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_ANIM_FILE.data(), pathStr.c_str(), pathStr.size() + 1);
                    ImGui::Text("Animation: %s", root->name.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }

        if (root->isDirectory) {
            treeNodeOpenStatus[root->fullPath.string()] = nodeOpen;
        }

        if (nodeOpen)
        {
            for (auto& child : root->children)
                RenderDirectoryTree(child);
            ImGui::TreePop();
        }

        if (root->isHovered && root->isDirectory) {
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }

    //update assetmanager based on files located:
        // textures(.png/.dds)
        // models(.fbx)
        // skybox(.hdr)
    //missing implementation file types : script, scene, prefab
    void DirectoryPanel::UpdateAssetRegistry() {
        std::unordered_set<std::filesystem::path> seenPaths;

        if (rootNode) {
            TraverseAndRegister(rootNode.get(), seenPaths);
        }

        RemoveStaleAssets(seenPaths);
    }

    //recursively traverse into linked list tree to register/update assets
    void DirectoryPanel::TraverseAndRegister(FileNode* node, std::unordered_set<std::filesystem::path>& seen)
    {
        if (!node) return;

        const auto& path = node->fullPath;
        std::string ext{ path.extension().string() };
        std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {return (char)::tolower(c); }); //lowercase

        // --- TEXTURES ---
        if (!node->isDirectory && (ext == ".png" || ext == ".dds")) {
            seen.insert(path);
            if (ext == ".dds" && Texture2D::IsHDR(path.generic_string()))
                RegisterAsset<SkyboxAsset>(path, node->texId);
            else
                RegisterAsset<TextureAsset>(path, node->texId);
        }
        // --- MODELS ---
        else if (!node->isDirectory && ext == ".fbx") {
            seen.insert(path);
            RegisterAsset<ModelAsset>(path, node->texId);
        }
        // --- SKYBOX ---
        else if (!node->isDirectory && ext == ".hdr") {
            seen.insert(path);
            RegisterAsset<SkyboxAsset>(path, node->texId);
        }
        // --- PREFABS ---
        else if (!node->isDirectory && ext == ".prefab") {
            seen.insert(path);
            RegisterAsset<PrefabAsset>(path, node->texId);
        }
        // --- AUDIO ---
        else if (!node->isDirectory && ext == ".wav") {
            seen.insert(path);
            RegisterAsset<AudioAsset>(path, node->texId);
		}
        // --- ANIMATIONS ---
        else if (!node->isDirectory && ext == ".anim") {
            seen.insert(path);
            RegisterAsset<AnimationAsset>(path, node->texId);
        }

        // --- more stuff in future ---

        // Recurse into children
        for (auto& child : node->children)
        {
            TraverseAndRegister(child.get(), seen);
        }
    }

    //logic for update/register of asset
    template <class T>
    void DirectoryPanel::RegisterAsset(const std::filesystem::path& path, unsigned int& texId)
    {
        AssetID uid{ m_App->AssetIDFromPath(path) };  // your hash function
        if (m_App->GetAssetRegistry().Get<T>(uid).uid == EMPTY_ASSET) {
            // New asset
            texId = (unsigned int)assetIcon;
            if constexpr (std::is_same_v<T, TextureAsset>) {
                texId = (GLuint)(*m_App->GetAssetRegistry().AddTexture(uid, path.generic_string()).get()->data);
            }
            else if constexpr (std::is_same_v<T, ModelAsset>) {
                m_App->GetAssetRegistry().AddModel(uid, path.generic_string());
            }
            else if constexpr (std::is_same_v<T, SkyboxAsset>) {
                m_App->GetAssetRegistry().AddSkybox(uid, path.generic_string());
            }
            else if constexpr (std::is_same_v<T, PrefabAsset>) {
                m_App->GetAssetRegistry().AddPrefab(uid, path.generic_string());
            }
            else if constexpr (std::is_same_v<T, AudioAsset>) {
                m_App->GetAssetRegistry().AddAudio(uid, path.generic_string());
            }
            else if constexpr (std::is_same_v<T, AnimationAsset>) {
                m_App->GetAssetRegistry().AddAnimation(uid, path.generic_string());
            }
            // --- more stuff in future ---

        }
    }

    //removes stale assets for: .png/.dds, .fbx, .anim, and .wav
    void DirectoryPanel::RemoveStaleAssets(const std::unordered_set<std::filesystem::path>& seen)
    {
        for (auto& [type, map] : m_App->GetAssetRegistry().GetAll()) {
            if (!map.empty()) {
                for (auto it{ map.begin() }; it != map.end(); ) {
                    std::string ext{ GetExtension(it->second->source) };
                    //ignore empty asset
                    if (ext == "" || (ext != "png" && ext != "dds" && ext != "fbx" && ext != "anim" && ext != "wav")) {
                        ++it;
                        continue; //ignore wrong types
                    }
                    //if file not located in seen path = deleted, must remove from assetmanager
                    if (seen.find(it->second->source) == seen.end()) {
                        it = map.erase(it);
                    }
                    else ++it;
                }
            }
        }
    }

    void DirectoryPanel::CopyFilesToDirectory(const std::vector<std::string>& files, const std::filesystem::path& targetDir)
    {
        for (const auto& filePath : files)
        {
            std::filesystem::path src(filePath);
            if (!std::filesystem::exists(src)) continue;

            std::filesystem::path dest = targetDir / src.filename();
            try
            {
                if (std::filesystem::exists(dest))
                {
                    std::string base = dest.stem().string();
                    std::string ext = dest.extension().string();
                    int i = 1;
                    do { dest = targetDir / (base + " (" + std::to_string(i++) + ")" + ext); } while (std::filesystem::exists(dest));
                }
                std::filesystem::copy(src, dest, std::filesystem::copy_options::recursive);
            }
            catch (const std::filesystem::filesystem_error& e)
            {
#ifdef DEBUG
                char const* dodo{ e.what() }; //needed due to warning level 4
                BOOM_ERROR("Directory.h_CopyFilesToDirectory:{}", dodo);
#endif // DEBUG

			std::cout << "Error copying file: " << e.what() << std::endl;
            }
        }
        rootNode = BuildDirectoryTree();
    }

    bool DirectoryPanel::DeletePath(const std::filesystem::path& path)
    {
        try
        {
            if (!std::filesystem::exists(path)) return false;
            if (std::filesystem::is_directory(path)) std::filesystem::remove_all(path);
            else                                     std::filesystem::remove(path);

            rootNode = BuildDirectoryTree();
            return true;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
#ifdef DEBUG
            char const* dodo{ e.what() }; //warning lvl 4
            BOOM_ERROR("Directory.h_DeletePath:{}", dodo);
#endif // DEBUG
			std::cout << "Error deleting path: " << e.what() << std::endl;
            
            return false;
        }
    }

    std::string DirectoryPanel::GetExtension(std::string const& filename) {
        uint32_t pos{ (uint32_t)filename.find_last_of('.') };
        if (pos == std::string::npos || pos == filename.length() - 1) {
            return ""; //no extension
        }
        std::string ext{ filename.substr(pos + 1) };
        std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {return (char)::tolower(c); }); //lowercase
        return ext;
    }

    void DirectoryPanel::OnDrop(GLFWwindow*, int count, const char** paths)
    {
        droppedFiles.clear();
        for (int i = 0; i < count; ++i) droppedFiles.push_back(paths[i]);
        filesDropped = true;
    }

} // namespace EditorUI