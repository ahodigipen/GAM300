#pragma once
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt/entt.hpp>
#include "ECS/ECS.hpp"
#include "Graphics/Shaders/Shader.h"
#include "Auxiliaries/Assets.h"
#include <random>
#include <GL/glew.h>

namespace Boom {

    // Per-emitter GPU-side data
    struct EmitterGPU {
        GLuint particleSSBO  = 0;   // particle state buffer (position, velocity, life, etc.)
        GLuint renderSSBO    = 0;   // compacted render output (pos+size+color for alive only)
        GLuint counterBuffer = 0;   // atomic counter: alive count
        GLuint indirectBuffer = 0;  // DrawArraysIndirectCommand
        int    maxParticles  = 0;
        int    spawnCarry    = 0;   // fractional spawn accumulator (fixed-point)
        float  spawnAccum    = 0.0f;
    };

    class ParticleSystem {
    public:
        ParticleSystem();
        ~ParticleSystem();

        void Init();

        // Destroy all live GPU particle buffers — call this when leaving play mode so
        // particles don't persist into the editor (they will be recreated on next Play)
        void Reset()
        {
            for (auto& [key, gpu] : m_Emitters)
                DestroyEmitterGPU(gpu);
            m_Emitters.clear();
            m_FrameSeed = 0;
        }

        // CPU-side: orchestrate spawning counts, dispatch compute, clean up destroyed emitters
        void Update(float dt, EntityRegistry& registry, const glm::vec3& cameraPos);

        // GPU-side: bind render SSBOs, draw instanced billboards
        template<typename AssetRegistryT>
        void Render(EntityRegistry& registry, AssetRegistryT& /*assets*/,
                    const glm::mat4& viewMatrix, const glm::mat4& projMatrix);

    private:
        void EnsureEmitterGPU(uint32_t key, int maxParticles);
        void DestroyEmitterGPU(EmitterGPU& e)
        {
            if (e.particleSSBO)   glDeleteBuffers(1, &e.particleSSBO);
            if (e.renderSSBO)     glDeleteBuffers(1, &e.renderSSBO);
            if (e.counterBuffer)  glDeleteBuffers(1, &e.counterBuffer);
            if (e.indirectBuffer) glDeleteBuffers(1, &e.indirectBuffer);
            e = {};
        }

        // Compile a single-stage compute program from file
        GLuint LoadComputeShader(const std::string& filename);

        // GPU programs
        GLuint m_SimulateProgram = 0;  // compute: simulate + spawn particles
        GLuint m_RenderProgram   = 0;  // vert+frag: draw billboards

        // Quad VAO for instanced billboard rendering
        GLuint m_VAO = 0;
        GLuint m_VBO = 0;

        // Per-emitter GPU data keyed by entt::entity uint32
        std::unordered_map<uint32_t, EmitterGPU> m_Emitters;

        bool m_Initialized = false;

        // RNG seed counter (each emitter gets unique seed per frame)
        uint32_t m_FrameSeed = 0;

        // Cached uniform locations (avoid glGetUniformLocation per-frame)
        struct ComputeUniforms {
            GLint uDt = -1, uMaxParticles = -1, uSpawnCount = -1;
            GLint uEmitterPos = -1, uGravity = -1;
            GLint uLifetimeMin = -1, uLifetimeMax = -1;
            GLint uSpeedMin = -1, uSpeedMax = -1;
            GLint uShapeType = -1, uShapeRadius = -1, uShapeAngle = -1;
            GLint uShapeSize = -1, uDirection = -1;
            GLint uStartSizeMin = -1, uStartSizeMax = -1, uEndSize = -1;
            GLint uStartColor = -1, uEndColor = -1, uFrameSeed = -1;
        } m_ComputeLocs;

        struct RenderUniforms {
            GLint uViewProj = -1, uCamRight = -1, uCamUp = -1, uBillboard = -1;
        } m_RenderLocs;

        void CacheUniformLocations();
    };

    // ──── Render template implementation ────────────────────────────────

    template<typename AssetRegistryT>
    void ParticleSystem::Render(EntityRegistry& registry, AssetRegistryT& /*assets*/,
                                const glm::mat4& viewMatrix, const glm::mat4& projMatrix)
    {
        if (!m_Initialized) Init();
        if (!m_RenderProgram || m_Emitters.empty()) return;

        // Extract camera right/up from view matrix for billboarding
        glm::vec3 camRight = glm::vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
        glm::vec3 camUp    = glm::vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);
        glm::mat4 viewProj = projMatrix * viewMatrix;

        glUseProgram(m_RenderProgram);

        // Set shared uniforms (using cached locations)
        glUniformMatrix4fv(m_RenderLocs.uViewProj, 1, GL_FALSE, &viewProj[0][0]);
        glUniform3fv(m_RenderLocs.uCamRight, 1, &camRight[0]);
        glUniform3fv(m_RenderLocs.uCamUp, 1, &camUp[0]);

        // Save blend state so we don't leak into fog/lighting passes that follow
        GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

        // GL state for particles
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        glBindVertexArray(m_VAO);

        auto view = registry.view<ParticleEmitterComponent>();
        for (auto entity : view) {
            auto& emitter = view.get<ParticleEmitterComponent>(entity);
            if (!emitter.isPlaying) continue;

            uint32_t key = static_cast<uint32_t>(entity);
            auto it = m_Emitters.find(key);
            if (it == m_Emitters.end()) continue;

            auto& gpu = it->second;

            // Set blend mode per-emitter
            if (emitter.additiveBlend)
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            else
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Set billboard flag
            glUniform1i(m_RenderLocs.uBillboard, emitter.billboard ? 1 : 0);

            // Bind this emitter's render SSBO to binding 0
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gpu.renderSSBO);

            // Draw with indirect command (alive count written by compute shader)
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gpu.indirectBuffer);
            glDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr);
        }

        glBindVertexArray(0);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        // Restore all GL state so lights, fog, and GUI passes are unaffected
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (!blendWasEnabled) glDisable(GL_BLEND);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);

        glUseProgram(0);
    }

} // namespace Boom
