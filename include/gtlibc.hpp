/*
 * GTLibc (Windows Process Management Subsystem for system-cleaner-agent)
 * Focused subset ported from haseeb-heaven/GTLibCpp for process management
 * License: MIT
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#else
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#endif

namespace GTLIBC {

struct ProcessInfo {
    DWORD pid = 0;
    std::string processName;
    size_t memoryUsageBytes = 0;
    bool isProtected = false;
};

struct AggregatedProcessGroup {
    std::string processName;
    size_t totalMemoryUsageBytes = 0;
    size_t instanceCount = 0;
    std::vector<DWORD> pids;
    bool isProtected = false;
};

class GTLibc {
public:
    // Process Discovery & Enumeration
    static HANDLE FindProcess(const std::string& processName, DWORD& outPid);
    static bool IsProcessRunning(const std::string& processName);
    static std::vector<ProcessInfo> EnumerateAllProcesses();
    static bool IsProtectedProcess(const std::string& processName);
    static void AddCustomProtectedProcess(const std::string& processName);
    static std::vector<ProcessInfo> GetHighMemoryCandidateProcesses(size_t minRamBytes);
    static std::vector<AggregatedProcessGroup> GetAggregatedProcessGroups(size_t minGroupRamBytes = 5ULL * 1024 * 1024);
    static size_t GetProcessMemoryUsage(DWORD pid);
    static bool IsElevatedProcess();

    // Process Termination & Resource Management
    static bool KillProcess(DWORD pid);
    static size_t KillProcessByName(const std::string& processName, bool enablePermission = false, bool userPermissionGranted = false);
    static size_t KillHighMemoryProcesses(size_t minRamBytes, bool enableTermination = true, bool userPermissionGranted = false);

    // Diagnostics & Utilities
    static std::string GetLastErrorAsString();
};

} // namespace GTLIBC
