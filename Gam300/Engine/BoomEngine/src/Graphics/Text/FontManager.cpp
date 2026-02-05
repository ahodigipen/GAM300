#include "Core.h"
#include "Graphics/Text/FontManager.h"
#include "GlobalConstants.h"
#include <algorithm>

namespace Boom {

    FontManager& FontManager::GetInstance() {
        static FontManager instance;
        return instance;
    }

    FontManager::FontManager() {
        BOOM_INFO("FontManager initialized.");
    }

    FontManager::~FontManager() {
        Cleanup();
        BOOM_INFO("FontManager destroyed.");
    }

    bool FontManager::Init() {
        BOOM_INFO("Initializing FreeType system...");

        if (FT_Init_FreeType(&m_FT)) {
            BOOM_ERROR("ERROR: Could not initialize FreeType Library");
            return false;
        }

        // Set up VAO and VBO for text rendering
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

        // Dynamic buffer for text quads (6 vertices of 4 float params each)
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        // Texture coord attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // Ensure unpack alignment is 1 for font textures/glyphs
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  
        
        // Initialize Shader
        try {
            // CONSTANTS::SHADERS_LOCATION is "Resources/Shaders/" and Boom::Shader prepends it.
            // So we pass "font.glsl"
            m_TextShader = std::make_shared<Shader>("font.glsl");
        } catch (const std::exception& e) {
            BOOM_ERROR("Failed to load font shader: {}", e.what());
            return false;
        }

        return true;
    }

    void FontManager::LoadFont(const std::string& name, const std::string& filepath, int size) {
        BOOM_INFO("Loading Font: {}", name);

        if (FT_New_Face(m_FT, filepath.c_str(), 0, &m_Face)) {
            BOOM_ERROR("ERROR: Failed to load font face: {}", filepath);
            return;
        }

        FT_Set_Pixel_Sizes(m_Face, 0, size);

        int padding = 2;
        int row = 0;
        int col = padding;
        const int textureWidth = 512;
        // Using vector for buffer to avoid stack overflow with large arrays if needed, though 512*512 is 262KB which is fine
        std::vector<unsigned char> textureBuffer(textureWidth * textureWidth, 0);

        Font font;
        font.fontHeight = 0;

        // ASCII 32 to 126
        for (FT_ULong glyphIdx = 32; glyphIdx < 127; ++glyphIdx) {
            FT_UInt glyphIndex = FT_Get_Char_Index(m_Face, glyphIdx);
            if (FT_Load_Glyph(m_Face, glyphIndex, FT_LOAD_DEFAULT)) continue;
            if (FT_Render_Glyph(m_Face->glyph, FT_RENDER_MODE_NORMAL)) continue;

            if (col + m_Face->glyph->bitmap.width + padding >= textureWidth) {
                col = padding;
                row += size; // Rough approximation of row height
            }
            
            // Calculate max font height dynamically
            int currentHeight = (int)((m_Face->size->metrics.ascender - m_Face->size->metrics.descender) >> 6);
            font.fontHeight = std::max(currentHeight, font.fontHeight);

            // Copy bitmap to texture buffer
            for (unsigned int y = 0; y < m_Face->glyph->bitmap.rows; ++y) {
                for (unsigned int x = 0; x < m_Face->glyph->bitmap.width; ++x) {
                    if ((row + y) * textureWidth + col + x < textureBuffer.size()) {
                         textureBuffer[(row + y) * textureWidth + col + x] =
                            m_Face->glyph->bitmap.buffer[y * m_Face->glyph->bitmap.width + x];
                    }
                }
            }

            Glyph& glyph = font.glyphs[glyphIdx];
            glyph.size = glm::ivec2(m_Face->glyph->bitmap.width, m_Face->glyph->bitmap.rows);
            // shift 6 because advances are in 1/64 pixels
            glyph.advance = {
                (float)(m_Face->glyph->advance.x >> 6),
                (float)(m_Face->glyph->advance.y >> 6)
            };
            glyph.offset = {
                (float)m_Face->glyph->bitmap_left,
                (float)m_Face->glyph->bitmap_top
            };
            
            float xTexbot = (col) / static_cast<float>(textureWidth);
            float xTextop = (col + static_cast<float>(m_Face->glyph->bitmap.width)) / static_cast<float>(textureWidth);
            float yTexbot = (row) / static_cast<float>(textureWidth);
            float yTextop = (row + static_cast<float>(m_Face->glyph->bitmap.rows)) / static_cast<float>(textureWidth);
           
            glyph.textureCoords = { xTexbot, xTextop, yTexbot, yTextop };
            
            col += m_Face->glyph->bitmap.width + padding;
        }

        glGenTextures(1, &font.textureID);
        glActiveTexture(GL_TEXTURE1); // Use unit 1 for upload creates no conflict
        glBindTexture(GL_TEXTURE_2D, font.textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, textureWidth, textureWidth, 0, GL_RED, GL_UNSIGNED_BYTE, textureBuffer.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Restore alignment to default
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        m_Fonts[name] = font;
    }

    void FontManager::RenderText(const std::string& fontName, const std::string& text, float x, float y, float scale, glm::vec3 color, float textAlpha) {
        auto it = m_Fonts.find(fontName);
        if (it == m_Fonts.end()) {
            // BOOM_ERROR("Font '{}' not loaded.", fontName); 
            // Commented out to avoid spamming if font is missing
            return;
        }
        const Font& font = it->second;

        if(!m_TextShader) return;
        m_TextShader->Use();

        // set uniforms
        // Note: Boom::Shader wrappers are convenient
        int32_t locColor = m_TextShader->GetUniformVar("textColor");
        m_TextShader->SetUniform(locColor, color);
        
        int32_t locAlpha = m_TextShader->GetUniformVar("textAlpha");
        m_TextShader->SetUniform(locAlpha, textAlpha);
        
        int32_t locTex = m_TextShader->GetUniformVar("text");
        m_TextShader->SetUniform(locTex, 0); // Unit 0

        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(CONSTANTS::WINDOW_WIDTH), 0.0f, static_cast<float>(CONSTANTS::WINDOW_HEIGHT));
        int32_t locProj = m_TextShader->GetUniformVar("projection");
        m_TextShader->SetUniform(locProj, projection);

        // --- RENDER STATE SETUP ---
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_SCISSOR_TEST); // Vital: ImGui leaves scissor enabled, which clips custom rendering!

        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(m_VAO);

        float startX = x;

        for (const char& c : text) {
            if (c == '\n') {
                x = startX;
                y -= font.fontHeight * scale;
                continue;
            }
            
            // Check bound
            if (c < 0 || c >= 127) continue;

            const Glyph& glyph = font.glyphs[c];

            float xpos = x + glyph.offset.x * scale;
            float ypos = y - (glyph.size.y - glyph.offset.y) * scale;

            float w = glyph.size.x * scale;
            float h = glyph.size.y * scale;

            // Updated VBO for each character
            float vertices[6][4] = {
                 { xpos,     ypos + h,   glyph.textureCoords.x,  glyph.textureCoords.z},
                 { xpos,     ypos,       glyph.textureCoords.x,  glyph.textureCoords.w },
                 { xpos + w, ypos,       glyph.textureCoords.y,  glyph.textureCoords.w },

                 { xpos,     ypos + h,   glyph.textureCoords.x,  glyph.textureCoords.z },
                 { xpos + w, ypos,       glyph.textureCoords.y,  glyph.textureCoords.w },
                 { xpos + w, ypos + h,   glyph.textureCoords.y,  glyph.textureCoords.z }
            };

            glBindTexture(GL_TEXTURE_2D, font.textureID);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);   
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            x += (glyph.advance.x) * scale;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_TextShader->UnUse();
    }

    void FontManager::Cleanup() {
        if (m_Face) {
             FT_Done_Face(m_Face);
             m_Face = nullptr;
        }
        if (m_FT) {
            FT_Done_FreeType(m_FT);
            m_FT = nullptr;
        }
        
        if (m_VAO) {
            glDeleteVertexArrays(1, &m_VAO);
            m_VAO = 0;
        }
        if (m_VBO) {
            glDeleteBuffers(1, &m_VBO);
            m_VBO = 0;
        }
        
        m_Fonts.clear();
        m_TextShader.reset();
    }

    void FontManager::SetMatrix4(GLuint programID, const std::string& uniformName, const glm::mat4& matrix) {
        GLuint location = glGetUniformLocation(programID, uniformName.c_str());
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
    }
    
    void FontManager::SetVector3f(GLuint programID, const std::string& uniformName, const glm::vec3& vector) {
        GLuint location = glGetUniformLocation(programID, uniformName.c_str());
        glUniform3f(location, vector.x, vector.y, vector.z);
    }
}
