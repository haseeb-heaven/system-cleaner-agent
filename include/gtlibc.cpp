/*
 * GTLibc (Windows Process Management Subsystem for system-cleaner-agent)
 * Focused subset ported from haseeb-heaven/GTLibCpp for process management
 * License: MIT
 */

#include "gtlibc.hpp"
#include "Logger.hpp"

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
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProc) {
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
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

size_t GTLibc::KillProcessByName(const std::string& processName) {
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

size_t GTLibc::KillHighMemoryProcesses(size_t minRamBytes, bool enableTermination) {
    size_t count = 0;
    static const std::vector<std::string> criticalSystemProcs = {
        "csrss.exe", "lsass.exe", "explorer.exe", "svchost.exe", "system",
        "smss.exe", "services.exe", "winlogon.exe", "system-cleaner-agent.exe",
        "conhost.exe", "dwm.exe", "taskhostw.exe"
    };

    auto procs = EnumerateAllProcesses();
    for (const auto& proc : procs) {
        std::string procLower = proc.processName;
        std::transform(procLower.begin(), procLower.end(), procLower.begin(), ::tolower);

        bool isProtected = false;
        for (const auto& prot : criticalSystemProcs) {
            if (procLower == prot) { isProtected = true; break; }
        }
        if (isProtected) continue;

        if (proc.memoryUsageBytes >= minRamBytes) {
            if (enableTermination) {
                if (KillProcess(proc.pid)) {
                    count++;
                }
            } else {
                count++;
            }
        }
    }
    return count;
}

} // namespace GTLIBC
