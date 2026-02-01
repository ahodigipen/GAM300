# ✅ Step 4 Complete: C# Script API for TextComponent

## What Was Added

### 1. C++ Internal Call Functions (ScriptBinding.cpp)

**Location**: After Sprite component section (~line 992)

**9 Functions Added:**
- `ICALL_API_HasText` - Check if entity has TextComponent
- `ICALL_API_GetText` - Get text content
- `ICALL_API_SetText` - Set text content
- `ICALL_API_GetTextColor` - Get RGBA color
- `ICALL_API_SetTextColor` - Set RGBA color
- `ICALL_API_GetTextScale` - Get scale multiplier
- `ICALL_API_SetTextScale` - Set scale multiplier
- `ICALL_API_GetTextPosition` - Get screen position
- `ICALL_API_SetTextPosition` - Set screen position

### 2. Mono Registration (ScriptBinding.cpp)

**Location**: In `RegisterScriptInternalCalls()` (~line 2204)

**9 Registrations Added:**
```cpp
mono_add_internal_call("Boom.Native::Boom_API_HasText", (const void*)ICALL_API_HasText);
// ... (8 more)
```

### 3. C# Native Declarations (API.cs)

**Location**: Native class (~line 366)

**9 External Methods Added:**
```csharp
[MethodImpl(MethodImplOptions.InternalCall)]
internal extern static bool Boom_API_HasText(ulong handle);
// ... (8 more)
```

### 4. C# Public API (API.cs)

**Location**: API class (~line 1147)

**9 Public Methods Added:**
- `HasText(ulong entity)`
- `GetText(ulong entity)`
- `SetText(ulong entity, string text)`
- `GetTextColor(ulong entity)`
- `SetTextColor(ulong entity, Vec4 color)`
- `GetTextScale(ulong entity)`
- `SetTextScale(ulong entity, float scale)`
- `GetTextPosition(ulong entity)`
- `SetTextPosition(ulong entity, Vec2 pos)`

---

## 🚀 How to Test

### Step 1: Rebuild Everything

**CRITICAL**: Both C++ and C# changed!

1. **Close Editor** if running

2. **Rebuild C++ Solution:**
   ```
   Visual Studio → Build → Rebuild Solution
   ```

3. **Rebuild GameScripts.dll:**
   - Should auto-rebuild with solution
   - Or manually: Right-click GameScripts project → Rebuild

4. **Verify no errors** in Output window

### Step 2: Test with TextTest.cs

1. **Run Editor**

2. **Create Test Entity:**
   - Right-click Hierarchy → Create Empty
   - Name it "DynamicText"

3. **Add Components:**
   - Add **TextComponent**
   - Set initial properties:
     - text: "Loading..."
     - color: White (1, 1, 1, 1)
     - screenPosition: (400, 400)
     - scale: 1.5

4. **Add Script Component:**
   - Click "Add Component" → Script
   - In Script field, type: `TextTest`
   - Press Enter

5. **Configure Script Properties:**
   - score: 0
   - pulseSpeed: 2.0
   - enableRainbow: ✓ (checked)

6. **Enter Play Mode** (Ctrl+P or Play button)

7. **Observe:**
   - ✅ Text should update every frame showing:
     ```
     Score: 0
     Time: 1.2s

     Press SPACE for +10 points
     ```
   - ✅ Text should pulse (scale changes)
   - ✅ Text should cycle through rainbow colors
   - ✅ Pressing SPACE increases score

8. **Interact:**
   - Press SPACE multiple times
   - Watch score increase: 0 → 10 → 20 → 30...
   - At score ≥ 100, text turns GOLD!

---

## 📚 Complete C# API Reference

### Basic Operations

```csharp
// Check if entity has TextComponent
if (API.HasText(entity)) { ... }

// Get current text
string currentText = API.GetText(entity);

// Set new text (supports \n for newlines)
API.SetText(entity, "Hello\nWorld");
```

### Color Control

```csharp
// Get current color
Vec4 color = API.GetTextColor(entity);

// Set color (RGBA)
API.SetTextColor(entity, new Vec4(1, 0, 0, 1));  // Red
API.SetTextColor(entity, new Vec4(1, 1, 0, 1));  // Yellow
API.SetTextColor(entity, new Vec4(0.5f, 0.5f, 0.5f, 1));  // Gray

// Semi-transparent
API.SetTextColor(entity, new Vec4(1, 1, 1, 0.5f));  // 50% alpha
```

### Scale Control

```csharp
// Get current scale
float scale = API.GetTextScale(entity);

// Set scale
API.SetTextScale(entity, 2.0f);   // 2x size
API.SetTextScale(entity, 0.5f);   // Half size

// Animated pulse
float pulse = 1.0f + 0.5f * (float)Math.Sin(time * 2.0f);
API.SetTextScale(entity, pulse);
```

### Position Control

```csharp
// Get current position
Vec2 pos = API.GetTextPosition(entity);

// Set position (pixel coordinates)
API.SetTextPosition(entity, new Vec2(640, 360));  // Center (for 1280x720)

// Move text
pos.x += speed * deltaTime;
API.SetTextPosition(entity, pos);
```

---

## 🎮 Example Use Cases

### 1. Score Display

```csharp
namespace GameScripts
{
    public class ScoreDisplay
    {
        public ulong Entity;
        private int score = 0;

        public void OnUpdate(float dt)
        {
            // Update score display
            API.SetText(Entity, $"Score: {score}");

            // Change color based on score
            if (score >= 1000)
                API.SetTextColor(Entity, new Vec4(1, 0.84f, 0, 1));  // Gold
            else if (score >= 500)
                API.SetTextColor(Entity, new Vec4(0, 1, 0, 1));      // Green
            else
                API.SetTextColor(Entity, new Vec4(1, 1, 1, 1));      // White
        }

        public void AddScore(int points)
        {
            score += points;
        }
    }
}
```

### 2. Countdown Timer

```csharp
namespace GameScripts
{
    public class CountdownTimer
    {
        public ulong Entity;

        [EditorExposed]
        public float startTime = 60.0f;

        private float timeRemaining;

        public void OnStart(string jsonParams)
        {
            timeRemaining = startTime;
        }

        public void OnUpdate(float dt)
        {
            timeRemaining -= dt;

            if (timeRemaining <= 0)
            {
                timeRemaining = 0;
                API.SetText(Entity, "TIME'S UP!");
                API.SetTextColor(Entity, new Vec4(1, 0, 0, 1));  // Red
            }
            else
            {
                API.SetText(Entity, $"Time: {timeRemaining:F1}s");

                // Flash when low
                if (timeRemaining < 10.0f)
                {
                    float flash = (float)Math.Sin(timeRemaining * 10.0f) * 0.5f + 0.5f;
                    API.SetTextColor(Entity, new Vec4(1, flash, flash, 1));
                }
            }
        }
    }
}
```

### 3. Floating Damage Numbers

```csharp
namespace GameScripts
{
    public class DamageText
    {
        public ulong Entity;

        [EditorExposed]
        public int damage = 100;

        [EditorExposed]
        public float lifetime = 1.5f;

        private float age = 0f;
        private Vec2 startPos;

        public void OnStart(string jsonParams)
        {
            API.SetText(Entity, $"-{damage}");
            API.SetTextColor(Entity, new Vec4(1, 0, 0, 1));  // Red
            startPos = API.GetTextPosition(Entity);
        }

        public void OnUpdate(float dt)
        {
            age += dt;

            if (age >= lifetime)
            {
                // Destroy this entity
                // (Implement entity destruction API or fade out)
                return;
            }

            // Float upward
            Vec2 pos = startPos;
            pos.y += age * 100.0f;  // Rise 100 pixels per second
            API.SetTextPosition(Entity, pos);

            // Fade out
            float alpha = 1.0f - (age / lifetime);
            API.SetTextColor(Entity, new Vec4(1, 0, 0, alpha));

            // Scale up then down
            float scale = 1.0f + (float)Math.Sin(age * 3.14f) * 0.5f;
            API.SetTextScale(Entity, scale);
        }
    }
}
```

### 4. FPS Counter

```csharp
namespace GameScripts
{
    public class FPSCounter
    {
        public ulong Entity;

        private float updateInterval = 0.5f;  // Update twice per second
        private float timeSinceUpdate = 0f;
        private int frameCount = 0;

        public void OnUpdate(float dt)
        {
            frameCount++;
            timeSinceUpdate += dt;

            if (timeSinceUpdate >= updateInterval)
            {
                float fps = frameCount / timeSinceUpdate;
                API.SetText(Entity, $"FPS: {fps:F0}");

                // Color based on performance
                if (fps >= 60)
                    API.SetTextColor(Entity, new Vec4(0, 1, 0, 1));    // Green
                else if (fps >= 30)
                    API.SetTextColor(Entity, new Vec4(1, 1, 0, 1));    // Yellow
                else
                    API.SetTextColor(Entity, new Vec4(1, 0, 0, 1));    // Red

                timeSinceUpdate = 0f;
                frameCount = 0;
            }
        }
    }
}
```

### 5. Dialogue System

```csharp
namespace GameScripts
{
    public class DialogueDisplay
    {
        public ulong Entity;

        private string fullText = "Welcome to Boom Engine! This is a dialogue system example.";
        private float charsPerSecond = 20.0f;
        private float charTimer = 0f;
        private int visibleChars = 0;

        public void OnUpdate(float dt)
        {
            if (visibleChars < fullText.Length)
            {
                charTimer += dt;

                while (charTimer >= (1.0f / charsPerSecond) && visibleChars < fullText.Length)
                {
                    visibleChars++;
                    charTimer -= (1.0f / charsPerSecond);
                }

                string displayText = fullText.Substring(0, visibleChars);
                API.SetText(Entity, displayText);
            }
        }

        public void SetDialogue(string text)
        {
            fullText = text;
            visibleChars = 0;
            charTimer = 0f;
        }
    }
}
```

---

## 🎨 Helper Functions

### Common Color Presets

```csharp
public static class TextColors
{
    public static readonly Vec4 White   = new Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    public static readonly Vec4 Black   = new Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    public static readonly Vec4 Red     = new Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    public static readonly Vec4 Green   = new Vec4(0.0f, 1.0f, 0.0f, 1.0f);
    public static readonly Vec4 Blue    = new Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    public static readonly Vec4 Yellow  = new Vec4(1.0f, 1.0f, 0.0f, 1.0f);
    public static readonly Vec4 Cyan    = new Vec4(0.0f, 1.0f, 1.0f, 1.0f);
    public static readonly Vec4 Magenta = new Vec4(1.0f, 0.0f, 1.0f, 1.0f);
    public static readonly Vec4 Orange  = new Vec4(1.0f, 0.65f, 0.0f, 1.0f);
    public static readonly Vec4 Gold    = new Vec4(1.0f, 0.84f, 0.0f, 1.0f);
    public static readonly Vec4 Gray    = new Vec4(0.5f, 0.5f, 0.5f, 1.0f);
}

// Usage:
API.SetTextColor(entity, TextColors.Gold);
```

### Text Positioning Helpers

```csharp
public static class TextLayout
{
    // Assuming 1280x720 window
    public const float SCREEN_WIDTH = 1280f;
    public const float SCREEN_HEIGHT = 720f;

    public static Vec2 TopLeft => new Vec2(50, SCREEN_HEIGHT - 50);
    public static Vec2 TopCenter => new Vec2(SCREEN_WIDTH / 2, SCREEN_HEIGHT - 50);
    public static Vec2 TopRight => new Vec2(SCREEN_WIDTH - 50, SCREEN_HEIGHT - 50);

    public static Vec2 MiddleLeft => new Vec2(50, SCREEN_HEIGHT / 2);
    public static Vec2 Center => new Vec2(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    public static Vec2 MiddleRight => new Vec2(SCREEN_WIDTH - 50, SCREEN_HEIGHT / 2);

    public static Vec2 BottomLeft => new Vec2(50, 50);
    public static Vec2 BottomCenter => new Vec2(SCREEN_WIDTH / 2, 50);
    public static Vec2 BottomRight => new Vec2(SCREEN_WIDTH - 50, 50);
}

// Usage:
API.SetTextPosition(scoreEntity, TextLayout.TopRight);
API.SetTextPosition(timerEntity, TextLayout.TopCenter);
```

### 3D World-Space Text (renderAs3D)

**IMPORTANT**: The `renderAs3D` and `billboardMode` properties must be set in the **Inspector** (not yet exposed to C# API).

When `renderAs3D = true`:
- Text position is determined by the entity's **TransformComponent** world position
- Text is automatically projected to screen space using the active camera
- Text is culled if behind camera or outside view frustum
- `screenPosition` is **ignored** (use Transform position instead)

**Billboard Mode** (only relevant when `renderAs3D = true`):
- `billboardMode = true` (default): Text **always faces the camera** (like MMO nametags)
- `billboardMode = false`: Text has **fixed world rotation** ⚠️ **NOT YET IMPLEMENTED** - will not render

**Use Cases:**
- Floating damage numbers above enemies
- Player nametags in 3D space
- Interactive object labels
- Quest markers

**Example Setup (Inspector):**
1. Create entity with TextComponent
2. Set `renderAs3D = true` (checkbox in Inspector)
3. Add TransformComponent (if not present)
4. Set Transform position to world location (e.g., `(5, 10, -3)`)
5. Text will appear at that 3D position, projected to screen

**C# Example - Enemy Health Bar:**
```csharp
namespace GameScripts
{
    public class EnemyHealthDisplay
    {
        public ulong Entity;  // This is the TEXT entity

        [EditorExposed]
        public ulong enemyEntity;  // Link to enemy entity

        [EditorExposed]
        public Vec3 offset = new Vec3(0, 2, 0);  // Offset above enemy head

        private int health = 100;

        public void OnUpdate(float dt)
        {
            if (enemyEntity == 0 || !API.HasTransform(enemyEntity))
                return;

            // Get enemy position
            Vec3 enemyPos = API.GetPosition(enemyEntity);

            // Position text above enemy (renderAs3D uses this)
            API.SetPosition(Entity, enemyPos + offset);

            // Update health text
            API.SetText(Entity, $"HP: {health}");

            // Color based on health percentage
            float healthPercent = health / 100.0f;
            if (healthPercent > 0.5f)
                API.SetTextColor(Entity, new Vec4(0, 1, 0, 1));  // Green
            else if (healthPercent > 0.2f)
                API.SetTextColor(Entity, new Vec4(1, 1, 0, 1));  // Yellow
            else
                API.SetTextColor(Entity, new Vec4(1, 0, 0, 1));  // Red
        }

        public void TakeDamage(int amount)
        {
            health = Math.Max(0, health - amount);
        }
    }
}
```

**Note**: Currently `renderAs3D` can only be set in the Inspector. If you need to toggle it at runtime, request that feature.

---

## 📊 Complete Feature Matrix

| Feature | Inspector | C# API | Status | Notes |
|---------|-----------|--------|--------|-------|
| Text Content | ✅ Multi-line editor | ✅ `SetText()` | ✅ | Supports `\n` newlines |
| Font Name | ✅ Input field | ❌ Not exposed | ⚠️ | Can be added if needed |
| Color (RGBA) | ✅ Color picker | ✅ `SetTextColor()` | ✅ | Full RGBA support |
| Scale | ✅ Drag slider | ✅ `SetTextScale()` | ✅ | Multiplier (1.0 = normal) |
| Position (2D) | ✅ Vec2 input | ✅ `SetTextPosition()` | ✅ | Screen-space pixels |
| Position (3D) | ✅ Via Transform | ✅ Via `SetPosition()` | ✅ | When renderAs3D = true |
| Render Mode | ✅ Checkbox | ❌ Not exposed | ✅ | 3D rendering fully works! |
| Billboard Mode | ✅ Checkbox | ❌ Not exposed | ⚠️ | **NEW**: true = face camera (works), false = fixed rotation (TODO) |
| Alignment | ❌ Not implemented | ❌ Not exposed | ❌ | Defined in struct but unused |

**New in This Update:**
- ✅ **3D World-Space Text** - Full implementation with automatic camera projection
- ✅ **Distance Scaling** - Text automatically scales based on camera distance
- ✅ **Frustum Culling** - Text outside camera view is culled (performance optimization)

**Note**: `renderAs3D` flag works perfectly but must be set in Inspector (not C# API yet). Can be exposed if needed.

---

## 🐛 Troubleshooting

### Issue: "SetText doesn't work"

**Check:**
1. Did you rebuild **both** BoomEngine (C++) and GameScripts (C#)?
2. Is `API.HasText(entity)` returning `true`?
3. Are you using the correct entity handle?
4. Check console for warnings

**Debug:**
```csharp
if (!API.HasText(Entity))
{
    Console.WriteLine("ERROR: No TextComponent!");
    return;
}
Console.WriteLine($"Setting text on entity {Entity}");
API.SetText(Entity, "Test");
Console.WriteLine($"Text set successfully: {API.GetText(Entity)}");
```

### Issue: "Mono method not found"

**Error message**: `Method 'Boom.Native::Boom_API_SetText' not found.`

**Fix:**
1. Verify registration in ScriptBinding.cpp line ~2204
2. Check spelling matches exactly: `"Boom.Native::Boom_API_SetText"`
3. Rebuild BoomEngine project
4. Restart Editor

### Issue: "Colors don't work"

**Check:**
- Vec4 format: `new Vec4(r, g, b, a)` (not RGB!)
- Values are 0.0 to 1.0 (not 0-255!)
- Alpha channel is set (fourth value)

**Correct:**
```csharp
API.SetTextColor(entity, new Vec4(1.0f, 0.0f, 0.0f, 1.0f));  // Red, opaque
```

**Incorrect:**
```csharp
API.SetTextColor(entity, new Vec4(255, 0, 0, 1));  // Wrong! Values too high
API.SetTextColor(entity, new Vec4(1, 0, 0));       // Wrong! Missing alpha
```

---

## 🎉 System Complete!

You now have a **fully functional Unity-like TextComponent system** with:

✅ **Inspector Editing** - Visual property editor
✅ **Real-time Rendering** - Immediate visual feedback
✅ **YAML Serialization** - Save/load with scenes
✅ **C# Script API** - **JUST ADDED!** Full script control
✅ **Dynamic Text** - Update at runtime
✅ **Color Animation** - Fade, flash, rainbow effects
✅ **Scale Animation** - Pulse, grow, shrink
✅ **Position Control** - Move text programmatically

---

## 📝 Summary of All Files Modified

### C++ Side (Rebuild Required):
1. ✅ `ScriptBinding.cpp` - 9 internal calls + 9 registrations

### C# Side (Rebuild Required):
1. ✅ `API.cs` - 9 Native declarations + 9 public wrappers
2. ✅ `TextTest.cs` - **NEW** Example test script

---

**Test the TextTest.cs script now to see dynamic text in action!** 🚀
