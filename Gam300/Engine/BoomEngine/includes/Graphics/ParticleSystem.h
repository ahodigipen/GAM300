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

    // Single particle data (CPU side)
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec4 color;
        float     size;
        float     life;       // remaining life (seconds)
        float     maxLife;    // initial life (for interpolation)
        bool      alive = false;
    };

    // Per-emitter runtime data (not stored in ECS — managed by ParticleSystem)
    struct EmitterData {
        std::vector<Particle> pool;
        int aliveCount = 0;
    };

    // GPU instance data (packed for SSBO upload)
    struct ParticleGPU {
        glm::mat4 modelMatrix;
        glm::vec4 color;
    };

    class ParticleSystem {
    public:
        ParticleSystem();
        ~ParticleSystem();

        // Call once after OpenGL context is ready
        void Init();

        // Call every frame: spawns, simulates, kills particles for all emitters
        void Update(float dt, EntityRegistry& registry, const glm::vec3& cameraPos);

        // Call during render: uploads instance data and draws all particles
        // viewMatrix needed for billboarding
        template<typename AssetRegistryT>
        void Render(EntityRegistry& registry, AssetRegistryT& assets,
                    const glm::mat4& viewMatrix, const glm::mat4& projMatrix);

    private:
        void SpawnParticle(Particle& p, const ParticleEmitterComponent& emitter,
                           const glm::vec3& emitterWorldPos);

        // GPU resources
        GLuint m_VAO = 0;
        GLuint m_VBO = 0;       // quad vertices
        GLuint m_SSBO = 0;      // instance data buffer
        std::unique_ptr<Shader> m_Shader;

        // Per-emitter pools keyed by entt::entity
        std::unordered_map<uint32_t, EmitterData> m_Emitters;

        // Staging buffer for GPU upload
        std::vector<ParticleGPU> m_GPUBuffer;

        // RNG
        std::mt19937 m_Rng;

        float RandFloat(float lo, float hi);
        glm::vec3 RandDirection();
        glm::vec3 RandInSphere(float radius);
        glm::vec3 RandInCone(const glm::vec3& dir, float halfAngleDeg);
        glm::vec3 RandInBox(const glm::vec3& halfExtents);
    };

    // ──── Template implementation (must be in header) ────────────────────

    template<typename AssetRegistryT>
    void ParticleSystem::Render(EntityRegistry& registry, AssetRegistryT& assets,
                                const glm::mat4& viewMatrix, const glm::mat4& projMatrix)
    {
        // Collect all alive particles from all emitters into GPU buffer
        m_GPUBuffer.clear();

        // Extract camera right/up from view matrix for billboarding
        glm::vec3 camRight = glm::vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
        glm::vec3 camUp    = glm::vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]);

        auto view = registry.view<ParticleEmitterComponent>();
        for (auto entity : view) {
            auto& emitter = view.get<ParticleEmitterComponent>(entity);
            uint32_t key = static_cast<uint32_t>(entity);
            auto it = m_Emitters.find(key);
            if (it == m_Emitters.end()) continue;

            auto& data = it->second;
            bool additive = emitter.additiveBlend;

            for (auto& p : data.pool) {
                if (!p.alive) continue;

                // Build billboard model matrix
                float s = p.size;
                glm::mat4 model(1.0f);

                if (emitter.billboard) {
                    model[0] = glm::vec4(camRight * s, 0.0f);
                    model[1] = glm::vec4(camUp * s, 0.0f);
                    model[2] = glm::vec4(glm::cross(camRight, camUp) * s, 0.0f);
                    model[3] = glm::vec4(p.position, 1.0f);
                } else {
                    model = glm::translate(glm::mat4(1.0f), p.position);
                    model = glm::scale(model, glm::vec3(s));
                }

                m_GPUBuffer.push_back({ model, p.color });
            }
        }

        if (m_GPUBuffer.empty()) return;

        // Upload to SSBO
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SSBO);
        size_t bufSize = m_GPUBuffer.size() * sizeof(ParticleGPU);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufSize, m_GPUBuffer.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_SSBO); // binding point 5 to avoid clash

        // Bind shader
        m_Shader->Use();
        m_Shader->SetUniform(m_Shader->GetUniformVar("uViewProj"), projMatrix * viewMatrix);

        // Bind default white texture (or particle texture — simplified for now)
        // TODO: per-emitter texture batching
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0); // 0 = use shader default

        m_Shader->SetUniform(m_Shader->GetUniformVar("uTexture"), 0);

        // GL state for particles
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);  // don't write depth for translucent particles
        glDisable(GL_CULL_FACE);

        // Draw all particles as instanced quads
        // We handle additive vs alpha blend per-batch later; for now use alpha
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindVertexArray(m_VAO);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(m_GPUBuffer.size()));
        glBindVertexArray(0);

        // Restore state
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);

        m_Shader->UnUse();
    }

} // namespace Boom
