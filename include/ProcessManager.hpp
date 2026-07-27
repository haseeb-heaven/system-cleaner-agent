#pragma once
#include "Logger.hpp"
#include "gtlibc.hpp"

#include <string>
#include <vector>
#include <algorithm>

class ProcessManager {
public:
    // Query high-RAM candidate processes for inspection/confirmation (> 200 MB)
    static std::vector<GTLIBC::ProcessInfo> GetHighMemoryCandidates(uintmax_t minRamBytes = 200ULL * 1024 * 1024) {
        return GTLIBC::GTLibc::GetHighMemoryCandidateProcesses(minRamBytes);
    }

    // Query aggregated process groups sorted by total RAM usage (e.g. Chrome 15 processes = 1.2 GB)
    static std::vector<GTLIBC::AggregatedProcessGroup> GetAggregatedProcessGroups(uintmax_t minGroupRamBytes = 5ULL * 1024 * 1024) {
        return GTLIBC::GTLibc::GetAggregatedProcessGroups(minGroupRamBytes);
    }

    // Safely process high-RAM background candidate processes ONLY with explicit user permission
    static size_t KillHighMemoryProcesses(uintmax_t minRamBytes = 200ULL * 1024 * 1024, bool enableTermination = true, bool userPermissionGranted = false) {
        Logger::Instance().Info("Scanning background processes using RAM > " + std::to_string(minRamBytes / (1024 * 1024)) + " MB via GTLibc Subsystem...");
        size_t count = GTLIBC::GTLibc::KillHighMemoryProcesses(minRamBytes, enableTermination, userPermissionGranted);
        if (count > 0) {
            Logger::Instance().Info("GTLibc Engine terminated " + std::to_string(count) + " candidate process(es) with permission.");
        } else {
            Logger::Instance().Info("GTLibc Engine: No high-RAM candidate processes terminated (Protected or pending user permission).");
        }
        return count;
    }

    // Safely stop background crash dump / error handles locking temporary directories
    static size_t StopLockingProcesses(bool enableTermination = true) {
        if (!enableTermination) return 0;
        size_t count = 0;
        // Only target crash reporting services by default, preserving user shells (bash, sh, mintty, powershell)
        static const std::vector<std::string> targetProcesses = {
            "werfault.exe", "werfaultsecure.exe"
        };
        for (const auto& proc : targetProcesses) {
            count += GTLIBC::GTLibc::KillProcessByName(proc, true);
        }
        if (count > 0) {
            Logger::Instance().Info("GTLibc Engine successfully released " + std::to_string(count) + " process lock handle(s).");
        } else {
            Logger::Instance().Info("GTLibc Engine: No background process lock handles detected.");
        }
        return count;
    }
};
