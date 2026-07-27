# Changelog

All notable changes to the system-cleaner-agent project will be documented in this file.

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
