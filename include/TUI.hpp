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
    int monitorIntervalSec = 5;     // 5 seconds refresh interval
    size_t ramThresholdMB = 200;    // 200 MB high RAM process cutoff
    std::string customPathsStr = "C:\\Users\\hasee\\AppData\\Local\\Temp";
    std::string tuiThemeEngine = "OpenTUI"; // OpenTUI, TermOx, FTXUI
    std::vector<std::string> customProtectedProcesses;

    void SyncFromAppConfig(const AppConfig& cfg) {
        sandboxMode = cfg.sandboxMode;
        pathProtection = cfg.pathProtection;
        dryRun = cfg.dryRun;
        killLocks = cfg.killLocks;
        monitorIntervalSec = cfg.monitorIntervalSec;
        ramThresholdMB = cfg.ramThresholdMB;
        customPathsStr = cfg.customPathsStr;
        tuiThemeEngine = cfg.tuiThemeEngine;
        customProtectedProcesses = cfg.customProtectedProcesses;
    }

    AppConfig ToAppConfig() const {
        AppConfig cfg;
        cfg.sandboxMode = sandboxMode;
        cfg.pathProtection = pathProtection;
        cfg.dryRun = dryRun;
        cfg.killLocks = killLocks;
        cfg.monitorIntervalSec = monitorIntervalSec;
        cfg.ramThresholdMB = ramThresholdMB;
        cfg.customPathsStr = customPathsStr;
        cfg.tuiThemeEngine = tuiThemeEngine;
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
            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();
            std::cout << "\033[1;36m================================================================================\033[0m\n";
            std::cout << "\033[1;97m                 SYSTEM RESOURCE & DRIVE MONITOR (LIVE MONITOR)                \033[0m\n";
            std::cout << "\033[1;36m================================================================================\033[0m\n\n";

            double memPercent = SmartScheduler::GetMemoryUsagePercent();
            std::cout << "  System Memory (RAM): " << OpenTUI::ProgressBar::Render(memPercent, 35, "% used") << "\n\n";

            std::cout << "  Storage Drives:\n";
            auto driveStats = SmartScheduler::GetAllDriveStats();
            for (const auto& ds : driveStats) {
                double freePercent = 100.0 - ds.usedPercent;
                std::cout << "  - Drive " << ds.driveName << "  " << OpenTUI::ProgressBar::Render(ds.usedPercent, 25, "% used")
                          << "  (Free: " << Cleaner::FormatSize(ds.freeBytes) << " [" << std::fixed << std::setprecision(1) << freePercent << "% free] / Total: " << Cleaner::FormatSize(ds.capacityBytes) << ")\n";
            }

            std::cout << "\n\033[90mRefreshing every " << g_tuiSettings.monitorIntervalSec << "s... Press ESC or 'q' to return to dashboard...\033[0m\n";

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
        std::vector<std::string> options = {
            "Live Task List (auto-refresh)",
            "All Tasks (chronological)",
            "Running Tasks Only",
            "Completed Tasks Only",
            "Failed Tasks Only",
            "Manage Task (Kill / Pause / Resume / Details)",
            "Clear All History",
            "Back to Main Menu"
        };

        OpenTUI::Menu libMenu("TASK LIBRARY / PROCESS HISTORY", options);
        libMenu.SetPreRenderCallback([]() { PrintBanner(); });

        while (true) {
            std::string headerLine = TaskHistory::Instance().HeaderSummary();
            libMenu.SetHeaderLines({headerLine});
            libMenu.SetStatusLine("[TASK LIB] Use UP/DOWN to navigate, ENTER to select, ESC to return");
            int sel = libMenu.Show();
            if (sel < 0 || sel == 7) break;

            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();

            if (sel == 0) {
                ShowTaskListView(true, -1);
            } else if (sel == 1) {
                ShowTaskListView(false, -1);
            } else if (sel == 2) {
                ShowTaskListView(false, (int)TaskStatus::Running);
            } else if (sel == 3) {
                ShowTaskListView(false, (int)TaskStatus::Completed);
            } else if (sel == 4) {
                ShowTaskListView(false, (int)TaskStatus::Failed);
            } else if (sel == 5) {
                std::string tidStr = OpenTUI::TextInput::ReadLine("Enter Task ID to Manage (#): ", "1");
                try {
                    uint64_t tid = std::stoull(tidStr);
                    ShowTaskActionDialog(tid);
                } catch (...) {
                    std::cout << "\033[1;31m[ERROR] Invalid Task ID entered.\033[0m\n";
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            } else if (sel == 6) {
                std::string confirm = OpenTUI::TextInput::ReadLine("Clear all task history? [y/N]: ", "n");
                if (confirm == "y" || confirm == "Y") {
                    TaskHistory::Instance().Clear();
                    std::cout << "\033[1;32m[OK] Task library cleared.\033[0m" << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }

            std::cout << "\n\033[90mPress Enter to return to Task Library menu...\033[0m";
            FlushInputBuffer();
            std::cin.get();
        }
    }

    static void ShowTaskListView(bool liveAutoRefresh, int statusFilter) {
        static int spinIdx = 0;
        static const char* spinChars[] = {"|","/","-","\\","|","/","-","\\","|","/"};

        auto renderOnce = [statusFilter]() {
            std::cout << std::setfill(' ');
            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();

            std::string filterLabel = "ALL TASKS";
            if (statusFilter == (int)TaskStatus::Running)    filterLabel = "RUNNING TASKS";
            else if (statusFilter == (int)TaskStatus::Completed) filterLabel = "COMPLETED TASKS";
            else if (statusFilter == (int)TaskStatus::Failed)    filterLabel = "FAILED TASKS";
            else if (statusFilter == (int)TaskStatus::Cancelled) filterLabel = "CANCELLED TASKS";

            std::cout << "\033[1;36m╔══════════════════════════════════════════════════════════════════════════════════════════╗\033[0m\n";
            std::cout << "\033[1;36m║\033[1;97m   ◈  TASK LIBRARY  ─  " << std::left << std::setw(66) << filterLabel << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m╠══════════════════════════════════════════════════════════════════════════════════════════╣\033[0m\n";

            // Summary stats row
            auto tasks = TaskHistory::Instance().Snapshot();
            int nRunning = 0, nDone = 0, nFailed = 0;
            uintmax_t totalFreed = 0;
            for (auto& t : tasks) {
                if (t.status == TaskStatus::Running || t.status == TaskStatus::Queued) ++nRunning;
                else if (t.status == TaskStatus::Completed) { ++nDone; totalFreed += t.bytesFreed; }
                else if (t.status == TaskStatus::Failed || t.status == TaskStatus::Cancelled) ++nFailed;
            }
            const char* spin = spinChars[spinIdx % 10];
            spinIdx = (spinIdx + 1) % 10;
            std::cout << "\033[1;36m║  \033[0m";
            std::cout << "\033[1;32m● " << nDone << " done\033[0m  ";
            std::cout << "\033[1;33m" << spin << " " << nRunning << " running\033[0m  ";
            std::cout << "\033[1;31m✗ " << nFailed << " failed\033[0m  ";
            std::cout << "\033[90m│  freed: \033[1;36m" << Cleaner::FormatSize(totalFreed) << "\033[0m  ";
            std::cout << "\033[90m│  total: " << tasks.size() << "\033[0m";
            std::cout << "\033[1;36m\033[0m\n";
            std::cout << "\033[1;36m╠══════════════════════════════════════════════════════════════════════════════════════════╣\033[0m\n";

            if (tasks.empty()) {
                std::cout << "\033[1;36m║\033[0m\n";
                std::cout << "\033[1;36m║  \033[1;33m⚠  No tasks recorded yet.\033[0m\n";
                std::cout << "\033[1;36m║  \033[90m   Run any clean / scan / shred / AQL / agent / daemon operation to populate.\033[0m\n";
                std::cout << "\033[1;36m║\033[0m\n";
                std::cout << "\033[1;36m╚══════════════════════════════════════════════════════════════════════════════════════════╝\033[0m\n";
                return;
            }
            std::reverse(tasks.begin(), tasks.end());

            // Header row
            std::cout << "\033[1;37m  ";
            std::cout << std::left << std::setw(5)  << "ID";
            std::cout << std::setw(11) << "STATUS";
            std::cout << std::setw(9)  << "CAT";
            std::cout << std::setw(11) << "TIME";
            std::cout << std::setw(8)  << "DUR";
            std::cout << std::setw(25) << "NAME";
            std::cout << std::setw(30) << "DETAIL / RESULT";
            std::cout << "PROGRESS\033[0m\n";
            std::cout << "\033[90m  " << std::string(95, '-') << "\033[0m\n";

            size_t shown = 0;
            for (auto& t : tasks) {
                if (statusFilter >= 0 && (int)t.status != statusFilter) continue;

                auto now     = std::chrono::system_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - t.startedAt);
                bool isLive  = (t.status == TaskStatus::Running || t.status == TaskStatus::Queued);

                std::string timeStr = TaskHistoryNS::FormatTimestamp(isLive ? t.startedAt : t.finishedAt);
                std::string durStr  = TaskHistoryNS::FormatDuration(elapsed);

                std::string name = t.name;
                if (name.size() > 23) name = name.substr(0, 20) + "...";

                std::string detail = isLive
                    ? (t.progressMsg.empty() ? TaskHistoryNS::StatusLabel(t.status) : t.progressMsg)
                    : (t.resultSummary.empty() ? TaskHistoryNS::StatusLabel(t.status) : t.resultSummary);
                if (detail.size() > 28) detail = detail.substr(0, 25) + "...";

                // Row prefix with live spinner for running tasks
                std::cout << "  ";
                std::cout << "\033[90m[" << std::setfill('0') << std::setw(3) << t.id << std::setfill(' ') << "]\033[0m ";

                // Status badge
                std::string statusBadge;
                if (isLive) {
                    const char* sp = spinChars[(spinIdx + t.id) % 10];
                    statusBadge = std::string("\033[1;33m") + sp + " RUNNING  \033[0m";
                } else {
                    std::string lbl = TaskHistoryNS::StatusLabel(t.status);
                    // Pad label to 10 chars
                    while (lbl.size() < 10) lbl += ' ';
                    statusBadge = std::string(TaskHistoryNS::StatusColor(t.status)) + lbl + "\033[0m";
                }
                std::cout << statusBadge << " ";

                std::cout << "\033[90m" << std::left << std::setw(8) << t.category << "\033[0m ";
                std::cout << "\033[1;33m" << std::setw(10) << timeStr << "\033[0m ";
                std::cout << "\033[90m" << std::setw(7) << durStr << "\033[0m ";
                std::cout << "\033[1;97m" << std::setw(24) << name << "\033[0m ";
                std::cout << "\033[1;36m" << std::setw(29) << detail << "\033[0m";

                // Inline progress bar for running, freed amount for done
                if (isLive && t.percent > 0.0 && t.percent < 100.0) {
                    int barW = 10;
                    int filled = static_cast<int>((t.percent / 100.0) * barW);
                    std::cout << " \033[1;32m";
                    for (int i = 0; i < filled; ++i) std::cout << "█";
                    std::cout << "\033[90m";
                    for (int i = filled; i < barW; ++i) std::cout << "░";
                    std::cout << "\033[0m \033[1;33m" << static_cast<int>(t.percent) << "%\033[0m";
                } else if (t.bytesFreed > 0) {
                    std::cout << " \033[1;32m↓ " << Cleaner::FormatSize(t.bytesFreed) << "\033[0m";
                } else if (t.filesProcessed > 0) {
                    std::cout << " \033[1;36m" << t.filesProcessed << " files\033[0m";
                } else if (t.processesHandled > 0) {
                    std::cout << " \033[1;36m" << t.processesHandled << " procs\033[0m";
                }
                std::cout << std::right << "\n";
                ++shown;
            }

            if (shown == 0) {
                std::cout << "\033[1;36m║  \033[1;33m(no tasks match this filter)\033[0m\n";
            }
            std::cout << "\033[1;36m╚══════════════════════════════════════════════════════════════════════════════════════════╝\033[0m\n";
        };

        if (liveAutoRefresh) {
            int refreshSec = 2;
            for (int tick = 0; tick < 1000; ++tick) {
                renderOnce();
                std::cout << "\033[90mLive refresh every " << refreshSec << "s.  Press ESC or 'q' to return...\033[0m";
                std::cout.flush();
                for (int s = 0; s < refreshSec * 10; ++s) {
#ifdef _WIN32
                    if (_kbhit()) {
                        int c = _getch();
                        if (c == 27 || c == 'q' || c == 'Q') return;
                    }
#endif
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        } else {
            renderOnce();
        }
    }
    static void ShowSettingsMenu(Cleaner& cleaner) {
        while (true) {
#ifdef _WIN32
            static const std::string osName = "Windows";
#elif defined(__APPLE__)
            static const std::string osName = "macOS";
#else
            static const std::string osName = "Linux";
#endif
            std::vector<std::string> settingsOptions = {
                std::string("TUI Engine Theme: [") + g_tuiSettings.tuiThemeEngine + " - OpenTUI / TermOx / FTXUI]",
                std::string("Sandbox Mode:     [") + (g_tuiSettings.sandboxMode ? "ON  - Preview Only" : "OFF - REAL DELETION ALLOWED") + "]",
                std::string("Path Protection:  [") + (g_tuiSettings.pathProtection ? "ON  - System Dir Guard" : "OFF - Disabled") + "]",
                std::string("Dry-Run Mode:     [") + (g_tuiSettings.dryRun ? "ON  - Preview Only" : "OFF - REAL CLEAN") + "]",
                std::string("Kill Locks:       [") + (g_tuiSettings.killLocks ? "ON" : "OFF") + "]",
                std::string("Monitor Interval: [") + std::to_string(g_tuiSettings.monitorIntervalSec) + " seconds]",
                std::string("Target Folders:   [") + g_tuiSettings.customPathsStr.substr(0, 50) + (g_tuiSettings.customPathsStr.size() > 50 ? "..." : "") + "]",
                std::string("Reset to OS Defaults (" + osName + " safe temp/cache paths)"),
                "Save & Return to Dashboard"
            };

            OpenTUI::Menu settingsMenu("SETTINGS & THEMES", settingsOptions);
            OpenTUI::MenuSelection sel = settingsMenu.ShowExtended();

            if (sel.index == -1 || sel.index == 8) {
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

            switch (sel.index) {
                case 0: {
                    if (g_tuiSettings.tuiThemeEngine == "OpenTUI") g_tuiSettings.tuiThemeEngine = "TermOx";
                    else if (g_tuiSettings.tuiThemeEngine == "TermOx") g_tuiSettings.tuiThemeEngine = "FTXUI";
                    else g_tuiSettings.tuiThemeEngine = "OpenTUI";
                    SaveTUISettings();
                    break;
                }
                case 1: g_tuiSettings.sandboxMode = !g_tuiSettings.sandboxMode; break;
                case 2: g_tuiSettings.pathProtection = !g_tuiSettings.pathProtection; break;
                case 3: g_tuiSettings.dryRun = !g_tuiSettings.dryRun; break;
                case 4: g_tuiSettings.killLocks = !g_tuiSettings.killLocks; break;
                case 5: {
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
                case 6: {
                    OpenTUI::TerminalEngine::ClearScreen();
                    PrintBanner();
                    std::string newPath = OpenTUI::TextInput::ReadLine("Enter target PATH folders (comma-separated): ", g_tuiSettings.customPathsStr);
                    if (!newPath.empty()) g_tuiSettings.customPathsStr = newPath;
                    break;
                }
                case 7: {
                    // Reset to OS-aware safe default temp/cache paths
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
        std::vector<std::string> queryMenuOptions = {
            "[Custom Query]",
            "CLEAN WHERE FREE_DISK < 500MB",
            "KILL PROCESS WHERE RAM > 200MB",
            "MONITOR WHERE RAM > 80%",
            "SHRED WHERE SIZE > 10MB",
            "PURGE RECYCLE_BIN",
            "SCAN WHERE AGE > 24H"
        };

        OpenTUI::Menu agentMenu("AGENT QUERY", queryMenuOptions);
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

        auto showHelp = []() {
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
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[1;33m  CONDITIONS\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;35mFREE_DISK < 500MB\033[0m | \033[1;35m2GB\033[0m       \033[90mTrigger when free disk space below threshold\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;35mRAM > 80%\033[0m | \033[1;35mRAM > 200MB\033[0m      \033[90mTrigger when RAM usage exceeds threshold\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;35mSIZE > 10MB\033[0m | \033[1;35m1GB\033[0m            \033[90mOnly match files larger than given size\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;35mAGE > 24H\033[0m | \033[1;35m7D\033[0m | \033[1;35m30D\033[0m         \033[90mOnly match files older than given age\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;35mEXT IN ('.log','.tmp')\033[0m         \033[90mFilter by file extension list\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m╠══════════════════════════════════════════════════════════════════════════════╣\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[1;33m  EXAMPLES\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;97mCLEAN 'C:/Users/hasee/AppData/Local/Temp' WHERE FREE_DISK < 1GB\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;97mSCAN 'C:/Windows/Temp' WHERE AGE > 1H\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;97mSHRED 'D:/Temp' WHERE SIZE > 10MB\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;97mKILL PROCESS WHERE RAM > 200MB\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;97mMONITOR WHERE RAM > 80% EVERY 15S\033[0m\n";
            std::cout << "\033[1;36m║  \033[1;97mPURGE RECYCLE_BIN\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m╠══════════════════════════════════════════════════════════════════════════════╣\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║\033[1;33m  NATURAL LANGUAGE TASKS (ReAct Agent)\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m║  \033[90m\"Clean temp files older than 7 days\"\033[0m\n";
            std::cout << "\033[1;36m║  \033[90m\"Kill all processes using more than 300MB of RAM\"\033[0m\n";
            std::cout << "\033[1;36m║  \033[90m\"Free up disk space on C: drive\"\033[0m\n";
            std::cout << "\033[1;36m║\033[0m\n";
            std::cout << "\033[1;36m╠══════════════════════════════════════════════════════════════════════════════╣\033[0m\n";
            std::cout << "\033[1;36m║  \033[90mTIP: TAB auto-completes · ESC cancels · Enter runs · 'help' shows this\033[0m\n";
            std::cout << "\033[1;36m╚══════════════════════════════════════════════════════════════════════════════╝\033[0m\n\n";
        };

        if (choice <= 0) {
            // Custom query input loop — intercept 'help'/'?' before launching
            while (true) {
                showHelp();
                std::string q = OpenTUI::TextInput::ReadLine("Query: ", "CLEAN 'C:/Users/hasee/AppData/Local/Temp' WHERE FREE_DISK < 500MB", suggestions);
                std::string qLower = q;
                std::transform(qLower.begin(), qLower.end(), qLower.begin(), ::tolower);
                if (qLower == "help" || qLower == "?" || qLower == "h") {
                    // Re-show help on next iteration
                    continue;
                }
                return q;
            }
        }

        static const std::vector<std::string> preMadeQueries = {
            "CLEAN 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE FREE_DISK < 500MB",
            "KILL PROCESS WHERE RAM > 200MB",
            "MONITOR WHERE RAM > 80% EVERY 15S",
            "SHRED 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE SIZE > 10MB",
            "PURGE RECYCLE_BIN",
            "SCAN 'C:\\' WHERE AGE > 24H"
        };

        return preMadeQueries[choice - 1];
    }

    static void RunInteractiveMenu(Cleaner& cleaner) {
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

        OpenTUI::Menu menu("SYSTEM-CLEANER-AGENT", options, g_tuiSettings.tuiThemeEngine);
        menu.SetPreRenderCallback([]() { PrintBanner(); });
        bool firstRender = true;

        while (true) {
            menu.SetTheme(g_tuiSettings.tuiThemeEngine);
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

                        OpenTUI::Menu ramMenu("RAM CLEANER PERMISSIONS", ramMenuOptions);
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
                    // Guard: skip if empty or the user just typed 'help'
                    if (selectedQuery.empty()) break;
                    std::string sqLower = selectedQuery;
                    std::transform(sqLower.begin(), sqLower.end(), sqLower.begin(), ::tolower);
                    if (sqLower == "help" || sqLower == "?" || sqLower == "h") break;

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

            std::cout << "\n\033[90mTask running in background! Press Enter to return to main OpenTUI menu...\033[0m";
            FlushInputBuffer();
            std::cin.get();
        }
    }
};
