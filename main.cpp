#include "Cleaner.hpp"
#include "Logger.hpp"
#include <iostream>
#include <string>

void PrintHelp() {
    std::cout << "=======================================\n"
              << "      Gemini System Cleaner v1.0       \n"
              << "=======================================\n"
              << "Usage: gemini-sys-cleaner.exe [command]\n\n"
              << "Commands:\n"
              << "  scan    Scan temp directories and calculate total size.\n"
              << "  clean   Scan and clean temp directories.\n"
              << "  help    Show this help message.\n";
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

    Logger::Instance().Info("Application started with command: " + cmd);
    Cleaner cleaner;

    if (cmd == "scan") {
        cleaner.Scan();
    } else if (cmd == "clean") {
        cleaner.Clean();
    } else {
        Logger::Instance().Error("Unknown command: " + cmd);
        PrintHelp();
    }

    Logger::Instance().Info("Application finished.");
    return 0;
}
