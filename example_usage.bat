@echo off
echo Overlay Hook Example Usage
echo.
echo First, build the project:
echo   build.bat
echo.
echo Then find a DirectX application's PID:
echo   tasklist | findstr /i "notepad"
echo   (Replace "notepad" with your target application)
echo.
echo Finally, inject the overlay:
echo   build\bin\Release\OverlayInjector.exe [PID]
echo.
echo Example:
echo   build\bin\Release\OverlayInjector.exe 1234
echo.
echo The overlay will display "Hello World" in the top-left corner
echo of any DirectX 11 application.
echo.
echo Note: Run as Administrator for best results.
pause 