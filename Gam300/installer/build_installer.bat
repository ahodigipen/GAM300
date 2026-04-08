@echo off
setlocal enabledelayedexpansion

echo === Starting Installer Package Build ===

if exist "D:\jenkins-agent\workspace\Team 11\Team Obsession\Gam300" (
    set WORKSPACE=D:\jenkins-agent\workspace\Team 11\Team Obsession\Gam300
) else (
    cd /d "%~dp0.."
    for /f "tokens=*" %%a in ('cd') do set WORKSPACE=%%a
)
set OUT_DIR=%WORKSPACE%\installer\output
set ZIP_NAME=TeamObsession_GAM300_Build_%BUILD_NUMBER%.zip
set ZIP_PATH=%WORKSPACE%\installer\%ZIP_NAME%

:: Clean previous output
if exist "%OUT_DIR%" (
    echo === Cleaning previous installer output ===
    rmdir /S /Q "%OUT_DIR%"
)
mkdir "%OUT_DIR%" 2>nul
mkdir "%OUT_DIR%\Release" 2>nul
mkdir "%OUT_DIR%\Debug" 2>nul

:: ========================
:: COPY RELEASE ARTIFACTS
:: ========================
echo === Copying Release artifacts ===

if not exist "%WORKSPACE%\x64\Release\BoomEngine.lib" (
    echo ERROR: BoomEngine.lib ^(Release^) not found! Did the build succeed?
    goto FAILED
)
if not exist "%WORKSPACE%\Runtime\x64\Release\BoomEngine.dll" (
    echo ERROR: BoomEngine.dll ^(Release^) not found! Did the build succeed?
    goto FAILED
)

xcopy /E /I /Y "%WORKSPACE%\x64\Release"          "%OUT_DIR%\Release\lib"
xcopy /E /I /Y "%WORKSPACE%\Runtime\x64\Release"  "%OUT_DIR%\Release\Runtime"
xcopy /E /I /Y "%WORKSPACE%\Editor\x64\Release"   "%OUT_DIR%\Release\Editor"   2>nul

:: ========================
:: COPY DEBUG ARTIFACTS
:: ========================
echo === Copying Debug artifacts ===

if not exist "%WORKSPACE%\x64\Debug\BoomEngine.lib" (
    echo ERROR: BoomEngine.lib ^(Debug^) not found! Did the build succeed?
    goto FAILED
)

xcopy /E /I /Y "%WORKSPACE%\x64\Debug"            "%OUT_DIR%\Debug\lib"
xcopy /E /I /Y "%WORKSPACE%\Runtime\x64\Debug"    "%OUT_DIR%\Debug\Runtime"
xcopy /E /I /Y "%WORKSPACE%\Editor\x64\Debug"     "%OUT_DIR%\Debug\Editor"     2>nul

:: ========================
:: COPY GAMESCRIPTS
:: ========================
echo === Copying GameScripts artifacts ===
xcopy /E /I /Y "%WORKSPACE%\GameScripts\bin"       "%OUT_DIR%\GameScripts"      2>nul

:: ========================
:: ZIP THE OUTPUT
:: ========================
echo === Creating zip: %ZIP_NAME% ===
if exist "%ZIP_PATH%" del /F /Q "%ZIP_PATH%"

powershell -Command "Compress-Archive -Path '%OUT_DIR%\*' -DestinationPath '%ZIP_PATH%' -Force"
if %ERRORLEVEL% neq 0 (
    echo ERROR: Failed to create zip archive!
    goto FAILED
)

echo === Installer package created successfully: %ZIP_NAME% ===
echo ZIP_PATH=%ZIP_PATH% > "%WORKSPACE%\installer\last_build.env"
echo ZIP_NAME=%ZIP_NAME% >> "%WORKSPACE%\installer\last_build.env"
goto END

:FAILED
echo === Installer packaging FAILED ===
exit /b 1

:END
echo === build_installer.bat complete ===
