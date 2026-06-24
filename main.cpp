#include "Cleaner.hpp"
#include "Logger.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

void PrintHelp() {
    std::cout << "=======================================\n"
              << "      Gemini System Cleaner v1.0       \n"
              << "=======================================\n"
              << "Usage: gemini-sys-cleaner [command] [options]\n\n"
              << "Commands:\n"
              << "  scan    Scan temp directories and calculate total size.\n"
              << "  clean   Scan and clean temp directories.\n"
              << "  help    Show this help message.\n\n"
              << "Options:\n"
              << "  --cron <duration>  Run periodically (e.g., 10s, 5m, 1h)\n";
}

long long ParseDuration(const std::string& durationStr) {
    if (durationStr.empty()) return 0;
    char unit = durationStr.back();
    std::string valueStr = durationStr.substr(0, durationStr.size() - 1);
    try {
        long long value = std::stoll(valueStr);
        if (unit == 's') return value;
        if (unit == 'm') return value * 60;
        if (unit == 'h') return value * 3600;
    } catch (...) {}
    return 0;
}

int main(int argc, char* argv[]) {
    // Initialize logging
    Logger::Instance().Init("gemini-sys-cleaner.log");

    if (argc < 2) {
        PrintHelp();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        PrintHelp();
        return 0;
    }

    long long cronIntervalSeconds = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cron" && i + 1 < argc) {
            cronIntervalSeconds = ParseDuration(argv[i+1]);
        }
    }

    Logger::Instance().Info("Application started with command: " + cmd);
    Cleaner cleaner;

    auto runTask = [&]() {
        if (cmd == "scan") {
            cleaner.Scan();
        } else if (cmd == "clean") {
            cleaner.Clean();
        } else {
            Logger::Instance().Error("Unknown command: " + cmd);
            PrintHelp();
            exit(1);
        }
    };

    if (cronIntervalSeconds > 0) {
        Logger::Instance().Info("Running in cron mode every " + std::to_string(cronIntervalSeconds) + " seconds.");
        while (true) {
            runTask();
            Logger::Instance().Info("Sleeping for " + std::to_string(cronIntervalSeconds) + " seconds...");
            std::this_thread::sleep_for(std::chrono::seconds(cronIntervalSeconds));
        }
    } else {
        runTask();
    }

    Logger::Instance().Info("Application finished.");
    return 0;
}
