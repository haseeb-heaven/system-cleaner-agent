/*
 * GTLibc (Windows Process & Memory Management Subsystem for system-cleaner-agent)
 * Ported & Enhanced from haseeb-heaven/GTLibCpp
 * License: MIT
 */

#include "gtlibc.hpp"
#include "Logger.hpp"

namespace GTLIBC {

GTLibc::GTLibc() : GTLibc("", false) {}

GTLibc::GTLibc(const std::string& processName, bool enableLogs)
    : targetProcessName(processName), enableLogs(enableLogs) {
    if (!processName.empty()) {
        FindProcess(processName);
    }
}

GTLibc::~GTLibc() {
#ifdef _WIN32
    if (targetHandle != nullptr && targetHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(targetHandle);
        targetHandle = nullptr;
    }
#endif
}

void GTLibc::AddLog(const std::string& method, const std::string& message) {
    if (enableLogs) {
        Logger::Instance().Info("[GTLibc::" + method + "] " + message);
    }
}

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

HANDLE GTLibc::FindProcess(const std::string& processName) {
    AddLog("FindProcess", "Searching for process: " + processName);
    targetProcessName = processName;

    std::string exeName = processName;
    if (exeName.length() < 4 || exeName.substr(exeName.length() - 4) != ".exe") {
        exeName += ".exe";
    }

#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        AddLog("FindProcess", "CreateToolhelp32Snapshot failed");
        return nullptr;
    }

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
                targetPid = pe32.th32ProcessID;
                targetHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetPid);
                if (!targetHandle) {
                    targetHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, targetPid);
                }
                targetHwnd = FindWindowByName(processName);
                targetBaseAddress = GetModuleBaseAddress(targetPid, exeName);
                CloseHandle(hSnap);
                AddLog("FindProcess", "Found process PID: " + std::to_string(targetPid));
                return targetHandle;
            }
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
#endif
    AddLog("FindProcess", "Process not found: " + processName);
    return nullptr;
}

HWND GTLibc::FindWindowByName(const std::string& windowName) {
#ifdef _WIN32
    return FindWindowA(NULL, windowName.c_str());
#else
    return nullptr;
#endif
}

DWORD GTLibc::GetProcessIDFromHWND(HWND hwnd) {
#ifdef _WIN32
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid;
#else
    return 0;
#endif
}

HANDLE GTLibc::GetHandleFromHWND(HWND hwnd) {
#ifdef _WIN32
    DWORD pid = GetProcessIDFromHWND(hwnd);
    if (pid != 0) {
        return OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    }
#endif
    return nullptr;
}

uintptr_t GTLibc::GetModuleBaseAddress(DWORD pid, const std::string& moduleName) {
#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me32;
        me32.dwSize = sizeof(MODULEENTRY32W);
        if (Module32FirstW(hSnap, &me32)) {
            do {
                std::wstring wMod(me32.szModule);
                std::string curMod;
                for (wchar_t c : wMod) curMod += (c < 128) ? static_cast<char>(c) : '?';

                std::string curLower = curMod;
                std::string targetLower = moduleName;
                std::transform(curLower.begin(), curLower.end(), curLower.begin(), ::tolower);
                std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);

                if (curLower == targetLower) {
                    CloseHandle(hSnap);
                    return reinterpret_cast<uintptr_t>(me32.modBaseAddr);
                }
            } while (Module32NextW(hSnap, &me32));
        }
        CloseHandle(hSnap);
    }
#endif
    return 0;
}

uintptr_t GTLibc::GetProcessBaseAddress() {
    if (targetBaseAddress != 0) return targetBaseAddress;
    if (targetPid != 0) {
        targetBaseAddress = GetModuleBaseAddress(targetPid, targetProcessName);
    }
    return targetBaseAddress;
}

bool GTLibc::IsProcessRunning(const std::string& processName) {
#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;
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
                CloseHandle(hSnap);
                return true;
            }
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
#endif
    return false;
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

bool GTLibc::ReadMemoryBuffer(uintptr_t address, void* buffer, size_t size) {
#ifdef _WIN32
    if (!targetHandle || address == 0 || !buffer) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(targetHandle, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead) && bytesRead == size;
#else
    return false;
#endif
}

bool GTLibc::WriteMemoryBuffer(uintptr_t address, const void* buffer, size_t size) {
#ifdef _WIN32
    if (!targetHandle || address == 0 || !buffer) return false;
    SIZE_T bytesWritten = 0;
    return WriteProcessMemory(targetHandle, reinterpret_cast<LPVOID>(address), buffer, size, &bytesWritten) && bytesWritten == size;
#else
    return false;
#endif
}

std::string GTLibc::ReadString(uintptr_t address, size_t maxLen) {
    if (maxLen == 0) return "";
    std::vector<char> buf(maxLen + 1, 0);
    if (ReadMemoryBuffer(address, buf.data(), maxLen)) {
        return std::string(buf.data());
    }
    return "";
}

bool GTLibc::WriteString(uintptr_t address, const std::string& str) {
    return WriteMemoryBuffer(address, str.c_str(), str.length() + 1);
}

std::string GTLibc::ShellExec(const std::string& cmdArgs, bool runAsAdmin, bool waitForExit) {
#ifdef _WIN32
    SHELLEXECUTEINFOA ShExecInfo = { 0 };
    ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    ShExecInfo.hwnd = NULL;
    ShExecInfo.lpVerb = runAsAdmin ? "runas" : "open";
    ShExecInfo.lpFile = "cmd.exe";
    std::string params = "/c " + cmdArgs;
    ShExecInfo.lpParameters = params.c_str();
    ShExecInfo.lpDirectory = NULL;
    ShExecInfo.nShow = SW_HIDE;
    ShExecInfo.hInstApp = NULL;

    if (ShellExecuteExA(&ShExecInfo)) {
        if (waitForExit && ShExecInfo.hProcess != NULL) {
            WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
            CloseHandle(ShExecInfo.hProcess);
        }
        return "Command executed successfully.";
    }
    return "ShellExecuteEx failed: " + GetLastErrorAsString();
#else
    int res = system(cmdArgs.c_str());
    return "Exit code: " + std::to_string(res);
#endif
}

} // namespace GTLIBC
