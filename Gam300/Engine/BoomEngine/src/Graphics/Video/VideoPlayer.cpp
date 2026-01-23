// VideoPlayer.cpp - MPEG1 video playback using pl_mpeg
#include "Core.h"
#define PL_MPEG_IMPLEMENTATION
#include "Graphics/Video/pl_mpeg.h"  // You must download this from GitHub!
#include "Graphics/Video/VideoPlayer.h"


namespace Boom {

    VideoPlayer::VideoPlayer() = default;

    VideoPlayer::~VideoPlayer() {
        Unload();
        DestroyTexture();
    }

    VideoPlayer::VideoPlayer(VideoPlayer&& other) noexcept {
        *this = std::move(other);
    }

    VideoPlayer& VideoPlayer::operator=(VideoPlayer&& other) noexcept {
        if (this != &other) {
            Unload();
            DestroyTexture();

            m_PLM = other.m_PLM;
            m_Width = other.m_Width;
            m_Height = other.m_Height;
            m_Duration = other.m_Duration;
            m_CurrentTime = other.m_CurrentTime;
            m_Framerate = other.m_Framerate;
            m_SampleRate = other.m_SampleRate;
            m_State = other.m_State;
            m_Loop = other.m_Loop;
            m_Volume = other.m_Volume;
            m_PlaybackSpeed = other.m_PlaybackSpeed;
            m_FrameBuffer = std::move(other.m_FrameBuffer);
            m_FrameBufferSize = other.m_FrameBufferSize;
            m_HasNewFrame = other.m_HasNewFrame;
            m_TextureID = other.m_TextureID;
            m_TextureCreated = other.m_TextureCreated;
            m_AudioCallback = std::move(other.m_AudioCallback);
            m_FilePath = std::move(other.m_FilePath);

            // Clear other's ownership
            other.m_PLM = nullptr;
            other.m_TextureID = 0;
            other.m_TextureCreated = false;
        }
        return *this;
    }

    bool VideoPlayer::Load(const std::string& filePath) {
        // Clean up any existing video
        Unload();

        // Open the MPEG file
        m_PLM = plm_create_with_filename(filePath.c_str());
        if (!m_PLM) {
            BOOM_ERROR("[VideoPlayer] Failed to load video: {}", filePath);
            return false;
        }

        // Wait for headers to be parsed
        if (!plm_has_headers(m_PLM)) {
            BOOM_ERROR("[VideoPlayer] Video file has no valid headers: {}", filePath);
            plm_destroy(m_PLM);
            m_PLM = nullptr;
            return false;
        }

        // Get video properties
        m_Width = plm_get_width(m_PLM);
        m_Height = plm_get_height(m_PLM);
        m_Duration = plm_get_duration(m_PLM);
        m_Framerate = plm_get_framerate(m_PLM);
        m_SampleRate = plm_get_samplerate(m_PLM);
        m_FilePath = filePath;

        // Allocate frame buffer (RGB format: 3 bytes per pixel)
        m_FrameBufferSize = static_cast<size_t>(m_Width) * m_Height * 3;
        m_FrameBuffer = std::make_unique<uint8_t[]>(m_FrameBufferSize);

        // Set loop behavior
        plm_set_loop(m_PLM, m_Loop ? 1 : 0);

        // Disable audio decoding by default (can be enabled if needed)
        plm_set_audio_enabled(m_PLM, 0);

        m_State = VideoState::Stopped;
        m_CurrentTime = 0.0;
        m_HasNewFrame = false;

        BOOM_INFO("[VideoPlayer] Loaded video: {} ({}x{}, {:.2f}s, {:.2f}fps)",
                  filePath, m_Width, m_Height, m_Duration, m_Framerate);

        // Create OpenGL texture
        CreateTexture();

        return true;
    }

    void VideoPlayer::Unload() {
        if (m_PLM) {
            plm_destroy(m_PLM);
            m_PLM = nullptr;
        }
        m_FrameBuffer.reset();
        m_FrameBufferSize = 0;
        m_Width = 0;
        m_Height = 0;
        m_Duration = 0.0;
        m_CurrentTime = 0.0;
        m_Framerate = 0.0;
        m_SampleRate = 0;
        m_State = VideoState::Stopped;
        m_HasNewFrame = false;
        m_FilePath.clear();
    }

    void VideoPlayer::Update(double deltaTime) {
        if (!m_PLM || m_State != VideoState::Playing) {
            return;
        }

        // Apply playback speed
        double adjustedDelta = deltaTime * m_PlaybackSpeed;

        // Decode video frame
        plm_frame_t* frame = plm_decode_video(m_PLM);
        if (frame) {
            // Convert YCrCb to RGB and store in frame buffer
            plm_frame_to_rgb(frame, m_FrameBuffer.get(), m_Width * 3);
            m_HasNewFrame = true;
        }

        // Update current time
        m_CurrentTime = plm_get_time(m_PLM);

        // Check if video has ended
        if (plm_has_ended(m_PLM)) {
            if (m_Loop) {
                Rewind();
            } else {
                m_State = VideoState::Stopped;
            }
        }
    }

    void VideoPlayer::DecodeFrame() {
        if (!m_PLM || !m_FrameBuffer) {
            return;
        }

        // Decode a single video frame (for manual frame-by-frame control)
        plm_frame_t* frame = plm_decode_video(m_PLM);
        if (frame) {
            // Convert YCrCb to RGB
            plm_frame_to_rgb(frame, m_FrameBuffer.get(), m_Width * 3);
            m_HasNewFrame = true;
        }
    }

    void VideoPlayer::Play() {
        if (!m_PLM) return;

        if (m_State == VideoState::Stopped) {
            // If stopped, rewind first
            plm_rewind(m_PLM);
            m_CurrentTime = 0.0;
        }
        m_State = VideoState::Playing;
    }

    void VideoPlayer::Pause() {
        if (m_State == VideoState::Playing) {
            m_State = VideoState::Paused;
        }
    }

    void VideoPlayer::Stop() {
        if (!m_PLM) return;

        m_State = VideoState::Stopped;
        plm_rewind(m_PLM);
        m_CurrentTime = 0.0;
    }

    void VideoPlayer::SetLoop(bool loop) {
        m_Loop = loop;
        if (m_PLM) {
            plm_set_loop(m_PLM, loop ? 1 : 0);
        }
    }

    void VideoPlayer::Seek(double time) {
        if (!m_PLM) return;

        // Clamp to valid range
        time = glm::clamp(time, 0.0, m_Duration);

        // PLM seek - second parameter: 1 = seek exact, 0 = seek to nearest keyframe (faster)
        plm_seek(m_PLM, time, 1);
        m_CurrentTime = plm_get_time(m_PLM);
    }

    void VideoPlayer::Rewind() {
        if (!m_PLM) return;

        plm_rewind(m_PLM);
        m_CurrentTime = 0.0;
    }

    bool VideoPlayer::HasEnded() const {
        return m_PLM ? plm_has_ended(m_PLM) != 0 : true;
    }

    void VideoPlayer::CreateTexture() {
        if (m_TextureCreated || m_Width <= 0 || m_Height <= 0) {
            return;
        }

        glGenTextures(1, &m_TextureID);
        glBindTexture(GL_TEXTURE_2D, m_TextureID);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Allocate texture storage (RGB format)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m_Width, m_Height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

        glBindTexture(GL_TEXTURE_2D, 0);
        m_TextureCreated = true;

        BOOM_INFO("[VideoPlayer] Created video texture (ID: {}, {}x{})", m_TextureID, m_Width, m_Height);
    }

    void VideoPlayer::DestroyTexture() {
        if (m_TextureCreated && m_TextureID != 0) {
            glDeleteTextures(1, &m_TextureID);
            m_TextureID = 0;
            m_TextureCreated = false;
        }
    }

    void VideoPlayer::UpdateTexture() {
        if (!m_TextureCreated || !m_HasNewFrame || !m_FrameBuffer) {
            return;
        }

        glBindTexture(GL_TEXTURE_2D, m_TextureID);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGB, GL_UNSIGNED_BYTE, m_FrameBuffer.get());
        glBindTexture(GL_TEXTURE_2D, 0);

        m_HasNewFrame = false;
    }

} // namespace Boom
