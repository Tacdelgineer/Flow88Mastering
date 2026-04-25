@echo off
echo ===================================================
echo Flow88 Mastering - VST3 Installer
echo ===================================================
echo.

set "VST3_DIR=%CommonProgramFiles%\VST3"
set "PLUGIN_FILE=Flow88 Master.vst3"
set "BUILD_PLUGIN_PATH=build\Flow88Master_artefacts\Release\VST3\%PLUGIN_FILE%"

:: Check if plugin exists in build folder
if exist "%BUILD_PLUGIN_PATH%" (
    set "SOURCE_FILE=%BUILD_PLUGIN_PATH%"
) else if exist "%PLUGIN_FILE%" (
    set "SOURCE_FILE=%PLUGIN_FILE%"
) else (
    echo [ERROR] Could not find '%PLUGIN_FILE%'.
    echo Please make sure you have built the plugin or downloaded the compiled release.
    echo.
    pause
    exit /b 1
)

echo [INFO] Found plugin at: %SOURCE_FILE%
echo [INFO] Installing to: %VST3_DIR%\%PLUGIN_FILE%
echo.

:: Create VST3 directory if it doesn't exist
if not exist "%VST3_DIR%" (
    echo [INFO] Creating VST3 folder...
    mkdir "%VST3_DIR%"
)

echo [INFO] Copying files...
xcopy /E /I /Y "%SOURCE_FILE%" "%VST3_DIR%\%PLUGIN_FILE%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Flow88 Master has been installed!
    echo You can now rescan your plugins in your DAW.
) else (
    echo.
    echo [ERROR] Installation failed. You may need to run this script as Administrator.
)

echo.
pause
