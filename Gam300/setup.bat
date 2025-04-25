@echo off
setlocal

REM ------------------------------------------------------------------------
REM 1) Ensure Python is on PATH
REM ------------------------------------------------------------------------
where python >nul 2>&1 || (
  echo [ERROR] Python 3.8+ not found. Please install and add to PATH.
  pause & exit /b 1
)

REM ------------------------------------------------------------------------
REM 2) Upgrade pip & install Conan into your user site
REM ------------------------------------------------------------------------
echo [STEP] Installing/upgrading pip, setuptools, wheel...
python -m pip install --upgrade pip setuptools wheel

echo [STEP] Installing/upgrading Conan into user site...
python -m pip install --user --upgrade conan
if errorlevel 1 (
  echo [ERROR] pip install conan failed.
  pause & exit /b 1
)

REM ------------------------------------------------------------------------
REM 3) Add the user‐scripts folder (where conan.exe lives) to PATH
REM ------------------------------------------------------------------------
for /f "usebackq delims=" %%i in (`
  python -c "import sysconfig; print(sysconfig.get_path('scripts','nt_user'))"
`) do set "USER_SCRIPTS=%%i"

if not exist "%USER_SCRIPTS%\conan.exe" (
  echo [ERROR] conan.exe not found in %USER_SCRIPTS%
  pause & exit /b 1
)
set "PATH=%USER_SCRIPTS%;%PATH%"
echo [INFO] Using Conan from: %USER_SCRIPTS%\conan.exe

REM ------------------------------------------------------------------------
REM 4) Bootstrap the default profile
REM ------------------------------------------------------------------------
echo [STEP] Detecting (or recreating) Conan default profile...
conan profile detect --force
if errorlevel 1 (
  echo [ERROR] Conan profile detect failed.
  pause & exit /b 1
)

REM ------------------------------------------------------------------------
REM 5) Install Release-x64 property sheets
conan install . --output-folder=conanbuild --build=missing -g MSBuildDeps ^
             -s build_type=Release -s arch=x86_64

REM 6) Install Debug-x64 property sheets (appends debug entries)
conan install . --output-folder=conanbuild --build=missing -g MSBuildDeps ^
             -s build_type=Debug   -s arch=x86_64

echo.
echo ==== Done! Both Release and Debug props are in conanbuild\ ====
pause
