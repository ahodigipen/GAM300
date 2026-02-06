#pragma once
#include "Shader.h"
#include "../Utilities/Quad.h"
#include "../Utilities/Data.h"
#include "GlobalConstants.h"

namespace Boom {
    /**
     * LoadingVideoShader - Renders a video texture as a fullscreen quad
     * Used during loading screens to display video content
     */
    struct LoadingVideoShader : Shader {
        BOOM_INLINE LoadingVideoShader(std::string const& filename)
            : Shader{ filename }
            , tintColor{ 1.f }
            , brightnessVal{ 1.f }
            , tintLoc{ GetUniformVar("tintColor") }
            , brightnessLoc{ GetUniformVar("brightness") }
            , projLoc{ GetUniformVar("uProj") }
            , texLoc{ GetUniformVar("videoTex") }
            , quad{ CreateQuad2D() }
        {
        }

        /**
         * Render a video frame texture to the screen
         * @param proj Projection matrix
         * @param textureId OpenGL texture ID from VideoPlayer
         */
        BOOM_INLINE void Show(glm::mat4 const& proj, uint32_t textureId) {
            Use();
            SetUniform(projLoc, proj * quadTransform.Matrix());
            SetUniform(tintLoc, tintColor);
            SetUniform(brightnessLoc, brightnessVal);

            // Bind video texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textureId);
            SetUniform(texLoc, 0);

            quad->Draw(GL_TRIANGLE_STRIP);

            glBindTexture(GL_TEXTURE_2D, 0);
            UnUse();
        }

        // Center of quad is pivot
        BOOM_INLINE void SetTransform(glm::vec2 const& pos, glm::vec2 const& scale, float rot) {
            quadTransform.translate = { pos.x, pos.y, 0.f };
            quadTransform.scale = { scale.x, scale.y, 1.f };
            quadTransform.rotate = { 0.f, 0.f, rot };
        }

        BOOM_INLINE void SetTintColor(glm::vec4 const& col) { tintColor = col; }
        BOOM_INLINE void SetBrightness(float val) { brightnessVal = val; }

    private:
        glm::vec4 tintColor;
        float brightnessVal;
        Transform3D quadTransform;
        int32_t projLoc;
        int32_t tintLoc;
        int32_t brightnessLoc;
        int32_t texLoc;
        Quad2D quad;
    };
}
