#pragma once
#include "Cleaner.hpp"
#include "Logger.hpp"
#include "OpenTUI.hpp"
#include "SmartScheduler.hpp"
#include "AgentEngine.hpp"
#include "AgentQueryLanguage.hpp"
#include "ConfigManager.hpp"
#include "TaskHistory.hpp"
#include "SecurityGuard.hpp"
#include "DeepScanner.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <conio.h>
#endif

// Global TUI Settings State
struct TUISettings {
    bool sandboxMode = false;        // OFF allows real deletion as requested!
    bool pathProtection = true;     // ON by default
    bool dryRun = false;            // OFF allows real deletion as requested!
    bool killLocks = true;          // ON by default
    bool enableLogging = true;      // File output logging ON/OFF
    std::string logLevel = "INFO";  // INFO, WARN, ERROR, VERBOSE
    int monitorIntervalSec = 5;     // 5 seconds refresh interval
    size_t ramThresholdMB = 200;    // 200 MB high RAM process cutoff
    std::string customPathsStr = "C:\\Users\\hasee\\AppData\\Local\\Temp";
    std::string tuiThemeEngine = "OpenTUI"; // OpenTUI, TermOx, FTXUI
    std::string tuiColorScheme = "Default"; // Preset Palette
    std::string tuiFgColor = "Default";     // Default, Cyan, Electric Magenta, Amber Gold, Emerald Green, Neon Pink, Bright White, Yellow, Royal Blue
    std::string tuiBgColor = "Default";     // Default, Black, Navy Blue, Electric Magenta, Amber Gold, Emerald Green, Dark Slate, Charcoal Gray
    std::vector<std::string> customProtectedProcesses;

    void SyncFromAppConfig(const AppConfig& cfg) {
        sandboxMode = cfg.sandboxMode;
        pathProtection = cfg.pathProtection;
        dryRun = cfg.dryRun;
        killLocks = cfg.killLocks;
        enableLogging = cfg.enableLogging;
        logLevel = cfg.logLevel;
        monitorIntervalSec = cfg.monitorIntervalSec;
        ramThresholdMB = cfg.ramThresholdMB;
        customPathsStr = cfg.customPathsStr;
        tuiThemeEngine = cfg.tuiThemeEngine;
        tuiColorScheme = cfg.tuiColorScheme;
        tuiFgColor = cfg.tuiFgColor;
        tuiBgColor = cfg.tuiBgColor;
        customProtectedProcesses = cfg.customProtectedProcesses;
    }

    AppConfig ToAppConfig() const {
        AppConfig cfg;
        cfg.sandboxMode = sandboxMode;
        cfg.pathProtection = pathProtection;
        cfg.dryRun = dryRun;
        cfg.killLocks = killLocks;
        cfg.enableLogging = enableLogging;
        cfg.logLevel = logLevel;
        cfg.monitorIntervalSec = monitorIntervalSec;
        cfg.ramThresholdMB = ramThresholdMB;
        cfg.customPathsStr = customPathsStr;
        cfg.tuiThemeEngine = tuiThemeEngine;
        cfg.tuiColorScheme = tuiColorScheme;
        cfg.tuiFgColor = tuiFgColor;
        cfg.tuiBgColor = tuiBgColor;
        cfg.customProtectedProcesses = customProtectedProcesses;
        return cfg;
    }
};

static TUISettings g_tuiSettings;

inline void SaveTUISettings() {
    ConfigManager::Save(g_tuiSettings.ToAppConfig());
}

inline void LoadTUISettings() {
    AppConfig cfg = ConfigManager::Load();
    // On first launch or if customPathsStr is still the old Windows-only default,
    // populate it with the full OS-aware safe temp/cache path list
    if (cfg.customPathsStr.empty() ||
        cfg.customPathsStr == "C:\\Users\\hasee\\AppData\\Local\\Temp") {
        cfg.customPathsStr = SecurityGuard::GetDefaultCleanPathsStr();
        ConfigManager::Save(cfg);
    }
    g_tuiSettings.SyncFromAppConfig(cfg);
    // Sync Logger file-write gate with the loaded setting
    Logger::Instance().SetFileLogging(g_tuiSettings.enableLogging);
    // Restore custom protected processes into the GTLibc runtime list
    for (const auto& proc : g_tuiSettings.customProtectedProcesses) {
        GTLIBC::GTLibc::AddCustomProtectedProcess(proc);
    }
}

// Background Task State for Non-Blocking TUI Interface
struct TUITaskStatus {
    std::atomic<bool> isRunning{false};
    std::string taskName = "Idle";
    std::string lastMessage = "Ready";
    std::mutex mtx;

    void SetActive(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx);
        isRunning = true;
        taskName = name;
        lastMessage = "Running: " + name;
    }

    void SetCompleted(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);
        isRunning = false;
        lastMessage = msg;
    }

    std::string GetStatusLine() {
        std::lock_guard<std::mutex> lock(mtx);
        std::string modeStr = g_tuiSettings.sandboxMode ? " [SANDBOX]" : " [REAL CLEAN]";
        auto tasks = TaskHistory::Instance().Snapshot();
        size_t runningCount = 0;
        std::string runningName = "";
        std::string runningDetail = "";
        for (const auto& t : tasks) {
            if (t.status == TaskStatus::Running || t.status == TaskStatus::Queued) {
                runningCount++;
                if (runningName.empty()) {
                    runningName = t.name;
                    runningDetail = t.progressMsg;
                }
            }
        }

        if (runningCount > 0) {
            std::string detailStr = runningDetail.empty() ? runningName : runningDetail;
            return "[STATUS] 🟢 ACTIVE (" + std::to_string(runningCount) + " task" + (runningCount > 1 ? "s" : "") + "): " + detailStr + modeStr;
        } else if (isRunning) {
            return "[STATUS] 🟢 ACTIVE: " + taskName + modeStr;
        } else {
            return "[STATUS] ⚪ READY | " + lastMessage + modeStr;
        }
    }
};




static TUITaskStatus g_tuiStatus;

class TUI {
public:
    // PrintBanner is now a no-op (the ASCII logo was removed in v5.6.4)
    static void PrintBanner(const std::string& themeOverride = "") { (void)themeOverride; }

    static void FlushInputBuffer() {
        std::cin.clear();
#ifdef _WIN32
        while (_kbhit()) { (void)_getch(); }
#endif
    }

    // ----------------------------------------------------------------
    // Resource history (for live sparkline trend charts)
    // ----------------------------------------------------------------
    struct ResourceHistory {
        std::vector<double> cpu;
        std::vector<double> ram;
        std::vector<double> disk;
        static constexpr size_t MAX_SAMPLES = 30;
        void Push(double c, double r, double d) {
            cpu.push_back(c);
            ram.push_back(r);
            disk.push_back(d);
            if (cpu.size() > MAX_SAMPLES) cpu.erase(cpu.begin());
            if (ram.size() > MAX_SAMPLES) ram.erase(ram.begin());
            if (disk.size() > MAX_SAMPLES) disk.erase(disk.begin());
        }
    };
    static ResourceHistory& GetResourceHistory() {
        static ResourceHistory hist;
        return hist;
    }

    static std::vector<std::string> GetLiveResourceHeaders() {
        std::vector<std::string> headers;

        // Sample CPU, RAM, and disk usage
        double cpuPercent = SmartScheduler::GetCpuUsagePercent();
        double memPercent = SmartScheduler::GetMemoryUsagePercent();
        auto driveStats = SmartScheduler::GetAllDriveStats();
        double maxDiskPercent = 0.0;
        for (const auto& ds : driveStats) {
            if (ds.usedPercent > maxDiskPercent) maxDiskPercent = ds.usedPercent;
        }

        // Update resource history (used for sparkline trend charts)
        GetResourceHistory().Push(cpuPercent, memPercent, maxDiskPercent);
        const auto& hist = GetResourceHistory();

        // Line 1: CPU with color-coded progress bar + sparkline trend
        std::string cpuBar = OpenTUI::RenderColoredBar(cpuPercent, 18, "%");
        std::ostringstream cpuLine;
        cpuLine << "[1;36mCPU:[0m  " << cpuBar
                 << "  [90m" << OpenTUI::Sparkline::Render(hist.cpu, 14) << "[0m";
        headers.push_back(cpuLine.str());

        // Line 2: RAM with color-coded progress bar + sparkline trend
        std::string ramBar = OpenTUI::RenderColoredBar(memPercent, 18, "%");
        std::ostringstream ramLine;
        ramLine << "[1;33mRAM:[0m  " << ramBar
                 << "  [90m" << OpenTUI::Sparkline::Render(hist.ram, 14) << "[0m";
        headers.push_back(ramLine.str());

        // Lines 3+: Each drive with progress bar
        for (const auto& ds : driveStats) {
            std::string diskBar = OpenTUI::RenderColoredBar(ds.usedPercent, 14, "%");
            std::ostringstream ss;
            ss << ds.driveName << " " << diskBar
               << " (F:" << Cleaner::FormatSize(ds.freeBytes)
               << " / " << Cleaner::FormatSize(ds.capacityBytes) << ")";
            headers.push_back(ss.str());
        }

        // Trend chart for disk usage (most recent on the right)
        if (!hist.disk.empty()) {
            std::ostringstream trendLine;
            trendLine << "[1;35mTREND:[0m [90m"
                       << OpenTUI::Sparkline::Render(hist.disk, 20)
                       << "[0m [90m(disk usage history)[0m";
            headers.push_back(trendLine.str());
        }

        return headers;
    }

    // Legacy: add running task info to header list (for callers that expect it)
    static void AppendTaskStatusToHeaders(std::vector<std::string>& headers) {
        auto tasks = TaskHistory::Instance().Snapshot();
        size_t runningCount = 0;
        std::string activeName = "";
        std::string activeProgress = "";
        for (const auto& t : tasks) {
            if (t.status == TaskStatus::Running || t.status == TaskStatus::Queued) {
                runningCount++;
                if (activeName.empty()) {
                    activeName = t.name;
                    activeProgress = t.progressMsg;
                }
            }
        }
        std::ostringstream taskSs;
        if (runningCount > 0) {
            taskSs << "Tasks: \033[1;36m" << runningCount << " RUNNING\033[0m (" << activeName;
            if (!activeProgress.empty()) taskSs << " - " << activeProgress;
            taskSs << ") | Total Recorded: " << tasks.size();
        } else {
            taskSs << "Tasks: \033[1;32mIDLE\033[0m (0 Running) | Total Recorded: " << tasks.size();
        }
        headers.push_back(taskSs.str());
    }

    static void ShowSystemResourceMonitor() {
        while (true) {
            auto style = OpenTUI::GetThemeStyle(g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            std::ostringstream ss;
            ss << OpenTUI::Box::DrawBorder(80, "SYSTEM RESOURCE & DRIVE MONITOR", g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            // CPU usage with color-coded progress bar + sparkline trend
            double cpuPercent = SmartScheduler::GetCpuUsagePercent();
            double memPercent = SmartScheduler::GetMemoryUsagePercent();
            const auto& hist = GetResourceHistory();

            std::string cpuBar = OpenTUI::RenderColoredBar(cpuPercent, 30, "%");
            std::ostringstream cpuLine;
            cpuLine << "\033[1;36mCPU  \033[0m" << cpuBar << "  trend: " << OpenTUI::Sparkline::Render(hist.cpu, 20);
            ss << OpenTUI::Box::DrawLine(80, cpuLine.str(), false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            // RAM with color-coded progress bar + sparkline trend
            std::string ramBar = OpenTUI::RenderColoredBar(memPercent, 30, "%");
            std::ostringstream ramLine;
            ramLine << "\033[1;33mRAM  \033[0m" << ramBar << "  trend: " << OpenTUI::Sparkline::Render(hist.ram, 20);
            ss << OpenTUI::Box::DrawLine(80, ramLine.str(), false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            ss << OpenTUI::Box::DrawDivider(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            // Each drive with progress bar + free/total info
            auto driveStats = SmartScheduler::GetAllDriveStats();
            for (const auto& ds : driveStats) {
                std::string diskBar = OpenTUI::RenderColoredBar(ds.usedPercent, 20, "%");
                std::ostringstream dss;
                dss << "Disk " << ds.driveName << " " << diskBar << " (Free: " << Cleaner::FormatSize(ds.freeBytes) << " / " << Cleaner::FormatSize(ds.capacityBytes) << ")";
                ss << OpenTUI::Box::DrawLine(80, dss.str(), false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            }
            ss << OpenTUI::Box::DrawDivider(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            std::string hint = "Press 'A'/Enter to run AQL Query | ESC/'q' to return (Refreshing every " + std::to_string(g_tuiSettings.monitorIntervalSec) + "s)";
            ss << OpenTUI::Box::DrawLine(80, hint, false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            ss << OpenTUI::Box::DrawFooter(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            // Clear, home, then flush entire frame atomically (no partial-render artifacts)
            OpenTUI::TerminalEngine::ClearScreen(style.panelBg);
            OpenTUI::TerminalEngine::MoveCursorToHome();
            std::cout << style.panelBg << ss.str() << "\033[J" << std::flush;

            // Sleep in 100ms intervals to allow ESC/q/A responsiveness
            int checkCycles = g_tuiSettings.monitorIntervalSec * 10;
            bool returnToMenu = false;
            for (int i = 0; i < checkCycles; ++i) {
#ifdef _WIN32
                if (_kbhit()) {
                    int c = _getch();
                    if (c == 27 || c == 'q' || c == 'Q') {
                        returnToMenu = true;
                        break;
                    } else if (c == 'a' || c == 'A' || c == 13) { // Enter or 'A' key
                        std::string q = SelectAgentQuery();
                        if (!q.empty()) {
                            Cleaner localCleaner;
                            AQLEngine::Execute(AQLEngine::Parse(q), localCleaner, g_tuiSettings.dryRun);
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                        }
                    }
                }
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (returnToMenu) break;
        }
    }


    static void ShowTaskActionDialog(uint64_t tid) {
        while (true) {
            TaskEntry t;
            if (!TaskHistory::Instance().GetTask(tid, t)) {
                std::cout << "\033[1;31m[ERROR] Task ID #" << tid << " not found in Task Library.\033[0m\n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                return;
            }

            bool isLive = (t.status == TaskStatus::Running || t.status == TaskStatus::Queued);
            std::string timeStr = TaskHistoryNS::FormatTimestamp(isLive ? t.startedAt : t.finishedAt);
            std::string durStr  = TaskHistoryNS::FormatDuration(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t.startedAt));
            std::string statusStr = isLive ? "RUNNING" : TaskHistoryNS::StatusLabel(t.status);
            std::string detail = isLive ? (t.progressMsg.empty() ? statusStr : t.progressMsg) : (t.resultSummary.empty() ? statusStr : t.resultSummary);

            std::vector<std::string> headers = {
                "TASK STATUS DETAILS & RUNTIME INFORMATION",
                "----------------------------------------------------------------",
                "  Task ID:      #" + std::to_string(t.id),
                "  Task Name:    " + t.name,
                "  Category:     " + t.category,
                "  Command:      " + t.command,
                "  Source:       " + t.source,
                "  Status:       " + statusStr,
                "  Progress:     " + detail,
                "  Runtime:      " + timeStr + " (" + durStr + ")",
                "  Bytes Freed:  " + Cleaner::FormatSize(t.bytesFreed),
                "----------------------------------------------------------------"
            };

            std::vector<std::string> actionOpts = {
                "Pause / Stop Task",
                "Resume Task",
                "Kill / Terminate Task",
                "Return to Task Library"
            };

            OpenTUI::Menu actMenu("TASK ACTIONS & MONITORING (#" + std::to_string(t.id) + ")", actionOpts, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            actMenu.SetHeaderLines(headers);
            int aSel = actMenu.Show();

            if (aSel == -1 || aSel == 3) break;

            if (aSel == 0) {
                if (TaskHistory::Instance().PauseTask(t.id)) {
                    std::cout << "\033[1;35m[OK] Task #" << t.id << " paused.\033[0m\n";
                } else {
                    std::cout << "\033[1;31m[WARN] Could not pause Task #" << t.id << " (Task not running).\033[0m\n";
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } else if (aSel == 1) {
                if (TaskHistory::Instance().ResumeTask(t.id)) {
                    std::cout << "\033[1;32m[OK] Task #" << t.id << " resumed.\033[0m\n";
                } else {
                    std::cout << "\033[1;31m[WARN] Could not resume Task #" << t.id << " (Task not paused).\033[0m\n";
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } else if (aSel == 2) {
                if (TaskHistory::Instance().KillTask(t.id)) {
                    std::cout << "\033[1;31m[OK] Task #" << t.id << " killed / terminated.\033[0m\n";
                } else {
                    std::cout << "\033[1;31m[WARN] Could not kill Task #" << t.id << ".\033[0m\n";
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    static void ShowTaskLibrary() {
        int selectedRow = 0;
        auto initialStyle = OpenTUI::GetThemeStyle(g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
        OpenTUI::TerminalEngine::ClearScreen(initialStyle.panelBg);

        while (true) {
            auto style = OpenTUI::GetThemeStyle(g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            std::ostringstream ss;
            ss << "\033[H" << style.panelBg;

            // Capture banner into ss buffer
            auto* oldBuf = std::cout.rdbuf(ss.rdbuf());
            std::cout.rdbuf(oldBuf);

            auto tasks = TaskHistory::Instance().Snapshot();
            int nRunning = 0, nDone = 0, nFailed = 0;
            uintmax_t totalFreed = 0;
            for (auto& t : tasks) {
                if (t.status == TaskStatus::Running || t.status == TaskStatus::Queued) ++nRunning;
                else if (t.status == TaskStatus::Completed) { ++nDone; totalFreed += t.bytesFreed; }
                else if (t.status == TaskStatus::Failed || t.status == TaskStatus::Cancelled) ++nFailed;
            }

            if (!tasks.empty()) {
                if (selectedRow < 0) selectedRow = static_cast<int>(tasks.size()) - 1;
                if (selectedRow >= static_cast<int>(tasks.size())) selectedRow = 0;
            } else {
                selectedRow = 0;
            }

            ss << OpenTUI::Box::DrawBorder(80, "INTERACTIVE TASK MANAGER", g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            std::string hotkeyBar = "[HOTKEYS] Enter/D/V: Details │ K: Kill │ P: Pause │ R: Resume │ C: Clear │ ESC: Exit";
            ss << OpenTUI::Box::DrawLine(80, hotkeyBar, false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            ss << OpenTUI::Box::DrawDivider(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            std::ostringstream summaryLine;
            summaryLine << "Tasks: " << nDone << " Done │ " << nRunning << " Running │ " << nFailed << " Failed │ Freed: " << Cleaner::FormatSize(totalFreed) << " │ Total: " << tasks.size();
            ss << OpenTUI::Box::DrawLine(80, summaryLine.str(), false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            ss << OpenTUI::Box::DrawDivider(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            if (tasks.empty()) {
                ss << OpenTUI::Box::DrawLine(80, "No tasks recorded yet. Run a clean/scan/AQL query to populate.", false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            } else {
                for (size_t i = 0; i < tasks.size(); ++i) {
                    const auto& t = tasks[i];
                    bool isSelected = (static_cast<int>(i) == selectedRow);
                    bool isLive = (t.status == TaskStatus::Running || t.status == TaskStatus::Queued);

                    std::string statusStr = isLive ? "RUNNING" : TaskHistoryNS::StatusLabel(t.status);
                    std::string detail = isLive
                        ? (t.progressMsg.empty() ? statusStr : t.progressMsg)
                        : (t.resultSummary.empty() ? statusStr : t.resultSummary);

                    std::ostringstream rowSS;
                    rowSS << "[" << std::setfill('0') << std::setw(3) << t.id << std::setfill(' ') << "] "
                          << std::left << std::setw(10) << statusStr << " "
                          << std::setw(8)  << t.category << " "
                          << std::setw(18) << (t.name.size() > 18 ? t.name.substr(0, 15) + "..." : t.name) << " "
                          << (detail.size() > 22 ? detail.substr(0, 19) + "..." : detail);

                    ss << OpenTUI::Box::DrawLine(80, rowSS.str(), isSelected, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
                }
            }

            ss << OpenTUI::Box::DrawDivider(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            std::string footerHint = "Use UP/DOWN to select task · Press Enter/D to view Details · ESC to return";
            ss << OpenTUI::Box::DrawLine(80, footerHint, false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            ss << OpenTUI::Box::DrawFooter(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            ss << style.panelBg << "\033[0m\033[J";
            std::cout << ss.str() << std::flush;

            OpenTUI::KeyEvent ev = OpenTUI::TerminalEngine::ReadKey();

            while (OpenTUI::TerminalEngine::HasKeyPending()) {
                OpenTUI::KeyEvent nextEv = OpenTUI::TerminalEngine::ReadKey();
                if ((nextEv.key == OpenTUI::Key::Up || nextEv.key == OpenTUI::Key::Down) && nextEv.key == ev.key) {
                    if (ev.key == OpenTUI::Key::Up) {
                        if (!tasks.empty()) selectedRow = (selectedRow > 0) ? selectedRow - 1 : static_cast<int>(tasks.size()) - 1;
                    } else if (ev.key == OpenTUI::Key::Down) {
                        if (!tasks.empty()) selectedRow = (selectedRow + 1) % static_cast<int>(tasks.size());
                    }
                } else {
                    ev = nextEv;
                    break;
                }
            }

            if (ev.key == OpenTUI::Key::Up) {
                if (!tasks.empty()) selectedRow = (selectedRow > 0) ? selectedRow - 1 : static_cast<int>(tasks.size()) - 1;
            } else if (ev.key == OpenTUI::Key::Down) {
                if (!tasks.empty()) selectedRow = (selectedRow + 1) % static_cast<int>(tasks.size());
            } else if (ev.key == OpenTUI::Key::Escape || (ev.key == OpenTUI::Key::Char && (ev.ch == 'q' || ev.ch == 'Q'))) {
                break;
            } else if ((ev.key == OpenTUI::Key::Enter || (ev.key == OpenTUI::Key::Char && (ev.ch == 'd' || ev.ch == 'D' || ev.ch == 'v' || ev.ch == 'V'))) && !tasks.empty() && selectedRow >= 0 && selectedRow < static_cast<int>(tasks.size())) {
                ShowTaskActionDialog(tasks[selectedRow].id);
            } else if (ev.key == OpenTUI::Key::Char && !tasks.empty() && selectedRow >= 0 && selectedRow < static_cast<int>(tasks.size())) {
                uint64_t targetId = tasks[selectedRow].id;
                char c = static_cast<char>(std::tolower(ev.ch));
                if (c == 'k') {
                    TaskHistory::Instance().KillTask(targetId);
                } else if (c == 'p') {
                    TaskHistory::Instance().PauseTask(targetId);
                } else if (c == 'r') {
                    TaskHistory::Instance().ResumeTask(targetId);
                } else if (c == 'c') {
                    TaskHistory::Instance().Clear();
                    selectedRow = 0;
                }
            }
        }
    }

    static void ShowSettingsMenu(Cleaner& cleaner) {
        int currentSelected = 0;
        while (true) {
#ifdef _WIN32
            static const std::string osName = "Windows";
#elif defined(__APPLE__)
            static const std::string osName = "macOS";
#else
            static const std::string osName = "Linux";
#endif
            std::vector<std::string> settingsOptions = {
                std::string("TUI Engine Theme:  [") + g_tuiSettings.tuiThemeEngine + " - OpenTUI / TermOx / FTXUI]",
                std::string("TUI Color Palette: [") + g_tuiSettings.tuiColorScheme + "]",
                std::string("Foreground Color:  [") + g_tuiSettings.tuiFgColor + "]",
                std::string("Background Color:  [") + g_tuiSettings.tuiBgColor + "]",
                std::string("Logs File Output:  [") + (g_tuiSettings.enableLogging ? "ON  - system-cleaner-agent.log" : "OFF - Disabled") + "]",
                std::string("Log Level:         [") + g_tuiSettings.logLevel + "]",
                std::string("Sandbox Mode:      [") + (g_tuiSettings.sandboxMode ? "ON  - Preview Only" : "OFF - REAL DELETION ALLOWED") + "]",
                std::string("Path Protection:   [") + (g_tuiSettings.pathProtection ? "ON  - System Dir Guard" : "OFF - Disabled") + "]",
                std::string("Dry-Run Mode:      [") + (g_tuiSettings.dryRun ? "ON  - Preview Only" : "OFF - REAL CLEAN") + "]",
                std::string("Kill Locks:        [") + (g_tuiSettings.killLocks ? "ON" : "OFF") + "]",
                std::string("Monitor Interval:  [") + std::to_string(g_tuiSettings.monitorIntervalSec)
                    + (g_tuiSettings.monitorIntervalSec == 1 ? " second (LIVE)]" : " seconds]"),
                std::string("Target Folders:    [") + g_tuiSettings.customPathsStr.substr(0, 40) + (g_tuiSettings.customPathsStr.size() > 40 ? "..." : "") + "]",
                std::string("Reset to OS Defaults (" + osName + " safe temp/cache paths)"),
                "Save & Return to Dashboard"
            };

            OpenTUI::Menu settingsMenu("SETTINGS & THEMES", settingsOptions, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            settingsMenu.SetSelectedIndex(currentSelected);
            OpenTUI::MenuSelection sel = settingsMenu.ShowExtended();

            if (sel.index != -1) {
                currentSelected = sel.index;
            }

            if (sel.index == -1 || sel.index == 13) {
                cleaner.SetSandbox(g_tuiSettings.sandboxMode);
                cleaner.SetDangerousPathProtection(g_tuiSettings.pathProtection);
                cleaner.SetDryRun(g_tuiSettings.dryRun);
                cleaner.SetKillLockingProcesses(g_tuiSettings.killLocks);

                if (!g_tuiSettings.customPathsStr.empty()) {
                    std::vector<fs::path> paths;
                    std::stringstream ss(g_tuiSettings.customPathsStr);
                    std::string item;
                    while (std::getline(ss, item, ',')) {
                        while (!item.empty() && (item.front() == ' ' || item.front() == '"')) item.erase(0, 1);
                        while (!item.empty() && (item.back() == ' ' || item.back() == '"')) item.pop_back();
                        if (!item.empty()) paths.push_back(fs::path(item));
                    }
                    cleaner.SetCustomPaths(paths);
                }
                SaveTUISettings();
                break;
            }

            static const std::vector<std::string> engines = { "OpenTUI", "TermOx", "FTXUI" };
            static const std::vector<std::string> schemes = {
                "Default", "Dracula Dark", "Tokyo Night", "Nordic Frost", "Catppuccin Mocha", "Cyberpunk 2077", "Solarized Ocean", "Cyan Matrix", "Electric Magenta", "Amber Gold", "Emerald Cyber", "Neon Cyberpunk", "Monochrome Slate"
            };
            static const std::vector<std::string> fgColors = {
                "Default", "Cyan", "Electric Magenta", "Amber Gold", "Emerald Green", "Neon Pink", "Bright White", "Yellow", "Royal Blue"
            };
            static const std::vector<std::string> bgColors = {
                "Default", "Black", "Dracula Charcoal", "Tokyo Night", "Nordic Frost", "Solarized Ocean", "Navy Blue", "Electric Magenta", "Amber Gold", "Emerald Green", "Dark Slate", "Charcoal Gray"
            };
            static const std::vector<std::string> logLevels = {
                "INFO", "WARN", "ERROR", "VERBOSE"
            };

            switch (sel.index) {
                case 0: { // Engine Theme
                    auto it = std::find(engines.begin(), engines.end(), g_tuiSettings.tuiThemeEngine);
                    int idx = (it != engines.end()) ? static_cast<int>(std::distance(engines.begin(), it)) : 0;
                    if (sel.actionKey == OpenTUI::Key::Left) {
                        idx = (idx > 0) ? idx - 1 : static_cast<int>(engines.size()) - 1;
                    } else {
                        idx = (idx + 1) % static_cast<int>(engines.size());
                    }
                    g_tuiSettings.tuiThemeEngine = engines[idx];
                    break;
                }
                case 1: { // Color Scheme Palette
                    auto it = std::find(schemes.begin(), schemes.end(), g_tuiSettings.tuiColorScheme);
                    int idx = (it != schemes.end()) ? static_cast<int>(std::distance(schemes.begin(), it)) : 0;
                    if (sel.actionKey == OpenTUI::Key::Left) {
                        idx = (idx > 0) ? idx - 1 : static_cast<int>(schemes.size()) - 1;
                    } else {
                        idx = (idx + 1) % static_cast<int>(schemes.size());
                    }
                    g_tuiSettings.tuiColorScheme = schemes[idx];
                    break;
                }
                case 2: { // Foreground Text Color
                    auto it = std::find(fgColors.begin(), fgColors.end(), g_tuiSettings.tuiFgColor);
                    int idx = (it != fgColors.end()) ? static_cast<int>(std::distance(fgColors.begin(), it)) : 0;
                    if (sel.actionKey == OpenTUI::Key::Left) {
                        idx = (idx > 0) ? idx - 1 : static_cast<int>(fgColors.size()) - 1;
                    } else {
                        idx = (idx + 1) % static_cast<int>(fgColors.size());
                    }
                    g_tuiSettings.tuiFgColor = fgColors[idx];
                    break;
                }
                case 3: { // Background Container Color
                    auto it = std::find(bgColors.begin(), bgColors.end(), g_tuiSettings.tuiBgColor);
                    int idx = (it != bgColors.end()) ? static_cast<int>(std::distance(bgColors.begin(), it)) : 0;
                    if (sel.actionKey == OpenTUI::Key::Left) {
                        idx = (idx > 0) ? idx - 1 : static_cast<int>(bgColors.size()) - 1;
                    } else {
                        idx = (idx + 1) % static_cast<int>(bgColors.size());
                    }
                    g_tuiSettings.tuiBgColor = bgColors[idx];
                    break;
                }
                case 4:
                    g_tuiSettings.enableLogging = !g_tuiSettings.enableLogging;
                    Logger::Instance().SetFileLogging(g_tuiSettings.enableLogging);
                    break;
                case 5: { // Log Level
                    auto it = std::find(logLevels.begin(), logLevels.end(), g_tuiSettings.logLevel);
                    int idx = (it != logLevels.end()) ? static_cast<int>(std::distance(logLevels.begin(), it)) : 0;
                    if (sel.actionKey == OpenTUI::Key::Left) {
                        idx = (idx > 0) ? idx - 1 : static_cast<int>(logLevels.size()) - 1;
                    } else {
                        idx = (idx + 1) % static_cast<int>(logLevels.size());
                    }
                    g_tuiSettings.logLevel = logLevels[idx];
                    Logger::Instance().SetVerbose(g_tuiSettings.logLevel == "VERBOSE");
                    break;
                }
                case 6: g_tuiSettings.sandboxMode = !g_tuiSettings.sandboxMode; break;
                case 7: g_tuiSettings.pathProtection = !g_tuiSettings.pathProtection; break;
                case 8: g_tuiSettings.dryRun = !g_tuiSettings.dryRun; break;
                case 9: g_tuiSettings.killLocks = !g_tuiSettings.killLocks; break;
                case 10: { // Monitor Interval
                    // 1 second = Live mode (max refresh rate)
                    // 3, 5, 10, 15, 30, 60 = normal intervals
                    static const std::vector<int> intervals = { 1, 3, 5, 10, 15, 30, 60 };
                    auto it = std::find(intervals.begin(), intervals.end(), g_tuiSettings.monitorIntervalSec);
                    int idx = (it != intervals.end()) ? static_cast<int>(std::distance(intervals.begin(), it)) : 2;  // default to 5s (index 2)
                    if (sel.actionKey == OpenTUI::Key::Left) {
                        idx = (idx > 0) ? idx - 1 : static_cast<int>(intervals.size()) - 1;
                    } else {
                        idx = (idx + 1) % static_cast<int>(intervals.size());
                    }
                    g_tuiSettings.monitorIntervalSec = intervals[idx];
                    break;
                }
                case 11: { // Target Folders
                    OpenTUI::TerminalEngine::ClearScreen();
                    std::string newPath = OpenTUI::TextInput::ReadLine("Enter target PATH folders (comma-separated): ", g_tuiSettings.customPathsStr);
                    if (!newPath.empty()) g_tuiSettings.customPathsStr = newPath;
                    break;
                }
                case 12: { // Reset to OS Defaults
                    g_tuiSettings.customPathsStr = SecurityGuard::GetDefaultCleanPathsStr();
                    std::cout << "\033[1;32m[OK] Target folders reset to OS-default safe temp/cache paths.\033[0m\n";
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    break;
                }
            }
            SaveTUISettings();
        }
    }

    static std::string SelectAgentQuery() {
        static std::vector<std::string> aqlHistory;

        std::vector<std::string> queryMenuOptions = {
            "[Custom Query / Interactive Console]",
            "CLEAN WHERE FREE_DISK < 500MB",
            "KILL PROCESS WHERE RAM > 200MB",
            "MONITOR WHERE RAM > 80%",
            "SHRED WHERE SIZE > 10MB",
            "PURGE RECYCLE_BIN",
            "SCAN WHERE AGE > 24H"
        };

        OpenTUI::Menu agentMenu("AGENT QUERY CONSOLE", queryMenuOptions, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
        int choice = agentMenu.Show();

        // CRITICAL: ESC at the AQL menu returns -1 to go back to the main menu.
        // The user should NOT be forced into the custom query loop.
        if (choice == -1) return "";

        static const std::vector<std::string> suggestions = {
            "CLEAN", "SCAN", "SHRED", "KILL", "MONITOR", "PURGE", "SELECT", "WIPE", "EMPTY",
            "WHERE", "RAM", "FREE_DISK", "DISK_C", "DISK_FREE", "SIZE", "AGE", "EVERY", "WHEN", "PROCESS",
            "KILL chrome.exe FROM PROCESS WHERE RAM > 200MB",
            "KILL chrome.exe WHEN RAM > 1GB",
            "KILL msedge.exe FROM PROCESS WHERE RAM > 300MB",
            "KILL notepad.exe FROM PROCESS WHERE RAM > 80%",
            "CLEAN 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE FREE_DISK < 500MB",
            "CLEAN TEMP_C WHERE DISK_C < 500MB",
            "CLEAN APPDATA WHERE DISK_C < 1GB",
            "CLEAN CACHE WHERE DISK_C < 2GB",
            "CLEAN '/tmp' WHERE FREE_DISK < 500MB",
            "CLEAN '/var/log' WHERE SIZE > 100MB",
            "CLEAN '~/.cache' WHERE SIZE > 500MB",
            "CLEAN '~/Library/Caches' WHERE SIZE > 1GB",
            "MONITOR WHERE RAM > 80% EVERY 15S",
            "MONITOR WHERE EXT IN ('.pyc', '.cache', 'node_modules') EVERY 5H",
            "SHRED 'C:\\Temp' WHERE SIZE > 10MB",
            "PURGE RECYCLE_BIN",
            "PURGE TRASH",
            "SELECT chrome.exe FROM PROCESS",
            "SCAN 'C:\\' WHERE AGE > 24H"
        };

        std::string lastErr = "";
        std::string lastHint = "";

        auto showHelp = [&]() {
            OpenTUI::TerminalEngine::ClearScreen();
            std::cout << "\033[1;36m╔══════════════════════════════════════════════════════════════════════════════╗\033[0m\n";
            std::cout << "\033[1;36m║\033[1;97m          AGENT QUERY LANGUAGE (AQL)  —  Command Reference                  \033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m╠══════════════════════════════════════════════════════════════════════════════╣\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[1;33m  COMMANDS\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;32mCLEAN\033[0m   <path> WHERE <condition>   \033[90mDelete matching cache/temp files\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;32mSCAN\033[0m    <path> WHERE <condition>   \033[90mDry-run preview of cleanable items\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;32mSHRED\033[0m   <path> WHERE <condition>   \033[90mSecure zero-overwrite wipe before delete\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;32mKILL\033[0m    PROCESS WHERE <condition>  \033[90mTerminate high-RAM / matching processes\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;32mMONITOR\033[0m <condition> EVERY <dur>    \033[90mWatch thresholds & auto-clean on trigger\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;32mPURGE\033[0m   RECYCLE_BIN                \033[90mEmpty OS Recycle Bin / Linux Trash\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m╠══════════════════════════════════════════════════════════════════════════════╣\033[0m\n";
            std::cout << "\033[1;36m║  \033[90mTIP: TAB auto-completes · UP/DOWN for history · ESC cancels · Enter runs\033[0m\n";
            std::cout << "\033[1;36m╚══════════════════════════════════════════════════════════════════════════════╝\033[0m\n\n";

            if (!lastErr.empty()) {
                std::cout << "\033[1;31m[AQL SYNTAX ERROR] " << lastErr << "\033[0m\n";
                std::cout << "\033[1;33m[STRICT AQL GRAMMAR RULES & EXAMPLES]\n" << lastHint << "\033[0m\n\n";
            }
        };

        if (choice <= 0) {
            // Custom query input loop — stays in same page on syntax error!
            while (true) {
                showHelp();
                std::string q = OpenTUI::TextInput::ReadLine("Query: ", "CLEAN 'C:/Users/hasee/AppData/Local/Temp' WHERE FREE_DISK < 500MB", suggestions, aqlHistory);
                if (q.empty()) return "";

                std::string qLower = q;
                std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);
                if (qLower == "help" || qLower == "?" || qLower == "h") {
                    lastErr = "";
                    lastHint = "";
                    continue;
                }

                // Check if query is AQL and validate grammar
                std::string sqUpper = q;
                std::transform(sqUpper.begin(), sqUpper.end(), sqUpper.begin(), ::toupper);
                static const std::vector<std::string> aqlVerbs = {
                    "KILL", "SELECT", "CLEAN", "SCAN", "SHRED", "PURGE", "MONITOR", "WIPE", "EMPTY"
                };
                bool isAQL = false;
                for (const auto& verb : aqlVerbs) {
                    if (sqUpper.find(verb) == 0 || sqUpper.find(" " + verb + " ") != std::string::npos || sqUpper.find(verb + " ") == 0) {
                        isAQL = true;
                        break;
                    }
                }

                if (isAQL) {
                    AQLEngine::ValidationResult val = AQLEngine::Validate(q);
                    if (!val.isValid) {
                        lastErr = val.errorMessage;
                        lastHint = val.suggestedHint;
                        continue; // Stay in same AQL Console page!
                    }
                }

                // Valid query — save to history and return!
                if (aqlHistory.empty() || aqlHistory.back() != q) {
                    aqlHistory.push_back(q);
                }
                return q;
            }
        }

        static const std::vector<std::string> preMadeQueries = {
            "KILL chrome.exe FROM PROCESS WHERE RAM > 80%",
            "CLEAN TEMP_C WHERE DISK_C < 500MB",
            "CLEAN APPDATA WHERE DISK_C < 1GB",
            "MONITOR WHERE EXT IN ('.pyc', '.cache', 'node_modules') EVERY 5H",
            "SHRED 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE SIZE > 10MB",
            "PURGE RECYCLE_BIN"
        };

        return preMadeQueries[choice - 1];
    }

    static void ShowDiskCleanerSubmenu(Cleaner& cleaner) {
        std::string driveName = SYSTEM_PRIMARY_DRIVE;
        std::vector<std::string> subOptions = {
            "Storage Scan (Preview All Cleanable Space)",
            "Deep Storage Scan & Hotspot Analyzer (Multi-Drive & Large File Bloat)",
            "Smart Deep Clean (Clean All Temp & Cache Targets)",
            "Preset: " + driveName + " System Temp & Update Downloads",
            "Preset: " + driveName + " Crash Dumps & System Shader Logs",
            "Preset: Web Browser Caches (Chrome/Edge/Firefox/Brave/Safari)",
            "Preset: Developer Cache Suite (npm/pip/cargo/gradle/uv/pnpm)",
            "Preset: IDE & Messaging Caches (VS Code/Cursor/Discord/Telegram)",
            "Secure Shred Wipe",
            "Empty OS Recycle Bin / Trash",
            "Deep Scan (Interactive Tree, Hotspot Analyzer & JSON Export)",
            "Back"
        };
        OpenTUI::Menu subMenu("DISK CLEANER SUITE (" + GetCurrentOSNameStr() + " - " + driveName + ")", subOptions, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
        int sel = subMenu.Show();

        // CRITICAL: ESC (sel == -1) or "Back" option (sel == 11) returns to main menu.
        if (sel == -1 || sel == 11) return;

        bool currentDryRun = g_tuiSettings.dryRun || g_tuiSettings.sandboxMode;

        if (sel == 0) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Storage Scan", "scan --dry-run", "Disk Cleaner");
            std::thread worker([&cleaner, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive("Storage Scan");
                cleaner.SetDryRun(true);
                auto reports = cleaner.Scan();
                uintmax_t freed = 0;
                for (const auto& r : reports) freed += r.sizeBytes;
                g_tuiStatus.SetCompleted("Scan completed. Cleanable: " + Cleaner::FormatSize(freed));
                TaskHistory::Instance().MarkCompleted(tid, "Scan found " + Cleaner::FormatSize(freed) + " cleanable across " + std::to_string(reports.size()) + " targets", freed, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 1) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Deep Storage Hotspot Scan", "deepscan --all-drives", "Disk Cleaner");
            std::thread worker([&cleaner, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive("Deep Storage Hotspot Scan...");
                DeepScanResult res = cleaner.DeepScan(100);
                std::string summary = "Deep Scan found " + Cleaner::FormatSize(res.totalCleanableCachesBytes) + " caches and " + std::to_string(res.largeFiles.size()) + " large files (>100MB)";
                g_tuiStatus.SetCompleted(summary);
                TaskHistory::Instance().MarkCompleted(tid, summary, res.totalCleanableCachesBytes, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 2) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Smart Deep Clean", currentDryRun ? "clean --dry-run" : "clean --real", "Disk Cleaner");
            std::thread worker([&cleaner, currentDryRun, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive(currentDryRun ? "Smart Deep Clean (DRY-RUN)" : "Smart Deep Clean (REAL DELETE)");
                cleaner.SetDryRun(currentDryRun);
                cleaner.Clean();
                g_tuiStatus.SetCompleted("Smart Deep Clean finished.");
                TaskHistory::Instance().MarkCompleted(tid, "Smart Deep Clean finished.", 0, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 3) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Preset: OS Temp & Updates", "clean preset-temp", "Disk Cleaner");
            std::thread worker([&cleaner, currentDryRun, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive("Cleaning OS Temp & Updates...");
                cleaner.SetDryRun(currentDryRun);
                cleaner.SetIncludeCategories({"system"});
                cleaner.Clean();
                cleaner.SetIncludeCategories({});
                g_tuiStatus.SetCompleted("OS Temp & Update cleanup finished.");
                TaskHistory::Instance().MarkCompleted(tid, "OS Temp & Update cleanup finished.", 0, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 4) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Preset: Crash Dumps & Logs", "clean preset-logs", "Disk Cleaner");
            std::thread worker([&cleaner, currentDryRun, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive("Cleaning Crash Dumps & System Logs...");
                cleaner.SetDryRun(currentDryRun);
                cleaner.SetIncludeCategories({"system"});
                cleaner.Clean();
                cleaner.SetIncludeCategories({});
                g_tuiStatus.SetCompleted("Crash Dumps & System Logs cleanup finished.");
                TaskHistory::Instance().MarkCompleted(tid, "Crash Dumps & System Logs cleanup finished.", 0, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 4) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Preset: Browser Caches", "clean preset-browser", "Disk Cleaner");
            std::thread worker([&cleaner, currentDryRun, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive("Cleaning Web Browser Caches...");
                cleaner.SetDryRun(currentDryRun);
                cleaner.SetIncludeCategories({"browser"});
                cleaner.Clean();
                cleaner.SetIncludeCategories({});
                g_tuiStatus.SetCompleted("Browser Caches cleanup finished.");
                TaskHistory::Instance().MarkCompleted(tid, "Browser Caches cleanup finished.", 0, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 5) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Preset: Developer Caches", "clean preset-dev", "Disk Cleaner");
            std::thread worker([&cleaner, currentDryRun, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive("Cleaning Developer Cache Suite...");
                cleaner.SetDryRun(currentDryRun);
                cleaner.SetIncludeCategories({"developer"});
                cleaner.Clean();
                cleaner.SetIncludeCategories({});
                g_tuiStatus.SetCompleted("Developer Caches cleanup finished.");
                TaskHistory::Instance().MarkCompleted(tid, "Developer Caches cleanup finished.", 0, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 6) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Preset: Messaging & IDE Caches", "clean preset-apps", "Disk Cleaner");
            std::thread worker([&cleaner, currentDryRun, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive("Cleaning Messaging & IDE Caches...");
                cleaner.SetDryRun(currentDryRun);
                cleaner.SetIncludeCategories({"messaging", "applications"});
                cleaner.Clean();
                cleaner.SetIncludeCategories({});
                g_tuiStatus.SetCompleted("Messaging & IDE Caches cleanup finished.");
                TaskHistory::Instance().MarkCompleted(tid, "Messaging & IDE Caches cleanup finished.", 0, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 7) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Secure Shred Wipe", currentDryRun ? "shred --dry-run" : "shred --real", "Disk Cleaner");
            std::thread worker([&cleaner, currentDryRun, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive("Secure Shred Wipe");
                cleaner.SetDryRun(currentDryRun);
                cleaner.SetMode(CleanMode::Shred);
                cleaner.Clean();
                g_tuiStatus.SetCompleted("Secure Shred Wipe finished.");
                TaskHistory::Instance().MarkCompleted(tid, "Secure Shred Wipe finished.", 0, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 8) {
            uint64_t tid = TaskHistory::Instance().Register("TUI", "Empty Recycle Bin / Trash", "empty-recycle-bin", "Disk Cleaner");
            std::thread worker([&cleaner, tid]() {
                TaskHistory::Instance().MarkRunning(tid);
                g_tuiStatus.SetActive("Empty Recycle Bin / Trash");
                cleaner.EmptyWindowsRecycleBin();
                g_tuiStatus.SetCompleted("Recycle Bin / Trash emptied.");
                TaskHistory::Instance().MarkCompleted(tid, "OS Recycle Bin / Trash purged.", 0, 0, 0, 0);
            });
            worker.detach();
        } else if (sel == 9) {
            // Delegate to ShowDeepScanSubmenu so Disk Cleaner also gets full Deep Scan power
            ShowDeepScanSubmenu(cleaner);
        }
    }

    static void ShowRamCleanerSubmenu(Cleaner& cleaner) {
        while (true) {
            OpenTUI::TerminalEngine::ClearScreen();
            std::cout << "\033[1;36m================================================================================\033[0m\n";
            std::cout << "\033[1;97m                 RAM CLEANER & PROCESS PERMISSION MANAGEMENT                    \033[0m\n";
            std::cout << "\033[1;36m================================================================================\033[0m\n\n";

            double curMemPct = SmartScheduler::GetMemoryUsagePercent();
            std::cout << "  System Memory (RAM): " << OpenTUI::ProgressBar::Render(curMemPct, 35, "% used") << "\n\n";

            auto highRamGroups = ProcessManager::GetAggregatedProcessGroups(200ULL * 1024 * 1024);

            std::cout << "\033[1;33mHigh-RAM Applications (> 200 MB Total RAM):\033[0m\n";
            if (highRamGroups.empty()) {
                std::cout << "  \033[32m[SAFE] No process applications consuming > 200 MB RAM detected.\033[0m\n\n";
            } else {
                for (size_t i = 0; i < highRamGroups.size(); ++i) {
                    const auto& grp = highRamGroups[i];
                    std::string ramStr = Cleaner::FormatSize(grp.totalMemoryUsageBytes);
                    std::string countStr = (grp.instanceCount > 1) ? (" (" + std::to_string(grp.instanceCount) + " processes)") : (" (PID: " + (grp.pids.empty() ? "?" : std::to_string(grp.pids[0])) + ")");
                    std::string tag = grp.isProtected ? "  \033[32m[PROTECTED APP]\033[0m" : "  \033[1;31m[PERMISSION REQUIRED]\033[0m";
                    std::cout << "  [" << (i + 1) << "] " << grp.processName << countStr << " - " << ramStr << tag << "\n";
                }
                std::cout << "\n";
            }

            std::vector<std::string> ramMenuOptions = {
                "Quick RAM Optimization (Flush Candidate Background Processes)",
                "OS System Memory Working Set Trimming",
                "Browser Memory Purge (Clean High-RAM Browser Instances)",
                "Terminate Specific High-RAM Process (> 200 MB)",
                "Manual Process List & Hot-Key Terminate (All > 200 MB)",
                "Add Process Name to Protection Whitelist",
                "Release Process Lock Handles",
                "Return to Dashboard"
            };

            OpenTUI::Menu ramMenu("RAM CLEANER PERMISSIONS", ramMenuOptions, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            int ramChoice = ramMenu.Show();

            if (ramChoice == -1 || ramChoice == 7) break;

            if (ramChoice == 0) {
                bool currentDryRun = g_tuiSettings.dryRun || g_tuiSettings.sandboxMode;
                size_t count = ProcessManager::KillHighMemoryProcesses(200ULL * 1024 * 1024, !currentDryRun, true);
                std::cout << "\033[1;32mQuick RAM Optimization complete. Candidate processes processed: " << count << "\033[0m\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            } else if (ramChoice == 1) {
                size_t trimmed = ProcessManager::TrimSystemWorkingSet();
                std::cout << "\033[1;32mOS System Memory Working Set Trimmed across " << trimmed << " process(es).\033[0m\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            } else if (ramChoice == 2) {
                bool currentDryRun = g_tuiSettings.dryRun || g_tuiSettings.sandboxMode;
                size_t k1 = GTLIBC::GTLibc::KillProcessByName("chrome.exe", !currentDryRun, true);
                size_t k2 = GTLIBC::GTLibc::KillProcessByName("msedge.exe", !currentDryRun, true);
                size_t k3 = GTLIBC::GTLibc::KillProcessByName("firefox.exe", !currentDryRun, true);
                size_t k4 = GTLIBC::GTLibc::KillProcessByName("brave.exe", !currentDryRun, true);
                std::cout << "\033[1;32mBrowser Memory Purge finished. Terminated " << (k1 + k2 + k3 + k4) << " browser instance(s).\033[0m\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            } else if (ramChoice == 3) {
                auto groups = ProcessManager::GetAggregatedProcessGroups(200ULL * 1024 * 1024);
                if (groups.empty()) {
                    std::cout << "\033[1;32m[SAFE] No process applications consuming > 200 MB RAM currently detected on system.\033[0m\n";
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                } else {
                    std::vector<std::string> killOptions;
                    for (const auto& grp : groups) {
                        std::string countStr = (grp.instanceCount > 1) ? (" (" + std::to_string(grp.instanceCount) + " procs)") : (" (PID: " + (grp.pids.empty() ? "?" : std::to_string(grp.pids[0])) + ")");
                        std::string statusLabel = grp.isProtected ? " [PROTECTED APP]" : " [PERMISSION REQUIRED]";
                        killOptions.push_back(grp.processName + countStr + " - RAM: " + Cleaner::FormatSize(grp.totalMemoryUsageBytes) + statusLabel);
                    }
                    killOptions.push_back("Cancel");

                    OpenTUI::Menu killMenu("SELECT PROCESS TO TERMINATE (> 200 MB RAM)", killOptions, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
                    int kChoice = killMenu.Show();

                    if (kChoice >= 0 && kChoice < static_cast<int>(groups.size())) {
                        const auto& targetGrp = groups[kChoice];
                        if (targetGrp.isProtected) {
                            std::cout << "\033[1;31m[WARNING] '" << targetGrp.processName << "' is classified as a protected application.\033[0m\n";
                        }
                        std::string details = (targetGrp.instanceCount > 1) 
                            ? ("all " + std::to_string(targetGrp.instanceCount) + " process instance(s) of " + targetGrp.processName + " (Total RAM: " + Cleaner::FormatSize(targetGrp.totalMemoryUsageBytes) + ")")
                            : (targetGrp.processName + " (PID: " + (targetGrp.pids.empty() ? "?" : std::to_string(targetGrp.pids[0])) + ", RAM: " + Cleaner::FormatSize(targetGrp.totalMemoryUsageBytes) + ")");

                        std::string confirmPrompt = "Grant explicit permission to terminate " + details + "? [y/N]: ";
                        std::string confirm = OpenTUI::TextInput::ReadLine(confirmPrompt, "n");
                        if (confirm == "y" || confirm == "Y" || confirm == "yes") {
                            size_t killed = 0;
                            if (targetGrp.instanceCount > 1) {
                                killed = GTLIBC::GTLibc::KillProcessByName(targetGrp.processName, true, true);
                            } else if (!targetGrp.pids.empty()) {
                                if (GTLIBC::GTLibc::KillProcess(targetGrp.pids[0])) killed = 1;
                            }
                            if (killed > 0) {
                                std::cout << "\033[1;32mSuccessfully terminated " << killed << " process(es) of " << targetGrp.processName << "\033[0m\n";
                                Logger::Instance().Info("User granted explicit permission: Terminated " + std::to_string(killed) + " process(es) of " + targetGrp.processName);
                            } else {
                                std::cout << "\033[1;31mFailed to terminate process (Access Denied or process already exited).\033[0m\n";
                            }
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                        } else {
                            std::cout << "\033[1;36mOperation cancelled by user. Process preserved.\033[0m\n";
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                    }
                }
            } else if (ramChoice == 4) {
                // ----------------------------------------------------------------
                // Manual Process List & Hot-Key Terminate (All > 200 MB)
                // ----------------------------------------------------------------
                // Display all processes currently using > 200 MB RAM in a navigable
                // list. Hot-keys:
                //   [UP/DOWN]   = Navigate the list
                //   [K]         = INSTANT KILL selected process (no extra confirm)
                //   [ENTER]     = Kill selected process WITH y/N confirmation
                //   [Q] / [ESC] = Cancel / return to RAM Cleaner
                // ----------------------------------------------------------------
                {
                    auto highProcs = ProcessManager::GetHighMemoryCandidates(200ULL * 1024 * 1024);
                    if (highProcs.empty()) {
                        OpenTUI::TerminalEngine::ClearScreen();
                        std::cout << "\033[1;32m[SAFE] No processes currently consuming > 200 MB RAM on the system.\033[0m\n";
                        std::cout << "\n\033[90m(Press Enter to return...)\033[0m";
                        std::cin.get();
                    } else {
                        // Sort by RAM descending so the biggest bloat is at the top
                        std::sort(highProcs.begin(), highProcs.end(), [](const GTLIBC::ProcessInfo& a, const GTLIBC::ProcessInfo& b) {
                            return a.memoryUsageBytes > b.memoryUsageBytes;
                        });

                        size_t selected = 0;
                        bool procListDone = false;
                        while (!procListDone) {
                            OpenTUI::TerminalEngine::ClearScreen();
                            std::cout << "\033[1;36m================================================================================\033[0m\n";
                            std::cout << "\033[1;97m            MANUAL PROCESS LIST - ALL PROCESSES > 200 MB RAM" << std::string(30, ' ') << "\033[0m\n";
                            std::cout << "\033[1;36m================================================================================\033[0m\n\n";
                            std::cout << "\033[1;33mHot-Keys: [UP/DOWN]=Navigate | [K]=INSTANT KILL | [ENTER]=Kill with confirm | [Q/ESC]=Cancel\033[0m\n\n";
                            std::cout << "  \033[1;97mPROCESS NAME" << std::string(27, ' ') << "RAM USAGE" << std::string(9, ' ') << "PID / STATUS\033[0m\n";
                            std::cout << "  --------------------------------------------------------------------------------\n";

                            for (size_t i = 0; i < highProcs.size(); ++i) {
                                const auto& p = highProcs[i];
                                std::string nm = "  " + p.processName;
                                if (nm.length() < 45) nm += std::string(45 - nm.length(), ' ');
                                nm += " | " + Cleaner::FormatSize(p.memoryUsageBytes);
                                if (nm.length() < 65) nm += std::string(65 - nm.length(), ' ');
                                nm += " | PID:" + std::to_string(p.pid);
                                if (p.isProtected) nm += "  \033[1;32m[PROTECTED]\033[0m";
                                else nm += "  \033[1;31m[CAN KILL]\033[0m";
                                if (i == selected) {
                                    std::cout << "\033[7;1;36m > " << nm << " \033[0m\n";
                                } else {
                                    std::cout << "   " << nm << "\n";
                                }
                            }
                            std::cout << "\n   Cancel / Return to RAM Cleaner\n";
                            std::cout << "\n\033[90mTotal processes > 200 MB: " << highProcs.size() << "\033[0m\n";
                            std::cout.flush();

                            // Use cross-platform ReadKey for arrow key input
                            OpenTUI::KeyEvent keyEv = OpenTUI::TerminalEngine::ReadKey();
                            if (keyEv.key == OpenTUI::Key::Up) {
                                if (selected > 0) selected--;
                            } else if (keyEv.key == OpenTUI::Key::Down) {
                                if (selected + 1 < highProcs.size()) selected++;
                            } else if (keyEv.key == OpenTUI::Key::Escape) { // ESC
                                procListDone = true;
                            } else if (ch == 13) { // Enter - kill with confirm
                                if (selected < highProcs.size()) {
                                    const auto& target = highProcs[selected];
                                    if (target.isProtected) {
                                        std::cout << "\n\033[1;31m[BLOCKED] \"" << target.processName << "\" is a PROTECTED process and cannot be terminated.\033[0m\n";
                                        std::this_thread::sleep_for(std::chrono::seconds(2));
                                    } else {
                                        std::string confirm = OpenTUI::TextInput::ReadLine("\nConfirm terminate PID " + std::to_string(target.pid) + " (" + target.processName + ")? [y/N]: ", "n");
                                        if (confirm == "y" || confirm == "Y") {
                                            bool ok = GTLIBC::GTLibc::KillProcess(target.pid);
                                            if (ok) {
                                                std::cout << "\033[1;32m[OK] Terminated PID " << target.pid << " (\"" << target.processName << "\").\033[0m\n";
                                                Logger::Instance().Info("Manual TUI kill: PID " + std::to_string(target.pid) + " (" + target.processName + ")");
                                            } else {
                                                std::cout << "\033[1;31m[FAIL] Could not terminate PID " << target.pid << " (Access Denied or already exited).\033[0m\n";
                                            }
                                            std::this_thread::sleep_for(std::chrono::seconds(2));
                                        }
                                    }
                                }
                            } else if (ch == 'k' || ch == 'K') { // Hot-key INSTANT KILL
                                if (selected < highProcs.size()) {
                                    const auto& target = highProcs[selected];
                                    if (target.isProtected) {
                                        std::cout << "\n\033[1;31m[BLOCKED] \"" << target.processName << "\" is a PROTECTED process and cannot be terminated.\033[0m\n";
                                        std::this_thread::sleep_for(std::chrono::seconds(2));
                                    } else {
                                        bool ok = GTLIBC::GTLibc::KillProcess(target.pid);
                                        if (ok) {
                                            std::cout << "\n\033[1;32m[OK] Hot-Key killed PID " << target.pid << " (\"" << target.processName << "\", RAM: " << Cleaner::FormatSize(target.memoryUsageBytes) << " freed).\033[0m\n";
                                            Logger::Instance().Info("Hot-key kill: PID " + std::to_string(target.pid) + " (" + target.processName + ")");
                                        } else {
                                            std::cout << "\n\033[1;31m[FAIL] Could not terminate PID " << target.pid << " (Access Denied or already exited).\033[0m\n";
                                        }
                                        std::this_thread::sleep_for(std::chrono::seconds(1));
                                    }
                                    // Refresh the list since a process was killed
                                    highProcs = ProcessManager::GetHighMemoryCandidates(200ULL * 1024 * 1024);
                                    std::sort(highProcs.begin(), highProcs.end(), [](const GTLIBC::ProcessInfo& a, const GTLIBC::ProcessInfo& b) {
                                        return a.memoryUsageBytes > b.memoryUsageBytes;
                                    });
                                    if (selected >= highProcs.size() && !highProcs.empty()) selected = highProcs.size() - 1;
                                    if (highProcs.empty()) procListDone = true;
                                }
                            } else if (ch == 'q' || ch == 'Q') { // Q - quit
                                procListDone = true;
                            }
                        }
                    }
                }
            } else if (ramChoice == 5) {
                OpenTUI::TerminalEngine::ClearScreen();
                std::string procName = OpenTUI::TextInput::ReadLine("Enter Process Name to Add to Protection Whitelist (e.g. myapp.exe): ", "");
                if (!procName.empty()) {
                    GTLIBC::GTLibc::AddCustomProtectedProcess(procName);
                    g_tuiSettings.customProtectedProcesses.push_back(procName);
                    SaveTUISettings();
                    std::cout << "\033[1;32mProcess '" << procName << "' added to protection whitelist & saved to cleaner_config.json!\033[0m\n";
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            } else if (ramChoice == 6) {
                size_t released = ProcessManager::StopLockingProcesses(true);
                std::cout << "\033[1;32mReleased " << released << " process lock handle(s).\033[0m\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
    }

    static void BuildVisibleNodes(std::shared_ptr<DeepScanNode> node, int depth, std::vector<std::pair<std::shared_ptr<DeepScanNode>, int>>& visible) {
        if (!node) return;
        visible.push_back({node, depth});
        if (node->expanded) {
            for (auto& child : node->children) {
                BuildVisibleNodes(child, depth + 1, visible);
            }
        }
    }

    static void ShowDeepScanSubmenu(Cleaner& cleaner) {
        std::vector<std::string> subOptions = {
            "Interactive Directory Tree Explorer",
            "Multi-Drive Parallel Scan (duf-style visualizer)",
            "Deep Memory Scan & Process Timeline",
            "Top-20 Largest Bloat Items",
            "Export Full Deep Scan to JSON Report",
            "Back"
        };

        OpenTUI::Menu deepMenu("DEEP DISK & MEMORY SCAN ENGINE (dust-architecture)", subOptions, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

        while (true) {
            int sel = deepMenu.Show();
            if (sel == -1 || sel == 5) break;

            OpenTUI::TerminalEngine::ClearScreen();
            std::cout << "--------------------------------------------------------------------------------\n";

            if (sel == 0) { // Interactive Directory Tree Explorer
                std::string scanPath = OpenTUI::TextInput::ReadLine("Enter directory path to deep scan: ", ".");
                if (scanPath.empty()) continue;

                std::atomic<bool> scanDone{false};
                std::string statusMsg = "Scanning directory tree...";
                std::shared_ptr<DeepScanNode> rootNode = nullptr;

                OpenTUI::SpinnerAnimation spinner;
                std::thread scanThread([&scanPath, &rootNode, &scanDone, &statusMsg]() {
                    DeepScanFilter filter;
                    rootNode = DeepScanner::ScanDirectory(scanPath, filter, [&statusMsg](const std::string& msg) {
                        statusMsg = msg;
                    });
                    scanDone = true;
                });

                while (!scanDone) {
                    OpenTUI::TerminalEngine::ClearScreen();
                    std::cout << "\033[1;36m" << spinner.GetNextFrame() << " Deep Scanning '" << scanPath << "'... " << statusMsg << "\033[0m\n";
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                if (scanThread.joinable()) scanThread.join();

                if (!rootNode) {
                    std::cout << "\033[1;31m[ERROR] Failed to scan path: " << scanPath << "\033[0m\n";
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    continue;
                }

                rootNode->expanded = true;
                size_t cursor = 0;

                while (true) {
                    std::vector<std::pair<std::shared_ptr<DeepScanNode>, int>> visible;
                    BuildVisibleNodes(rootNode, 0, visible);

                    if (visible.empty()) break;
                    if (cursor >= visible.size()) cursor = visible.size() - 1;

                    OpenTUI::TerminalEngine::ClearScreen();
                    std::cout << "\033[1;33m+--[ Deep Scan Interactive Tree Explorer ]---------------------------------------+\033[0m\n";
                    std::cout << "\033[90m| Controls: UP/DOWN=Navigate | RIGHT/ENTER=Expand | LEFT=Collapse | D=Clean | E=Export | Q=Exit |\033[0m\n";
                    std::cout << "\033[1;33m+--------------------------------------------------------------------------------+\033[0m\n\n";

                    uintmax_t totalSize = rootNode->sizeBytes;
                    size_t displayCount = std::min<size_t>(visible.size(), 20);
                    size_t startIndex = 0;
                    if (cursor >= 10) startIndex = cursor - 10;
                    if (startIndex + displayCount > visible.size()) startIndex = visible.size() > displayCount ? visible.size() - displayCount : 0;

                    for (size_t i = startIndex; i < startIndex + displayCount && i < visible.size(); ++i) {
                        auto n = visible[i].first;
                        int depth = visible[i].second;

                        std::string indent = "";
                        for (int d = 0; d < depth; ++d) indent += "  ";

                        std::string icon = n->isDirectory ? (n->expanded ? "📂 " : "📁 ") : "📄 ";
                        std::string bar = DeepScanner::RenderVisualSizeBar(n->sizeBytes, totalSize, 10);

                        std::string line = indent + icon + n->name;
                        if (line.size() > 40) line = line.substr(0, 37) + "...";
                        
                        std::stringstream rowSs;
                        rowSs << std::left << std::setw(42) << line << "  " << bar;

                        if (i == cursor) {
                            std::cout << "\033[1;36m > " << OpenTUI::Color::Reverse << rowSs.str() << OpenTUI::Color::Reset << "\033[0m\n";
                        } else {
                            std::cout << "   " << rowSs.str() << "\n";
                        }
                    }

                    std::cout << "\n\033[90mNode: " << visible[cursor].first->path.string() << " (" << Cleaner::FormatSize(visible[cursor].first->sizeBytes) << ")\033[0m\n";

                    OpenTUI::KeyEvent ev = OpenTUI::TerminalEngine::ReadKey();
                    if (ev.key == OpenTUI::Key::Up) {
                        if (cursor > 0) cursor--;
                    } else if (ev.key == OpenTUI::Key::Down) {
                        if (cursor + 1 < visible.size()) cursor++;
                    } else if (ev.key == OpenTUI::Key::Right || ev.key == OpenTUI::Key::Enter) {
                        if (visible[cursor].first->isDirectory) visible[cursor].first->expanded = true;
                    } else if (ev.key == OpenTUI::Key::Left) {
                        if (visible[cursor].first->isDirectory && visible[cursor].first->expanded) {
                            visible[cursor].first->expanded = false;
                        } else if (visible[cursor].first->parent) {
                            for (size_t idx = 0; idx < visible.size(); ++idx) {
                                if (visible[idx].first.get() == visible[cursor].first->parent) {
                                    cursor = idx;
                                    break;
                                }
                            }
                        }
                    } else if (ev.key == OpenTUI::Key::Char) {
                        char c = std::tolower(ev.ch);
                        if (c == 'q') break;
                        if (c == 'e') {
                            DeepScanner::ExportToJson(scanPath, rootNode, "deep_scan_report.json");
                            std::cout << "\033[1;32m[OK] Exported JSON report to deep_scan_report.json\033[0m\n";
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                        } else if (c == 'd') {
                            auto targetPath = visible[cursor].first->path;
                            std::cout << "\033[1;33mSchedule cleaning for target directory: " << targetPath.string() << "? (y/N): \033[0m";
                            char confirm = 0;
                            std::cin >> confirm;
                            if (confirm == 'y' || confirm == 'Y') {
                                cleaner.SetCustomPaths({targetPath});
                                cleaner.Clean();
                                std::cout << "\033[1;32m[OK] Cleanup executed on target path!\033[0m\n";
                                std::this_thread::sleep_for(std::chrono::seconds(2));
                            }
                        }
                    }
                }
            } else if (sel == 1) { // Multi-Drive Parallel Scan
                std::cout << "\033[1;36mScanning all mounted drives simultaneously in parallel threads...\033[0m\n\n";
                auto driveResults = SmartScheduler::ScanAllDrivesSimultaneously(90.0);

                std::cout << "\033[1;33m+--[ Multi-Drive Deep Storage Matrix ]-------------------------------------------+\033[0m\n";
                for (const auto& dr : driveResults) {
                    std::string badge = dr.isWarning ? "\033[1;31m [WARNING: LOW DISK SPACE > 90%]\033[0m" : "\033[1;32m [OK]\033[0m";
                    std::cout << "  Drive " << std::left << std::setw(8) << dr.driveName
                              << " " << dr.usageBar
                              << "  (Free: " << Cleaner::FormatSize(dr.freeBytes) << " / " << Cleaner::FormatSize(dr.totalBytes) << ")"
                              << badge << "\n";
                }
                std::cout << "\033[1;33m+--------------------------------------------------------------------------------+\033[0m\n\n";
                std::cout << "\033[90m(Press Enter to return...)\033[0m";
                std::cin.get();
            } else if (sel == 2) { // Deep Memory Scan & Process Timeline
                GTLIBC::GTLibc::RecordMemorySnapshot();
                auto topProcs = GTLIBC::GTLibc::GetTopMemoryConsumers(15);
                auto leakCandidates = GTLIBC::GTLibc::DetectMemoryLeakCandidates(10.0);

                std::cout << "\033[1;36m=== Deep Process Memory Breakdown & RAM Timeline ===\033[0m\n\n";
                if (!leakCandidates.empty()) {
                    std::cout << "\033[1;31m[MEMORY LEAK WARNING] Detected process(es) growing >10% RAM across snapshots:\033[0m\n";
                    for (DWORD leakPid : leakCandidates) {
                        std::cout << "  - PID " << leakPid << "\n";
                    }
                    std::cout << "\n";
                }

                std::cout << std::left << std::setw(8) << "PID"
                          << std::setw(25) << "PROCESS"
                          << std::setw(14) << "WORKING SET"
                          << std::setw(14) << "HEAP/PRIV"
                          << std::setw(14) << "MAPPED"
                          << std::setw(14) << "SHARED" << "\n";
                std::cout << std::string(89, '-') << "\n";

                for (const auto& proc : topProcs) {
                    std::cout << std::left << std::setw(8) << proc.pid
                              << std::setw(25) << proc.processName
                              << std::setw(14) << Cleaner::FormatSize(proc.workingSetBytes)
                              << std::setw(14) << Cleaner::FormatSize(proc.privateHeapBytes)
                              << std::setw(14) << Cleaner::FormatSize(proc.mappedFilesBytes)
                              << std::setw(14) << Cleaner::FormatSize(proc.sharedMemoryBytes) << "\n";
                }
                std::cout << "\n\033[90m(Press Enter to return...)\033[0m";
                std::cin.get();
            } else if (sel == 3) { // Top-20 Largest Bloat Items
                std::string p = OpenTUI::TextInput::ReadLine("Enter search path for top bloat items: ", ".");
                if (p.empty()) continue;

                DeepScanFilter filter;
                filter.topN = 20;
                auto rootNode = DeepScanner::ScanDirectory(p, filter);
                auto topNodes = DeepScanner::GetTopN(rootNode, 20);

                std::cout << "\033[1;36m=== Top " << topNodes.size() << " Largest Disk Bloat Items in '" << p << "' ===\033[0m\n\n";
                uintmax_t rootTotal = rootNode ? rootNode->sizeBytes : 0;

                for (size_t i = 0; i < topNodes.size(); ++i) {
                    std::string bar = DeepScanner::RenderVisualSizeBar(topNodes[i]->sizeBytes, rootTotal, 12);
                    std::cout << "  #" << std::setw(2) << (i + 1) << "  "
                              << std::left << std::setw(45) << (topNodes[i]->name + (topNodes[i]->isDirectory ? "/" : ""))
                              << " " << bar << "\n";
                }
                std::cout << "\n\033[90m(Press Enter to return...)\033[0m";
                std::cin.get();
            } else if (sel == 4) { // Export Full Deep Scan to JSON Report
                std::string p = OpenTUI::TextInput::ReadLine("Enter root directory to scan & export: ", ".");
                std::string outFile = OpenTUI::TextInput::ReadLine("Enter output JSON filename: ", "deep_scan_report.json");

                auto rootNode = DeepScanner::ScanDirectory(p);
                DeepScanner::ExportToJson(p, rootNode, outFile);
                std::cout << "\033[1;32m[OK] Deep scan JSON report exported to: " << outFile << "\033[0m\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
    }

    static void RunInteractiveMenu(Cleaner& cleaner) {
        Logger::Instance().SetTUIActive(true);
        LoadTUISettings();

        std::vector<std::string> options = {
            "Disk Cleaner (with Deep Scan)",
            "RAM Cleaner",
            "AQL Query",
            "Daemon Monitor",
            "Task Library",
            "Settings",
            "Exit"
        };

        OpenTUI::Menu menu("SYSTEM-CLEANER-AGENT", options, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
        menu.SetRefreshIntervalMs(g_tuiSettings.monitorIntervalSec * 1000);
        bool firstRender = true;

        while (true) {
            // Note: We do NOT call FlushInputBuffer() here because it would
            // eat the user's keypresses between menu renders. The startup
            // flush in Menu::ShowExtended() handles stray chars on first show.

            menu.SetTheme(g_tuiSettings.tuiThemeEngine);
            menu.SetColorScheme(g_tuiSettings.tuiColorScheme);
            menu.SetFgColor(g_tuiSettings.tuiFgColor);
            menu.SetBgColor(g_tuiSettings.tuiBgColor);
            if (firstRender) {
                firstRender = false;
            } else {
                OpenTUI::TerminalEngine::ClearScreen();
            }
            // Update refresh interval in case user changed the setting via Settings menu
            menu.SetRefreshIntervalMs(g_tuiSettings.monitorIntervalSec * 1000);
            // Set initial header lines (will be auto-refreshed on every frame)
            {
                auto headers = GetLiveResourceHeaders();
                AppendTaskStatusToHeaders(headers);
                menu.SetHeaderLines(headers);
            }
            // CRITICAL: Set callback to re-query live data on EVERY auto-refresh tick.
            // Without this, the auto-refresh re-renders the SAME cached data.
            menu.SetHeaderRefreshCallback([]() -> std::vector<std::string> {
                auto fresh = GetLiveResourceHeaders();
                AppendTaskStatusToHeaders(fresh);
                return fresh;
            });
            menu.SetStatusLine(g_tuiStatus.GetStatusLine());
            int selected = menu.Show();

            if (selected == -1 || selected == 6) {
                std::cout << "\n\033[32mExiting system-cleaner-agent. Goodbye!\033[0m\n";
                break;
            }

            OpenTUI::TerminalEngine::ClearScreen();
            std::cout << "--------------------------------------------------------------------------------\n";

            switch (selected) {
                case 0: {
                    ShowDiskCleanerSubmenu(cleaner);
                    break;
                }
                case 1: {
                    ShowRamCleanerSubmenu(cleaner);
                    break;
                }
                case 2: {
                    std::string selectedQuery = SelectAgentQuery();
                    if (selectedQuery.empty()) {
                        std::cout << "\033[1;33m[AQL CANCELLED] Operation cancelled by user.\033[0m\n";
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        break;
                    }
                    std::string sqUpper = selectedQuery;
                    std::transform(sqUpper.begin(), sqUpper.end(), sqUpper.begin(), ::toupper);
                    if (sqUpper == "HELP" || sqUpper == "?" || sqUpper == "H") break;

                    static const std::vector<std::string> aqlVerbs = {
                        "KILL", "SELECT", "CLEAN", "SCAN", "SHRED", "PURGE", "MONITOR", "WIPE", "EMPTY"
                    };
                    bool isAQL = false;
                    for (const auto& verb : aqlVerbs) {
                        if (sqUpper.find(verb) == 0 || sqUpper.find(" " + verb + " ") != std::string::npos || sqUpper.find(verb + " ") == 0) {
                            isAQL = true;
                            break;
                        }
                    }

                    if (isAQL) {
                        AQLQuery parsed = AQLEngine::Parse(selectedQuery);
                        AQLEngine::ValidationResult val = AQLEngine::Validate(selectedQuery);
                        if (!val.isValid) {
                            std::cout << "\033[1;31m[AQL SYNTAX ERROR] " << val.errorMessage << "\033[0m\n";
                            std::cout << "\033[1;33m[STRICT AQL GRAMMAR RULES & EXAMPLES]\n" << val.suggestedHint << "\033[0m\n\n";
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            break;
                        }
                        bool currentDryRun = g_tuiSettings.dryRun || g_tuiSettings.sandboxMode;
                        AQLEngine::Execute(parsed, cleaner, currentDryRun);
                    } else {
                        std::cout << "\033[1;33mLaunching background Agent Task: " << selectedQuery << "\033[0m\n";
                        {
                            uint64_t tid = TaskHistory::Instance().Register("AGENT", "Agent: " + selectedQuery, selectedQuery, "OpenTUI Menu");
                            bool currentDryRun = g_tuiSettings.dryRun || g_tuiSettings.sandboxMode;
                            std::thread worker([selectedQuery, currentDryRun, tid]() {
                                TaskHistory::Instance().MarkRunning(tid);
                                TaskHistory::Instance().UpdateProgress(tid, "ReAct agent reasoning...");
                                g_tuiStatus.SetActive("Agent Task: " + selectedQuery);
                                AgentEngine agent(selectedQuery);
                                agent.RunReActLoop(currentDryRun);
                                g_tuiStatus.SetCompleted("Agent Task finished: " + selectedQuery);
                                TaskHistory::Instance().MarkCompleted(tid, "Agent loop finished for: " + selectedQuery);
                            });
                            worker.detach();
                        }
                    }
                    break;
                }
                case 3: {
                    std::cout << "\033[1;36mLaunching background Smart Daemon Service...\033[0m\n";
                    {
                        uint64_t tid = TaskHistory::Instance().Register("DAEMON", "Smart Daemon Monitor", "daemon --mem-threshold 80% --disk-threshold", "OpenTUI Menu");
                        bool currentDryRun = g_tuiSettings.dryRun || g_tuiSettings.sandboxMode;
                        int interval = g_tuiSettings.monitorIntervalSec;
                        std::thread worker([&cleaner, currentDryRun, interval, tid]() {
                            TaskHistory::Instance().MarkRunning(tid);
                            TaskHistory::Instance().UpdateProgress(tid, "Daemon watching RAM > 80% / Free Disk < 500MB...");
                            g_tuiStatus.SetActive("Smart Daemon (RAM > 80% / Free Disk < 500MB)");
                            SmartScheduler::RunDaemonService(cleaner, {}, 80.0, 0.0, interval, currentDryRun, 500ULL * 1024 * 1024, "C:\\");
                            TaskHistory::Instance().MarkCompleted(tid, "Daemon service stopped.");
                        });
                        worker.detach();
                    }
                    break;
                }
                case 4: {
                    ShowTaskLibrary();
                    break;
                }
                case 5: {
                    ShowSettingsMenu(cleaner);
                    break;
                }
            }
        }
    }
};

