/*
 * GTLibc (Windows Process Management Subsystem for system-cleaner-agent)
 * Focused subset ported from haseeb-heaven/GTLibCpp for process management
 * License: MIT
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>
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
};

class GTLibc {
public:
    // Process Discovery & Enumeration
    static HANDLE FindProcess(const std::string& processName, DWORD& outPid);
    static bool IsProcessRunning(const std::string& processName);
    static std::vector<ProcessInfo> EnumerateAllProcesses();
    static size_t GetProcessMemoryUsage(DWORD pid);
    static bool IsElevatedProcess();

    // Process Termination & Resource Management
    static bool KillProcess(DWORD pid);
    static size_t KillProcessByName(const std::string& processName);
    static size_t KillHighMemoryProcesses(size_t minRamBytes, bool enableTermination = true);

    // Diagnostics & Utilities
    static std::string GetLastErrorAsString();
};

} // namespace GTLIBC
