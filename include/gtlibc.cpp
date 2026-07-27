/*
 * GTLibc (Windows Process Management Subsystem for system-cleaner-agent)
 * Focused subset ported from haseeb-heaven/GTLibCpp for process management
 * License: MIT
 */

#include "gtlibc.hpp"
#include "Logger.hpp"
#include <map>

namespace GTLIBC {

std::string GTLibc::GetLastErrorAsString() {
#ifdef _WIN32
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0) return std::string();

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);
    return message;
#else
    return "Error code: " + std::to_string(errno);
#endif
}

HANDLE GTLibc::FindProcess(const std::string& processName, DWORD& outPid) {
    outPid = 0;
    std::string exeName = processName;
    if (exeName.length() < 4 || exeName.substr(exeName.length() - 4) != ".exe") {
        exeName += ".exe";
    }

#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return nullptr;

    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(hSnap, &pe32)) {
        do {
            std::wstring wExe(pe32.szExeFile);
            std::string curName;
            for (wchar_t c : wExe) curName += (c < 128) ? static_cast<char>(c) : '?';

            std::string curLower = curName;
            std::string targetLower = exeName;
            std::transform(curLower.begin(), curLower.end(), curLower.begin(), ::tolower);
            std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);

            if (curLower == targetLower) {
                outPid = pe32.th32ProcessID;
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, FALSE, outPid);
                CloseHandle(hSnap);
                return hProc;
            }
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
#endif
    return nullptr;
}

bool GTLibc::IsProcessRunning(const std::string& processName) {
    DWORD pid = 0;
    HANDLE hProc = FindProcess(processName, pid);
    if (hProc) {
        CloseHandle(hProc);
        return true;
    }
    return pid > 0;
}

std::vector<ProcessInfo> GTLibc::EnumerateAllProcesses() {
    std::vector<ProcessInfo> list;
#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnap, &pe32)) {
            do {
                ProcessInfo info;
                info.pid = pe32.th32ProcessID;
                std::wstring wExe(pe32.szExeFile);
                for (wchar_t c : wExe) info.processName += (c < 128) ? static_cast<char>(c) : '?';
                info.memoryUsageBytes = GetProcessMemoryUsage(info.pid);
                list.push_back(info);
            } while (Process32NextW(hSnap, &pe32));
        }
        CloseHandle(hSnap);
    }
#endif
    return list;
}

size_t GTLibc::GetProcessMemoryUsage(DWORD pid) {
#ifdef _WIN32
    // Use PROCESS_QUERY_LIMITED_INFORMATION so memory queries succeed for elevated/user processes (Chrome, Edge, IDEs)
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProc) {
        hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    }
    if (hProc) {
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(hProc, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            CloseHandle(hProc);
            return pmc.WorkingSetSize;
        }
        CloseHandle(hProc);
    }
#endif
    return 0;
}

bool GTLibc::IsElevatedProcess() {
#ifdef _WIN32
    BOOL fRet = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) {
            fRet = Elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return fRet != FALSE;
#else
    return geteuid() == 0;
#endif
}

bool GTLibc::KillProcess(DWORD pid) {
#ifdef _WIN32
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProc) {
        BOOL res = TerminateProcess(hProc, 0);
        CloseHandle(hProc);
        return res != FALSE;
    }
#else
    return kill(pid, SIGTERM) == 0;
#endif
    return false;
}

static std::vector<std::string> g_userProtectedProcesses;

bool GTLibc::IsProtectedProcess(const std::string& processName) {
    std::string procLower = processName;
    std::transform(procLower.begin(), procLower.end(), procLower.begin(), ::tolower);
    if (procLower.length() >= 4 && procLower.substr(procLower.length() - 4) == ".exe") {
        procLower = procLower.substr(0, procLower.length() - 4);
    }

    static const std::set<std::string> protectedSet = {
        // OS System Services & Shell
        "csrss", "lsass", "explorer", "svchost", "system", "smss", "services",
        "winlogon", "system-cleaner-agent", "agy", "antigravity", "gemini-cli",
        "cline", "grok", "conhost", "dwm", "taskhostw",
        "sihost", "ctfmon", "fontdrvhost", "runtimebroker", "searchhost",
        "searchindexer", "securityhealthservice", "spoolsv", "taskmgr",
        "audiodg", "shellexperiencehost", "startmenuexperiencehost", "lockapp",
        "textinputhost", "wipfw", "smartscreen", "registry",

        // User Browsers
        "chrome", "msedge", "brave", "firefox", "opera", "vivaldi", "iexplore", "arc",

        // IDEs & Code Editors
        "code", "cursor", "devenv", "idea64", "pycharm64", "clion64", "webstorm64",
        "rider64", "goland64", "notepad++", "sublime_text", "atom", "visualstudio",

        // Terminals & Shells
        "powershell", "cmd", "wt", "bash", "sh", "mintty", "windowsterminal",
        "git-bash", "zsh", "fish",

        // Productivity, Audio/Video & Communication Apps
        "discord", "slack", "telegram", "spotify", "steam", "teams", "outlook",
        "excel", "word", "powerpnt", "onedrive", "zoom", "notion", "obsidian"
    };

    if (protectedSet.count(procLower) > 0) return true;

    for (const auto& custom : g_userProtectedProcesses) {
        std::string cLower = custom;
        std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
        if (cLower.length() >= 4 && cLower.substr(cLower.length() - 4) == ".exe") {
            cLower = cLower.substr(0, cLower.length() - 4);
        }
        if (procLower == cLower) return true;
    }

    return false;
}

void GTLibc::AddCustomProtectedProcess(const std::string& processName) {
    if (!processName.empty()) {
        g_userProtectedProcesses.push_back(processName);
    }
}

std::vector<ProcessInfo> GTLibc::GetHighMemoryCandidateProcesses(size_t minRamBytes) {
    std::vector<ProcessInfo> candidates;
    auto procs = EnumerateAllProcesses();
    for (auto proc : procs) {
        proc.isProtected = IsProtectedProcess(proc.processName);
        if (proc.memoryUsageBytes >= minRamBytes) {
            candidates.push_back(proc);
        }
    }
    return candidates;
}

std::vector<AggregatedProcessGroup> GTLibc::GetAggregatedProcessGroups(size_t minGroupRamBytes) {
    std::map<std::string, AggregatedProcessGroup> groupMap;
    auto procs = EnumerateAllProcesses();

    for (const auto& proc : procs) {
        std::string lowerName = proc.processName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        auto& grp = groupMap[lowerName];
        if (grp.processName.empty()) {
            grp.processName = proc.processName;
            grp.isProtected = IsProtectedProcess(proc.processName);
        }
        grp.totalMemoryUsageBytes += proc.memoryUsageBytes;
        grp.instanceCount++;
        grp.pids.push_back(proc.pid);
    }

    std::vector<AggregatedProcessGroup> groups;
    for (const auto& kv : groupMap) {
        if (kv.second.totalMemoryUsageBytes >= minGroupRamBytes) {
            groups.push_back(kv.second);
        }
    }

    std::sort(groups.begin(), groups.end(), [](const AggregatedProcessGroup& a, const AggregatedProcessGroup& b) {
        return a.totalMemoryUsageBytes > b.totalMemoryUsageBytes;
    });

    return groups;
}

size_t GTLibc::KillProcessByName(const std::string& processName, bool enablePermission, bool userPermissionGranted) {
    if (!enablePermission) {
        Logger::Instance().Warn("RAM Cleaner: Killing process by name '" + processName + "' skipped (Permission disabled).");
        return 0;
    }
    if (IsProtectedProcess(processName) && !userPermissionGranted) {
        Logger::Instance().Info("RAM Cleaner: Cannot kill protected process without explicit user permission: " + processName);
        return 0;
    }

    size_t count = 0;
#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnap, &pe32)) {
            std::string targetLower = processName;
            if (targetLower.length() < 4 || targetLower.substr(targetLower.length() - 4) != ".exe") {
                targetLower += ".exe";
            }
            std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);

            do {
                std::wstring wExe(pe32.szExeFile);
                std::string curName;
                for (wchar_t c : wExe) curName += (c < 128) ? static_cast<char>(c) : '?';
                std::string curLower = curName;
                std::transform(curLower.begin(), curLower.end(), curLower.begin(), ::tolower);

                if (curLower == targetLower) {
                    if (KillProcess(pe32.th32ProcessID)) {
                        count++;
                    }
                }
            } while (Process32NextW(hSnap, &pe32));
        }
        CloseHandle(hSnap);
    }
#endif
    return count;
}

size_t GTLibc::KillHighMemoryProcesses(size_t minRamBytes, bool enableTermination, bool userPermissionGranted) {
    size_t count = 0;
    auto candidates = GetHighMemoryCandidateProcesses(minRamBytes);

    for (const auto& proc : candidates) {
        if (proc.isProtected) {
            Logger::Instance().Info("RAM Cleaner Guard: Preserved protected application: " + proc.processName + " (PID: " + std::to_string(proc.pid) + ", RAM: " + std::to_string(proc.memoryUsageBytes / (1024 * 1024)) + " MB)");
            continue;
        }

        if (!userPermissionGranted) {
            Logger::Instance().Warn("RAM Cleaner Guard: Candidate process skipped (Explicit user permission required): " + proc.processName + " (PID: " + std::to_string(proc.pid) + ", RAM: " + std::to_string(proc.memoryUsageBytes / (1024 * 1024)) + " MB)");
            continue;
        }

        if (enableTermination) {
            if (KillProcess(proc.pid)) {
                Logger::Instance().Info("RAM Cleaner: Terminated high-RAM candidate process with permission: " + proc.processName + " (PID: " + std::to_string(proc.pid) + ")");
                count++;
            }
        } else {
            count++;
        }
    }
    return count;
}

} // namespace GTLIBC
