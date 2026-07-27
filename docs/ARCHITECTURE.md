# system-cleaner-agent Architecture Specification

This document details the modular C++17 architecture and ReAct autonomous agent trajectory of **system-cleaner-agent**.

```
                           ┌───────────────────────────┐
                           │   system-cleaner-agent    │
                           │   C++17 CLI / TUI / ReAct │
                           └─────────────┬─────────────┘
                                         │
                 ┌───────────────────────┼───────────────────────┐
                 │                       │                       │
     ┌───────────▼───────────┐ ┌─────────▼───────────┐ ┌─────────▼───────────┐
     │   AgentEngine (ReAct) │ │ ContentInspector    │ │  ProcessManager     │
     │ Reason-Act-Observe    │ │ Magic Bytes & Shield│ │ Handle Lock Release │
     └───────────┬───────────┘ └─────────┬───────────┘ └─────────┬───────────┘
                 │                       │                       │
                 └───────────────────────┼───────────────────────┘
                                         │
                               ┌─────────▼───────────┐
                               │ Cleaner Core Engine │
                               │ Multi-Thread Async  │
                               └─────────────────────┘
```

## Module Specifications

### 1. `AgentEngine.hpp`
- Implements the **ReAct (Reasoning + Action + Observation)** autonomous state machine.
- Steps:
  1. `Thought`: Analyzes system storage state and user-defined goal.
  2. `Action`: Triggers modular engine tools (`SCAN_SYSTEM_TARGETS`, `RELEASE_PROCESS_LOCKS`, `INSPECT_FILE_MAGIC_BYTES`, `EXECUTE_PARALLEL_CLEANUP`).
  3. `Observation`: Evaluates freed storage, handles errors, and feeds observation back into the next iteration.

### 2. `ContentInspector.hpp`
- **Magic Bytes Inspector**: Validates magic signatures (`%PDF`, `\x89PNG`, `SQLite format 3`, `MZ`, `ELF`).
- **User Safety Shield**: Preserves source code (`.py`, `.cpp`, `.js`, `.ts`), documents (`.pdf`, `.docx`), databases (`.sqlite`, `.db`), and key assets.
- **SHA-1 / SHA-256 Hex Hash Detector**: Identifies 40-character and 64-character hash cache blobs.

### 3. `ProcessManager.hpp`
- Uses Win32 Toolhelp32 snapshot API on Windows and `/proc` process scanner on POSIX/Linux.
- Terminates background lock handles (`mintty`, `cat`, `bash`, `werfault`) locking temporary directories.

### 4. `Cleaner.hpp`
- Multi-threaded parallel scanner using `std::async` and `std::hardware_concurrency`.
- $O(N)$ non-blocking directory cleanup with reparse point symlink protection.
- Windows Shell API `SHEmptyRecycleBinW` integration.

### 5. `TUI.hpp`
- Interactive terminal UI menu rendering ANSI color layouts and diagnostic menus.
