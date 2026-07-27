# Changelog

All notable changes to the Gemini System Cleaner project will be documented in this file.

## [3.5.0] - 2026-07-27

### Added
- **Terminal User Interface (TUI)**: Interactive terminal menu (`gemini-sys-cleaner tui`) with operations menu.
- **Unit Test & TDD Regression Suite**: Automated test suite (`unit_tests.exe` and `gemini-sys-cleaner test`) covering content protection, hex hash detectors, duration/size parsers, and size formatters.
- **ASCII Art Banner**: Stylish high-resolution ASCII logo header.
- **Ultra-Fast $O(N)$ Non-Blocking Deletion Engine**: Prevents directory traversal loops and exponential recursion hangs.

### Changed
- Refactored project structure into modular headers under `include/` (`Cleaner.hpp`, `ContentInspector.hpp`, `ProcessManager.hpp`, `Logger.hpp`, `TUI.hpp`).
- Updated `CMakeLists.txt` to integrate `unit_tests` executable and CTest runner.

---

## [3.0.0] - 2026-07-27

### Added
- Granular CLI suite with WHAT, WHERE, and HOW flags (`--category`, `--path`, `--drive`, `--older-than`, `--min-size`, `--mode shred`, `--json-report`).
- Hex-hash filename detector for 40/64 character SHA cache blobs.
- Multi-threaded parallel target scanning and cleaning using `std::async`.

---

## [2.0.0] - 2026-07-27

### Added
- Smart File Content Inspection engine with magic byte checks (`%PDF`, `PNG`, `JPEG`, `SQLite format 3`).
- Win32 Toolhelp32 process lock manager for releasing background handles (`mintty`, `cat`, `bash`, `werfault`).
- Windows Shell API `SHEmptyRecycleBinW` integration.

---

## [1.0.0] - Initial Release
- Basic Python and C++ system temp cleaner script.
