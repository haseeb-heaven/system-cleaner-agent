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
#else
    DIR* dir = opendir("/proc");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name(entry->d_name);
            if (std::all_of(name.begin(), name.end(), ::isdigit)) {
                DWORD pid = static_cast<DWORD>(std::stoul(name));
                std::ifstream statusFile("/proc/" + name + "/comm");
                std::string procName;
                if (statusFile >> procName) {
                    ProcessInfo info;
                    info.pid = pid;
                    info.processName = procName;
                    info.memoryUsageBytes = GetProcessMemoryUsage(pid);
                    list.push_back(info);
                }
            }
        }
        closedir(dir);
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
#else
    std::ifstream statm("/proc/" + std::to_string(pid) + "/statm");
    size_t size = 0, resident = 0;
    if (statm >> size >> resident) {
        long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize > 0) return resident * static_cast<size_t>(pageSize);
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

bool GTLibc::IsOSCoreProcess(const std::string& processName) {
    std::string procLower = processName;
    std::transform(procLower.begin(), procLower.end(), procLower.begin(), ::tolower);
    if (procLower.length() >= 4 && procLower.substr(procLower.length() - 4) == ".exe") {
        procLower = procLower.substr(0, procLower.length() - 4);
    }
    static const std::set<std::string> osCoreSet = {
        "csrss", "lsass", "explorer", "svchost", "system", "smss", "services",
        "winlogon", "system-cleaner-agent", "agy", "antigravity", "dwm", "taskhostw",
        "sihost", "ctfmon", "fontdrvhost", "runtimebroker", "spoolsv", "taskmgr", "audiodg",
        "smartscreen", "registry"
    };
    return osCoreSet.count(procLower) > 0;
}

size_t GTLibc::KillProcessByName(const std::string& processName, bool enablePermission, bool userPermissionGranted) {
    if (!enablePermission) {
        Logger::Instance().Warn("RAM Cleaner: Killing process by name '" + processName + "' skipped (Permission disabled).");
        return 0;
    }
    if (IsOSCoreProcess(processName)) {
        Logger::Instance().Warn("SAFETY GUARD: Cannot kill OS core system service: " + processName);
        return 0;
    }
    if (IsProtectedProcess(processName) && !userPermissionGranted) {
        Logger::Instance().Info("RAM Cleaner: Cannot kill protected application without explicit user permission: " + processName);
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

// -----------------------------------------------------------------------------
// Deep Memory Scan & Process Memory Timeline
// -----------------------------------------------------------------------------

static std::map<DWORD, std::vector<ProcessMemorySnapshot>> g_processMemoryHistory;

DetailedMemoryBreakdown GTLibc::GetDetailedProcessMemory(DWORD pid) {
    DetailedMemoryBreakdown breakdown;
    breakdown.pid = pid;
    breakdown.workingSetBytes = GetProcessMemoryUsage(pid);

#ifdef _WIN32
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnap, &pe32)) {
            do {
                if (pe32.th32ProcessID == pid) {
                    std::wstring wExe(pe32.szExeFile);
                    for (wchar_t c : wExe) breakdown.processName += (c < 128) ? static_cast<char>(c) : '?';
                    break;
                }
            } while (Process32NextW(hSnap, &pe32));
        }
        CloseHandle(hSnap);
    }

    if (breakdown.processName.empty()) {
        breakdown.processName = "PID_" + std::to_string(pid);
    }

    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProc) {
        MEMORY_BASIC_INFORMATION mbi;
        unsigned char* addr = nullptr;
        size_t privHeap = 0, mapped = 0, shared = 0;

        while (VirtualQueryEx(hProc, addr, &mbi, sizeof(mbi))) {
            if (mbi.State == MEM_COMMIT) {
                if (mbi.Type == MEM_PRIVATE) privHeap += mbi.RegionSize;
                else if (mbi.Type == MEM_MAPPED) mapped += mbi.RegionSize;
                else if (mbi.Type == MEM_IMAGE) shared += mbi.RegionSize;
            }
            unsigned char* nextAddr = static_cast<unsigned char*>(mbi.BaseAddress) + mbi.RegionSize;
            if (nextAddr <= addr) break; // overflow safety
            addr = nextAddr;
        }
        CloseHandle(hProc);

        if (privHeap > 0 || mapped > 0 || shared > 0) {
            breakdown.privateHeapBytes = (privHeap > breakdown.workingSetBytes) ? static_cast<size_t>(breakdown.workingSetBytes * 0.70) : privHeap;
            breakdown.mappedFilesBytes = mapped;
            breakdown.sharedMemoryBytes = shared;
            breakdown.stackBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.05);
        } else {
            breakdown.privateHeapBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.70);
            breakdown.mappedFilesBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.15);
            breakdown.sharedMemoryBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.10);
            breakdown.stackBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.05);
        }
    } else {
        breakdown.privateHeapBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.70);
        breakdown.mappedFilesBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.15);
        breakdown.sharedMemoryBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.10);
        breakdown.stackBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.05);
    }
#else
    std::ifstream comm("/proc/" + std::to_string(pid) + "/comm");
    if (comm >> breakdown.processName) {
        // name populated
    } else {
        breakdown.processName = "PID_" + std::to_string(pid);
    }

    std::ifstream statm("/proc/" + std::to_string(pid) + "/statm");
    size_t size = 0, resident = 0, shared = 0, text = 0, lib = 0, data = 0;
    if (statm >> size >> resident >> shared >> text >> lib >> data) {
        long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize > 0) {
            size_t pSize = static_cast<size_t>(pageSize);
            breakdown.sharedMemoryBytes = shared * pSize;
            breakdown.privateHeapBytes = data * pSize;
            breakdown.mappedFilesBytes = text * pSize;
            breakdown.stackBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.05);
        }
    } else {
        breakdown.privateHeapBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.70);
        breakdown.mappedFilesBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.15);
        breakdown.sharedMemoryBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.10);
        breakdown.stackBytes = static_cast<size_t>(breakdown.workingSetBytes * 0.05);
    }
#endif
    return breakdown;
}

std::vector<DetailedMemoryBreakdown> GTLibc::GetTopMemoryConsumers(size_t topN) {
    auto procs = EnumerateAllProcesses();
    std::vector<DetailedMemoryBreakdown> list;
    list.reserve(procs.size());

    for (const auto& proc : procs) {
        DetailedMemoryBreakdown bd = GetDetailedProcessMemory(proc.pid);
        bd.processName = proc.processName;
        list.push_back(bd);
    }

    std::sort(list.begin(), list.end(), [](const DetailedMemoryBreakdown& a, const DetailedMemoryBreakdown& b) {
        return a.workingSetBytes > b.workingSetBytes;
    });

    if (list.size() > topN) {
        list.resize(topN);
    }
    return list;
}

void GTLibc::RecordMemorySnapshot() {
    auto now = std::chrono::system_clock::now();
    auto procs = EnumerateAllProcesses();

    for (const auto& proc : procs) {
        ProcessMemorySnapshot snap;
        snap.timestamp = now;
        snap.workingSetBytes = proc.memoryUsageBytes;

        auto& history = g_processMemoryHistory[proc.pid];
        history.push_back(snap);
        if (history.size() > 10) { // Keep last 10 snapshots (circular buffer)
            history.erase(history.begin());
        }
    }
}

std::vector<DWORD> GTLibc::DetectMemoryLeakCandidates(double growthThresholdPct) {
    std::vector<DWORD> candidates;

    for (const auto& kv : g_processMemoryHistory) {
        DWORD pid = kv.first;
        const auto& history = kv.second;
        if (history.size() >= 2) {
            size_t baseline = history.front().workingSetBytes;
            size_t current = history.back().workingSetBytes;

            if (baseline > 0 && current > baseline) {
                double growth = ((static_cast<double>(current) - static_cast<double>(baseline)) / static_cast<double>(baseline)) * 100.0;
                size_t diff = current - baseline;
                if (growth >= growthThresholdPct && diff >= 5ULL * 1024 * 1024) { // At least 5MB growth and >threshold%
                    candidates.push_back(pid);
                }
            }
        }
    }
    return candidates;
}

std::vector<ProcessMemorySnapshot> GTLibc::GetProcessMemoryHistory(DWORD pid) {
    auto it = g_processMemoryHistory.find(pid);
    if (it != g_processMemoryHistory.end()) {
        return it->second;
    }
    return {};
}

} // namespace GTLIBC
