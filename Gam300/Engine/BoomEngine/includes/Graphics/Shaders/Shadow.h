#pragma once
#include "Shader.h"
#include "Graphics/Models/Model.h"
#include "Graphics/Utilities/Data.h"
namespace Boom {

    inline constexpr int MAX_SPOT_SHADOW_LIGHTS = 25;

    struct ShadowShader : Shader
    {
        BOOM_INLINE ShadowShader(const std::string& path) : Shader(path)
        {
            u_LightSpace = glGetUniformLocation(shaderId, "u_lightSpace");
            u_Model = glGetUniformLocation(shaderId, "u_model");
            u_Opacity = glGetUniformLocation(shaderId, "u_opacity");
            u_HasOpacityMap = glGetUniformLocation(shaderId, "u_hasOpacityMap");
            u_OpacityMap = glGetUniformLocation(shaderId, "u_opacityMap");
            jointsLoc = glGetUniformLocation(shaderId, "hasJoints");
            u_InstancingMode = glGetUniformLocation(shaderId, "u_instancingMode");
            u_BaseInstance = glGetUniformLocation(shaderId, "u_baseInstance");
            u_JointBaseInstance = glGetUniformLocation(shaderId, "u_jointBaseInstance");

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
            SetUniform(u_Opacity, 1.0f);
            SetUniform(u_HasOpacityMap, false);
            SetUniform(jointsLoc, model->HasJoint());
            SetUniform(u_Model, transform.Matrix() * model->modelTransform.Matrix());
            model->Draw();
        }

        BOOM_INLINE void Draw(Model3D& model, Transform3D& transform, const PbrMaterial& material)
        {
            SetUniform(u_Opacity, material.opacity);

            bool hasOpacityMap = material.opacityMap != nullptr;
            SetUniform(u_HasOpacityMap, hasOpacityMap);
            if (hasOpacityMap) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, *material.opacityMap);
                SetUniform(u_OpacityMap, 0);
            }

            SetUniform(jointsLoc, model->HasJoint());
            SetUniform(u_Model, transform.Matrix() * model->modelTransform.Matrix());
            model->Draw();
        }

        BOOM_INLINE void SetInstancingMode(int mode, uint32_t baseInstance = 0, uint32_t jointBaseInstance = 0)
        {
            SetUniform(u_InstancingMode, mode);
            SetUniform(u_BaseInstance, baseInstance);
            SetUniform(u_JointBaseInstance, jointBaseInstance);
        }

        BOOM_INLINE void DrawInstanced(Model3D& model, uint32_t instanceCount, uint32_t baseInstance)
        {
            SetUniform(u_Opacity, 1.0f);
            SetUniform(u_HasOpacityMap, false);
            SetUniform(jointsLoc, false);
            SetInstancingMode(1, baseInstance, 0);
            model->DrawInstanced(GL_TRIANGLES, instanceCount);
            SetInstancingMode(0, 0, 0);
        }

        BOOM_INLINE void DrawInstanced(Model3D& model, uint32_t instanceCount, uint32_t baseInstance, const PbrMaterial& material)
        {
            SetUniform(u_Opacity, material.opacity);

            bool hasOpacityMap = material.opacityMap != nullptr;
            SetUniform(u_HasOpacityMap, hasOpacityMap);
            if (hasOpacityMap) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, *material.opacityMap);
                SetUniform(u_OpacityMap, 0);
            }

            SetUniform(jointsLoc, false);
            SetInstancingMode(1, baseInstance, 0);
            model->DrawInstanced(GL_TRIANGLES, instanceCount);
            SetInstancingMode(0, 0, 0);
        }

        BOOM_INLINE void DrawAnimatedInstanced(Model3D& model, uint32_t instanceCount,
                                               uint32_t baseInstance, uint32_t jointBaseInstance)
        {
            SetUniform(u_Opacity, 1.0f);
            SetUniform(u_HasOpacityMap, false);
            SetUniform(jointsLoc, true);
            SetInstancingMode(2, baseInstance, jointBaseInstance);
            model->DrawInstanced(GL_TRIANGLES, instanceCount);
            SetInstancingMode(0, 0, 0);
        }

        BOOM_INLINE void DrawAnimatedInstanced(Model3D& model, uint32_t instanceCount,
                                               uint32_t baseInstance, uint32_t jointBaseInstance,
                                               const PbrMaterial& material)
        {
            SetUniform(u_Opacity, material.opacity);

            bool hasOpacityMap = material.opacityMap != nullptr;
            SetUniform(u_HasOpacityMap, hasOpacityMap);
            if (hasOpacityMap) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, *material.opacityMap);
                SetUniform(u_OpacityMap, 0);
            }

            SetUniform(jointsLoc, true);
            SetInstancingMode(2, baseInstance, jointBaseInstance);
            model->DrawInstanced(GL_TRIANGLES, instanceCount);
            SetInstancingMode(0, 0, 0);
        }

        BOOM_INLINE void BeginFrame(const glm::mat4& lightSpaceMtx)
        {
            Use();
            SetUniform(u_LightSpace, lightSpaceMtx);
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
            UnUse();
        }

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

        BOOM_INLINE void SetJoints(std::vector<glm::mat4>& transforms)
        {
            for (size_t i = 0; i < transforms.size() && i < 100; ++i)
            {
                std::string uniform = "jointsMat[" + std::to_string(i) + "]";
                SetUniform(GetUniformVar(uniform.c_str()), transforms[i]);
            }
        }

    private:
        uint32_t m_FrameBuffer = 0u;
        uint32_t m_DepthMap = 0u;
        int32_t MapSize = 2048;

        uint32_t m_SpotFrameBuffers[MAX_SPOT_SHADOW_LIGHTS] = {};
        uint32_t m_SpotDepthMaps[MAX_SPOT_SHADOW_LIGHTS] = {};
        glm::mat4 m_SpotLightSpaceMatrices[MAX_SPOT_SHADOW_LIGHTS] = {};
        int32_t SpotMapSize = 2048;

        int32_t jointsLoc{};
        uint32_t u_LightSpace = 0u;
        uint32_t u_Model = 0u;
        int32_t u_Opacity = 0;
        int32_t u_HasOpacityMap = 0;
        int32_t u_OpacityMap = 0;

        int32_t u_InstancingMode = 0;
        int32_t u_BaseInstance = 0;
        int32_t u_JointBaseInstance = 0;
    };
}
