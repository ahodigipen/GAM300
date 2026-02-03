#pragma once

#include "Core.h"
#include "Graphics/Shaders/Shader.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>

namespace Boom {

    class BOOM_API FontManager {
    public:
        static FontManager& GetInstance();

        FontManager();
        ~FontManager();

        bool Init();
        void LoadFont(const std::string& name, const std::string& filepath, int size);
        void RenderText(const std::string& fontName, const std::string& text, float x, float y, float scale, glm::vec3 color, float textAlpha = 1.0f);
        void Cleanup();

    private:
        struct Glyph {
            glm::vec2 offset{};
            glm::vec2 advance{};
            glm::ivec2 size{};
            glm::vec4 textureCoords{};
        };

        struct Font {
            GLuint textureID{};         // OpenGL texture ID for the font atlas
            Glyph glyphs[127];        // Array of glyphs for the font
            int fontHeight{};           // Font height (for vertical spacing)
        };

#pragma warning(push)
#pragma warning(disable: 4251)
        std::unordered_map<std::string, Font> m_Fonts;
        FT_Library m_FT{};
        FT_Face m_Face{};
        std::shared_ptr<Shader> m_TextShader;
        GLuint m_VAO{}, m_VBO{};
#pragma warning(pop)

        void SetMatrix4(GLuint programID, const std::string& uniformName, const glm::mat4& matrix);
        void SetVector3f(GLuint programID, const std::string& uniformName, const glm::vec3& vector);
    };
}
