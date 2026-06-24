# System Cleaning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Modify the system cleaner to target only global caches and perform a scan followed by a cleanup.

**Architecture:** Use the existing C++ `gemini-sys-cleaner` tool. Modify its configuration to exclude recursive project cache searches.

**Tech Stack:** C++, Batch (build.bat)

---

### Task 1: Scoping - Remove Project Cache Targets

**Files:**
- Modify: `Cleaner.hpp`

- [ ] **Step 1: Modify Cleaner.hpp constructor**

Update the `Cleaner()` constructor to remove the "Project caches" target.

```cpp
<<<<
        if (!userProfile.empty()) {
            targets.push_back({userProfile + "\\.cache", false, ".cache folder"});
            targets.push_back({userProfile + "\\.npm", false, ".npm folder"});
            targets.push_back({userProfile, true, "Project caches"});
        }
====
        if (!userProfile.empty()) {
            targets.push_back({userProfile + "\\.cache", false, ".cache folder"});
            targets.push_back({userProfile + "\\.npm", false, ".npm folder"});
        }
>>>>
```

- [ ] **Step 2: Commit changes**

```bash
git add Cleaner.hpp
git commit -m "config: remove recursive project cache targets from cleaner"
```

---

### Task 2: Build - Compile the Cleaner

**Files:**
- Run: `build.bat`

- [ ] **Step 1: Run build script**

Run: `.\build.bat`
Expected: "Build successful!" and `gemini-sys-cleaner.exe` exists in the root.

- [ ] **Step 2: Verify executable exists**

Run: `dir gemini-sys-cleaner.exe`
Expected: File exists.

---

### Task 3: Execution - Scan and Clean

**Files:**
- Run: `gemini-sys-cleaner.exe`

- [ ] **Step 1: Perform Scan**

Run: `.\gemini-sys-cleaner.exe scan`
Expected: Output showing sizes of npm, pip, and temp folders.

- [ ] **Step 2: Log Scan Results**

(I will manually read the output and confirm the total size with the user before cleaning, but for this plan, we proceed to clean as the user said "do it").

- [ ] **Step 3: Perform Clean**

Run: `.\gemini-sys-cleaner.exe clean`
Expected: Output showing "Cleanup completed. Total freed: X MB".

- [ ] **Step 4: Verify Cleanup via Log**

Run: `type gemini-sys-cleaner.log`
Expected: Logs showing the cleanup process and total freed space.
