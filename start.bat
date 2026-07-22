@echo off
rem Roadside Editor launcher.
rem Usage: start.bat [project.terrainproj]
rem Without an argument, opens the demo project (Ribbon -> Multi-Scale Erosion).

setlocal
set "EXE=%~dp0build\Debug\roadside_editor.exe"
set "PROJECT=%~1"
if "%PROJECT%"=="" set "PROJECT=%~dp0assets\sample\demo_ribbon_erosion.terrainproj"

if not exist "%EXE%" (
    echo roadside_editor.exe not found: %EXE%
    echo Build it first:
    echo   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
    echo   cmake --build build --config Debug
    pause
    exit /b 1
)

start "" /d "%~dp0build\Debug" "%EXE%" "%PROJECT%"
endlocal
