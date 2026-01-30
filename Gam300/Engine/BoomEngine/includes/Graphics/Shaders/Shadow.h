#pragma once
#include "Shader.h"
#include "Graphics/Models/Model.h"
#include "Graphics/Utilities/Data.h"
namespace Boom {

    // Maximum number of shadow-casting spot lights
    inline constexpr int MAX_SPOT_SHADOW_LIGHTS = 4;

    struct ShadowShader : Shader
    {
        BOOM_INLINE ShadowShader(const std::string& path) : Shader(path)
        {
            u_LightSpace = glGetUniformLocation(shaderId, "u_lightSpace");
            u_Model = glGetUniformLocation(shaderId, "u_model");

            // === Directional Light Shadow Map ===
            glGenFramebuffers(1, &m_FrameBuffer);
            glGenTextures(1, &m_DepthMap);
            glBindTexture(GL_TEXTURE_2D, m_DepthMap);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, MapSize, MapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

            glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthMap, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);

            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                BOOM_ERROR("CreateDepthBuffer() for directional light Failed!");
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // === Spot Light Shadow Maps ===
            glGenFramebuffers(MAX_SPOT_SHADOW_LIGHTS, m_SpotFrameBuffers);
            glGenTextures(MAX_SPOT_SHADOW_LIGHTS, m_SpotDepthMaps);

            for (int i = 0; i < MAX_SPOT_SHADOW_LIGHTS; ++i)
            {
                glBindTexture(GL_TEXTURE_2D, m_SpotDepthMaps[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SpotMapSize, SpotMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

                glBindFramebuffer(GL_FRAMEBUFFER, m_SpotFrameBuffers[i]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_SpotDepthMaps[i], 0);
                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);

                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                {
                    BOOM_ERROR("CreateDepthBuffer() for spot light {} Failed!", i);
                }
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        BOOM_INLINE void Draw(Model3D& model, Transform3D& transform)
        {
            SetUniform(jointsLoc, model->HasJoint());
            SetUniform(u_Model, transform.Matrix() * model->modelTransform.Matrix());
            model->Draw();
        }

        BOOM_INLINE void BeginFrame(const glm::mat4& lightSpaceMtx)
        {
            // bind shadow shader
            Use();

            // set view projection matrix
            SetUniform(u_LightSpace, lightSpaceMtx);

            // set viewport a clear buffer
            glEnable(GL_DEPTH_TEST);
            glViewport(0, 0, MapSize, MapSize);
            glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);
            glClear(GL_DEPTH_BUFFER_BIT);
        }

        BOOM_INLINE uint32_t GetDepthMap()
        {
            return m_DepthMap;
        }

        BOOM_INLINE void EndFrame()
        {
            // Just unbind the shadow shader - let the renderer restore the FBO
            UnUse();
        }

        // === Spot Light Shadow Functions ===
        BOOM_INLINE void BeginSpotLightFrame(int index, const glm::mat4& lightSpaceMtx)
        {
            if (index < 0 || index >= MAX_SPOT_SHADOW_LIGHTS) return;

            Use();
            SetUniform(u_LightSpace, lightSpaceMtx);
            m_SpotLightSpaceMatrices[index] = lightSpaceMtx;

            glEnable(GL_DEPTH_TEST);
            glViewport(0, 0, SpotMapSize, SpotMapSize);
            glBindFramebuffer(GL_FRAMEBUFFER, m_SpotFrameBuffers[index]);
            glClear(GL_DEPTH_BUFFER_BIT);
        }

        BOOM_INLINE void EndSpotLightFrame()
        {
            UnUse();
        }

        BOOM_INLINE uint32_t GetSpotDepthMap(int index) const
        {
            if (index < 0 || index >= MAX_SPOT_SHADOW_LIGHTS) return 0;
            return m_SpotDepthMaps[index];
        }

        BOOM_INLINE const glm::mat4& GetSpotLightSpaceMatrix(int index) const
        {
            static glm::mat4 identity(1.0f);
            if (index < 0 || index >= MAX_SPOT_SHADOW_LIGHTS) return identity;
            return m_SpotLightSpaceMatrices[index];
        }

        BOOM_INLINE ~ShadowShader()
        {
            glDeleteFramebuffers(1, &m_FrameBuffer);
            glDeleteTextures(1, &m_DepthMap);
            glDeleteFramebuffers(MAX_SPOT_SHADOW_LIGHTS, m_SpotFrameBuffers);
            glDeleteTextures(MAX_SPOT_SHADOW_LIGHTS, m_SpotDepthMaps);
        }

        //Animation 
        BOOM_INLINE void SetJoints(std::vector<glm::mat4>& transforms)
        {
            for (size_t i = 0; i < transforms.size() && i < 100; ++i)
            {
                std::string uniform = "jointsMat[" + std::to_string(i) + "]";
                SetUniform(GetUniformVar(uniform.c_str()), transforms[i]);
            }
        }

    private:
        // Directional light shadow map
        uint32_t m_FrameBuffer = 0u;
        uint32_t m_DepthMap = 0u;
        int32_t MapSize = 1024;

        // Spot light shadow maps
        uint32_t m_SpotFrameBuffers[MAX_SPOT_SHADOW_LIGHTS] = {};
        uint32_t m_SpotDepthMaps[MAX_SPOT_SHADOW_LIGHTS] = {};
        glm::mat4 m_SpotLightSpaceMatrices[MAX_SPOT_SHADOW_LIGHTS] = {};
        int32_t SpotMapSize = 1024;

        int32_t jointsLoc{};
        uint32_t u_LightSpace = 0u;
        uint32_t u_Model = 0u;
    };
}