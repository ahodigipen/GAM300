#pragma once

#include "VideoPlayer.h"
#include "ECS/ECS.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>

// Suppress C4251 warnings for STL types in DLL interface
// These are internal implementation details and are safe to ignore
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

namespace Boom {

    // Forward declarations
    struct GraphicsRenderer;

    /**
     * VideoSystem - Manages video playback for all entities with VideoComponent
     *
     * This system handles:
     * - Loading/unloading videos when VideoComponent is added/removed
     * - Updating video playback each frame
     * - Rendering video frames to textures that can be displayed
     */
    class BOOM_API VideoSystem {
    public:
        VideoSystem() = default;
        ~VideoSystem() = default;

        // Delete copy/move (singleton-like usage)
        VideoSystem(const VideoSystem&) = delete;
        VideoSystem& operator=(const VideoSystem&) = delete;

        /**
         * Initialize the video system
         * Called once at startup
         */
        void Initialize();

        /**
         * Shutdown the video system
         * Releases all video resources
         */
        void Shutdown();

        /**
         * Update all video players
         * Should be called each frame with delta time
         * @param scene The entity registry
         * @param deltaTime Time since last frame in seconds
         */
        void Update(EntityRegistry& scene, double deltaTime);

        /**
         * Render video components
         * Call this during the render pass to draw videos
         * @param scene The entity registry
         * @param renderer The graphics renderer
         */
        void Render(EntityRegistry& scene, GraphicsRenderer& renderer);

        /**
         * Get the video player for a specific entity
         * @param entity The entity ID
         * @return Pointer to VideoPlayer, or nullptr if not found
         */
        VideoPlayer* GetPlayer(EntityID entity);

        /**
         * Manually load a video for an entity
         * Usually handled automatically, but can be called explicitly
         */
        bool LoadVideo(EntityID entity, const std::string& path);

        /**
         * Manually unload a video for an entity
         */
        void UnloadVideo(EntityID entity);

        /**
         * Play video for an entity
         */
        void Play(EntityID entity);

        /**
         * Pause video for an entity
         */
        void Pause(EntityID entity);

        /**
         * Stop video for an entity
         */
        void Stop(EntityID entity);

        /**
         * Get the base path for video resources
         */
        const std::string& GetVideoBasePath() const { return m_VideoBasePath; }
        void SetVideoBasePath(const std::string& path) { m_VideoBasePath = path; }

    private:
        /**
         * Synchronize video players with VideoComponents in the scene
         * Creates players for new components, removes players for deleted components
         */
        void SyncWithScene(EntityRegistry& scene);

        /**
         * Handle playOnStart logic for newly loaded videos
         */
        void HandlePlayOnStart(EntityRegistry& scene);

        // Map of entity ID to video player
        std::unordered_map<uint32_t, std::unique_ptr<VideoPlayer>> m_Players;

        // Set of entities that have been initialized (for playOnStart tracking)
        std::unordered_set<uint32_t> m_InitializedEntities;

        // Base path for video resources (e.g., "Resources/Videos/")
        std::string m_VideoBasePath = "Resources/Videos/";

        // Flag to track if system is initialized
        bool m_Initialized = false;
    };

} // namespace Boom

#ifdef _MSC_VER
#pragma warning(pop)
#endif
