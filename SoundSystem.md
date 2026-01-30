# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Initial Setup (First Time)
```bash
# Run from Gam300/ directory - installs CMake, Conan, configures dependencies
setup.bat
```

### Building
Open `Gam300.sln` in Visual Studio 2022 and build. Build order:
1. **BoomEngine** (C++ shared library)
2. **GameScripts** (C# DLL via MSBuild)
3. **Editor** or **Runtime** (depends on BoomEngine)

Configuration: x64 Debug or Release

### Conan Dependency Install (Manual)
```bash
# Release
conan install . -of conanbuild\Release -pr:h profiles\msvc17 -pr:b profiles\msvc17 -s build_type=Release -g MSBuildDeps --build=missing -o glfw/*:shared=True -o glew/*:shared=True

# Debug
conan install . -of conanbuild\Debug -pr:h profiles\msvc17 -pr:b profiles\msvc17 -s build_type=Debug -g MSBuildDeps --build=missing -o glfw/*:shared=True -o glew/*:shared=True
```

## Architecture Overview

### Project Structure
- **Engine/BoomEngine/** - Core C++ engine (shared library)
- **Editor/** - ImGui-based editor application
- **Runtime/** - Standalone game runtime (no editor UI)
- **GameScripts/** - C# game logic scripts (compiled to DLL, loaded via Mono)

### Engine Systems (BoomEngine)

| System | Location | Technology |
|--------|----------|------------|
| ECS | `includes/ECS/ECS.hpp` | EnTT |
| Graphics | `includes/Graphics/` | OpenGL 4.5, PBR shaders |
| Physics | `includes/Physics/` | PhysX 4.1 |
| Audio | `includes/Audio/` | FMOD Studio |
| Scripting | `includes/Scripting/` | Mono/.NET |
| AI/Navigation | `includes/AI/` | Recast/Detour |
| Serialization | `includes/Auxiliaries/` | YAML-cpp |

### Entry Points
- **Editor**: `Editor/src/main.cpp` → `app->RunContext(true)` (with ImGui)
- **Runtime**: `Runtime/src/main.cpp` → `app->RunContext(false)` (no editor)

### C# Scripting
Scripts in `GameScripts/` are compiled to DLL and loaded by Mono runtime. Key files:
- `API.cs` - C++ engine bindings (internal calls)
- `Entry.cs` - Game entry point and state machine
- Script lifecycle: Init → Update → Destroy

### Key Dependencies
- **Graphics**: OpenGL, GLFW 3.3.8, GLEW 2.2.0, GLM 1.0.1
- **Physics**: PhysX 4.1.1
- **Audio**: FMOD Studio API (external, in `../FMOD Studio API Windows/`)
- **Scripting**: Mono runtime (external, in `../mono/`)
- **Compression**: Compressonator (external, in `../Compressonator/`)

### Scene Format
Scenes are YAML files in `Editor/Scenes/`. Entity components are serialized via the property system in `includes/Auxiliaries/PropertyAPI.h`.

### Shaders
GLSL shaders in `Editor/Resources/Shaders/`:
- `pbr.glsl` - Main physically based rendering
- `shadow.glsl` - Shadow mapping
- `bloom.glsl` - Post-processing bloom
- `final.glsl` - Tone mapping and final composite
- `picking.glsl` - Object selection (editor)

### Editor Panels
Editor panels in `Editor/src/Panels/` - each panel inherits from panel interface and handles its own ImGui rendering.

## Audio System

The audio system uses FMOD and consists of two layers:

### SoundEngine (`includes/Audio/Audio.hpp`, `Audio.cpp`)
Low-level singleton wrapper around FMOD providing:

**Core Features:**
- `Init()` / `Shutdown()` - FMOD system lifecycle
- `Update()` - Must be called every frame for FMOD processing and channel cleanup

**2D Playback:**
- `PlaySound(name, filePath, loop)` - Play non-positional audio
- `PlaySound(name, filePath, loop, groupName)` - Play to specific channel group

**3D Spatial Audio:**
- `PlaySoundAt(name, filePath, position, loop)` - Play at world position with distance attenuation
- `SetSoundPosition(name, position)` - Update position of moving sound sources
- `SetListenerAttributes(pos, vel, forward, up)` - Set listener (camera) for 3D calculations
- `Set3DMinMaxDistance(name, min, max)` - Configure attenuation range

**3D Audio Model:**
- Uses linear rolloff: full volume within `minDistance`, silent at `maxDistance`
- Looped sounds in `PlaySoundAt()` play as 2D (always audible)
- One-shot sounds play as 3D with positional attenuation
- 3D sounds stored with `"|3D"` suffix in sound cache to separate from 2D variants

**Channel Groups:**
- `Master` - Root group, all audio routes through
- `Music` - Background music and looping ambient (looped sounds auto-route here)
- `SFX` - One-shot effects (non-looped sounds auto-route here)
- `CreateChannelGroup()` / `RemoveChannelGroup()` - Custom groups
- `SetGroupVolume()` / `GetGroupVolume()` - Group-level volume control

**Unity-Style Properties:**
- `SetPitch(name, pitch)` - Playback speed (0.5 = half, 1.0 = normal, 2.0 = double)
- `SetPan(name, pan)` - Stereo position (-1.0 left, 0.0 center, 1.0 right)
- `SetPriority(name, priority)` - Voice stealing priority (0 = highest, 256 = lowest)
- `SetMute(name, mute)` - Mute without changing volume
- `SetSpatialBlend(name, blend)` - 2D/3D mix (0.0 = 2D, 1.0 = 3D)

**Preloading:**
- `PreloadSound(name, filePath, stream, loop)` - Load into memory (reference counted)
- `UnloadSound(name)` - Decrement ref count, release when zero

**Debugging:**
- `SetDebug3D(true)` - Enable verbose logging of listener/channel positions and distances

### SoundSystem (`includes/Audio/SoundSystem.hpp`, `src/SoundSystem.cpp`)
ECS integration that processes `SoundComponent` on entities:

**How It Works:**
1. Iterates all entities with `TransformComponent` + `SoundComponent`
2. For each `SoundComponent.entries[]`:
   - If `playOnStart` and not active → preload and play at entity position
   - If `animTrigger` matches an animator trigger → play one-shot (with random file selection from `filePaths[]`)
   - Updates 3D position each frame for moving entities
   - Applies all audio properties in real-time (volume, pitch, pan, etc.)

**Instance Naming:**
- Format: `ent_{entityID}_{entryIndex}_{entryName}`
- Animation-triggered sounds append `_play_{timestamp}` for uniqueness

**Cleanup:**
- Removes instances when corresponding `SoundComponent` entry is deleted
- Cleans up sounds for destroyed entities
- Transient one-shot sounds kept for 10s grace period before cleanup

### TrackLibrary (`includes/Audio/TrackLibrary.h`)
Static list of built-in audio tracks for the editor's audio panel dropdown.
