# Command & Agent Query Language (AQL) Specification

> **Full Command Line Reference & Natural Language AQL Grammar Guide for `system-cleaner-agent` v5.7.1**

---

## Table of Contents

- [Agent Query Language (AQL)](#-agent-query-language-aql)
  - [Command Verbs](#aql-command-verbs)
  - [WHERE Conditions & Operators](#aql-where-conditions--operators)
  - [Target Path Aliases](#aql-target-path-aliases)
  - [Query Examples](#ready-made-aql-query-examples)
  - [Persistent Daemon Jobs (Cron-Style)](#cron-style-aql-persistent-daemon-jobs)
  - [AQL Validation & Agent Hints](#aql-syntax-validator)
- [CLI Options & Specification](#-cli-options--specification)
  - [Subcommands](#cli-subcommands)
  - [Scheduler & Threshold Monitoring](#smart-scheduler--threshold-monitoring)
  - [ReAct Agentic Execution](#agentic-react-options)
  - [Filtering & Target Selection](#filtering--target-selection)
  - [Location & Scope](#location--scope)
  - [Execution & Security Controls](#execution--security-controls)

---

## 💬 Agent Query Language (AQL)

AQL is the natural-language command interface for autonomous storage operations. The AQL engine parses queries into AST tokens and dispatches execution to the native C++ subsystems (Disk, RAM, Process, Trash, Daemon).

### AQL Command Verbs

| Verb | Action | Target Domain | Example Query |
|---|---|---|---|
| `CLEAN` | Delete temporary files and caches | File System | `CLEAN TEMP_C WHERE DISK_C < 500MB` |
| `SCAN` / `SELECT` | Analyze target paths without mutating disk | File System | `SELECT 'D:/Temp' WHERE SIZE > 100MB` |
| `SHRED` / `WIPE` | Overwrite target files with zero bytes | Secure Wipe | `SHRED 'C:/Temp/old.log' WHERE SIZE > 10MB` |
| `KILL` / `TERMINATE` | End running processes | Process Manager | `KILL chrome.exe FROM PROCESS WHERE RAM > 1GB` |
| `MONITOR` / `WATCH` | Register background threshold watch job | Smart Scheduler | `MONITOR WHERE RAM > 80% EVERY 15S` |
| `PURGE` / `EMPTY` / `TRASH` | Empty OS Recycle Bin / Trash | OS Shell API | `PURGE RECYCLE_BIN` |

---

### AQL WHERE Conditions & Operators

| Condition Syntax | Meaning | Evaluated Subsystem |
|---|---|---|
| `DISK_C < 500MB` | Primary drive C free space below threshold | Disk Inspector |
| `DISK_D < 2GB` | Secondary drive D free space below threshold | Disk Inspector |
| `FREE_DISK < 10GB` | Any drive free space below threshold | Disk Inspector |
| `RAM > 80%` | System total RAM usage above percentage | Memory Monitor |
| `RAM > 200MB` | Process memory usage above threshold | Process Manager |
| `MEMORY > 1GB` | Process working set above threshold | Process Manager |
| `SIZE > 10MB` | File size greater than 10 Megabytes | File Scanner |
| `SIZE > 1GB` | File size greater than 1 Gigabyte | File Scanner |
| `AGE > 24H` | File modification time older than 24 hours | File Scanner |
| `AGE > 7D` | File modification time older than 7 days | File Scanner |
| `EXT IN ('.tmp', '.log')` | Match file extensions | Filter Engine |
| `EVERY 15S` | Recurring execution interval (S/M/H/D) | Smart Scheduler |

---

### AQL Target Path Aliases

AQL maps cross-platform aliases to OS-native locations:

| Alias | Windows Target Path | Linux / macOS Target Path |
|---|---|---|
| `TEMP_C` | `%USERPROFILE%\AppData\Local\Temp`, `%WINDIR%\Temp` | `/tmp`, `/var/tmp` |
| `TEMP_D` | `D:\Temp`, `D:\Cache` | N/A |
| `APPDATA` | `AppData\Local\Temp`, `AppData\Local\Caches` | `~/.cache`, `~/Library/Caches` |
| `CACHE` | Chrome, Edge, Firefox, Brave cache folders | `~/.cache/google-chrome`, `~/Library/Caches` |
| `RECYCLE_BIN` | Windows Shell Recycle Bin (`CSIDL_BITBUCKET`) | OS Trash (`~/.local/share/Trash`) |

---

### Ready-Made AQL Query Examples

```bash
# 1. Terminate Chrome when system RAM exceeds 80%
system-cleaner-agent aql "KILL chrome.exe FROM PROCESS WHERE RAM > 80%"

# 2. Clean temp folder when drive C has less than 500MB free space
system-cleaner-agent aql "CLEAN TEMP_C WHERE DISK_C < 500MB"

# 3. Clean user AppData cache when free space is below 1GB
system-cleaner-agent aql "CLEAN APPDATA WHERE DISK_C < 1GB"

# 4. Background daemon: watch build caches every 5 hours
system-cleaner-agent aql "MONITOR WHERE EXT IN ('.pyc', '.cache', 'node_modules') EVERY 5H"

# 5. Zero-overwrite shred temp files larger than 10MB
system-cleaner-agent aql "SHRED 'C:\Users\User\AppData\Local\Temp' WHERE SIZE > 10MB"

# 6. Purge Windows Recycle Bin / OS Trash
system-cleaner-agent aql "PURGE RECYCLE_BIN"

# 7. Multi-condition query: clean temp files if RAM is high AND disk space is low
system-cleaner-agent aql "CLEAN TEMP_C WHERE RAM > 70% AND DISK_C < 1GB"

# 8. Filter specific log and temp file extensions
system-cleaner-agent aql "CLEAN 'C:/Temp' WHERE EXT IN ('.tmp', '.log', '.cache')"

# 9. Terminate process by executable name
system-cleaner-agent aql "KILL notepad.exe"
```

---

### Cron-Style AQL (Persistent Daemon Jobs)

Append `EVERY <duration>` to any `MONITOR` or `CLEAN` query to turn it into a persistent background job. The daemon parses the duration suffix (`S` seconds, `M` minutes, `H` hours, `D` days) and registers it in the unified Task Library so it auto-resumes across system restarts.

```powershell
# Monitor system RAM every 30 seconds
.\system-cleaner-agent.exe aql "MONITOR WHERE RAM > 85% EVERY 30S"

# Sweep build artifacts every 2 hours
.\system-cleaner-agent.exe aql "MONITOR WHERE EXT IN ('.pyc', '.cache', 'node_modules') EVERY 2H"

# Auto-clean temp folder every 15 minutes when disk space is below 1GB
.\system-cleaner-agent.exe aql "CLEAN TEMP_C WHERE DISK_C < 1GB EVERY 15M"
```

---

### AQL Syntax Validator

Every AQL query is validated before execution by `AQLEngine::Validate(query)`. If a query contains invalid grammar or unrecognized tokens, it returns a structured error object with a suggested fix hint:

```json
{
  "isValid": false,
  "errorMessage": "Invalid WHERE operator '>>>'",
  "suggestedHint": "Use standard comparison operators: WHERE RAM > 80% or WHERE DISK_C < 500MB"
}
```

---

## ⚡ CLI Options & Specification

### CLI Subcommands

```
USAGE:
  system-cleaner-agent [COMMAND] [FLAGS]

COMMANDS:
  tui          Launch OpenTUI interactive dashboard (default when run without arguments).
  chat         Launch Interactive Local LLM ReAct Prompt Shell.
  agent        Launch Autonomous ReAct AI Agent Loop (Thought->Action->Observe->Verify).
  daemon       Run Background Scheduler & Memory Threshold Daemon.
  scan         Analyze system/drive targets and report cleanable storage.
  clean        Execute multi-threaded cleanup using active policy rules.
  deep-clean   Perform full system cache cleanup + empty OS Recycle Bin / Trash.
  aql          Execute a single Agent Query Language (AQL) statement.
  history      Show the unified Task Library history.
  test         Execute engine diagnostic & unit test suite.
  version      Display version, engine build, and architecture details.
  help         Show CLI usage and command reference.
```

---

### Smart Scheduler & Threshold Monitoring

| Flag | Argument | Description | Default |
|---|---|---|---|
| `--mem-threshold` | `<pct>` | RAM usage percentage cutoff for auto-cleanup | `80%` |
| `--disk-threshold` | `<pct>` | Disk usage percentage cutoff for auto-cleanup | `90%` |
| `--schedule` | `<rule>` | Custom folder rule (e.g. `"D:/Temp:15m:mem>80%"`) | N/A |
| `--interval` | `<dur>` | Check interval for daemon monitor (e.g. `1s`, `5s`, `15s`) | `15s` |

---

### Agentic ReAct Options

| Flag | Argument | Description | Default |
|---|---|---|---|
| `--task` | `<goal>` | Custom natural language goal for autonomous ReAct loop | N/A |
| `--agent` | None | Enable self-directed reasoning trajectory | Disabled |

---

### Filtering & Target Selection

| Flag | Argument | Description |
|---|---|---|
| `--category` | `<list>` | Target specific categories: `system`, `browser`, `dev`, `messaging`, `app` |
| `--exclude-category` | `<list>` | Exclude target categories from operation |
| `--only-ext` | `<exts>` | Restrict operation to matching extensions (e.g. `.tmp,.log`) |
| `--exclude-ext` | `<exts>` | Protect specific extensions from deletion (e.g. `.py,.cpp`) |
| `--older-than` | `<dur>` | Filter files older than duration (e.g. `30m`, `24h`, `7d`) |
| `--min-size` | `<size>` | Minimum file size threshold (e.g. `10MB`, `100MB`) |
| `--max-size` | `<size>` | Maximum file size threshold |
| `--strategy` | `<mode>` | Inspection policy: `smart` (default), `force`, `safe` |

---

### Location & Scope

| Flag | Argument | Description |
|---|---|---|
| `--path` | `<p1,p2>` | Process specific custom directory paths |
| `--drive` | `<drives>` | Target specific drive letters (e.g. `C:\`, `D:\`) or `all` |
| `--project-root` | `<dir>` | Root directory for recursive dev build cache discovery |

---

### Execution & Security Controls

| Flag | Argument | Description | Default |
|---|---|---|---|
| `--mode` | `<mode>` | Execution mode: `light`, `deep`, `full`, `shred` | `light` |
| `--dry-run` | None | Preview execution without modifying disk state | Disabled |
| `--sandbox` | None | Enable sandbox dry-run mode (prevents deletions) | Config |
| `--path-protection` | None | Enable system path protection guard | Enabled |
| `--no-path-protection` | None | Disable path safety guard (**Use with Caution**) | Disabled |
| `--recycle-bin` | None | Purge OS Recycle Bin / Trash via Shell API | Disabled |
| `--kill-locks` | `<bool>` | Release file lock handles before operation | `true` |
| `--json-report` | `<file>` | Export structured execution results to JSON file | N/A |

---

## 🔗 Related Documentation

- [README.md](README.md) - Project overview, features, and screenshots.
- [CHANGELOG.md](CHANGELOG.md) - Full version release history.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - ReAct Agent, OpenTUI, and GTLibc engine architecture.
