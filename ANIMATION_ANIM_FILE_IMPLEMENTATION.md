# Animation .anim File System Implementation Plan

**Project:** GAM300
**Feature:** Editable Animation Clips with .anim File Format
**Status:** Planning Phase
**Last Updated:** 2026-01-09

---

## Table of Contents
1. [Overview](#overview)
2. [Current System Analysis](#current-system-analysis)
3. [Problem Statement](#problem-statement)
4. [Proposed Solution](#proposed-solution)
5. [Implementation Steps](#implementation-steps)
6. [Workflow Examples](#workflow-examples)
7. [Testing Plan](#testing-plan)
8. [Future Enhancements](#future-enhancements)

---

## Overview

This document outlines the implementation of a Unity-style `.anim` file format to allow artists and designers to create and edit animation clips directly in the engine, independent of the original `.fbx` model files.

### Goals
- ✅ Save edited animation clips to disk
- ✅ Create new animations from scratch in the timeline editor
- ✅ Mix `.fbx` and `.anim` clips in the same animator
- ✅ Non-destructive editing (preserve original `.fbx` files)
- ✅ Version control friendly (YAML format)
- ✅ Reuse animations across different models (retargeting)

### Non-Goals (Future Work)
- ❌ Animation retargeting/remapping (defer to Phase 2)
- ❌ Animation compression (use existing keyframe data)
- ❌ Binary format optimization (YAML is readable and good enough)

---

## Current System Analysis

### How It Works Now

**File Structure:**
```
Gam300/Engine/BoomEngine/includes/
├── Graphics/Models/
│   ├── Animation.h          # KeyFrame, AnimationClip structures
│   └── Animator.h           # Animator class with clip management
├── Auxiliaries/
│   └── Assets.h             # Asset types and registry
└── ECS/
    └── ECS.hpp              # AnimatorComponent

Gam300/Engine/BoomEngine/src/Auxiliaries/
├── AssetSerializer.cpp      # Model/Texture loading
└── ComponentSerializer.cpp  # Scene serialization (lines 87-249)
```

**Current Workflow:**
1. Import `Player.fbx` → Loaded as `ModelAsset` with embedded animations
2. Add `AnimatorComponent` → Clones animator from `SkeletalModel`
3. Edit keyframes in timeline → Changes stored in memory only
4. Save scene → Only saves `.fbx` file path, NOT keyframe edits
5. Reload scene → Re-imports `.fbx`, **losing all edits**

**Key Code Locations:**
- **AnimationClip Structure**: `Graphics/Models/Animation.h:63-79`
- **Animator::AddKeyframe()**: `Graphics/Models/Animator.h:323-337`
- **Animator Serialization**: `ComponentSerializer.cpp:87-249`
- **Timeline Recording**: `AnimationTimelinePanel.cpp:2281-2339` (CaptureCurrentBoneTransform)

---

## Problem Statement

### Current Issues

1. **Keyframe Edits Are Not Saved**
   ```cpp
   // ComponentSerializer.cpp:105 - Only saves file path!
   e << YAML::Key << "filePath" << YAML::Value << clip->filePath;
   // ❌ Doesn't save actual keyframe data
   ```

2. **Can't Create New Animations**
   - Timeline editor can record keyframes
   - But no way to save them as a new clip

3. **Can't Iterate on Animations**
   - Artists must re-export from Blender/Maya for every change
   - No in-engine animation prototyping

4. **No Animation Reuse**
   - Can't share animations between different models
   - Each model must have its own embedded animations

---

## Proposed Solution

### .anim File Format (YAML)

**File Location:**
```
Gam300/Editor/Resources/Animations/
├── player_walk_custom.anim
├── enemy_idle.anim
└── special_attack.anim
```

**File Structure:**
```yaml
# player_walk_custom.anim
AnimationClip:
  name: "Walk_Custom"
  duration: 2.5
  ticksPerSecond: 24.0
  filePath: "Resources/Animations/player_walk_custom.anim"

  tracks:
    - boneName: "Hips"
      keyframes:
        - time: 0.0
          position: [0.0, 1.0, 0.0]
          rotation: [0.0, 0.0, 0.0, 1.0]  # xyzw quaternion
          scale: [1.0, 1.0, 1.0]
        - time: 0.5
          position: [0.1, 1.05, 0.0]
          rotation: [0.0, 0.707, 0.0, 0.707]
          scale: [1.0, 1.0, 1.0]

    - boneName: "Spine"
      keyframes:
        - time: 0.0
          position: [0.0, 0.5, 0.0]
          rotation: [0.0, 0.0, 0.0, 1.0]
          scale: [1.0, 1.0, 1.0]
```

### Integration with Existing System

**Scene YAML (No Breaking Changes):**
```yaml
Entity_Player:
  ModelComponent:
    modelID: "player_model"  # Player.fbx (skeleton + base animations)

  AnimatorComponent:
    Clips:
      # ✅ .fbx clips still work (backward compatible)
      - name: "Idle"
        filePath: "Resources/Models/Player.fbx"

      # ✅ .anim clips work alongside .fbx
      - name: "Walk_Custom"
        filePath: "Resources/Animations/player_walk_custom.anim"

      - name: "SpecialAttack"
        filePath: "Resources/Animations/special_attack.anim"

    States:
      # ... state machine data ...
```

**Key Insight:**
> Your system already supports loading clips from external files via `LoadAnimationFromFile()` (ComponentSerializer.cpp:188). We just need to teach it to recognize `.anim` files!

---

## Implementation Steps

### Phase 1: Core Infrastructure (2-3 hours)

#### Step 1.1: Add AnimationAsset Type

**File:** `Gam300/Engine/BoomEngine/includes/Auxiliaries/Assets.h`

```cpp
// Line 17: Add to AssetType enum
enum class AssetType : uint8_t {
    UNKNOWN = 0u,
    MATERIAL,
    TEXTURE,
    SKYBOX,
    SCRIPT,
    SCENE,
    MODEL,
    PHYSICS_MESH,
    PREFAB,
    AUDIO,
    ANIMATION,  // ⭐ ADD THIS
};

// Line 29: Add to TYPE_NAMES
constexpr char const* TYPE_NAMES[] {
    "All",
    "Materials",
    "Textures",
    "Skybox",
    "Scripts",
    "Scenes",
    "Models(.fbx)",
    "Physics Meshes (.pxm)",
    "Prefabs",
    "Audio",
    "Animations(.anim)",  // ⭐ ADD THIS
};

// Line 158: Add AnimationAsset struct (after AudioAsset)
struct AnimationAsset : Asset {
    std::shared_ptr<AnimationClip> data;  // Runtime animation data

    AnimationAsset() { type = AssetType::ANIMATION; }

    XPROPERTY_DEF(
        "AnimationAsset", AnimationAsset,
        obj_member<"Data", &AnimationAsset::data>
    )
};

// Line 177: Register in AssetRegistry constructor
BOOM_INLINE AssetRegistry() {
    AddEmpty<MaterialAsset>();
    AddEmpty<TextureAsset>();
    AddEmpty<SkyboxAsset>();
    AddEmpty<ModelAsset>();
    AddEmpty<PrefabAsset>();
    AddEmpty<ScriptAsset>();
    AddEmpty<SceneAsset>();
    AddEmpty<PhysicsMeshAsset>();
    AddEmpty<AudioAsset>();
    AddEmpty<AnimationAsset>();  // ⭐ ADD THIS
}
```

---

#### Step 1.2: Implement .anim File I/O

**Create New File:** `Gam300/Engine/BoomEngine/src/Auxiliaries/AnimationIO.cpp`

```cpp
#include "Graphics/Models/Animation.h"
#include "common/Core.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Boom {

// ========== SAVE ANIMATION TO .anim FILE ==========
bool SaveAnimationClip(const AnimationClip& clip, const std::string& filepath) {
    try {
        YAML::Emitter out;

        out << YAML::BeginMap;
        out << YAML::Key << "AnimationClip" << YAML::Value << YAML::BeginMap;

        // Metadata
        out << YAML::Key << "name" << YAML::Value << clip.name;
        out << YAML::Key << "duration" << YAML::Value << clip.duration;
        out << YAML::Key << "ticksPerSecond" << YAML::Value << clip.ticksPerSecond;
        out << YAML::Key << "filePath" << YAML::Value << filepath;

        // Bone tracks
        out << YAML::Key << "tracks" << YAML::Value << YAML::BeginSeq;
        for (const auto& [boneName, keyframes] : clip.tracks) {
            if (keyframes.empty()) continue;  // Skip empty tracks

            out << YAML::BeginMap;
            out << YAML::Key << "boneName" << YAML::Value << boneName;

            out << YAML::Key << "keyframes" << YAML::Value << YAML::BeginSeq;
            for (const auto& kf : keyframes) {
                out << YAML::BeginMap;
                out << YAML::Key << "time" << YAML::Value << kf.timeStamp;

                // Position (flow style for compactness)
                out << YAML::Key << "position" << YAML::Value << YAML::Flow;
                out << YAML::BeginSeq << kf.position.x << kf.position.y << kf.position.z << YAML::EndSeq;

                // Rotation (xyzw quaternion)
                out << YAML::Key << "rotation" << YAML::Value << YAML::Flow;
                out << YAML::BeginSeq << kf.rotation.x << kf.rotation.y << kf.rotation.z << kf.rotation.w << YAML::EndSeq;

                // Scale
                out << YAML::Key << "scale" << YAML::Value << YAML::Flow;
                out << YAML::BeginSeq << kf.scale.x << kf.scale.y << kf.scale.z << YAML::EndSeq;

                out << YAML::EndMap;
            }
            out << YAML::EndSeq;  // end keyframes

            out << YAML::EndMap;  // end track
        }
        out << YAML::EndSeq;  // end tracks

        out << YAML::EndMap;  // end AnimationClip
        out << YAML::EndMap;  // end root

        // Write to file
        std::ofstream fout(filepath);
        if (!fout.is_open()) {
            BOOM_ERROR("[AnimationIO] Failed to open file for writing: {}", filepath);
            return false;
        }

        fout << out.c_str();
        fout.close();

        BOOM_INFO("[AnimationIO] Saved animation clip '{}' to {}", clip.name, filepath);
        return true;
    }
    catch (const std::exception& e) {
        BOOM_ERROR("[AnimationIO] Exception while saving: {}", e.what());
        return false;
    }
}

// ========== LOAD ANIMATION FROM .anim FILE ==========
std::shared_ptr<AnimationClip> LoadAnimationClip(const std::string& filepath) {
    try {
        YAML::Node root = YAML::LoadFile(filepath);

        if (!root["AnimationClip"]) {
            BOOM_ERROR("[AnimationIO] Invalid .anim file (missing AnimationClip): {}", filepath);
            return nullptr;
        }

        YAML::Node data = root["AnimationClip"];

        auto clip = std::make_shared<AnimationClip>();
        clip->name = data["name"].as<std::string>("Unnamed");
        clip->duration = data["duration"].as<float>(0.0f);
        clip->ticksPerSecond = data["ticksPerSecond"].as<float>(24.0f);
        clip->filePath = filepath;

        // Load tracks
        if (data["tracks"]) {
            for (const auto& trackNode : data["tracks"]) {
                std::string boneName = trackNode["boneName"].as<std::string>("");
                if (boneName.empty()) continue;

                std::vector<KeyFrame> keyframes;

                if (trackNode["keyframes"]) {
                    for (const auto& kfNode : trackNode["keyframes"]) {
                        KeyFrame kf;
                        kf.timeStamp = kfNode["time"].as<float>(0.0f);

                        // Position
                        auto pos = kfNode["position"];
                        kf.position = glm::vec3(
                            pos[0].as<float>(0.0f),
                            pos[1].as<float>(0.0f),
                            pos[2].as<float>(0.0f)
                        );

                        // Rotation (xyzw)
                        auto rot = kfNode["rotation"];
                        kf.rotation = glm::quat(
                            rot[3].as<float>(1.0f),  // w
                            rot[0].as<float>(0.0f),  // x
                            rot[1].as<float>(0.0f),  // y
                            rot[2].as<float>(0.0f)   // z
                        );

                        // Scale
                        auto scl = kfNode["scale"];
                        kf.scale = glm::vec3(
                            scl[0].as<float>(1.0f),
                            scl[1].as<float>(1.0f),
                            scl[2].as<float>(1.0f)
                        );

                        keyframes.push_back(kf);
                    }
                }

                clip->tracks[boneName] = keyframes;
            }
        }

        BOOM_INFO("[AnimationIO] Loaded animation clip '{}' from {} ({} tracks)",
                  clip->name, filepath, clip->tracks.size());
        return clip;
    }
    catch (const std::exception& e) {
        BOOM_ERROR("[AnimationIO] Exception while loading: {}", e.what());
        return nullptr;
    }
}

}  // namespace Boom
```

**Create Header:** `Gam300/Engine/BoomEngine/includes/Graphics/Models/AnimationIO.h`

```cpp
#pragma once
#include "Animation.h"
#include <memory>
#include <string>

namespace Boom {
    /**
     * @brief Save an AnimationClip to a .anim file (YAML format)
     * @param clip The clip to save
     * @param filepath Path to .anim file (e.g., "Resources/Animations/walk.anim")
     * @return true if successful
     */
    bool SaveAnimationClip(const AnimationClip& clip, const std::string& filepath);

    /**
     * @brief Load an AnimationClip from a .anim file
     * @param filepath Path to .anim file
     * @return Loaded clip or nullptr on failure
     */
    std::shared_ptr<AnimationClip> LoadAnimationClip(const std::string& filepath);
}
```

---

#### Step 1.3: Update Animator to Support .anim Files

**File:** `Gam300/Engine/BoomEngine/includes/Graphics/Models/Animator.h`

Find the `LoadAnimationFromFile()` method and update it:

```cpp
// Around line 150-180 (wherever LoadAnimationFromFile is defined)
BOOM_INLINE void LoadAnimationFromFile(const std::string& filePath, const std::string& name = "") {
    std::shared_ptr<AnimationClip> clip;

    // Check file extension
    if (filePath.ends_with(".anim")) {
        // ⭐ NEW: Load from .anim file
        clip = LoadAnimationClip(filePath);
        if (clip && !name.empty()) {
            clip->name = name;  // Override name if specified
        }
    }
    else if (filePath.ends_with(".fbx") || filePath.ends_with(".gltf")) {
        // ✅ EXISTING: Load from model file
        // Your existing FBX/GLTF loading code here...
        // (Keep this code as-is)
    }
    else {
        BOOM_WARN("[Animator] Unsupported file format: {}", filePath);
        return;
    }

    if (clip) {
        AddClip(clip);
        BOOM_INFO("[Animator] Loaded clip '{}' from {}", clip->name, filePath);
    }
}
```

**Don't forget to include the header:**
```cpp
#include "Graphics/Models/AnimationIO.h"
```

---

#### Step 1.4: Add .anim Asset Loading to AssetRegistry

**File:** `Gam300/Engine/BoomEngine/src/Auxiliaries/AssetSerializer.cpp`

Add a loader for `.anim` files (similar to how `.fbx` models are loaded):

```cpp
// Add this function to AssetSerializer.cpp
void LoadAnimationAsset(const std::string& filepath, AssetRegistry& registry) {
    auto clip = Boom::LoadAnimationClip(filepath);
    if (!clip) {
        BOOM_ERROR("[AssetLoader] Failed to load animation: {}", filepath);
        return;
    }

    auto asset = std::make_shared<AnimationAsset>();
    asset->name = clip->name;
    asset->source = filepath;
    asset->data = clip;

    AssetID id = std::hash<std::string>{}(filepath);
    registry.Add(id, asset);

    BOOM_INFO("[AssetLoader] Loaded AnimationAsset: {} (ID: {})", asset->name, id);
}

// In your asset loading loop (wherever you scan for assets):
// Add this alongside .fbx, .png, etc.
if (extension == ".anim") {
    LoadAnimationAsset(filepath, registry);
}
```

---

### Phase 2: Timeline Editor Integration (1-2 hours)

#### Step 2.1: Add "Save Clip" Button to AnimationTimelinePanel

**File:** `Gam300/Editor/src/Panels/AnimationTimelinePanel.cpp`

Find the clip management buttons (around line 888-950) and add a "Save" button:

```cpp
// After the "Duplicate" button (around line 932)
ImGui::SameLine();
ImGui::BeginDisabled(m_SelectedClipIndex < 0);
if (ImGui::Button("Save")) {
    ImGui::OpenPopup("SaveClipPopup");
}
if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Save clip to .anim file");
}
ImGui::EndDisabled();

// Add the popup for save dialog
if (ImGui::BeginPopup("SaveClipPopup")) {
    if (m_Animator && m_SelectedClipIndex >= 0) {
        auto* clip = m_Animator->GetClipMutable(m_SelectedClipIndex);
        if (clip) {
            static char saveNameBuffer[128] = "";
            static bool initBuffer = false;

            if (!initBuffer || ImGui::IsWindowAppearing()) {
                // Pre-fill with clip name
                strncpy(saveNameBuffer, clip->name.c_str(), 127);
                saveNameBuffer[127] = '\0';
                initBuffer = true;
            }

            ImGui::Text("Save Animation Clip");
            ImGui::Separator();

            ImGui::Text("Clip Name:");
            ImGui::InputText("##SaveName", saveNameBuffer, 128);

            ImGui::Spacing();

            if (ImGui::Button("Save", ImVec2(120, 0))) {
                std::string clipName = saveNameBuffer;
                if (!clipName.empty()) {
                    // Sanitize filename (replace spaces with underscores)
                    std::string filename = clipName;
                    std::replace(filename.begin(), filename.end(), ' ', '_');

                    std::string filepath = "Resources/Animations/" + filename + ".anim";

                    if (Boom::SaveAnimationClip(*clip, filepath)) {
                        clip->name = clipName;
                        clip->filePath = filepath;
                        BOOM_INFO("[AnimTimeline] Saved clip '{}' to {}", clipName, filepath);
                    } else {
                        BOOM_ERROR("[AnimTimeline] Failed to save clip to {}", filepath);
                    }

                    initBuffer = false;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                initBuffer = false;
                ImGui::CloseCurrentPopup();
            }
        }
    }
    ImGui::EndPopup();
}
```

**Don't forget to include the header:**
```cpp
#include "Graphics/Models/AnimationIO.h"
```

---

#### Step 2.2: Add Visual Indicator for Modified Clips

Add a "dirty" flag to track unsaved changes:

```cpp
// In AnimationTimelinePanel.h (around line 142)
bool m_ClipModified = false;  // ⭐ ADD THIS

// In AnimationTimelinePanel.cpp, after any keyframe edit operation:
// (e.g., after AddKeyframe, RemoveKeyframe, UpdateKeyframeTime)
m_ClipModified = true;

// In the UI (around line 863 where clip name is displayed):
std::string clipDisplay = m_Animator->GetClip(m_SelectedClipIndex)->name;
if (m_ClipModified) {
    clipDisplay += " *";  // Show asterisk for unsaved changes
}

if (ImGui::BeginCombo("##AnimClip", clipDisplay.c_str())) {
    // ... existing code ...
}

// Reset flag after save:
// In the save button handler:
if (Boom::SaveAnimationClip(*clip, filepath)) {
    // ... existing code ...
    m_ClipModified = false;  // ⭐ RESET FLAG
}
```

---

### Phase 3: Inspector Integration (1 hour)

#### Step 3.1: Add Clip Management to AnimatorComponent Inspector

**File:** `Gam300/Editor/src/Panels/Inspector/InspectorPanel.cpp`

Find where AnimatorComponent is rendered (search for "AnimatorComponent") and add:

```cpp
// Add after rendering other AnimatorComponent properties
if (entity.Has<AnimatorComponent>()) {
    auto& animComp = entity.Get<AnimatorComponent>();
    auto& animator = animComp.animator;

    if (!animator) return;

    ImGui::Separator();
    ImGui::Text("Animation Clips (%zu):", animator->GetClipCount());

    // List all clips
    for (size_t i = 0; i < animator->GetClipCount(); ++i) {
        const auto* clip = animator->GetClip(i);
        if (!clip) continue;

        // Extract filename from path
        std::string filename = clip->filePath;
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }

        // Color code by source
        if (clip->filePath.ends_with(".anim")) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[.anim]");
        } else if (clip->filePath.ends_with(".fbx")) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f), "[.fbx]");
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[other]");
        }

        ImGui::SameLine();
        ImGui::Text("%s", clip->name.c_str());

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Source: %s", clip->filePath.c_str());
            ImGui::Text("Duration: %.2fs", clip->duration);
            ImGui::Text("Tracks: %zu", clip->tracks.size());
            ImGui::EndTooltip();
        }
    }

    ImGui::Spacing();

    // Add clip button
    if (ImGui::Button("Add Animation Clip", ImVec2(-1, 0))) {
        ImGui::OpenPopup("AddAnimClipPopup");
    }

    if (ImGui::BeginPopup("AddAnimClipPopup")) {
        ImGui::Text("Add Animation Clip");
        ImGui::Separator();

        // Show all .anim assets
        if (m_Ctx && m_Ctx->assets) {
            auto& animAssets = m_Ctx->assets->GetMap<AnimationAsset>();

            bool hasAssets = false;
            for (auto& [id, asset] : animAssets) {
                if (id == EMPTY_ASSET) continue;
                hasAssets = true;

                auto* animAsset = static_cast<AnimationAsset*>(asset.get());
                if (ImGui::Selectable(animAsset->name.c_str())) {
                    if (animAsset->data) {
                        animator->AddClip(animAsset->data);
                        BOOM_INFO("[Inspector] Added clip '{}' to animator", animAsset->name);
                    }
                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Source: %s", animAsset->source.c_str());
                    if (animAsset->data) {
                        ImGui::Text("Duration: %.2fs", animAsset->data->duration);
                        ImGui::Text("Tracks: %zu", animAsset->data->tracks.size());
                    }
                    ImGui::EndTooltip();
                }
            }

            if (!hasAssets) {
                ImGui::TextDisabled("No .anim assets found");
                ImGui::TextDisabled("Create them in the Animation Timeline editor");
            }
        }

        ImGui::EndPopup();
    }
}
```

---

### Phase 4: Testing & Validation (30 mins)

#### Test Cases

**Test 1: Save Edited .fbx Animation**
1. Load entity with `Player.fbx` model
2. Open Animation Timeline
3. Select "Walk" clip from model
4. Edit keyframes (move bone, record with K key)
5. Click "Save" → save as `walk_custom.anim`
6. Verify file exists in `Resources/Animations/`
7. Reload scene → should use edited version

**Test 2: Create Animation from Scratch**
1. Load entity with skeletal model (no animations)
2. Open Animation Timeline
3. Click "New Clip" → create empty clip
4. Select bones and record keyframes
5. Click "Save" → save as `custom_idle.anim`
6. Verify file exists
7. Reload scene → animation should persist

**Test 3: Mix .fbx and .anim Clips**
1. Entity has `Player.fbx` (Idle, Walk, Run)
2. Add custom clip via Inspector → "Dance.anim"
3. Verify all 4 clips appear in Animator
4. Switch between clips in Timeline
5. Save scene → verify YAML has both .fbx and .anim paths

**Test 4: .anim File Format Validation**
1. Create animation with 2 bones, 3 keyframes each
2. Save to .anim file
3. Open file in text editor → verify YAML structure
4. Manually edit a keyframe value
5. Reload in engine → should reflect changes

---

## Workflow Examples

### Example 1: Artist Iterating on Walk Cycle

**Before (Current System):**
```
1. Artist edits in Blender
2. Export Player.fbx
3. Copy to Unity/Engine
4. Test in game
5. See issue → GOTO 1 (re-export entire model)
   ↑ Time wasted: 5-10 minutes per iteration
```

**After (.anim System):**
```
1. Open Animation Timeline in engine
2. Edit keyframes with gizmo
3. Click "Save"
4. Test immediately in game
5. See issue → GOTO 2 (instant iteration)
   ↑ Time saved: ~90% faster iteration
```

---

### Example 2: Procedural Animation Creation

**Use Case:** Enemy death animations (ragdoll → pose transition)

```
1. Designer poses skeleton in Timeline
2. Records keyframes at 0s (ragdoll start)
3. Poses skeleton at 1s (final death pose)
4. Engine interpolates between keyframes
5. Save as "enemy_death_01.anim"
6. Reuse across all enemy types with same skeleton
```

---

### Example 3: Animation Variants

**Use Case:** Player has multiple walk variations

```
# Start with base walk from Player.fbx
Player.fbx → Walk (1.2s cycle)

# Create variants:
1. Load "Walk" in Timeline
2. Edit slightly (add limp)
3. Save as "walk_injured.anim"

4. Load "Walk" again
5. Edit differently (sneaking)
6. Save as "walk_sneak.anim"

Result:
- walk (from .fbx)
- walk_injured.anim
- walk_sneak.anim
All share same skeleton, different keyframes
```

---

## File Structure After Implementation

```
Gam300/
├── Editor/
│   └── Resources/
│       ├── Animations/              # ⭐ NEW FOLDER
│       │   ├── player_walk_custom.anim
│       │   ├── enemy_idle.anim
│       │   ├── special_attack.anim
│       │   └── death_01.anim
│       └── Models/
│           └── Player.fbx           # Original (untouched)
│
└── Engine/BoomEngine/
    ├── includes/
    │   ├── Graphics/Models/
    │   │   ├── Animation.h
    │   │   ├── AnimationIO.h        # ⭐ NEW
    │   │   └── Animator.h
    │   └── Auxiliaries/
    │       └── Assets.h              # ✏️ MODIFIED (add AnimationAsset)
    └── src/
        └── Auxiliaries/
            ├── AnimationIO.cpp       # ⭐ NEW
            ├── AssetSerializer.cpp   # ✏️ MODIFIED (add .anim loader)
            └── ComponentSerializer.cpp # ✅ NO CHANGES NEEDED
```

---

## Future Enhancements (Phase 2+)

### Priority 1: Animation Retargeting
- Map animations between different skeletons
- Bone name remapping UI
- Proportion adjustments

### Priority 2: Curve Editor Integration
- Save curve tangents in .anim file
- Interpolation type per keyframe (linear/cubic/constant)
- Visual curve editing

### Priority 3: Animation Events
- Add events to .anim file format
- Timeline markers for events
- Callback system for gameplay

### Priority 4: Animation Compression
- Keyframe reduction algorithms
- Binary .anim format (optional)
- LOD for distant characters

### Priority 5: Animation Blending/Layers
- Save blend tree configurations
- Animation layers for additive animations
- Masking (upper body vs lower body)

---

## Migration Guide (Backward Compatibility)

### Existing Scenes Will Still Work!

**Old scene (before .anim system):**
```yaml
AnimatorComponent:
  Clips:
    - name: "Idle"
      filePath: "Resources/Models/Player.fbx"
```
✅ **Still works** → Loads clips from .fbx as before

**New scene (with .anim files):**
```yaml
AnimatorComponent:
  Clips:
    - name: "Idle"
      filePath: "Resources/Models/Player.fbx"       # Old
    - name: "Walk"
      filePath: "Resources/Animations/walk.anim"    # New
```
✅ **Backward compatible** → Both formats work side-by-side

### Manual Migration (Optional)

To convert existing .fbx animations to editable .anim:

1. Open Animation Timeline
2. Select clip from .fbx
3. Click "Save" → exports to .anim
4. Scene auto-updates to reference .anim instead of .fbx

---

## Performance Considerations

### Memory
- **Impact:** Minimal (clips already loaded in memory)
- **Change:** Just different file source (disk I/O)
- **.anim files:** ~1-5 KB per clip (YAML text)

### Loading Time
- **YAML parsing:** ~1-2ms per clip (negligible)
- **Same as .fbx:** Animation data structure is identical
- **Optimization:** Can cache parsed clips in AssetRegistry

### Runtime
- **Zero impact:** AnimationClip structure is unchanged
- **Playback:** Identical performance to .fbx clips
- **Skinning:** No difference (same keyframe interpolation)

---

## Questions & Answers

### Q: Can I still use .fbx animations?
**A:** Yes! 100% backward compatible. .fbx clips work exactly as before.

### Q: What happens if I edit a .fbx clip and don't save it?
**A:** Changes are lost on scene reload (same as current behavior). You must click "Save" to persist edits.

### Q: Can I share .anim files between projects?
**A:** Yes! As long as the skeleton bone names match, .anim files are portable.

### Q: Can I edit .anim files in a text editor?
**A:** Yes! They're YAML files. You can manually tweak values if needed.

### Q: Do I need to re-export .fbx files anymore?
**A:** Only for:
  - New models/skeletons
  - New base animations
  - Mesh/material changes

  Animation tweaks can now be done in-engine!

### Q: Can I version control .anim files?
**A:** Absolutely! YAML format is git-friendly. Diffs and merges work well.

---

## Summary Checklist

### Implementation Checklist

- [ ] **Phase 1: Core Infrastructure**
  - [ ] Add `AnimationAsset` to `Assets.h`
  - [ ] Create `AnimationIO.h` and `AnimationIO.cpp`
  - [ ] Update `Animator::LoadAnimationFromFile()`
  - [ ] Add `.anim` loader to `AssetSerializer.cpp`

- [ ] **Phase 2: Timeline Editor**
  - [ ] Add "Save Clip" button
  - [ ] Add save dialog popup
  - [ ] Add modified/dirty flag indicator
  - [ ] Test save/load roundtrip

- [ ] **Phase 3: Inspector**
  - [ ] Display clip list with source indicators
  - [ ] Add "Add Clip" button
  - [ ] Show .anim assets in popup

- [ ] **Phase 4: Testing**
  - [ ] Test edit .fbx → save .anim
  - [ ] Test create new → save .anim
  - [ ] Test mix .fbx + .anim clips
  - [ ] Test .anim file format validity
  - [ ] Test scene persistence

- [ ] **Phase 5: Documentation**
  - [ ] Update user manual
  - [ ] Create tutorial video
  - [ ] Add example .anim files

---

## Estimated Timeline

| Phase | Tasks | Time | Cumulative |
|-------|-------|------|------------|
| Phase 1 | Core Infrastructure | 2-3 hours | 2-3h |
| Phase 2 | Timeline Editor | 1-2 hours | 3-5h |
| Phase 3 | Inspector | 1 hour | 4-6h |
| Phase 4 | Testing | 30 mins | 4.5-6.5h |
| **Total** | **Full Implementation** | **~5-7 hours** | - |

**Recommendation:** Implement in order. Each phase builds on the previous.

---

## Success Metrics

After implementation, you should be able to:

✅ Edit an existing .fbx animation and save it as .anim
✅ Create a new animation from scratch and save it
✅ Load both .fbx and .anim clips in the same animator
✅ See saved .anim files persist across scene reloads
✅ Share .anim files between different entities with same skeleton
✅ Iterate on animations 10x faster than re-exporting from DCC tools

---

**Status:** Ready for Implementation
**Next Step:** Begin with Phase 1.1 (Add AnimationAsset Type)
**Questions?** Review the Q&A section or consult this document.
