/*
 * GTLibc (Windows Process & Memory Management Subsystem for system-cleaner-agent)
 * Ported & Enhanced from haseeb-heaven/GTLibCpp
 * License: MIT
 */

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <thread>

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
#include <shellapi.h>
#else
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#endif

namespace fs = std::filesystem;

namespace GTLIBC {

struct ProcessInfo {
    DWORD pid = 0;
    std::string processName;
    HANDLE handle = nullptr;
    HWND hwnd = nullptr;
    uintptr_t baseAddress = 0;
    size_t memoryUsageBytes = 0;
};

class GTLibc {
private:
    std::string targetProcessName;
    DWORD targetPid = 0;
    HANDLE targetHandle = nullptr;
    HWND targetHwnd = nullptr;
    uintptr_t targetBaseAddress = 0;
    bool enableLogs = false;
    std::string lastErrorString;

    void AddLog(const std::string& method, const std::string& message);

public:
    GTLibc();
    GTLibc(const std::string& processName, bool enableLogs = false);
    ~GTLibc();

    // Process & Window Discovery
    HANDLE FindProcess(const std::string& processName);
    HWND FindWindowByName(const std::string& windowName);
    DWORD GetProcessIDFromHWND(HWND hwnd);
    HANDLE GetHandleFromHWND(HWND hwnd);
    uintptr_t GetModuleBaseAddress(DWORD pid, const std::string& moduleName);
    uintptr_t GetProcessBaseAddress();

    // Process Status & Enumeration
    static bool IsProcessRunning(const std::string& processName);
    static std::vector<ProcessInfo> EnumerateAllProcesses();
    static size_t GetProcessMemoryUsage(DWORD pid);
    static bool IsElevatedProcess();

    // Process Termination & Control
    static bool KillProcess(DWORD pid);
    static size_t KillProcessByName(const std::string& processName);
    static size_t KillHighMemoryProcesses(size_t minRamBytes, bool enableTermination = true);

    // Memory Manipulation
    bool ReadMemoryBuffer(uintptr_t address, void* buffer, size_t size);
    bool WriteMemoryBuffer(uintptr_t address, const void* buffer, size_t size);

    template <typename T>
    T ReadMemory(uintptr_t address) {
        T value{};
        ReadMemoryBuffer(address, &value, sizeof(T));
        return value;
    }

    template <typename T>
    bool WriteMemory(uintptr_t address, const T& value) {
        return WriteMemoryBuffer(address, &value, sizeof(T));
    }

    std::string ReadString(uintptr_t address, size_t maxLen);
    bool WriteString(uintptr_t address, const std::string& str);

    // Utility & Execution
    static std::string ShellExec(const std::string& cmdArgs, bool runAsAdmin = false, bool waitForExit = true);
    static std::string GetLastErrorAsString();

    // Getters
    DWORD GetProcessId() const { return targetPid; }
    HANDLE GetProcessHandle() const { return targetHandle; }
    HWND GetProcessWindow() const { return targetHwnd; }
    uintptr_t GetBaseAddress() const { return targetBaseAddress; }
    std::string GetProcessName() const { return targetProcessName; }
};

} // namespace GTLIBC
