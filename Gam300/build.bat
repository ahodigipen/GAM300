@echo off
setlocal enabledelayedexpansion

echo === Finding MSBuild ===
for /f "tokens=*" %%i in ('"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe') do set MSBUILD=%%i
echo MSBuild found at: %MSBUILD%

:: Get actual commit info
for /f "tokens=*" %%a in ('git log -1 --pretty^=format:"%%an"') do set ACTUAL_AUTHOR=%%a
for /f "tokens=*" %%b in ('git log -1 --pretty^=format:"%%ae"') do set ACTUAL_EMAIL=%%b
for /f "tokens=*" %%c in ('git log -1 --pretty^=format:"%%s"') do set COMMIT_MSG=%%c
set COMMIT_MSG=%COMMIT_MSG:|= -%

:: ========================
:: AUTO-DETECT WORKSPACE
:: ========================
if exist "D:\jenkins-agent\workspace\Team 11\Team Obsession\Gam300" (
    set WORKSPACE=D:\jenkins-agent\workspace\Team 11\Team Obsession\Gam300
    set IS_JENKINS=1
) else (
    cd /d "%~dp0"
    for /f "tokens=*" %%a in ('cd') do set WORKSPACE=%%a
    set IS_JENKINS=0
)
echo === Workspace: %WORKSPACE% ===

:: ========================
:: VERIFY SDK FOLDERS EXIST
:: ========================
if not exist "%WORKSPACE%\..\FMOD Studio API Windows\api\core\inc\fmod.hpp" (
    echo ERROR: FMOD headers not found!
    goto FAILED
)
if not exist "%WORKSPACE%\..\mono\include\mono-2.0\mono\jit\jit.h" (
    echo ERROR: Mono headers not found!
    goto FAILED
)
if not exist "%WORKSPACE%\..\Compressonator\include\compressonator.h" (
    echo ERROR: Compressonator headers not found!
    goto FAILED
)
echo === All SDK headers verified ===

:: ========================
:: ENSURE CONAN IS INSTALLED
:: ========================
where conan >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo === Conan not found, installing via pip ===
    pip install conan
    if !ERRORLEVEL! neq 0 (
        echo ERROR: Failed to install Conan. Is Python/pip installed?
        goto FAILED
    )
    echo === Conan installed successfully ===
) else (
    echo === Conan already installed, skipping ===
)

:: ========================
:: INSTALL CONAN DEPENDENCIES
:: ========================
cd /d "%WORKSPACE%"
conan profile detect --exist-ok

echo === Clearing stale Conan cache for assimp and yaml-cpp ===
conan remove "assimp*" -c
conan remove "yaml-cpp*" -c

echo === Installing Conan dependencies Release ===
conan install . --build=missing -s build_type=Release -s compiler.cppstd=17 -s compiler.version=194 -s compiler.runtime=dynamic -s compiler.runtime_type=Release
if %ERRORLEVEL% neq 0 goto FAILED

echo === Installing Conan dependencies Debug ===
conan install . --build=missing -s build_type=Debug -s compiler.cppstd=17 -s compiler.version=194 -s compiler.runtime=dynamic -s compiler.runtime_type=Debug
if %ERRORLEVEL% neq 0 goto FAILED

cd /d "%WORKSPACE%\.."

:: ========================
:: SETUP GLEW / GLFW / GLM
:: ========================
set GLEW_DIR=D:\dependencies\glew
if not exist "%GLEW_DIR%\include\GL\glew.h" (
    echo === Downloading GLEW ===
    mkdir "%GLEW_DIR%" 2>nul
    curl -L "https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0-win32.zip" -o "%GLEW_DIR%\glew.zip"
    powershell -command "Expand-Archive -Path '%GLEW_DIR%\glew.zip' -DestinationPath '%GLEW_DIR%' -Force"
    xcopy /E /I /Y "%GLEW_DIR%\glew-2.2.0\include" "%GLEW_DIR%\include"
) else ( echo === GLEW already present === )

set GLFW_DIR=D:\dependencies\glfw
if not exist "%GLFW_DIR%\include\GLFW\glfw3.h" (
    echo === Downloading GLFW ===
    mkdir "%GLFW_DIR%" 2>nul
    curl -L "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip" -o "%GLFW_DIR%\glfw.zip"
    powershell -command "Expand-Archive -Path '%GLFW_DIR%\glfw.zip' -DestinationPath '%GLFW_DIR%' -Force"
    xcopy /E /I /Y "%GLFW_DIR%\glfw-3.4.bin.WIN64\include" "%GLFW_DIR%\include"
) else ( echo === GLFW already present === )

set GLM_DIR=D:\dependencies\glm
if not exist "%GLM_DIR%\glm\glm.hpp" (
    echo === Downloading GLM ===
    mkdir "%GLM_DIR%" 2>nul
    curl -L "https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip" -o "%GLM_DIR%\glm.zip"
    powershell -command "Expand-Archive -Path '%GLM_DIR%\glm.zip' -DestinationPath '%GLM_DIR%' -Force"
) else ( echo === GLM already present === )

:: ========================
:: PROPS PATHS
:: ========================
set EXTRA_PROPS=D:\dependencies\extra_includes.props
set CONAN_PROPS=%WORKSPACE%\conandeps.props
set FORCE_IMPORTS=%EXTRA_PROPS%;%CONAN_PROPS%

:: ========================
:: BUILD ALL - RELEASE
:: ========================
echo === Building BoomEngine Release ===
"%MSBUILD%" "%WORKSPACE%\Engine\BoomEngine\BoomEngine.vcxproj" /p:Configuration=Release /p:Platform=x64 /m /p:ForceImportBeforeCppTargets="%FORCE_IMPORTS%"
if %ERRORLEVEL% neq 0 goto FAILED

echo === Staging BoomEngine outputs for Runtime (Release) ===
mkdir "%WORKSPACE%\x64\Release" 2>nul
copy /Y "%WORKSPACE%\Engine\BoomEngine\x64\Release\BoomEngine.lib" "%WORKSPACE%\x64\Release\BoomEngine.lib"
if %ERRORLEVEL% neq 0 goto FAILED

mkdir "%WORKSPACE%\Runtime\x64\Release" 2>nul
copy /Y "%WORKSPACE%\Engine\BoomEngine\x64\Release\BoomEngine.dll" "%WORKSPACE%\Runtime\x64\Release\BoomEngine.dll"
if %ERRORLEVEL% neq 0 goto FAILED

echo === Verifying Release staging ===
if not exist "%WORKSPACE%\x64\Release\BoomEngine.lib" (
    echo ERROR: BoomEngine.lib Release staging failed
    goto FAILED
)
echo === Release staging verified ===

echo === Building Editor Release ===
"%MSBUILD%" "%WORKSPACE%\Editor\Editor.vcxproj" /p:Configuration=Release /p:Platform=x64 /m /p:BuildProjectReferences=false /p:ForceImportBeforeCppTargets="%FORCE_IMPORTS%"
if %ERRORLEVEL% neq 0 goto FAILED

echo === Building Runtime Release ===
"%MSBUILD%" "%WORKSPACE%\Runtime\Runtime.vcxproj" /p:Configuration=Release /p:Platform=x64 /m /p:BuildProjectReferences=false /p:ForceImportBeforeCppTargets="%FORCE_IMPORTS%"
if %ERRORLEVEL% neq 0 goto FAILED

echo === Building GameScripts Release ===
"%MSBUILD%" "%WORKSPACE%\GameScripts\GameScripts.csproj" /p:Configuration=Release /p:Platform=x64 /m
if %ERRORLEVEL% neq 0 goto FAILED

:: ========================
:: BUILD ALL - DEBUG
:: ========================
echo === Building BoomEngine Debug ===
"%MSBUILD%" "%WORKSPACE%\Engine\BoomEngine\BoomEngine.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m /p:CL_MPCount=1 /p:ForceImportBeforeCppTargets="%FORCE_IMPORTS%"
if %ERRORLEVEL% neq 0 goto FAILED

echo === Staging BoomEngine outputs for Runtime + Editor (Debug) ===
mkdir "%WORKSPACE%\x64\Debug" 2>nul
copy /Y "%WORKSPACE%\Engine\BoomEngine\x64\Debug\BoomEngine.lib" "%WORKSPACE%\x64\Debug\BoomEngine.lib"
if %ERRORLEVEL% neq 0 goto FAILED

mkdir "%WORKSPACE%\Runtime\x64\Debug" 2>nul
copy /Y "%WORKSPACE%\Engine\BoomEngine\x64\Debug\BoomEngine.dll" "%WORKSPACE%\Runtime\x64\Debug\BoomEngine.dll"
if %ERRORLEVEL% neq 0 goto FAILED

mkdir "%WORKSPACE%\Editor\x64\Debug" 2>nul
copy /Y "%WORKSPACE%\Engine\BoomEngine\x64\Debug\BoomEngine.dll" "%WORKSPACE%\Editor\x64\Debug\BoomEngine.dll"
if %ERRORLEVEL% neq 0 goto FAILED

echo === Building Editor Debug ===
"%MSBUILD%" "%WORKSPACE%\Editor\Editor.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m /p:CL_MPCount=1 /p:BuildProjectReferences=false /p:ForceImportBeforeCppTargets="%FORCE_IMPORTS%"
if %ERRORLEVEL% neq 0 goto FAILED

echo === Building Runtime Debug ===
"%MSBUILD%" "%WORKSPACE%\Runtime\Runtime.vcxproj" /p:Configuration=Debug /p:Platform=x64 /m /p:CL_MPCount=1 /p:BuildProjectReferences=false /p:ForceImportBeforeCppTargets="%FORCE_IMPORTS%"
if %ERRORLEVEL% neq 0 goto FAILED

echo === Building GameScripts Debug ===
"%MSBUILD%" "%WORKSPACE%\GameScripts\GameScripts.csproj" /p:Configuration=Debug /p:Platform=x64 /m
if %ERRORLEVEL% neq 0 goto FAILED

:: ========================
:: SUCCESS
:: ========================
:SUCCESS
powershell -Command "$body = @{embeds=@(@{title='All Builds Succeeded';color=3066993;fields=@(@{name='Project';value='Team Obsession - GAM300';inline=$true},@{name='Build';value='#%BUILD_NUMBER%';inline=$true},@{name='Branch';value='%GIT_BRANCH%';inline=$true},@{name='Pushed By';value='%ACTUAL_AUTHOR%';inline=$true},@{name='Email';value='%ACTUAL_EMAIL%';inline=$true},@{name='Commit Message';value='%COMMIT_MSG%';inline=$false},@{name='Configurations';value='Release + Debug';inline=$true},@{name='Projects Built';value='BoomEngine, Editor, Runtime, GameScripts';inline=$false},@{name='Jenkins URL';value='%BUILD_URL%';inline=$false});footer=@{text='Jenkins CI - Team 11'}})} | ConvertTo-Json -Depth 10; Invoke-RestMethod -Uri 'https://discord.com/api/webhooks/1484446045667594340/ya3Mqf-qjHocsqBFyUEkXK4YR0lJ-qqEHegyEjpkxbmZVos6u25FnHmmSNI9O4lovt31' -Method Post -ContentType 'application/json' -Body $body"
goto END

:: ========================
:: FAILED
:: ========================
:FAILED
powershell -Command "$body = @{embeds=@(@{title='Build Failed';color=15158332;fields=@(@{name='Project';value='Team Obsession - GAM300';inline=$true},@{name='Build';value='#%BUILD_NUMBER%';inline=$true},@{name='Branch';value='%GIT_BRANCH%';inline=$true},@{name='Pushed By';value='%ACTUAL_AUTHOR%';inline=$true},@{name='Email';value='%ACTUAL_EMAIL%';inline=$true},@{name='Commit Message';value='%COMMIT_MSG%';inline=$false},@{name='Action Required';value='Check Jenkins console for errors!';inline=$false},@{name='Jenkins URL';value='%BUILD_URL%';inline=$false});footer=@{text='Jenkins CI - Team 11'}})} | ConvertTo-Json -Depth 10; Invoke-RestMethod -Uri 'https://discord.com/api/webhooks/1484446045667594340/ya3Mqf-qjHocsqBFyUEkXK4YR0lJ-qqEHegyEjpkxbmZVos6u25FnHmmSNI9O4lovt31' -Method Post -ContentType 'application/json' -Body $body"
exit /b 1

:END
