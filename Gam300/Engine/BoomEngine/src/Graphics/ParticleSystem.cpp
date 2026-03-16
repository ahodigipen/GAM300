#include "Core.h"
#include "Graphics/ParticleSystem.h"
#include <glm/gtc/constants.hpp>
#include <fstream>
#include <sstream>

namespace Boom {

    // ─── Quad vertices for billboard (position + UV) ─────────────────
    static const float s_QuadVerts[] = {
        // x,    y,    z,    u,    v
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
         0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
    };

    // DrawArraysIndirectCommand struct matching GL spec
    struct DrawArraysIndirectCommand {
        GLuint count;         // vertices per instance (4 for quad)
        GLuint instanceCount; // number of instances (alive particles)
        GLuint first;         // first vertex (0)
        GLuint baseInstance;  // base instance (0)
    };

    ParticleSystem::ParticleSystem()
        : m_FrameSeed(0)
    {
    }

    ParticleSystem::~ParticleSystem()
    {
        for (auto& [key, gpu] : m_Emitters)
            DestroyEmitterGPU(gpu);

        if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
        if (m_VBO) glDeleteBuffers(1, &m_VBO);
        if (m_SimulateProgram) glDeleteProgram(m_SimulateProgram);
        if (m_RenderProgram)   glDeleteProgram(m_RenderProgram);
    }

    GLuint ParticleSystem::LoadComputeShader(const std::string& filename)
    {
        std::string path = std::string(CONSTANTS::SHADERS_LOCATION) + filename;
        std::ifstream file(path);
        if (!file.is_open()) {
            BOOM_ERROR("[ParticleSystem] Failed to open compute shader: {}", path);
            return 0;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        std::string src = ss.str();
        const char* srcPtr = src.c_str();

        GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shader, 1, &srcPtr, nullptr);
        glCompileShader(shader);

        GLint status = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status) {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            BOOM_ERROR("[ParticleSystem] Compute shader compile error ({}): {}", filename, log);
            glDeleteShader(shader);
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, shader);
        glLinkProgram(program);

        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (!status) {
            char log[1024];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            BOOM_ERROR("[ParticleSystem] Compute shader link error ({}): {}", filename, log);
            glDeleteProgram(program);
            glDeleteShader(shader);
            return 0;
        }

        glDeleteShader(shader);
        return program;
    }

    void ParticleSystem::Init()
    {
        if (m_Initialized) return;
        m_Initialized = true;

        // Load compute shader
        m_SimulateProgram = LoadComputeShader("particle_compute.glsl");

        // Load render shader (vert+frag) using the engine's Shader class pattern
        {
            std::string path = std::string(CONSTANTS::SHADERS_LOCATION) + "particle_render.glsl";
            std::ifstream fs(path);
            if (!fs.is_open()) {
                BOOM_ERROR("[ParticleSystem] Failed to open render shader: {}", path);
                return;
            }

            std::string line, vtxStr, fragStr;
            bool isVtx = true;
            while (std::getline(fs, line)) {
                if (isVtx) {
                    if (line.compare("==VERTEX==")) {
                        vtxStr += line + '\n';
                    } else {
                        isVtx = false;
                    }
                } else {
                    if (!line.compare("==FRAGMENT==")) break;
                    fragStr += line + '\n';
                }
            }

            auto compileStage = [](const char* src, GLenum type) -> GLuint {
                GLuint id = glCreateShader(type);
                glShaderSource(id, 1, &src, nullptr);
                glCompileShader(id);
                GLint ok = 0;
                glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
                if (!ok) {
                    char log[1024];
                    glGetShaderInfoLog(id, sizeof(log), nullptr, log);
                    BOOM_ERROR("[ParticleSystem] Render shader compile: {}", log);
                    glDeleteShader(id);
                    return 0;
                }
                return id;
            };

            GLuint vs = compileStage(vtxStr.c_str(), GL_VERTEX_SHADER);
            GLuint fs2 = compileStage(fragStr.c_str(), GL_FRAGMENT_SHADER);

            if (vs && fs2) {
                m_RenderProgram = glCreateProgram();
                glAttachShader(m_RenderProgram, vs);
                glAttachShader(m_RenderProgram, fs2);
                glLinkProgram(m_RenderProgram);

                GLint ok = 0;
                glGetProgramiv(m_RenderProgram, GL_LINK_STATUS, &ok);
                if (!ok) {
                    char log[1024];
                    glGetProgramInfoLog(m_RenderProgram, sizeof(log), nullptr, log);
                    BOOM_ERROR("[ParticleSystem] Render shader link: {}", log);
                    glDeleteProgram(m_RenderProgram);
                    m_RenderProgram = 0;
                }
            } else {
                BOOM_ERROR("[ParticleSystem] Render shader compilation failed — vs:{} fs:{}", vs, fs2);
                m_RenderProgram = 0;
            }

            if (vs)  glDeleteShader(vs);
            if (fs2) glDeleteShader(fs2);
        }

        // Create quad VAO
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(s_QuadVerts), s_QuadVerts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Cache uniform locations once after linking
        CacheUniformLocations();

        // Check for any GL errors accumulated during init
        GLenum initErr = glGetError();

        BOOM_INFO("[ParticleSystem] GPU compute particle system initialized");
        BOOM_INFO("[ParticleSystem] Compute program: {}, Render program: {}", m_SimulateProgram, m_RenderProgram);
        BOOM_INFO("[ParticleSystem] VAO: {}, VBO: {}", m_VAO, m_VBO);
        BOOM_INFO("[ParticleSystem] Compute uniform uDt: {}, uEmitterPos: {}, uStartColor: {}",
                  m_ComputeLocs.uDt, m_ComputeLocs.uEmitterPos, m_ComputeLocs.uStartColor);
        BOOM_INFO("[ParticleSystem] Render uniform uViewProj: {}, uCamRight: {}, uBillboard: {}",
                  m_RenderLocs.uViewProj, m_RenderLocs.uCamRight, m_RenderLocs.uBillboard);
        if (initErr != GL_NO_ERROR)
            BOOM_ERROR("[ParticleSystem] GL error during init: 0x{:X}", initErr);
    }

    void ParticleSystem::CacheUniformLocations()
    {
        if (m_SimulateProgram) {
            m_ComputeLocs.uDt           = glGetUniformLocation(m_SimulateProgram, "uDt");
            m_ComputeLocs.uMaxParticles = glGetUniformLocation(m_SimulateProgram, "uMaxParticles");
            m_ComputeLocs.uSpawnCount   = glGetUniformLocation(m_SimulateProgram, "uSpawnCount");
            m_ComputeLocs.uEmitterPos   = glGetUniformLocation(m_SimulateProgram, "uEmitterPos");
            m_ComputeLocs.uGravity      = glGetUniformLocation(m_SimulateProgram, "uGravity");
            m_ComputeLocs.uLifetimeMin  = glGetUniformLocation(m_SimulateProgram, "uLifetimeMin");
            m_ComputeLocs.uLifetimeMax  = glGetUniformLocation(m_SimulateProgram, "uLifetimeMax");
            m_ComputeLocs.uSpeedMin     = glGetUniformLocation(m_SimulateProgram, "uSpeedMin");
            m_ComputeLocs.uSpeedMax     = glGetUniformLocation(m_SimulateProgram, "uSpeedMax");
            m_ComputeLocs.uShapeType    = glGetUniformLocation(m_SimulateProgram, "uShapeType");
            m_ComputeLocs.uShapeRadius  = glGetUniformLocation(m_SimulateProgram, "uShapeRadius");
            m_ComputeLocs.uShapeAngle   = glGetUniformLocation(m_SimulateProgram, "uShapeAngle");
            m_ComputeLocs.uShapeSize    = glGetUniformLocation(m_SimulateProgram, "uShapeSize");
            m_ComputeLocs.uDirection    = glGetUniformLocation(m_SimulateProgram, "uDirection");
            m_ComputeLocs.uStartSizeMin = glGetUniformLocation(m_SimulateProgram, "uStartSizeMin");
            m_ComputeLocs.uStartSizeMax = glGetUniformLocation(m_SimulateProgram, "uStartSizeMax");
            m_ComputeLocs.uEndSize      = glGetUniformLocation(m_SimulateProgram, "uEndSize");
            m_ComputeLocs.uStartColor   = glGetUniformLocation(m_SimulateProgram, "uStartColor");
            m_ComputeLocs.uEndColor     = glGetUniformLocation(m_SimulateProgram, "uEndColor");
            m_ComputeLocs.uFrameSeed    = glGetUniformLocation(m_SimulateProgram, "uFrameSeed");
        }

        if (m_RenderProgram) {
            m_RenderLocs.uViewProj  = glGetUniformLocation(m_RenderProgram, "uViewProj");
            m_RenderLocs.uCamRight  = glGetUniformLocation(m_RenderProgram, "uCamRight");
            m_RenderLocs.uCamUp     = glGetUniformLocation(m_RenderProgram, "uCamUp");
            m_RenderLocs.uBillboard = glGetUniformLocation(m_RenderProgram, "uBillboard");
        }
    }

    void ParticleSystem::EnsureEmitterGPU(uint32_t key, int maxParticles)
    {
        auto it = m_Emitters.find(key);
        if (it != m_Emitters.end() && it->second.maxParticles == maxParticles) return;

        // Destroy old if size changed
        if (it != m_Emitters.end()) {
            DestroyEmitterGPU(it->second);
            m_Emitters.erase(it);
        }

        EmitterGPU gpu{};
        gpu.maxParticles = maxParticles;

        // Particle state SSBO: each particle = 12 floats
        // [posX, posY, posZ, life, velX, velY, velZ, maxLife, size, alive, seedX, seedY]
        size_t particleSize = 12 * sizeof(float);
        size_t bufSize = particleSize * maxParticles;

        glGenBuffers(1, &gpu.particleSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu.particleSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bufSize, nullptr, GL_DYNAMIC_COPY);
        // Zero-initialize (all particles dead — alive=0)
        std::vector<float> zeros(12 * maxParticles, 0.0f);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bufSize, zeros.data());

        // Render output SSBO: each alive particle = 8 floats [posX, posY, posZ, size, r, g, b, a]
        size_t renderSize = 8 * sizeof(float) * maxParticles;
        glGenBuffers(1, &gpu.renderSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu.renderSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, renderSize, nullptr, GL_DYNAMIC_COPY);

        // Counter SSBO: [aliveCount, spawnCount] — 2 uints
        glGenBuffers(1, &gpu.counterBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu.counterBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        // Indirect draw buffer
        DrawArraysIndirectCommand cmd{};
        cmd.count = 4;          // quad vertices
        cmd.instanceCount = 0;  // filled by compute
        cmd.first = 0;
        cmd.baseInstance = 0;

        glGenBuffers(1, &gpu.indirectBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gpu.indirectBuffer);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), &cmd, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        m_Emitters[key] = gpu;
    }

    void ParticleSystem::Update(float dt, EntityRegistry& registry, const glm::vec3& /*cameraPos*/)
    {
        if (!m_Initialized) Init();
        if (!m_SimulateProgram) return;

        m_FrameSeed++;

        auto view = registry.view<ParticleEmitterComponent, TransformComponent>();

        // Early-out: no emitters with transforms — skip all GL calls
        if (view.size_hint() == 0) return;

        glUseProgram(m_SimulateProgram);
        for (auto entity : view) {
            auto& emitter = view.get<ParticleEmitterComponent>(entity);
            uint32_t key  = static_cast<uint32_t>(entity);

            // Auto-start
            if (emitter.playOnStart && !emitter.isPlaying) {
                emitter.isPlaying = true;
                emitter.emitterTimer = 0.0f;
                emitter.spawnAccum = 0.0f;
            }

            if (!emitter.isPlaying) continue;

            // Ensure GPU buffers exist
            EnsureEmitterGPU(key, emitter.maxParticles);
            auto& gpu = m_Emitters[key];

            // Full world transform — walks the parent chain so particles follow
            // whichever entity the emitter is attached to or parented under
            glm::mat4 worldMat = GetWorldMatrix(registry, entity);
            glm::vec3 worldPos = glm::vec3(worldMat[3]);

            // Calculate spawn count on CPU (cheap)
            int spawnCount = 0;
            emitter.emitterTimer += dt;
            if (emitter.looping || emitter.emitterTimer < emitter.duration) {
                gpu.spawnAccum += emitter.emissionRate * dt;
                spawnCount = static_cast<int>(gpu.spawnAccum);
                gpu.spawnAccum -= static_cast<float>(spawnCount);
            }

            // Reset counters: [aliveCount=0, spawnCount=N]
            GLuint counters[2] = { 0u, static_cast<GLuint>(spawnCount) };
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, gpu.counterBuffer);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 2 * sizeof(GLuint), counters);

            // Reset indirect draw command (count=4 vertices, instanceCount=0)
            DrawArraysIndirectCommand cmd{};
            cmd.count = 4;
            cmd.instanceCount = 0;
            cmd.first = 0;
            cmd.baseInstance = 0;
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gpu.indirectBuffer);
            glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(cmd), &cmd);

            // Bind all SSBOs (matching shader bindings 0, 1, 2)
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gpu.particleSSBO);  // particle state
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gpu.renderSSBO);    // render output
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpu.counterBuffer); // counters [alive, spawn]

            // Set uniforms (using cached locations — no string lookups per frame)
            glUniform1f(m_ComputeLocs.uDt, dt);
            glUniform1i(m_ComputeLocs.uMaxParticles, emitter.maxParticles);
            glUniform1i(m_ComputeLocs.uSpawnCount, spawnCount);
            glUniform3fv(m_ComputeLocs.uEmitterPos, 1, &worldPos[0]);
            glUniform1f(m_ComputeLocs.uGravity, emitter.gravity);
            glUniform1f(m_ComputeLocs.uLifetimeMin, emitter.lifetimeMin);
            glUniform1f(m_ComputeLocs.uLifetimeMax, emitter.lifetimeMax);
            glUniform1f(m_ComputeLocs.uSpeedMin, emitter.speedMin);
            glUniform1f(m_ComputeLocs.uSpeedMax, emitter.speedMax);
            glUniform1i(m_ComputeLocs.uShapeType, emitter.shapeType);
            glUniform1f(m_ComputeLocs.uShapeRadius, emitter.shapeRadius);
            glUniform1f(m_ComputeLocs.uShapeAngle, emitter.shapeAngle);
            glUniform3fv(m_ComputeLocs.uShapeSize, 1, &emitter.shapeSize[0]);
            // Extract pure rotation from world matrix (remove scale from columns)
            // so local-space direction is correctly oriented in world space, including parent rotation
            glm::vec3 worldDir = glm::normalize(glm::mat3(
                glm::normalize(glm::vec3(worldMat[0])),
                glm::normalize(glm::vec3(worldMat[1])),
                glm::normalize(glm::vec3(worldMat[2]))
            ) * emitter.direction);
            glUniform3fv(m_ComputeLocs.uDirection, 1, &worldDir[0]);
            glUniform1f(m_ComputeLocs.uStartSizeMin, emitter.startSizeMin);
            glUniform1f(m_ComputeLocs.uStartSizeMax, emitter.startSizeMax);
            glUniform1f(m_ComputeLocs.uEndSize, emitter.endSize);
            glUniform4fv(m_ComputeLocs.uStartColor, 1, &emitter.startColor[0]);
            glUniform4fv(m_ComputeLocs.uEndColor, 1, &emitter.endColor[0]);
            glUniform1ui(m_ComputeLocs.uFrameSeed, m_FrameSeed * 1000 + key);

            // Dispatch: one thread per particle slot
            GLuint groups = (emitter.maxParticles + 255) / 256;
            glDispatchCompute(groups, 1, 1);

            // Memory barrier: compute writes must be visible to rendering + buffer copies
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT
                          | GL_BUFFER_UPDATE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

            // Copy alive count into indirect draw command's instanceCount field
            glBindBuffer(GL_COPY_READ_BUFFER, gpu.counterBuffer);
            glBindBuffer(GL_COPY_WRITE_BUFFER, gpu.indirectBuffer);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                                0, sizeof(GLuint), sizeof(GLuint));
            // writeOffset = sizeof(GLuint) = instanceCount field in DrawArraysIndirectCommand
        }

        glUseProgram(0);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
        glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

        // Unbind SSBO slots 0-2 so no other render pass accidentally reads particle buffers
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);

        // Clean up emitters for destroyed entities
        for (auto it = m_Emitters.begin(); it != m_Emitters.end(); ) {
            entt::entity e = static_cast<entt::entity>(it->first);
            if (!registry.valid(e) || !registry.all_of<ParticleEmitterComponent>(e)) {
                DestroyEmitterGPU(it->second);
                it = m_Emitters.erase(it);
            } else {
                ++it;
            }
        }
    }

} // namespace Boom
