# Changelog

All notable changes to the SystemCleanerAgent project will be documented in this file.

## [4.0.0] - 2026-07-27

### Added
- **Autonomous ReAct Agentic Engine (`AgentEngine.hpp`)**:
  - Implemented Reasoning + Action + Observation (ReAct) cycle.
  - Command: `SystemCleanerAgent agent --task "<goal>"`.
- **Project Renaming & Relocation**:
  - Relocated project to `D:\Code\SystemCleanerAgent`.
  - Executable target renamed to `SystemCleanerAgent.exe` and `sys-cleaner-agent.exe`.
- **Expanded Unit Test Suite**:
  - Comprehensive assertions covering ReAct trajectory, magic bytes, hex hash detector, size formatters, and duration/size parsers (`unit_tests.exe`).
- **Interactive TUI ReAct Integration**: Added option `[5]` to run ReAct Agent Execution Loop directly from TUI.

---

## [3.5.0] - 2026-07-27
- Added Terminal User Interface (TUI) and ASCII art banner.
- Non-blocking $O(N)$ linear deletion engine.

---

## [3.0.0] - 2026-07-27
- Added granular CLI suite (`--category`, `--path`, `--drive`, `--older-than`, `--min-size`, `--mode shred`, `--json-report`).
- 40/64 character hex hash cache blob detector.

---

## [2.0.0] - 2026-07-27
- Added magic byte verifier and Win32 process lock manager.

---

## [1.0.0] - Initial Release
- Basic system cleaner utility.
