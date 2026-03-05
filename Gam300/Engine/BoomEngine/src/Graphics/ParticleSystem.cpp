#include "Graphics/ParticleSystem.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace Boom {

    // ─── Quad vertices (position + UV) for a unit billboard ──────────
    static const float s_QuadVertices[] = {
        // x,    y,    z,    u,    v
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
    };

    ParticleSystem::ParticleSystem()
        : m_Rng(std::random_device{}())
    {
    }

    ParticleSystem::~ParticleSystem()
    {
        if (m_VAO)  glDeleteVertexArrays(1, &m_VAO);
        if (m_VBO)  glDeleteBuffers(1, &m_VBO);
        if (m_SSBO) glDeleteBuffers(1, &m_SSBO);
    }

    void ParticleSystem::Init()
    {
        // Create shader
        m_Shader = std::make_unique<Shader>("particle.glsl");

        // Create quad VAO
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(s_QuadVertices), s_QuadVertices, GL_STATIC_DRAW);

        // position (location 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

        // uv (location 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindVertexArray(0);

        // Create SSBO for instance data
        glGenBuffers(1, &m_SSBO);
    }

    void ParticleSystem::Update(float dt, EntityRegistry& registry, const glm::vec3& cameraPos)
    {
        auto view = registry.view<ParticleEmitterComponent, TransformComponent>();
        for (auto entity : view) {
            auto& emitter = view.get<ParticleEmitterComponent>(entity);
            auto& tc      = view.get<TransformComponent>(entity);
            uint32_t key  = static_cast<uint32_t>(entity);

            // Auto-start
            if (emitter.playOnStart && !emitter.isPlaying) {
                emitter.isPlaying = true;
                emitter.emitterTimer = 0.0f;
                emitter.spawnAccum = 0.0f;
            }

            if (!emitter.isPlaying) continue;

            // Get or create emitter data
            auto& data = m_Emitters[key];
            if (static_cast<int>(data.pool.size()) != emitter.maxParticles) {
                data.pool.resize(emitter.maxParticles);
                for (auto& p : data.pool) p.alive = false;
                data.aliveCount = 0;
            }

            // Emitter world position
            glm::vec3 worldPos = tc.transform.translate;

            // Update emitter timer
            emitter.emitterTimer += dt;
            if (!emitter.looping && emitter.emitterTimer >= emitter.duration) {
                // Stop spawning but let existing particles die
            } else {
                // Spawn particles
                emitter.spawnAccum += emitter.emissionRate * dt;
                int toSpawn = static_cast<int>(emitter.spawnAccum);
                emitter.spawnAccum -= static_cast<float>(toSpawn);

                for (int s = 0; s < toSpawn; ++s) {
                    // Find dead particle
                    for (auto& p : data.pool) {
                        if (!p.alive) {
                            SpawnParticle(p, emitter, worldPos);
                            data.aliveCount++;
                            break;
                        }
                    }
                }
            }

            // Simulate alive particles
            data.aliveCount = 0;
            for (auto& p : data.pool) {
                if (!p.alive) continue;

                p.life -= dt;
                if (p.life <= 0.0f) {
                    p.alive = false;
                    continue;
                }

                // Physics
                p.velocity.y += emitter.gravity * dt;
                p.position += p.velocity * dt;

                // Interpolation factor (0 = just born, 1 = about to die)
                float t = 1.0f - (p.life / p.maxLife);

                // Size over lifetime
                float startSize = (emitter.startSizeMin + emitter.startSizeMax) * 0.5f;
                p.size = startSize + (emitter.endSize - startSize) * t;

                // Color over lifetime
                p.color = emitter.startColor * (1.0f - t) + emitter.endColor * t;

                data.aliveCount++;
            }
        }

        // Clean up emitters for destroyed entities
        for (auto it = m_Emitters.begin(); it != m_Emitters.end(); ) {
            entt::entity e = static_cast<entt::entity>(it->first);
            if (!registry.valid(e) || !registry.all_of<ParticleEmitterComponent>(e)) {
                it = m_Emitters.erase(it);
            } else {
                ++it;
            }
        }
    }

    void ParticleSystem::SpawnParticle(Particle& p, const ParticleEmitterComponent& emitter,
                                        const glm::vec3& emitterWorldPos)
    {
        p.alive = true;
        p.maxLife = RandFloat(emitter.lifetimeMin, emitter.lifetimeMax);
        p.life = p.maxLife;
        p.size = RandFloat(emitter.startSizeMin, emitter.startSizeMax);
        p.color = emitter.startColor;

        // Spawn position based on shape
        switch (emitter.shapeType) {
        case 1: // sphere
            p.position = emitterWorldPos + RandInSphere(emitter.shapeRadius);
            break;
        case 2: // cone
            p.position = emitterWorldPos;
            break;
        case 3: // box
            p.position = emitterWorldPos + RandInBox(emitter.shapeSize);
            break;
        default: // point
            p.position = emitterWorldPos;
            break;
        }

        // Velocity direction based on shape
        glm::vec3 dir;
        switch (emitter.shapeType) {
        case 1: { // sphere — outward from center
            glm::vec3 offset = p.position - emitterWorldPos;
            float len = glm::length(offset);
            dir = (len > 0.001f) ? offset / len : RandDirection();
            break;
        }
        case 2: // cone
            dir = RandInCone(emitter.direction, emitter.shapeAngle);
            break;
        default:
            dir = RandInCone(emitter.direction, 15.0f); // slight spread for point
            break;
        }

        float speed = RandFloat(emitter.speedMin, emitter.speedMax);
        p.velocity = dir * speed;
    }

    // ─── RNG helpers ─────────────────────────────────────────────────

    float ParticleSystem::RandFloat(float lo, float hi)
    {
        std::uniform_real_distribution<float> dist(lo, hi);
        return dist(m_Rng);
    }

    glm::vec3 ParticleSystem::RandDirection()
    {
        float theta = RandFloat(0.0f, glm::two_pi<float>());
        float phi   = std::acos(RandFloat(-1.0f, 1.0f));
        return {
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        };
    }

    glm::vec3 ParticleSystem::RandInSphere(float radius)
    {
        float r = radius * std::cbrt(RandFloat(0.0f, 1.0f));
        return RandDirection() * r;
    }

    glm::vec3 ParticleSystem::RandInCone(const glm::vec3& dir, float halfAngleDeg)
    {
        float halfAngleRad = glm::radians(halfAngleDeg);
        float cosAngle = std::cos(halfAngleRad);
        float z = RandFloat(cosAngle, 1.0f);
        float phi = RandFloat(0.0f, glm::two_pi<float>());
        float sinTheta = std::sqrt(1.0f - z * z);

        glm::vec3 localDir(sinTheta * std::cos(phi), sinTheta * std::sin(phi), z);

        // Build rotation from (0,0,1) to dir
        glm::vec3 d = glm::normalize(dir);
        glm::vec3 up(0, 0, 1);
        if (std::abs(glm::dot(d, up)) > 0.999f) {
            up = glm::vec3(1, 0, 0);
        }
        glm::vec3 right = glm::normalize(glm::cross(d, up));
        glm::vec3 newUp = glm::cross(right, d);

        return right * localDir.x + newUp * localDir.y + d * localDir.z;
    }

    glm::vec3 ParticleSystem::RandInBox(const glm::vec3& halfExtents)
    {
        return {
            RandFloat(-halfExtents.x, halfExtents.x),
            RandFloat(-halfExtents.y, halfExtents.y),
            RandFloat(-halfExtents.z, halfExtents.z)
        };
    }

} // namespace Boom
