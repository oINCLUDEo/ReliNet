@echo off
REM Simple build script for ReliNet (Windows)

echo === Building ReliNet ===

REM Create build directory if it doesn't exist
if not exist "build" (
    mkdir build
    echo Created build directory
)

cd build

REM Configure with CMake
echo.
echo Configuring with CMake...
cmake .. -G "Ninja"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Failed to configure. Trying with default generator...
    cmake ..
    if %ERRORLEVEL% NEQ 0 (
        echo Configuration failed!
        cd ..
        exit /b 1
    )
)

REM Build
echo.
echo Building...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    cd ..
    exit /b 1
)

cd ..

echo.
echo === Build completed successfully! ===
echo.
echo Executables:
echo   - Server: build\ReliNetServer.exe
echo   - Client: build\ReliNetClient.exe
echo.
echo To run:
echo   Terminal 1: build\ReliNetServer.exe
echo   Terminal 2: build\ReliNetClient.exe
