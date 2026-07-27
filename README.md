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

## ⚡ 100% Pure Native C++ Architecture

`system-cleaner-agent` is written **100% natively in ISO C++17**. Zero external scripting dependencies (no Python, no Node.js, no Bash dependencies). Compiled directly to high-performance native machine code across Windows, Linux, and macOS.

```
                           ┌───────────────────────────┐
                           │   system-cleaner-agent    │
                           │   100% ISO C++17 Engine   │
                           └─────────────┬─────────────┘
                                         │
                 ┌───────────────────────┼───────────────────────┐
                 │                       │                       │
     ┌───────────▼───────────┐ ┌─────────▼───────────┐ ┌─────────▼───────────┐
     │   AgentEngine (ReAct) │ │ ContentInspector    │ │ OpenTUI Framework   │
     │ Reason-Act-Observe    │ │ Magic Bytes & Shield│ │ Key-Navigable UI    │
     └───────────┬───────────┘ └─────────┬───────────┘ └─────────┬───────────┘
                 │                       │                       │
                 └───────────────────────┼───────────────────────┘
                                         │
                               ┌─────────▼───────────┐
                               │ Multi-Thread Async  │
                               │ Parallel Engine     │
                               └─────────────────────┘
```

---

## 🤖 ReAct Autonomous Agent Architecture

Runs an autonomous **Reasoning + Action + Observation (ReAct)** loop:

```powershell
.\system-cleaner-agent.exe agent --task "Perform autonomous storage optimization"
```

---

## Key Features

- ⚡ **100% Pure Native C++17 Engine**: Zero scripting overhead, native speed.
- 🧠 **Autonomous ReAct Agent Loop (`AgentEngine.hpp`)**: Self-directed storage optimization trajectory.
- 💻 **OpenTUI Framework (`OpenTUI.hpp`)**: Zero-dependency cross-platform TUI with ANSI escape sequences, arrow-key menu navigation, and progress bars.
- 🛡️ **Content Protection Shield**: Validates magic bytes (`%PDF`, `PNG`, `JPEG`, `SQLite format 3`) and protects user source code (`.py`, `.cpp`, `.js`).
- 🔓 **Process Lock Manager**: Releases process handles (`mintty`, `cat`, `bash`, `werfault`) locking temporary paths.
- 🔒 **Secure Zero-Overwrite Shredding**: Overwrites junk files with binary zeros before deletion (`--mode shred`).
- 🧪 **Unit Test Suite**: 100% pass rate across all engine components (`unit_tests.exe`).

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

## License
MIT License © 2026 system-cleaner-agent Project.
