```
   ______                  _       _  _____                  _____  _                           
  / ____/___  ____  ____  (_)     | |/ /   |  _________     / ___/ (_)___  ____ _____  ___  _____
 / / __/ _ \/ __ \/ __ \/ /______/   / /| | / ___/ __ \    \__ \ / / __ \/ __ `/ __ \/ _ \/ ___/
/ /_/ /  __/ / / / / / / /_____/   / ___ |/ /__/ /_/ /   ___/ / / / /_/ / /_/ / /_/ /  __/ /    
\____/\___/_/ /_/_/ /_/_/     /_/|_/_/  |_|\___/\____/   /____/_/_/ .___/\__,_/ .___/\___/_/     
                                                                 /_/         /_/                
```

# Gemini System Cleaner v3.5 (Enterprise Edition)

> **High-Performance C++17 Multi-Platform Storage Optimization & Content-Inspected Cleanup Engine**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/haseeb-heaven/gemini-sys-cleaner)
[![Language](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)](https://github.com/haseeb-heaven/gemini-sys-cleaner)
[![Tests](https://img.shields.io/badge/unit--tests-100%25%20passing-success.svg)](https://github.com/haseeb-heaven/gemini-sys-cleaner)

---

## Key Features

- 🛡️ **Smart File Content Inspection & Protection Shield**:
  - Magic byte verification (`%PDF`, `PNG`, `JPEG`, `SQLite format 3`, `MZ`, `ELF`).
  - **User Data Safety Shield**: Never deletes source code (`.py`, `.cpp`, `.js`, `.ts`), documents, databases, or project assets even if located in a `temp` folder.
  - 40/64-character SHA-1/SHA-256 hex hash cache blob detector.
- ⚡ **Multi-Threaded $O(N)$ Parallel Execution**:
  - Multi-threaded scanning and non-blocking deletion using `std::async` worker threads.
  - Reparse point and junction safety (prevents recursive loop hangs).
- 🔓 **Process Lock Manager**:
  - Automatically releases process file handles (`mintty`, `cat`, `bash`, `werfault`) prior to cleaning.
- 💻 **Terminal User Interface (TUI)**:
  - Interactive terminal menu accessible via `gemini-sys-cleaner tui` or `-i`.
- 🔒 **Secure Zero-Overwrite Shredding**:
  - Overwrites matching junk files with binary zeros before disk removal (`--mode shred`).
- 📊 **JSON Execution Reports**:
  - Export structured audit logs with `--json-report report.json`.
- 🧪 **Unit Test & TDD Regression Suite**:
  - Comprehensive assertions covering size formatters, duration/size parsers, hex hash detectors, and safety shields.

---

## Quick Start

### Build Instructions

#### **Windows (MSVC / MinGW / CMake)**:
```cmd
build.bat
```

#### **Linux / macOS (GCC / Clang / CMake)**:
```bash
chmod +x build.sh
./build.sh
```

---

## CLI Usage & Options

```
USAGE:
  gemini-sys-cleaner <COMMAND> [FLAGS]

COMMANDS:
  scan         Analyze system/drive targets and report cleanable storage.
  clean        Execute multi-threaded cleanup using active policy rules.
  deep-clean   Perform full system cache cleanup + empty OS Recycle Bin / Trash.
  tui          Launch interactive Terminal User Interface (TUI).
  test         Execute automated engine diagnostic & unit test suite.
  version      Display version, engine build, and architecture details.
  help         Show this help and usage specification.

FILTERING (WHAT):
  --category <list>     Target categories: system, browser, dev, messaging, app.
  --exclude-category <c> Exclude target categories from operation.
  --only-ext <exts>      Only clean matching file extensions (e.g. .log,.tmp).
  --exclude-ext <exts>   Protect specific extensions from deletion (e.g. .py,.cpp).
  --older-than <dur>     Filter files older than duration (e.g. 30m, 24h, 7d).
  --min-size <size>      Minimum file size threshold (e.g. 10MB, 100MB, 1GB).

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

## Running Unit Tests

Run the engine diagnostic test suite directly:
```powershell
.\gemini-sys-cleaner.exe test
```
Or run the compiled test binary:
```powershell
.\build\Release\unit_tests.exe
```

---

## License
MIT License © 2026 Gemini Sys Cleaner Project.
