# Creating a Unity-Like Text Component System for Boom Engine

## Table of Contents
1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Prerequisites](#prerequisites)
4. [Step-by-Step Implementation](#step-by-step-implementation)
   - [Step 1: Define the TextComponent](#step-1-define-the-textcomponent)
   - [Step 2: Add to Component Registry](#step-2-add-to-component-registry)
   - [Step 3: Create Inspector UI](#step-3-create-inspector-ui)
   - [Step 4: Expose to C# Scripts](#step-4-expose-to-c-scripts)
   - [Step 5: Implement Rendering System](#step-5-implement-rendering-system)
   - [Step 6: Testing & Validation](#step-6-testing--validation)
5. [Advanced Features](#advanced-features)
6. [Complete Code Reference](#complete-code-reference)
7. [Troubleshooting](#troubleshooting)

---

## Overview

This guide documents the **complete process** of creating a Unity-like Text Component system for Boom Engine. By the end, you'll have:

✅ A **TextComponent** that can be added to any entity
✅ **Inspector editing** (change text, font, color, size, etc.)
✅ **C# script access** (modify text at runtime via code)
✅ **Automatic rendering** using the FontManager system
✅ **Serialization support** (save/load scenes with text)

### What You'll Build

A text system similar to Unity's TextMeshPro or UI Text component:
- Text content (string)
- Font selection (from loaded fonts)
- Color (RGB + Alpha)
- Scale/Size
- Position (2D screen space or 3D world space)
- Alignment options (left, center, right)

---

## System Architecture

### Component Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      TextComponent                          │
│  (Defined in ECS.hpp, attached to entities)                 │
└──────────────┬──────────────────────────────────────────────┘
               │
       ┌───────┴───────┐
       │               │
       ▼               ▼
┌─────────────┐   ┌──────────────┐
│  Inspector  │   │  C# Scripts  │
│  (ImGui)    │   │  (Mono API)  │
└──────┬──────┘   └──────┬───────┘
       │                 │
       └────────┬────────┘
                ▼
       ┌─────────────────┐
       │ Rendering System│
       │ (Application.cpp│
       │  or TextSystem) │
       └────────┬────────┘
                ▼
       ┌─────────────────┐
       │  FontManager    │
       │ (Draws glyphs)  │
       └─────────────────┘
```

### Integration Points

| Layer | File(s) | Purpose |
|-------|---------|---------|
| **Component Definition** | `ECS.hpp` | Define struct, properties, serialization |
| **Component Registry** | `ECS.hpp` | Add to enum, component list |
| **Inspector UI** | `InspectorPanel.cpp` | Add component selector entry |
| **Script Bindings** | `ScriptBinding.cpp` | C++ → C# bridge functions |
| **C# API** | `API.cs` | Public C# interface |
| **Rendering** | `Application.cpp` or custom system | Iterate entities, call FontManager |

---

## Prerequisites

Before starting, ensure you have:

1. ✅ **FontManager initialized** (see FontSystemImplementation.md)
2. ✅ **At least one font loaded** (e.g., "Roboto-Regular")
3. ✅ **Basic understanding of**:
   - EnTT entity-component system
   - XPROPERTY serialization macros
   - ImGui for UI
   - Mono C# interop

---

## Step-by-Step Implementation

### Step 1: Define the TextComponent

**Location**: `Engine/BoomEngine/includes/ECS/ECS.hpp`

Add this struct definition after the existing components (around line 760+):

```cpp
// Text Component - Unity-like text rendering
struct TextComponent {
    std::string text = "New Text";               // The actual text to display
    std::string fontName = "Roboto-Regular";     // Font to use (must be loaded)
    glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };  // RGBA color
    float scale = 1.0f;                          // Size multiplier
    glm::vec2 screenPosition{ 100.0f, 100.0f };  // Screen space position (pixels)
    bool renderAs3D = false;                     // false = 2D overlay, true = 3D world space

    // Text alignment
    enum class Alignment {
        Left = 0,
        Center = 1,
        Right = 2
    };
    Alignment alignment = Alignment::Left;

    // Serialization support using XPROPERTY system
    XPROPERTY_DEF(
        "TextComponent", TextComponent,
        obj_member<"text", &TextComponent::text>,
        obj_member<"fontName", &TextComponent::fontName>,
        obj_member<"color", &TextComponent::color>,
        obj_member<"scale", &TextComponent::scale>,
        obj_member<"screenPosition", &TextComponent::screenPosition>,
        obj_member<"renderAs3D", &TextComponent::renderAs3D>
        // Note: Alignment enum would need custom serialization (skip for now)
    )
};
```

**Key Design Decisions**:

- **`text`**: The string content (default: "New Text" so it's visible when added)
- **`fontName`**: String key matching loaded fonts (e.g., "Roboto-Regular")
- **`color`**: RGBA (glm::vec4) for full color control including transparency
- **`scale`**: Multiplier applied to font rendering (1.0 = normal size)
- **`screenPosition`**: 2D pixel coordinates (bottom-left origin like FontManager)
- **`renderAs3D`**: Future-proofing for world-space text (attach to 3D objects)
- **`alignment`**: Enum for text justification (implementation in rendering step)

**XPROPERTY_DEF Macro**:
- Enables automatic serialization to YAML
- Allows inspector property editing via reflection
- Must match struct member names exactly (with quotes)

---

### Step 2: Add to Component Registry

Still in `ECS.hpp`, update the component enumeration and name list.

#### 2.1: Update ComponentID Enum

**Location**: `ECS.hpp` around line 18-30

```cpp
enum class ComponentID : size_t {
    INFO, TRANSFORM, CAMERA, RIGIDBODY, COLLIDER,
    MODEL, ANIMATOR, DIRECT_LIGHT, POINT_LIGHT, SPOT_LIGHT,
    SOUND, SCRIPT,
    THIRD_PERSON_CAMERA,
    NAV_AGENT_COMPONENT,
    AI_COMPONENT,
    SPRITE,
    TEXT,              // ← ADD THIS LINE
    MENU_COMPONENT,
    DEACTIVATED_TAG,
    VIDEO,
    CHARACTER_CONTROLLER,
    COUNT
};
```

#### 2.2: Update Component Names Array

**Location**: `ECS.hpp` around line 32-55

```cpp
constexpr std::string_view COMPONENT_NAMES[]{
    "Info",                 //0
    "Transform",            //1
    "Camera",               //2
    "Rigidbody",            //3
    "Collider",             //4
    "Model",                //5
    "Animator",             //6
    "Direct Light",         //7
    "Point Light",          //8
    "Spot Light",           //9
    "Sound",                //10
    "Script",               //11
    "Third Person Camera",  //12
    "Nav Agent Component",  //13
    "AI Component",         //14
    "Sprite",               //15
    "Text",                 //16  ← ADD THIS LINE
    "Menu Component",       //17
    "Deactivated Tag",      //18
    "Video",                //19
    "Character Controller", //20
    "Count"
};
```

**CRITICAL**: The enum value and array index **must match exactly**. If TEXT is at position 16 in the enum, "Text" must be at index 16 in the array.

---

### Step 3: Create Inspector UI

**Location**: `Editor/src/Panels/Inspector/InspectorPanel.cpp`

#### 3.1: Add to Component Selector

Find the `ComponentSelector` function (around line 3544) and add this line:

```cpp
void InspectorPanel::ComponentSelector(Boom::Entity& selected) {
    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::SetNextWindowSizeConstraints(ImVec2(300, 200), ImVec2(500, 600));

        ImGui::Text("Select component to add:");
        ImGui::Separator();
        if (ImGui::BeginChild("ComponentScrollArea", ImVec2(0, 250), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            if (ImGui::BeginTable("Component Table", 1, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg)) {
                // ... existing component entries ...
                UpdateComponent<Boom::SpriteComponent>(Boom::ComponentID::SPRITE, selected);
                UpdateComponent<Boom::TextComponent>(Boom::ComponentID::TEXT, selected);  // ← ADD THIS
                UpdateComponent<Boom::MenuComponent>(Boom::ComponentID::MENU_COMPONENT, selected);
                // ... rest of components ...
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
}
```

**What This Does**:
- Adds "Text" to the "Add Component" dropdown in the Inspector
- When clicked, attaches a TextComponent to the selected entity
- Uses the templated `UpdateComponent<>` function for type-safe component management

#### 3.2: Automatic Property Display

**No additional code needed!**

The `DrawPropertiesUI` system (in `PropertiesImgui.cpp`) automatically generates UI for all fields in the `XPROPERTY_DEF` macro:

- `std::string` → Input text box
- `glm::vec4` → RGBA color picker
- `float` → Drag slider
- `glm::vec2` → 2D vector input
- `bool` → Checkbox

**Result**: When you select an entity with TextComponent, you'll see:

```
┌─ Text Component ─────────────────┐
│ text:            [New Text____]  │
│ fontName:        [Roboto-Regular]│
│ color:           [🎨 RGBA picker]│
│ scale:           [1.00]          │
│ screenPosition:  [100, 100]      │
│ renderAs3D:      [ ] (checkbox)  │
└──────────────────────────────────┘
```

#### 3.3: (Optional) Custom Inspector Panel

For advanced features like font dropdown or alignment buttons, create a custom section:

**Location**: `InspectorPanel.cpp`, inside the `Render()` function where other components are drawn

```cpp
// Custom Text Component UI (enhanced version)
if (auto* textComp = selected.TryGet<Boom::TextComponent>()) {
    DrawComponentSection("Text Component", textComp,
        [&]() { return &textComp->GetProps(); },
        true, // removable
        [&]() {
            selected.Remove<Boom::TextComponent>();
            BOOM_INFO("Removed TextComponent from entity");
        }
    );

    if (ImGui::TreeNode("Advanced Text Settings")) {
        // Font selector dropdown
        static const char* fonts[] = { "Roboto-Regular", "Arial", "Courier" };
        static int currentFont = 0;
        if (ImGui::Combo("Font", &currentFont, fonts, IM_ARRAYSIZE(fonts))) {
            textComp->fontName = fonts[currentFont];
        }

        // Alignment buttons
        ImGui::Text("Alignment:");
        if (ImGui::RadioButton("Left", textComp->alignment == Boom::TextComponent::Alignment::Left)) {
            textComp->alignment = Boom::TextComponent::Alignment::Left;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Center", textComp->alignment == Boom::TextComponent::Alignment::Center)) {
            textComp->alignment = Boom::TextComponent::Alignment::Center;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Right", textComp->alignment == Boom::TextComponent::Alignment::Right)) {
            textComp->alignment = Boom::TextComponent::Alignment::Right;
        }

        ImGui::TreePop();
    }
}
```

---

### Step 4: Expose to C# Scripts

Enable C# scripts to read/modify TextComponent at runtime.

#### 4.1: C++ Internal Call Functions

**Location**: `Engine/BoomEngine/src/Scripting/ScriptBinding.cpp`

Add these functions near the other component functions (around line 940+):

```cpp
// ========== TEXT COMPONENT INTERNAL CALLS ==========

static bool ICALL_API_HasText(uint64_t entityHandle) {
    entt::entity e = static_cast<entt::entity>(entityHandle);
    return (e != entt::null && s_Ctx->scene.valid(e) && s_Ctx->scene.any_of<TextComponent>(e));
}

static void ICALL_API_GetText(uint64_t entityHandle, MonoString** outText) {
    entt::entity e = static_cast<entt::entity>(entityHandle);
    if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
        *outText = mono_string_new(mono_domain_get(), "");
        return;
    }

    const std::string& text = s_Ctx->scene.get<TextComponent>(e).text;
    *outText = mono_string_new(mono_domain_get(), text.c_str());
}

static void ICALL_API_SetText(uint64_t entityHandle, MonoString* newText) {
    entt::entity e = static_cast<entt::entity>(entityHandle);
    if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
        BOOM_WARN("[ScriptBinding] SetText: Entity doesn't have TextComponent");
        return;
    }

    char* cStr = mono_string_to_utf8(newText);
    s_Ctx->scene.get<TextComponent>(e).text = std::string(cStr);
    mono_free(cStr);
}

static void ICALL_API_GetTextColor(uint64_t entityHandle, glm::vec4* outColor) {
    entt::entity e = static_cast<entt::entity>(entityHandle);
    if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
        *outColor = glm::vec4(1.0f);
        return;
    }
    *outColor = s_Ctx->scene.get<TextComponent>(e).color;
}

static void ICALL_API_SetTextColor(uint64_t entityHandle, glm::vec4* color) {
    entt::entity e = static_cast<entt::entity>(entityHandle);
    if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
        BOOM_WARN("[ScriptBinding] SetTextColor: Entity doesn't have TextComponent");
        return;
    }
    s_Ctx->scene.get<TextComponent>(e).color = *color;
}

static float ICALL_API_GetTextScale(uint64_t entityHandle) {
    entt::entity e = static_cast<entt::entity>(entityHandle);
    if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
        return 1.0f;
    }
    return s_Ctx->scene.get<TextComponent>(e).scale;
}

static void ICALL_API_SetTextScale(uint64_t entityHandle, float scale) {
    entt::entity e = static_cast<entt::entity>(entityHandle);
    if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
        BOOM_WARN("[ScriptBinding] SetTextScale: Entity doesn't have TextComponent");
        return;
    }
    s_Ctx->scene.get<TextComponent>(e).scale = scale;
}

static void ICALL_API_GetTextPosition(uint64_t entityHandle, glm::vec2* outPos) {
    entt::entity e = static_cast<entt::entity>(entityHandle);
    if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
        *outPos = glm::vec2(0.0f);
        return;
    }
    *outPos = s_Ctx->scene.get<TextComponent>(e).screenPosition;
}

static void ICALL_API_SetTextPosition(uint64_t entityHandle, glm::vec2* pos) {
    entt::entity e = static_cast<entt::entity>(entityHandle);
    if (e == entt::null || !s_Ctx->scene.valid(e) || !s_Ctx->scene.any_of<TextComponent>(e)) {
        BOOM_WARN("[ScriptBinding] SetTextPosition: Entity doesn't have TextComponent");
        return;
    }
    s_Ctx->scene.get<TextComponent>(e).screenPosition = *pos;
}
```

#### 4.2: Register Internal Calls

In the same file, find the `RegisterScriptInternalCalls` function (around line 2000+) and add:

```cpp
void RegisterScriptInternalCalls(Boom::AppContext* context)
{
    s_Ctx = context;

    // ... existing registrations ...

    // Text Component
    mono_add_internal_call("Boom.Native::Boom_API_HasText", (const void*)ICALL_API_HasText);
    mono_add_internal_call("Boom.Native::Boom_API_GetText", (const void*)ICALL_API_GetText);
    mono_add_internal_call("Boom.Native::Boom_API_SetText", (const void*)ICALL_API_SetText);
    mono_add_internal_call("Boom.Native::Boom_API_GetTextColor", (const void*)ICALL_API_GetTextColor);
    mono_add_internal_call("Boom.Native::Boom_API_SetTextColor", (const void*)ICALL_API_SetTextColor);
    mono_add_internal_call("Boom.Native::Boom_API_GetTextScale", (const void*)ICALL_API_GetTextScale);
    mono_add_internal_call("Boom.Native::Boom_API_SetTextScale", (const void*)ICALL_API_SetTextScale);
    mono_add_internal_call("Boom.Native::Boom_API_GetTextPosition", (const void*)ICALL_API_GetTextPosition);
    mono_add_internal_call("Boom.Native::Boom_API_SetTextPosition", (const void*)ICALL_API_SetTextPosition);

    // ... rest of registrations ...
}
```

**Important**: `mono_add_internal_call` links C# function names to C++ implementations. The string format is: `"Namespace.ClassName::MethodName"`

#### 4.3: C# API Declarations

**Location**: `GameScripts/API.cs`

##### Add to `Native` class (around line 350+):

```csharp
// ========= TEXT COMPONENT INTERNAL CALLS =========
[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static bool Boom_API_HasText(ulong handle);

[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static void Boom_API_GetText(ulong handle, out string text);

[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static void Boom_API_SetText(ulong handle, string text);

[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static void Boom_API_GetTextColor(ulong handle, out Vec4 color);

[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static void Boom_API_SetTextColor(ulong handle, ref Vec4 color);

[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static float Boom_API_GetTextScale(ulong handle);

[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static void Boom_API_SetTextScale(ulong handle, float scale);

[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static void Boom_API_GetTextPosition(ulong handle, out Vec2 pos);

[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static void Boom_API_SetTextPosition(ulong handle, ref Vec2 pos);
```

##### Add public API wrappers to `API` class (around line 1050+):

```csharp
// ========== TEXT COMPONENT API ==========

/// <summary>
/// Check if entity has a TextComponent
/// </summary>
public static bool HasText(ulong entity) => Native.Boom_API_HasText(entity);

/// <summary>
/// Get the text content from a TextComponent
/// </summary>
public static string GetText(ulong entity)
{
    Native.Boom_API_GetText(entity, out string text);
    return text;
}

/// <summary>
/// Set the text content of a TextComponent
/// </summary>
public static void SetText(ulong entity, string text)
{
    if (!HasText(entity))
    {
        Console.WriteLine($"[API] Warning: Entity {entity} has no TextComponent");
        return;
    }
    Native.Boom_API_SetText(entity, text);
}

/// <summary>
/// Get the color of a TextComponent (RGBA)
/// </summary>
public static Vec4 GetTextColor(ulong entity)
{
    Native.Boom_API_GetTextColor(entity, out Vec4 color);
    return color;
}

/// <summary>
/// Set the color of a TextComponent (RGBA)
/// </summary>
public static void SetTextColor(ulong entity, Vec4 color)
{
    Native.Boom_API_SetTextColor(entity, ref color);
}

/// <summary>
/// Get the scale/size multiplier of text
/// </summary>
public static float GetTextScale(ulong entity) => Native.Boom_API_GetTextScale(entity);

/// <summary>
/// Set the scale/size multiplier of text
/// </summary>
public static void SetTextScale(ulong entity, float scale)
{
    Native.Boom_API_SetTextScale(entity, scale);
}

/// <summary>
/// Get the screen position of text (2D pixel coordinates)
/// </summary>
public static Vec2 GetTextPosition(ulong entity)
{
    Native.Boom_API_GetTextPosition(entity, out Vec2 pos);
    return pos;
}

/// <summary>
/// Set the screen position of text (2D pixel coordinates)
/// </summary>
public static void SetTextPosition(ulong entity, Vec2 pos)
{
    Native.Boom_API_SetTextPosition(entity, ref pos);
}
```

#### 4.4: Usage Example in C#

Create a test script `ScoreDisplay.cs`:

```csharp
using Boom;

public class ScoreDisplay : ScriptComponent
{
    [EditorExposed]
    public int score = 0;

    private ulong textEntity;

    public override void Start()
    {
        // Find the text entity (assuming it's named "ScoreText")
        textEntity = API.FindEntity("ScoreText");

        if (!API.HasText(textEntity))
        {
            Console.WriteLine("ERROR: ScoreText entity doesn't have TextComponent!");
            return;
        }

        UpdateScoreDisplay();
    }

    public override void Update()
    {
        // Example: Increase score when space is pressed
        if (API.IsKeyDown((int)KeyCode.Space))
        {
            score += 10;
            UpdateScoreDisplay();
        }
    }

    private void UpdateScoreDisplay()
    {
        API.SetText(textEntity, $"Score: {score}");

        // Change color based on score
        if (score >= 100)
        {
            API.SetTextColor(textEntity, new Vec4(1.0f, 0.84f, 0.0f, 1.0f)); // Gold
        }
        else
        {
            API.SetTextColor(textEntity, new Vec4(1.0f, 1.0f, 1.0f, 1.0f)); // White
        }
    }
}
```

---

### Step 5: Implement Rendering System

The final step is rendering all TextComponents every frame.

#### Option A: Add to Existing Render Loop (Quick)

**Location**: `Engine/BoomEngine/src/Application/Application.cpp`

Find the main render loop (around line 920-940 where sprite rendering happens) and add:

```cpp
void Application::RenderFrame(bool isPicking) {
    // ... existing rendering code ...

    // --- Render all TextComponents ---
    if (!isPicking) {  // Don't render text during picking pass
        auto textView = m_Context->scene.view<TextComponent, TransformComponent>();
        for (auto entity : textView) {
            auto& textComp = textView.get<TextComponent>(entity);

            // Skip if renderAs3D is true (3D world-space text not yet implemented)
            if (textComp.renderAs3D) continue;

            // Render text using FontManager
            Boom::FontManager::GetInstance().RenderText(
                textComp.fontName,
                textComp.text,
                textComp.screenPosition.x,
                textComp.screenPosition.y,
                textComp.scale,
                glm::vec3(textComp.color),  // RGB from color
                textComp.color.a            // Alpha from color
            );
        }
    }

    // ... rest of rendering ...
}
```

**Notes**:
- This iterates all entities with TextComponent
- Uses `FontManager::RenderText()` for actual drawing
- `isPicking` check prevents text from interfering with object picking
- Currently only supports 2D overlay text (3D world-space requires projection math)

#### Option B: Create Dedicated TextRenderingSystem (Clean)

For better organization, create a separate system:

**Create**: `Engine/BoomEngine/includes/Graphics/Text/TextRenderingSystem.h`

```cpp
#pragma once
#include "Core.h"
#include "ECS/ECS.hpp"
#include "Graphics/Text/FontManager.h"

namespace Boom {

class BOOM_API TextRenderingSystem {
public:
    static TextRenderingSystem& GetInstance();

    void RenderAllText(entt::registry& registry, bool renderWorldSpace = false);

private:
    TextRenderingSystem() = default;
    ~TextRenderingSystem() = default;

    void RenderScreenSpaceText(TextComponent& textComp);
    void RenderWorldSpaceText(TextComponent& textComp, TransformComponent& transform);
};

} // namespace Boom
```

**Create**: `Engine/BoomEngine/src/Graphics/Text/TextRenderingSystem.cpp`

```cpp
#include "Graphics/Text/TextRenderingSystem.h"
#include "GlobalConstants.h"

namespace Boom {

TextRenderingSystem& TextRenderingSystem::GetInstance() {
    static TextRenderingSystem instance;
    return instance;
}

void TextRenderingSystem::RenderAllText(entt::registry& registry, bool renderWorldSpace) {
    auto view = registry.view<TextComponent>();

    for (auto entity : view) {
        auto& textComp = view.get<TextComponent>(entity);

        // Filter by render mode
        if (textComp.renderAs3D != renderWorldSpace) continue;

        if (renderWorldSpace) {
            // 3D world-space text (requires TransformComponent)
            if (registry.all_of<TransformComponent>(entity)) {
                auto& transform = registry.get<TransformComponent>(entity);
                RenderWorldSpaceText(textComp, transform);
            }
        } else {
            // 2D screen-space text (overlay)
            RenderScreenSpaceText(textComp);
        }
    }
}

void TextRenderingSystem::RenderScreenSpaceText(TextComponent& textComp) {
    // Simple 2D rendering
    FontManager::GetInstance().RenderText(
        textComp.fontName,
        textComp.text,
        textComp.screenPosition.x,
        textComp.screenPosition.y,
        textComp.scale,
        glm::vec3(textComp.color),
        textComp.color.a
    );
}

void TextRenderingSystem::RenderWorldSpaceText(TextComponent& textComp, TransformComponent& transform) {
    // TODO: Implement 3D world-space text
    // Requires:
    // 1. Project 3D position to 2D screen coordinates
    // 2. Apply camera transformation
    // 3. Handle depth testing (billboarding)

    BOOM_WARN("[TextRenderingSystem] 3D world-space text not yet implemented");
}

} // namespace Boom
```

**Then call from Application.cpp**:

```cpp
#include "Graphics/Text/TextRenderingSystem.h"

void Application::RenderFrame(bool isPicking) {
    // ... existing rendering ...

    if (!isPicking) {
        // Render 2D overlay text
        Boom::TextRenderingSystem::GetInstance().RenderAllText(m_Context->scene, false);
    }

    // ... rest of rendering ...
}
```

**Advantages of Option B**:
- Cleaner separation of concerns
- Easier to extend with 3D text support later
- Can add text-specific features (batching, culling, etc.)
- Follows Boom Engine's system architecture pattern

---

### Step 6: Testing & Validation

#### 6.1: Basic Functionality Test

1. **Create Test Entity**:
   - Run the Editor
   - Create new entity (right-click Hierarchy → Create Empty)
   - Name it "TestText"

2. **Add TextComponent**:
   - Select "TestText" entity
   - In Inspector, click "Add Component"
   - Select "Text" from the dropdown
   - TextComponent should appear with default values

3. **Edit Properties**:
   - Change `text` to "Hello Boom Engine!"
   - Change `color` to red (1.0, 0.0, 0.0, 1.0)
   - Change `screenPosition` to (500, 400)
   - Change `scale` to 2.0

4. **Verify Rendering**:
   - Enter Play mode
   - You should see "Hello Boom Engine!" in red at position (500, 400)
   - Text should be 2x normal size

#### 6.2: Script Integration Test

Create `TextTest.cs`:

```csharp
using Boom;
using System;

public class TextTest : ScriptComponent
{
    private float timeElapsed = 0f;

    public override void Update()
    {
        timeElapsed += API.GetDeltaTime();

        // Update text every frame
        API.SetText(Entity, $"Time: {timeElapsed:F2}s");

        // Pulse effect (scale between 1.0 and 1.5)
        float scale = 1.0f + 0.25f * (float)Math.Sin(timeElapsed * 2.0f);
        API.SetTextScale(Entity, scale);

        // Rainbow color effect
        float r = (float)(Math.Sin(timeElapsed) * 0.5f + 0.5f);
        float g = (float)(Math.Sin(timeElapsed + 2.0f) * 0.5f + 0.5f);
        float b = (float)(Math.Sin(timeElapsed + 4.0f) * 0.5f + 0.5f);
        API.SetTextColor(Entity, new Vec4(r, g, b, 1.0f));
    }
}
```

Attach this script to the TestText entity and verify:
- Text content updates every frame
- Scale pulses smoothly
- Color cycles through rainbow

#### 6.3: Serialization Test

1. Save the scene with TextComponent entities
2. Close the Editor
3. Reopen and load the scene
4. Verify all text properties are preserved

#### 6.4: Performance Test

Create 100 text entities and verify:
- Frame rate stays reasonable (>30 FPS)
- No memory leaks
- No crashes

If performance is poor, consider:
- Batching text rendering (single draw call per font)
- Culling off-screen text
- Caching rendered glyphs

---

## Advanced Features

### Feature 1: Text Alignment Implementation

Add alignment logic to the rendering code:

```cpp
void TextRenderingSystem::RenderScreenSpaceText(TextComponent& textComp) {
    float xPos = textComp.screenPosition.x;

    // Calculate text width for alignment
    if (textComp.alignment != TextComponent::Alignment::Left) {
        float textWidth = CalculateTextWidth(textComp.text, textComp.fontName, textComp.scale);

        if (textComp.alignment == TextComponent::Alignment::Center) {
            xPos -= textWidth * 0.5f;
        } else if (textComp.alignment == TextComponent::Alignment::Right) {
            xPos -= textWidth;
        }
    }

    FontManager::GetInstance().RenderText(
        textComp.fontName,
        textComp.text,
        xPos,
        textComp.screenPosition.y,
        textComp.scale,
        glm::vec3(textComp.color),
        textComp.color.a
    );
}

// Helper function to calculate text width
float TextRenderingSystem::CalculateTextWidth(const std::string& text,
                                                const std::string& fontName,
                                                float scale) {
    // This requires adding a GetFont() method to FontManager
    // For now, approximate or extend FontManager API
    float width = 0.0f;
    // Sum up glyph advances for all characters
    // ... implementation depends on FontManager internals
    return width * scale;
}
```

### Feature 2: Multi-Line Text Support

FontManager already supports `\n` newlines. Enhance TextComponent:

```cpp
struct TextComponent {
    // ... existing fields ...
    float lineSpacing = 1.0f;  // Multiplier for line height
};
```

Rendering handles newlines automatically (see FontManager.cpp:192-196).

### Feature 3: 3D World-Space Text (Billboard)

Implement `RenderWorldSpaceText`:

```cpp
void TextRenderingSystem::RenderWorldSpaceText(TextComponent& textComp,
                                                 TransformComponent& transform) {
    // Get active camera
    Camera3D* camera = GetActiveCamera(); // Implement this helper
    if (!camera) return;

    // Project 3D position to screen space
    glm::vec3 worldPos = transform.transform.translate;
    glm::vec4 clipSpace = camera->GetProjectionMatrix() * camera->GetViewMatrix() * glm::vec4(worldPos, 1.0f);

    // Perspective divide
    glm::vec3 ndc = glm::vec3(clipSpace) / clipSpace.w;

    // Convert to screen coordinates
    float screenX = (ndc.x * 0.5f + 0.5f) * CONSTANTS::WINDOW_WIDTH;
    float screenY = (ndc.y * 0.5f + 0.5f) * CONSTANTS::WINDOW_HEIGHT;

    // Depth test: skip if behind camera
    if (clipSpace.w < 0.0f) return;

    // Render at calculated screen position
    FontManager::GetInstance().RenderText(
        textComp.fontName,
        textComp.text,
        screenX,
        screenY,
        textComp.scale,
        glm::vec3(textComp.color),
        textComp.color.a
    );
}
```

### Feature 4: Text Outline/Shadow

Extend FontManager or render text multiple times with offset:

```cpp
// Render shadow (offset and darker)
FontManager::GetInstance().RenderText(
    textComp.fontName,
    textComp.text,
    xPos + 2.0f,  // Offset right
    yPos - 2.0f,  // Offset down
    textComp.scale,
    glm::vec3(0.0f, 0.0f, 0.0f),  // Black shadow
    textComp.color.a * 0.5f       // Semi-transparent
);

// Render main text
FontManager::GetInstance().RenderText(
    textComp.fontName,
    textComp.text,
    xPos, yPos,
    textComp.scale,
    glm::vec3(textComp.color),
    textComp.color.a
);
```

### Feature 5: Rich Text Tags

Parse markup like `<color=red>Hello</color>`:

```cpp
struct TextSpan {
    std::string text;
    glm::vec4 color;
    float scale;
};

std::vector<TextSpan> ParseRichText(const std::string& input) {
    // Simple parser for <color=...> tags
    // ... parsing logic ...
}

void RenderRichText(const std::vector<TextSpan>& spans, ...) {
    float xOffset = 0.0f;
    for (const auto& span : spans) {
        FontManager::GetInstance().RenderText(..., xPos + xOffset, ...);
        xOffset += CalculateTextWidth(span.text, ...);
    }
}
```

---

## Complete Code Reference

### ECS.hpp - TextComponent Definition

```cpp
// Add to ECS.hpp around line 760
struct TextComponent {
    std::string text = "New Text";
    std::string fontName = "Roboto-Regular";
    glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    float scale = 1.0f;
    glm::vec2 screenPosition{ 100.0f, 100.0f };
    bool renderAs3D = false;

    enum class Alignment {
        Left = 0,
        Center = 1,
        Right = 2
    };
    Alignment alignment = Alignment::Left;

    XPROPERTY_DEF(
        "TextComponent", TextComponent,
        obj_member<"text", &TextComponent::text>,
        obj_member<"fontName", &TextComponent::fontName>,
        obj_member<"color", &TextComponent::color>,
        obj_member<"scale", &TextComponent::scale>,
        obj_member<"screenPosition", &TextComponent::screenPosition>,
        obj_member<"renderAs3D", &TextComponent::renderAs3D>
    )
};

// Update enum (line ~18)
enum class ComponentID : size_t {
    // ... existing ...
    SPRITE,
    TEXT,              // ← ADD
    MENU_COMPONENT,
    // ... rest ...
};

// Update names array (line ~32)
constexpr std::string_view COMPONENT_NAMES[]{
    // ... existing ...
    "Sprite",          // 15
    "Text",            // 16 ← ADD
    "Menu Component",  // 17
    // ... rest ...
};
```

### InspectorPanel.cpp - Component Selector

```cpp
// Add to ComponentSelector() around line 3568
UpdateComponent<Boom::TextComponent>(Boom::ComponentID::TEXT, selected);
```

### ScriptBinding.cpp - Summary

```cpp
// Add internal call implementations (line ~940)
static bool ICALL_API_HasText(uint64_t entityHandle) { /*...*/ }
static void ICALL_API_GetText(uint64_t entityHandle, MonoString** outText) { /*...*/ }
static void ICALL_API_SetText(uint64_t entityHandle, MonoString* newText) { /*...*/ }
static void ICALL_API_GetTextColor(uint64_t entityHandle, glm::vec4* outColor) { /*...*/ }
static void ICALL_API_SetTextColor(uint64_t entityHandle, glm::vec4* color) { /*...*/ }
static float ICALL_API_GetTextScale(uint64_t entityHandle) { /*...*/ }
static void ICALL_API_SetTextScale(uint64_t entityHandle, float scale) { /*...*/ }
static void ICALL_API_GetTextPosition(uint64_t entityHandle, glm::vec2* outPos) { /*...*/ }
static void ICALL_API_SetTextPosition(uint64_t entityHandle, glm::vec2* pos) { /*...*/ }

// Register in RegisterScriptInternalCalls() (line ~2100)
mono_add_internal_call("Boom.Native::Boom_API_HasText", (const void*)ICALL_API_HasText);
mono_add_internal_call("Boom.Native::Boom_API_GetText", (const void*)ICALL_API_GetText);
mono_add_internal_call("Boom.Native::Boom_API_SetText", (const void*)ICALL_API_SetText);
mono_add_internal_call("Boom.Native::Boom_API_GetTextColor", (const void*)ICALL_API_GetTextColor);
mono_add_internal_call("Boom.Native::Boom_API_SetTextColor", (const void*)ICALL_API_SetTextColor);
mono_add_internal_call("Boom.Native::Boom_API_GetTextScale", (const void*)ICALL_API_GetTextScale);
mono_add_internal_call("Boom.Native::Boom_API_SetTextScale", (const void*)ICALL_API_SetTextScale);
mono_add_internal_call("Boom.Native::Boom_API_GetTextPosition", (const void*)ICALL_API_GetTextPosition);
mono_add_internal_call("Boom.Native::Boom_API_SetTextPosition", (const void*)ICALL_API_SetTextPosition);
```

### API.cs - C# Wrapper Summary

```csharp
// Native class declarations (line ~350)
[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static bool Boom_API_HasText(ulong handle);
[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static void Boom_API_GetText(ulong handle, out string text);
[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static void Boom_API_SetText(ulong handle, string text);
// ... (7 more methods)

// API class public wrappers (line ~1050)
public static bool HasText(ulong entity) => Native.Boom_API_HasText(entity);
public static string GetText(ulong entity) { /*...*/ }
public static void SetText(ulong entity, string text) { /*...*/ }
public static Vec4 GetTextColor(ulong entity) { /*...*/ }
public static void SetTextColor(ulong entity, Vec4 color) { /*...*/ }
public static float GetTextScale(ulong entity) => Native.Boom_API_GetTextScale(entity);
public static void SetTextScale(ulong entity, float scale) { /*...*/ }
public static Vec2 GetTextPosition(ulong entity) { /*...*/ }
public static void SetTextPosition(ulong entity, Vec2 pos) { /*...*/ }
```

### Application.cpp - Rendering

```cpp
// Add to RenderFrame() around line 930
if (!isPicking) {
    auto textView = m_Context->scene.view<TextComponent, TransformComponent>();
    for (auto entity : textView) {
        auto& textComp = textView.get<TextComponent>(entity);
        if (textComp.renderAs3D) continue;

        Boom::FontManager::GetInstance().RenderText(
            textComp.fontName,
            textComp.text,
            textComp.screenPosition.x,
            textComp.screenPosition.y,
            textComp.scale,
            glm::vec3(textComp.color),
            textComp.color.a
        );
    }
}
```

---

## Troubleshooting

### Issue 1: "Text Component" Not in Add Component Menu

**Symptoms**: Can't find "Text" in the component dropdown

**Fixes**:
1. Check `ComponentID` enum has `TEXT` entry
2. Check `COMPONENT_NAMES` array has "Text" at matching index
3. Check `ComponentSelector()` has `UpdateComponent<TextComponent>` line
4. Rebuild solution (component changes require full rebuild)

### Issue 2: Text Not Rendering

**Symptoms**: TextComponent visible in Inspector but no text on screen

**Checklist**:
- ✅ FontManager initialized? (check console logs)
- ✅ Font loaded? (e.g., "Roboto-Regular")
- ✅ `textComp.fontName` matches loaded font exactly?
- ✅ Text position on screen? (not off-screen like x=-1000)
- ✅ Alpha > 0? (check `textComp.color.a`)
- ✅ Text not empty? (check `textComp.text != ""`)
- ✅ Rendering code in correct pass? (not in `isPicking`)

### Issue 3: Inspector Shows No Properties

**Symptoms**: TextComponent appears but fields are empty/missing

**Fixes**:
1. Check `XPROPERTY_DEF` macro syntax (commas, quotes, semicolons)
2. Ensure all member variables have `obj_member<>` entries
3. Rebuild GameScripts.dll (C# changes)
4. Rebuild BoomEngine (C++ changes)

### Issue 4: C# API Crashes

**Symptoms**: Game crashes when calling `API.SetText()`

**Debug Steps**:
1. Check entity handle is valid (not 0 or null)
2. Check entity has TextComponent (`API.HasText()` first)
3. Check string isn't null in C#
4. Add BOOM_INFO logs in C++ internal calls to trace execution
5. Verify `mono_add_internal_call` names match exactly (case-sensitive!)

Example debug log:

```cpp
static void ICALL_API_SetText(uint64_t entityHandle, MonoString* newText) {
    BOOM_INFO("[ICALL] SetText called for entity {}", entityHandle);

    entt::entity e = static_cast<entt::entity>(entityHandle);
    if (e == entt::null) {
        BOOM_ERROR("[ICALL] Entity handle is null!");
        return;
    }

    if (!s_Ctx->scene.valid(e)) {
        BOOM_ERROR("[ICALL] Entity {} is not valid!", (uint32_t)e);
        return;
    }

    if (!s_Ctx->scene.any_of<TextComponent>(e)) {
        BOOM_ERROR("[ICALL] Entity {} has no TextComponent!", (uint32_t)e);
        return;
    }

    char* cStr = mono_string_to_utf8(newText);
    BOOM_INFO("[ICALL] Setting text to: {}", cStr);
    s_Ctx->scene.get<TextComponent>(e).text = std::string(cStr);
    mono_free(cStr);
}
```

### Issue 5: Serialization Fails

**Symptoms**: TextComponent doesn't save/load with scene

**Fixes**:
1. Check `XPROPERTY_DEF` includes all fields you want saved
2. Verify YAML file after save (open in text editor)
3. Check for missing commas in macro
4. Enum types (like Alignment) need custom serialization handlers

Example YAML output (should look like this):

```yaml
entities:
  - components:
      TextComponent:
        text: "Hello World"
        fontName: "Roboto-Regular"
        color: [1.0, 0.0, 0.0, 1.0]
        scale: 2.0
        screenPosition: [500.0, 300.0]
        renderAs3D: false
```

### Issue 6: Performance Issues (Many Text Entities)

**Symptoms**: Frame rate drops with >50 text entities

**Optimizations**:

1. **Batch Rendering**:
```cpp
// Group by font, render in batches
std::unordered_map<std::string, std::vector<TextComponent*>> batchesByFont;
for (auto& textComp : allText) {
    batchesByFont[textComp.fontName].push_back(&textComp);
}
for (auto& [font, texts] : batchesByFont) {
    // Render all texts with same font together (reduce texture binds)
}
```

2. **Frustum Culling** (skip off-screen text):
```cpp
if (xPos < 0 || xPos > WINDOW_WIDTH || yPos < 0 || yPos > WINDOW_HEIGHT) {
    continue; // Skip off-screen
}
```

3. **Dirty Flag** (only re-render when changed):
```cpp
struct TextComponent {
    // ... existing fields ...
    bool dirty = true;  // Mark when text/properties change
    GLuint cachedTexture = 0;  // Cached pre-rendered texture
};
```

---

## Summary Checklist

Use this checklist when implementing the system:

### C++ Engine Side
- [ ] Define `TextComponent` struct in `ECS.hpp`
- [ ] Add `TEXT` to `ComponentID` enum
- [ ] Add "Text" to `COMPONENT_NAMES` array
- [ ] Add `UpdateComponent<TextComponent>` to `ComponentSelector()`
- [ ] Implement 9 ICALL functions in `ScriptBinding.cpp`
- [ ] Register 9 internal calls in `RegisterScriptInternalCalls()`
- [ ] Add rendering loop in `Application.cpp` or `TextRenderingSystem`
- [ ] Test rendering with basic text

### C# Scripts Side
- [ ] Declare 9 internal calls in `Native` class in `API.cs`
- [ ] Implement 9 public wrapper functions in `API` class
- [ ] Test with simple script (SetText, GetText)
- [ ] Verify color, scale, position changes work

### Testing & Validation
- [ ] Create entity with TextComponent in editor
- [ ] Edit properties in Inspector
- [ ] Save and load scene (serialization test)
- [ ] Run C# script test
- [ ] Verify text renders correctly
- [ ] Check performance with multiple text entities

---

## Next Steps

After completing this guide, consider:

1. **Text Layout System**:
   - Word wrapping
   - Paragraph formatting
   - Text boxes with bounds

2. **Localization Support**:
   - String tables
   - Language switching
   - Unicode support (extend FontManager)

3. **Animated Text**:
   - Typewriter effect
   - Fade in/out
   - Character-by-character animation

4. **UI Integration**:
   - Anchor to UI elements
   - Responsive positioning
   - Z-order/layer support

5. **Advanced Rendering**:
   - SDF (Signed Distance Field) fonts for crisp scaling
   - GPU-based text rendering (vertex buffers)
   - Text mesh generation for 3D text

---

## See Also

- **FontSystemImplementation.md** - Low-level FontManager details
- **BoomFontSystem_Guide.md** - User-facing font usage guide
- **XPROPERTY Documentation** - Serialization system
- **EnTT Documentation** - Entity-component system
- **Mono Documentation** - C# interop

---

**Document Version**: 1.0
**Last Updated**: 2026-02-01
**Author**: Boom Engine Team
**Tested With**: Boom Engine v3.0, Mono 6.12, C++17
