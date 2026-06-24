# Deep System Cleaning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand the system cleaner to perform a deep recursive search for project-related caches and build artifacts in the user's profile.

**Architecture:** Update the C++ `Cleaner` class to include more recursive targets and ensure they are searched within the user's profile.

**Tech Stack:** C++, Batch (build.bat)

---

### Task 1: Update Cleaner Targets

**Files:**
- Modify: `Cleaner.hpp`

- [ ] **Step 1: Expand recursive target list in Cleaner.hpp**

Modify the `Cleaner()` constructor and `Scan()`/`Clean()` methods to handle the expanded list of directory names.

```cpp
<<<<
    Cleaner() {
        std::string temp = GetEnv("TEMP");
        std::string winDir = GetEnv("WINDIR");
        std::string localAppData = GetEnv("LOCALAPPDATA");
        std::string userProfile = GetEnv("USERPROFILE");

        if (!temp.empty()) targets.push_back({temp, false, "Windows User Temp"});
        if (!winDir.empty()) targets.push_back({winDir + "\\Temp", false, "Windows System Temp"});
        if (!localAppData.empty()) {
            targets.push_back({localAppData + "\\npm-cache", false, "npm cache"});
            targets.push_back({localAppData + "\\uv\\cache", false, "uv cache"});
            targets.push_back({localAppData + "\\pip\\Cache", false, "pip cache"});
            targets.push_back({localAppData + "\\Yarn\\Cache", false, "Yarn cache"});
        }
        if (!userProfile.empty()) {
            targets.push_back({userProfile + "\\.cache", false, ".cache folder"});
            targets.push_back({userProfile + "\\.npm", false, ".npm folder"});
        }
    }
====
    Cleaner() {
        std::string temp = GetEnv("TEMP");
        std::string winDir = GetEnv("WINDIR");
        std::string localAppData = GetEnv("LOCALAPPDATA");
        std::string userProfile = GetEnv("USERPROFILE");

        if (!temp.empty()) targets.push_back({temp, false, "Windows User Temp"});
        if (!winDir.empty()) targets.push_back({winDir + "\\Temp", false, "Windows System Temp"});
        if (!localAppData.empty()) {
            targets.push_back({localAppData + "\\npm-cache", false, "npm cache"});
            targets.push_back({localAppData + "\\uv\\cache", false, "uv cache"});
            targets.push_back({localAppData + "\\pip\\Cache", false, "pip cache"});
            targets.push_back({localAppData + "\\Yarn\\Cache", false, "Yarn cache"});
        }
        if (!userProfile.empty()) {
            targets.push_back({userProfile + "\\.cache", false, ".cache folder"});
            targets.push_back({userProfile + "\\.npm", false, ".npm folder"});
            // Deep targets (recursive)
            targets.push_back({userProfile, true, "User Profile Project Caches"});
        }
    }
>>>>
```

- [ ] **Step 2: Update target names in Scan and Clean methods**

Update `Scan()` and `Clean()` to include the new folder names.

```cpp
<<<<
                std::vector<std::string> targetNames = {"node_modules", "__pycache__", ".pytest_cache"};
====
                std::vector<std::string> targetNames = {
                    "node_modules", "venv", ".venv", "env", "__pycache__", 
                    ".pytest_cache", ".next", ".nuxt", ".cache", ".sass-cache", 
                    "dist", "build"
                };
>>>>
```

- [ ] **Step 3: Commit changes**

```bash
git add Cleaner.hpp
git commit -m "feat: add deep recursive targets for project caches"
```

---

### Task 2: Build and Verify

**Files:**
- Run: `build.bat`

- [ ] **Step 1: Run build script**

Run: `.\build.bat`
Expected: "Build successful!" and `gemini-sys-cleaner.exe` is updated.

- [ ] **Step 2: Verify executable exists**

Run: `dir gemini-sys-cleaner.exe`
Expected: File exists and timestamp is current.

---

### Task 3: Execution - Deep Scan and Clean

**Files:**
- Run: `gemini-sys-cleaner.exe`

- [ ] **Step 1: Perform Deep Scan**

Run: `.\gemini-sys-cleaner.exe scan`
Expected: Output showing sizes of global caches AND recursive project caches.

- [ ] **Step 2: Perform Deep Clean**

Run: `.\gemini-sys-cleaner.exe clean`
Expected: Output showing "Cleanup completed. Total freed: X MB".

- [ ] **Step 3: Verify via Log**

Run: `type gemini-sys-cleaner.log`
Expected: Log entries for deep cleaning steps.
