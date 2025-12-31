#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "Vendors/imgui/imgui.h"
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <entt/entity/entity.hpp>

namespace Boom {
    struct AppContext;
    struct AppInterface;
    struct Model;
    struct Animator;
    struct Joint;
}

namespace EditorUI {
    class Editor;

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

    private:
        // UI Sections
        void RenderControlBar();      // Model selection, clip selection, playback controls
        void RenderViewport();        // 3D preview viewport
        void RenderTimelineRuler();   // Horizontal time ruler with scrubber
        void RenderTrackList();       // Bone tracks and keyframes

        // Bone track helpers
        void RenderBoneTrack(const Boom::Joint& joint, float duration);

        // 3D Viewport Rendering
        void UpdateCamera();
        void HandleCameraControls();
        void RenderModel();
        void RenderSkeleton();
        void RenderGrid();

        // Camera orbit controls
        void ResetCamera();
        void FrameModel(); // Auto-adjust camera to fit model in view

        // Model loading (standalone mode)
        void LoadModel(const std::string& modelPath);
        void ClearModel();

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

        // Orbit camera
        glm::vec3 m_CameraPosition = glm::vec3(0.0f, 1.5f, 3.0f);
        glm::vec3 m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
        float m_CameraDistance = 3.0f;
        float m_CameraYaw = 0.0f;      // Horizontal rotation
        float m_CameraPitch = 0.0f;    // Vertical rotation
        bool m_IsOrbitingCamera = false;
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

        // Selected bone (for keyframe editing later)
        std::string m_SelectedBoneName;
    };

} // namespace EditorUI
