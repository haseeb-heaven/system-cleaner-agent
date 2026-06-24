# Design Document: System Cleaning (Global Caches)

**Date:** 2026-05-08  
**Topic:** System Cleaning  
**Status:** Approved

## Purpose
Clean system temporary folders and package manager caches (npm, pip, yarn, uv) to free up disk space, while avoiding aggressive recursive searches in the user's project directories.

## Scope
The cleaning will target the following "Global Caches":
- **Windows User Temp:** `%TEMP%`
- **Windows System Temp:** `%WINDIR%\Temp`
- **npm cache:** `%LOCALAPPDATA%\npm-cache` and `%USERPROFILE%\.npm`
- **pip cache:** `%LOCALAPPDATA%\pip\Cache`
- **uv cache:** `%LOCALAPPDATA%\uv\cache`
- **Yarn cache:** `%LOCALAPPDATA%\Yarn\Cache`
- **Generic .cache:** `%USERPROFILE%\.cache`

**Excluded:** Recursive search for `node_modules`, `__pycache__`, and `.pytest_cache` in the user profile.

## Architecture & Implementation
The existing C++ tool `gemini-sys-cleaner` will be used.

### 1. Modification
- Modify `Cleaner.hpp` to remove the `Target` entry for "Project caches" (recursive search).

### 2. Build Process
- Execute `build.bat`. This will use CMake, MSVC (cl.exe), or G++ to produce `gemini-sys-cleaner.exe`.

### 3. Execution Flow
- **Scan Phase:** Run `gemini-sys-cleaner.exe scan`. This will:
    - Iterate through the defined global targets.
    - Calculate the total size of each directory.
    - Print the individual sizes and a grand total to the log/console.
- **Reporting:** I will report the total potential space savings to the user.
- **Clean Phase (Future):** Once the user confirms the scan results, `gemini-sys-cleaner.exe clean` will be run in a subsequent step.

## Success Criteria
- The tool successfully builds on the current system.
- The scan accurately identifies the size of global caches.
- No files in active project directories are targeted.
