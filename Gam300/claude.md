# Claude Code Session - Game Export/Runtime Debugging

## ✅ SOLUTION SUMMARY

**Problem**: Exported game (Game.exe) crashes on launch due to missing assets
**Root Cause**: `LoadScene()` was only loading scene structure, not the actual asset files (textures, models, etc.)
**Fix Applied**: Modified `LoadScene()` in Application.h to call `DeserializeAsync()` which loads all assets from assets.yaml

**Files Modified**:
1. `Engine/BoomEngine/includes/Application/Application.h` - LoadScene() now loads assets
2. `Engine/BoomEngine/src/Application/Application.cpp` - Added detailed logging
3. `Editor/src/Editor.cpp` - Export now copies assets.yaml + Mono runtime folders
4. `Engine/BoomEngine/src/Scripting/MonoRuntime.cpp` - Auto-detects shipped vs dev mode for Mono paths

**Status**: ✅ **FIXED AND WORKING!** Game exports and runs successfully.

**Latest Fix (Portability)**:
- MonoRuntime now auto-detects shipped mode (looks for `etc/` and `lib/` next to exe)
- In shipped mode: uses `exeDir/lib` and `exeDir/etc`
- In dev mode: uses `repoRoot/mono/lib` and `repoRoot/mono/etc`
- Export copies Mono runtime folders so game is truly portable

**Git Configuration**: Added `Build/` and `x64/` folders to .gitignore (build artifacts should not be committed)

---

## 🚀 RESUME HERE - Quick Start

**Current Task**: Assets not loading from assets.yaml in shipped mode

**What Was Just Done**:
- ✅ Fixed assets.yaml export - file now copies correctly
- ✅ Added comprehensive logging to Application::RunContext()
- ✅ **NEW ISSUE FOUND**: Assets.yaml exists but **assets are not being loaded from it**
  - Asset name: (empty)
  - Asset source: (empty)
  - EnvMap texture: null
- 🔍 **Root Cause**: Scene deserialization reads assets.yaml but doesn't trigger actual file loading

**The Problem**:
When `LoadScene()` is called, it deserializes the scene and assets.yaml, but the actual texture/model files are never loaded from disk. The AssetRegistry has entries with correct IDs but no data.

**What Needs To Be Fixed**:
The deserialization process needs to:
1. Read assets.yaml to get asset IDs and file paths ✅ (working)
2. **Actually load the texture/model files from those paths** ❌ (NOT happening)
3. The AsyncAssetLoader or similar mechanism needs to be triggered during scene load

**THE FIX** (ALREADY APPLIED ✅):
There are TWO deserialization functions:
- `Deserialize(scene, assets, filepath)` - Loads ONLY scene structure, not asset files
- `DeserializeAsync(assets, filepath, window)` - Actually loads textures/models from disk

**File modified**: `Engine/BoomEngine/includes/Application/Application.h`
**Function**: `LoadScene()` at line 470-488
**Change**: Added asset loading BEFORE scene loading:

```cpp
// OLD CODE (didn't load assets):
CleanupCurrentScene();
serializer.Deserialize(m_Context->scene, *m_Context->assets, sceneFilePath);

// NEW CODE (loads assets first):
CleanupCurrentScene();

// CRITICAL: Load all assets from assets.yaml BEFORE loading the scene
BOOM_INFO("[Scene] Loading assets from Resources/assets.yaml");
serializer.DeserializeAsync(*m_Context->assets, "Resources/assets.yaml", m_Context->window->Handle().get());

serializer.Deserialize(m_Context->scene, *m_Context->assets, sceneFilePath);
```

**What To Do Next** (THIS SHOULD FIX IT!):
1. **Rebuild BoomEngine** in Visual Studio (right-click BoomEngine → Build)
   - This includes the LoadScene fix that actually loads assets
2. **Rebuild Runtime** (right-click Runtime → Build)
3. **Run Editor** and export:
   - File → Export Game → Build/Game
   - Configuration: Debug
4. **Run Game.exe**
5. **Check log** - should now show:
   ```
   [Scene] Loading assets from Resources/assets.yaml
   [DataSerializer] Starting multithreaded asset loading...
   [DataSerializer] Asset loading complete: X succeeded, 0 failed
   [RunContext]   - Asset name: sky.DDS
   [RunContext]   - Asset source: Resources/Textures/Skybox/sky.DDS
   [RunContext]   - EnvMap texture valid: YES
   ```
6. **GAME SHOULD NOW RUN!**

**Expected Result**: Game should now either:
- ✅ **Work completely** - skybox loads, game runs!
- ⚠️ Show specific error message about skybox asset (if there are other issues)

---

## 🔍 Root Cause Found

**The Crash Location**: Skybox initialization in `Application::RunContext()` at line ~155

**Root Cause**: The exported game was missing `assets.yaml` - the asset registry file that maps asset IDs to file paths. Without it, the engine couldn't find the skybox asset when trying to initialize it, causing a crash.

**The Fix**:
1. **Editor/src/Editor.cpp** (line ~678): Added code to copy `Editor/AssetsProp/assets.yaml` to `Build/Game/Resources/assets.yaml` during export
2. **Application.cpp** (line ~150): Added try-catch error handling around skybox initialization with detailed logging

**Files Modified**:
- `Editor/src/Editor.cpp` - Export dialog now copies assets.yaml
- `Engine/BoomEngine/src/Application/Application.cpp` - Added comprehensive logging and error handling

---

## Current Issue: Runtime Crash During Game Launch (RESOLVED)

### Problem Summary
The exported Game.exe launches, opens a window briefly, then crashes. The crash occurs inside `Application::RunContext()` after the "Starting game loop..." log message.

**Last Known Log Output** (`Build/Game/Game_log.txt`):
```
========================================
Game Runtime Starting...
========================================
Getting exe path...
Exe path: C:\Users\amosh\Desktop\GAM300\Gam300\Build\Game
Runtime working directory: C:\Users\amosh\Desktop\GAM300\Gam300\Build\Game
Checking for required folders...
  Resources/ exists: YES
  Scenes/ exists: YES
  Scripts/ exists: YES
Creating engine instance...
Engine instance created successfully
[Event] Resize -> 1280x720
[Event] Quit requested
[Event] Frame task ran
Dispatcher smoketest finished inside MyEngineClass::whatup().
Engine whatup() called
Game Runtime Started
Initializing FMOD...
FMOD initialized successfully
Creating application...
Application created
Starting game loop...
[CRASH OCCURS HERE]
```

## Current Progress

### What We've Completed
1. ✅ Created Runtime project (standalone game executable)
2. ✅ Added Runtime to Gam300.sln with proper dependencies
3. ✅ Implemented export dialog in Editor (Editor/src/Editor.cpp)
4. ✅ Fixed all DLL copying (PhysX, FMOD, Mono, GLFW, GLEW) in Directory.Build.props
5. ✅ Implemented shipped mode vs development mode detection
6. ✅ Added extensive logging to Application::RunContext()

### What We're Currently Debugging
**File**: `Engine/BoomEngine/src/Application/Application.cpp`
**Function**: `Application::RunContext(bool showFrame)`

**Detailed Logging Added** (Lines 12-93):
```cpp
void Application::RunContext(bool showFrame)
{
    BOOM_INFO("[Application] RunContext started");

    std::cout << "[RunContext] Loading scene MainMenu..." << std::endl;
    std::cout.flush();
    LoadScene("MainMenu");
    std::cout << "[RunContext] Scene loaded successfully" << std::endl;

    const std::string exeDir = GetExeDir();
    std::cout << "[RunContext] Exe directory: " << exeDir << std::endl;

    std::filesystem::path scriptsFolder = std::filesystem::path(exeDir) / "Scripts";
    bool isShippedMode = std::filesystem::exists(scriptsFolder);
    std::cout << "[RunContext] Shipped mode: " << (isShippedMode ? "YES" : "NO") << std::endl;

    // ... path detection logic ...

    std::cout << "[RunContext] Script directory: " << asmDir << std::endl;
    std::cout << "[RunContext] Initializing scripting system..." << std::endl;

    if (!m_Context->scriptingSystem->Init(asmDir, m_Context)) {
        std::cout << "[RunContext] ERROR: Failed to initialize scripting system!" << std::endl;
    }
    else {
        std::cout << "[RunContext] Scripting system initialized" << std::endl;
        std::string dllPath = (std::filesystem::path(asmDir) / "GameScripts.dll").string();
        std::cout << "[RunContext] Loading GameScripts.dll from: " << dllPath << std::endl;

        if (!m_Context->scriptingSystem->LoadScriptsDll(dllPath)) {
            std::cout << "[RunContext] ERROR: Failed to load GameScripts.dll" << std::endl;
        }
        else {
            std::cout << "[RunContext] GameScripts.dll loaded successfully" << std::endl;
            // ... continues with game loop ...
        }
    }
}
```

## Current Status: READY TO REBUILD

### ⚠️ PCH Memory Error Workaround

The command-line MSBuild is hitting PCH (precompiled header) memory errors. **Use Visual Studio GUI for rebuilding**:

1. **Open Visual Studio 2022**
2. **Open solution**: `Gam300.sln`
3. **Important**: Close **ALL** other applications to free up RAM
4. **Clean solution**: Build → Clean Solution
5. **Close Visual Studio completely** and wait 10 seconds
6. **Reopen Visual Studio** with `Gam300.sln`
7. **Build BoomEngine**:
   - Right-click `BoomEngine` project → Build
   - Wait for it to complete (may take 2-3 minutes)
8. **Build Runtime**:
   - Right-click `Runtime` project → Build

### Immediate Next Steps After Rebuild

1. **Launch Editor** (F5 or click Run in Visual Studio on Editor project)

2. **Export game**:
   - File → Export Game
   - Export to: `Build/Game`
   - Configuration: Debug
   - Click Export

3. **Run exported game**:
   ```bash
   cd Build/Game
   ./Game.exe
   ```

4. **Check crash log**:
   ```bash
   cat Build/Game/Game_log.txt
   ```

5. **Analyze the detailed output** - it should now show exactly where in RunContext() the crash occurs

### Expected Outcome
The new `Game_log.txt` should show exactly where in `RunContext()` the crash occurs:
- If it shows **"Scene loaded successfully"** → crash is after scene loading
- If it shows **"Scripting system initialized"** → crash is during script loading
- If it shows **"GameScripts.dll loaded successfully"** → crash is in game loop setup
- If it shows **"ERROR: Failed to initialize scripting system!"** → scripting init failure
- If it shows **"ERROR: Failed to load GameScripts.dll"** → DLL loading failure

### What the Full Successful Log Should Look Like
If everything works, the log should show:
```
========================================
Game Runtime Starting...
========================================
Getting exe path...
Exe path: C:\Users\amosh\Desktop\GAM300\Gam300\Build\Game
Runtime working directory: C:\Users\amosh\Desktop\GAM300\Gam300\Build\Game
Checking for required folders...
  Resources/ exists: YES
  Scenes/ exists: YES
  Scripts/ exists: YES
Creating engine instance...
Engine instance created successfully
[Event] Resize -> 1280x720
[Event] Quit requested
[Event] Frame task ran
Dispatcher smoketest finished inside MyEngineClass::whatup().
Engine whatup() called
Game Runtime Started
Initializing FMOD...
FMOD initialized successfully
Creating application...
Application created
Starting game loop...
[RunContext] Loading scene MainMenu...
[RunContext] Scene loaded successfully
[RunContext] Exe directory: C:\Users\amosh\Desktop\GAM300\Gam300\Build\Game
[RunContext] Shipped mode: YES
[RunContext] Script directory: C:\Users\amosh\Desktop\GAM300\Gam300\Build\Game\Scripts
[RunContext] Initializing scripting system...
[RunContext] Scripting system initialized
[RunContext] Loading GameScripts.dll from: C:\Users\amosh\Desktop\GAM300\Gam300\Build\Game\Scripts\GameScripts.dll
[RunContext] GameScripts.dll loaded successfully
[Game loop continues running...]
```

**The crash happens wherever the log stops** - that's the exact operation that failed.

## Key Files Modified

### Runtime/src/main.cpp
Main entry point for exported games. Sets working directory, initializes FMOD, creates Application, runs game loop with extensive logging and error handling via message boxes.

### Runtime/Runtime.vcxproj
Project configuration with proper include paths for BoomEngine headers and Detour navigation libraries. Links against BoomEngine.lib.

### Directory.Build.props
Shared MSBuild configuration. **Critical section** for DLL copying:
```xml
<ItemDefinitionGroup Condition="'$(MSBuildProjectName)'=='Editor' OR '$(MSBuildProjectName)'=='Runtime'">
  <PreBuildEvent>
    <Command>
      REM --- Mono runtime ---
      xcopy /Y /D "$(MonoBinDir)\mono-2.0-sgen.dll" "$(OutDir)"

      REM --- FMOD runtime ---
      xcopy /Y /D "$(FMODCoreLibDir)\fmod$(FMODDebugSuffix).dll" "$(OutDir)"
      xcopy /Y /D "$(FMODStudioLibDir)\fmodstudio$(FMODDebugSuffix).dll" "$(OutDir)"

      REM --- PhysX runtime (from Conan) ---
      if exist "$(Conanphysx_physxcommonRootFolder)\bin" xcopy /Y /D "$(Conanphysx_physxcommonRootFolder)\bin\*.dll" "$(OutDir)"

      REM --- GLFW runtime (from Conan) ---
      if exist "$(ConanglfwRootFolder)\bin" xcopy /Y /D "$(ConanglfwRootFolder)\bin\*.dll" "$(OutDir)"

      REM --- GLEW runtime (from Conan) ---
      if exist "$(Conanglew_glewlibRootFolder)\bin" xcopy /Y /D "$(Conanglew_glewlibRootFolder)\bin\*.dll" "$(OutDir)"
    </Command>
  </PreBuildEvent>
</ItemDefinitionGroup>
```

### Editor/src/Editor.cpp
Export dialog implementation (lines ~320-420). Copies Runtime.exe → Game.exe, all DLLs, Resources/, Scenes/, and Scripts/GameScripts.dll to export directory.

### Engine/BoomEngine/src/Application/Application.cpp
**MOST RECENTLY MODIFIED**: Added detailed logging throughout RunContext() function to diagnose crash.

## Build Issues to Watch For

### PCH Memory Errors
**Symptom**: `error C3859: Failed to create virtual memory for PCH`
**Fix**:
1. Kill all build processes: `taskkill /F /IM MSBuild.exe /IM cl.exe /IM link.exe`
2. Delete intermediate folders: `rm -rf Engine/BoomEngine/x64 x64/Debug/BoomEngine.*`
3. Clean rebuild

### Missing DLLs in Export
**Symptom**: Game.exe crashes with "DLL not found" system error
**Fix**: Check Directory.Build.props PreBuildEvent - ensure all DLL paths are correct and Conan variables are defined

### Missing Include Paths
**Symptom**: Cannot open source file errors for Detour headers
**Fix**: Runtime.vcxproj must include:
```xml
<EngineIncludeRoots>
  $(SolutionDir)Engine\BoomEngine\includes;
  $(SolutionDir)Engine\BoomEngine\includes\common;
  $(SolutionDir)Engine\BoomEngine\Vendors\Detour\Include;
  $(SolutionDir)Engine\BoomEngine\Vendors\DetourCrowd\Include;
  $(SolutionDir)Engine\BoomEngine\Vendors\DetourTileCache\Include
</EngineIncludeRoots>
```

## Architecture Overview

### Project Structure
```
GAM300/
├── Gam300/                    # Main solution folder
│   ├── Gam300.sln            # Visual Studio solution
│   ├── Directory.Build.props # Shared build config (DLL copying)
│   ├── Engine/
│   │   └── BoomEngine/       # Engine DLL (graphics, physics, audio, scripting)
│   ├── Editor/               # Editor EXE (uses BoomEngine + ImGui)
│   │   ├── Resources/        # Game assets (textures, models, shaders)
│   │   └── Scenes/           # Scene YAML files
│   ├── Runtime/              # Standalone game player EXE (uses BoomEngine, no ImGui)
│   │   └── src/main.cpp
│   ├── GameScripts/          # C# game scripts (compiled to DLL)
│   └── Build/
│       └── Game/             # Export destination (Game.exe + DLLs + assets)
├── mono/                      # Mono runtime (C# scripting)
├── FMOD Studio API Windows/   # FMOD audio library
└── Compressonator/           # Texture compression library
```

### Shipped Mode vs Development Mode
The engine auto-detects whether it's running as an exported game or in development:

**Shipped Mode** (exported game):
- Detection: `Scripts/` folder exists next to .exe
- Script path: `<exe_dir>/Scripts/GameScripts.dll`
- Mono path: `<exe_dir>` (mono-2.0-sgen.dll is next to exe)

**Development Mode** (running from Visual Studio):
- Detection: No `Scripts/` folder next to .exe
- Script path: `GAM300/Gam300/GameScripts/bin/x64/Debug/GameScripts.dll`
- Mono path: `GAM300/mono/`

Code in Application.cpp (lines 27-60):
```cpp
std::filesystem::path scriptsFolder = std::filesystem::path(exeDir) / "Scripts";
bool isShippedMode = std::filesystem::exists(scriptsFolder);

if (isShippedMode)
{
    // SHIPPED MODE: Everything is relative to exe
    BOOM_INFO("[Application] Running in SHIPPED mode (exported game)");
    asmDir = scriptsFolder.string();
    monoBase = exeDir;
}
else
{
    // DEVELOPMENT MODE: Use repository structure
    BOOM_INFO("[Application] Running in DEVELOPMENT mode");
    std::filesystem::path repoRoot = std::filesystem::path(exeDir)
        .parent_path()  // Debug -> x64
        .parent_path()  // x64 -> Gam300
        .parent_path(); // Gam300 -> GAM300

    monoBase = (repoRoot / "mono").string();
    #if defined(_DEBUG)
        asmDir = (repoRoot / "Gam300" / "GameScripts" / "bin" / "x64" / "Debug").string();
    #else
        asmDir = (repoRoot / "Gam300" / "GameScripts" / "bin" / "x64" / "Release").string();
    #endif
}
```

## Dependencies

### For Development (Building the engine)
- Visual Studio 2022 (v143 toolset)
- Conan package manager (for PhysX, GLFW, GLEW, etc.)
- Mono runtime (sibling to Gam300/)
- FMOD Studio API (sibling to Gam300/)
- Compressonator library (sibling to Gam300/)

### For End Users (Exported game)
**Only needs**:
- Game.exe
- DLLs (all copied by export system):
  - BoomEngine.dll
  - PhysX.dll, PhysXCommon.dll, PhysXCooking.dll, PhysXFoundation.dll
  - fmodL.dll, fmodstudioL.dll (Debug) or fmod.dll, fmodstudio.dll (Release)
  - mono-2.0-sgen.dll
  - glfw3.dll
  - glew32d.dll (Debug) or glew32.dll (Release)
- Resources/ folder (textures, models, shaders)
- Scenes/ folder (scene YAML files)
- Scripts/ folder (GameScripts.dll)

**No Conan, Visual Studio, or development tools required for end users.**

## Debugging Tips

### If Game.exe Crashes Immediately
1. Check `Build/Game/Game_log.txt` for last log message
2. Run with debugger attached: `windbg Game.exe` or attach Visual Studio debugger
3. Check all required DLLs are present: `ls Build/Game/*.dll`
4. Verify Resources/, Scenes/, Scripts/ folders exist

### If Exported Game Missing DLLs
1. Rebuild Runtime project (triggers PreBuildEvent DLL copying)
2. Check `x64/Debug/` folder has all DLLs before export
3. Verify export dialog copied all files (check export log in console panel)

### If Build Fails with PCH Errors
1. Close Visual Studio completely
2. Kill all MSBuild/cl.exe processes: `taskkill /F /IM MSBuild.exe /IM cl.exe`
3. Delete intermediate folders: `rm -rf Engine/BoomEngine/x64 x64/Debug/BoomEngine.*`
4. Rebuild solution

## Recent History

### Last Session Actions
1. User reported Game.exe crashes on launch with white screen
2. Added file logging to Runtime/src/main.cpp (Game_log.txt)
3. Found crash occurs after "Starting game loop..." → inside RunContext()
4. Added extensive logging to Application::RunContext() to pinpoint exact crash location
5. **CURRENT**: Need to rebuild BoomEngine + Runtime with new logging, then export and test

### What We Were About to Do
Run these commands:
```bash
# 1. Clean build
rm -rf Engine/BoomEngine/x64 x64/Debug/BoomEngine.*

# 2. Rebuild engine with new logging
MSBuild.exe Gam300.sln -t:BoomEngine -p:Configuration=Debug -p:Platform=x64 -v:minimal

# 3. Rebuild runtime
MSBuild.exe Runtime/Runtime.vcxproj -p:Configuration=Debug -p:Platform=x64 -v:minimal

# 4. Run Editor and export (File > Export Game)
# 5. Check Build/Game/Game_log.txt for detailed crash location
```

## Testing Checklist

When testing the export:
- [ ] Build Runtime project in Debug configuration
- [ ] All DLLs copied to x64/Debug/ (check ~15-20 DLLs)
- [ ] Export via Editor → File → Export Game
- [ ] Check Build/Game/ has:
  - [ ] Game.exe
  - [ ] All DLLs (~15-20 files)
  - [ ] Resources/ folder with shaders, textures, models
  - [ ] Scenes/ folder with .yaml files
  - [ ] Scripts/ folder with GameScripts.dll
- [ ] Run Game.exe
- [ ] Check Game_log.txt for crash location
- [ ] If crash: note last log message and work backwards to find issue

---

**Status**: Waiting for rebuild with new logging to diagnose crash location
**Next Person**: Clean build folders, rebuild BoomEngine + Runtime, export, test, analyze Game_log.txt
