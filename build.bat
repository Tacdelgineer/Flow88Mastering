@echo off
echo ===================================================
echo Flow88 Mastering - Build Helper
echo ===================================================
echo.

:: Check for CMake
where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake is not installed or not in your PATH.
    echo Please install CMake from https://cmake.org/download/
    echo.
    pause
    exit /b 1
)

echo [INFO] CMake found. Configuring project...
echo.

cmake -S . -B build -G "Visual Studio 17 2022"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] CMake configuration failed. Please ensure Visual Studio 2022 with C++ is installed.
    echo.
    pause
    exit /b 1
)

echo.
echo [INFO] Configuration successful. Building Release version...
echo.

cmake --build build --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed. Check the errors above.
    echo.
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Build complete!
echo The VST3 plugin is located in: build\Flow88Master_artefacts\Release\VST3\
echo.
pause
