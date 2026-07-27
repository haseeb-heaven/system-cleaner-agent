```
   _____ _   _ _____ _____ _____ ___  ___ _____ _     _____ ___   _   _ _____ _____ _   _ _____ 
  /  ___| | | /  ___|_   _|  ___|  \/  |/  __ \ |   |  ___/ _ \ | \ | |  ___|  ___| \ | |_   _|
  \ `--.| |_| \ `--.  | | | |__ | .  . || /  \/ |   | |__/ /_\ \|  \| | |__ | |__ |  \| | | |  
   `--. \__  | `--. \ | | |  __|| |\/| || |   | |   |  __|  _  || . ` |  __||  __|| . ` | | |  
  /\__/ / | |/\__/ / | | | |___| |  | || \__/\ |___| |__| | | || |\  | |___| |___| |\  | | |  
  \____/  \_/\____/  \_/ \____/\_|  |_/ \____/\____/\____\_| |_/\_| \_/\____/\____/\_| \_/ \_/  
```

# system-cleaner-agent v5.0.0

> **100% Pure C++17 Autonomous ReAct Agentic Storage Optimization Engine & OpenTUI Framework**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)
[![Language](https://img.shields.io/badge/Language-100%25%20Pure%20C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Architecture](https://img.shields.io/badge/Architecture-ReAct%20Loop-magenta.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)
[![OpenTUI](https://img.shields.io/badge/TUI-OpenTUI%20Framework-orange.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)
[![Tests](https://img.shields.io/badge/unit--tests-100%25%20passing-success.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)

---

## ⚡ Default OpenTUI Mode

Running `system-cleaner-agent` with no arguments or `--tui` launches the key-navigable **OpenTUI Interactive Dashboard**:

```powershell
.\system-cleaner-agent.exe
# or
.\system-cleaner-agent.exe --tui
```

---

## 🤖 ReAct Autonomous Agent Architecture

Runs an autonomous **Reasoning + Action + Observation (ReAct)** loop with natural language path parsing:

```powershell
.\system-cleaner-agent.exe agent --task "Perform clean code on D:/Temp"
```

---

## Key Features

- ⚡ **100% Pure Native C++17 Engine**: Zero scripting overhead, native speed.
- 🧠 **Autonomous ReAct Agent Loop (`AgentEngine.hpp`)**: Self-directed storage optimization trajectory with target path extraction.
- 💻 **OpenTUI Framework (`OpenTUI.hpp`)**: Zero-dependency cross-platform TUI with ANSI escape sequences, arrow-key menu navigation, and progress bars.
- 🛡️ **Content Protection Shield**: Validates magic bytes (`%PDF`, `PNG`, `JPEG`, `SQLite format 3`) and protects user source code (`.py`, `.cpp`, `.js`).
- 🔓 **Process Lock Manager**: Releases process handles (`mintty`, `cat`, `bash`, `werfault`) locking temporary paths.
- 🔒 **Secure Zero-Overwrite Shredding**: Overwrites junk files with binary zeros before deletion (`--mode shred`).
- 🧪 **Unit Test Suite**: 100% pass rate across all engine components (`unit_tests.exe`).

---

## CLI Options & Usage

```
USAGE:
  system-cleaner-agent [COMMAND] [FLAGS]

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

FILTERING & TARGET SELECTION:
  --category <list>     Target categories: system, browser, dev, messaging, app.
  --only-ext <exts>      Only clean matching file extensions (e.g. .log,.tmp).
  --exclude-ext <exts>   Protect specific extensions from deletion (e.g. .py,.cpp).
  --older-than <dur>     Filter files older than duration (e.g. 30m, 24h, 7d).

LOCATION & SCOPE:
  --path <p1,p2>         Specify custom directory path(s) to process.
  --drive <drives>       Target specific drive(s) (e.g. C:\, D:\) or 'all'.

EXECUTION CONTROL:
  --mode <mode>          Execution mode: light, deep, full, shred (secure wipe).
  --dry-run              Preview operational results without disk state mutation.
  --recycle-bin          Purge Windows Recycle Bin / OS Trash via Shell API.
  --json-report <file>   Export structured execution results to JSON report.
```

---

## License
MIT License © 2026 system-cleaner-agent Project.
