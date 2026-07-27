#include "include/Cleaner.hpp"
#include "include/Logger.hpp"
#include "include/ProcessManager.hpp"
#include "include/ContentInspector.hpp"
#include "include/TUI.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>
#include <sstream>

void PrintHeader() {
    TUI::PrintBanner();
}

void PrintHelp() {
    PrintHeader();
    std::cout << "\033[1mUSAGE:\033[0m\n"
              << "  gemini-sys-cleaner-pro <COMMAND> [FLAGS]\n"
              << "  gemini-pro-cleaner     <COMMAND> [FLAGS]\n\n"
              << "\033[1mCOMMANDS:\033[0m\n"
              << "  \033[36mscan\033[0m         Analyze system/drive targets and report cleanable storage.\n"
              << "  \033[36mclean\033[0m        Execute multi-threaded cleanup using active policy rules.\n"
              << "  \033[36mdeep-clean\033[0m   Perform full system cache cleanup + empty OS Recycle Bin / Trash.\n"
              << "  \033[36mtui\033[0m          Launch interactive Terminal User Interface (TUI).\n"
              << "  \033[36mtest\033[0m         Execute automated engine diagnostic & unit test suite.\n"
              << "  \033[36mversion\033[0m      Display version, engine build, and architecture details.\n"
              << "  \033[36mhelp\033[0m         Show this help and usage specification.\n\n"
              << "\033[1mFILTERING & TARGET SELECTION (WHAT):\033[0m\n"
              << "  \033[33m--category <list>\033[0m     Target categories: system, browser, dev, messaging, app.\n"
              << "  \033[33m--exclude-category <c>\033[0m Exclude target categories from operation.\n"
              << "  \033[33m--only-ext <ext1,ext2>\033[0m Only operate on matching file extensions (e.g. .log,.tmp).\n"
              << "  \033[33m--exclude-ext <exts>\033[0m   Protect specific extensions from deletion (e.g. .py,.cpp).\n"
              << "  \033[33m--older-than <dur>\033[0m     Filter files older than duration (e.g. 30m, 24h, 7d, 30d).\n"
              << "  \033[33m--min-size <size>\033[0m      Minimum file size threshold (e.g. 10MB, 100MB, 1GB).\n"
              << "  \033[33m--max-size <size>\033[0m      Maximum file size threshold.\n"
              << "  \033[33m--strategy <mode>\033[0m      Inspection policy: smart (default), force, safe.\n\n"
              << "\033[1mLOCATION & SCOPE (WHERE):\033[0m\n"
              << "  \033[33m--path <p1,p2>\033[0m         Specify custom directory path(s) to process.\n"
              << "  \033[33m--drive <drives>\033[0m       Target specific drive(s) (e.g. C:\\, D:\\) or 'all'.\n"
              << "  \033[33m--project-root <dir>\033[0m   Root directory for recursive dev build cache discovery.\n\n"
              << "\033[1mEXECUTION CONTROL (HOW):\033[0m\n"
              << "  \033[33m--mode <mode>\033[0m          Execution mode: light, deep, full, shred (secure wipe).\n"
              << "  \033[33m--dry-run\033[0m              Preview operational results without disk state mutation.\n"
              << "  \033[33m--recycle-bin\033[0m          Purge Windows Recycle Bin / OS Trash via Shell API.\n"
              << "  \033[33m--kill-locks <bool>\033[0m    Release file lock handles before operation (default: true).\n"
              << "  \033[33m--threads <N>\033[0m          Worker thread count for parallel processing (default: auto).\n"
              << "  \033[33m--json-report <file>\033[0m   Export structured execution results to JSON report.\n"
              << "  \033[33m--cron <duration>\033[0m      Run daemon service on recurring schedule (e.g. 10m, 1h).\n"
              << "  \033[33m--verbose\033[0m              Enable detailed trace logging.\n\n"
              << "\033[1mPRODUCTION EXAMPLES:\033[0m\n"
              << "  gemini-sys-cleaner-pro tui\n"
              << "  gemini-sys-cleaner-pro scan --dry-run\n"
              << "  gemini-sys-cleaner-pro deep-clean --recycle-bin\n"
              << "  gemini-sys-cleaner-pro clean --path \"D:\\tmp\" --older-than 24h\n"
              << "  gemini-sys-cleaner-pro scan --drive all --json-report report.json\n\n";
}

std::vector<std::string> SplitString(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

void ExportJsonReport(const std::string& jsonPath, const std::vector<TargetReport>& reports) {
    try {
        std::ofstream jsonFile(jsonPath);
        if (!jsonFile.is_open()) return;

        jsonFile << "{\n";
        jsonFile << "  \"engine\": \"Gemini System Cleaner Professional Edition v3.5 (C++17)\",\n";
        jsonFile << "  \"targets\": [\n";

        uintmax_t grandTotal = 0;
        for (size_t i = 0; i < reports.size(); ++i) {
            const auto& r = reports[i];
            grandTotal += r.sizeBytes;
            jsonFile << "    {\n";
            jsonFile << "      \"name\": \"" << r.name << "\",\n";
            jsonFile << "      \"category\": \"" << r.category << "\",\n";
            jsonFile << "      \"path\": \"" << r.path.string() << "\",\n";
            jsonFile << "      \"size_bytes\": " << r.sizeBytes << ",\n";
            jsonFile << "      \"file_count\": " << r.fileCount << ",\n";
            jsonFile << "      \"protected_files_preserved\": " << (r.skippedProtectedFiles ? "true" : "false") << "\n";
            jsonFile << "    }" << (i + 1 < reports.size() ? "," : "") << "\n";
        }

        jsonFile << "  ],\n";
        jsonFile << "  \"total_cleanable_bytes\": " << grandTotal << ",\n";
        jsonFile << "  \"total_cleanable_formatted\": \"" << Cleaner::FormatSize(grandTotal) << "\"\n";
        jsonFile << "}\n";

        Logger::Instance().Info("JSON execution report saved: " + jsonPath);
    } catch (const std::exception& e) {
        Logger::Instance().Error("JSON export failed: " + std::string(e.what()));
    }
}

long long ParseDuration(const std::string& durationStr) {
    if (durationStr.empty()) return 0;
    char unit = durationStr.back();
    std::string valueStr = durationStr.substr(0, durationStr.size() - 1);
    try {
        long long value = std::stoll(valueStr);
        if (unit == 's' || unit == 'S') return value;
        if (unit == 'm' || unit == 'M') return value * 60;
        if (unit == 'h' || unit == 'H') return value * 3600;
    } catch (...) {}
    return 0;
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") verbose = true;
    }

    Logger::Instance().Init("gemini-sys-cleaner.log", verbose);

    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    std::string cmd = argv[1];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        PrintHelp();
        return 0;
    }

    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        std::cout << "Gemini System Cleaner Professional Edition v3.5.0 (C++17 Enterprise Engine - 64-bit Architecture)\n";
        return 0;
    }

    Cleaner cleaner;
    InspectionConfig cfg;
    CleanMode cleanMode = CleanMode::Light;

    if (cmd == "tui" || cmd == "--interactive" || cmd == "-i") {
        TUI::RunInteractiveMenu(cleaner);
        return 0;
    }

    if (cmd == "test" || cmd == "--test") {
        std::cout << "Executing Engine Unit Tests...\n";
#ifdef _WIN32
        system("unit_tests.exe");
#else
        system("./unit_tests");
#endif
        return 0;
    }

    bool dryRun = false;
    bool recycleBin = false;
    bool killLocks = true;
    long long cronIntervalSeconds = 0;
    size_t threadCount = 0;
    std::string jsonReportPath = "";

    std::vector<fs::path> customPaths;
    std::vector<fs::path> targetDrives;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        std::string lowerArg = arg;
        std::transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);

        if (lowerArg == "--dry-run") {
            dryRun = true;
        } else if (lowerArg == "--recycle-bin") {
            recycleBin = true;
        } else if (lowerArg == "--kill-locks" && i + 1 < argc) {
            std::string val = argv[++i];
            killLocks = (val == "true" || val == "1" || val == "yes");
        } else if (lowerArg == "--cron" && i + 1 < argc) {
            cronIntervalSeconds = ParseDuration(argv[++i]);
        } else if (lowerArg == "--threads" && i + 1 < argc) {
            try { threadCount = std::stoul(argv[++i]); } catch (...) {}
        } else if (lowerArg == "--json-report" && i + 1 < argc) {
            jsonReportPath = argv[++i];
        } else if (lowerArg == "--path" && i + 1 < argc) {
            auto pathList = SplitString(argv[++i], ',');
            for (const auto& p : pathList) customPaths.push_back(fs::path(p));
        } else if (lowerArg == "--drive" && i + 1 < argc) {
            auto driveList = SplitString(argv[++i], ',');
            for (const auto& d : driveList) targetDrives.push_back(fs::path(d));
        } else if (lowerArg == "--project-root" && i + 1 < argc) {
            cleaner.SetProjectRoot(fs::path(argv[++i]));
        } else if (lowerArg == "--mode" && i + 1 < argc) {
            std::string mStr = argv[++i];
            std::transform(mStr.begin(), mStr.end(), mStr.begin(), ::tolower);
            if (mStr == "light") cleanMode = CleanMode::Light;
            else if (mStr == "deep") cleanMode = CleanMode::Deep;
            else if (mStr == "full") cleanMode = CleanMode::Full;
            else if (mStr == "shred") cleanMode = CleanMode::Shred;
        } else if (lowerArg == "--strategy" && i + 1 < argc) {
            cfg.strategy = argv[++i];
            std::transform(cfg.strategy.begin(), cfg.strategy.end(), cfg.strategy.begin(), ::tolower);
        } else if (lowerArg == "--category" && i + 1 < argc) {
            auto catList = SplitString(argv[++i], ',');
            for (auto c : catList) {
                std::transform(c.begin(), c.end(), c.begin(), ::tolower);
                cfg.includeCategories.insert(c);
            }
        } else if (lowerArg == "--exclude-category" && i + 1 < argc) {
            auto catList = SplitString(argv[++i], ',');
            for (auto c : catList) {
                std::transform(c.begin(), c.end(), c.begin(), ::tolower);
                cfg.excludeCategories.insert(c);
            }
        } else if (lowerArg == "--only-ext" && i + 1 < argc) {
            auto extList = SplitString(argv[++i], ',');
            for (auto e : extList) {
                if (e.front() != '.') e = "." + e;
                std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                cfg.onlyExts.insert(e);
            }
        } else if (lowerArg == "--exclude-ext" && i + 1 < argc) {
            auto extList = SplitString(argv[++i], ',');
            for (auto e : extList) {
                if (e.front() != '.') e = "." + e;
                std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                cfg.excludeExts.insert(e);
            }
        } else if (lowerArg == "--older-than" && i + 1 < argc) {
            cfg.minAgeMinutes = ContentInspector::ParseDurationToMinutes(argv[++i]);
        } else if (lowerArg == "--min-size" && i + 1 < argc) {
            cfg.minSizeBytes = ContentInspector::ParseSizeToBytes(argv[++i]);
        } else if (lowerArg == "--max-size" && i + 1 < argc) {
            cfg.maxSizeBytes = ContentInspector::ParseSizeToBytes(argv[++i]);
        }
    }

    if (cmd == "deep-clean") {
        recycleBin = true;
        cleanMode = CleanMode::Deep;
        cmd = "clean";
    }

    cleaner.SetMode(cleanMode);
    cleaner.SetDryRun(dryRun);
    cleaner.SetEmptyRecycleBin(recycleBin);
    cleaner.SetKillLockingProcesses(killLocks);
    cleaner.SetCustomPaths(customPaths);
    cleaner.SetTargetDrives(targetDrives);
    cleaner.SetInspectionConfig(cfg);
    if (threadCount > 0) cleaner.SetMaxThreads(threadCount);

    PrintHeader();
    Logger::Instance().Info("Execution mode: " + cmd + (dryRun ? " [DRY-RUN]" : ""));

    auto executeTask = [&]() {
        if (cmd == "scan") {
            auto reports = cleaner.Scan();
            if (!jsonReportPath.empty()) {
                ExportJsonReport(jsonReportPath, reports);
            }
        } else if (cmd == "clean") {
            cleaner.Clean();
        } else {
            std::cout << "Invalid command: " << cmd << "\n\n";
            PrintHelp();
            exit(1);
        }
    };

    if (cronIntervalSeconds > 0) {
        Logger::Instance().Info("Running daemon service every " + std::to_string(cronIntervalSeconds) + " seconds.");
        while (true) {
            executeTask();
            Logger::Instance().Info("Service sleeping for " + std::to_string(cronIntervalSeconds) + " seconds...");
            std::this_thread::sleep_for(std::chrono::seconds(cronIntervalSeconds));
        }
    } else {
        executeTask();
    }

    return 0;
}
