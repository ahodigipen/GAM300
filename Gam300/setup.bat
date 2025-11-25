@echo off
setlocal enabledelayedexpansion

REM ================================================================
REM  GAM300 setup (CMake + Conan + Mono)
REM  Layout:
REM   <repo>\Gam300\          (your solution / CMake root)
REM   <repo>\mono\            (sibling; contains bin/, include/, lib/)
REM     bin\mono-2.0-sgen.dll
REM     lib\mono-2.0-sgen.lib
REM     include\mono-2.0\mono\jit\jit.h ...
REM ================================================================

REM ----------------------------------------------------------------
REM 0) Ensure CMake is available (install only if missing)
REM ----------------------------------------------------------------
call :FindCMake
if defined CMAKE_EXE (
    echo [INFO] CMake found: "%CMAKE_EXE%"
    goto :cmake_ready
)
echo [INFO] CMake not found. Installing...
call :InstallCMake
if not defined CMAKE_EXE (
    echo [ERROR] Failed to install CMake. Please install manually and re-run.
    pause & exit /b 1
)

:cmake_ready
set "PATH=%CMAKE_DIR%;%PATH%"
echo [INFO] CMake ready: "%CMAKE_EXE%"
"%CMAKE_EXE%" --version >nul 2>&1 || (
    echo [ERROR] CMake installed but not working properly.
    pause & exit /b 1
)

REM ----------------------------------------------------------------
REM 1) Ensure Python is available
REM ----------------------------------------------------------------
python --version >nul 2>&1 || (
    echo [ERROR] Python 3.8+ not found. Please install Python and add to PATH.
    pause & exit /b 1
)
for /f "tokens=1,*" %%a in ('python --version') do echo [INFO] Python found: %%a %%b

REM ----------------------------------------------------------------
REM 2) Ensure pip tooling exists
REM ----------------------------------------------------------------
echo [STEP] Checking pip, setuptools, wheel...
python -c "import pip, setuptools, wheel" >nul 2>&1 || (
    echo [STEP] Upgrading pip tooling...
    python -m pip install --upgrade pip setuptools wheel --quiet
)

REM ----------------------------------------------------------------
REM 3) Ensure Conan is installed (install only if missing)
REM ----------------------------------------------------------------
where conan >nul 2>&1
if errorlevel 1 (
    echo [STEP] Installing Conan...
    python -m pip install --user conan --quiet
    if errorlevel 1 (
        echo [ERROR] Failed to install Conan.
        pause & exit /b 1
    )
) else (
    echo [INFO] Conan already available.
)

REM ----------------------------------------------------------------
REM 4) Locate conan.exe and add to PATH
REM ----------------------------------------------------------------
call :FindConan
if not defined CONAN_EXE (
    echo [ERROR] Conan installed but not found on PATH.
    pause & exit /b 1
)
set "PATH=%CONAN_DIR%;%PATH%"
echo [INFO] Conan ready: "%CONAN_EXE%"
conan config set tools.cmake.cmake_program="%CMAKE_EXE%" >nul 2>&1

REM ----------------------------------------------------------------
REM 5) Repo root and third-party locations
REM ----------------------------------------------------------------
for %%i in ("%~dp0..") do set "REPO_ROOT=%%~fi"
echo [INFO] REPO_ROOT = "%REPO_ROOT%"

REM ---- FMOD ----
set "FMOD_DIR=%REPO_ROOT%\FMOD Studio API Windows\api"
if not exist "%FMOD_DIR%\core\lib\x64\fmod_vc.lib" (
  echo [ERROR] FMOD API not found at: "%FMOD_DIR%"
  echo         Expect: core\inc, core\lib\x64\fmod_vc.lib, studio\inc, studio\lib\x64\fmodstudio_vc.lib
  pause & exit /b 1
)
echo [INFO] FMOD_DIR = "%FMOD_DIR%"

REM ---- Compressonator ----
set "COMPRESSONATOR_DIR=%REPO_ROOT%\Compressonator"
if not exist "%COMPRESSONATOR_DIR%\Compressonator_MD.lib" (
   echo [ERROR] COMPRESSONATOR not found at: "%COMPRESSONATOR_DIR%"
   echo         Expect: Compressonator_MD.lib, Compressonator_MDd.lib, include\cmp_core.h, include\compressonator.h
   pause & exit /b 1
)
echo [INFO] COMPRESSONATOR_DIR = "%COMPRESSONATOR_DIR%"

REM ----------------------------------------------------------------
REM 5.5) Ensure Mono is found and set paths
REM ----------------------------------------------------------------
echo [STEP] Locating Mono runtime...
call :EnsureMono
if errorlevel 1 (
    echo [ERROR] Mono not found. Please install Mono MDK or place it in "%REPO_ROOT%\mono"
    echo         Required structure:
    echo           mono\bin\mono-2.0-sgen.dll
    echo           mono\lib\mono-2.0-sgen.lib
    echo           mono\include\mono-2.0\mono\jit\jit.h
    echo.
    echo [INFO] Download Mono MDK from: https://www.mono-project.com/download/stable/
    pause & exit /b 1
)
echo [INFO] MONO_INCLUDE_DIR = "%MONO_INCLUDE_DIR%"
echo [INFO] MONO_LIB_DIR     = "%MONO_LIB_DIR%"
echo [INFO] MONO_BIN_DIR     = "%MONO_BIN_DIR%"

REM ----------------------------------------------------------------
REM 6) Conan profile + installs (MSBuildDeps)
REM ----------------------------------------------------------------
if not exist profiles mkdir profiles

(
    echo [settings]
    echo os=Windows
    echo arch=x86_64
    echo compiler=msvc
    echo compiler.version=193
    echo compiler.runtime=dynamic
    echo compiler.cppstd=17
    echo build_type=Release
) > profiles\msvc17

echo [STEP] Installing dependencies for Release...
conan install . -of conanbuild\Release -pr:h profiles\msvc17 -pr:b profiles\msvc17 ^
  -s build_type=Release -g MSBuildDeps --build=missing ^
  -o glfw/*:shared=True -o glew/*:shared=True

echo [STEP] Installing dependencies for Debug...
conan install . -of conanbuild\Debug -pr:h profiles\msvc17 -pr:b profiles\msvc17 ^
  -s build_type=Debug -g MSBuildDeps --build=missing ^
  -o glfw/*:shared=True -o glew/*:shared=True


REM ----------------------------------------------------------------
REM 7) Copy Mono runtime DLLs next to Editor.exe (if present)
REM ----------------------------------------------------------------
set "EDITOR_DBG=%REPO_ROOT%\Gam300\Editor\x64\Debug"
set "EDITOR_REL=%REPO_ROOT%\Gam300\Editor\x64\Release"

call :CopyMonoDlls "%EDITOR_DBG%"
call :CopyMonoDlls "%EDITOR_REL%"

echo.
echo ==== Setup Complete! ====
echo - MONO_INCLUDE_DIR: "%MONO_INCLUDE_DIR%"
echo - MONO_LIB_DIR:     "%MONO_LIB_DIR%"
echo - MONO_BIN_DIR:     "%MONO_BIN_DIR%"
echo.
echo - CMake: include(^"${CMAKE_SNIPPET_DIR:\=\\}^/MonoPaths.cmake^")
echo - Link BoomEngine with: ^"${MONO_LIB_DIR}\mono-2.0-sgen.lib^"
echo - Keep code includes as: ^<mono/jit/jit.h^>, ^<mono/metadata/assembly.h^>, ^<mono/metadata/debug-helpers.h^>
pause
exit /b 0

REM ===================== Helper Functions =====================

:FindCMake
for /f "usebackq delims=" %%p in (`where cmake 2^>nul`) do (
    set "CMAKE_EXE=%%p"
    for %%d in ("%%p") do set "CMAKE_DIR=%%~dpd"
    goto :eof
)
set "PATHS[0]=C:\Program Files\CMake\bin\cmake.exe"
set "PATHS[1]=%LOCALAPPDATA%\Programs\CMake\bin\cmake.exe"
for /l %%i in (0,1,1) do (
    if defined PATHS[%%i] if exist "!PATHS[%%i]!" (
        set "CMAKE_EXE=!PATHS[%%i]!"
        for %%d in ("!PATHS[%%i]!") do set "CMAKE_DIR=%%~dpd"
        goto :eof
    )
)
call :FindVSCMake
goto :eof

:InstallCMake
where winget >nul 2>&1 && (
    echo [STEP] Installing CMake with winget...
    winget install -e --id Kitware.CMake --silent --accept-source-agreements --accept-package-agreements >nul 2>&1
    call :FindCMake
    if defined CMAKE_EXE goto :eof
)
where choco >nul 2>&1 && (
    echo [STEP] Installing CMake with Chocolatey...
    choco install cmake -y --no-progress --limit-output >nul 2>&1
    call :FindCMake
    if defined CMAKE_EXE goto :eof
)
call :FindVSCMake
goto :eof

:FindVSCMake
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :eof
for /f "usebackq delims=" %%p in (`
    "%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild ^
    -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe 2^>nul
`) do (
    if exist "%%p" (
        set "CMAKE_EXE=%%p"
        for %%d in ("%%p") do set "CMAKE_DIR=%%~dpd"
        goto :eof
    )
)
goto :eof

:FindConan
for /f "usebackq delims=" %%p in (`where conan 2^>nul`) do (
    set "CONAN_EXE=%%p"
    for %%d in ("%%p") do set "CONAN_DIR=%%~dpd"
    goto :eof
)
for /f "usebackq delims=" %%i in (`python -c "import sysconfig; print(sysconfig.get_path('scripts','nt_user'))" 2^>nul`) do (
    if exist "%%i\conan.exe" (
        set "CONAN_EXE=%%i\conan.exe"
        set "CONAN_DIR=%%i"
        goto :eof
    )
)
for /d %%d in ("%LOCALAPPDATA%\Programs\Python\*") do (
    if exist "%%d\Scripts\conan.exe" (
        set "CONAN_EXE=%%d\Scripts\conan.exe"
        set "CONAN_DIR=%%d\Scripts"
        goto :eof
    )
)
set "CONAN_EXE="
set "CONAN_DIR="
goto :eof

:CopyMonoDlls
set "DST=%~1"
if not defined DST goto :eof
if not exist "%DST%" (
  echo [WARN] Output dir not found yet: "%DST%" (build Editor once, then re-run copy if needed)
  goto :eof
)
if exist "%MONO_BIN_DIR%\mono-2.0-sgen.dll" (
  copy /Y "%MONO_BIN_DIR%\mono-2.0-sgen.dll" "%DST%\mono-2.0-sgen.dll" >nul
  echo [INFO] Copied mono-2.0-sgen.dll to "%DST%"
) else (
  echo [WARN] mono-2.0-sgen.dll not found in "%MONO_BIN_DIR%"
)
if exist "%MONO_BIN_DIR%\MonoPosixHelper.dll" (
  copy /Y "%MONO_BIN_DIR%\MonoPosixHelper.dll" "%DST%\MonoPosixHelper.dll" >nul
  echo [INFO] Copied MonoPosixHelper.dll to "%DST%"
)
goto :eof

:EnsureMono
REM 1) Prefer vendored mono in repo
set "MONO_ROOT=%REPO_ROOT%\mono"
echo [INFO] Checking for Mono in repo: "%MONO_ROOT%"
if exist "%MONO_ROOT%\include\mono-2.0\mono\jit\jit.h" (
  if exist "%MONO_ROOT%\lib\mono-2.0-sgen.lib" (
    if exist "%MONO_ROOT%\bin\mono-2.0-sgen.dll" (
      set "MONO_INCLUDE_DIR=%MONO_ROOT%\include\mono-2.0"
      set "MONO_LIB_DIR=%MONO_ROOT%\lib"
      set "MONO_BIN_DIR=%MONO_ROOT%\bin"
      echo [INFO] Found Mono in repo: "%MONO_ROOT%"
      exit /b 0
    )
  )
)

REM 2) Try system-wide Mono MDK default installs
echo [INFO] Checking system-wide Mono installations...

REM Check Program Files
if exist "%ProgramFiles%\Mono\include\mono-2.0\mono\jit\jit.h" (
  if exist "%ProgramFiles%\Mono\lib\mono-2.0-sgen.lib" (
    if exist "%ProgramFiles%\Mono\bin\mono-2.0-sgen.dll" (
      set "MONO_INCLUDE_DIR=%ProgramFiles%\Mono\include\mono-2.0"
      set "MONO_LIB_DIR=%ProgramFiles%\Mono\lib"
      set "MONO_BIN_DIR=%ProgramFiles%\Mono\bin"
      echo [INFO] Found Mono at: "%ProgramFiles%\Mono"
      exit /b 0
    )
  )
)

REM Check Program Files (x86)
if exist "%ProgramFiles(x86)%\Mono\include\mono-2.0\mono\jit\jit.h" (
  if exist "%ProgramFiles(x86)%\Mono\lib\mono-2.0-sgen.lib" (
    if exist "%ProgramFiles(x86)%\Mono\bin\mono-2.0-sgen.dll" (
      set "MONO_INCLUDE_DIR=%ProgramFiles(x86)%\Mono\include\mono-2.0"
      set "MONO_LIB_DIR=%ProgramFiles(x86)%\Mono\lib"
      set "MONO_BIN_DIR=%ProgramFiles(x86)%\Mono\bin"
      echo [INFO] Found Mono at: "%ProgramFiles(x86)%\Mono"
      exit /b 0
    )
  )
)

REM Check Chocolatey location
if exist "C:\ProgramData\chocolatey\lib\mono\tools\include\mono-2.0\mono\jit\jit.h" (
  if exist "C:\ProgramData\chocolatey\lib\mono\tools\lib\mono-2.0-sgen.lib" (
    if exist "C:\ProgramData\chocolatey\lib\mono\tools\bin\mono-2.0-sgen.dll" (
      set "MONO_INCLUDE_DIR=C:\ProgramData\chocolatey\lib\mono\tools\include\mono-2.0"
      set "MONO_LIB_DIR=C:\ProgramData\chocolatey\lib\mono\tools\lib"
      set "MONO_BIN_DIR=C:\ProgramData\chocolatey\lib\mono\tools\bin"
      echo [INFO] Found Mono at: C:\ProgramData\chocolatey\lib\mono\tools
      exit /b 0
    )
  )
)

REM 3) If we reach here, we failed
echo [ERROR] Mono not found in any standard location!
exit /b 1