@echo off
setlocal

taskkill /F /IM unit_tests.exe /IM system-cleaner-agent.exe >nul 2>nul

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
        copy Release\system-cleaner-agent.exe ..\system-cleaner-agent.exe /Y >nul 2>nul
        copy Release\system-cleaner-agent.exe "D:\Software\bin\system-cleaner-agent.exe" /Y >nul 2>nul
        echo Deployed to: D:\Software\bin\system-cleaner-agent.exe (global PATH)
    ) else (
        echo Build failed.
    )
    cd ..
    exit /b %ERRORLEVEL%
)

echo CMake not found in PATH.
exit /b 1
