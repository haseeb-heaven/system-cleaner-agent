@echo off
setlocal

echo Building Gemini System Cleaner Professional Edition...
where cmake >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Using CMake to build...
    mkdir build 2>nul
    cd build
    cmake ..
    cmake --build . --config Release
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo Build successful! Executables built.
        copy Release\gemini-sys-cleaner-pro.exe ..\gemini-sys-cleaner-pro.exe /Y 2>nul
        copy Release\gemini-pro-cleaner.exe ..\gemini-pro-cleaner.exe /Y 2>nul
        copy Release\gemini-sys-cleaner-pro.exe ..\gemini-sys-cleaner.exe /Y 2>nul
    ) else (
        echo Build failed.
    )
    cd ..
    exit /b %ERRORLEVEL%
)

echo CMake not found in PATH.
exit /b 1
