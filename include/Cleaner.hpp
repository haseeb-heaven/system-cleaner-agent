#pragma once
#include "Logger.hpp"
#include "ContentInspector.hpp"
#include "ProcessManager.hpp"
#include "SecurityGuard.hpp"
#include "ConfigManager.hpp"

#include <filesystem>
#include <vector>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <system_error>
#include <future>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <set>
#include <map>
#include <fstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "shell32.lib")
#endif

// =============================================================================
// Cross-Platform OS Detection & Environment Utility Macros
// =============================================================================
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #ifndef TARGET_OS_WINDOWS
    #define TARGET_OS_WINDOWS 1
    #endif
    #define SYSTEM_OS_NAME "Windows"
    #define SYSTEM_PRIMARY_DRIVE "C:\\"
#elif defined(__APPLE__) || defined(__MACH__)
    #ifndef TARGET_OS_MAC
    #define TARGET_OS_MAC 1
    #endif
    #define SYSTEM_OS_NAME "macOS"
    #define SYSTEM_PRIMARY_DRIVE "/"
#elif defined(__linux__) || defined(__linux) || defined(linux)
    #ifndef TARGET_OS_LINUX
    #define TARGET_OS_LINUX 1
    #endif
    #define SYSTEM_OS_NAME "Linux"
    #define SYSTEM_PRIMARY_DRIVE "/"
#else
    #ifndef TARGET_OS_POSIX
    #define TARGET_OS_POSIX 1
    #endif
    #define SYSTEM_OS_NAME "POSIX OS"
    #define SYSTEM_PRIMARY_DRIVE "/"
#endif

enum class OperatingSystemKind {
    Windows,
    macOS,
    Linux,
    POSIX
};

inline OperatingSystemKind GetCurrentOSKind() {
#if defined(TARGET_OS_WINDOWS)
    return OperatingSystemKind::Windows;
#elif defined(TARGET_OS_MAC)
    return OperatingSystemKind::macOS;
#elif defined(TARGET_OS_LINUX)
    return OperatingSystemKind::Linux;
#else
    return OperatingSystemKind::POSIX;
#endif
}

inline std::string GetCurrentOSNameStr() {
    return SYSTEM_OS_NAME;
}

namespace fs = std::filesystem;

struct Target {
    fs::path path;
    bool isRecursive;
    std::string name;
    std::string category;
};

enum class CleanMode {
    Light,   // Fast cache targets
    Deep,    // Fixed targets + project build caches
    Full,    // Deep + drive analysis
    Shred    // Secure wipe (zero-overwrite before deletion)
};

struct TargetReport {
    std::string name;
    std::string category;
    fs::path path;
    uintmax_t sizeBytes = 0;
    size_t fileCount = 0;
    bool skippedProtectedFiles = false;
};

struct LargeFileInfo {
    fs::path path;
    uintmax_t sizeBytes = 0;
};

struct FolderHotspot {
    fs::path path;
    uintmax_t sizeBytes = 0;
};

struct DeepScanResult {
    uintmax_t totalCleanableCachesBytes = 0;
    uintmax_t scannedFilesCount = 0;
    uintmax_t scannedFoldersCount = 0;
    std::vector<TargetReport> cacheReports;
    std::vector<LargeFileInfo> largeFiles;
    std::vector<FolderHotspot> folderHotspots;
};

class Cleaner {
    std::vector<Target> fixedTargets;
    std::vector<fs::path> customPaths;
    std::vector<fs::path> targetDrives;
    fs::path projectRoot;

    CleanMode mode = CleanMode::Light;
    InspectionConfig config;
    SecurityGuard security;

    bool dryRun = false;
    bool emptyRecycleBin = false;
    bool killLockingProcesses = true;
    // When true, Scan()/Clean() use UNION behavior: built-in OS temp/cache
    // targets (category-filtered) PLUS the user's custom folders. When false
    // (default), legacy scoped behavior applies: a non-empty customPaths list
    // restricts the operation to those folders only (used by the ReAct agent,
    // AQL engine and unit tests for fast, targeted scans).
    bool includeBuiltInTargets = false;
    size_t maxThreads = 8;

    std::string GetEnv(const char* name) {
#ifdef _WIN32
        char* val = nullptr;
        size_t len = 0;
        _dupenv_s(&val, &len, name);
        std::string res = val ? val : "";
        if (val) free(val);
        return res;
#else
        char* val = std::getenv(name);
        return val ? std::string(val) : "";
#endif
    }

    fs::path GetUserProfile() {
#ifdef _WIN32
        std::string profile = GetEnv("USERPROFILE");
        if (profile.empty()) profile = "C:\\Users\\Default";
        return fs::path(profile);
#else
        std::string home = GetEnv("HOME");
        if (home.empty()) home = "/tmp";
        return fs::path(home);
#endif
    }

    bool IsSystemProtectedRoot(const fs::path& path) {
        try {
            fs::path p = fs::weakly_canonical(path);
            std::string s = p.string();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);

#ifdef _WIN32
            if (s == "c:\\" || s == "d:\\" || s == "c:\\windows" || s == "c:\\program files" || s == "c:\\program files (x86)") return true;
            if (s == "c:\\users" || s == GetUserProfile().string()) return true;
#else
            if (s == "/" || s == "/usr" || s == "/var" || s == "/etc" || s == "/bin" || s == "/sbin" || s == "/lib" || s == "/home" || s == GetUserProfile().string()) return true;
#endif
        } catch (...) {}
        return false;
    }

    void InitTargets() {
        fixedTargets.clear();
        fs::path userProfile = GetUserProfile();

#ifdef _WIN32
        std::string temp = GetEnv("TEMP");
        std::string winDir = GetEnv("WINDIR");
        std::string localAppData = GetEnv("LOCALAPPDATA");
        std::string programData = GetEnv("PROGRAMDATA");
        std::string appData = GetEnv("APPDATA");

        if (!temp.empty()) fixedTargets.push_back({temp, false, "Windows User Temp", "System"});
        if (!winDir.empty()) {
            fixedTargets.push_back({winDir + "\\Temp", false, "Windows System Temp", "System"});
            fixedTargets.push_back({winDir + "\\SoftwareDistribution\\Download", false, "Windows Update Downloads", "System"});
            fixedTargets.push_back({winDir + "\\Prefetch", false, "Windows Prefetch", "System"});
            fixedTargets.push_back({winDir + "\\Logs", false, "Windows System Logs", "System"});
            fixedTargets.push_back({winDir + "\\System32\\LogFiles", false, "Windows LogFiles", "System"});
            fixedTargets.push_back({winDir + "\\Minidump", false, "Windows Minidumps", "Diagnostics"});
            fixedTargets.push_back({winDir + "\\LiveKernelReports", false, "Live Kernel Reports (Dumps)", "Diagnostics"});
        }
        fixedTargets.push_back({"D:\\tmp", false, "D: Drive Temp", "System"});

        if (!programData.empty()) {
            fixedTargets.push_back({programData + "\\Microsoft\\Windows\\WER\\ReportArchive", false, "WER Archive", "Diagnostics"});
            fixedTargets.push_back({programData + "\\Microsoft\\Windows\\WER\\ReportQueue", false, "WER Queue", "Diagnostics"});
            fixedTargets.push_back({programData + "\\Microsoft\\Windows\\DeliveryOptimization\\Cache", false, "Delivery Optimization Cache", "System"});
        }
        if (!localAppData.empty()) {
            fixedTargets.push_back({localAppData + "\\CrashDumps", false, "Windows Crash Dumps", "Diagnostics"});
            fixedTargets.push_back({localAppData + "\\D3DSCache", false, "DirectX Shader Cache", "Diagnostics"});
            fixedTargets.push_back({localAppData + "\\Microsoft\\Windows\\WER", false, "Windows Error Reporting Cache", "Diagnostics"});
            fixedTargets.push_back({localAppData + "\\Microsoft\\Windows\\WebCache", false, "Windows WebCache", "System"});
            fixedTargets.push_back({localAppData + "\\Microsoft\\Windows\\INetCache", false, "Windows INetCache", "System"});
            fixedTargets.push_back({localAppData + "\\Microsoft\\Windows\\Explorer", false, "Thumbnail & Icon Cache", "System"});

            fixedTargets.push_back({localAppData + "\\npm-cache", false, "npm cache", "Developer"});
            fixedTargets.push_back({localAppData + "\\uv\\cache", false, "uv cache", "Developer"});
            fixedTargets.push_back({localAppData + "\\pip\\Cache", false, "pip cache", "Developer"});
            fixedTargets.push_back({localAppData + "\\Yarn\\Cache", false, "Yarn cache", "Developer"});
            fixedTargets.push_back({localAppData + "\\pnpm\\store", false, "pnpm store", "Developer"});
            fixedTargets.push_back({localAppData + "\\go-build", false, "Go Build Cache", "Developer"});

            fixedTargets.push_back({localAppData + "\\Google\\Chrome\\User Data\\Default\\Cache", false, "Chrome Web Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\Google\\Chrome\\User Data\\Default\\Code Cache", false, "Chrome Code Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\Google\\Chrome\\User Data\\Default\\GPUCache", false, "Chrome GPU Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\Google\\Chrome\\User Data\\Default\\Service Worker\\CacheStorage", false, "Chrome Service Worker Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\Microsoft\\Edge\\User Data\\Default\\Cache", false, "Edge Web Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\Microsoft\\Edge\\User Data\\Default\\Code Cache", false, "Edge Code Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\Microsoft\\Edge\\User Data\\Default\\GPUCache", false, "Edge GPU Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\Microsoft\\Edge\\User Data\\Default\\Service Worker\\CacheStorage", false, "Edge Service Worker Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Cache", false, "Brave Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\BraveSoftware\\Brave-Browser\\User Data\\Default\\GPUCache", false, "Brave GPU Cache", "Browser"});
            fixedTargets.push_back({localAppData + "\\Mozilla\\Firefox\\Profiles", false, "Firefox Profiles Cache", "Browser"});

            fixedTargets.push_back({localAppData + "\\Spotify\\Storage", false, "Spotify Cache", "Applications"});
        }
        if (!appData.empty()) {
            fixedTargets.push_back({appData + "\\discord\\Cache", false, "Discord Cache", "Messaging"});
            fixedTargets.push_back({appData + "\\discord\\Code Cache", false, "Discord Code Cache", "Messaging"});
            fixedTargets.push_back({appData + "\\Slack\\Cache", false, "Slack Cache", "Messaging"});
            fixedTargets.push_back({appData + "\\Telegram Desktop\\tdata\\user_data", false, "Telegram Cache", "Messaging"});
            fixedTargets.push_back({appData + "\\Code\\Cache", false, "VS Code Cache", "Developer"});
            fixedTargets.push_back({appData + "\\Code\\CachedData", false, "VS Code Cached Data", "Developer"});
            fixedTargets.push_back({appData + "\\Cursor\\Cache", false, "Cursor IDE Cache", "Developer"});
            fixedTargets.push_back({appData + "\\Opera Software\\Opera Stable\\Cache", false, "Opera Browser Cache", "Browser"});
        }
        if (!userProfile.empty()) {
            fixedTargets.push_back({userProfile / ".cache", false, "User .cache Folder", "System"});
            fixedTargets.push_back({userProfile / ".npm", false, "User .npm Folder", "Developer"});
            fixedTargets.push_back({userProfile / ".nuget\\packages", false, "NuGet Packages Cache", "Developer"});
            fixedTargets.push_back({userProfile / ".cargo\\registry\\cache", false, "Cargo Registry Cache", "Developer"});
            fixedTargets.push_back({userProfile / ".gradle\\caches", false, "Gradle Caches", "Developer"});
            fixedTargets.push_back({userProfile / ".m2\\repository", false, "Maven Repository Cache", "Developer"});
            fixedTargets.push_back({"D:\\npm-cache", false, "D: npm Cache", "Developer"});
        }
#else
        // POSIX / Linux & macOS Deep Cache Targets
        fixedTargets.push_back({"/tmp", false, "System Temp (/tmp)", "System"});
        fixedTargets.push_back({"/var/tmp", false, "System Temp (/var/tmp)", "System"});
        fixedTargets.push_back({"/var/log", false, "System Logs (/var/log)", "System"});
        fixedTargets.push_back({"/var/crash", false, "Linux System Crash Reports", "System"});
        fixedTargets.push_back({"/var/cache/apt/archives", false, "APT Package Archives Cache", "System"});
        fixedTargets.push_back({"/var/cache/pacman/pkg", false, "Pacman Package Cache", "System"});
        fixedTargets.push_back({"/var/cache/dnf", false, "DNF Package Cache", "System"});
        fixedTargets.push_back({"/var/lib/docker/tmp", false, "Docker Temp Storage", "Developer"});

        if (!userProfile.empty()) {
            fixedTargets.push_back({userProfile / ".cache", false, "User ~/.cache", "System"});
            fixedTargets.push_back({userProfile / ".local/share/Trash/files", false, "Linux Trash Files", "System"});
            fixedTargets.push_back({userProfile / ".local/share/Trash/info", false, "Linux Trash Info", "System"});
            fixedTargets.push_back({userProfile / ".npm", false, "User ~/.npm Cache", "Developer"});
            fixedTargets.push_back({userProfile / ".cache/pip", false, "User pip Cache", "Developer"});
            fixedTargets.push_back({userProfile / ".cargo/registry/cache", false, "Cargo Registry Cache", "Developer"});
            fixedTargets.push_back({userProfile / ".gradle/caches", false, "Gradle Build Cache", "Developer"});
            fixedTargets.push_back({userProfile / ".m2/repository", false, "Maven Repository Cache", "Developer"});
            fixedTargets.push_back({userProfile / ".cache/yarn", false, "Yarn Build Cache", "Developer"});
            fixedTargets.push_back({userProfile / ".cache/google-chrome", false, "Chrome Web Cache", "Browser"});
            fixedTargets.push_back({userProfile / ".cache/chromium", false, "Chromium Web Cache", "Browser"});
            fixedTargets.push_back({userProfile / ".cache/microsoft-edge", false, "Edge Web Cache", "Browser"});
            fixedTargets.push_back({userProfile / ".mozilla/firefox", false, "Firefox Profiles Cache", "Browser"});
            fixedTargets.push_back({userProfile / ".config/Code/Cache", false, "VS Code Cache", "Developer"});
            fixedTargets.push_back({userProfile / ".config/Cursor/Cache", false, "Cursor IDE Cache", "Developer"});

#ifdef __APPLE__
            fixedTargets.push_back({userProfile / "Library/Caches", false, "macOS User Caches", "System"});
            fixedTargets.push_back({userProfile / "Library/Logs", false, "macOS User Logs", "System"});
            fixedTargets.push_back({userProfile / "Library/Application Support/CrashReporter", false, "macOS Crash Dumps", "System"});
            fixedTargets.push_back({userProfile / ".Trash", false, "macOS Trash Bin", "System"});
            fixedTargets.push_back({userProfile / "Library/Developer/Xcode/DerivedData", false, "Xcode DerivedData Build Artifacts", "Developer"});
            fixedTargets.push_back({userProfile / "Library/Caches/com.apple.dt.Xcode", false, "Xcode IDE Caches", "Developer"});
            fixedTargets.push_back({userProfile / "Library/Caches/Homebrew", false, "Homebrew Formulae Cache", "Developer"});
            fixedTargets.push_back({userProfile / "Library/Caches/Google/Chrome", false, "macOS Chrome Cache", "Browser"});
            fixedTargets.push_back({userProfile / "Library/Caches/Firefox", false, "macOS Firefox Cache", "Browser"});
            fixedTargets.push_back({userProfile / "Library/Caches/com.apple.Safari", false, "macOS Safari Cache", "Browser"});
            fixedTargets.push_back({userProfile / "Library/Caches/com.microsoft.Edge", false, "macOS Edge Cache", "Browser"});
#endif
        }
#endif
    }

    bool IsCategoryAllowed(const std::string& cat) {
        std::string cLower = cat;
        std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
        if (!config.includeCategories.empty() && config.includeCategories.count(cLower) == 0) return false;
        if (!config.excludeCategories.empty() && config.excludeCategories.count(cLower) > 0) return false;
        return true;
    }

    // ----------------------------------------------------------------
    // Build the active target list for Scan()/Clean().
    //  - includeBuiltInTargets=true  -> UNION: fixed OS targets (category
    //    filtered) + custom folders ("Custom" category).
    //  - includeBuiltInTargets=false -> legacy scoped mode: customPaths
    //    non-empty restricts to custom folders; otherwise fixed targets.
    // ----------------------------------------------------------------
    void AssembleActiveTargets(std::vector<Target>& activeTargets) {
        auto addFixedTargets = [&]() {
            for (const auto& target : fixedTargets) {
                if (fs::exists(target.path) && IsCategoryAllowed(target.category)) {
                    activeTargets.push_back(target);
                }
            }
        };
        auto addCustomTargets = [&]() {
            for (const auto& cp : customPaths) {
                if (fs::exists(cp)) {
                    // Security audit: check path before accepting it
                    SecurityReport sr = security.AuditPath(cp);
                    if (sr.level == ThreatLevel::Critical) {
                        SecurityGuard::PrintBlockedReport(sr);
                        continue;
                    }
                    activeTargets.push_back({cp, true, "Custom Path [" + cp.string() + "]", "Custom"});
                }
            }
        };

        if (includeBuiltInTargets) {
            addFixedTargets();
            if (IsCategoryAllowed("Custom")) addCustomTargets();
        } else if (!customPaths.empty()) {
            addCustomTargets();
        } else {
            addFixedTargets();
        }
    }

    void SecureShredFile(const fs::path& filePath) {
        try {
            std::error_code ec;
            uintmax_t size = fs::file_size(filePath, ec);
            if (!ec && size > 0) {
                std::ofstream file(filePath, std::ios::binary | std::ios::out);
                if (file.is_open()) {
                    std::vector<char> zeros(std::min<size_t>(size, 65536), 0);
                    uintmax_t written = 0;
                    while (written < size) {
                        size_t chunk = std::min<size_t>(size - written, zeros.size());
                        file.write(zeros.data(), chunk);
                        written += chunk;
                    }
                    file.flush();
                }
            }
            fs::remove(filePath, ec);
        } catch (...) {}
    }

public:
    Cleaner() {
        size_t threads = std::thread::hardware_concurrency();
        maxThreads = (threads > 0) ? threads : 8;
        InitTargets();

        AppConfig cfg = ConfigManager::Load();
        security.sandboxEnabled = cfg.sandboxMode;
        security.dangerousPathProtection = cfg.pathProtection;
        dryRun = cfg.dryRun;
        killLockingProcesses = cfg.killLocks;
    }

    void SetMode(CleanMode m) { mode = m; }
    void SetDryRun(bool enable) {
        dryRun = enable;
        // Sandbox enabled = no real deletions; sandbox disables when dryRun=false AND user explicitly opted out
        if (!enable) security.sandboxEnabled = false;
    }
    void SetSandbox(bool enable) { security.sandboxEnabled = enable; }
    void SetDangerousPathProtection(bool enable) { security.dangerousPathProtection = enable; }
    void SetEmptyRecycleBin(bool enable) { emptyRecycleBin = enable; }
    void SetKillLockingProcesses(bool enable) { killLockingProcesses = enable; }
    void SetMaxThreads(size_t t) { maxThreads = (t > 0) ? t : 4; }
    void SetCustomPaths(const std::vector<fs::path>& paths) { customPaths = paths; }
    void SetIncludeBuiltInTargets(bool enable) { includeBuiltInTargets = enable; }
    void SetTargetDrives(const std::vector<fs::path>& drives) { targetDrives = drives; }
    void SetProjectRoot(const fs::path& root) { projectRoot = root; }
    void SetInspectionConfig(const InspectionConfig& cfg) { config = cfg; }
    void SetIncludeCategories(const std::vector<std::string>& cats) {
        config.includeCategories.clear();
        for (const auto& c : cats) {
            std::string cLower = c;
            std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
            config.includeCategories.insert(cLower);
        }
    }
    void SetExcludeCategories(const std::vector<std::string>& cats) {
        config.excludeCategories.clear();
        for (const auto& c : cats) {
            std::string cLower = c;
            std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
            config.excludeCategories.insert(cLower);
        }
    }
    const SecurityGuard& GetSecurity() const { return security; }

    static std::string FormatSize(uintmax_t bytes) {
        if (bytes == 0) return "0 B";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2);
        if (bytes < 1024) {
            ss << static_cast<double>(bytes) << " B";
        } else if (bytes < 1024 * 1024) {
            ss << (bytes / 1024.0) << " KB";
        } else if (bytes < 1024 * 1024 * 1024) {
            ss << (bytes / (1024.0 * 1024.0)) << " MB";
        } else {
            ss << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
        }
        return ss.str();
    }

    TargetReport InspectTarget(const Target& target) {
        TargetReport report;
        report.name = target.name;
        report.category = target.category;
        report.path = target.path;

        try {
            std::error_code ec;
            if (!fs::exists(target.path, ec) || fs::is_symlink(target.path, ec)) return report;

            if (fs::is_regular_file(target.path, ec)) {
                if (ContentInspector::InspectFile(target.path, config) != FileJunkType::ProtectedUserFile) {
                    report.sizeBytes = fs::file_size(target.path, ec);
                    report.fileCount = 1;
                } else {
                    report.skippedProtectedFiles = true;
                }
                return report;
            }

            auto options = fs::directory_options::skip_permission_denied;
            auto it = fs::recursive_directory_iterator(target.path, options, ec);
            if (ec) return report;

            while (it != fs::recursive_directory_iterator()) {
                try {
                    std::error_code entry_ec;
                    if (it->is_symlink(entry_ec)) {
                        it.disable_recursion_pending();
                    } else if (it->is_regular_file(entry_ec)) {
                        if (ContentInspector::InspectFile(it->path(), config) != FileJunkType::ProtectedUserFile) {
                            uintmax_t sz = it->file_size(entry_ec);
                            if (!entry_ec) {
                                report.sizeBytes += sz;
                                report.fileCount++;
                            }
                        } else {
                            report.skippedProtectedFiles = true;
                        }
                    }
                } catch (...) {}
                it.increment(ec);
                if (ec) ec.clear();
            }
        } catch (...) {}
        return report;
    }

    DeepScanResult DeepScan(uintmax_t thresholdMB = 100) {
        DeepScanResult res;
        res.cacheReports = Scan();
        for (const auto& r : res.cacheReports) {
            res.totalCleanableCachesBytes += r.sizeBytes;
        }

        std::vector<fs::path> drivesToScan = targetDrives;
        if (drivesToScan.empty()) {
#ifdef _WIN32
            drivesToScan.push_back("C:\\");
            drivesToScan.push_back("D:\\");
#else
            drivesToScan.push_back("/");
#endif
        }

        uintmax_t limitSizeBytes = thresholdMB * 1024 * 1024;
        std::map<std::string, uintmax_t> folderSizes;
        std::map<std::string, std::vector<std::string>> folderSubdirs;
        std::mutex mtx;

        for (const auto& drive : drivesToScan) {
            std::error_code ec;
            if (!fs::exists(drive, ec)) continue;

            std::vector<fs::path> topItems;
            auto options = fs::directory_options::skip_permission_denied;
            for (const auto& entry : fs::directory_iterator(drive, options, ec)) {
                try {
                    if (entry.is_directory(ec) && !entry.is_symlink(ec)) {
                        std::string nameLower = entry.path().filename().string();
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        if (nameLower != "$recycle.bin" && nameLower != "system volume information" && nameLower != "windows" && nameLower != "$winre_backup_partition.marker") {
                            topItems.push_back(entry.path());
                        }
                    }
                } catch (...) {}
            }

            std::vector<std::thread> workers;
            for (const auto& item : topItems) {
                workers.emplace_back([this, item, limitSizeBytes, &folderSizes, &folderSubdirs, &res, &mtx]() {
                    try {
                        std::error_code ec;
                        auto options = fs::directory_options::skip_permission_denied;
                        auto it = fs::recursive_directory_iterator(item, options, ec);
                        while (it != fs::recursive_directory_iterator()) {
                            try {
                                if (it->is_symlink(ec)) {
                                    it.disable_recursion_pending();
                                } else if (it->is_regular_file(ec)) {
                                    uintmax_t sz = it->file_size(ec);
                                    if (!ec && sz >= limitSizeBytes) {
                                        std::lock_guard<std::mutex> lock(mtx);
                                        res.largeFiles.push_back({ it->path(), sz });
                                        res.scannedFilesCount++;
                                    }
                                } else if (it->is_directory(ec)) {
                                    std::lock_guard<std::mutex> lock(mtx);
                                    res.scannedFoldersCount++;
                                    std::string pStr = it->path().string();
                                    std::string parentStr = it->path().parent_path().string();
                                    folderSubdirs[parentStr].push_back(pStr);
                                }
                            } catch (...) {}
                            it.increment(ec);
                            if (ec) ec.clear();
                        }
                    } catch (...) {}
                });
            }
            for (auto& w : workers) {
                if (w.joinable()) w.join();
            }
        }

        std::sort(res.largeFiles.begin(), res.largeFiles.end(), [](const LargeFileInfo& a, const LargeFileInfo& b) {
            return a.sizeBytes > b.sizeBytes;
        });

        return res;
    }

    void EmptyWindowsRecycleBin() {
        if (!emptyRecycleBin) return;

#ifdef _WIN32
        Logger::Instance().Info("Emptying Windows Recycle Bin via Shell API...");
        if (!dryRun) {
            HRESULT hr = SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
            if (SUCCEEDED(hr)) {
                Logger::Instance().Info("Windows Recycle Bin cleared successfully.");
            } else {
                Logger::Instance().Info("Recycle Bin was already empty or skipping.");
            }
        } else {
            Logger::Instance().Info("[DRY-RUN] Would empty Windows Recycle Bin.");
        }
#else
        fs::path userProfile = GetUserProfile();
        std::vector<fs::path> trashPaths = {
            userProfile / ".local/share/Trash/files",
            userProfile / ".local/share/Trash/info"
        };
#ifdef __APPLE__
        trashPaths.push_back(userProfile / ".Trash");
#endif
        Logger::Instance().Info("Purging POSIX Trash Bins...");
        for (const auto& tp : trashPaths) {
            if (fs::exists(tp)) {
                if (!dryRun) {
                    SmartDeleteContentsFast(tp);
                } else {
                    Logger::Instance().Info("[DRY-RUN] Would purge trash directory: " + tp.string());
                }
            }
        }
#endif
    }

    std::vector<TargetReport> Scan() {
        ProcessManager::StopLockingProcesses(killLockingProcesses);

        Logger::Instance().Info("Starting Multi-Platform Parallel Scan (Threads: " + std::to_string(maxThreads) + ")...");
        uintmax_t grandTotal = 0;
        size_t itemsCount = 0;

        // Print security status before every scan
        security.PrintSecurityStatus();

        std::vector<Target> activeTargets;
        AssembleActiveTargets(activeTargets);

        std::vector<std::future<TargetReport>> futures;
        std::vector<TargetReport> reports;

        for (const auto& target : activeTargets) {
            futures.push_back(std::async(std::launch::async, [this, target]() {
                return InspectTarget(target);
            }));
        }

        for (auto& fut : futures) {
            TargetReport report = fut.get();
            if (report.sizeBytes > 0) {
                grandTotal += report.sizeBytes;
                itemsCount++;
                reports.push_back(report);

                std::string shieldNote = report.skippedProtectedFiles ? " (Protected user files preserved)" : "";
                Logger::Instance().Info("[" + report.category + "] " + report.name + " [" + report.path.string() + "] | Size: " + FormatSize(report.sizeBytes) + shieldNote);
            }
        }

        // Recycle bin purge is self-guarded by the emptyRecycleBin flag.
        EmptyWindowsRecycleBin();

        Logger::Instance().Info("------------------------------------------------------------------");
        Logger::Instance().Info("Scan Summary: Found " + std::to_string(itemsCount) + " cleanable locations.");
        Logger::Instance().Info("Total Cleanable Junk Space: " + FormatSize(grandTotal));

        return reports;
    }

    void Clean() {
        ProcessManager::StopLockingProcesses(killLockingProcesses);

        std::string modeHeader = dryRun ? "[DRY-RUN PREVIEW]" : (mode == CleanMode::Shred ? "[SECURE SHREDDING]" : "[EXECUTING CLEANUP]");
        Logger::Instance().Info("Starting Multi-Platform Parallel Cleanup " + modeHeader + "...");
        
        std::atomic<uintmax_t> totalFreed{0};
        std::atomic<size_t> cleanedCount{0};

        std::vector<Target> activeTargets;
        AssembleActiveTargets(activeTargets);

        std::vector<std::future<void>> futures;

        for (const auto& target : activeTargets) {
            if (!IsSystemProtectedRoot(target.path)) {
                // Security audit every target before clean
                SecurityReport sr = security.AuditPath(target.path);
                if (sr.level == ThreatLevel::Critical || sr.level == ThreatLevel::Dangerous) {
                    SecurityGuard::PrintBlockedReport(sr);
                    Logger::Instance().Warn("SECURITY BLOCKED: " + sr.reason);
                    continue;
                }
                // In sandbox mode, SecurityReport.blocked=true but level=Suspicious — simulate only
                bool simulateOnly = sr.blocked && security.sandboxEnabled;

                futures.push_back(std::async(std::launch::async, [this, target, modeHeader, &totalFreed, &cleanedCount, simulateOnly, sr]() {
                    TargetReport report = InspectTarget(target);
                    if (report.sizeBytes > 0) {
                        Logger::Instance().Info(modeHeader + " Cleaning [" + target.category + "] " + target.name + " (" + FormatSize(report.sizeBytes) + ")...");
                        if (simulateOnly || dryRun) {
                            if (simulateOnly) SecurityGuard::PrintBlockedReport(sr);
                            totalFreed += report.sizeBytes;
                        } else {
                            uintmax_t freed = SmartDeleteContentsFast(target.path);
                            totalFreed += freed;
                        }
                        cleanedCount++;
                    }
                }));
            }
        }

        for (auto& fut : futures) {
            fut.get();
        }

        // Recycle bin purge is self-guarded by the emptyRecycleBin flag.
        EmptyWindowsRecycleBin();

        Logger::Instance().Info("------------------------------------------------------------------");
        std::string verb = dryRun ? "Would free total" : "Total space freed";
        Logger::Instance().Info("Cleanup Completed! Cleaned " + std::to_string(cleanedCount) + " targets. " + verb + ": " + FormatSize(totalFreed));
    }

    uintmax_t SmartDeleteContentsFast(const fs::path& dir) {
        if (IsSystemProtectedRoot(dir)) {
            Logger::Instance().Warn("SAFETY GUARD BLOCKED: Refusing to delete system root path: " + dir.string());
            return 0;
        }

        uintmax_t freed = 0;
        try {
            std::error_code ec;
            auto options = fs::directory_options::skip_permission_denied;
            auto it = fs::recursive_directory_iterator(dir, options, ec);
            if (ec) return 0;

            std::vector<fs::path> dirsToRemove;

            while (it != fs::recursive_directory_iterator()) {
                try {
                    std::error_code entry_ec;
                    fs::path itemPath = it->path();

                    if (it->is_symlink(entry_ec)) {
                        it.disable_recursion_pending();
                    } else if (it->is_regular_file(entry_ec)) {
                        if (ContentInspector::InspectFile(itemPath, config) != FileJunkType::ProtectedUserFile) {
                            uintmax_t fsize = it->file_size(entry_ec);
                            if (mode == CleanMode::Shred) {
                                SecureShredFile(itemPath);
                                freed += fsize;
                            } else if (fs::remove(itemPath, entry_ec)) {
                                freed += fsize;
                            }
                        }
                    } else if (it->is_directory(entry_ec)) {
                        dirsToRemove.push_back(itemPath);
                    }
                } catch (...) {}
                it.increment(ec);
                if (ec) ec.clear();
            }

            std::sort(dirsToRemove.begin(), dirsToRemove.end(), [](const fs::path& a, const fs::path& b) {
                return a.string().length() > b.string().length();
            });

            for (const auto& d : dirsToRemove) {
                try {
                    std::error_code rec;
                    if (fs::is_empty(d, rec)) {
                        fs::remove(d, rec);
                    }
                } catch (...) {}
            }
        } catch (...) {}
        return freed;
    }
};
