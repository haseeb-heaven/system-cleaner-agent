#pragma once

#include "Cleaner.hpp"
#include "Logger.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#if defined(__APPLE__)
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif
#endif

namespace fs = std::filesystem;

struct ScheduleRule {
    fs::path targetFolder;
    long long intervalSeconds    = 900;  // Default 15 mins
    double memThresholdPercent   = 0.0;  // 0 = disabled
    double diskThresholdPercent  = 0.0;  // 0 = disabled
    uintmax_t diskFreeBelowBytes = 0;    // 0 = disabled  (e.g. 500 MB free remaining)
    fs::path monitorDrive;               // Drive to monitor for free space (e.g. C:\)
};

class SmartScheduler {
public:
    static uintmax_t GetTotalMemoryBytes() {
#ifdef _WIN32
        MEMORYSTATUSEX statex;
        statex.dwLength = sizeof(statex);
        if (GlobalMemoryStatusEx(&statex)) {
            return statex.ullTotalPhys;
        }
#endif
        return 16ULL * 1024 * 1024 * 1024;
    }

    // ----------------------------------------------------------------
    // RAM usage %
    // ----------------------------------------------------------------
    static double GetMemoryUsagePercent() {
#ifdef _WIN32
        MEMORYSTATUSEX statex;
        statex.dwLength = sizeof(statex);
        if (GlobalMemoryStatusEx(&statex)) {
            return static_cast<double>(statex.dwMemoryLoad);
        }
#elif defined(__APPLE__)
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        vm_statistics64_data_t vmstat;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmstat, &count) == KERN_SUCCESS) {
            int64_t total_mem = 0;
            size_t len = sizeof(total_mem);
            sysctlbyname("hw.memsize", &total_mem, &len, NULL, 0);
            long long page_size = getpagesize();
            long long used_mem = (vmstat.active_count + vmstat.wire_count) * page_size;
            if (total_mem > 0)
                return (static_cast<double>(used_mem) / static_cast<double>(total_mem)) * 100.0;
        }
#else
        long pages       = sysconf(_SC_PHYS_PAGES);
        long avail_pages = sysconf(_SC_AVPHYS_PAGES);
        long page_size   = sysconf(_SC_PAGE_SIZE);
        if (pages > 0 && page_size > 0) {
            long total     = pages * page_size;
            long available = avail_pages * page_size;
            return (static_cast<double>(total - available) / static_cast<double>(total)) * 100.0;
        }
#endif
        return 0.0;
    }

    static fs::path NormalizeDrivePath(const fs::path& p) {
        std::string s = p.string();
        while (!s.empty() && (s.front() == '"' || s.front() == '\'')) s.erase(0, 1);
        while (!s.empty() && (s.back() == '"' || s.back() == '\'')) s.pop_back();
        if (s.size() == 2 && s[1] == ':') {
            s += "\\";
        }
        return fs::path(s.empty() ? "C:\\" : s);
    }

    // ----------------------------------------------------------------
    // Disk used %
    // ----------------------------------------------------------------
    static double GetDiskUsagePercent(const fs::path& path = "C:\\") {
        try {
            std::error_code ec;
            fs::path norm = NormalizeDrivePath(path);
            fs::space_info si = fs::space(norm, ec);
            if (!ec && si.capacity > 0) {
                uintmax_t used = si.capacity - si.available;
                return (static_cast<double>(used) / static_cast<double>(si.capacity)) * 100.0;
            }
        } catch (...) {}
        return 0.0;
    }

    // ----------------------------------------------------------------
    // Disk free bytes on a given drive/path
    // ----------------------------------------------------------------
    static uintmax_t GetDiskFreeBytes(const fs::path& path = "C:\\") {
        try {
            std::error_code ec;
            fs::path norm = NormalizeDrivePath(path);
            fs::space_info si = fs::space(norm, ec);
            if (!ec) return si.available;
        } catch (...) {}
        return 0;
    }

    struct DriveStat {
        std::string driveName;
        double usedPercent = 0.0;
        uintmax_t freeBytes = 0;
        uintmax_t capacityBytes = 0;
    };

    static std::vector<DriveStat> GetAllDriveStats() {
        std::vector<DriveStat> stats;
#ifdef _WIN32
        DWORD drives = GetLogicalDrives();
        for (char letter = 'A'; letter <= 'Z'; ++letter) {
            if (drives & (1 << (letter - 'A'))) {
                std::string drivePath = std::string(1, letter) + ":\\";
                UINT driveType = GetDriveTypeA(drivePath.c_str());
                if (driveType == DRIVE_FIXED || driveType == DRIVE_REMOVABLE) {
                    std::error_code ec;
                    fs::space_info si = fs::space(drivePath, ec);
                    if (!ec && si.capacity > 0) {
                        DriveStat ds;
                        ds.driveName = drivePath;
                        ds.capacityBytes = si.capacity;
                        ds.freeBytes = si.available;
                        uintmax_t used = si.capacity - si.available;
                        ds.usedPercent = (static_cast<double>(used) / static_cast<double>(si.capacity)) * 100.0;
                        stats.push_back(ds);
                    }
                }
            }
        }
#else
        std::error_code ec;
        fs::space_info si = fs::space("/", ec);
        if (!ec && si.capacity > 0) {
            DriveStat ds;
            ds.driveName = "/";
            ds.capacityBytes = si.capacity;
            ds.freeBytes = si.available;
            uintmax_t used = si.capacity - si.available;
            ds.usedPercent = (static_cast<double>(used) / static_cast<double>(si.capacity)) * 100.0;
            stats.push_back(ds);
        }
#endif
        return stats;
    }

    // ----------------------------------------------------------------
    // Parse rule: "C:/Temp:15m:disk-free<500mb:C:\"
    //   field 0 = target folder to clean
    //   field 1 = check interval
    //   field 2 = trigger condition  (mem>80%  |  disk>90%  |  disk-free<500mb)
    //   field 3 = (optional) drive to monitor for free space
    // ----------------------------------------------------------------
    static ScheduleRule ParseRuleString(const std::string& ruleStr) {
        ScheduleRule rule;
        std::stringstream ss(ruleStr);
        std::string part;
        int idx = 0;
        while (std::getline(ss, part, ':')) {
            if (idx == 0) {
                if (part.length() == 1 && std::isalpha(static_cast<unsigned char>(part[0]))) {
                    std::string rest;
                    if (std::getline(ss, rest, ':')) {
                        part = part + ":" + rest;
                    }
                }
                rule.targetFolder = fs::path(part);
            } else if (idx == 1) {
                rule.intervalSeconds = ContentInspector::ParseDurationToMinutes(part) * 60;
                if (rule.intervalSeconds <= 0) {
                    try { rule.intervalSeconds = std::stoll(part); } catch (...) { rule.intervalSeconds = 900; }
                }
            } else if (idx == 2) {
                std::string lp = part;
                std::transform(lp.begin(), lp.end(), lp.begin(), [](unsigned char c){ return std::tolower(c); });

                if (lp.find("mem>") != std::string::npos || lp.find("ram>") != std::string::npos) {
                    // mem>80%
                    size_t pos = lp.find('>');
                    std::string val = lp.substr(pos + 1);
                    if (!val.empty() && val.back() == '%') val.pop_back();
                    try { rule.memThresholdPercent = std::stod(val); } catch (...) {}

                } else if (lp.find("disk-free<") != std::string::npos || lp.find("free<") != std::string::npos) {
                    // disk-free<500mb  or  free<500mb
                    size_t pos = lp.find('<');
                    std::string val = lp.substr(pos + 1);
                    rule.diskFreeBelowBytes = ContentInspector::ParseSizeToBytes(val);

                } else if (lp.find("disk>") != std::string::npos) {
                    // disk>90%
                    size_t pos = lp.find('>');
                    std::string val = lp.substr(pos + 1);
                    if (!val.empty() && val.back() == '%') val.pop_back();
                    try { rule.diskThresholdPercent = std::stod(val); } catch (...) {}
                }
            } else if (idx == 3) {
                // Optional: drive to monitor  (e.g. "C:\")
                rule.monitorDrive = fs::path(part);
            }
            idx++;
        }
        return rule;
    }

    // ----------------------------------------------------------------
    // Pretty-print a live status line for the daemon
    // ----------------------------------------------------------------
    static void PrintDaemonStatus(double memPct, uintmax_t diskFreeBytes,
                                  double diskUsedPct, const fs::path& drive) {
        std::cout << "\033[1;36m[MONITOR] "
                  << "RAM: "   << static_cast<int>(memPct)    << "%  |  "
                  << "Disk("   << drive.string()              << ") used: "
                  << static_cast<int>(diskUsedPct)            << "%  |  free: "
                  << Cleaner::FormatSize(diskFreeBytes)
                  << "\033[0m\n";
    }

    // ----------------------------------------------------------------
    // Background Daemon — watches RAM %, disk % and free-bytes
    // ----------------------------------------------------------------
    static void RunDaemonService(Cleaner& cleaner,
                                 const std::vector<ScheduleRule>& rules,
                                 double defaultMemThreshold   = 0.0,
                                 double defaultDiskThreshold  = 0.0,
                                 long long checkIntervalSecs  = 10,
                                 bool dryRun                  = false,
                                 uintmax_t diskFreeBelowBytes = 0,
                                 const fs::path& monitorDrive = "C:\\") {
        cleaner.SetDryRun(dryRun);

        Logger::Instance().Info(
            "Smart Daemon active | RAM threshold: " + std::to_string(defaultMemThreshold) +
            "% | Disk threshold: " + std::to_string(defaultDiskThreshold) +
            "% | Free-below: " + Cleaner::FormatSize(diskFreeBelowBytes) +
            " | Drive: " + monitorDrive.string() +
            " | Check interval: " + std::to_string(checkIntervalSecs) + "s");

        std::cout << "\033[1;36m"
                  << "+--[ Smart Daemon Service Active ]----------------------------------+\n"
                  << "|  Monitoring: RAM / Disk Thresholds + Free Space Triggers          |\n"
                  << "|  Drive: " << monitorDrive.string()
                  << "  |  Check interval: " << checkIntervalSecs << "s"
                  << "\n+-------------------------------------------------------------------+\n"
                  << "\033[0m\n";

        while (true) {
            double currentMem     = GetMemoryUsagePercent();
            double currentDiskPct = GetDiskUsagePercent(monitorDrive);
            uintmax_t currentFree = GetDiskFreeBytes(monitorDrive);

            PrintDaemonStatus(currentMem, currentFree, currentDiskPct, monitorDrive);

            bool triggerCleanup   = false;
            std::string reason;

            // --- Global RAM threshold ---
            if (defaultMemThreshold > 0.0 && currentMem >= defaultMemThreshold) {
                triggerCleanup = true;
                reason = "RAM " + std::to_string(static_cast<int>(currentMem)) +
                         "% >= threshold " + std::to_string(static_cast<int>(defaultMemThreshold)) + "%";
            }

            // --- Global disk used % threshold ---
            if (!triggerCleanup && defaultDiskThreshold > 0.0 && currentDiskPct >= defaultDiskThreshold) {
                triggerCleanup = true;
                reason = "Disk used " + std::to_string(static_cast<int>(currentDiskPct)) +
                         "% >= threshold " + std::to_string(static_cast<int>(defaultDiskThreshold)) + "%";
            }

            // --- Global disk FREE bytes trigger ---
            if (!triggerCleanup && diskFreeBelowBytes > 0 && currentFree <= diskFreeBelowBytes) {
                triggerCleanup = true;
                reason = "Disk free " + Cleaner::FormatSize(currentFree) +
                         " <= " + Cleaner::FormatSize(diskFreeBelowBytes) + " trigger threshold";
            }

            // --- Per-rule triggers ---
            for (const auto& rule : rules) {
                bool ruleTriggered = false;

                fs::path watchDrive = rule.monitorDrive.empty() ? monitorDrive : rule.monitorDrive;
                uintmax_t ruleFree  = GetDiskFreeBytes(watchDrive);
                double ruleDiskPct  = GetDiskUsagePercent(watchDrive);

                if (rule.memThresholdPercent > 0.0 && currentMem >= rule.memThresholdPercent) {
                    ruleTriggered = true;
                    reason = "[Rule:" + rule.targetFolder.string() + "] RAM " +
                             std::to_string(static_cast<int>(currentMem)) + "% >= " +
                             std::to_string(static_cast<int>(rule.memThresholdPercent)) + "%";
                } else if (rule.diskThresholdPercent > 0.0 && ruleDiskPct >= rule.diskThresholdPercent) {
                    ruleTriggered = true;
                    reason = "[Rule:" + rule.targetFolder.string() + "] Disk used " +
                             std::to_string(static_cast<int>(ruleDiskPct)) + "% >= " +
                             std::to_string(static_cast<int>(rule.diskThresholdPercent)) + "%";
                } else if (rule.diskFreeBelowBytes > 0 && ruleFree <= rule.diskFreeBelowBytes) {
                    ruleTriggered = true;
                    reason = "[Rule:" + rule.targetFolder.string() + "] Disk free " +
                             Cleaner::FormatSize(ruleFree) + " <= " +
                             Cleaner::FormatSize(rule.diskFreeBelowBytes);
                }

                if (ruleTriggered) {
                    std::cout << "\033[1;33m[TRIGGER] " << reason << " -> Cleaning: "
                              << rule.targetFolder.string() << "\033[0m\n";
                    Logger::Instance().Warn("THRESHOLD TRIGGERED: " + reason);
                    cleaner.SetCustomPaths({rule.targetFolder});
                    cleaner.Clean();
                    cleaner.SetCustomPaths({});
                }
            }

            if (triggerCleanup && rules.empty()) {
                std::cout << "\033[1;33m[TRIGGER] " << reason << " -> Executing global cleanup!\033[0m\n";
                Logger::Instance().Warn("THRESHOLD TRIGGERED: " + reason);
                cleaner.Clean();
            }

            std::this_thread::sleep_for(std::chrono::seconds(checkIntervalSecs));
        }
    }
};
