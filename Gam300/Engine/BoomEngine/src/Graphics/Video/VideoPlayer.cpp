// VideoPlayer.cpp - MPEG1 video playback using pl_mpeg with FMOD audio
#include "Core.h"

// Suppress warnings from third-party pl_mpeg library
#pragma warning(push)
#pragma warning(disable: 4305)  // truncation from double to float
#pragma warning(disable: 4244)  // conversion from 'int' to 'uint8_t', possible loss of data
#pragma warning(disable: 4267)  // conversion from 'size_t' to 'long', possible loss of data
#pragma warning(disable: 26451) // arithmetic overflow

#define PL_MPEG_IMPLEMENTATION
#include "Graphics/Video/pl_mpeg.h"

#pragma warning(pop)

#include "Graphics/Video/VideoPlayer.h"
#include <fmod.hpp>
#include <algorithm>
#include <cstring>

// Global FMOD system pointer - defined at end of file, declared here
namespace Boom {
    extern FMOD::System* g_FMODSystem;
}

// Helper to get FMOD system - accesses global pointer set by SoundEngine
static FMOD::System* GetFMODSystemInternal() {
    return Boom::g_FMODSystem;
}

// Static audio callback for pl_mpeg - called when audio frames are decoded
static void PLMAudioCallback(plm_t* plm, plm_samples_t* samples, void* user) {

    (void)plm;

    Boom::VideoPlayer* player = static_cast<Boom::VideoPlayer*>(user);
    if (player && samples) {
        // pl_mpeg provides interleaved stereo samples (left, right, left, right...)
        // Each frame has PLM_AUDIO_SAMPLES_PER_FRAME (1152) samples per channel
        player->OnAudioDecoded(samples->interleaved, samples->count * 2);
    }
}

// FMOD PCM read callback - called by FMOD when it needs more audio data
static FMOD_RESULT F_CALL FMODPCMReadCallback(
    FMOD_SOUND* sound, void* data, unsigned int datalen)
{
    // Get the VideoPlayer instance from user data
    Boom::VideoPlayer* player = nullptr;
    if (sound) {
        FMOD::Sound* fmodSound = reinterpret_cast<FMOD::Sound*>(sound);
        void* userData = nullptr;
        fmodSound->getUserData(&userData);
        player = static_cast<Boom::VideoPlayer*>(userData);
    }

    if (!player || player->IsAudioShuttingDown()) {
        // No player or shutting down, fill with silence
        std::memset(data, 0, datalen);
        return FMOD_OK;
    }

    // Use the public ReadAudioSamples method to get audio data
    float* outBuffer = static_cast<float*>(data);
    size_t samplesRequested = datalen / sizeof(float);
    size_t samplesRead = player->ReadAudioSamples(outBuffer, samplesRequested);

    // Fill any remaining with silence (underrun)
    if (samplesRead < samplesRequested) {
        std::memset(outBuffer + samplesRead, 0, (samplesRequested - samplesRead) * sizeof(float));
    }

    return FMOD_OK;
}

namespace Boom {

    VideoPlayer::VideoPlayer() {
        // Initialize audio buffer
        m_AudioBuffer.resize(AUDIO_BUFFER_SIZE, 0.0f);
    }

    VideoPlayer::~VideoPlayer() {
        ShutdownAudio();
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

            // Audio members
            m_AudioEnabled = other.m_AudioEnabled;
            m_HasAudioTrack = other.m_HasAudioTrack;
            m_FMODSound = other.m_FMODSound;
            m_FMODChannel = other.m_FMODChannel;
            m_AudioBuffer = std::move(other.m_AudioBuffer);
            m_AudioWritePos.store(other.m_AudioWritePos.load());
            m_AudioReadPos.store(other.m_AudioReadPos.load());
            m_AudioBufferReady.store(other.m_AudioBufferReady.load());
            m_AudioShuttingDown.store(other.m_AudioShuttingDown.load());
            m_AudioChannels = other.m_AudioChannels;

            // CRITICAL: Update callback user data to point to this object
            // instead of the moved-from object, so callbacks access the correct instance
            if (m_FMODSound && GetFMODSystemInternal()) {
                m_FMODSound->setUserData(this);
            }
            if (m_PLM) {
                plm_set_audio_decode_callback(m_PLM, PLMAudioCallback, this);
            }

            // Timing members
            m_AccumulatedTime = other.m_AccumulatedTime;
            m_SecondsPerFrame = other.m_SecondsPerFrame;

            // Clear other's ownership
            other.m_PLM = nullptr;
            other.m_TextureID = 0;
            other.m_TextureCreated = false;
            other.m_FMODSound = nullptr;
            other.m_FMODChannel = nullptr;
            other.m_AudioShuttingDown.store(true, std::memory_order_release);
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

        // Calculate frame timing for proper playback speed
        m_SecondsPerFrame = (m_Framerate > 0.0) ? (1.0 / m_Framerate) : (1.0 / 30.0);
        m_AccumulatedTime = 0.0;

        // Check for audio track
        m_HasAudioTrack = (plm_get_num_audio_streams(m_PLM) > 0) && (m_SampleRate > 0);

        // 1. Raw Buffer: RGB (3 bytes) for the decoder to write into
        size_t rgbSize = static_cast<size_t>(m_Width) * m_Height * 3;
        m_RawRGBBuffer = std::make_unique<uint8_t[]>(rgbSize);

        // 2. Texture Buffer: RGBA (4 bytes) for OpenGL upload (supports transparency)
        m_FrameBufferSize = static_cast<size_t>(m_Width) * m_Height * 4;
        m_FrameBuffer = std::make_unique<uint8_t[]>(m_FrameBufferSize);

        // Set loop behavior
        plm_set_loop(m_PLM, m_Loop ? 1 : 0);

        // Initialize audio if available and enabled
        if (m_HasAudioTrack && m_AudioEnabled) {
            InitAudio();
        } else {
            plm_set_audio_enabled(m_PLM, 0);
        }

        m_State = VideoState::Stopped;
        m_CurrentTime = 0.0;
        m_HasNewFrame = false;

        BOOM_INFO("[VideoPlayer] Loaded video: {} ({}x{}, {:.2f}s, {:.2f}fps, audio: {})",
                  filePath, m_Width, m_Height, m_Duration, m_Framerate,
                  m_HasAudioTrack ? "yes" : "no");

        // Create OpenGL texture
        CreateTexture();

        return true;
    }

    void VideoPlayer::InitAudio() {
        if (!m_PLM || !m_HasAudioTrack || m_FMODSound) {
            return;
        }

        // Clear shutdown flag - we're initializing fresh
        m_AudioShuttingDown.store(false, std::memory_order_release);

        // Reset audio buffer
        m_AudioWritePos.store(0, std::memory_order_release);
        m_AudioReadPos.store(0, std::memory_order_release);
        m_AudioBufferReady.store(false, std::memory_order_release);
        std::fill(m_AudioBuffer.begin(), m_AudioBuffer.end(), 0.0f);

        // Enable audio decoding in pl_mpeg and set callback
        plm_set_audio_enabled(m_PLM, 1);
        plm_set_audio_decode_callback(m_PLM, PLMAudioCallback, this);

        // Create FMOD sound with PCM read callback
        FMOD::System* fmodSystem = GetFMODSystemInternal();
        if (!fmodSystem) {
            BOOM_WARN("[VideoPlayer] FMOD system not available, audio disabled");
            plm_set_audio_enabled(m_PLM, 0);
            return;
        }

        // Set up FMOD sound creation info for streaming PCM
        FMOD_CREATESOUNDEXINFO exinfo = {};
        exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
        exinfo.numchannels = m_AudioChannels;  // Stereo
        exinfo.defaultfrequency = m_SampleRate > 0 ? m_SampleRate : 44100;
        exinfo.decodebuffersize = 4096;  // Decode buffer size in samples
        exinfo.length = exinfo.defaultfrequency * exinfo.numchannels * sizeof(float) * 10;  // 10 seconds of audio
        exinfo.format = FMOD_SOUND_FORMAT_PCMFLOAT;
        exinfo.pcmreadcallback = FMODPCMReadCallback;
        exinfo.userdata = this;

        FMOD_RESULT result = fmodSystem->createStream(
            nullptr,  // No filename - we provide PCM data
            FMOD_OPENUSER | FMOD_LOOP_NORMAL | FMOD_2D,
            &exinfo,
            &m_FMODSound
        );

        if (result != FMOD_OK || !m_FMODSound) {
            BOOM_WARN("[VideoPlayer] Failed to create FMOD sound for video audio: {}", static_cast<int>(result));
            plm_set_audio_enabled(m_PLM, 0);
            m_FMODSound = nullptr;
            return;
        }

        BOOM_INFO("[VideoPlayer] Audio initialized (sample rate: {}, channels: {})",
            exinfo.defaultfrequency, m_AudioChannels);
    }

    void VideoPlayer::ShutdownAudio() {
        // Signal shutdown FIRST - this tells the FMOD callback to stop
        // accessing this VideoPlayer immediately and return silence instead.
        // This must happen before any other cleanup to prevent race conditions.
        m_AudioShuttingDown.store(true, std::memory_order_release);

        // Check if FMOD system is still valid - if not, the sounds have already
        // been cleaned up when the system was destroyed, so just clear our pointers
        FMOD::System* fmodSystem = GetFMODSystemInternal();

        // Stop and release FMOD channel/sound only if FMOD system is still alive
        if (m_FMODChannel) {
            if (fmodSystem) {
                m_FMODChannel->stop();
            }
            m_FMODChannel = nullptr;
        }

        if (m_FMODSound) {
            if (fmodSystem) {
                // Clear user data as additional safety
                m_FMODSound->setUserData(nullptr);
                m_FMODSound->release();
            }
            m_FMODSound = nullptr;
        }

        // Disable audio in pl_mpeg
        if (m_PLM) {
            plm_set_audio_enabled(m_PLM, 0);
            plm_set_audio_decode_callback(m_PLM, nullptr, nullptr);
        }

        // Reset buffer
        m_AudioWritePos.store(0, std::memory_order_release);
        m_AudioReadPos.store(0, std::memory_order_release);
        m_AudioBufferReady.store(false, std::memory_order_release);
    }

    void VideoPlayer::OnAudioDecoded(const float* samples, size_t count) {
        if (!samples || count == 0 || m_AudioBuffer.empty()) {
            return;
        }

        const size_t bufferSize = m_AudioBuffer.size();
        size_t writePos = m_AudioWritePos.load(std::memory_order_acquire);
        size_t readPos = m_AudioReadPos.load(std::memory_order_acquire);

        // Write samples to ring buffer
        for (size_t i = 0; i < count; ++i) {
            // Check for buffer overflow (leave some space to avoid catching up to read position)
            size_t nextWrite = (writePos + 1) % bufferSize;
            if (nextWrite == readPos) {
                // Buffer full - drop samples (this shouldn't happen often)
                break;
            }

            m_AudioBuffer[writePos] = samples[i];
            writePos = nextWrite;
        }

        m_AudioWritePos.store(writePos, std::memory_order_release);

        // Signal that we have audio data ready
        if (!m_AudioBufferReady.load(std::memory_order_acquire)) {
            // Wait until we have enough samples buffered before starting playback
            size_t available = 0;
            if (writePos >= readPos) {
                available = writePos - readPos;
            } else {
                available = bufferSize - readPos + writePos;
            }

            // Start playback when we have at least 0.1 seconds of audio buffered
            size_t minSamples = static_cast<size_t>(m_SampleRate * m_AudioChannels * 0.1);
            if (available >= minSamples) {
                m_AudioBufferReady.store(true, std::memory_order_release);
            }
        }

        // Call external callback if set (for custom processing)
        if (m_AudioCallback) {
            m_AudioCallback(samples, count / 2, m_AudioChannels, m_SampleRate);
        }
    }

    size_t VideoPlayer::ReadAudioSamples(float* outBuffer, size_t samplesRequested) {
        if (!outBuffer || samplesRequested == 0 || m_AudioBuffer.empty()) {
            return 0;
        }

        const size_t bufferSize = m_AudioBuffer.size();
        size_t readPos = m_AudioReadPos.load(std::memory_order_acquire);
        size_t writePos = m_AudioWritePos.load(std::memory_order_acquire);
        size_t samplesWritten = 0;

        while (samplesWritten < samplesRequested) {
            // Calculate available samples
            size_t available = 0;
            if (writePos >= readPos) {
                available = writePos - readPos;
            } else {
                available = bufferSize - readPos + writePos;
            }

            if (available == 0) {
                // Buffer underrun - return what we have
                break;
            }

            // Read sample and apply volume
            outBuffer[samplesWritten] = m_AudioBuffer[readPos] * m_Volume;
            samplesWritten++;
            readPos = (readPos + 1) % bufferSize;
        }

        m_AudioReadPos.store(readPos, std::memory_order_release);
        return samplesWritten;
    }

    void VideoPlayer::Unload() {
        // Shutdown audio first
        

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
        m_HasAudioTrack = false;
    }

    void VideoPlayer::Update(double deltaTime) {
        if (!m_PLM || m_State != VideoState::Playing) {
            return;
        }

        // Apply playback speed to delta time
        double adjustedDelta = deltaTime * m_PlaybackSpeed;

        plm_frame_t* frame = nullptr;

        // Decode audio if enabled - plm_decode() will call our audio callback
        // This only advances the audio decoder, not the video decoder
        if (m_HasAudioTrack && m_AudioEnabled && m_FMODSound) {
            plm_decode(m_PLM, adjustedDelta);
        }

        // Use frame-rate limiting for video playback
        // This ensures video plays at the correct speed regardless of audio
        m_AccumulatedTime += adjustedDelta;

        // Only decode video frames at the proper framerate
        // This prevents fast-forward playback and keeps audio/video in sync
        while (m_AccumulatedTime >= m_SecondsPerFrame) {
            m_AccumulatedTime -= m_SecondsPerFrame;

            // Decode one video frame
            frame = plm_decode_video(m_PLM);
            if (frame) {
                plm_frame_to_rgb(frame, m_RawRGBBuffer.get(), m_Width * 3);
                m_HasNewFrame = true;
            }

            // Prevent infinite loop if video has ended
            if (plm_has_ended(m_PLM)) {
                m_AccumulatedTime = 0.0;
                break;
            }
        }

        // Update current time from pl_mpeg's internal state
        m_CurrentTime = plm_get_time(m_PLM);

        // Check if video has ended
        if (plm_has_ended(m_PLM)) {
            if (m_Loop) {
                Rewind();
                // Restart audio playback for looped video (only if FMOD system is still valid)
                if (m_FMODChannel && GetFMODSystemInternal()) {
                    m_FMODChannel->setPaused(false);
                }
            } else {
                m_State = VideoState::Stopped;
                // Stop audio (only if FMOD system is still valid)
                if (m_FMODChannel && GetFMODSystemInternal()) {
                    m_FMODChannel->setPaused(true);
                }
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
            plm_frame_to_rgb(frame, m_RawRGBBuffer.get(), m_Width * 3);
            m_HasNewFrame = true;
        }
    }

    void VideoPlayer::Play() {
        if (!m_PLM) return;

        if (m_State == VideoState::Stopped) {
            // If stopped, rewind first
            Rewind();
        }

        m_State = VideoState::Playing;

        // Start audio playback if available
        if (m_FMODSound && m_HasAudioTrack && m_AudioEnabled) {
            FMOD::System* fmodSystem = GetFMODSystemInternal();
            if (fmodSystem) {
                if (!m_FMODChannel) {
                    // Start playing the sound
                    FMOD_RESULT result = fmodSystem->playSound(m_FMODSound, nullptr, false, &m_FMODChannel);
                    if (result == FMOD_OK && m_FMODChannel) {
                        m_FMODChannel->setVolume(m_Volume);
                        m_FMODChannel->setPaused(false);
                    }
                } else {
                    // Resume existing channel
                    m_FMODChannel->setPaused(false);
                }
            }
        }
    }

    void VideoPlayer::Pause() {
        if (m_State == VideoState::Playing) {
            m_State = VideoState::Paused;

            // Pause audio (only if FMOD system is still valid)
            if (m_FMODChannel && GetFMODSystemInternal()) {
                m_FMODChannel->setPaused(true);
            }
        }
    }

    void VideoPlayer::Stop() {
        if (!m_PLM) return;

        m_State = VideoState::Stopped;

        // Stop audio (only if FMOD system is still valid)
        if (m_FMODChannel) {
            FMOD::System* fmodSystem = GetFMODSystemInternal();
            if (fmodSystem) {
                m_FMODChannel->stop();
            }
            m_FMODChannel = nullptr;
        }

        // Reset audio buffer
        m_AudioWritePos.store(0, std::memory_order_release);
        m_AudioReadPos.store(0, std::memory_order_release);
        m_AudioBufferReady.store(false, std::memory_order_release);

        plm_rewind(m_PLM);
        m_CurrentTime = 0.0;
        m_AccumulatedTime = 0.0;
    }

    void VideoPlayer::SetLoop(bool loop) {
        m_Loop = loop;
        if (m_PLM) {
            plm_set_loop(m_PLM, loop ? 1 : 0);
        }
    }

    void VideoPlayer::SetVolume(float volume) {
        m_Volume = glm::clamp(volume, 0.0f, 1.0f);
        if (m_FMODChannel && GetFMODSystemInternal()) {
            m_FMODChannel->setVolume(m_Volume);
        }
    }

    void VideoPlayer::SetAudioEnabled(bool enabled) {
        if (m_AudioEnabled == enabled) return;

        m_AudioEnabled = enabled;

        if (!m_PLM || !m_HasAudioTrack) return;

        if (enabled) {
            InitAudio();
            if (m_State == VideoState::Playing) {
                // Start audio if video is already playing
                Play();
            }
        } else {
            ShutdownAudio();
        }
    }

    void VideoPlayer::Seek(double time) {
        if (!m_PLM) return;

        // Clamp to valid range
        time = glm::clamp(time, 0.0, m_Duration);

        // Stop audio during seek (only if FMOD system is still valid)
        bool wasPlaying = (m_State == VideoState::Playing);
        FMOD::System* fmodSystem = GetFMODSystemInternal();
        if (m_FMODChannel) {
            if (fmodSystem) {
                m_FMODChannel->stop();
            }
            m_FMODChannel = nullptr;
        }

        // Reset audio buffer
        m_AudioWritePos.store(0, std::memory_order_release);
        m_AudioReadPos.store(0, std::memory_order_release);
        m_AudioBufferReady.store(false, std::memory_order_release);

        // Reset frame timing
        m_AccumulatedTime = 0.0;

        // PLM seek - second parameter: 1 = seek exact, 0 = seek to nearest keyframe (faster)
        plm_seek(m_PLM, time, 1);
        m_CurrentTime = plm_get_time(m_PLM);

        // Resume audio if was playing (only if FMOD system is still valid)
        if (wasPlaying && m_FMODSound && m_HasAudioTrack && m_AudioEnabled && fmodSystem) {
            fmodSystem->playSound(m_FMODSound, nullptr, false, &m_FMODChannel);
            if (m_FMODChannel) {
                m_FMODChannel->setVolume(m_Volume);
            }
        }
    }

    void VideoPlayer::Rewind() {
        if (!m_PLM) return;

        // Stop audio channel (will be restarted when Play is called)
        // Only interact with FMOD if the system is still valid
        if (m_FMODChannel) {
            FMOD::System* fmodSystem = GetFMODSystemInternal();
            if (fmodSystem) {
                m_FMODChannel->stop();
            }
            m_FMODChannel = nullptr;
        }

        // Reset audio buffer
        m_AudioWritePos.store(0, std::memory_order_release);
        m_AudioReadPos.store(0, std::memory_order_release);
        m_AudioBufferReady.store(false, std::memory_order_release);

        plm_rewind(m_PLM);
        m_CurrentTime = 0.0;
        m_AccumulatedTime = 0.0;
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

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
        if (!m_TextureCreated || !m_HasNewFrame || !m_RawRGBBuffer || !m_FrameBuffer) {
            return;
        }

        // --- CPU CHROMA KEY LOGIC ---
        int pixelCount = m_Width * m_Height;

        // Loop through pixels
        for (int i = 0; i < pixelCount; ++i) {
            // Indices
            int rgbIndex = i * 3;
            int rgbaIndex = i * 4;

            // Read RGB
            uint8_t r = m_RawRGBBuffer[rgbIndex + 0];
            uint8_t g = m_RawRGBBuffer[rgbIndex + 1];
            uint8_t b = m_RawRGBBuffer[rgbIndex + 2];

            // Write RGB
            m_FrameBuffer[rgbaIndex + 0] = r;
            m_FrameBuffer[rgbaIndex + 1] = g;
            m_FrameBuffer[rgbaIndex + 2] = b;

            // Calculate Alpha
            if (m_RemoveBlack) {
                // Sum of channels (Max 765)
                float brightness = (float)(r + g + b);

                // Threshold 1: Absolute Black (Delete compression noise)
                // Increase to remove the "dirty pixels"
                if (brightness < 60.0f) {
                    m_FrameBuffer[rgbaIndex + 3] = 0; // Fully Transparent
                }
                // Threshold 2: Smooth Transition (Fade out edges)
                // Pixels between brightness 60 and 100 will fade from 0% to 100% alpha
                else if (brightness < 100.0f) {
                    float alpha = (brightness - 60.0f) / 40.0f;
                    m_FrameBuffer[rgbaIndex + 3] = static_cast<uint8_t>(alpha * 255.0f);
                }
                // Threshold 3: Visible Content
                else {
                    m_FrameBuffer[rgbaIndex + 3] = 255; // Opaque
                }
            }
            else {
                m_FrameBuffer[rgbaIndex + 3] = 255; // Always Opaque
            }
        }
        // ----------------------------

        glBindTexture(GL_TEXTURE_2D, m_TextureID);
        // --- UPLOAD AS RGBA ---
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, GL_RGBA, GL_UNSIGNED_BYTE, m_FrameBuffer.get());
        glBindTexture(GL_TEXTURE_2D, 0);

        m_HasNewFrame = false;
    }

} // namespace Boom

// Global FMOD system pointer - needs to be set by SoundEngine
namespace Boom {
    FMOD::System* g_FMODSystem = nullptr;
}
