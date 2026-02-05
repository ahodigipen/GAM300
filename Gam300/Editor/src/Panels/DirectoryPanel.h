#pragma once
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <unordered_set>
#include <set>
#include "imgui.h"

namespace Boom { struct AppContext; struct AppInterface; struct Texture2D; }
struct GLFWwindow;

namespace EditorUI {

    class Editor;

    class DirectoryPanel {
    public:
        explicit DirectoryPanel(Editor* owner);
		~DirectoryPanel();
        void Init();
        void OnShow();
        void Render() { OnShow(); }
        void ForceRefresh();  // Immediately refresh asset registry (for audio picker, etc.)

    private:
        // Helpers
        void RefreshUpdate();
        void DeleteUpdate();
        void NewFolderUpdate();
        void RenameUpdate();
        void PrintSelectedInfo();

        // Multi-select helpers
        bool IsPathSelected(const std::string& path) const;
        void SelectPath(const std::string& path, bool addToSelection = false);
        void TogglePathSelection(const std::string& path);
        void ClearSelection();
        void SelectRange(const std::string& fromPath, const std::string& toPath);

        // Move helpers
        bool MovePathsToDirectory(const std::vector<std::string>& paths, const std::filesystem::path& targetDir);
        bool IsDescendantOf(const std::filesystem::path& child, const std::filesystem::path& parent);
        void UpdateAssetRegistryAfterMove(const std::filesystem::path& oldPath, const std::filesystem::path& newPath, bool isDirectory);

        // Directory tree
        struct FileNode;
        std::unique_ptr<FileNode> BuildDirectoryTree();
        void RenderDirectoryTree(std::unique_ptr<FileNode> const& root);

        // Asset Managing
        void UpdateAssetRegistry();
        void TraverseAndRegister(FileNode* node, std::unordered_set<std::filesystem::path>& seen);
        template <class T>
        void RegisterAsset(const std::filesystem::path& path, unsigned int& texId);
        void RemoveStaleAssets(const std::unordered_set<std::filesystem::path>& seen); //should only remove .png/.dds and .fbx for now

        // Filesystem ops
        void CopyFilesToDirectory(const std::vector<std::string>& filePaths, const std::filesystem::path& targetDir);
        bool DeletePath(const std::filesystem::path& path);
        std::string GetExtension(std::string const& filename);

        // Drag-and-drop callback
        static void OnDrop(GLFWwindow*, int count, const char** paths);

    private:
        // Owner / engine access
        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;   // preferred access (Interface.h)
        Boom::AppContext* m_Ctx = nullptr;   // cached from m_App->GetContext()

        // Config
        const std::filesystem::path ROOT_PATH{ "Resources" };
        const uint32_t  MAX_DEPTH = 7;
        const double    AUTO_REFRESH_SEC = 3.0;
        const std::string_view CUSTOM_PAYLOAD_TYPE{ "_GLFW_DROP" };

        // UI state
        std::unique_ptr<FileNode>      rootNode{};
        std::string                    selectedPath{};  // For backwards compat, keeps last clicked
        std::set<std::string>          selectedPaths{}; // Multi-selection
        std::string                    selectionAnchor{}; // For shift-click range selection
        std::vector<std::string>       visiblePathsInOrder{}; // Built during render for range select
        double                         rTimer = AUTO_REFRESH_SEC;
        std::unordered_map<std::string, bool> treeNodeOpenStatus;

        // Drag & drop (external GLFW drops)
        inline static std::vector<std::string> droppedFiles{};
        inline static bool filesDropped{ false };
        std::filesystem::path dropTargetPath{};

        // Deferred move operation (to avoid modifying tree during traversal)
        bool                        pendingMove{};
        std::vector<std::string>    pendingMovePaths{};
        std::filesystem::path       pendingMoveTarget{};

        // Delete handling
        bool        showDeleteConfirm{};
        bool        showDeleteError{};
        std::string deleteErrorMessage{};

        // New folder handling
        bool        showNewFolderPopup{};
        std::string newFolderParentPath{};
        char        newFolderNameBuf[256]{};

        // Rename handling
        bool        showRenamePopup{};
        std::string renameTargetPath{};
        char        renameNameBuf[256]{};

        ImTextureID folderIcon;
        ImTextureID assetIcon;
    };

} // namespace EditorUI
