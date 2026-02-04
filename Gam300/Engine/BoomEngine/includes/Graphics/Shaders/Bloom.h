#pragma once
#pragma once
#include "Shader.h"
#include "../Utilities/Quad.h"

namespace Boom
{
    struct BloomShader : Shader
    {
        BOOM_INLINE BloomShader(const std::string& path, int32_t width, int32_t height) :
            Shader(path), m_Width(width), m_Height(height)
        {
            u_HorizontalPass = glGetUniformLocation(shaderId, "u_horizontalPass");
            u_BrightnessMap = glGetUniformLocation(shaderId, "u_brightnessMap");
            u_FrameHeight = glGetUniformLocation(shaderId, "u_frameHeight");
            u_FrameWidth = glGetUniformLocation(shaderId, "u_frameWidth");
            m_Quad = CreateQuad2D();

            // --------------------------------------------------

            // scale down size
            m_Height = (height / m_Scale);
            m_Width = (width / m_Scale);

            // create frame buffer
            glGenFramebuffers(2, m_GausianFBO);

            // create horizontal texture
            glGenTextures(2, m_PingPongMaps);

            for (auto i = 0; i < 2; i++)
            {
                // bind target frame buffer
                glBindFramebuffer(GL_FRAMEBUFFER, m_GausianFBO[i]);

                // bind current texture 
                glBindTexture(GL_TEXTURE_2D, m_PingPongMaps[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                    m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, NULL);

                // set texture parameters
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                // attach to frame buffer
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                    GL_TEXTURE_2D, m_PingPongMaps[i], 0);

                // check frame buffer
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                {
                    BOOM_ERROR("BloomShader() Failed!");
                }
            }


            // unbind frame buffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        BOOM_INLINE void Compute(uint32_t brightnessMap, uint32_t stepCount)
        {
            // bind shader program
            glUseProgram(shaderId);

            // set brightness map
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, brightnessMap);
            glUniform1i(u_BrightnessMap, 0);

            // set frame size
            glUniform1i(u_FrameHeight, m_Height);
            glUniform1i(u_FrameWidth, m_Width);

            // set viewport a clear buffer
            glViewport(0, 0, m_Width, m_Height);
            glClear(GL_COLOR_BUFFER_BIT);

            bool horizontal = true;

            for (uint32_t i = 0u; i < stepCount; i++)
            {
                int target = horizontal ? 1 : 0;
                glBindFramebuffer(GL_FRAMEBUFFER, m_GausianFBO[horizontal]);
                glUniform1i(u_HorizontalPass, horizontal);

                if (i > 0)
                {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, m_PingPongMaps[!horizontal]);
                    glUniform1i(u_BrightnessMap, 0);
                }

                m_Quad->Draw(GL_TRIANGLE_STRIP);
                horizontal = !horizontal;
                m_LastTarget = target;
            }

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
        }

        BOOM_INLINE void Resize(int32_t width, int32_t height)
        {
            m_Height = (height / m_Scale);
            m_Width = (width / m_Scale);

            for (auto i = 0; i < 2; i++)
            {
                glBindTexture(GL_TEXTURE_2D, m_PingPongMaps[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Width,
                    m_Height, 0, GL_RGBA, GL_FLOAT, NULL);
            }

            glBindTexture(GL_TEXTURE_2D, 0);
        }

        BOOM_INLINE uint32_t GetMap()
        {
            return m_PingPongMaps[m_LastTarget];
        }

        BOOM_INLINE ~BloomShader()
        {
            glDeleteTextures(2, m_PingPongMaps);
            glDeleteFramebuffers(2, m_GausianFBO);
        }

    private:
        uint32_t u_HorizontalPass = 0u;
        uint32_t u_BrightnessMap = 0u;
        uint32_t u_FrameHeight = 0u;
        uint32_t u_FrameWidth = 0u;

        uint32_t m_PingPongMaps[2];
        uint32_t m_GausianFBO[2];

        int32_t m_Height = 0;
        int32_t m_Width = 0;
        int32_t m_Scale = 5;
        int m_LastTarget = 0;
        Quad2D m_Quad;
    };
}