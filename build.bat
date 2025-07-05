@echo off
echo Building Overlay Hook Project...

REM Create build directory
if not exist build mkdir build
cd build

REM Configure CMake
cmake .. -G "Visual Studio 17 2022" -A x64

REM Build the project
cmake --build . --config Release

echo Build complete!
echo.
echo Executable: build/bin/Release/OverlayInjector.exe
echo DLL: build/bin/Release/OverlayHook.dll
echo.
echo Usage: OverlayInjector.exe [PID]

pause 