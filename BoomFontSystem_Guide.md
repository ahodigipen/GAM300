# Boom Engine Font System Guide

## Overview
This document guides you through the `FontManager` system implemented for Boom Engine, adapted from the Atlantis Font System context. The system allows loading TrueType fonts (.ttf) and rendering text with custom colors, scaling, and alpha transparency using OpenGL.

## Integration Details

### Files Added
1. **Header**: `Engine/BoomEngine/includes/Graphics/Text/FontManager.h`
2. **Source**: `Engine/BoomEngine/src/Graphics/Text/FontManager.cpp`
3. **Shader**: `Editor/Resources/Shaders/font.glsl`

### Dependencies
- **FreeType**: The system relies on FreeType library. Ensure `freetype` is linked in your project configuration (`BoomEngine.vcxproj`).
- **GLM**: Used for math operations.
- **Boom::Shader**: Used for shader management.

## Usage Guide

### 1. Initialization
Initialize the `FontManager` typically during the engine's initialization phase (e.g., in `BoomEngine.cpp` or `Graphics` system init).

```cpp
#include "Graphics/Text/FontManager.h"

// In your initialization code:
if (!Boom::FontManager::GetInstance().Init()) {
    BOOM_ERROR("Failed to initialize Font Manager");
}
```

### 2. Loading Fonts
Load fonts before you intend to render them. You need to provide a name (key) and the path to the `.ttf` file.

```cpp
// Load a font named "Roboto"
Boom::FontManager::GetInstance().LoadFont("Roboto", "Resources/Fonts/Roboto-Regular.ttf", 48);
```
*Note: Ensure the font file exists at the specified path relative to the executable (e.g. `bin/Resources/Fonts/...`).*

### 3. Rendering Text
Render text inside your render loop (e.g., in `OnRender` or `Draw`).

```cpp
// Render text at position (100, 100) with scale 1.0, white color
Boom::FontManager::GetInstance().RenderText(
    "Roboto",              // Font Name
    "Hello Boom Engine!",  // Text
    100.0f, 100.0f,        // X, Y positions (Screen space, bottom-left origin)
    1.0f,                  // Scale
    glm::vec3(1.0f),       // Color (RGB)
    1.0f                   // Alpha (Opacity)
);
```

### 4. Cleanup
The `FontManager` cleans up resources in its destructor, but you can explicitly call `Cleanup()` if needed during shutdown.

```cpp
Boom::FontManager::GetInstance().Cleanup();
```

## Shader System
The system uses `font.glsl` located in `Resources/Shaders/`. This file contains both Vertex and Fragment shaders as expected by `Boom::Shader`. 

**Uniforms used:**
- `projection`: Orthographic projection matrix (automatically set based on window size).
- `text`: Sampler2D for the font atlas.
- `textColor`: RGB color of the text.
- `textAlpha`: Transparency of the text.

## Troubleshooting
- **Text not showing?** 
  - Check if `LoadFont` succeeded (check console logs).
  - Ensure coordinates are within the screen properties (0,0 is bottom-left usually).
  - Check if the correct shader `font.glsl` is in the `Resources/Shaders` folder relative to the running executable.
  - Verify `textAlpha` is 1.0.

- **Crash on Init?**
  - Ensure FreeType DLLs are in the executable directory or path.

