# Changelog

All notable changes to the system-cleaner-agent project will be documented in this file.

## [5.5.0] - 2026-07-27

### Fixed
- **AQL Help Intercept (`include/TUI.hpp`)**:
  - Typing `help`, `?`, or `h` in the query input box no longer launches an `AgentEngine` task with the literal string "help".
  - Help screen is now re-shown and the user is re-prompted until a real query is entered or ESC is pressed.
  - Added safety guard in `case 5` dispatch to skip empty or help-only queries before launching background thread.

### Added / Features
- **Boxed AQL Help Screen (`include/TUI.hpp`)**:
  - Replaced plain-text AQL help with a rich, boxed, color-coded reference panel showing:
    - COMMANDS section: CLEAN, SCAN, SHRED, KILL, MONITOR, PURGE with descriptions.
    - CONDITIONS section: FREE_DISK, RAM, SIZE, AGE, EXT IN with examples.
    - EXAMPLES section: 6 ready-to-use example queries.
    - NATURAL LANGUAGE section: Freeform task descriptions for the ReAct agent.
    - Footer tip: TAB autocomplete · ESC cancel · 'help' re-shows help.
- **Task Library Live Dashboard Overhaul (`include/TUI.hpp`)**:
  - Complete visual rewrite of `ShowTaskListView`:
    - Boxed `╔╠╚` frame with header showing current filter label.
    - Stats bar: `● N done  ⠋ N running  ✗ N failed  freed: X  total: N`.
    - Animated Braille spinner (`⠋⠙⠹⠸⠼`) for running tasks — spins on each 2s refresh.
    - Inline `█░` progress bar for tasks with percent tracking.
    - `↓ freed` / `N files` / `N procs` inline suffixes for completed tasks.
    - Color-coded status badges, dim category, bold task name, cyan detail columns.

## [5.4.0] - 2026-07-27

### Fixed
- **Security Guard False Block (`include/SecurityGuard.hpp`)**:
  - `AppData\Local\Temp` and other standard temp/cache paths were incorrectly classified as `CRITICAL SYSTEM PATH BLOCKED`.
  - Added `GetSafeCleanPaths()` allowlist that is evaluated **before** any critical path check, permanently whitelisting known-safe temp/cache/junk directories.

### Added / Features
- **OS-Aware Safe Default Clean Paths (`include/SecurityGuard.hpp`)**:
  - **Windows**: `%USERPROFILE%\AppData\Local\Temp`, `%WINDIR%\Temp`, `%WINDIR%\Prefetch`, `%WINDIR%\SoftwareDistribution\Download`, Chrome/Edge/Firefox caches, npm/pip/nuget caches, WER dumps, INetCache, CrashDumps, Packages.
  - **macOS**: `~/Library/Caches`, `~/Library/Logs`, `~/Library/Saved Application State`, `~/.Trash`, `/private/tmp`, `/private/var/folders`, Spotlight cache, pip/npm caches.
  - **Linux**: `~/.cache`, `~/.local/share/Trash`, `~/.thumbnails`, `/tmp`, `/var/tmp`, `/var/cache/apt`, `/var/cache/yum`, `/var/cache/dnf`, `/var/cache/pacman/pkg`, Gradle/Maven/Cargo/Docker/Yarn caches.
- **TUI Settings: "Reset to OS Defaults" Option (`include/TUI.hpp`)**:
  - New Settings menu item: `Reset to OS Defaults (Windows|macOS|Linux safe temp/cache paths)`.
  - One-press resets Target Folders to the full OS-aware safe path list and saves to `cleaner_config.json`.
  - Target Folders display now truncated to 60 chars to avoid wrapping.
- **Auto-Init on First Launch (`include/TUI.hpp`)**:
  - If `cleaner_config.json` is missing or has an empty/legacy path, the app automatically populates the full OS-aware safe clean path list and saves it.
  - Custom protected processes from config are restored into the GTLibc runtime list on startup.

## [5.3.1] - 2026-07-27

### Added / Features
- **Compact ASCII Diagram Shield Logo (`include/TUI.hpp`)**:
  - Replaced oversized ASCII text with a high-tech compact ASCII Diagram Shield Emblem (`_/_\_`) & emblem header (`SYSTEM-CLEANER-AGENT v5.3`).
  - Fits perfectly on standard 80-column terminal screens without line wrapping.
- **Cross-Platform Operating System Support (`include/gtlibc.cpp`)**:
  - Enhanced POSIX process enumeration and memory tracking via `/proc` filesystem (`/proc/[pid]/comm` and `/proc/[pid]/statm`) for Linux and macOS.
  - Ensures full cross-platform compatibility across Windows, Linux, and macOS.

## [5.3.0] - 2026-07-27

### Added / Features
- **Persistent Configuration Engine (`include/ConfigManager.hpp`)**:
  - Implemented `ConfigManager` to load and save persistent application settings to `cleaner_config.json`.
  - Automatically loads and persists all TUI settings (`sandboxMode`, `pathProtection`, `dryRun`, `killLocks`, `monitorIntervalSec`, `ramThresholdMB`, `customPathsStr`, `customProtectedProcesses`).
  - Restores custom process protection whitelist entries across application launches.
  - Automatically saves settings whenever configured via TUI Settings or RAM Cleaner whitelist option.
- **Unit Test Suite Expansion (`tests/test_main.cpp`)**:
  - Added Test 22 (`TestConfigManager`) covering JSON configuration persistence and validation (22 test functions, 207 assertions, 100% pass rate).

## [5.2.0] - 2026-07-27

### Added / Features
- **Comprehensive Process Protection Whitelist (`include/gtlibc.cpp`, `include/gtlibc.hpp`)**:
  - Implemented `GTLibc::IsProtectedProcess()`, `GTLibc::AddCustomProtectedProcess()`, and `GTLibc::GetHighMemoryCandidateProcesses()`.
  - Added protection for OS Services, Browsers (Chrome, Edge, Firefox), IDEs (VS Code, Cursor, Visual Studio), Terminals (PowerShell, CMD, WT, Bash), Apps (Discord, Slack, Telegram, Spotify), and AI Agent CLI Tools (`system-cleaner-agent`, `agy`, `antigravity`, `gemini-cli`, `cline`, `grok`).
- **Aggregated Multi-Process RAM Tracker (`GTLibc::GetAggregatedProcessGroups`)**:
  - Added memory aggregation by process name for multi-process applications (Chrome, Edge, VS Code), displaying total combined RAM (e.g. `chrome.exe (21 procs) - RAM: 1.14 GB`).
- **Interactive TUI Process Permission & Termination Engine (`include/TUI.hpp`)**:
  - Displays high-RAM applications (> 200 MB Total RAM) sorted by memory usage.
  - Allows selecting process applications for termination with explicit `y/N` user permission confirmation.
  - Integrated `OpenTUI::TextInput::ReadLine` to eliminate Windows raw console input freezing.
  - Updated `GTLibc::KillProcessByName` to support explicit user permission overrides (`userPermissionGranted = true`).
  - Reused existing `g_tuiSettings.monitorIntervalSec` setting for snapshot refresh intervals.

## [5.1.0] - 2026-07-27

### Added / Features
- **Agent Query Language (AQL) Engine (`include/AgentQueryLanguage.hpp`)**:
  - Implemented SQL-style query syntax (`CLEAN`, `SCAN`, `SHRED`, `MONITOR`, `PURGE`, `KILL`).
  - Added support for free disk queries (`WHERE FREE_DISK < 500MB`), RAM percentage cutoffs (`WHERE RAM > 70%`), file size filters (`WHERE SIZE > 10MB`), and file age (`WHERE AGE > 24H`).
  - Added AQL syntax validator (`AQLEngine::Validate()`) with real-time error messages and suggested syntax hints.
- **`GTLibc` Process Management Subsystem (`include/gtlibc.hpp`, `include/gtlibc.cpp`)**:
  - Ported and integrated focused Win32 process management library from `haseeb-heaven/GTLibCpp` (`FindProcess`, `IsProcessRunning`, `EnumerateAllProcesses`, `GetProcessMemoryUsage`, `KillProcess`, `KillProcessByName`, `KillHighMemoryProcesses`, `IsElevatedProcess`).
  - Updated `ProcessManager.hpp` to delegate handle unlocking and RAM process scanning directly to `GTLibc`.
- **Advanced Extended ASCII OpenTUI Suite (`include/OpenTUI.hpp`, `include/TUI.hpp`)**:
  - Upgraded OpenTUI box drawing using Unicode double-line frames (`╔ ═ ╗ ║ ╚ ╝`).
  - Implemented Extended ASCII block loading bars (`█ ░`).
  - Added live animated braille spinners (`⠋ ⠙ ⠹`) for background task feedback.
  - Added live System Resource Header Panel showing real-time RAM usage and storage capacity across all system drives (`C:\`, `D:\`, etc.).
  - Added dedicated **RAM Cleaner** menu option in OpenTUI Dashboard to release process locks and terminate dead background processes using `GTLibc`.
  - Added dedicated **System Resource Monitor** screen with configurable refresh intervals (3s - 60s).
  - Added non-blocking background thread task execution: OpenTUI Dashboard remains 100% active, visible, and selectable during operations.
  - Added `OpenTUI::TextInput` with TAB key ghost text autocomplete and backspace support.
  - Added `Settings` menu supporting Left/Right (`◄ ►`) arrow keys for instant setting toggles (`Sandbox Mode`, `Dry-Run Mode`, `Path Guard`, `Monitor Interval`, `Target PATH Folders`).

### Fixed / Improved
- **Border Alignment**: Fixed 2-column wide UTF-8 character padding (`U+26AA` `⚪` and 4-byte Emojis) and ANSI escape sequences using `GetVisibleDisplayWidth()`. Added `TruncateVisibleText()` so long status lines never overflow box borders.
- **Drive Usage Labels**: Clarified progress bar labels to explicitly display `% used` (e.g. `99.9% used (Free: 175.91 MB / 153.91 GB)`).
- **PowerShell Escaped Quote Bug**: Resolved `CommandLineToArgvW` quote escaping issue for drive paths with trailing backslashes (`--monitor-drive "C:\"`).
- **Unit Test Suite**: Expanded from 11 to 21 test functions (198 assertions, 0 failures, 100% pass rate).

---

## [5.0.0] - 2026-07-27

### Changed / Migrated
- **100% Pure Native C++ Migration**:
  - Removed all legacy Python scripts (`scanner.py`, `sys_cleaner.py`, `requirements.txt`).
  - System is now 100% written in ISO C++17 with zero external runtime dependencies.
- **OpenTUI Framework (`OpenTUI.hpp`)**:
  - Added zero-dependency cross-platform OpenTUI framework with ANSI escape sequences, arrow-key navigation, and progress indicators.
- **CI/CD Pipeline Update**:
  - Updated `.github/workflows/ci.yml` for multi-platform (Windows, Ubuntu, macOS) C++ CMake build matrix.

---

## [4.0.0] - 2026-07-27
- Added ReAct Autonomous Agent Execution Loop (`AgentEngine.hpp`).
- Added Hex Hash Cache Blob Detector (40/64 char SHA-1/SHA-256).
- Renamed project to `system-cleaner-agent`.

---

## [3.0.0] - 2026-07-27
- Added magic byte inspection, process lock release, and TUI interface.

---

## [1.0.0] - Initial Release
- Basic system cleaner utility.
