# Changelog

All notable changes to the system-cleaner-agent project will be documented in this file.

## [5.6.3] - 2026-07-28

### Fixed
- **TUI Logo at Bottom of Screen (`include/TUI.hpp`)**: The ASCII banner
  was being rendered as a single concatenated line (because each `ss << "...";`
  statement in `BuildBannerString` was missing a trailing newline). This caused
  the top-banner newline counter (`topLines`) to stay at 0, so the menu box
  was positioned at row 1 and overwrote the banner. Fix: appended `\n` to
  every banner line in `BuildBannerString()` so the banner spans 11+ rows
  and the menu box correctly starts BELOW the banner.
- **TUI Live Refresh Left Artifacts (`include/OpenTUI.hpp`)**: The auto-refresh
  re-render loop was only clearing the screen ONCE at startup. On subsequent
  refresh ticks the previous frame's menu box content remained on screen
  (causing ghosting / partial-render tearing). Fix: emit `\033[2J\033[H`
  (clear screen + home) at the start of EVERY re-render so each live frame
  is a clean atomic snapshot.
- **TUI Stale Crash on stdin Close (`include/OpenTUI.hpp`)**: The TUI
  `Menu::ShowExtended` now uses a `firstRender` guard so the initial
  `TerminalEngine::ClearScreen()` is only called on the first iteration,
  preventing redundant screen-clears on every keypress.

### Added
- **Scratch Folder + .gitignore (`scratch/`, `.gitignore`)**: Created a new
  `scratch/` directory at the project root for temporary Python helper
  scripts (TUI render tests, banner verification, etc.). Added `scratch/`
  plus additional editor/IDE file patterns (`.vscode/`, `.idea/`, `*.tmp`,
  `*.bak`, `*.swp`, `*~`, `.DS_Store`) to `.gitignore` so they never
  accidentally get committed.

## [5.6.2] - 2026-07-28

### Added / Features
- **TUI Top-Banner Layout (`include/OpenTUI.hpp`, `include/TUI.hpp`)**: Replaced the legacy
  `SetPreRenderCallback` banner injection (which overwrote/displaced the menu box) with a
  new `Menu::SetTopBanner(std::function<std::string()>)` API. The banner is now drawn
  ABOVE the menu box at the top of the screen, and the menu starts below the banner
  using a calculated vertical offset -- fixing the "logo drawn at bottom" rendering bug.
  Applied to all 4 submenus (main menu, Disk Cleaner, Deep Scan, Task Actions).
- **TUI Live Auto-Refresh (`include/OpenTUI.hpp`, `include/TUI.hpp`)**: Added
  `Menu::SetRefreshIntervalMs(int)` which re-renders the menu frame (including the live
  RAM/Disk header stats) on a polling interval without requiring user input. The main
  menu now refreshes every `monitorIntervalSec` seconds (set in Settings) so the live
  RAM/Disk usage bars update in real-time instead of being frozen at the moment the menu
  was last re-rendered after a key press.
- **New "Manual Process List & Hot-Key Terminate" Option (`include/TUI.hpp`)**: Added to the
  RAM Cleaner submenu. Lists every process currently using > 200 MB RAM in a navigable
  selector with two kill paths:
    - **[K] hot-key** = INSTANT KILL of the highlighted process (no extra confirm prompt)
    - **[ENTER]** = Kill with y/N confirmation prompt
  Other hot-keys: [UP/DOWN] navigate, [Q]/[ESC] cancel. Each row shows PID, process name,
  RAM usage and a `[PROTECTED]` / `[CAN KILL]` status badge. The list auto-refreshes after
  each kill so the displayed bloat sizes stay accurate.
- **Deep Scan Option in Disk Cleaner (`include/TUI.hpp`)**: Added a new "Deep Scan
  (Interactive Tree, Hotspot Analyzer & JSON Export)" entry to the Disk Cleaner
  submenu (sel == 9) that delegates to the full `ShowDeepScanSubmenu` interface.

### Changed
- **ASCII Logo Redesign -- Now Text-Free (`include/TUI.hpp`)**: Replaced the previous
  "SYSTEM-CLEANER-AGENT" text banner with a clean, two-badge icon-style design (corner
  accents + central shield-and-sweep motif) that visually matches the application
  icon. The new `TUI::BuildBannerString()` returns the logo as a string buffer
  suitable for the new `Menu::SetTopBanner` layout.

### Fixed
- **Pre-existing Duplicate `sel == 3` Bug (`include/TUI.hpp`)**: Two consecutive
  `else if (sel == 3)` blocks in `ShowDiskCleanerSubmenu` caused the "Preset: Crash
  Dumps & Logs" entry (originally index 4) to be silently unreachable. Renamed the
  second duplicate to `sel == 4` so the "Crash Dumps" preset is now correctly
  selectable.

## [5.6.1] - 2026-07-28

### Fixed
- **TUI Live Monitor Refresh (`include/TUI.hpp`)**: The System Resource & Drive Monitor screen no longer flickers or renders artifacts. Previously `PrintBanner()` was called before the stats were assembled, then `MoveCursorToHome()` caused the monitor box to overwrite the banner. The fix assembles the full frame (banner + stats box) in a single `ostringstream` and flushes it atomically after clear+home, eliminating all partial-render tearing.
- **Log File Toggle (`include/Logger.hpp`, `include/TUI.hpp`)**: Turning "Logs File Output" OFF in Settings now actually stops writing to `system-cleaner-agent.log`. Previously toggling `g_tuiSettings.enableLogging` only changed the UI label but `Logger::Log()` continued writing unconditionally. Added `SetFileLogging(bool)` gate to `Logger` and wired it into the settings toggle and startup config load.

## [5.6.0] - 2026-07-28


### Added / Features
- **Deep Disk Scan Engine (`include/DeepScanner.hpp`)**:
  - Cloned deep scan architecture inspired by `dust` (bootandy/dust).
  - Recursive directory tree traversal with cumulative size aggregation per node.
  - Top-N largest dirs/files ranking and visual size bars (`████████░░ 18.25 MB 100.0%`).
  - Filtering by `--min-size <bytes>`, `--depth <N>`, `--exclude-ext <exts>`.
  - JSON export report support (`ExportToJson`).
  - Cross-platform storage queries using `GetDiskFreeSpaceExW` (Win) / `statvfs` (Linux/macOS).
- **Deep Memory Scan & Process Timeline (`include/gtlibc.hpp`, `include/gtlibc.cpp`)**:
  - Full per-PID memory breakdown (Heap/Private, Stack, Mapped Files, Shared Memory).
  - Top-N RAM consumers sorted by Working Set.
  - Circular buffer memory history tracking (last 10 snapshots per PID).
  - Memory leak candidate detection for processes growing >10% RAM across scan intervals.
- **Multi-Drive Parallel Scan (`include/SmartScheduler.hpp`)**:
  - Simultaneous multi-drive scanning via `std::thread` per drive.
  - duf-style disk usage bars (`[███████████████████.] 99.6%`).
  - Warning badges (`[WARNING: LOW DISK SPACE > 90%]`) for drives exceeding configurable threshold.
- **Interactive Deep Scan TUI (`include/TUI.hpp`)**:
  - Added "Deep Scan" menu item to main menu.
  - Interactive Tree View Explorer with key navigation (Arrow keys expand/collapse, `D` schedule clean, `E` export JSON).
  - Non-blocking background thread with live OpenTUI braille spinner animation (`SpinnerAnimation`).
- **Task History JSON Persistence & Auto-Resume (`include/TaskHistory.hpp`)**:
  - Automatically saves task library state to `cleaner_tasks.json` on task registration, progress updates, completion, or pause/resume.
  - Automatically loads and resumes unfinished tasks (`Running`/`Queued`/`Paused`) with `[RESUMED ON STARTUP]` status tags on app restart.
- **Unit Test Expansion (`tests/test_main.cpp`)**:
  - Added `TestDeepScanner` (Test #26) and `TestTaskHistoryPersistenceAndResume` (Test #27) (283 total assertions across 27 test suites, 100% pass rate).

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
