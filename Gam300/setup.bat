@echo off
setlocal enabledelayedexpansion

REM ------------------------------------------------------------------------
REM 1) Ensure Python is available
REM ------------------------------------------------------------------------
where python >nul 2>&1 || (
  echo [ERROR] Python 3.8+ not found in PATH. Please install and add to PATH.
  pause & exit /b 1
)

REM ------------------------------------------------------------------------
REM 2) Upgrade pip and install Conan
REM ------------------------------------------------------------------------
echo [STEP] Installing/upgrading pip, setuptools, wheel...
python -m pip install --upgrade pip setuptools wheel

echo [STEP] Installing/upgrading Conan...
python -m pip install --user --upgrade conan
if errorlevel 1 (
  echo [ERROR] pip install conan failed.
  pause & exit /b 1
)

REM ------------------------------------------------------------------------
REM 3) Try to find conan.exe in common paths
REM ------------------------------------------------------------------------
set "CONAN_PATH="

REM First: check sysconfig location
for /f "usebackq delims=" %%i in (`python -c "import sysconfig; print(sysconfig.get_path('scripts','nt_user'))"`) do (
    if exist "%%i\conan.exe" set "CONAN_PATH=%%i"
)

REM Second: fallback to common LocalAppData-based locations
if not defined CONAN_PATH (
  for /d %%d in ("%LOCALAPPDATA%\Programs\Python\*") do (
    if exist "%%d\Scripts\conan.exe" (
      set "CONAN_PATH=%%d\Scripts"
      goto :found
    )
  )
)

:found
if not defined CONAN_PATH (
  echo [ERROR] conan.exe not found in user scripts folders.
  pause & exit /b 1
)

set "PATH=%CONAN_PATH%;%PATH%"
echo [INFO] Using Conan from: %CONAN_PATH%\conan.exe

REM ------------------------------------------------------------------------
REM 4) Detect default profile
REM ------------------------------------------------------------------------
echo [STEP] Detecting Conan default profile...
conan profile detect --force
if errorlevel 1 (
  echo [ERROR] Conan profile detect failed.
  pause & exit /b 1
)

REM ------------------------------------------------------------------------
REM 5 & 6) Install Debug and Release property sheets
REM Output goes to conanbuild\ for each build type
echo [STEP] Installing Conan packages for Release...
conan install . --output-folder=conanbuild --build=missing -g MSBuildDeps ^
             -s build_type=Release -s arch=x86_64

echo [STEP] Installing Conan packages for Debug...
conan install . --output-folder=conanbuild --build=missing -g MSBuildDeps ^
             -s build_type=Debug   -s arch=x86_64

echo.
echo ==== Done! Props are inside conanbuild/ ====
pause