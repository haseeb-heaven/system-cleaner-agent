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
#include <cinttypes>   // for SCNu64 (CPU usage parsing on POSIX)
#include <cstdio>      // for std::fopen (CPU usage on POSIX)

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

    // ----------------------------------------------------------------
    // CPU usage % (sampled over a short interval for accuracy)
    // ----------------------------------------------------------------
    static double GetCpuUsagePercent() {
        // Static cache of previous measurement for delta calculation
        static uint64_t s_lastIdle = 0;
        static uint64_t s_lastTotal = 0;
        static double s_lastResult = 0.0;
        static std::chrono::steady_clock::time_point s_lastSample;
        static bool s_initialized = false;
        auto now = std::chrono::steady_clock::now();
        // On first call, do an immediate double-sample (sleep 100ms) to get a real value.
        // Subsequent calls cache for 500ms minimum for accurate delta calculation.
        if (!s_initialized) {
            // First call: take two samples 100ms apart for an immediate real value
            uint64_t idle1, total1, idle2, total2;
#ifdef _WIN32
            FILETIME idleTime, kernelTime, userTime;
            auto filetimeToUint64 = [](const FILETIME& ft) -> uint64_t {
                return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
            };
            if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return 0.0;
            idle1 = filetimeToUint64(idleTime);
            total1 = filetimeToUint64(kernelTime) + filetimeToUint64(userTime);
#else
            FILE* f1 = std::fopen("/proc/stat", "r");
            if (!f1) return 0.0;
            char buf[1024];
            uint64_t u=0,n=0,s=0,i=0,io=0,ir=0,si=0,st=0;
            if (std::fgets(buf, sizeof(buf), f1))
                std::sscanf(buf, "cpu %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                              " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
                      &u, &n, &s, &i, &io, &ir, &si, &st);
            std::fclose(f1);
            idle1 = i; total1 = u+n+s+i+io+ir+si+st;
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
#ifdef _WIN32
            if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return 0.0;
            idle2 = filetimeToUint64(idleTime);
            total2 = filetimeToUint64(kernelTime) + filetimeToUint64(userTime);
#else
            FILE* f2 = std::fopen("/proc/stat", "r");
            if (!f2) return 0.0;
            if (std::fgets(buf, sizeof(buf), f2))
                std::sscanf(buf, "cpu %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                              " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
                      &u, &n, &s, &i, &io, &ir, &si, &st);
            std::fclose(f2);
            idle2 = i; total2 = u+n+s+i+io+ir+si+st;
#endif
            s_lastIdle = idle1;
            s_lastTotal = total1;
            s_lastSample = now;
            s_initialized = true;
            if (total2 > total1) {
                uint64_t totalDelta = total2 - total1;
                uint64_t idleDelta = (idle2 >= idle1) ? (idle2 - idle1) : 0;
                if (totalDelta > 0) {
                    s_lastResult = 100.0 * (1.0 - (static_cast<double>(idleDelta) / static_cast<double>(totalDelta)));
                    if (s_lastResult < 0.0) s_lastResult = 0.0;
                    if (s_lastResult > 100.0) s_lastResult = 100.0;
                }
            }
            return s_lastResult;
        }
        // Re-sample at most every 500ms to get a meaningful delta
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                now - s_lastSample).count() < 500 && s_lastTotal > 0) {
            return s_lastResult;
        }
#ifdef _WIN32
        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            return s_lastResult;
        }
        auto filetimeToUint64 = [](const FILETIME& ft) -> uint64_t {
            return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        uint64_t idle = filetimeToUint64(idleTime);
        uint64_t kernel = filetimeToUint64(kernelTime);
        uint64_t user = filetimeToUint64(userTime);
        // Total = kernel + user (idle is already excluded from kernel on Windows)
        uint64_t total = kernel + user;
#else
        // POSIX: read /proc/stat for cpu line
        FILE* f = std::fopen("/proc/stat", "r");
        if (!f) return s_lastResult;
        char buf[1024];
        uint64_t user = 0, nice = 0, system = 0, idle = 0;
        uint64_t iowait = 0, irq = 0, softirq = 0, steal = 0;
        if (std::fgets(buf, sizeof(buf), f)) {
            std::sscanf(buf, "cpu %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                          " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
                  &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
        }
        std::fclose(f);
        uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
#endif
        double pct = 0.0;
        if (s_lastTotal > 0 && total > s_lastTotal) {
            uint64_t totalDelta = total - s_lastTotal;
            uint64_t idleDelta = (idle >= s_lastIdle) ? (idle - s_lastIdle) : 0;
            if (totalDelta > 0) {
                pct = 100.0 * (1.0 - (static_cast<double>(idleDelta) / static_cast<double>(totalDelta)));
                if (pct < 0.0) pct = 0.0;
                if (pct > 100.0) pct = 100.0;
            }
        }
        s_lastIdle = idle;
        s_lastTotal = total;
        s_lastSample = now;
        s_lastResult = pct;
        return pct;
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

    struct MultiDriveScanResult {
        std::string driveName;
        uintmax_t totalBytes = 0;
        uintmax_t usedBytes = 0;
        uintmax_t freeBytes = 0;
        double usedPercent = 0.0;
        bool isWarning = false;
        std::string usageBar;
    };

    static std::string RenderDufUsageBar(double usedPercent, size_t barWidth = 20) {
        size_t filled = static_cast<size_t>((usedPercent / 100.0) * barWidth);
        if (filled > barWidth) filled = barWidth;
        if (filled == 0 && usedPercent > 0.0) filled = 1;

        std::string bar = "[";
        for (size_t i = 0; i < filled; ++i) bar += "█";
        for (size_t i = filled; i < barWidth; ++i) bar += ".";
        bar += "]";

        std::stringstream ss;
        ss << bar << " " << std::fixed << std::setprecision(1) << std::setw(5) << usedPercent << "%";
        return ss.str();
    }

    static std::vector<MultiDriveScanResult> ScanAllDrivesSimultaneously(double warningThreshold = 90.0) {
        std::vector<std::string> drivePaths;
#ifdef _WIN32
        DWORD drives = GetLogicalDrives();
        for (char letter = 'A'; letter <= 'Z'; ++letter) {
            if (drives & (1 << (letter - 'A'))) {
                std::string dPath = std::string(1, letter) + ":\\";
                UINT dType = GetDriveTypeA(dPath.c_str());
                if (dType == DRIVE_FIXED || dType == DRIVE_REMOVABLE) {
                    drivePaths.push_back(dPath);
                }
            }
        }
#else
        drivePaths.push_back("/");
        std::vector<std::string> mountRoots = {"/Volumes", "/mnt", "/media"};
        for (const auto& mr : mountRoots) {
            std::error_code ec;
            if (fs::exists(mr, ec)) {
                for (const auto& entry : fs::directory_iterator(mr, ec)) {
                    if (entry.is_directory(ec)) drivePaths.push_back(entry.path().string());
                }
            }
        }
#endif
        if (drivePaths.empty()) drivePaths.push_back("C:\\");

        std::vector<MultiDriveScanResult> results(drivePaths.size());
        std::vector<std::thread> threads;

        for (size_t i = 0; i < drivePaths.size(); ++i) {
            threads.push_back(std::thread([i, &drivePaths, &results, warningThreshold]() {
                MultiDriveScanResult res;
                res.driveName = drivePaths[i];
                std::error_code ec;
                fs::space_info si = fs::space(drivePaths[i], ec);
                if (!ec && si.capacity > 0) {
                    res.totalBytes = si.capacity;
                    res.freeBytes = si.available;
                    res.usedBytes = si.capacity - si.available;
                    res.usedPercent = (static_cast<double>(res.usedBytes) / static_cast<double>(si.capacity)) * 100.0;
                }
                res.isWarning = (res.usedPercent >= warningThreshold);
                res.usageBar = RenderDufUsageBar(res.usedPercent);
                results[i] = res;
            }));
        }

        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }

        return results;
    }

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
        Logger::Instance().Info("[MONITOR] RAM: " + std::to_string(static_cast<int>(memPct)) + "% | Disk(" + drive.string() + ") used: " + std::to_string(static_cast<int>(diskUsedPct)) + "% | free: " + Cleaner::FormatSize(diskFreeBytes));
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
                    Logger::Instance().Warn("THRESHOLD TRIGGERED: " + reason + " -> Cleaning: " + rule.targetFolder.string());
                    cleaner.SetCustomPaths({rule.targetFolder});
                    cleaner.Clean();
                    cleaner.SetCustomPaths({});
                }
            }

            if (triggerCleanup && rules.empty()) {
                Logger::Instance().Warn("THRESHOLD TRIGGERED: " + reason + " -> Executing global cleanup!");
                cleaner.Clean();
            }

            std::this_thread::sleep_for(std::chrono::seconds(checkIntervalSecs));
        }
    }
};
