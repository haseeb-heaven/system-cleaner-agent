@echo off
setlocal

echo Building system-cleaner-agent (C++17 ReAct Autonomous Agent Engine)...
where cmake >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Using CMake to build...
    mkdir build 2>nul
    cd build
    cmake ..
    cmake --build . --config Release
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo Build successful! Executable built.
        copy Release\system-cleaner-agent.exe ..\system-cleaner-agent.exe /Y 2>nul
    ) else (
        echo Build failed.
    )
    cd ..
    exit /b %ERRORLEVEL%
)

echo CMake not found in PATH.
exit /b 1
