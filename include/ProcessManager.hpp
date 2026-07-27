#pragma once
#include "Logger.hpp"
#include "gtlibc.hpp"

#include <string>
#include <vector>
#include <algorithm>

class ProcessManager {
public:
    // Safely kill orphan/dead background processes consuming more RAM than minRamBytes via GTLibc Engine
    static size_t KillHighMemoryProcesses(uintmax_t minRamBytes = 200ULL * 1024 * 1024, bool enableTermination = true) {
        Logger::Instance().Info("Scanning background processes using RAM > " + std::to_string(minRamBytes / (1024 * 1024)) + " MB via GTLibc Subsystem...");
        size_t count = GTLIBC::GTLibc::KillHighMemoryProcesses(minRamBytes, enableTermination);
        if (count > 0) {
            Logger::Instance().Info("GTLibc Engine processed " + std::to_string(count) + " background candidate process(es).");
        } else {
            Logger::Instance().Info("GTLibc Engine: No high-RAM background dead processes found.");
        }
        return count;
    }

    // Safely stop background processes locking temporary/cache directories via GTLibc Engine
    static size_t StopLockingProcesses(bool enableTermination = true) {
        if (!enableTermination) return 0;
        size_t count = 0;
        static const std::vector<std::string> targetProcesses = {
            "mintty.exe", "cat.exe", "bash.exe", "sh.exe", "werfault.exe"
        };
        for (const auto& proc : targetProcesses) {
            count += GTLIBC::GTLibc::KillProcessByName(proc);
        }
        if (count > 0) {
            Logger::Instance().Info("GTLibc Engine successfully released " + std::to_string(count) + " process lock(s).");
        } else {
            Logger::Instance().Info("GTLibc Engine: No background process lock handles detected.");
        }
        return count;
    }
};
