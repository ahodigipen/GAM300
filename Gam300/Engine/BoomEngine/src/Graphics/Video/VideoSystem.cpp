// VideoSystem.cpp - Manages video playback for all VideoComponent entities

#include "Core.h"
#include "Graphics/Video/VideoSystem.h"
#include "Graphics/Renderer.h"
#include <filesystem>

namespace Boom {

    void VideoSystem::Initialize() {
        if (m_Initialized) return;

        BOOM_INFO("[VideoSystem] Initializing video system");
        m_Players.clear();
        m_InitializedEntities.clear();
        m_Initialized = true;
    }

    void VideoSystem::Shutdown() {
        if (!m_Initialized) return;

        BOOM_INFO("[VideoSystem] Shutting down video system");

        // Stop and unload all videos
        for (auto& [entityId, player] : m_Players) {
            if (player) {
                player->Stop();
                player->Unload();
            }
        }
        m_Players.clear();
        m_InitializedEntities.clear();
        m_Initialized = false;
    }

    void VideoSystem::Update(EntityRegistry& scene, double deltaTime) {
        if (!m_Initialized) return;

        // Sync video players with VideoComponents in the scene
        SyncWithScene(scene);

        // Handle playOnStart for newly loaded videos
        HandlePlayOnStart(scene);

        // Update all video players
        auto view = scene.view<VideoComponent>();
        for (auto [entity, videoComp] : view.each()) {
            uint32_t entityId = static_cast<uint32_t>(entity);
            auto it = m_Players.find(entityId);

            if (it != m_Players.end() && it->second && it->second->IsLoaded()) {
                VideoPlayer& player = *it->second;

                // Update playback settings from component
                player.SetLoop(videoComp.loop);
                player.SetVolume(videoComp.volume);
                player.SetPlaybackSpeed(videoComp.playbackSpeed);
                player.SetRemoveBlackBackground(videoComp.removeBlackBackground);

                // Update the player (decodes frames)
                player.Update(deltaTime);

                // Update component state
                videoComp.isPlaying = player.IsPlaying();
                videoComp.currentTime = player.GetTickCount();

                // Upload new frame to texture
                if (player.HasNewFrame()) {
                    player.UpdateTexture();
                }
            }
        }
    }

    void VideoSystem::Render(EntityRegistry& scene, GraphicsRenderer& renderer) {
        if (!m_Initialized) return;

        auto view = scene.view<VideoComponent, TransformComponent>();
        for (auto [entity, videoComp, transformComp] : view.each()) {
            uint32_t entityId = static_cast<uint32_t>(entity);
            auto it = m_Players.find(entityId);

            if (it != m_Players.end() && it->second && it->second->IsLoaded()) {
                VideoPlayer& player = *it->second;
                uint32_t textureId = player.GetTextureID();

                if (textureId != 0) {
                    // Create a temporary texture wrapper
                    // The renderer expects a Texture (shared_ptr<Texture2D>)
                    // We need to wrap the raw OpenGL texture ID

                    // Apply brightness to tint color (multiply RGB, preserve alpha)
                    glm::vec4 adjustedTint = videoComp.tintColor;
                    adjustedTint.r *= videoComp.brightness;
                    adjustedTint.g *= videoComp.brightness;
                    adjustedTint.b *= videoComp.brightness;

                    if (videoComp.renderAs3D) {
                        // Render as a 3D quad in world space
                        renderer.DrawQuad(
                            Texture{}, // Placeholder - will need to create proper texture wrapper
                            transformComp.transform,
                            adjustedTint
                        );
                    } else {
                        // Render as 2D UI overlay
                        Transform2D transform2D(transformComp.transform);
                        renderer.DrawQuad(
                            Texture{}, // Placeholder
                            transform2D,
                            adjustedTint
                        );
                    }
                }
            }
        }
    }

    VideoPlayer* VideoSystem::GetPlayer(EntityID entity) {
        uint32_t entityId = static_cast<uint32_t>(entity);
        auto it = m_Players.find(entityId);
        return (it != m_Players.end()) ? it->second.get() : nullptr;
    }

    bool VideoSystem::LoadVideo(EntityID entity, const std::string& path) {
        uint32_t entityId = static_cast<uint32_t>(entity);

        // Create or get player
        auto& player = m_Players[entityId];
        if (!player) {
            player = std::make_unique<VideoPlayer>();
        }

        // Build full path
        std::string fullPath = m_VideoBasePath + path;

        // Check if file exists
        if (!std::filesystem::exists(fullPath)) {
            BOOM_ERROR("[VideoSystem] Video file not found: {}", fullPath);
            return false;
        }

        return player->Load(fullPath);
    }

    void VideoSystem::UnloadVideo(EntityID entity) {
        uint32_t entityId = static_cast<uint32_t>(entity);
        auto it = m_Players.find(entityId);
        if (it != m_Players.end()) {
            if (it->second) {
                it->second->Unload();
            }
            m_Players.erase(it);
            m_InitializedEntities.erase(entityId);
        }
    }

    void VideoSystem::Play(EntityID entity) {
        VideoPlayer* player = GetPlayer(entity);
        if (player && player->IsLoaded()) {
            player->Play();
        }
    }

    void VideoSystem::Pause(EntityID entity) {
        VideoPlayer* player = GetPlayer(entity);
        if (player) {
            player->Pause();
        }
    }

    void VideoSystem::Stop(EntityID entity) {
        VideoPlayer* player = GetPlayer(entity);
        if (player) {
            player->Stop();
        }
    }

    void VideoSystem::OnSceneChange() {
        BOOM_INFO("[VideoSystem] Scene change - resetting video system state");

        // Stop and unload all current videos
        for (auto& [entityId, player] : m_Players) {
            if (player) {
                player->Stop();
                player->Unload();
            }
        }

        // Clear all tracking
        m_Players.clear();
        m_InitializedEntities.clear();

        BOOM_INFO("[VideoSystem] Video system state reset complete");
    }

    void VideoSystem::SyncWithScene(EntityRegistry& scene) {
        // Get all current entities with VideoComponent
        std::unordered_set<uint32_t> currentEntities;
        auto view = scene.view<VideoComponent>();

        for (auto [entity, videoComp] : view.each()) {
            uint32_t entityId = static_cast<uint32_t>(entity);
            currentEntities.insert(entityId);

            // Check if we need to load/reload the video
            auto it = m_Players.find(entityId);
            if (it == m_Players.end() || !it->second) {
                // New entity - create player
                m_Players[entityId] = std::make_unique<VideoPlayer>();
            }

            VideoPlayer& player = *m_Players[entityId];

            // Check if video path changed or needs loading
            if (!videoComp.videoPath.empty()) {
                std::string fullPath = m_VideoBasePath + videoComp.videoPath;

                // Load if not loaded or path changed
                if (!player.IsLoaded()) {
                    if (std::filesystem::exists(fullPath)) {
                        player.Load(fullPath);
                    }
                }
            }
        }

        // Remove players for entities that no longer have VideoComponent
        for (auto it = m_Players.begin(); it != m_Players.end(); ) {
            if (currentEntities.find(it->first) == currentEntities.end()) {
                BOOM_INFO("[VideoSystem] Removing video player for entity {}", it->first);
                it = m_Players.erase(it);
                m_InitializedEntities.erase(it->first);
            } else {
                ++it;
            }
        }
    }

    void VideoSystem::HandlePlayOnStart(EntityRegistry& scene) {
        auto view = scene.view<VideoComponent>();

        for (auto [entity, videoComp] : view.each()) {
            uint32_t entityId = static_cast<uint32_t>(entity);

            // Skip if already initialized
            if (m_InitializedEntities.count(entityId) > 0) {
                continue;
            }

            auto it = m_Players.find(entityId);
            if (it != m_Players.end() && it->second && it->second->IsLoaded()) {
                // Mark as initialized
                m_InitializedEntities.insert(entityId);

                // Handle playOnStart
                if (videoComp.playOnStart) {
                    it->second->Play();
                    BOOM_INFO("[VideoSystem] Auto-playing video for entity {}", entityId);
                }
            }
        }
    }

} // namespace Boom
