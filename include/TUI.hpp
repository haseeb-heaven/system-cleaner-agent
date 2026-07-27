#pragma once
#include "Cleaner.hpp"
#include "Logger.hpp"
#include "OpenTUI.hpp"
#include "SmartScheduler.hpp"
#include "AgentEngine.hpp"
#include "AgentQueryLanguage.hpp"
#include "ConfigManager.hpp"

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
    g_tuiSettings.SyncFromAppConfig(cfg);
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
                  << "   _____ _   _ _____ _____ _____ ___  ___ _____ _     _____ ___   _   _ _____ _____ _   _ _____ \n"
                  << "  /  ___| | | /  ___|_   _|  ___|  \\/  |/  __ \\ |   |  ___/ _ \\ | \\ | |  ___|  ___| \\ | |_   _|\n"
                  << "  \\ `--.| |_| \\ `--.  | | | |__ | .  . || /  \\/ |   | |__/ /_\\ \\|  \\| | |__ | |__ |  \\| | | |  \n"
                  << "   `--. \\__  | `--. \\ | | |  __|| |\\/| || |   | |   |  __|  _  || . ` |  __||  __|| . ` | | |  \n"
                  << "  /\\__/ / | |/\\__/ / | | | |___| |  | || \\__/\\ |___| |__| | | || |\\  | |___| |___| |\\  | | |  \n"
                  << "  \\____/  \\_/\\____/  \\_/ \\____/\\_|  |_/ \\____/\\____/\\____\\_| |_/\\_| \\_/\\____/\\____/\\_| \\_/ \\_/  \n"
                  << "\033[0m"
                  << "\033[1;32m   [ system-cleaner-agent v5.0 - 100% Pure C++17 OpenTUI Suite ]\033[0m\n\n";
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

    static void ShowSettingsMenu(Cleaner& cleaner) {
        while (true) {
            std::vector<std::string> settingsOptions = {
                std::string("Sandbox Mode:     [") + (g_tuiSettings.sandboxMode ? "ON  - Preview Only" : "OFF - REAL DELETION ALLOWED") + "]",
                std::string("Path Protection:  [") + (g_tuiSettings.pathProtection ? "ON  - System Dir Guard" : "OFF - Disabled") + "]",
                std::string("Dry-Run Mode:     [") + (g_tuiSettings.dryRun ? "ON  - Preview Only" : "OFF - REAL CLEAN") + "]",
                std::string("Kill Locks:       [") + (g_tuiSettings.killLocks ? "ON" : "OFF") + "]",
                std::string("Monitor Interval: [") + std::to_string(g_tuiSettings.monitorIntervalSec) + " seconds]",
                std::string("Target Folders:   [") + g_tuiSettings.customPathsStr + "]",
                "Save & Return to Dashboard"
            };

            OpenTUI::Menu settingsMenu("SETTINGS", settingsOptions);
            OpenTUI::MenuSelection sel = settingsMenu.ShowExtended();

            if (sel.index == -1 || sel.index == 6) {
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
            std::cout << "\033[90m(Type query or press TAB to autocomplete suggestion)\033[0m\n\n";
            return OpenTUI::TextInput::ReadLine("Query: ", "CLEAN 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE FREE_DISK < 500MB", suggestions);
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
            "Settings",
            "Exit Agent"
        };

        OpenTUI::Menu menu("SYSTEM-CLEANER-AGENT", options);

        while (true) {
            menu.SetHeaderLines(GetLiveResourceHeaders());
            menu.SetStatusLine(g_tuiStatus.GetStatusLine());
            int selected = menu.Show();

            if (selected == -1 || selected == 9) {
                std::cout << "\n\033[32mExiting system-cleaner-agent OpenTUI Suite. Goodbye!\033[0m\n";
                break;
            }

            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();
            std::cout << "--------------------------------------------------------------------------------\n";

            switch (selected) {
                case 0: {
                    std::cout << "\033[1;36mLaunching background Storage Scan...\033[0m\n";
                    std::thread worker([&cleaner]() {
                        g_tuiStatus.SetActive("Storage Scan");
                        cleaner.SetDryRun(true);
                        auto reports = cleaner.Scan();
                        uintmax_t freed = 0;
                        for (const auto& r : reports) freed += r.sizeBytes;
                        g_tuiStatus.SetCompleted("Scan completed. Cleanable: " + Cleaner::FormatSize(freed));
                    });
                    worker.detach();
                    break;
                }
                case 1: {
                    std::cout << "\033[1;36mLaunching background Smart Deep Clean...\033[0m\n";
                    std::thread worker([&cleaner]() {
                        g_tuiStatus.SetActive("Smart Deep Clean");
                        cleaner.SetSandbox(g_tuiSettings.sandboxMode);
                        cleaner.SetDryRun(g_tuiSettings.dryRun);
                        cleaner.Clean();
                        g_tuiStatus.SetCompleted("Deep Clean finished.");
                    });
                    worker.detach();
                    break;
                }
                case 2: {
                    std::cout << "\033[1;36mLaunching background Secure Shred Wipe...\033[0m\n";
                    std::thread worker([&cleaner]() {
                        g_tuiStatus.SetActive("Secure Shred Wipe");
                        cleaner.SetMode(CleanMode::Shred);
                        cleaner.SetSandbox(g_tuiSettings.sandboxMode);
                        cleaner.SetDryRun(g_tuiSettings.dryRun);
                        cleaner.Clean();
                        g_tuiStatus.SetCompleted("Shred Wipe finished.");
                    });
                    worker.detach();
                    break;
                }
                case 3: {
                    std::cout << "\033[1;36mLaunching background Empty Recycle Bin...\033[0m\n";
                    std::thread worker([&cleaner]() {
                        g_tuiStatus.SetActive("Empty Recycle Bin");
                        cleaner.SetEmptyRecycleBin(true);
                        cleaner.EmptyWindowsRecycleBin();
                        g_tuiStatus.SetCompleted("Recycle Bin emptied.");
                    });
                    worker.detach();
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
                    bool currentDryRun = g_tuiSettings.dryRun || g_tuiSettings.sandboxMode;
                    std::thread worker([selectedQuery, currentDryRun]() {
                        g_tuiStatus.SetActive("Agent Task: " + selectedQuery);
                        AgentEngine agent(selectedQuery);
                        agent.RunReActLoop(currentDryRun);
                        g_tuiStatus.SetCompleted("Agent Task finished: " + selectedQuery);
                    });
                    worker.detach();
                    break;
                }
                case 6: {
                    std::cout << "\033[1;36mLaunching background Smart Daemon Service...\033[0m\n";
                    bool currentDryRun = g_tuiSettings.dryRun || g_tuiSettings.sandboxMode;
                    int interval = g_tuiSettings.monitorIntervalSec;
                    std::thread worker([&cleaner, currentDryRun, interval]() {
                        g_tuiStatus.SetActive("Smart Daemon (RAM > 80% / Free Disk < 500MB)");
                        SmartScheduler::RunDaemonService(cleaner, {}, 80.0, 0.0, interval, currentDryRun, 500ULL * 1024 * 1024, "C:\\");
                    });
                    worker.detach();
                    break;
                }
                case 7: {
                    ShowSystemResourceMonitor();
                    break;
                }
                case 8: {
                    ShowSettingsMenu(cleaner);
                    break;
                }
            }

            std::cout << "\n\033[90mTask running in background! Press Enter to return to main OpenTUI menu...\033[0m";
            FlushInputBuffer();
            std::cin.get();
        }
    }
};
