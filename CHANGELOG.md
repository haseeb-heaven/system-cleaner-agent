# Changelog

All notable changes to the system-cleaner-agent project will be documented in this file.

## [5.7.3] - 2026-07-28

### Changed
- **Static CRT MultiThreaded Linking (`CMakeLists.txt`)**:
  Enforced static C++ runtime linking (`/MT`) via `CMAKE_MSVC_RUNTIME_LIBRARY` and `CMP0091`. All Visual C++ runtime functions are statically embedded into `system-cleaner-agent.exe` so the executable requires zero external DLLs and runs on any clean Windows machine without needing VC++ Redistributable installed.

### Test Results
- 27 Test Suites | 292 Assertions | 0 Failures | 100% Pass Rate

## [5.7.2] - 2026-07-28

### Fixed
- **Interactive Console Input Fix (`include/OpenTUI.hpp`)**:
  Added `GetConsoleMode()` check in `TerminalEngine::IsInteractiveConsole()`. Distinguishes interactive PowerShell/CMD/Windows Terminal/double-click Explorer sessions from redirected subprocess pipes so that double-clicking the executable or running it directly from PowerShell launches OpenTUI smoothly without exiting.

### Test Results
- 27 Test Suites | 292 Assertions | 0 Failures | 100% Pass Rate

## [5.7.1] - 2026-07-28

### Added
- **Dedicated Command Specification Guide (`COMMANDS.md`)**:
  Created a standalone document for complete Agent Query Language (AQL) syntax, command verbs, `WHERE` operators, target path aliases, cron-style daemon jobs, and CLI flag references.
- **Windows Piped Input Support (`include/OpenTUI.hpp`)**:
  Added `PeekNamedPipe` and `_isatty(_fileno(stdin))` check in `TerminalEngine::HasKeyPending()` & `ReadKey()` to support automated stdin key input scripts on Windows.
- **Security Shield & Path Protection Notice (`README.md`)**:
  Added explicit callouts and safety recommendations for keeping Path Protection Guard and Sandbox Dry-Run Mode enabled.

### Changed
- **Streamlined README Documentation**:
  Refactored `README.md` to be clean, professional, and concise, linking directly to `COMMANDS.md`, `CHANGELOG.md`, and `docs/ARCHITECTURE.md`.
- **Updated SVG Screenshots**:
  Re-captured and generated full-frame SVG screenshots for Main Menu, Disk Cleaner Suite, RAM Cleaner, Task Library, Settings, and CLI subcommands.

### Test Results
- 27 Test Suites | 292 Assertions | 0 Failures | 100% Pass Rate

## [5.7.0] - 2026-07-28

### Changed
- **Main Menu Restructured: "Deep Scan" Merged into "Disk Cleaner"**:
  The "Deep Scan" option has been removed from the top-level main menu and is now
  only accessible as option 9 inside the "Disk Cleaner" submenu. The main menu
  now has 7 options (down from 8): Disk Cleaner, RAM Cleaner, AQL Query,
  Daemon Monitor, Task Library, Settings, Exit.
  The "Disk Cleaner" option is now labeled "Disk Cleaner (with Deep Scan)" to make
  the merged functionality discoverable.

### Fixed
- **ESC Key Now Works in All Submenus (`include/TUI.hpp`)**:
  Previously, pressing ESC in the "Disk Cleaner" submenu or the "AQL Query"
  submenu did not return to the main menu correctly:
    - **Disk Cleaner**: The "Back" option (index 11) was not handled. Added
      `if (sel == -1 || sel == 11) return;` after `subMenu.Show()` to return to
      the main menu when ESC is pressed or "Back" is selected.
    - **AQL Query**: ESC was entering the custom query input loop (forcing the
      user to type an empty query to go back). Now ESC immediately returns empty
      string to the main menu: `if (choice == -1) return "";`
    - **RAM Cleaner / Daemon Monitor / Deep Scan / Task Library**: Already worked
      correctly with ESC (the `if (sel == -1 || sel == N) break;` checks were present).
  Result: All submenus now consistently use ESC (or "Back" option) to return to
  the main menu, and ESC from the main menu exits the application.

### Test Results
- 27 Test Suites | 292 Assertions | 0 Failures | 100% Pass Rate

## [5.6.7] - 2026-07-28

### Added
- **CPU Usage Live Monitoring (`include/SmartScheduler.hpp`)**:
  New `SmartScheduler::GetCpuUsagePercent()` function that samples CPU
  usage at most every 500ms for accuracy. On Windows it uses
  `GetSystemTimes()` to read kernel/user/idle times. On POSIX it reads
  `/proc/stat` for the aggregate `cpu` line. Returns 0.0-100.0 percentage.
- **Color-Coded Progress Bars (`include/OpenTUI.hpp`)**:
  New `OpenTUI::RenderColoredBar()` function renders a progress bar with
  color based on usage level: green (< 60%), yellow (60-85%), red (> 85%).
  Uses Unicode block characters for a modern look.
- **Sparkline / Mini-Chart Widget (`include/OpenTUI.hpp`)**:
  New `OpenTUI::Sparkline::Render()` function renders a compact trend
  chart from a series of values using Unicode block characters.
  Auto-downsamples if more values than width. Color-coded per-character
  based on current value.
- **Resource History Tracking (`include/TUI.hpp`)**:
  New `TUI::ResourceHistory` struct stores last 30 samples of CPU,
  RAM, and disk usage. Used by sparkline widgets to show live trends.
- **Enhanced Live Resource Headers (`include/TUI.hpp`)**:
  `GetLiveResourceHeaders()` now shows:
    - **CPU** with color-coded progress bar + sparkline trend (14 chars)
    - **RAM** with color-coded progress bar + sparkline trend (14 chars)
    - **Each drive** with color-coded progress bar + free/total info
    - **TREND** line showing disk usage history (20 chars sparkline)
    - **Tasks** status (running count, active task name)
  The headers update automatically based on the `monitorIntervalSec`
  setting (3s/5s/10s/15s/30s/60s in Settings).

### Changed
- **ShowSystemResourceMonitor (`include/TUI.hpp`)**: Now shows CPU usage
  in addition to RAM, with color-coded progress bars and sparkline
  trend charts for both metrics. Each drive is shown with a color-coded
  progress bar and free/total size info.

## [5.6.6] - 2026-07-28

### Fixed
- **TUI Main Menu Cursor Not Moving (`include/OpenTUI.hpp`, `include/TUI.hpp`)**:
  Pressing arrow keys in the main menu did not move the cursor - it stayed
  at position 0 and always selected "Deep Scan" (the first option).
  The Deep Scan submenu worked correctly because it does not use auto-refresh.
  Root cause: The Windows `ReadKey()` function only handled Windows extended
  key codes (0x00 or 0xE0 prefix + arrow code), but some terminal
  configurations send arrow keys as ANSI escape sequences (ESC + [ + A/B/C/D).
  When that happened, the first byte (ESC = 27) was interpreted as the
  Escape key, causing the TUI to return -1 and exit or re-render without
  processing the arrow key. Additionally, `FlushInputBuffer()` was being
  called in the main menu loop which could eat the user's keypresses.
  Fixes:
    1. Updated Windows `ReadKey()` to handle ANSI escape sequences. When
       byte 27 (ESC) is received, it peeks the next two bytes and maps
       `[A`/`[B`/`[C`/`[D` to Up/Down/Right/Left respectively. Standalone
       ESC (not followed by `[` or `O`) is still treated as Escape key.
    2. Removed `FlushInputBuffer()` call from the main menu loop. The
       startup flush in `Menu::ShowExtended()` is sufficient - flushing
       between menu renders was eating the user's keypresses.
    3. Reduced auto-refresh loop sleep from 50ms to 10ms for better
       keypress responsiveness (less latency between keypress and render).

## [5.6.5] - 2026-07-28

### Fixed
- **TUI Immediate Exit on Stray stdin Characters (`include/OpenTUI.hpp`, `include/TUI.hpp`)**:
  The TUI was immediately exiting or auto-selecting the first option on
  startup because stray characters (\r, \n, ESC, etc.) in the stdin buffer
  were being consumed as real keypresses. On Windows, `_kbhit()` can return
  true for leftover terminal characters from the previous command or shell
  state. When the auto-refresh loop in `Menu::ShowExtended()` read these
  stray characters, they were mapped to `Key::Enter` (\r = 13) or
  `Key::Escape` (\x1B = 27), causing the TUI to either auto-select the
  first option or print "Exiting system-cleaner-agent. Goodbye!" and exit.
  Fix:
    1. Added a startup flush loop in `Menu::ShowExtended()` that discards
       up to 256 stray characters from stdin before the menu is shown.
    2. Added `FlushInputBuffer()` call at the start of each main menu loop
       iteration in `TUI::RunInteractiveMenu()` to discard stray chars
       that may have accumulated between renders.
    3. Added an unknown-key filter in the auto-refresh polling loop so that
       if a stray character IS read (default `Key::Unknown`), it is
       discarded and the loop continues waiting for a real keypress
       instead of breaking out and processing it as a menu action.
  Result: The TUI now stays open reliably regardless of stdin buffer state.

## [5.6.4] - 2026-07-28

### Removed
- **ASCII Text Logo Completely Removed (`include/TUI.hpp`, `include/OpenTUI.hpp`, `main.cpp`)**:
  The old ASCII text banner that was being drawn above the menu box has been
  removed entirely. The application still ships the image icon
  (`resources/app_icon.ico` and `resources/app_icon.rc`) which is embedded
  into the Windows .exe via the resource compiler -- only the terminal
  ASCII art was removed. Changes:
    - Removed `TUI::BuildBannerString()` inline implementation (the 2-badge icon-style ASCII art)
    - Made `TUI::PrintBanner()` a no-op (kept symbol for ABI compatibility)
    - Removed all 4 `Menu::SetTopBanner()` calls in TUI.hpp
    - Removed all 11 `PrintBanner()` calls in TUI.hpp
    - Removed the inline ASCII banner in `ShowSystemResourceMonitor()`
    - Removed `topBannerCallback` member and `SetTopBanner()` method from `OpenTUI::Menu`
    - Removed the top-banner render path from `Menu::ShowExtended()` (banner setup, per-frame refresh, cursor positioning)
    - Removed `TUI::PrintBanner()` call in `main.cpp` history command
  Result: The TUI now renders directly without any ASCII art, the menu box
  appears immediately after the TermOx ribbon, and the `help` command output
  no longer has a banner before the USAGE section.
- **Scratch Test Scripts Cleaned (`scratch/`)**: All logo-related test scripts
  have been removed. The `scratch/` folder is in `.gitignore` so future
  temp scripts never get committed.

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
