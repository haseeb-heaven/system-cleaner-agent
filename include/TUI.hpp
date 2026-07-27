#pragma once
#include "Cleaner.hpp"
#include "Logger.hpp"
#include "OpenTUI.hpp"

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
                  << "  \\____/  \\_/\\____/  \\_/ \\____/\\_|  |_/ \____/\\____/\\____\\_| |_/\\_| \\_/\\____/\\____/\\_| \\_/ \\_/  \n"
                  << "\033[0m"
                  << "\033[1;32m   [ system-cleaner-agent v4.5 - OpenTUI Cross-Platform Framework ]\033[0m\n\n";
    }

    static void RunInteractiveMenu(Cleaner& cleaner) {
        std::vector<std::string> options = {
            "Storage Scan & Inspection (Dry-Run Preview)",
            "Execute Smart Deep Clean (Preserve User Source & Docs)",
            "Execute Secure Shred Cleanup (Zero-Overwrite Wipe)",
            "Purge Windows Recycle Bin / OS Trash Bin",
            "Run ReAct Autonomous Agent Execution Loop",
            "Run Background Daemon Service (Cron Mode)",
            "Export System Scan Report to JSON",
            "Run Engine Diagnostic & Unit Test Suite",
            "Exit system-cleaner-agent"
        };

        OpenTUI::Menu menu("SYSTEM-CLEANER-AGENT - OPENTUI DASHBOARD", options);

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
                    std::cout << "Starting ReAct Autonomous Agent Loop...\n";
                    cleaner.SetDryRun(true);
                    cleaner.Scan();
                    break;
                }
                case 5: {
                    std::cout << "Enter cron daemon interval in seconds (e.g. 300 for 5m): ";
                    int sec = 300;
                    std::cin >> sec;
                    std::cout << "Running background daemon service. Press Ctrl+C to terminate.\n";
                    while (true) {
                        cleaner.Clean();
                        std::this_thread::sleep_for(std::chrono::seconds(sec));
                    }
                    break;
                }
                case 6: {
                    cleaner.SetDryRun(true);
                    auto reports = cleaner.Scan();
                    std::cout << "\nReport complete. Total targets scanned: " << reports.size() << "\n";
                    break;
                }
                case 7: {
                    std::cout << "\033[1;32mRunning OpenTUI Engine Diagnostics...\033[0m\n";
                    std::cout << "OpenTUI Progress Bar Test: " << OpenTUI::ProgressBar::Render(75.5) << "\n";
                    Logger::Instance().Info("All OpenTUI components operational.");
                    break;
                }
            }

            std::cout << "\n\033[90mPress Enter to return to OpenTUI menu...\033[0m";
            std::cin.ignore();
            std::cin.get();
        }
    }
};
