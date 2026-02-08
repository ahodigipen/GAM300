#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "Vendors/imgui/imgui.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <GL/glew.h>
#include <entt/entity/entity.hpp>
#include "Graphics/Models/Animation.h"  // For Boom::KeyFrame

namespace Boom {
    struct AppContext;
    struct AppInterface;
    struct Model;
    struct Animator;
    struct Joint;
}

namespace EditorUI {
    class Editor;
    class CommandHistory;  // Forward declaration

    // Undo/Redo command for keyframe, bone pose, and audio event operations
    struct KeyframeCommand {
        enum Type { ADD, REMOVE, MOVE, BONE_POSE, BATCH, AUDIO_ADD, AUDIO_EDIT, AUDIO_REMOVE };

        Type type;
        std::string boneName;
        size_t keyframeIndex = 0;
        Boom::KeyFrame keyframe;  // The keyframe data
        float oldTime = 0.0f;     // For move operations
        float newTime = 0.0f;     // For move operations

        // For BONE_POSE operations
        glm::vec3 oldPosition = glm::vec3(0.0f);
        glm::quat oldRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 oldScale = glm::vec3(1.0f);
        glm::vec3 newPosition = glm::vec3(0.0f);
        glm::quat newRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 newScale = glm::vec3(1.0f);

        // For BATCH operations (groups multiple commands into one undo)
        std::vector<KeyframeCommand> batchCommands;

        // For AUDIO_ADD, AUDIO_EDIT, AUDIO_REMOVE operations
        Boom::AudioEventMarker audioEvent;      // The audio event data (for ADD/REMOVE, or new state for EDIT)
        Boom::AudioEventMarker oldAudioEvent;   // Previous state (for EDIT undo)
        size_t audioEventIndex = 0;             // Index in audioEvents vector (for EDIT/REMOVE)
    };

    // Bone pose for undo/redo
    struct BonePose {
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
    };

    // Selected keyframe identifier (for multiselect)
    struct SelectedKeyframe {
        std::string boneName;
        size_t keyframeIndex;

        // Comparison operator for std::set ordering
        bool operator<(const SelectedKeyframe& other) const {
            if (boneName != other.boneName) return boneName < other.boneName;
            return keyframeIndex < other.keyframeIndex;
        }

        bool operator==(const SelectedKeyframe& other) const {
            return boneName == other.boneName && keyframeIndex == other.keyframeIndex;
        }
    };

    /**
     * @brief Animation Timeline Editor - Unity-style animation editing with integrated 3D preview
     *
     * Features:
     * - Integrated 3D viewport for animation preview
     * - Timeline scrubber and keyframe editing
     * - Playback controls
     * - Bone track visualization
     */
    class AnimationTimelinePanel {
    public:
        explicit AnimationTimelinePanel(Editor* owner);
        ~AnimationTimelinePanel();

        void Render();

        // Bone pose manipulation (public for undo/redo commands)
        void SetBonePose(const std::string& boneName, const BonePose& pose);  // Set bone pose (for undo/redo)
        void ClearBonePose(const std::string& boneName);  // Clear bone pose override
        void ClearAllBonePoses();  // Clear all bone pose overrides

        // Record bone pose change for undo/redo (called from viewport gizmo manipulation)
        void RecordBonePoseChange(const std::string& boneName, const BonePose& oldPose, const BonePose& newPose);

    private:
        // UI Sections
        void RenderControlBar();      // Model selection, clip selection, playback controls
        void RenderViewport();        // 3D preview viewport
        void RenderTimelineRuler();   // Horizontal time ruler with scrubber
        void RenderTrackList();       // Bone tracks and keyframes

        // Bone track helpers
        void RenderBoneTrack(const Boom::Joint& joint, float duration);

        // Audio track helper
        void RenderAudioTrack(float duration);

        // 3D Viewport Rendering
        void UpdateCamera();
        void HandleCameraControls();
        void HandleBonePicking();  // Mouse picking for bone selection in viewport
        void HandleGizmo(const ImVec2& viewportMin, const ImVec2& viewportSize);  // Transform gizmo for bone manipulation
        void RenderModel();
        void RenderSkeleton();
        void RenderGrid();

        // Camera orbit controls
        void ResetCamera();
        void FrameModel(); // Auto-adjust camera to fit model in view

        // Model loading (standalone mode)
        void LoadModel(const std::string& modelPath);
        void ClearModel();

        // Bone manipulation helpers
        glm::mat4 GetBoneWorldTransform(const std::string& boneName);  // Get bone's world matrix
        std::string GetParentBoneName(const std::string& boneName);    // Find parent bone name
        void ApplyManualBonePosesToTransforms(std::vector<glm::mat4>& transforms);  // Apply manual poses to skinning transforms

        // Keyframe recording
        Boom::KeyFrame CaptureCurrentBoneTransform(const std::string& boneName);  // Capture bone's current pose

    private:
        Editor* m_Owner = nullptr;
        Boom::AppInterface* m_App = nullptr;
        Boom::AppContext* m_Ctx = nullptr;

        // Loaded model (from selected entity OR standalone)
        std::shared_ptr<Boom::Model> m_Model;
        std::shared_ptr<Boom::Animator> m_Animator;  // Our independent cloned animator
        std::shared_ptr<Boom::Animator> m_SourceAnimator;  // The original animator we cloned from (for change detection)
        entt::entity m_SourceEntityID = entt::null;  // Track which entity we cloned from
        bool m_HasModel = false;
        bool m_StandaloneMode = false; // true if model loaded directly (not from entity)
        std::string m_LoadedModelPath;

        // Viewport framebuffer
        GLuint m_FramebufferID = 0;
        GLuint m_TextureID = 0;
        GLuint m_DepthBufferID = 0;
        ImVec2 m_ViewportSize = { 800.0f, 400.0f };

        // Viewport splitter (resizable viewport/timeline split)
        float m_ViewportHeightRatio = 0.45f;  // User-adjustable ratio (0.2 to 0.8)
        bool m_IsDraggingSplitter = false;     // Is user dragging the splitter bar?

        // Compact mode (hides less-used controls for smaller screens)
        bool m_CompactMode = false;

        // Orbit camera
        glm::vec3 m_CameraPosition = glm::vec3(0.0f, 1.5f, 3.0f);
        glm::vec3 m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
        float m_CameraDistance = 3.0f;
        float m_CameraYaw = 0.0f;      // Horizontal rotation
        float m_CameraPitch = 0.0f;    // Vertical rotation
        bool m_IsOrbitingCamera = false;
        bool m_IsPanningCamera = false;  // Middle mouse button panning
        ImVec2 m_LastMousePos = { 0.0f, 0.0f };

        // Visualization settings
        bool m_ShowSkeleton = true;
        bool m_ShowGrid = true;
        bool m_ShowWireframe = false;
        float m_ModelScale = 1.0f;  // Preview scale adjustment

        // Timeline state
        float m_CurrentTime = 0.0f;
        float m_TimelineZoom = 1.0f;
        bool m_IsPlaying = false;
        bool m_Loop = true;  // Loop animation playback
        float m_PlaybackSpeed = 1.0f;  // Animation playback speed multiplier
        int m_SelectedClipIndex = -1;  // Currently selected animation clip (-1 = none)
        float m_LastFrameTime = 0.0f;  // For delta time calculation
        bool m_IsDraggingTimeline = false;  // Is user scrubbing the timeline?
        bool m_ClipModified = false;  // Track unsaved changes to current clip

        // Selected bone (for keyframe editing later)
        std::string m_SelectedBoneName;

        // Bone picking state (viewport 3D interaction)
        std::string m_HoveredBoneNameViewport;  // Bone currently hovered in 3D viewport
        ImVec2 m_ViewportMousePos = { 0.0f, 0.0f };  // Mouse position relative to viewport

        // Transform gizmo state
        int m_GizmoOperation = 120;  // ImGuizmo::ROTATE (120 = rotate, default for animation editing)
        int m_GizmoMode = 1;       // ImGuizmo::WORLD (1 = world space, 0 = local space)
        bool m_GizmoWasUsing = false;  // Track if gizmo was being used last frame
        bool m_UseSnap = false;    // Snap to grid
        float m_SnapValues[3] = { 0.1f, 0.1f, 0.1f };  // Snap grid size
        bool m_RotationOnlyMode = true;  // Rotation-only mode (prevents translation warping)

        // Manual bone pose overrides (for gizmo manipulation)
        std::map<std::string, BonePose> m_ManualBonePoses;  // Overrides animation data
        bool m_HasManualPoses = false;  // Flag to know if we need to apply overrides

        // Bone manipulation state (for undo/redo)
        std::string m_BoneBeingManipulated;  // Bone name currently being manipulated
        BonePose m_BonePoseBeforeManipulation;  // Pose before gizmo manipulation started
        bool m_HasPoseBeforeManipulation = false;  // Flag to track if we captured the before state

        // Keyframe interaction state
        bool m_IsDraggingKeyframe = false;
        std::string m_DraggedBoneName;
        size_t m_DraggedKeyframeIndex = 0;
        int m_HoveredKeyframeIndex = -1;  // -1 = no hover
        std::string m_HoveredBoneName;

        // Keyframe multiselect state (Unity-style)
        std::set<SelectedKeyframe> m_SelectedKeyframes;  // Currently selected keyframes
        SelectedKeyframe m_SelectionAnchor;              // Anchor for shift-click range selection
        bool m_HasSelectionAnchor = false;               // Is anchor valid?
        float m_MultiDragStartTime = 0.0f;               // Original time of dragged keyframe (for delta calculation)
        std::map<SelectedKeyframe, float> m_SelectedKeyframeOriginalTimes;  // Store original times during drag

        // Box/marquee selection state
        bool m_IsBoxSelecting = false;           // Currently drawing a selection box?
        ImVec2 m_BoxSelectStart = {0, 0};        // Start position of box selection (screen coords)
        ImVec2 m_BoxSelectEnd = {0, 0};          // Current end position of box selection
        bool m_BoxSelectAdditive = false;        // Ctrl held = add to selection instead of replace

        // Keyframe screen positions (populated during rendering for box selection)
        struct KeyframeScreenPos {
            std::string boneName;
            size_t keyframeIndex;
            ImVec2 screenPos;  // Center of the keyframe diamond
        };
        std::vector<KeyframeScreenPos> m_KeyframeScreenPositions;  // Cleared each frame

        // Audio event interaction state
        int m_SelectedAudioEventIndex = -1;  // -1 = no selection
        int m_HoveredAudioEventIndex = -1;   // -1 = no hover
        bool m_IsDraggingAudioEvent = false;
        float m_DraggedAudioEventOriginalTime = 0.0f;

        // Audio marker screen positions (for click detection)
        struct AudioMarkerScreenPos {
            size_t eventIndex;
            ImVec2 screenPos;  // Center of the marker
        };
        std::vector<AudioMarkerScreenPos> m_AudioMarkerScreenPositions;  // Cleared each frame

        // Undo/Redo system
        std::vector<KeyframeCommand> m_UndoStack;
        std::vector<KeyframeCommand> m_RedoStack;
        const size_t MAX_UNDO_HISTORY = 50;

        void ExecuteCommand(const KeyframeCommand& cmd);
        void Undo();
        void Redo();

        // Helper: Find keyframe index by timestamp (returns -1 if not found)
        int FindKeyframeByTimestamp(const std::string& boneName, float timestamp, float tolerance = 0.001f);

        // Helper: Execute a single command (used internally, does NOT modify undo/redo stacks)
        void ExecuteSingleCommand(const KeyframeCommand& cmd);

        // Helper: Undo a single command (used internally)
        void UndoSingleCommand(const KeyframeCommand& cmd);

        // Keyframe multiselect helpers
        bool IsKeyframeSelected(const std::string& boneName, size_t index) const;
        void SelectKeyframe(const std::string& boneName, size_t index, bool addToSelection = false);
        void DeselectKeyframe(const std::string& boneName, size_t index);
        void ToggleKeyframeSelection(const std::string& boneName, size_t index);
        void ClearKeyframeSelection();
        void DeleteSelectedKeyframes();
    };

} // namespace EditorUI
