# Changelog

All notable changes to the system-cleaner-agent project will be documented in this file.

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
