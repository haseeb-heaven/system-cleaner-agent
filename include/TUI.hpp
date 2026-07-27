#pragma once
#include "Cleaner.hpp"
#include "Logger.hpp"
#include "OpenTUI.hpp"
#include "SmartScheduler.hpp"
#include "AgentEngine.hpp"
#include "AgentQueryLanguage.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _WIN32
#include <conio.h>
#endif

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
        if (isRunning) {
            return "[STATUS] 🟢 ACTIVE: " + taskName;
        } else {
            return "[STATUS] ⚪ READY | " + lastMessage;
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

    static std::string SelectAgentQuery() {
        std::vector<std::string> queryMenuOptions = {
            "[Custom Query]",
            "CLEAN WHERE FREE_DISK < 500MB",
            "MONITOR WHERE RAM > 80%",
            "SHRED WHERE SIZE > 10MB",
            "PURGE RECYCLE_BIN",
            "SCAN WHERE AGE > 24H"
        };

        OpenTUI::Menu agentMenu("AGENT QUERY", queryMenuOptions);
        int choice = agentMenu.Show();

        if (choice <= 0) {
            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();
            FlushInputBuffer();
            std::cout << "\033[1;36mQuery: \033[0m";
            std::string userQuery;
            std::getline(std::cin, userQuery);
            if (userQuery.empty()) {
                userQuery = "CLEAN 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE FREE_DISK < 500MB";
            }
            return userQuery;
        }

        static const std::vector<std::string> preMadeQueries = {
            "CLEAN 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE FREE_DISK < 500MB",
            "MONITOR WHERE RAM > 80% EVERY 15S",
            "SHRED 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE SIZE > 10MB",
            "PURGE RECYCLE_BIN",
            "SCAN 'C:\\' WHERE AGE > 24H"
        };

        return preMadeQueries[choice - 1];
    }

    static void RunInteractiveMenu(Cleaner& cleaner) {
        std::vector<std::string> options = {
            "Storage Scan",
            "Smart Deep Clean",
            "Secure Shred Wipe",
            "Empty Recycle Bin",
            "Agent & AQL Query",
            "Daemon Monitor",
            "Export JSON Report",
            "Engine Unit Tests",
            "Exit Agent"
        };

        OpenTUI::Menu menu("SYSTEM-CLEANER-AGENT", options);

        while (true) {
            menu.SetStatusLine(g_tuiStatus.GetStatusLine());
            int selected = menu.Show();

            if (selected == -1 || selected == 8) {
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
                        cleaner.SetDryRun(false);
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
                        cleaner.SetDryRun(false);
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
                    std::string selectedQuery = SelectAgentQuery();
                    std::cout << "\033[1;33mLaunching background Agent Task: " << selectedQuery << "\033[0m\n";
                    std::thread worker([selectedQuery]() {
                        g_tuiStatus.SetActive("Agent Task: " + selectedQuery);
                        AgentEngine agent(selectedQuery);
                        agent.RunReActLoop(true);
                        g_tuiStatus.SetCompleted("Agent Task finished: " + selectedQuery);
                    });
                    worker.detach();
                    break;
                }
                case 5: {
                    std::cout << "\033[1;36mLaunching background Smart Daemon Service...\033[0m\n";
                    std::thread worker([&cleaner]() {
                        g_tuiStatus.SetActive("Smart Daemon (RAM > 80% / Free Disk < 500MB)");
                        SmartScheduler::RunDaemonService(cleaner, {}, 80.0, 0.0, 15, true, 500ULL * 1024 * 1024, "C:\\");
                    });
                    worker.detach();
                    break;
                }
                case 6: {
                    cleaner.SetDryRun(true);
                    auto reports = cleaner.Scan();
                    std::cout << "\nReport complete. Total targets scanned: " << reports.size() << "\n";
                    break;
                }
                case 7: {
                    std::cout << "\033[1;32mRunning OpenTUI & Smart Scheduler Diagnostics...\033[0m\n";
                    std::cout << "System RAM Usage: " << SmartScheduler::GetMemoryUsagePercent() << "%\n";
                    std::cout << "OpenTUI Progress Bar Test: " << OpenTUI::ProgressBar::Render(SmartScheduler::GetMemoryUsagePercent()) << "\n";
                    Logger::Instance().Info("All OpenTUI and SmartScheduler components operational.");
                    break;
                }
            }

            std::cout << "\n\033[90mTask running in background! Press Enter to return to main OpenTUI menu...\033[0m";
            FlushInputBuffer();
            std::cin.get();
        }
    }
};
