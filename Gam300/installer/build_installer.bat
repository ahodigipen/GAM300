@echo off
setlocal enabledelayedexpansion

echo === Starting Installer Package Build ===

:: ========================
:: AUTO-DETECT WORKSPACE
:: ========================
if exist "D:\jenkins-agent\workspace\Team 11\Team Obsession\Gam300" (
    set WORKSPACE=D:\jenkins-agent\workspace\Team 11\Team Obsession\Gam300
) else (
    cd /d "%~dp0.."
    for /f "tokens=*" %%a in ('cd') do set WORKSPACE=%%a
)
echo === Workspace: !WORKSPACE! ===

set OUT_DIR=!WORKSPACE!\installer\output
if not defined BUILD_NUMBER set BUILD_NUMBER=0
set ZIP_NAME=TeamObsession_GAM300_Build_!BUILD_NUMBER!.zip
set ZIP_PATH=!WORKSPACE!\installer\!ZIP_NAME!

:: Clean previous output
if exist "!OUT_DIR!" (
    echo === Cleaning previous installer output ===
    rmdir /S /Q "!OUT_DIR!"
)
mkdir "!OUT_DIR!" 2>nul
mkdir "!OUT_DIR!\Release" 2>nul
mkdir "!OUT_DIR!\Debug" 2>nul

:: ========================
:: COPY RELEASE ARTIFACTS
:: ========================
echo === Copying Release artifacts ===

if not exist "!WORKSPACE!\x64\Release\BoomEngine.lib" (
    echo ERROR: BoomEngine.lib ^(Release^) not found at !WORKSPACE!\x64\Release\
    echo Did you run build.bat or build in Visual Studio first?
    goto FAILED
)

:: Copy lib files
xcopy /E /I /Y "!WORKSPACE!\x64\Release" "!OUT_DIR!\Release\lib"

:: Copy Runtime DLL - check staged location first, then fallback to x64\Release
if exist "!WORKSPACE!\Runtime\x64\Release\BoomEngine.dll" (
    xcopy /E /I /Y "!WORKSPACE!\Runtime\x64\Release" "!OUT_DIR!\Release\Runtime"
) else if exist "!WORKSPACE!\x64\Release\BoomEngine.dll" (
    echo === Using x64\Release for Runtime artifacts ===
    mkdir "!OUT_DIR!\Release\Runtime" 2>nul
    copy /Y "!WORKSPACE!\x64\Release\BoomEngine.dll" "!OUT_DIR!\Release\Runtime\BoomEngine.dll"
    copy /Y "!WORKSPACE!\x64\Release\Runtime.exe" "!OUT_DIR!\Release\Runtime\Runtime.exe" 2>nul
) else (
    echo ERROR: BoomEngine.dll ^(Release^) not found!
    goto FAILED
)

:: Copy Editor - check staged location first, then fallback to x64\Release
if exist "!WORKSPACE!\Editor\x64\Release\Editor.exe" (
    xcopy /E /I /Y "!WORKSPACE!\Editor\x64\Release" "!OUT_DIR!\Release\Editor"
) else if exist "!WORKSPACE!\x64\Release\Editor.exe" (
    echo === Using x64\Release for Editor artifacts ===
    mkdir "!OUT_DIR!\Release\Editor" 2>nul
    copy /Y "!WORKSPACE!\x64\Release\Editor.exe" "!OUT_DIR!\Release\Editor\Editor.exe"
)

:: ========================
:: COPY DEBUG ARTIFACTS
:: ========================
echo === Copying Debug artifacts ===

if exist "!WORKSPACE!\x64\Debug\BoomEngine.lib" (
    xcopy /E /I /Y "!WORKSPACE!\x64\Debug" "!OUT_DIR!\Debug\lib"

    if exist "!WORKSPACE!\Runtime\x64\Debug\BoomEngine.dll" (
        xcopy /E /I /Y "!WORKSPACE!\Runtime\x64\Debug" "!OUT_DIR!\Debug\Runtime"
    )
    if exist "!WORKSPACE!\Editor\x64\Debug\Editor.exe" (
        xcopy /E /I /Y "!WORKSPACE!\Editor\x64\Debug" "!OUT_DIR!\Debug\Editor"
    )
) else (
    echo WARNING: Debug artifacts not found, skipping Debug configuration.
    echo This is OK if you only built Release.
)

:: ========================
:: COPY GAMESCRIPTS
:: ========================
echo === Copying GameScripts artifacts ===
if exist "!WORKSPACE!\GameScripts\bin" (
    xcopy /E /I /Y "!WORKSPACE!\GameScripts\bin" "!OUT_DIR!\GameScripts"
) else (
    echo WARNING: GameScripts bin not found, skipping.
)

:: ========================
:: ZIP THE OUTPUT
:: ========================
echo === Creating zip: !ZIP_NAME! ===
if exist "!ZIP_PATH!" del /F /Q "!ZIP_PATH!"

powershell -Command "Compress-Archive -Path '!OUT_DIR!\*' -DestinationPath '!ZIP_PATH!' -Force"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Failed to create zip archive!
    goto FAILED
)

echo === Installer package created successfully: !ZIP_NAME! ===
echo ZIP_PATH=!ZIP_PATH!> "!WORKSPACE!\installer\last_build.env"
echo ZIP_NAME=!ZIP_NAME!>> "!WORKSPACE!\installer\last_build.env"
goto END

:FAILED
echo === Installer packaging FAILED ===
pause
exit /b 1

:END
echo === build_installer.bat complete ===
pause
