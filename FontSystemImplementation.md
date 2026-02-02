# Boom Engine Font System - Implementation Documentation

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [System Design](#system-design)
3. [Component Breakdown](#component-breakdown)
4. [Initialization Flow](#initialization-flow)
5. [Rendering Pipeline](#rendering-pipeline)
6. [Font Atlas Generation](#font-atlas-generation)
7. [Memory Management](#memory-management)
8. [Integration with Engine](#integration-with-engine)
9. [Troubleshooting & Debugging](#troubleshooting--debugging)

---

## Architecture Overview

The Boom Engine Font System is a singleton-based text rendering system built on top of:
- **FreeType** - TrueType font loading and glyph rasterization
- **OpenGL** - GPU-accelerated rendering
- **Custom Font Shader** - Specialized GLSL shader for text rendering

### Key Design Decisions

1. **Singleton Pattern**: `FontManager` uses a singleton to ensure:
   - Single FreeType library instance across the engine
   - Global access for rendering text from any system
   - Centralized resource management

2. **Font Atlas Approach**: Instead of individual textures per glyph:
   - All glyphs of a font are packed into a single 512x512 texture atlas
   - Reduces texture binding overhead (one bind per font, not per character)
   - Improves cache locality and rendering performance

3. **Pre-baked Glyphs**:
   - ASCII characters 32-126 are pre-rendered at load time
   - Trade-off: Memory for performance
   - Eliminates runtime glyph generation

---

## System Design

### Class Structure

```cpp
class FontManager {
public:
    static FontManager& GetInstance();  // Singleton accessor

    bool Init();                        // Initialize FreeType & OpenGL resources
    void LoadFont(...);                 // Load and atlas-pack a font
    void RenderText(...);               // Render text to screen
    void Cleanup();                     // Release all resources

private:
    struct Glyph { ... };              // Per-glyph metrics
    struct Font { ... };                // Font atlas + glyph data

    FT_Library m_FT;                   // FreeType library instance
    FT_Face m_Face;                    // Current font face
    GLuint m_VAO, m_VBO;               // OpenGL vertex objects
    std::shared_ptr<Shader> m_TextShader;
    std::unordered_map<std::string, Font> m_Fonts;
};
```

### Data Flow

```
Font File (.ttf)
    ↓
FreeType (Load Face)
    ↓
Glyph Rasterization (per character)
    ↓
Atlas Packing (512x512 texture)
    ↓
GPU Upload (GL_R8 texture)
    ↓
Stored in m_Fonts map

Render Text
    ↓
Look up Font in map
    ↓
For each character:
    - Get glyph metrics
    - Build quad vertices
    - Upload to VBO
    - Draw with font atlas texture
```

---

## Component Breakdown

### 1. Glyph Structure

```cpp
struct Glyph {
    glm::vec2 offset;          // Bitmap offset (bearing)
    glm::vec2 advance;         // How much to advance cursor
    glm::ivec2 size;           // Glyph bitmap dimensions
    glm::vec4 textureCoords;   // UV coords in atlas (xMin, xMax, yMin, yMax)
};
```

**Purpose**: Stores per-character layout and texture information needed to position and sample the glyph during rendering.

### 2. Font Structure

```cpp
struct Font {
    GLuint textureID;          // OpenGL texture ID for the atlas
    Glyph glyphs[127];         // Glyph data for ASCII 32-126
    int fontHeight;            // Line height for vertical spacing
};
```

**Purpose**: Encapsulates all data needed to render text with a specific font at a specific size.

### 3. Shader Integration

**File**: `Resources/Shaders/font.glsl`

**Vertex Shader**:
- Takes 2D position + UV coordinates
- Applies orthographic projection (screen space)
- Passes UVs to fragment shader

**Fragment Shader**:
- Samples single-channel (RED) font atlas texture
- Applies user-specified color and alpha
- Uses the RED channel as alpha for text transparency

**Uniforms**:
```glsl
uniform mat4 projection;     // Ortho projection (window dimensions)
uniform sampler2D text;       // Font atlas texture
uniform vec3 textColor;       // RGB color
uniform float textAlpha;      // Overall transparency
```

---

## Initialization Flow

### Application Startup Sequence

```
main() or Editor::OnStart()
    ↓
Application::RunContext()  [Line 28-46]
    ↓
FontManager::GetInstance().Init()
    ↓
    1. FT_Init_FreeType()          // Initialize FreeType library
    2. Create VAO and VBO          // OpenGL vertex objects
    3. Configure vertex attributes  // Position + UV layout
    4. Load font.glsl shader       // Text rendering shader
    ↓
FontManager::LoadFont("Roboto-Regular", "Resources/Fonts/Roboto-Regular.ttf", 48)
    ↓
    [Font Atlas Generation - see next section]
```

### Why Application::RunContext()?

**Critical Insight**:
- `Application::RunContext()` is called in **both** Editor and Runtime builds
- `Editor::Init()` is **only** called in Editor builds (disabled in exports)
- Moving initialization to `RunContext()` ensures fonts work in shipped games

**Code Location**: `Engine/BoomEngine/src/Application/Application.cpp:28-46`

```cpp
std::cout << "[RunContext] Initializing Font System..." << std::endl;

if (Boom::FontManager::GetInstance().Init()) {
    Boom::FontManager::GetInstance().LoadFont("Roboto-Regular",
        "Resources/Fonts/Roboto-Regular.ttf", 48);
    std::cout << "[RunContext] Font System initialized successfully" << std::endl;
} else {
    BOOM_ERROR("Failed to initialize Font Manager");
}
```

---

## Rendering Pipeline

### RenderText() Call Flow

```cpp
FontManager::GetInstance().RenderText(
    "Roboto-Regular",              // Font name (key in m_Fonts map)
    "Hello World",                 // Text to render
    100.0f, 200.0f,               // X, Y position (screen space)
    1.0f,                          // Scale factor
    glm::vec3(1.0f, 1.0f, 1.0f),  // Color (white)
    1.0f                           // Alpha (fully opaque)
);
```

### Internal Steps (FontManager.cpp:152-233)

1. **Font Lookup**
   ```cpp
   auto it = m_Fonts.find(fontName);
   if (it == m_Fonts.end()) return;  // Font not loaded
   const Font& font = it->second;
   ```

2. **Shader Setup**
   ```cpp
   m_TextShader->Use();
   m_TextShader->SetUniform(locColor, color);
   m_TextShader->SetUniform(locAlpha, textAlpha);
   m_TextShader->SetUniform(locTex, 0);  // Texture unit 0

   // Orthographic projection (0,0 = bottom-left, WINDOW_WIDTH/HEIGHT = top-right)
   glm::mat4 projection = glm::ortho(0.0f, (float)WINDOW_WIDTH,
                                     0.0f, (float)WINDOW_HEIGHT);
   m_TextShader->SetUniform(locProj, projection);
   ```

3. **OpenGL State Configuration**
   ```cpp
   glDisable(GL_DEPTH_TEST);     // Text renders in 2D screen space
   glDisable(GL_CULL_FACE);      // Ensure quads are visible
   glEnable(GL_BLEND);           // Enable alpha blending
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glDisable(GL_SCISSOR_TEST);   // CRITICAL: ImGui leaves scissor enabled!
   ```

4. **Per-Character Rendering Loop**
   ```cpp
   for (const char& c : text) {
       // Handle newlines
       if (c == '\n') {
           x = startX;
           y -= font.fontHeight * scale;
           continue;
       }

       const Glyph& glyph = font.glyphs[c];

       // Calculate quad position
       float xpos = x + glyph.offset.x * scale;
       float ypos = y - (glyph.size.y - glyph.offset.y) * scale;
       float w = glyph.size.x * scale;
       float h = glyph.size.y * scale;

       // Build quad vertices (2 triangles = 6 vertices)
       float vertices[6][4] = {
           { xpos,     ypos + h,  glyph.textureCoords.x, glyph.textureCoords.z },
           { xpos,     ypos,      glyph.textureCoords.x, glyph.textureCoords.w },
           { xpos + w, ypos,      glyph.textureCoords.y, glyph.textureCoords.w },

           { xpos,     ypos + h,  glyph.textureCoords.x, glyph.textureCoords.z },
           { xpos + w, ypos,      glyph.textureCoords.y, glyph.textureCoords.w },
           { xpos + w, ypos + h,  glyph.textureCoords.y, glyph.textureCoords.z }
       };

       // Upload to GPU and draw
       glBindTexture(GL_TEXTURE_2D, font.textureID);
       glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
       glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
       glDrawArrays(GL_TRIANGLES, 0, 6);

       // Advance cursor
       x += glyph.advance.x * scale;
   }
   ```

### Coordinate System

- **Origin**: Bottom-left (0, 0)
- **X-Axis**: Left to right (0 → WINDOW_WIDTH)
- **Y-Axis**: Bottom to top (0 → WINDOW_HEIGHT)
- **Baseline**: Text baseline is at the Y coordinate provided
- **Newline**: Moves Y down by `fontHeight * scale`

---

## Font Atlas Generation

### Algorithm (FontManager.cpp:67-150)

The atlas packing algorithm is a **simple left-to-right, top-to-bottom row packing** approach:

```cpp
const int textureWidth = 512;
std::vector<unsigned char> textureBuffer(512 * 512, 0);  // RGBA8 would be 512*512*4

int padding = 2;        // Pixels between glyphs (prevents bleeding)
int row = 0;            // Current row Y position
int col = padding;      // Current column X position

for (FT_ULong glyphIdx = 32; glyphIdx < 127; ++glyphIdx) {  // ASCII 32-126
    // 1. Load glyph from FreeType
    FT_UInt glyphIndex = FT_Get_Char_Index(m_Face, glyphIdx);
    FT_Load_Glyph(m_Face, glyphIndex, FT_LOAD_DEFAULT);
    FT_Render_Glyph(m_Face->glyph, FT_RENDER_MODE_NORMAL);

    // 2. Check if glyph fits in current row
    if (col + m_Face->glyph->bitmap.width + padding >= textureWidth) {
        col = padding;           // Reset to start of row
        row += size;             // Move down (size = font size in pixels)
    }

    // 3. Copy glyph bitmap to atlas buffer
    for (unsigned int y = 0; y < bitmap.rows; ++y) {
        for (unsigned int x = 0; x < bitmap.width; ++x) {
            int atlasIndex = (row + y) * textureWidth + col + x;
            textureBuffer[atlasIndex] = bitmap.buffer[y * bitmap.width + x];
        }
    }

    // 4. Store glyph metrics and UV coordinates
    Glyph& glyph = font.glyphs[glyphIdx];
    glyph.size = glm::ivec2(bitmap.width, bitmap.rows);
    glyph.advance = glm::vec2(m_Face->glyph->advance.x >> 6,
                               m_Face->glyph->advance.y >> 6);
    glyph.offset = glm::vec2(m_Face->glyph->bitmap_left,
                              m_Face->glyph->bitmap_top);

    // Calculate normalized UV coordinates
    float xMin = col / (float)textureWidth;
    float xMax = (col + bitmap.width) / (float)textureWidth;
    float yMin = row / (float)textureWidth;
    float yMax = (row + bitmap.rows) / (float)textureWidth;
    glyph.textureCoords = { xMin, xMax, yMin, yMax };

    // 5. Advance column position
    col += bitmap.width + padding;
}

// 6. Upload atlas to GPU
glGenTextures(1, &font.textureID);
glBindTexture(GL_TEXTURE_2D, font.textureID);
glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 512, 512, 0,
             GL_RED, GL_UNSIGNED_BYTE, textureBuffer.data());
```

### Atlas Layout Example

```
+------------------------------------------------+
| PADDING | 'A' | 'B' | 'C' | ... | 'z' | PAD  | Row 0
+------------------------------------------------+
| PADDING | '0' | '1' | '2' | ... | '9' | PAD  | Row 1
+------------------------------------------------+
| PADDING | '!' | '@' | '#' | ... | ... | PAD  | Row 2
+------------------------------------------------+
|                 ... (unused) ...               | Rows 3+
+------------------------------------------------+
```

### Texture Format: GL_R8

- **Why single channel?** Glyphs are grayscale (antialiased edges)
- **Memory efficiency**: 1 byte per pixel vs 4 bytes for RGBA
- **Shader usage**: RED channel → Alpha channel in fragment shader

```glsl
// Fragment shader samples RED channel
float alpha = texture(text, TexCoords).r;
FragColor = vec4(textColor, 1.0) * vec4(1.0, 1.0, 1.0, alpha) * textAlpha;
```

---

## Memory Management

### Lifetime Management

1. **Singleton Initialization**
   ```cpp
   FontManager& FontManager::GetInstance() {
       static FontManager instance;  // Created on first call
       return instance;
   }
   ```
   - Instance lives for entire application lifetime
   - Automatically destroyed on program exit

2. **FreeType Resources**
   ```cpp
   ~FontManager() {
       Cleanup();  // Releases FreeType + OpenGL resources
   }

   void FontManager::Cleanup() {
       if (m_Face)  FT_Done_Face(m_Face);
       if (m_FT)    FT_Done_FreeType(m_FT);
       if (m_VAO)   glDeleteVertexArrays(1, &m_VAO);
       if (m_VBO)   glDeleteBuffers(1, &m_VBO);
       m_Fonts.clear();  // Deletes all font atlas textures
   }
   ```

3. **Font Atlas Textures**
   - Each loaded font creates **one** 512x512 GL_R8 texture (256 KB)
   - Textures stored in `std::unordered_map<std::string, Font>`
   - Cleared in `Cleanup()` or when fonts are replaced

### Memory Footprint Per Font

- **Atlas Texture**: 512 × 512 × 1 byte = **256 KB** (GPU)
- **Glyph Array**: 127 × sizeof(Glyph) ≈ 127 × 48 bytes = **6 KB** (CPU)
- **Total per font**: ~**262 KB**

**Loading 10 fonts** = ~2.6 MB (negligible for modern hardware)

---

## Integration with Engine

### 1. Build Configuration

**Include Paths** (BoomEngine.vcxproj):
```xml
<AdditionalIncludeDirectories>
    $(SolutionDir)..\freetype\include;
    ...
</AdditionalIncludeDirectories>
```

**Linker Dependencies**:
```xml
<AdditionalDependencies>
    freetype.lib;
    ...
</AdditionalDependencies>
```

**DLL Requirements** (Runtime):
- `freetype.dll` must be in the same directory as the executable
- Automatically copied during export (see Editor.cpp:701-720)

### 2. Shader Integration

**Shader Loading** (FontManager.cpp:58):
```cpp
m_TextShader = std::make_shared<Shader>("font.glsl");
```

**Boom::Shader** automatically prepends `CONSTANTS::SHADERS_LOCATION` ("Resources/Shaders/")

**Required File**: `Resources/Shaders/font.glsl`
- Must be present in **both** Editor and exported game directories
- Export process copies entire `Resources/` folder (Editor.cpp:723-730)

### 3. Application Integration Points

**Initialization** (Application.cpp:28-46):
```cpp
// Called in both Editor and Runtime
void Application::RunContext(bool showFrame) {
    // ... load scenes, assets ...

    // Initialize Font System
    if (Boom::FontManager::GetInstance().Init()) {
        Boom::FontManager::GetInstance().LoadFont("Roboto-Regular",
            "Resources/Fonts/Roboto-Regular.ttf", 48);
    }

    // ... continue initialization ...
}
```

**Rendering** (Application.cpp:938):
```cpp
// Render test text in viewport
Boom::FontManager::GetInstance().RenderText(
    "Roboto-Regular",
    "Hello Boom Engine Viewport!",
    50.0f, 50.0f, 1.0f,
    glm::vec3(1.0f, 1.0f, 0.0f)  // Yellow text
);
```

### 4. Coordinate System Alignment

**Important**: FontManager uses **orthographic projection** in screen space:

```cpp
glm::mat4 projection = glm::ortho(
    0.0f, (float)WINDOW_WIDTH,    // Left, Right
    0.0f, (float)WINDOW_HEIGHT    // Bottom, Top
);
```

- **Origin**: Bottom-left corner
- **No depth testing**: Text always renders on top (if called last)
- **No perspective**: 1 pixel = 1 screen pixel at scale 1.0

---

## Troubleshooting & Debugging

### Common Issues

#### 1. Text Not Rendering

**Symptoms**: No text appears on screen

**Possible Causes**:

- **Font not initialized**
  ```cpp
  // Check logs for:
  // "FontManager initialized." (constructor)
  // "Initializing FreeType system..." (Init())
  // "Loading Font: <name>" (LoadFont())
  ```

- **Incorrect font name**
  ```cpp
  // Font loaded as "Roboto-Regular" but called with "Roboto"
  // Names MUST match exactly
  ```

- **Coordinates off-screen**
  ```cpp
  // Y = 1000 when WINDOW_HEIGHT = 720 → text above viewport
  // X = -100 → text left of viewport
  ```

- **Alpha = 0**
  ```cpp
  RenderText(..., glm::vec3(1.0f), 0.0f);  // Fully transparent!
  ```

- **Scissor test enabled** (Fixed in code line 184)
  ```cpp
  glDisable(GL_SCISSOR_TEST);  // ImGui leaves this on!
  ```

#### 2. Crash on Init

**Symptoms**: Application crashes during FontManager::Init()

**Possible Causes**:

- **FreeType DLL missing**
  - Ensure `freetype.dll` is in the executable directory
  - Check export process copies all DLLs (Editor.cpp:701-720)

- **OpenGL context not created**
  - FontManager must initialize **after** window/OpenGL creation
  - Current placement in `RunContext()` is correct

- **Shader not found**
  ```
  ERROR: "Failed to load font shader: font.glsl"
  ```
  - Verify `Resources/Shaders/font.glsl` exists
  - Check working directory matches executable location

#### 3. Garbled/Incorrect Text

**Symptoms**: Text renders but looks wrong

**Possible Causes**:

- **Wrong texture unit**
  ```cpp
  // Shader expects unit 0, but texture bound to unit 1
  glActiveTexture(GL_TEXTURE0);  // Must match shader uniform
  ```

- **Atlas overflow** (very rare with 512x512 + 95 glyphs)
  - Check if font size > 100px (large glyphs may not fit)

- **Incorrect UV coordinates**
  - Bug in atlas packing algorithm
  - Check `glyph.textureCoords` values (should be 0.0-1.0)

### Debugging Tools

#### Visual Font Atlas Dump

Add this code after atlas generation to save atlas as image:

```cpp
// After glTexImage2D in LoadFont()
#ifdef DEBUG_FONT_ATLAS
    std::ofstream file("font_atlas_debug.raw", std::ios::binary);
    file.write((char*)textureBuffer.data(), 512 * 512);
    file.close();
    // Convert to PNG: ffmpeg -f rawvideo -pixel_format gray -s 512x512 -i font_atlas_debug.raw atlas.png
#endif
```

#### Glyph Metrics Logging

```cpp
BOOM_INFO("Glyph '{}': size=({},{}), advance=({},{}), offset=({},{}), UV=({},{},{},{})",
          (char)glyphIdx,
          glyph.size.x, glyph.size.y,
          glyph.advance.x, glyph.advance.y,
          glyph.offset.x, glyph.offset.y,
          glyph.textureCoords.x, glyph.textureCoords.y,
          glyph.textureCoords.z, glyph.textureCoords.w);
```

#### OpenGL State Verification

```cpp
GLboolean depthTest, blend, scissor;
glGetBooleanv(GL_DEPTH_TEST, &depthTest);
glGetBooleanv(GL_BLEND, &blend);
glGetBooleanv(GL_SCISSOR_TEST, &scissor);

BOOM_INFO("GL State: DepthTest={}, Blend={}, Scissor={}",
          depthTest, blend, scissor);
```

### Performance Monitoring

**Render Call Count**:
- Text "Hello" = **5 draw calls** (1 per character)
- Batching not currently implemented (could be future optimization)

**GPU Memory Usage**:
- Check with tools like RenderDoc, Nsight Graphics
- Each font = 1 texture (256 KB)

**CPU Overhead**:
- Per-character vertex buffer update (`glBufferSubData`)
- Could optimize with instance rendering for many characters

---

## Future Enhancements

### Potential Optimizations

1. **Batched Rendering**
   - Build vertex buffer for entire string
   - Single draw call instead of per-character

2. **Dynamic Atlas Expansion**
   - Support fonts > 512x512
   - On-demand glyph loading (not pre-baking all 95 characters)

3. **Unicode Support**
   - Extend beyond ASCII 32-126
   - Use hash map instead of fixed array for glyphs

4. **SDF (Signed Distance Field) Fonts**
   - Scale fonts smoothly without re-loading
   - Better visual quality at large sizes

5. **Text Layout Engine**
   - Word wrapping
   - Text alignment (left, center, right)
   - Rich text (color/style tags)

### Known Limitations

- **ASCII Only**: Characters outside 32-126 not supported
- **Fixed Size**: Each font loaded at one size (e.g., 48px)
  - To get different sizes, load the font twice with different size parameters
- **No Kerning**: Character spacing doesn't account for glyph pairs (e.g., "AV")
- **No Text Layout**: Client code must handle line breaks, alignment, etc.

---

## See Also

- **BoomFontSystem_Guide.md** - User-facing usage guide
- **TextComponentSystem_CreationGuide.md** - Complete guide to building Unity-like Text Component system
- **Boom::Shader** - Shader system documentation
- **FreeType Documentation** - https://freetype.org/freetype2/docs/
- **Learn OpenGL Text Rendering** - https://learnopengl.com/In-Practice/Text-Rendering

---

## Change Log

### 2026-02-01 - Initialization Fix
- **Issue**: Text not rendering in exported builds (without ImGui)
- **Root Cause**: FontManager initialized in `Editor::Init()` which doesn't run in Runtime
- **Fix**: Moved initialization to `Application::RunContext()` (line 28-46)
- **Files Changed**:
  - `Engine/BoomEngine/src/Application/Application.cpp` (added init code)
  - `Editor/src/Editor.cpp` (removed init code, added comment)
- **Result**: Fonts now work in both Editor and exported game builds

---

**Document Version**: 1.0
**Last Updated**: 2026-02-01
**Author**: Boom Engine Team
