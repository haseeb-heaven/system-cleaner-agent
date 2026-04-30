@echo off
setlocal

echo Building Gemini System Cleaner...

:: Check for CMake
where cmake >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo Using CMake to build...
    mkdir build 2>nul
    cd build
    cmake ..
    cmake --build . --config Release
    if %ERRORLEVEL% equ 0 (
        echo.
        echo Build successful! Executable is at build\Release\gemini-sys-cleaner.exe
        copy Release\gemini-sys-cleaner.exe ..\gemini-sys-cleaner.exe
    ) else (
        echo Build failed.
    )
    cd ..
    exit /b
)

:: Check for MSVC (cl.exe)
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo Using MSVC cl.exe to build...
    cl /EHsc /std:c++17 /O2 main.cpp /Fe:gemini-sys-cleaner.exe
    if %ERRORLEVEL% equ 0 (
        echo Build successful!
    ) else (
        echo Build failed.
    )
    exit /b
)

:: Check for G++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo Using G++ to build...
    g++ -std=c++17 -O2 main.cpp -o gemini-sys-cleaner.exe -static
    if %ERRORLEVEL% equ 0 (
        echo Build successful!
    ) else (
        echo Build failed.
    )
    exit /b
)

echo No suitable compiler found. Please install CMake, MSVC, or MinGW G++.
exit /b 1
