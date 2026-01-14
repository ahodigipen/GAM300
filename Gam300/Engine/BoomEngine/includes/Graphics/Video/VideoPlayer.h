#pragma once

// Download pl_mpeg.h from: https://raw.githubusercontent.com/phoboslab/pl_mpeg/master/pl_mpeg.h
// Place it in this directory (Graphics/Video/pl_mpeg.h)

#include "BoomProperties.h"
#include <string>
#include <memory>
#include <functional>

// Forward declare pl_mpeg main type (actual header included in .cpp)
// Only plm_t is needed in the header; other types are only used internally
struct plm_t;

namespace Boom {

    // Video playback states
    enum class VideoState {
        Stopped,
        Playing,
        Paused
    };

    // Video player class - handles MPEG1 video decoding using pl_mpeg
    class BOOM_API VideoPlayer {
    public:
        VideoPlayer();
        ~VideoPlayer();

        // Prevent copying (pl_mpeg context is not copyable)
        VideoPlayer(const VideoPlayer&) = delete;
        VideoPlayer& operator=(const VideoPlayer&) = delete;

        // Move semantics
        VideoPlayer(VideoPlayer&& other) noexcept;
        VideoPlayer& operator=(VideoPlayer&& other) noexcept;

        // Load video from file path
        bool Load(const std::string& filePath);

        // Unload/close the video
        void Unload();

        // Update decoding (call every frame with delta time)
        void Update(double deltaTime);

        // Playback controls
        void Play();
        void Pause();
        void Stop();
        void SetLoop(bool loop);
        void Seek(double time);
        void Rewind();

        // State queries
        bool IsLoaded() const { return m_PLM != nullptr; }
        bool IsPlaying() const { return m_State == VideoState::Playing; }
        bool IsPaused() const { return m_State == VideoState::Paused; }
        bool IsStopped() const { return m_State == VideoState::Stopped; }
        bool HasEnded() const;
        VideoState GetState() const { return m_State; }

        // Video properties
        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }
        double GetDuration() const { return m_Duration; }
        double GetCurrentTime() const { return m_CurrentTime; }
        double GetFramerate() const { return m_Framerate; }
        int GetSampleRate() const { return m_SampleRate; }

        // Frame data access (RGB format)
        const uint8_t* GetFrameData() const { return m_FrameBuffer.get(); }
        size_t GetFrameDataSize() const { return m_FrameBufferSize; }
        bool HasNewFrame() const { return m_HasNewFrame; }
        void ClearNewFrameFlag() { m_HasNewFrame = false; }

        // OpenGL texture (created/updated by VideoPlayer)
        uint32_t GetTextureID() const { return m_TextureID; }
        void UpdateTexture(); // Call after Update() to upload new frame to GPU

        // Audio callback (optional - for FMOD integration)
        using AudioCallback = std::function<void(const float* samples, size_t count, int channels, int sampleRate)>;
        void SetAudioCallback(AudioCallback callback) { m_AudioCallback = std::move(callback); }

        // Volume control (0.0 - 1.0)
        void SetVolume(float volume) { m_Volume = glm::clamp(volume, 0.0f, 1.0f); }
        float GetVolume() const { return m_Volume; }

        // Playback speed (1.0 = normal)
        void SetPlaybackSpeed(float speed) { m_PlaybackSpeed = glm::clamp(speed, 0.1f, 4.0f); }
        float GetPlaybackSpeed() const { return m_PlaybackSpeed; }

    private:
        void CreateTexture();
        void DestroyTexture();
        void DecodeFrame();

        // pl_mpeg context
        plm_t* m_PLM = nullptr;

        // Video properties
        int m_Width = 0;
        int m_Height = 0;
        double m_Duration = 0.0;
        double m_CurrentTime = 0.0;
        double m_Framerate = 0.0;
        int m_SampleRate = 0;

        // Playback state
        VideoState m_State = VideoState::Stopped;
        bool m_Loop = false;
        float m_Volume = 1.0f;
        float m_PlaybackSpeed = 1.0f;

        // Frame buffer (RGB data)
        std::unique_ptr<uint8_t[]> m_FrameBuffer;
        size_t m_FrameBufferSize = 0;
        bool m_HasNewFrame = false;

        // OpenGL texture
        uint32_t m_TextureID = 0;
        bool m_TextureCreated = false;

        // Audio callback
        AudioCallback m_AudioCallback;

        // File path for reload
        std::string m_FilePath;
    };

} // namespace Boom
