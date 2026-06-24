# Design Document: Deep System Cleaning

**Date:** 2026-05-17  
**Topic:** Deep System Cleaning  
**Status:** In Review

## Purpose
Expand the `gemini-sys-cleaner` to perform a "Deep Clean" by recursively searching for large project-related caches and build artifacts in the user's profile on the C: drive, in addition to standard global temp folders.

## Scope
The cleaning will target the following areas on the **C: drive**:

### 1. Global System Targets (Existing)
- **Windows User Temp:** `%TEMP%`
- **Windows System Temp:** `%WINDIR%\Temp`
- **npm cache:** `%LOCALAPPDATA%\npm-cache`
- **pip cache:** `%LOCALAPPDATA%\pip\Cache`
- **uv cache:** `%LOCALAPPDATA%\uv\cache`
- **Yarn cache:** `%LOCALAPPDATA%\Yarn\Cache`

### 2. Deep Project Targets (Recursive Search in `%USERPROFILE%`)
The tool will recursively search for the following directory names starting from `C:\Users\hasee\`:
- **Node.js:** `node_modules`
- **Python:** `venv`, `.venv`, `env`, `__pycache__`, `.pytest_cache`
- **Web/Frontend:** `.next`, `.nuxt`, `.cache`, `.sass-cache`
- **Build Artifacts:** `dist`, `build`

## Architecture & Implementation
The C++ tool `gemini-sys-cleaner` will be updated.

### 1. Code Changes
- **`Cleaner.hpp`:** 
    - Re-enable and expand the `targets` list in the constructor.
    - Add a `Target` for recursive search in `%USERPROFILE%`.
    - Update `FindRecursiveTargets` (if needed) to include the expanded list of folder names.
- **`main.cpp`:** No changes needed as it already calls `Scan()` and `Clean()`.

### 2. Execution Flow
1. **Build:** Run `build.bat` to compile the updated tool.
2. **Deep Scan:** Run `.\gemini-sys-cleaner.exe scan`.
3. **Review:** Report the findings (folders found and total size) to the user.
4. **Deep Clean:** Once confirmed, run `.\gemini-sys-cleaner.exe clean`.

## Safety & Constraints
- **Scope Restriction:** Recursive searches are strictly limited to the user's profile directory on the C: drive.
- **Access Errors:** The tool will continue to gracefully skip folders it doesn't have permission to access.
- **Build Consistency:** The tool must be recompiled to reflect the new target configuration.

## Success Criteria
- The tool identifies significantly more than the initial 290 MB (by finding `node_modules`, etc.).
- The cleanup process successfully removes the targeted directories.
- No essential system files or non-cache user data are touched.
