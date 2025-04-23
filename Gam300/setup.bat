@echo off
setlocal

:: ---------------------------------------------------------------------
:: 0. Go into the repo root (where this .bat lives)
:: ---------------------------------------------------------------------
pushd "%~dp0"

:: ---------------------------------------------------------------------
:: 1/4 — Ensure build dir exists
:: ---------------------------------------------------------------------
echo [1/4] Ensuring build directory exists…
set "BUILD_DIR=out\build\x64-Debug"
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%" || goto :err
)

:: ---------------------------------------------------------------------
:: 2/4 — Configure with CMake
:: ---------------------------------------------------------------------
echo [2/4] Running CMake configure…
cmake -S . -B "%BUILD_DIR%" ^
    "-GVisual Studio 17 2022" ^
    "-Ax64" || goto :err

:: ---------------------------------------------------------------------
:: 3/4 — Bootstrap Python→pip→Conan
:: ---------------------------------------------------------------------
echo [3/4] Installing Python, pip & Conan…
cmake --build "%BUILD_DIR%" --target InstallDependencies --config Debug || goto :err

:: ---------------------------------------------------------------------
:: 4/4 — Done
:: ---------------------------------------------------------------------
echo.
echo [4/4] All prerequisites installed!
echo Embedded Python version:
"%BUILD_DIR%\external\python\python.exe" --version
echo.
echo Next steps:
echo   * Open Gam300.sln
echo   * or  cmake --build "%BUILD_DIR%" --config Debug
pause
popd
exit /b 0

:err
echo.
echo [ERROR] Something went wrong. Press any key to exit…
pause >nul
popd
exit /b 1
