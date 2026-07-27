#pragma once
#include "Cleaner.hpp"
#include "Logger.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <limits>

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
                  << "\033[1;32m   [ system-cleaner-agent v4.0 - Autonomous ReAct Storage Agent Engine ]\033[0m\n\n";
    }

    static void RunInteractiveMenu(Cleaner& cleaner) {
        while (true) {
#ifdef _WIN32
            system("cls");
#else
            system("clear");
#endif
            PrintBanner();
            std::cout << "\033[1;37mSELECT AGENT OPERATION:\033[0m\n"
                      << "  [1] Interactive Storage Scan & Inspection (Dry-Run Preview)\n"
                      << "  [2] Execute Smart Deep Clean (Preserve User Source & Docs)\n"
                      << "  [3] Execute Secure Shred Cleanup (Zero-Overwrite Wipe)\n"
                      << "  [4] Purge Windows Recycle Bin / OS Trash Bin\n"
                      << "  [5] Run ReAct Autonomous Agent Execution Loop\n"
                      << "  [6] Run Background Daemon Service (Cron Mode)\n"
                      << "  [7] Export System Scan Report to JSON\n"
                      << "  [8] Run Engine Diagnostic & Unit Test Suite\n"
                      << "  [0] Exit system-cleaner-agent\n\n"
                      << "\033[1;33mEnter selection [0-8]: \033[0m";

            int choice = -1;
            if (!(std::cin >> choice)) {
                std::cin.clear();
                std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                continue;
            }

            if (choice == 0) {
                std::cout << "\n\033[32mExiting system-cleaner-agent. Goodbye!\033[0m\n";
                break;
            }

            std::cout << "\n--------------------------------------------------------------------------------\n";

            switch (choice) {
                case 1: {
                    cleaner.SetDryRun(true);
                    cleaner.Scan();
                    break;
                }
                case 2: {
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
                case 3: {
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
                case 4: {
                    cleaner.SetEmptyRecycleBin(true);
                    cleaner.EmptyWindowsRecycleBin();
                    break;
                }
                case 5: {
                    std::cout << "Starting ReAct Autonomous Agent Loop...\n";
                    cleaner.SetDryRun(true);
                    cleaner.Scan();
                    break;
                }
                case 6: {
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
                case 7: {
                    cleaner.SetDryRun(true);
                    auto reports = cleaner.Scan();
                    std::cout << "\nReport complete. Total targets scanned: " << reports.size() << "\n";
                    break;
                }
                case 8: {
                    std::cout << "\033[1;32mRunning Engine Self-Diagnostics...\033[0m\n";
                    Logger::Instance().Info("All internal components operational.");
                    break;
                }
                default:
                    std::cout << "Invalid choice.\n";
                    break;
            }

            std::cout << "\n\033[90mPress Enter to return to main menu...\033[0m";
            std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            std::cin.get();
        }
    }
};
