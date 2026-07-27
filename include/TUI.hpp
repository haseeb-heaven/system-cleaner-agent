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
    static void PrintBanner(const std::string& themeOverride = "") {
        std::string theme = themeOverride.empty() ? g_tuiSettings.tuiThemeEngine : themeOverride;
        auto style = OpenTUI::GetThemeStyle(theme);
        std::cout << style.primaryColor
                  << "    _/_\\_      ____  _  _  ____  ____  ____  _  _   \n"
                  << "   /     \\    / ___)( \\/ )( ___)(_  _)(  __)( \\/ )  \n"
                  << "  |   *   |   \\___ \\ )  /  )__)   )(   ) _) / \\/ \\  \n"
                  << "   \\     /    (____/(__/  (____) (__) (____)\\_/\\_/  \n"
                  << "    \\___/     \033[1;33mSYSTEM-CLEANER-AGENT \033[1;32mv5.5.0\033[0m\n"
                  << style.secondaryColor << "  " << style.bannerSubtitle << "\033[0m\n";
    }

    static void FlushInputBuffer() {
        std::cin.clear();
#ifdef _WIN32
        while (_kbhit()) { (void)_getch(); }
#endif
    }

    static std::vector<std::string> GetLiveResourceHeaders() {
        std::vector<std::string> headers;
        double memPercent = SmartScheduler::GetMemoryUsagePercent();
        std::string ramBar = OpenTUI::ProgressBar::Render(memPercent, 16, "% used");
        headers.push_back("RAM:  " + ramBar);

        auto driveStats = SmartScheduler::GetAllDriveStats();
        for (const auto& ds : driveStats) {
            std::string diskBar = OpenTUI::ProgressBar::Render(ds.usedPercent, 14, "% used");
            std::ostringstream ss;
            ss << ds.driveName << " " << diskBar << " (Free: " << Cleaner::FormatSize(ds.freeBytes) << " / " << Cleaner::FormatSize(ds.capacityBytes) << ")";
            headers.push_back(ss.str());
        }

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

        return headers;
    }

    static void ShowSystemResourceMonitor() {
        while (true) {
            auto style = OpenTUI::GetThemeStyle(g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            OpenTUI::TerminalEngine::ClearScreen(style.panelBg);
            PrintBanner();
            std::ostringstream ss;
            ss << OpenTUI::Box::DrawBorder(80, "SYSTEM RESOURCE & DRIVE MONITOR", g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            double memPercent = SmartScheduler::GetMemoryUsagePercent();
            std::string ramBar = OpenTUI::ProgressBar::Render(memPercent, 35, "% used");
            ss << OpenTUI::Box::DrawLine(80, "System Memory (RAM): " + ramBar, false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            ss << OpenTUI::Box::DrawDivider(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            auto driveStats = SmartScheduler::GetAllDriveStats();
            for (const auto& ds : driveStats) {
                double freePercent = 100.0 - ds.usedPercent;
                std::string diskBar = OpenTUI::ProgressBar::Render(ds.usedPercent, 20, "% used");
                std::ostringstream dss;
                dss << "Drive " << ds.driveName << " " << diskBar << " (Free: " << Cleaner::FormatSize(ds.freeBytes) << " / " << Cleaner::FormatSize(ds.capacityBytes) << ")";
                ss << OpenTUI::Box::DrawLine(80, dss.str(), false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            }

            ss << OpenTUI::Box::DrawDivider(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            std::string hint = "Refreshing every " + std::to_string(g_tuiSettings.monitorIntervalSec) + "s... Press ESC or 'q' to return to dashboard.";
            ss << OpenTUI::Box::DrawLine(80, hint, false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            ss << OpenTUI::Box::DrawFooter(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            OpenTUI::TerminalEngine::MoveCursorToHome();
            std::cout << style.panelBg << ss.str() << style.panelBg << "\033[J" << std::flush;

            // Sleep in 100ms intervals to allow ESC/q responsiveness
            int checkCycles = g_tuiSettings.monitorIntervalSec * 10;
            bool returnToMenu = false;
            for (int i = 0; i < checkCycles; ++i) {
#ifdef _WIN32
                if (_kbhit()) {
                    int c = _getch();
                    if (c == 27 || c == 'q' || c == 'Q') {
                        returnToMenu = true;
                        break;
                    }
                }
#endif
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            if (returnToMenu) break;
        }
    }


    static void ShowTaskActionDialog(uint64_t tid) {
        TaskEntry t;
        if (!TaskHistory::Instance().GetTask(tid, t)) {
            std::cout << "\033[1;31m[ERROR] Task ID #" << tid << " not found in Task Library.\033[0m\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            return;
        }

        while (true) {
            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();
            bool isLive = (t.status == TaskStatus::Running || t.status == TaskStatus::Queued);
            std::string timeStr = TaskHistoryNS::FormatTimestamp(isLive ? t.startedAt : t.finishedAt);
            std::string durStr  = TaskHistoryNS::FormatDuration(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t.startedAt));

            std::cout << "\033[1;36m╔══════════════════════════════════════════════════════════════════════════════════════════╗\033[0m\n";
            std::cout << "\033[1;36m║\033[1;97m   ◈  TASK ACTION CONTROL  ─  Task #" << std::left << std::setw(55) << std::to_string(t.id) << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m╠══════════════════════════════════════════════════════════════════════════════════════════╣\033[0m\n";
            std::cout << "\033[1;36m║\033[0m  ID:          #" << std::setw(6) << t.id << "                                                                 \033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[0m  Name:        " << std::left << std::setw(70) << t.name << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[0m  Category:    " << std::left << std::setw(70) << t.category << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[0m  Command:     " << std::left << std::setw(70) << t.command << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[0m  Source:      " << std::left << std::setw(70) << t.source << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[0m  Status:      " << TaskHistoryNS::StatusColor(t.status) << std::left << std::setw(60) << TaskHistoryNS::StatusLabel(t.status) << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[0m  Progress:    " << std::left << std::setw(70) << (t.progressMsg.empty() ? t.resultSummary : t.progressMsg) << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[0m  Runtime:     " << std::left << std::setw(70) << (timeStr + " (" + durStr + ")") << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[0m  Bytes Freed: " << std::left << std::setw(70) << Cleaner::FormatSize(t.bytesFreed) << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m╚══════════════════════════════════════════════════════════════════════════════════════════╝\033[0m\n\n";

            std::vector<std::string> actionOpts = {
                "Pause / Stop Task",
                "Resume Task",
                "Kill / Terminate Task",
                "Return to Task Library"
            };
            OpenTUI::Menu actMenu("TASK ACTIONS (#" + std::to_string(t.id) + ")", actionOpts);
            int aSel = actMenu.Show();
            if (aSel == -1 || aSel == 3) break;

            if (aSel == 0) {
                if (TaskHistory::Instance().PauseTask(t.id)) {
                    std::cout << "\033[1;35m[OK] Task #" << t.id << " paused.\033[0m\n";
                } else {
                    std::cout << "\033[1;31m[WARN] Could not pause Task #" << t.id << " (Task not in running state).\033[0m\n";
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } else if (aSel == 1) {
                if (TaskHistory::Instance().ResumeTask(t.id)) {
                    std::cout << "\033[1;32m[OK] Task #" << t.id << " resumed.\033[0m\n";
                } else {
                    std::cout << "\033[1;31m[WARN] Could not resume Task #" << t.id << " (Task not in paused state).\033[0m\n";
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
            TaskHistory::Instance().GetTask(t.id, t);
        }
    }

    static void ShowTaskLibrary() {
        int selectedRow = 0;

        while (true) {
            auto style = OpenTUI::GetThemeStyle(g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            OpenTUI::TerminalEngine::ClearScreen(style.panelBg);
            PrintBanner();

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

            std::ostringstream ss;
            ss << OpenTUI::Box::DrawBorder(80, "INTERACTIVE TASK MANAGER", g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            std::string hotkeyBar = "[HOTKEYS] K: Kill │ P: Pause │ R: Resume │ D: Details │ C: Clear Finished │ ESC: Exit";
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
            std::string footerHint = "Use UP/DOWN to select task · Press K/P/R/D/C hotkeys · ESC to return";
            ss << OpenTUI::Box::DrawLine(80, footerHint, false, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
            ss << OpenTUI::Box::DrawFooter(80, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);

            OpenTUI::TerminalEngine::MoveCursorToHome();
            std::cout << style.panelBg << ss.str() << style.panelBg << "\033[J" << std::flush;

            OpenTUI::KeyEvent ev = OpenTUI::TerminalEngine::ReadKey();
            if (ev.key == OpenTUI::Key::Up) {
                if (!tasks.empty()) selectedRow = (selectedRow > 0) ? selectedRow - 1 : static_cast<int>(tasks.size()) - 1;
            } else if (ev.key == OpenTUI::Key::Down) {
                if (!tasks.empty()) selectedRow = (selectedRow + 1) % static_cast<int>(tasks.size());
            } else if (ev.key == OpenTUI::Key::Escape || (ev.key == OpenTUI::Key::Char && (ev.ch == 'q' || ev.ch == 'Q'))) {
                break;
            } else if (ev.key == OpenTUI::Key::Char && !tasks.empty() && selectedRow >= 0 && selectedRow < static_cast<int>(tasks.size())) {
                uint64_t targetId = tasks[selectedRow].id;
                char c = static_cast<char>(std::tolower(ev.ch));
                if (c == 'k') {
                    TaskHistory::Instance().KillTask(targetId);
                } else if (c == 'p') {
                    TaskHistory::Instance().PauseTask(targetId);
                } else if (c == 'r') {
                    TaskHistory::Instance().ResumeTask(targetId);
                } else if (c == 'd') {
                    ShowTaskActionDialog(targetId);
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
                std::string("Monitor Interval:  [") + std::to_string(g_tuiSettings.monitorIntervalSec) + " seconds]",
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
                "Default", "Cyan Matrix", "Electric Magenta", "Amber Gold", "Emerald Cyber", "Neon Cyberpunk", "Monochrome Slate"
            };
            static const std::vector<std::string> fgColors = {
                "Default", "Cyan", "Electric Magenta", "Amber Gold", "Emerald Green", "Neon Pink", "Bright White", "Yellow", "Royal Blue"
            };
            static const std::vector<std::string> bgColors = {
                "Default", "Black", "Navy Blue", "Electric Magenta", "Amber Gold", "Emerald Green", "Dark Slate", "Charcoal Gray"
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
                case 4: g_tuiSettings.enableLogging = !g_tuiSettings.enableLogging; break;
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
                    static const std::vector<int> intervals = { 3, 5, 10, 15, 30, 60 };
                    auto it = std::find(intervals.begin(), intervals.end(), g_tuiSettings.monitorIntervalSec);
                    int idx = (it != intervals.end()) ? static_cast<int>(std::distance(intervals.begin(), it)) : 1;
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
                    PrintBanner();
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

        static const std::vector<std::string> suggestions = {
            "KILL PROCESS WHERE RAM > 70%",
            "KILL PROCESS WHERE RAM > 200MB",
            "CLEAN 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE FREE_DISK < 500MB",
            "MONITOR WHERE RAM > 80% EVERY 15S",
            "SHRED 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE SIZE > 10MB",
            "PURGE RECYCLE_BIN",
            "SCAN 'C:\\' WHERE AGE > 24H"
        };

        std::string lastErr = "";
        std::string lastHint = "";

        auto showHelp = [&]() {
            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();
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

    static void RunInteractiveMenu(Cleaner& cleaner) {
        Logger::Instance().SetTUIActive(true);
        LoadTUISettings();

        std::vector<std::string> options = {
            "Storage Scan",
            "Smart Deep Clean",
            "Secure Shred Wipe",
            "Empty Recycle Bin",
            "RAM Cleaner",
            "Agent & AQL Query",
            "Daemon Monitor",
            "System Resource Monitor",
            "Task Library",
            "Settings",
            "Exit Agent"
        };

        OpenTUI::Menu menu("SYSTEM-CLEANER-AGENT", options, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme, g_tuiSettings.tuiFgColor, g_tuiSettings.tuiBgColor);
        menu.SetPreRenderCallback([]() { PrintBanner(); });
        bool firstRender = true;

        while (true) {
            menu.SetTheme(g_tuiSettings.tuiThemeEngine);
            menu.SetColorScheme(g_tuiSettings.tuiColorScheme);
            menu.SetFgColor(g_tuiSettings.tuiFgColor);
            menu.SetBgColor(g_tuiSettings.tuiBgColor);
            if (firstRender) {
                firstRender = false;
            } else {
                // On re-entry clear once so banner from callback is fresh
                OpenTUI::TerminalEngine::ClearScreen();
            }
            menu.SetHeaderLines(GetLiveResourceHeaders());
            menu.SetStatusLine(g_tuiStatus.GetStatusLine());
            int selected = menu.Show();

            if (selected == -1 || selected == 10) {
                std::cout << "\n\033[32mExiting system-cleaner-agent OpenTUI Suite. Goodbye!\033[0m\n";
                break;
            }

            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();
            std::cout << "--------------------------------------------------------------------------------\n";

            switch (selected) {
                case 0: {
                    std::cout << "\033[1;36mLaunching background Storage Scan...\033[0m\n";
                    {
                        uint64_t tid = TaskHistory::Instance().Register("TUI", "Storage Scan", "scan --dry-run", "OpenTUI Menu");
                        std::thread worker([&cleaner, tid]() {
                            TaskHistory::Instance().MarkRunning(tid);
                            TaskHistory::Instance().UpdateProgress(tid, "Scanning all targets...");
                            g_tuiStatus.SetActive("Storage Scan");
                            cleaner.SetDryRun(true);
                            auto reports = cleaner.Scan();
                            uintmax_t freed = 0;
                            for (const auto& r : reports) freed += r.sizeBytes;
                            g_tuiStatus.SetCompleted("Scan completed. Cleanable: " + Cleaner::FormatSize(freed));
                            TaskHistory::Instance().MarkCompleted(tid, "Scan found " + Cleaner::FormatSize(freed) + " cleanable across " + std::to_string(reports.size()) + " targets", freed, 0, 0, 0);
                        });
                        worker.detach();
                    }
                    break;
                }
                case 1: {
                    std::cout << "\033[1;36mLaunching background Smart Deep Clean...\033[0m\n";
                    {
                        uint64_t tid = TaskHistory::Instance().Register("TUI", "Smart Deep Clean", "clean --mode deep", "OpenTUI Menu");
                        std::thread worker([&cleaner, tid]() {
                            TaskHistory::Instance().MarkRunning(tid);
                            TaskHistory::Instance().UpdateProgress(tid, "Deep cleaning all targets...");
                            g_tuiStatus.SetActive("Smart Deep Clean");
                            cleaner.SetSandbox(g_tuiSettings.sandboxMode);
                            cleaner.SetDryRun(g_tuiSettings.dryRun);
                            cleaner.Clean();
                            g_tuiStatus.SetCompleted("Deep Clean finished.");
                            TaskHistory::Instance().MarkCompleted(tid, "Deep Clean completed.");
                        });
                        worker.detach();
                    }
                    break;
                }
                case 2: {
                    std::cout << "\033[1;36mLaunching background Secure Shred Wipe...\033[0m\n";
                    {
                        uint64_t tid = TaskHistory::Instance().Register("TUI", "Secure Shred Wipe", "clean --mode shred", "OpenTUI Menu");
                        std::thread worker([&cleaner, tid]() {
                            TaskHistory::Instance().MarkRunning(tid);
                            TaskHistory::Instance().UpdateProgress(tid, "Shredding (zero-overwrite) targets...");
                            g_tuiStatus.SetActive("Secure Shred Wipe");
                            cleaner.SetMode(CleanMode::Shred);
                            cleaner.SetSandbox(g_tuiSettings.sandboxMode);
                            cleaner.SetDryRun(g_tuiSettings.dryRun);
                            cleaner.Clean();
                            g_tuiStatus.SetCompleted("Shred Wipe finished.");
                            TaskHistory::Instance().MarkCompleted(tid, "Secure Shred Wipe completed.");
                        });
                        worker.detach();
                    }
                    break;
                }
                case 3: {
                    std::cout << "\033[1;36mLaunching background Empty Recycle Bin...\033[0m\n";
                    {
                        uint64_t tid = TaskHistory::Instance().Register("TUI", "Empty Recycle Bin", "clean --recycle-bin", "OpenTUI Menu");
                        std::thread worker([&cleaner, tid]() {
                            TaskHistory::Instance().MarkRunning(tid);
                            TaskHistory::Instance().UpdateProgress(tid, "Emptying Recycle Bin...");
                            g_tuiStatus.SetActive("Empty Recycle Bin");
                            cleaner.SetEmptyRecycleBin(true);
                            cleaner.EmptyWindowsRecycleBin();
                            g_tuiStatus.SetCompleted("Recycle Bin emptied.");
                            TaskHistory::Instance().MarkCompleted(tid, "Recycle Bin emptied.");
                        });
                        worker.detach();
                    }
                    break;
                }
                case 4: {
                    while (true) {
                        OpenTUI::TerminalEngine::ClearScreen();
                        PrintBanner();
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
                            "Terminate Process",
                            "Add Process Name to Whitelist",
                            "Release Process Lock Handles",
                            "Return to Dashboard"
                        };

                        OpenTUI::Menu ramMenu("RAM CLEANER PERMISSIONS", ramMenuOptions, g_tuiSettings.tuiThemeEngine, g_tuiSettings.tuiColorScheme);
                        int ramChoice = ramMenu.Show();

                        if (ramChoice == -1 || ramChoice == 3) break;

                        if (ramChoice == 0) {
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

                                OpenTUI::Menu killMenu("SELECT PROCESS TO TERMINATE (> 200 MB RAM)", killOptions);
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
                        } else if (ramChoice == 1) {
                            OpenTUI::TerminalEngine::ClearScreen();
                            PrintBanner();
                            std::string procName = OpenTUI::TextInput::ReadLine("Enter Process Name to Add to Protection Whitelist (e.g. myapp.exe): ", "");
                            if (!procName.empty()) {
                                GTLIBC::GTLibc::AddCustomProtectedProcess(procName);
                                g_tuiSettings.customProtectedProcesses.push_back(procName);
                                SaveTUISettings();
                                std::cout << "\033[1;32mProcess '" << procName << "' added to protection whitelist & saved to cleaner_config.json!\033[0m\n";
                                std::this_thread::sleep_for(std::chrono::seconds(2));
                            }
                        } else if (ramChoice == 2) {
                            size_t released = ProcessManager::StopLockingProcesses(true);
                            std::cout << "\033[1;32mReleased " << released << " process lock handle(s).\033[0m\n";
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                        }
                    }
                    break;
                }
                case 5: {
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
                case 6: {
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
                case 7: {
                    ShowSystemResourceMonitor();
                    break;
                }
                case 9: {
                    ShowSettingsMenu(cleaner);
                    break;
                }
                case 8: {
                    ShowTaskLibrary();
                    break;
                }
            }
        }
    }
};
