#pragma once
#include "Logger.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#else
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#endif

namespace fs = std::filesystem;

class ProcessManager {
public:
    // Safely stop background processes locking temporary/cache directories
    static size_t StopLockingProcesses(bool enableTermination = true) {
        if (!enableTermination) return 0;

        size_t count = 0;
        static const std::vector<std::string> targetProcesses = {
            "mintty.exe", "cat.exe", "bash.exe", "sh.exe", "werfault.exe",
            "mintty", "cat", "bash", "sh"
        };

#ifdef _WIN32
        Logger::Instance().Info("Scanning system process handles for temporary directory locks...");
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe32;
            pe32.dwSize = sizeof(PROCESSENTRY32W);
            if (Process32FirstW(hSnap, &pe32)) {
                do {
                    std::wstring exeW(pe32.szExeFile);
                    std::string exeName;
                    for (wchar_t c : exeW) {
                        exeName += (c < 128) ? static_cast<char>(c) : '?';
                    }
                    std::string exeLower = exeName;
                    std::transform(exeLower.begin(), exeLower.end(), exeLower.begin(), ::tolower);

                    for (const auto& proc : targetProcesses) {
                        std::string pLower = proc;
                        std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
                        if (exeLower == pLower) {
                            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                            if (hProc) {
                                if (TerminateProcess(hProc, 0)) {
                                    count++;
                                    Logger::Instance().Info("Terminated background process lock holder: " + exeName + " (PID: " + std::to_string(pe32.th32ProcessID) + ")");
                                }
                                CloseHandle(hProc);
                            }
                        }
                    }
                } while (Process32NextW(hSnap, &pe32));
            }
            CloseHandle(hSnap);
        }
#else
        // POSIX Process Scanner for Linux / macOS
        DIR* procDir = opendir("/proc");
        if (procDir) {
            struct dirent* entry;
            while ((entry = readdir(procDir)) != nullptr) {
                if (entry->d_type == DT_DIR) {
                    std::string pidStr = entry->d_name;
                    if (std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) {
                        pid_t pid = std::stoi(pidStr);
                        fs::path cmdlinePath = fs::path("/proc") / pidStr / "cmdline";
                        std::ifstream cmdFile(cmdlinePath);
                        if (cmdFile.is_open()) {
                            std::string cmd;
                            std::getline(cmdFile, cmd, '\0');
                            std::string cmdLower = cmd;
                            std::transform(cmdLower.begin(), cmdLower.end(), cmdLower.begin(), ::tolower);

                            for (const auto& proc : targetProcesses) {
                                if (cmdLower.find(proc) != std::string::npos) {
                                    if (kill(pid, SIGTERM) == 0) {
                                        count++;
                                        Logger::Instance().Info("Terminated POSIX background process lock holder: " + cmd + " (PID: " + std::to_string(pid) + ")");
                                    }
                                }
                            }
                        }
                    }
                }
            }
            closedir(procDir);
        }
#endif
        if (count > 0) {
            Logger::Instance().Info("Successfully released " + std::to_string(count) + " process lock(s).");
        } else {
            Logger::Instance().Info("No background process lock handles detected.");
        }
        return count;
    }
};
