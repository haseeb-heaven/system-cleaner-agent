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
    long long intervalSeconds = 900; // Default 15 mins
    double memThresholdPercent = 0.0; // 0 = disabled
    double diskThresholdPercent = 0.0; // 0 = disabled
};

class SmartScheduler {
public:
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
            if (total_mem > 0) {
                return (static_cast<double>(used_mem) / static_cast<double>(total_mem)) * 100.0;
            }
        }
#else
        long pages = sysconf(_SC_PHYS_PAGES);
        long avail_pages = sysconf(_SC_AVPHYS_PAGES);
        long page_size = sysconf(_SC_PAGE_SIZE);
        if (pages > 0 && page_size > 0) {
            long total = pages * page_size;
            long available = avail_pages * page_size;
            long used = total - available;
            return (static_cast<double>(used) / static_cast<double>(total)) * 100.0;
        }
#endif
        return 0.0;
    }

    static double GetDiskUsagePercent(const fs::path& path = "C:\\") {
        try {
            std::error_code ec;
            fs::space_info si = fs::space(path, ec);
            if (!ec && si.capacity > 0) {
                uintmax_t used = si.capacity - si.available;
                return (static_cast<double>(used) / static_cast<double>(si.capacity)) * 100.0;
            }
        } catch (...) {}
        return 0.0;
    }

    static ScheduleRule ParseRuleString(const std::string& ruleStr) {
        ScheduleRule rule;
        std::stringstream ss(ruleStr);
        std::string part;
        int idx = 0;
        while (std::getline(ss, part, ':')) {
            if (idx == 0) {
                rule.targetFolder = fs::path(part);
            } else if (idx == 1) {
                rule.intervalSeconds = ContentInspector::ParseDurationToMinutes(part) * 60;
                if (rule.intervalSeconds <= 0) {
                    try { rule.intervalSeconds = std::stoll(part); } catch (...) { rule.intervalSeconds = 900; }
                }
            } else if (idx == 2) {
                if (part.find("mem>") != std::string::npos || part.find("ram>") != std::string::npos) {
                    size_t pos = part.find('>');
                    std::string valStr = part.substr(pos + 1);
                    if (valStr.back() == '%') valStr.pop_back();
                    try { rule.memThresholdPercent = std::stod(valStr); } catch (...) {}
                } else if (part.find("disk>") != std::string::npos) {
                    size_t pos = part.find('>');
                    std::string valStr = part.substr(pos + 1);
                    if (valStr.back() == '%') valStr.pop_back();
                    try { rule.diskThresholdPercent = std::stod(valStr); } catch (...) {}
                }
            }
            idx++;
        }
        return rule;
    }

    static void RunDaemonService(Cleaner& cleaner, 
                                 const std::vector<ScheduleRule>& rules, 
                                 double defaultMemThreshold = 0.0, 
                                 double defaultDiskThreshold = 0.0, 
                                 long long checkIntervalSeconds = 10,
                                 bool dryRun = false) {
        cleaner.SetDryRun(dryRun);
        Logger::Instance().Info("Starting Smart Daemon Service (Memory Threshold: " + std::to_string(defaultMemThreshold) + "%, Check Interval: " + std::to_string(checkIntervalSeconds) + "s)...");

        std::cout << "\033[1;36m[Smart Daemon Service Active - Monitoring System RAM & Disk Thresholds]\033[0m\n";

        while (true) {
            double currentMem = GetMemoryUsagePercent();
            double currentDisk = GetDiskUsagePercent();

            bool triggerCleanup = false;
            std::string triggerReason = "";

            if (defaultMemThreshold > 0.0 && currentMem >= defaultMemThreshold) {
                triggerCleanup = true;
                triggerReason = "RAM Memory Usage (" + Cleaner::FormatSize(0) + " / " + std::to_string(currentMem) + "%) exceeded threshold (" + std::to_string(defaultMemThreshold) + "%)";
            } else if (defaultDiskThreshold > 0.0 && currentDisk >= defaultDiskThreshold) {
                triggerCleanup = true;
                triggerReason = "Disk Storage Usage (" + std::to_string(currentDisk) + "%) exceeded threshold (" + std::to_string(defaultDiskThreshold) + "%)";
            }

            for (const auto& rule : rules) {
                if (rule.memThresholdPercent > 0.0 && currentMem >= rule.memThresholdPercent) {
                    triggerCleanup = true;
                    triggerReason = "Rule Target [" + rule.targetFolder.string() + "] Memory Triggered (" + std::to_string(currentMem) + "% >= " + std::to_string(rule.memThresholdPercent) + "%)";
                    cleaner.SetCustomPaths({rule.targetFolder});
                    break;
                }
            }

            if (triggerCleanup) {
                Logger::Instance().Warn("SMART THRESHOLD TRIGGERED: " + triggerReason);
                std::cout << "\033[1;33m[THRESHOLD TRIGGERED] " << triggerReason << " -> Executing Automated Cleanup!\033[0m\n";
                cleaner.Clean();
            }

            std::this_thread::sleep_for(std::chrono::seconds(checkIntervalSeconds));
        }
    }
};
