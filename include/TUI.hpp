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
    std::vector<std::string> customProtectedProcesses;

    void SyncFromAppConfig(const AppConfig& cfg) {
        sandboxMode = cfg.sandboxMode;
        pathProtection = cfg.pathProtection;
        dryRun = cfg.dryRun;
        killLocks = cfg.killLocks;
        monitorIntervalSec = cfg.monitorIntervalSec;
        ramThresholdMB = cfg.ramThresholdMB;
        customPathsStr = cfg.customPathsStr;
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
        if (isRunning) {
            return "[STATUS] 🟢 ACTIVE: " + taskName + modeStr;
        } else {
            return "[STATUS] ⚪ READY | " + lastMessage + modeStr;
        }
    }
};

static TUITaskStatus g_tuiStatus;

class TUI {
public:
    static void PrintBanner() {
        std::cout << "\033[1;36m"
                  << "    _/_\\_      ____  _  _  ____  ____  ____  _  _   \n"
                  << "   /     \\    / ___)( \\/ )( ___)(_  _)(  __)( \\/ )  \n"
                  << "  |   *   |   \\___ \\ )  /  )__)   )(   ) _) / \\/ \\  \n"
                  << "   \\     /    (____/(__/  (____) (__) (____)\\_/\\_/  \n"
                  << "    \\___/     \033[1;33mSYSTEM-CLEANER-AGENT \033[1;32mv5.3\033[0m\n"
                  << "\033[90m  [ Autonomous ReAct Agent | C++17 OpenTUI | AQL Engine ]\033[0m\n\n";
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


    // =================================================================
    //  Task Library / History Viewer
    // =================================================================
    static void ShowTaskLibrary() {
        std::vector<std::string> options = {
            "Live Task List (auto-refresh)",
            "All Tasks (chronological)",
            "Running Tasks Only",
            "Completed Tasks Only",
            "Failed Tasks Only",
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
            if (sel < 0 || sel == 6) break;

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
        auto renderOnce = [statusFilter]() {
            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();
            std::cout << "\033[1;36m================================================================================\033[0m" << std::endl;
            std::string filterLabel = "ALL TASKS";
            if (statusFilter == (int)TaskStatus::Running)   filterLabel = "RUNNING TASKS";
            else if (statusFilter == (int)TaskStatus::Completed) filterLabel = "COMPLETED TASKS";
            else if (statusFilter == (int)TaskStatus::Failed)    filterLabel = "FAILED TASKS";
            else if (statusFilter == (int)TaskStatus::Cancelled) filterLabel = "CANCELLED TASKS";
            std::cout << "\033[1;97m              TASK LIBRARY  -  " << filterLabel << "\033[0m" << std::endl;
            std::cout << "\033[1;36m================================================================================\033[0m" << std::endl;
            std::cout << TaskHistory::Instance().HeaderSummary() << std::endl << std::endl;

            auto tasks = TaskHistory::Instance().Snapshot();
            if (tasks.empty()) {
                std::cout << "\033[1;33m  No tasks recorded yet.\033[0m" << std::endl;
                std::cout << "\033[90m  Run any clean / scan / shred / AQL / agent / daemon operation to populate.\033[0m" << std::endl << std::endl;
                return;
            }
            std::reverse(tasks.begin(), tasks.end());

            std::cout << "\033[1;37m"
                      << std::left
                      << std::setw(6)  << "ID"
                      << std::setw(11) << "STATUS"
                      << std::setw(10) << "CATEGORY"
                      << std::setw(11) << "TIME"
                      << std::setw(9)  << "DUR"
                      << std::setw(26) << "NAME"
                      << std::setw(40) << "DETAIL / RESULT"
                      << "\033[0m" << std::endl;
            std::cout << "\033[90m" << std::string(113, '-') << "\033[0m" << std::endl;

            size_t shown = 0;
            for (auto& t : tasks) {
                if (statusFilter >= 0 && (int)t.status != statusFilter) continue;

                auto now    = std::chrono::system_clock::now();
                auto elapsed= std::chrono::duration_cast<std::chrono::milliseconds>(now - t.startedAt);
                bool isLive = (t.status == TaskStatus::Running || t.status == TaskStatus::Queued);

                std::string timeStr = TaskHistoryNS::FormatTimestamp(isLive ? t.startedAt : t.finishedAt);
                std::string durStr  = TaskHistoryNS::FormatDuration(elapsed);

                std::string name = t.name;
                if (name.size() > 24) name = name.substr(0, 21) + "...";

                std::string detail = isLive
                    ? (t.progressMsg.empty() ? TaskHistoryNS::StatusLabel(t.status) : t.progressMsg)
                    : (t.resultSummary.empty() ? TaskHistoryNS::StatusLabel(t.status) : t.resultSummary);
                if (detail.size() > 38) detail = detail.substr(0, 35) + "...";

                std::cout << "\033[90m"
                          << "[" << std::setw(3) << std::setfill('0') << t.id << "]\033[0m "
                          << TaskHistoryNS::StatusColor(t.status) << std::left
                          << std::setw(10) << TaskHistoryNS::StatusLabel(t.status) << std::right
                          << "\033[0m "
                          << std::left
                          << std::setw(10) << t.category << std::right
                          << " "
                          << "\033[1;33m" << std::left << std::setw(10) << timeStr << std::right << "\033[0m "
                          << "\033[90m" << std::left << std::setw(8)  << durStr << std::right << "\033[0m "
                          << "\033[1;97m" << std::left << std::setw(25) << name << std::right << "\033[0m "
                          << "\033[1;36m" << std::left << std::setw(38) << detail << std::right << "\033[0m";

                if (isLive && t.percent > 0.0 && t.percent < 100.0) {
                    int barWidth = 14;
                    int filled = static_cast<int>((t.percent / 100.0) * barWidth);
                    std::cout << "  \033[1;32m";
                    for (int i = 0; i < filled; ++i) std::cout << "#";
                    std::cout << "\033[90m";
                    for (int i = filled; i < barWidth; ++i) std::cout << "-";
                    std::cout << "\033[0m \033[1;33m" << static_cast<int>(t.percent) << "%\033[0m";
                } else if (t.bytesFreed > 0) {
                    std::cout << "  \033[1;32mfreed " << Cleaner::FormatSize(t.bytesFreed) << "\033[0m";
                } else if (t.filesProcessed > 0) {
                    std::cout << "  \033[1;36m" << t.filesProcessed << " files\033[0m";
                } else if (t.processesHandled > 0) {
                    std::cout << "  \033[1;36m" << t.processesHandled << " procs\033[0m";
                }
                std::cout << std::endl;
                ++shown;
            }
            if (shown == 0) {
                std::cout << "\033[1;33m  (no tasks match this filter)\033[0m" << std::endl;
            }
            std::cout << std::endl;
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
                std::string("Sandbox Mode:     [") + (g_tuiSettings.sandboxMode ? "ON  - Preview Only" : "OFF - REAL DELETION ALLOWED") + "]",
                std::string("Path Protection:  [") + (g_tuiSettings.pathProtection ? "ON  - System Dir Guard" : "OFF - Disabled") + "]",
                std::string("Dry-Run Mode:     [") + (g_tuiSettings.dryRun ? "ON  - Preview Only" : "OFF - REAL CLEAN") + "]",
                std::string("Kill Locks:       [") + (g_tuiSettings.killLocks ? "ON" : "OFF") + "]",
                std::string("Monitor Interval: [") + std::to_string(g_tuiSettings.monitorIntervalSec) + " seconds]",
                std::string("Target Folders:   [") + g_tuiSettings.customPathsStr.substr(0, 60) + (g_tuiSettings.customPathsStr.size() > 60 ? "..." : "") + "]",
                std::string("Reset to OS Defaults (" + osName + " safe temp/cache paths)"),
                "Save & Return to Dashboard"
            };

            OpenTUI::Menu settingsMenu("SETTINGS", settingsOptions);
            OpenTUI::MenuSelection sel = settingsMenu.ShowExtended();

            if (sel.index == -1 || sel.index == 7) {
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
                break;
            }

            switch (sel.index) {
                case 0: g_tuiSettings.sandboxMode = !g_tuiSettings.sandboxMode; break;
                case 1: g_tuiSettings.pathProtection = !g_tuiSettings.pathProtection; break;
                case 2: g_tuiSettings.dryRun = !g_tuiSettings.dryRun; break;
                case 3: g_tuiSettings.killLocks = !g_tuiSettings.killLocks; break;
                case 4: {
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
                case 5: {
                    OpenTUI::TerminalEngine::ClearScreen();
                    PrintBanner();
                    std::string newPath = OpenTUI::TextInput::ReadLine("Enter target PATH folders (comma-separated): ", g_tuiSettings.customPathsStr);
                    if (!newPath.empty()) g_tuiSettings.customPathsStr = newPath;
                    break;
                }
                case 6: {
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

        if (choice <= 0) {
            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();
            std::cout << "\033[1;36m=== Agent Query Language (AQL) Help ===\033[0m" << std::endl;
            std::cout << "\033[90m" << std::endl;
            std::cout << "  COMMANDS:" << std::endl;
            std::cout << "    CLEAN  <path> WHERE <condition>     Clean files in path matching condition" << std::endl;
            std::cout << "    SCAN   <path> WHERE <condition>     Preview cleanable items" << std::endl;
            std::cout << "    SHRED  <path> WHERE <condition>     Secure-wipe with zero-overwrite before delete" << std::endl;
            std::cout << "    KILL   PROCESS WHERE <condition>    Terminate processes matching condition" << std::endl;
            std::cout << "    MONITOR <condition> EVERY <dur>     Watch thresholds & auto-clean" << std::endl;
            std::cout << "    PURGE  RECYCLE_BIN                  Empty OS Recycle Bin / Trash" << std::endl;
            std::cout << std::endl;
            std::cout << "  CONDITIONS:" << std::endl;
            std::cout << "    FREE_DISK < 500MB | 2GB              Free space below threshold" << std::endl;
            std::cout << "    RAM > 80%                            Memory usage percentage" << std::endl;
            std::cout << "    SIZE > 10MB | 1GB                    Minimum file size" << std::endl;
            std::cout << "    AGE  > 24H | 7D | 30D                File age threshold" << std::endl;
            std::cout << "    EXT IN ('.log', '.tmp')              Match extension list" << std::endl;
            std::cout << std::endl;
            std::cout << "  EXAMPLES:" << std::endl;
            std::cout << "    CLEAN  'D:/Temp' WHERE FREE_DISK < 1GB" << std::endl;
            std::cout << "    SCAN   'C:/Windows/Temp' WHERE AGE > 1H" << std::endl;
            std::cout << "    SHRED  'D:/Temp' WHERE SIZE > 10MB" << std::endl;
            std::cout << "    KILL   PROCESS WHERE RAM > 200MB" << std::endl;
            std::cout << "    MONITOR WHERE RAM > 80% EVERY 15S" << std::endl;
            std::cout << "    PURGE  RECYCLE_BIN" << std::endl;
            std::cout << "\033[0m" << std::endl;
            std::cout << "\033[90m(Type your query - TAB auto-completes, ESC keeps default, ENTER to run)\033[0m" << std::endl << std::endl;
            return OpenTUI::TextInput::ReadLine("Query: ", "CLEAN 'C:/Users/hasee/AppData/Local/Temp' WHERE FREE_DISK < 500MB", suggestions);
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
            "Task Library / History",
            "Settings",
            "Exit Agent"
        };

        OpenTUI::Menu menu("SYSTEM-CLEANER-AGENT", options);
        menu.SetPreRenderCallback([]() { PrintBanner(); });

        while (true) {
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
