#pragma once
#include "Shader.h"
#include "../Utilities/Quad.h"
#include "GlobalConstants.h"
#include "../Textures/Texture.h"

namespace Boom {
    struct ColorShader : Shader {
        BOOM_INLINE ColorShader(std::string const& filename, glm::vec4 col)
            : Shader{ filename }
            , color{ col }
            , colLoc{ GetUniformVar("color") }
            , texLoc{ GetUniformVar("texMap") }
            , matLoc{ GetUniformVar("mat") }
            , toneMapLoc{ GetUniformVar("u_applyToneMap") }
            , gammaLoc{ GetUniformVar("u_gamma") }
            , quad{ CreateQuad2D() }
        {
        }
        BOOM_INLINE void ChangeColor(glm::vec4 const& col) {
            color = col;
        }
        BOOM_INLINE void SetToneMap(bool enable) {
            applyToneMap = enable;
        }
        BOOM_INLINE void SetGamma(float g) {
            gamma = g;
        }
        BOOM_INLINE void Show(uint32_t texid, Transform2D const& t) {
            Use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texid);
            SetUniform(texLoc, 0);
            SetUniform(colLoc, color);
            SetUniform(matLoc, t.Matrix());
            SetUniform(toneMapLoc, applyToneMap);
            SetUniform(gammaLoc, gamma);
            quad->Draw(GL_TRIANGLE_STRIP);
            UnUse();
        }

    private:
        glm::vec4 color;
        int32_t colLoc;
        int32_t texLoc;
        int32_t matLoc;
        int32_t toneMapLoc;
        int32_t gammaLoc;
        bool applyToneMap{ false };
        float gamma{ 2.2f };
        Quad2D quad;
    };

    struct Color3DShader : Shader {
        BOOM_INLINE Color3DShader(std::string const& filename, glm::vec4 col)
            : Shader{ filename }
            , color{ col }
            , texLoc{ GetUniformVar("texMap") }
            , colLoc{ GetUniformVar("color") }
            , matLoc{ GetUniformVar("mat") }
            , projLoc{ GetUniformVar("proj") }
            , quad{ CreateQuad3D() }
        {
        }
        BOOM_INLINE void SetCamera(Camera3D const& cam, Transform3D const& transform, float ratio) {
            Use();
            SetUniform(projLoc, cam.Frustum(transform, ratio));
        }
        BOOM_INLINE void ChangeColor(glm::vec4 const& col) {
            color = col;
        }
        BOOM_INLINE void Show(uint32_t texid, Transform3D const& t) {
            Use();
            glActiveTexture(GL_TEXTURE15);
            glBindTexture(GL_TEXTURE_2D, texid);
            SetUniform(texLoc, 15);
            SetUniform(colLoc, color);
            SetUniform(matLoc, t.Matrix());
            quad->Draw(GL_TRIANGLES);
            UnUse();
        }

    private:
        glm::vec4 color;
        int32_t texLoc;
        int32_t colLoc;
        int32_t matLoc;
        int32_t projLoc;
        Quad3D quad;
    };
}