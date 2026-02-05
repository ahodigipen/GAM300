#include "Panels/DirectoryPanel.h"
#include "Editor.h"
#include "Context/Context.h"
#include "Context/DebugHelpers.h"
#include "Vendors/imgui/imgui.h"
#include "Graphics/Textures/Texture.h"
#include "Application/Interface.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <future>

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

        if (m_App) {
            if (auto wh = m_App->GetWindowHandle())
                glfwSetDropCallback(wh.get(), OnDrop);
        }
        else if (m_Ctx && m_Ctx->window) {
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

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
                ClearSelection();
            }

            if (ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
                for (const auto& path : visiblePathsInOrder) {
                    selectedPaths.insert(path);
                }
                if (!selectedPaths.empty()) {
                    selectedPath = *selectedPaths.begin();
                    selectionAnchor = selectedPath;
                }
            }

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

        if (pendingMove && !pendingMovePaths.empty())
        {
            if (MovePathsToDirectory(pendingMovePaths, pendingMoveTarget)) {
                rootNode = BuildDirectoryTree();
                UpdateAssetRegistry();
            }
            pendingMove = false;
            pendingMovePaths.clear();
            pendingMoveTarget.clear();
        }

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
            if (popupActive) return;
            rootNode = refreshFuture.get();
            UpdateAssetRegistry();
        }
    }

    void DirectoryPanel::ForceRefresh()
    {
        rootNode = BuildDirectoryTree();
        UpdateAssetRegistry();
        rTimer = 0.0;
    }

    bool DirectoryPanel::IsPathSelected(const std::string& path) const {
        return selectedPaths.find(path) != selectedPaths.end();
    }

    void DirectoryPanel::SelectPath(const std::string& path, bool addToSelection) {
        if (!addToSelection) selectedPaths.clear();
        selectedPaths.insert(path);
        selectionAnchor = path;
        selectedPath = path;
    }

    void DirectoryPanel::TogglePathSelection(const std::string& path) {
        if (IsPathSelected(path)) {
            selectedPaths.erase(path);
            selectedPath = selectedPaths.empty() ? "" : *selectedPaths.begin();
        } else {
            selectedPaths.insert(path);
            selectionAnchor = path;
            selectedPath = path;
        }
    }

    void DirectoryPanel::ClearSelection() {
        selectedPaths.clear();
        selectionAnchor.clear();
        selectedPath.clear();
    }

    void DirectoryPanel::SelectRange(const std::string& fromPath, const std::string& toPath) {
        auto fromIt = std::find(visiblePathsInOrder.begin(), visiblePathsInOrder.end(), fromPath);
        auto toIt = std::find(visiblePathsInOrder.begin(), visiblePathsInOrder.end(), toPath);

        if (fromIt == visiblePathsInOrder.end() || toIt == visiblePathsInOrder.end()) {
            SelectPath(toPath, false);
            return;
        }

        if (fromIt > toIt) std::swap(fromIt, toIt);

        selectedPaths.clear();
        for (auto it = fromIt; it <= toIt; ++it) {
            selectedPaths.insert(*it);
        }
        selectedPath = toPath;
    }

    bool DirectoryPanel::IsDescendantOf(const std::filesystem::path& child, const std::filesystem::path& parent) {
        auto current = child;
        while (current.has_parent_path() && current != current.parent_path()) {
            current = current.parent_path();
            if (current == parent) return true;
        }
        return false;
    }

    void DirectoryPanel::UpdateAssetRegistryAfterMove(const std::filesystem::path& oldPath,
                                                       const std::filesystem::path& newPath,
                                                       bool isDirectory) {
        std::string oldPathStr = oldPath.generic_string();
        std::string newPathStr = newPath.generic_string();

        for (auto& [type, map] : m_App->GetAssetRegistry().GetAll()) {
            for (auto& [id, asset] : map) {
                std::string assetSource = asset->source;
                std::replace(assetSource.begin(), assetSource.end(), '\\', '/');

                if (isDirectory) {
                    std::string prefix = oldPathStr + "/";
                    if (assetSource.compare(0, prefix.size(), prefix) == 0) {
                        asset->source = newPathStr + "/" + assetSource.substr(prefix.size());
                    }
                    else if (assetSource == oldPathStr) {
                        asset->source = newPathStr;
                    }
                }
                else {
                    if (assetSource == oldPathStr) {
                        asset->source = newPathStr;
                    }
                }
            }
        }

        if (isDirectory) {
            auto it = treeNodeOpenStatus.find(oldPath.string());
            if (it != treeNodeOpenStatus.end()) {
                treeNodeOpenStatus[newPath.string()] = it->second;
                treeNodeOpenStatus.erase(it);
            }
        }
    }

    bool DirectoryPanel::MovePathsToDirectory(const std::vector<std::string>& paths,
                                               const std::filesystem::path& targetDir) {
        std::error_code ec;

        for (const auto& pathStr : paths) {
            std::filesystem::path source(pathStr);

            if (!std::filesystem::exists(source)) continue;
            if (source == ROOT_PATH) continue;
            if (source.parent_path() == targetDir) continue;
            if (std::filesystem::is_directory(source) && IsDescendantOf(targetDir, source)) {
                BOOM_ERROR("Cannot move folder into itself: {}", pathStr);
                return false;
            }

            std::filesystem::path target = targetDir / source.filename();
            if (std::filesystem::exists(target)) {
                std::string base = target.stem().string();
                std::string ext = target.extension().string();
                int i = 1;
                do {
                    target = targetDir / (base + " (" + std::to_string(i++) + ")" + ext);
                } while (std::filesystem::exists(target));
            }

            bool isDir = std::filesystem::is_directory(source);
            std::filesystem::rename(source, target, ec);
            if (ec) {
                BOOM_ERROR("Move failed: {} -> {}: {}", pathStr, target.string(), ec.message());
                return false;
            }

            UpdateAssetRegistryAfterMove(source, target, isDir);
        }

        ClearSelection();
        return true;
    }

    void DirectoryPanel::DeleteUpdate()
    {
        if (!selectedPaths.empty() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            showDeleteConfirm = true;

        if (showDeleteConfirm) {
            ImGui::OpenPopup("Confirm Delete##Dir");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(450, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Confirm Delete##Dir", &showDeleteConfirm,
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {

            if (selectedPaths.size() == 1) {
                std::filesystem::path pathToDelete(*selectedPaths.begin());
                std::string displayName = pathToDelete.filename().string();
                bool isDir = std::filesystem::is_directory(pathToDelete);

                ImGui::Text("Are you sure you want to delete:");
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "  %s", displayName.c_str());
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Path: %s", selectedPaths.begin()->c_str());

                if (isDir) {
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
            }
            else {
                ImGui::Text("Are you sure you want to delete %zu items?", selectedPaths.size());
                ImGui::Spacing();

                int shown = 0;
                for (const auto& pathStr : selectedPaths) {
                    if (shown++ >= 5) break;
                    std::filesystem::path p(pathStr);
                    ImGui::BulletText("%s", p.filename().string().c_str());
                }
                if (selectedPaths.size() > 5) {
                    ImGui::BulletText("... and %zu more", selectedPaths.size() - 5);
                }

                int totalFiles = 0;
                int folderCount = 0;
                for (const auto& pathStr : selectedPaths) {
                    std::filesystem::path p(pathStr);
                    if (std::filesystem::is_directory(p)) {
                        folderCount++;
                        std::error_code ec;
                        for (auto& entry : std::filesystem::recursive_directory_iterator(p, ec)) {
                            if (!entry.is_directory()) totalFiles++;
                        }
                    }
                    else {
                        totalFiles++;
                    }
                }
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Total: %d files, %d folders", totalFiles, folderCount);
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "This will permanently delete all items from disk!");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
                bool allSuccess = true;
                for (const auto& pathStr : selectedPaths) {
                    if (!DeletePath(pathStr)) {
                        allSuccess = false;
                        showDeleteError = true;
                        deleteErrorMessage = "Failed to delete some items";
                    }
                }

                if (allSuccess) {
                    UpdateAssetRegistry();
                }
                ClearSelection();
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
            showNewFolderPopup = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("New Folder##Dir", nullptr,
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("Create new folder in:");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "  %s", newFolderParentPath.c_str());
            ImGui::Spacing();

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
        if (selectedPaths.size() > 1) {
            showRenamePopup = false;
            return;
        }

        if (showRenamePopup) {
            ImGui::OpenPopup("Rename##Dir");
            showRenamePopup = false;
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
                    std::string oldPathStr = oldPath.generic_string();
                    std::string newPathStr = newPath.generic_string();

                    for (auto& [type, map] : m_App->GetAssetRegistry().GetAll()) {
                        for (auto& [id, asset] : map) {
                            if (isDir) {
                                std::string prefix = oldPathStr + "/";
                                if (asset->source.compare(0, prefix.size(), prefix) == 0) {
                                    asset->source = newPathStr + "/" + asset->source.substr(prefix.size());
                                }
                            }
                            else {
                                if (asset->source == oldPathStr) {
                                    asset->source = newPathStr;
                                }
                            }
                        }
                    }

                    if (selectedPath == renameTargetPath) {
                        selectedPath = newPath.string();
                    }

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

        if (selectedPaths.empty()) {
            ImGui::Text("Selected: None");
        }
        else if (selectedPaths.size() == 1) {
            std::string path = *selectedPaths.begin();
            ImGui::Text("Selected: %s", path.c_str());
            if (std::filesystem::exists(path) && !std::filesystem::is_directory(path)) {
                ImGui::Text("Size: %llu bytes",
                    static_cast<unsigned long long>(std::filesystem::file_size(path)));
            }
        }
        else {
            ImGui::Text("Selected: %zu items", selectedPaths.size());

            int fileCount = 0;
            int folderCount = 0;
            for (const auto& path : selectedPaths) {
                if (std::filesystem::is_directory(path)) folderCount++;
                else fileCount++;
            }

            if (folderCount > 0 && fileCount > 0) {
                ImGui::Text("  %d folders, %d files", folderCount, fileCount);
            }
            else if (folderCount > 0) {
                ImGui::Text("  %d folders", folderCount);
            }
            else {
                ImGui::Text("  %d files", fileCount);
            }
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

                    unsigned id{ isDir ? (unsigned)folderIcon : (unsigned)assetIcon };

                    if (!isDir) {
                        std::string ext{ path.extension().string() };
                        if (ext == ".dds" || ext == ".png") {
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
        if (root->fullPath == ROOT_PATH) {
            visiblePathsInOrder.clear();
        }

        std::stable_sort(root->children.begin(), root->children.end(),
            [](const auto& a, const auto& b)
            {
                return a->isDirectory > b->isDirectory ||
                    (a->isDirectory == b->isDirectory && a->name < b->name);
            });

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow;
        if (root->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        bool isSelected = IsPathSelected(root->fullPath.string());
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::PushID(root->fullPath.string().c_str());

        bool isOpen{ treeNodeOpenStatus[root->fullPath.string()] };
        if (root->isDirectory && treeNodeOpenStatus.find(root->fullPath.string()) == treeNodeOpenStatus.end()) {
            isOpen = (root->fullPath == ROOT_PATH);
        }
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

        visiblePathsInOrder.push_back(root->fullPath.string());

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly | ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
            root->isHovered = true;
            if (root->isDirectory) {
                dropTargetPath = root->fullPath;
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.6f, 0.4f));
            }
            else {
                dropTargetPath = root->fullPath.parent_path();
            }
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_App->ResetAllSelected();
            std::string clickedPath = root->fullPath.string();
            ImGuiIO& io = ImGui::GetIO();

            if (io.KeyShift && !selectionAnchor.empty()) {
                SelectRange(selectionAnchor, clickedPath);
            } else if (io.KeyCtrl) {
                TogglePathSelection(clickedPath);
            } else {
                if (!IsPathSelected(clickedPath)) {
                    SelectPath(clickedPath, false);
                }
            }
        }

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
                bool multiSelect = selectedPaths.size() > 1;
                if (multiSelect) ImGui::BeginDisabled();

                if (ImGui::MenuItem("Rename")) {
                    renameTargetPath = root->fullPath.string();
                    memset(renameNameBuf, 0, sizeof(renameNameBuf));
                    std::string currentName = root->fullPath.filename().string();
                    strncpy_s(renameNameBuf, currentName.c_str(), sizeof(renameNameBuf) - 1);
                    showRenamePopup = true;
                }

                if (multiSelect) ImGui::EndDisabled();

                if (ImGui::MenuItem("Delete")) {
                    if (!IsPathSelected(root->fullPath.string())) {
                        SelectPath(root->fullPath.string(), false);
                    }
                    showDeleteConfirm = true;
                }
            }
            ImGui::EndPopup();
        }

        if (root->fullPath != ROOT_PATH) {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                std::string thisPath = root->fullPath.string();

                ImGui::SetDragDropPayload(CONSTANTS::DND_PAYLOAD_DIR_MOVE.data(), thisPath.c_str(), thisPath.size() + 1);

                if (IsPathSelected(thisPath) && selectedPaths.size() > 1) {
                    ImGui::Text("Move %zu items", selectedPaths.size());
                } else {
                    ImGui::Text("Move: %s", root->name.c_str());
                }

                ImGui::EndDragDropSource();
            }
        }

        if (root->isDirectory && ImGui::BeginDragDropTarget()) {
            const ImGuiPayload* previewPayload = ImGui::AcceptDragDropPayload(CONSTANTS::DND_PAYLOAD_DIR_MOVE.data(), ImGuiDragDropFlags_AcceptPeekOnly);
            if (previewPayload) {
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                ImGui::GetForegroundDrawList()->AddRect(min, max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
            }

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONSTANTS::DND_PAYLOAD_DIR_MOVE.data())) {
                std::string draggedPath(static_cast<const char*>(payload->Data));

                pendingMovePaths.clear();
                if (IsPathSelected(draggedPath) && !selectedPaths.empty()) {
                    for (const auto& p : selectedPaths) {
                        pendingMovePaths.push_back(p);
                    }
                } else {
                    pendingMovePaths.push_back(draggedPath);
                }

                pendingMoveTarget = root->fullPath;
                pendingMove = true;
            }
            ImGui::EndDragDropTarget();
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

    void DirectoryPanel::UpdateAssetRegistry() {
        std::unordered_set<std::filesystem::path> seenPaths;

        if (rootNode) {
            TraverseAndRegister(rootNode.get(), seenPaths);
        }

        RemoveStaleAssets(seenPaths);
    }

    void DirectoryPanel::TraverseAndRegister(FileNode* node, std::unordered_set<std::filesystem::path>& seen)
    {
        if (!node) return;

        const auto& path = node->fullPath;
        std::string ext{ path.extension().string() };
        std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {return (char)::tolower(c); });

        if (!node->isDirectory && (ext == ".png" || ext == ".dds")) {
            seen.insert(path);
            if (ext == ".dds" && Texture2D::IsHDR(path.generic_string()))
                RegisterAsset<SkyboxAsset>(path, node->texId);
            else
                RegisterAsset<TextureAsset>(path, node->texId);
        }
        else if (!node->isDirectory && ext == ".fbx") {
            seen.insert(path);
            RegisterAsset<ModelAsset>(path, node->texId);
        }
        else if (!node->isDirectory && ext == ".hdr") {
            seen.insert(path);
            RegisterAsset<SkyboxAsset>(path, node->texId);
        }
        else if (!node->isDirectory && ext == ".prefab") {
            seen.insert(path);
            RegisterAsset<PrefabAsset>(path, node->texId);
        }
        else if (!node->isDirectory && ext == ".wav") {
            seen.insert(path);
            RegisterAsset<AudioAsset>(path, node->texId);
		}
        else if (!node->isDirectory && ext == ".anim") {
            seen.insert(path);
            RegisterAsset<AnimationAsset>(path, node->texId);
        }

        for (auto& child : node->children)
        {
            TraverseAndRegister(child.get(), seen);
        }
    }

    template <class T>
    void DirectoryPanel::RegisterAsset(const std::filesystem::path& path, unsigned int& texId)
    {
        AssetID uid{ m_App->AssetIDFromPath(path) };
        if (m_App->GetAssetRegistry().Get<T>(uid).uid == EMPTY_ASSET) {
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
        }
    }

    void DirectoryPanel::RemoveStaleAssets(const std::unordered_set<std::filesystem::path>& seen)
    {
        for (auto& [type, map] : m_App->GetAssetRegistry().GetAll()) {
            if (!map.empty()) {
                for (auto it{ map.begin() }; it != map.end(); ) {
                    std::string ext{ GetExtension(it->second->source) };
                    if (ext == "" || (ext != "png" && ext != "dds" && ext != "fbx" && ext != "anim" && ext != "wav")) {
                        ++it;
                        continue;
                    }
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
                char const* dodo{ e.what() };
                BOOM_ERROR("Directory.h_CopyFilesToDirectory:{}", dodo);
#endif
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
            char const* dodo{ e.what() };
            BOOM_ERROR("Directory.h_DeletePath:{}", dodo);
#endif
			std::cout << "Error deleting path: " << e.what() << std::endl;
            return false;
        }
    }

    std::string DirectoryPanel::GetExtension(std::string const& filename) {
        uint32_t pos{ (uint32_t)filename.find_last_of('.') };
        if (pos == std::string::npos || pos == filename.length() - 1) {
            return "";
        }
        std::string ext{ filename.substr(pos + 1) };
        std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {return (char)::tolower(c); });
        return ext;
    }

    void DirectoryPanel::OnDrop(GLFWwindow*, int count, const char** paths)
    {
        droppedFiles.clear();
        for (int i = 0; i < count; ++i) droppedFiles.push_back(paths[i]);
        filesDropped = true;
    }

} // namespace EditorUI