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

    static std::string SelectAgentQuery() {
        std::vector<std::string> queryMenuOptions = {
            "[Custom Query] Enter natural language or AQL query (e.g. CLEAN WHERE FREE_DISK < 500MB)",
            "AQL: CLEAN 'C:\\Temp' WHERE FREE_DISK < 500MB",
            "AQL: MONITOR WHERE RAM > 80% EVERY 15S",
            "AQL: SHRED 'C:\\Temp' WHERE SIZE > 10MB",
            "AQL: PURGE RECYCLE_BIN",
            "AQL: SCAN 'C:\\' WHERE AGE > 24H"
        };

        OpenTUI::Menu agentMenu("AGENT QUERY LANGUAGE (AQL) - SELECT OR TYPE QUERY", queryMenuOptions);
        int choice = agentMenu.Show();

        if (choice <= 0) {
            OpenTUI::TerminalEngine::ClearScreen();
            PrintBanner();
            std::cout << "\033[1;36mCleaner Agent > Enter AQL Query or Natural Goal:\033[0m ";
            std::string userQuery;
            std::getline(std::cin, userQuery);
            if (userQuery.empty()) {
                userQuery = "CLEAN 'C:\\Temp' WHERE FREE_DISK < 500MB";
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
            "Storage Scan (Dry-Run)",
            "Smart Deep Clean",
            "Secure Shred Wipe",
            "Empty Recycle Bin / Trash",
            "Autonomous ReAct Agent & AQL",
            "Smart Daemon & Monitor",
            "Export JSON Report",
            "Engine Unit Tests",
            "Exit Agent"
        };

        OpenTUI::Menu menu("SYSTEM-CLEANER-AGENT DASHBOARD", options);

        while (true) {
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
                    cleaner.SetDryRun(true);
                    cleaner.Scan();
                    break;
                }
                case 1: {
                    std::cout << "\033[1;31mWARNING: This will delete temporary junk files across your system.\033[0m\n"
                              << "Proceed with cleanup? (y/n): ";
                    char confirm;
                    std::cin >> confirm;
                    if (confirm == 'y' || confirm == 'Y') {
                        cleaner.SetDryRun(false);
                        cleaner.Clean();
                    } else {
                        std::cout << "Cleanup operation cancelled.\n";
                    }
                    break;
                }
                case 2: {
                    std::cout << "\033[1;31mWARNING: Secure shredding will zero-overwrite matching junk files before deletion.\033[0m\n"
                              << "Proceed with shredding? (y/n): ";
                    char confirm;
                    std::cin >> confirm;
                    if (confirm == 'y' || confirm == 'Y') {
                        cleaner.SetMode(CleanMode::Shred);
                        cleaner.SetDryRun(false);
                        cleaner.Clean();
                    } else {
                        std::cout << "Shred operation cancelled.\n";
                    }
                    break;
                }
                case 3: {
                    cleaner.SetEmptyRecycleBin(true);
                    cleaner.EmptyWindowsRecycleBin();
                    break;
                }
                case 4: {
                    std::string selectedQuery = SelectAgentQuery();
                    OpenTUI::TerminalEngine::ClearScreen();
                    PrintBanner();
                    std::cout << "\033[1;33mExecuting Agent Goal: " << selectedQuery << "\033[0m\n\n";
                    AgentEngine agent(selectedQuery);
                    agent.RunReActLoop(true);
                    break;
                }
                case 5: {
                    double currentMem = SmartScheduler::GetMemoryUsagePercent();
                    double currentDisk = SmartScheduler::GetDiskUsagePercent();
                    std::cout << "Current System RAM Usage: " << currentMem << "%\n";
                    std::cout << "Current Disk Storage Usage: " << currentDisk << "%\n\n";

                    std::cout << "Enter Memory RAM Threshold Percentage to trigger Auto-Clean (e.g. 80 for 80%): ";
                    double memThresh = 80.0;
                    std::cin >> memThresh;

                    std::cout << "Enter Check Interval in seconds (e.g. 15): ";
                    long long sec = 15;
                    std::cin >> sec;

                    std::cout << "Starting Smart Daemon Service... Press Ctrl+C to stop.\n";
                    SmartScheduler::RunDaemonService(cleaner, {}, memThresh, 0.0, sec, true);
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

            std::cout << "\n\033[90mPress Enter to return to OpenTUI menu...\033[0m";
            std::cin.ignore();
            std::cin.get();
        }
    }
};
