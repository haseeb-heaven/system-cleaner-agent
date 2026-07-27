```
   _____ _   _ _____ _____ _____ ___  ___ _____ _     _____ ___   _   _ _____ _____ _   _ _____ 
  /  ___| | | /  ___|_   _|  ___|  \/  |/  __ \ |   |  ___/ _ \ | \ | |  ___|  ___| \ | |_   _|
  \ `--.| |_| \ `--.  | | | |__ | .  . || /  \/ |   | |__/ /_\ \|  \| | |__ | |__ |  \| | | |  
   `--. \__  | `--. \ | | |  __|| |\/| || |   | |   |  __|  _  || . ` |  __||  __|| . ` | | |  
  /\__/ / | |/\__/ / | | | |___| |  | || \__/\ |___| |__| | | || |\  | |___| |___| |\  | | |  
  \____/  \_/\____/  \_/ \____/\_|  |_/ \____/\____/\____\_| |_/\_| \_/\____/\____/\_| \_/ \_/  
```

# system-cleaner-agent v4.0.0

> **Autonomous ReAct Agentic Storage Optimization Engine & System Cleanup Framework (C++17 Multi-Platform)**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)
[![Agentic ReAct](https://img.shields.io/badge/architecture-ReAct%20Loop-magenta.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)
[![Language](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Tests](https://img.shields.io/badge/unit--tests-100%25%20passing-success.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)

---

## 🤖 ReAct Autonomous Agent Architecture

`system-cleaner-agent` incorporates an autonomous **Reasoning + Action + Observation (ReAct)** execution loop:

```
┌──────────────────────────────────────────────────────────────────┐
│ 1. THOUGHT     : Reason about drive state, target paths & locks   │
│ 2. ACTION      : Issue atomic tool action (Scan, Release, Inspect)│
│ 3. OBSERVATION : Observe result, measure freed bytes & verify    │
└──────────────────────────────────────────────────────────────────┘
```

Run the agent in autonomous mode:
```powershell
.\system-cleaner-agent.exe agent --task "Perform autonomous full storage optimization"
```

---

## Key Features

- 🧠 **Autonomous ReAct Agent Loop (`AgentEngine.hpp`)**:
  - Cycles through Thought -> Action -> Observation trajectories.
  - Automatically reasons about target locations, releases process lock handles, verifies content magic bytes, and executes non-blocking parallel cleanup.
- 🛡️ **Content Protection Shield & Magic Byte Verifier**:
  - Verifies magic bytes (`%PDF`, `PNG`, `JPEG`, `SQLite format 3`).
  - Never deletes user source code (`.py`, `.cpp`, `.js`), documents, or databases.
  - Detects 40/64-character SHA-1/SHA-256 hex hash cache blobs.
- 🔓 **Process Lock Manager**:
  - Releases process handles (`mintty`, `cat`, `bash`, `werfault`) locking temporary paths.
- 💻 **Terminal User Interface (TUI)**:
  - Interactive terminal menu accessible via `system-cleaner-agent tui` or `-i`.
- 🔒 **Secure Zero-Overwrite Shredding**:
  - Overwrites junk files with binary zeros before deletion (`--mode shred`).
- 🧪 **Unit Test Suite**:
  - Executable test runner `unit_tests.exe` with 100% pass rate across all engine modules.

---

## Build Instructions

### **Windows (MSVC / MinGW / CMake)**:
```cmd
build.bat
```

### **Linux / macOS (GCC / Clang / CMake)**:
```bash
chmod +x build.sh
./build.sh
```

---

## CLI & Agentic Options

```
USAGE:
  system-cleaner-agent <COMMAND> [FLAGS]

COMMANDS:
  agent        Launch Autonomous ReAct Agent Loop (Thought->Action->Observation).
  scan         Analyze system/drive targets and report cleanable storage.
  clean        Execute multi-threaded cleanup using active policy rules.
  deep-clean   Perform full system cache cleanup + empty OS Recycle Bin / Trash.
  tui          Launch interactive Terminal User Interface (TUI).
  test         Execute automated engine diagnostic & unit test suite.
  version      Display version, engine build, and architecture details.
  help         Show this help and usage specification.

AGENTIC REACT OPTIONS:
  --task <goal>        Specify custom natural language goal for ReAct loop.
  --agent              Enable autonomous reasoning and action trajectory.

FILTERING (WHAT):
  --category <list>     Target categories: system, browser, dev, messaging, app.
  --only-ext <exts>      Only clean matching file extensions (e.g. .log,.tmp).
  --exclude-ext <exts>   Protect specific extensions from deletion (e.g. .py,.cpp).
  --older-than <dur>     Filter files older than duration (e.g. 30m, 24h, 7d).

LOCATION (WHERE):
  --path <p1,p2>         Specify custom directory path(s) to process.
  --drive <drives>       Target specific drive(s) (e.g. C:\, D:\) or 'all'.

CONTROL (HOW):
  --mode <mode>          Execution mode: light, deep, full, shred (secure wipe).
  --dry-run              Preview operational results without disk state mutation.
  --recycle-bin          Purge Windows Recycle Bin / OS Trash via Shell API.
  --json-report <file>   Export structured execution results to JSON report.
```

---

## License
MIT License © 2026 system-cleaner-agent Project.
