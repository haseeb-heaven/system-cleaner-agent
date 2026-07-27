@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo ===================================================
echo  Gemini System Cleaner v2.0 - Automatic Deep Clean
echo ===================================================

for /f "tokens=3" %%a in ('dir C:\ /-c ^| findstr /c:"bytes free"') do set FREE_BEFORE_C=%%a
for /f "tokens=3" %%a in ('dir D:\ /-c ^| findstr /c:"bytes free"') do set FREE_BEFORE_D=%%a

echo.
echo [1/3] Running Cache & Junk Cleaner (Python Engine)...
D:\henv\Scripts\python.exe sys_cleaner.py --clean --recycle-bin

echo.
echo [2/3] Running Native C++ Deep Engine...
if exist gemini-sys-cleaner.exe (
    .\gemini-sys-cleaner.exe clean
)

echo.
echo [3/3] Scanning Space Hotspots across Drives...
D:\henv\Scripts\python.exe sys_cleaner.py --scan --drive all

for /f "tokens=3" %%a in ('dir C:\ /-c ^| findstr /c:"bytes free"') do set FREE_AFTER_C=%%a
for /f "tokens=3" %%a in ('dir D:\ /-c ^| findstr /c:"bytes free"') do set FREE_AFTER_D=%%a

echo.
echo ===================================================
echo  Clean Completed!
echo  C: Free Space Before: %FREE_BEFORE_C% bytes
echo  C: Free Space After:  %FREE_AFTER_C% bytes
echo  D: Free Space Before: %FREE_BEFORE_D% bytes
echo  D: Free Space After:  %FREE_AFTER_D% bytes
echo ===================================================
