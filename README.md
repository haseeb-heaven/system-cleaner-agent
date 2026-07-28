# system-cleaner-agent v5.6.9

> **100% Pure C++17 Autonomous ReAct Agentic Storage Optimization Engine & OpenTUI Framework**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)
[![Language](https://img.shields.io/badge/Language-100%25%20Pure%20C%2B%2B17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Architecture](https://img.shields.io/badge/Architecture-ReAct%20Loop-magenta.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)
[![OpenTUI](https://img.shields.io/badge/TUI-OpenTUI%20Framework-orange.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)
[![Tests](https://img.shields.io/badge/unit--tests-100%25%20passing-success.svg)](https://github.com/haseeb-heaven/system-cleaner-agent)
[![Release](https://img.shields.io/badge/release-v5.6.9-blue.svg)](https://github.com/haseeb-heaven/system-cleaner-agent/releases/latest)

---

## 📸 Screenshots

### Main Menu - Live CPU/RAM/Disk Monitoring with Sparklines

![Main Menu](assets/screenshot-main-menu.svg)

*The main menu shows real-time CPU, RAM, and disk usage with **color-coded progress bars** and **sparkline trend charts** (▁▂▃▄▅▆▇█). The headers auto-refresh based on the Monitor Interval setting (1s/3s/5s/10s/15s/30s/60s).*

### Settings Menu - Configure All TUI Options

![Settings](assets/screenshot-settings.svg)

*Navigate with **Left/Right** arrow keys to instantly toggle settings. **1 second (LIVE)** mode is available for ultra-fast monitoring.*

### Disk Cleaner Suite

![Disk Cleaner](assets/screenshot-disk-cleaner.svg)

*Multi-drive parallel scanning, deep storage hotspot analysis, smart deep clean, browser caches, developer caches, secure shred wipe, and OS Recycle Bin purge.*

### RAM Cleaner - Process Management

![RAM Cleaner](assets/screenshot-ram-cleaner.svg)

*Quick RAM optimization, OS memory working set trimming, browser memory purge, and high-RAM process termination with permission management.*

### AQL Console - Agent Query Language

![AQL Console](assets/screenshot-aql-console.svg)

*Natural language Agent Query Language (AQL) for autonomous operations. Example: `KILL chrome.exe WHEN RAM > 1GB`.*

### Smart Daemon Monitor

![Daemon Monitor](assets/screenshot-daemon-monitor.svg)

*Background scheduler that watches RAM, disk free space, and triggers automatic cleanup when thresholds are exceeded.*

### Task Library - Persistent History

![Task Library](assets/screenshot-task-library.svg)

*All tasks (clean, scan, AQL, agent, daemon) are saved to `cleaner_config.json` and auto-resumed on startup.*

### CLI: Help Output

![CLI Help](assets/screenshot-cli-help.svg)

*Full CLI command reference with all options documented.*

### CLI: Version Output

![CLI Version](assets/screenshot-cli-version.svg)

*Version and architecture information.*

### CLI: Task Library (History)

![CLI History](assets/screenshot-cli-history.svg)

*Persistent task history accessible via `history` command.*

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
- 🧠 **Autonomous ReAct Agent Loop**: Self-directed storage optimization trajectory.
- 💻 **OpenTUI Framework**: Zero-dependency cross-platform TUI with ANSI escape sequences.
- 📊 **Live CPU/RAM/Disk Monitoring**: Color-coded progress bars + sparkline trend charts.
- 🛡 **Content Protection Shield**: Validates magic bytes and protects user source code.
- 🔓 **Process Lock Manager**: Releases process handles locking temporary paths.
- 🔒 **Secure Zero-Overwrite Shredding**: Overwrites junk files with binary zeros.
- 🧪 **Unit Test Suite**: 100% pass rate across all engine components.
- 📁 **Persistent Configuration**: All settings auto-saved to `cleaner_config.json`.

---

## CLI Options & Usage

```
USAGE:
  system-cleaner-agent [COMMAND] [FLAGS]

COMMANDS:
  chat         Launch Interactive Local LLM ReAct Prompt Shell (try 'menu' for queries).
  agent        Launch Autonomous ReAct Agent Loop (Thought->Action->Observation).
  daemon       Run Smart Background Scheduler & Memory Threshold Monitoring Daemon.
  scan         Analyze system/drive targets and report cleanable storage.
  clean        Execute multi-threaded cleanup using active policy rules.
  deep-clean   Perform full system cache cleanup + empty OS Recycle Bin / Trash.
  aql          Execute a single Agent Query Language (AQL) statement.
  history      Show the unified Task Library (all clean/scan/AQL/agent/daemon tasks).
  tui          Launch interactive Terminal User Interface (TUI) - default mode.
  test         Execute automated engine diagnostic & unit test suite.
  version      Display version, engine build, and architecture details.
  help         Show this help and usage specification.

SMART SCHEDULER & THRESHOLD MONITORING:
  --mem-threshold <pct>    Automatic cleanup trigger when system RAM exceeds percentage.
  --disk-threshold <pct>   Automatic cleanup trigger when Disk space exceeds percentage.
  --schedule <rule>        Set folder-specific rule (e.g. "D:/Temp:15m:mem>80%").
  --interval <dur>         Check interval duration for daemon monitor (default: 15s).

AGENTIC REACT OPTIONS:
  --task <goal>            Specify custom natural language goal for ReAct loop.
  --agent                  Enable autonomous reasoning and action trajectory.

FILTERING & TARGET SELECTION:
  --category <list>        Target categories: system, browser, dev, messaging, app.
  --only-ext <exts>        Only clean matching file extensions (e.g. .log,.tmp).
  --exclude-ext <exts>     Protect specific extensions from deletion (e.g. .py,.cpp).
  --older-than <dur>       Filter files older than duration (e.g. 30m, 24h, 7d).

LOCATION & SCOPE:
  --path <p1,p2>           Specify custom directory path(s) to process.
  --drive <drives>         Target specific drive(s) (e.g. C:\, D:\) or 'all'.

EXECUTION CONTROL:
  --mode <mode>            Execution mode: light, deep, full, shred (secure wipe).
  --dry-run                Preview operational results without disk state mutation.
  --recycle-bin            Purge Windows Recycle Bin / OS Trash via Shell API.
  --kill-locks <bool>      Release file lock handles before operation.
  --json-report <file>     Export structured execution results to JSON report.
```

---

## 📦 Download

Download the latest Windows x64 binary from the [Releases page](https://github.com/haseeb-heaven/system-cleaner-agent/releases/latest):

**[⬇ Download system-cleaner-agent v5.6.9 (Windows x64)](https://github.com/haseeb-heaven/system-cleaner-agent/releases/latest)**

---

## License

MIT License © 2026 system-cleaner-agent Project.
