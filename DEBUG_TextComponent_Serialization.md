# Debug Guide: TextComponent Serialization Issue

## Problem
Changes made to TextComponent in the Inspector don't save to the YAML scene file.

---

## Step 1: Verify Rebuild

**You MUST rebuild** after adding the serialization registration!

1. Close Editor if running
2. In Visual Studio: **Build → Rebuild Solution**
3. Wait for complete rebuild (not just Build, use Rebuild!)
4. Check Output window for errors

**Why?** We added `RegisterPropertyComponent<TextComponent>` to ComponentSerializer.cpp. This code must be recompiled and linked.

---

## Step 2: Check YAML File Directly

### Test Procedure:

1. Run Editor
2. Create entity called "DebugText"
3. Add TextComponent
4. Set properties:
   - text: "SERIALIZATION TEST 123"
   - fontName: "TestFont"
   - color: (0.5, 0.25, 0.75, 1.0)
   - scale: 3.5
   - screenPosition: (777, 888)
   - renderAs3D: true

5. **Save scene** as "SerializationDebug.yaml"

6. **Close Editor completely** (don't just exit play mode)

7. **Open YAML file** in text editor:
   ```
   Gam300/Editor/Scenes/SerializationDebug.yaml
   ```

8. **Search for "TextComponent"**

### Expected Result (GOOD):

```yaml
entities:
  - components:
      InfoComponent:
        Name: "DebugText"
      TextComponent:
        text: "SERIALIZATION TEST 123"
        fontName: "TestFont"
        color: [0.5, 0.25, 0.75, 1.0]
        scale: 3.5
        screenPosition: [777.0, 888.0]
        renderAs3D: true
```

### Possible Bad Results:

**Scenario A: No TextComponent at all**
```yaml
entities:
  - components:
      InfoComponent:
        Name: "DebugText"
      # TextComponent missing entirely!
```
→ **Cause**: Serialization not registered. Rebuild solution!

**Scenario B: TextComponent with default values**
```yaml
TextComponent:
  text: "New Text"
  fontName: "Roboto-Regular"
  color: [1.0, 1.0, 1.0, 1.0]
  scale: 1.0
  screenPosition: [100.0, 100.0]
  renderAs3D: false
```
→ **Cause**: Inspector changes not persisting to component. See Step 3.

**Scenario C: TextComponent partially saved**
```yaml
TextComponent:
  text: "SERIALIZATION TEST 123"  # Saved!
  fontName: "TestFont"             # Saved!
  # Other fields missing
```
→ **Cause**: Some properties not in XPROPERTY_DEF. Check ECS.hpp.

---

## Step 3: Verify Inspector Changes Persist

### Test: Do changes stick?

1. Run Editor
2. Create/select entity with TextComponent
3. Change text to "TEST A"
4. **Select a different entity** (click another in hierarchy)
5. **Re-select original entity**
6. Check Inspector - does it still say "TEST A"?

**If NO** (reverts to old value):
→ Problem is in Inspector code, not serialization

**If YES** (shows "TEST A"):
→ Inspector is working, problem is in save/load

### Fix for Inspector Not Persisting:

The issue might be that we're modifying a temporary copy. Verify the Inspector code has:

```cpp
auto& textComp = selected.Get<Boom::TextComponent>();  // Must be reference (&)!
```

Not:
```cpp
auto textComp = selected.Get<Boom::TextComponent>();  // Missing & = copy, not reference!
```

---

## Step 4: Add Debug Logging

Temporarily add logs to verify serialization is called:

### Edit: `ComponentSerializer.cpp` (line ~29)

```cpp
RegisterPropertyComponent<TextComponent>("TextComponent");
```

Change the template to add logging:

```cpp
// TEMPORARY DEBUG VERSION
registry.RegisterComponentSerializer(
    "TextComponent",
    // ===== SERIALIZE =====
    [](YAML::Emitter& e, EntityRegistry& scene, EntityID entity) {
        if (scene.all_of<TextComponent>(entity)) {
            auto& comp = scene.get<TextComponent>(entity);

            BOOM_INFO("========================================");
            BOOM_INFO("[DEBUG] Serializing TextComponent:");
            BOOM_INFO("  text: '{}'", comp.text);
            BOOM_INFO("  fontName: '{}'", comp.fontName);
            BOOM_INFO("  color: ({}, {}, {}, {})", comp.color.r, comp.color.g, comp.color.b, comp.color.a);
            BOOM_INFO("  scale: {}", comp.scale);
            BOOM_INFO("  screenPosition: ({}, {})", comp.screenPosition.x, comp.screenPosition.y);
            BOOM_INFO("  renderAs3D: {}", comp.renderAs3D);
            BOOM_INFO("========================================");

            e << YAML::Key << "TextComponent" << YAML::Value << YAML::BeginMap;

            xproperty::settings::context ctx;
            if (auto* pObj = xproperty::getObject(comp)) {
                SerializeObjectToYAML(e, pObj, (void*)&comp, ctx);
            } else {
                BOOM_ERROR("[DEBUG] Failed to get object info for TextComponent!");
            }

            e << YAML::EndMap;
        }
    },
    // ===== DESERIALIZE =====
    [](const YAML::Node& node, EntityRegistry& scene, EntityID entity, AssetRegistry&) {
        auto& comp = scene.get_or_emplace<TextComponent>(entity);

        BOOM_INFO("========================================");
        BOOM_INFO("[DEBUG] Deserializing TextComponent");
        BOOM_INFO("========================================");

        if (node.IsMap()) {
            xproperty::settings::context ctx;
            if (auto* pObj = xproperty::getObject(comp)) {
                DeserializeObjectFromYAML(node, pObj, (void*)&comp, ctx);

                BOOM_INFO("[DEBUG] After deserialize:");
                BOOM_INFO("  text: '{}'", comp.text);
                BOOM_INFO("  fontName: '{}'", comp.fontName);
            }
        }
    }
);
```

### Test with Logging:

1. Rebuild
2. Run Editor
3. Create TextComponent entity, edit properties
4. **Save scene (Ctrl+S)**
5. **Check console logs** - you should see:
   ```
   ========================================
   [DEBUG] Serializing TextComponent:
     text: 'Your Text Here'
     fontName: 'Roboto-Regular'
     ...
   ========================================
   ```

**If you DON'T see this log**:
→ Serialization function isn't being called at all
→ Check that entity actually has TextComponent (use `selected.Has<TextComponent>()` check)

**If you DO see the log but values are wrong**:
→ Inspector isn't modifying the actual component
→ Check Step 3 again (verify reference vs copy)

---

## Step 5: Check xproperty Recognition

Test if xproperty can see TextComponent:

### Add to main.cpp or Editor startup:

```cpp
#include "ECS/ECS.hpp"

// In main() or Editor::Init():
Boom::TextComponent testComp;
auto* pObj = xproperty::getObject(testComp);
if (pObj) {
    BOOM_INFO("[XPROPERTY TEST] TextComponent recognized!");
    BOOM_INFO("[XPROPERTY TEST] Members count: {}", pObj->m_Members.size());
    for (const auto& member : pObj->m_Members) {
        BOOM_INFO("[XPROPERTY TEST]   - {}", member.m_pName);
    }
} else {
    BOOM_ERROR("[XPROPERTY TEST] TextComponent NOT RECOGNIZED by xproperty!");
    BOOM_ERROR("[XPROPERTY TEST] Check XPROPERTY_DEF macro in ECS.hpp");
}
```

**Expected output:**
```
[XPROPERTY TEST] TextComponent recognized!
[XPROPERTY TEST] Members count: 6
[XPROPERTY TEST]   - text
[XPROPERTY TEST]   - fontName
[XPROPERTY TEST]   - color
[XPROPERTY TEST]   - scale
[XPROPERTY TEST]   - screenPosition
[XPROPERTY TEST]   - renderAs3D
```

**If members count is 0**:
→ XPROPERTY_DEF macro is malformed
→ Check ECS.hpp for syntax errors (missing commas, wrong brackets)

---

## Step 6: Common Issues Checklist

### ✅ Rebuild Solution?
- [ ] Did full Rebuild (not just Build)
- [ ] No compile errors
- [ ] Editor is using newly built DLL

### ✅ Component Registration?
- [ ] `RegisterPropertyComponent<TextComponent>("TextComponent");` exists in ComponentSerializer.cpp
- [ ] Line is uncommented
- [ ] Located in `RegisterAllComponentSerializers()` function

### ✅ XPROPERTY_DEF Correct?
- [ ] All property names in quotes match member variable names exactly
- [ ] No typos (e.g., "screenPosition" not "ScreenPosition")
- [ ] All commas present
- [ ] Closing parenthesis present

### ✅ Inspector Using Reference?
- [ ] `auto& textComp = selected.Get<TextComponent>();` has `&`
- [ ] Changes reflect immediately in viewport
- [ ] Changes persist when re-selecting entity

### ✅ Save Triggered?
- [ ] Actually pressed Ctrl+S or File → Save
- [ ] No save errors in console
- [ ] YAML file timestamp updated

---

## Step 7: Nuclear Option - Clean Rebuild

If all else fails:

1. Close Visual Studio
2. Delete these folders:
   ```
   Gam300/x64/
   Gam300/.vs/
   Gam300/Editor/x64/
   Gam300/Engine/BoomEngine/x64/
   ```
3. Reopen solution
4. **Rebuild Solution**
5. Test again

---

## Quick Diagnostic Script

Run this in the Editor's debug console or add to a test button:

```cpp
void DiagnoseTextComponent(entt::entity entity, entt::registry& reg) {
    if (!reg.valid(entity)) {
        BOOM_ERROR("[DIAG] Invalid entity!");
        return;
    }

    if (!reg.all_of<TextComponent>(entity)) {
        BOOM_ERROR("[DIAG] Entity has no TextComponent!");
        return;
    }

    auto& tc = reg.get<TextComponent>(entity);

    BOOM_INFO("========== TextComponent Diagnostic ==========");
    BOOM_INFO("Entity ID: {}", (uint32_t)entity);
    BOOM_INFO("text: '{}'", tc.text);
    BOOM_INFO("fontName: '{}'", tc.fontName);
    BOOM_INFO("color: ({:.2f}, {:.2f}, {:.2f}, {:.2f})",
              tc.color.r, tc.color.g, tc.color.b, tc.color.a);
    BOOM_INFO("scale: {:.2f}", tc.scale);
    BOOM_INFO("screenPosition: ({:.1f}, {:.1f})",
              tc.screenPosition.x, tc.screenPosition.y);
    BOOM_INFO("renderAs3D: {}", tc.renderAs3D);
    BOOM_INFO("==============================================");
}
```

---

## Most Likely Cause

**90% of the time**, the issue is:

1. **Forgot to rebuild** after adding serialization registration
2. **Inspector uses copy instead of reference** (`auto` vs `auto&`)
3. **XPROPERTY_DEF has typo** in property name

**Check these three things first!**

---

## Report Back

After going through these steps, report:

1. Which step failed?
2. What did you see in the YAML file (Scenario A, B, or C)?
3. Did the debug logs appear?
4. What was the xproperty members count?

This will help pinpoint the exact issue!
