#pragma once

#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

// Threat level for an attempted operation
enum class ThreatLevel {
    Safe,       // Fine to proceed
    Suspicious, // Warn & log, proceed with caution
    Dangerous,  // Block unless user explicitly overrode sandbox
    Critical    // Always block — never allowed
};

struct SecurityReport {
    ThreatLevel level = ThreatLevel::Safe;
    std::string reason;
    fs::path blockedPath;
    bool blocked = false;
};

class SecurityGuard {
public:
    // Sandbox is ON by default — real deletions require explicit --no-sandbox flag
    bool sandboxEnabled = true;

    // Extra dangerous path protection (ON by default)
    bool dangerousPathProtection = true;

    SecurityGuard() = default;
    SecurityGuard(bool sandbox, bool pathProtection)
        : sandboxEnabled(sandbox), dangerousPathProtection(pathProtection) {}

    // ----------------------------------------------------------------
    // Core Path Audit
    // ----------------------------------------------------------------
    SecurityReport AuditPath(const fs::path& path) const {
        SecurityReport report;
        report.blockedPath = path;

        std::string pathStr = path.string();
        std::string pathLower = pathStr;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        // --- ALLOWLIST: Explicitly safe temp/cache/junk paths — NEVER blocked ---
        for (const auto& safe : GetSafeCleanPaths()) {
            std::string sl = safe;
            std::transform(sl.begin(), sl.end(), sl.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (pathLower == sl || pathLower.find(sl + "\\") == 0 || pathLower.find(sl + "/") == 0) {
                // This is an explicitly safe temp/cache path — allow it
                report.level = ThreatLevel::Safe;
                report.blocked = false;
                return report;
            }
        }

        // --- CRITICAL: Always blocked system roots ---
        for (const auto& blocked : GetCriticalBlockedPaths()) {
            std::string bl = blocked;
            std::transform(bl.begin(), bl.end(), bl.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (pathLower == bl || pathLower.find(bl + "\\") == 0 || pathLower.find(bl + "/") == 0) {
                report.level = ThreatLevel::Critical;
                report.reason = "CRITICAL SYSTEM PATH BLOCKED: '" + pathStr + "' is a protected OS directory. Deletion would cause system damage.";
                report.blocked = true;
                return report;
            }
        }

        // --- DANGEROUS: High-risk paths (blocked when dangerousPathProtection=true) ---
        if (dangerousPathProtection) {
            for (const auto& danger : GetDangerousPaths()) {
                std::string dl = danger;
                std::transform(dl.begin(), dl.end(), dl.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if (pathLower == dl || pathLower.find(dl + "\\") == 0 || pathLower.find(dl + "/") == 0) {
                    report.level = ThreatLevel::Dangerous;
                    report.reason = "DANGEROUS PATH BLOCKED: '" + pathStr + "' is a sensitive system or user directory. Enable --no-path-protection to override.";
                    report.blocked = true;
                    return report;
                }
            }
        }

        // --- SANDBOX: Any real deletion blocked unless sandbox is off ---
        if (sandboxEnabled) {
            report.level = ThreatLevel::Suspicious;
            report.reason = "SANDBOX MODE: Operation on '" + pathStr + "' simulated (dry-run). Use --no-sandbox to allow real deletion.";
            report.blocked = true; // In sandbox mode, actual file mutations are blocked
            return report;
        }

        report.level = ThreatLevel::Safe;
        report.blocked = false;
        return report;
    }

    // ----------------------------------------------------------------
    // Check whether a file extension is dangerous to delete
    // ----------------------------------------------------------------
    bool IsDangerousExtension(const fs::path& filePath) const {
        std::string ext = filePath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        static const std::set<std::string> criticalExts = {
            ".exe", ".dll", ".sys", ".drv", ".vxd",   // Windows executables/drivers
            ".msi", ".inf", ".cat",                     // Windows installers
            ".bat", ".cmd", ".ps1", ".vbs", ".reg",     // Scripts & registry
            ".py", ".cpp", ".c", ".h", ".hpp", ".cs",  // Source code
            ".js", ".ts", ".html", ".css", ".json",    // Web source
            ".sql", ".db", ".sqlite", ".mdb",          // Databases
            ".pdf", ".doc", ".docx", ".xls", ".xlsx",  // Documents
            ".png", ".jpg", ".jpeg", ".gif", ".svg",   // Images
            ".mp4", ".mp3", ".avi", ".mov",            // Media
            ".zip", ".rar", ".7z", ".tar", ".gz"       // Archives
        };

        return criticalExts.count(ext) > 0;
    }

    // ----------------------------------------------------------------
    // Print security status banner
    // ----------------------------------------------------------------
    void PrintSecurityStatus() const {
        std::cout << "\033[1;33m+--[ Security Status ]----------------------------------------------+\033[0m\n";
        std::cout << (sandboxEnabled
            ? "\033[1;32m|  [ON]  Sandbox Mode         - Real deletions BLOCKED (dry-run)     |\033[0m\n"
            : "\033[1;31m|  [OFF] Sandbox Mode         - Real deletions ENABLED                |\033[0m\n");
        std::cout << (dangerousPathProtection
            ? "\033[1;32m|  [ON]  Dangerous Path Guard - System directory deletion BLOCKED      |\033[0m\n"
            : "\033[1;31m|  [OFF] Dangerous Path Guard - System directory protection disabled   |\033[0m\n");
        std::cout << "\033[1;33m+-------------------------------------------------------------------+\033[0m\n\n";
    }

    // ----------------------------------------------------------------
    // Report a blocked operation
    // ----------------------------------------------------------------
    static void PrintBlockedReport(const SecurityReport& report) {
        std::string color;
        std::string label;
        switch (report.level) {
            case ThreatLevel::Critical:    label = "[CRITICAL]  "; break;
            case ThreatLevel::Dangerous:   label = "[DANGEROUS] "; break;
            case ThreatLevel::Suspicious:  label = "[SANDBOX]   "; break;
            default:                       label = "[SAFE]      "; break;
        }
        // Route through the Logger so the message is automatically suppressed
        // when the TUI is active (otherwise a background scan/clean worker
        // would write directly to std::cout and corrupt the menu render).
        std::string fullMsg = label + report.reason;
        if (report.level == ThreatLevel::Critical || report.level == ThreatLevel::Dangerous) {
            Logger::Instance().Warn(fullMsg);
        } else {
            Logger::Instance().Info(fullMsg);
        }
    }

    // Public helper: Get comma-joined default safe paths for the current OS
    static std::string GetDefaultCleanPathsStr() {
        auto paths = GetSafeCleanPaths();
        std::string result;
        for (size_t i = 0; i < paths.size(); ++i) {
            if (i > 0) result += ",";
            result += paths[i];
        }
        return result;
    }

private:
    // ----------------------------------------------------------------
    // Explicitly safe temp/cache/junk paths — always allowed to clean
    // These override the critical blocked list checks
    // ----------------------------------------------------------------
    static std::vector<std::string> GetSafeCleanPaths() {
        std::vector<std::string> paths;

#ifdef _WIN32
        // Resolve %USERPROFILE% and %TEMP% dynamically
        char userProfile[MAX_PATH] = {};
        char localAppData[MAX_PATH] = {};
        char winDir[MAX_PATH] = {};
        ExpandEnvironmentStringsA("%USERPROFILE%", userProfile, MAX_PATH);
        ExpandEnvironmentStringsA("%LOCALAPPDATA%", localAppData, MAX_PATH);
        ExpandEnvironmentStringsA("%WINDIR%", winDir, MAX_PATH);

        std::string up(userProfile);
        std::string la(localAppData);
        std::string wd(winDir);

        // Primary user temp / cache folders
        paths.push_back(up + "\\AppData\\Local\\Temp");
        paths.push_back(la + "\\Temp");
        paths.push_back(la + "\\Microsoft\\Windows\\INetCache");
        paths.push_back(la + "\\Microsoft\\Windows\\Explorer");
        paths.push_back(la + "\\Microsoft\\Windows\\WER");          // Windows Error Reporting
        paths.push_back(la + "\\Microsoft\\Windows\\WebCache");
        paths.push_back(la + "\\CrashDumps");
        paths.push_back(la + "\\D3DSCache");                        // DirectX shader cache
        paths.push_back(la + "\\Microsoft\\Edge\\User Data\\Default\\Cache");
        paths.push_back(la + "\\Microsoft\\Edge\\User Data\\Default\\GPUCache");
        paths.push_back(la + "\\Microsoft\\Edge\\User Data\\Default\\Service Worker\\CacheStorage");
        paths.push_back(la + "\\Google\\Chrome\\User Data\\Default\\Cache");
        paths.push_back(la + "\\Google\\Chrome\\User Data\\Default\\Code Cache");
        paths.push_back(la + "\\Google\\Chrome\\User Data\\Default\\GPUCache");
        paths.push_back(la + "\\Google\\Chrome\\User Data\\Default\\Service Worker\\CacheStorage");
        paths.push_back(la + "\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Cache");
        paths.push_back(la + "\\BraveSoftware\\Brave-Browser\\User Data\\Default\\GPUCache");
        paths.push_back(la + "\\Mozilla\\Firefox\\Profiles");       // browser cache subfolders
        paths.push_back(la + "\\Packages");                         // UWP package caches
        paths.push_back(up + "\\AppData\\Local\\pip\\cache");
        paths.push_back(up + "\\AppData\\Local\\npm-cache");
        paths.push_back(up + "\\AppData\\Local\\nuget\\cache");
        paths.push_back(up + "\\AppData\\Local\\SquirrelTemp");
        paths.push_back(up + "\\AppData\\Roaming\\npm-cache");

        // ProgramData junk caches (these live under the critical-blocked
        // C:\ProgramData root, so they MUST be explicitly allowlisted here)
        char programData[MAX_PATH] = {};
        ExpandEnvironmentStringsA("%PROGRAMDATA%", programData, MAX_PATH);
        std::string pd(programData);
        paths.push_back(pd + "\\Microsoft\\Windows\\WER");                       // Error reporting archive/queue
        paths.push_back(pd + "\\Microsoft\\Windows\\DeliveryOptimization\\Cache"); // Windows Update DO cache

        // Windows system temp (safe to clean contents, not the folder itself)
        paths.push_back(wd + "\\Temp");
        paths.push_back(wd + "\\Prefetch");
        paths.push_back(wd + "\\SoftwareDistribution\\Download");   // Windows Update cache
        paths.push_back(wd + "\\Logs");
        paths.push_back(wd + "\\Minidump");                         // BSOD minidumps
        paths.push_back(wd + "\\LiveKernelReports");                // Live kernel dump reports
        paths.push_back(wd + "\\System32\\LogFiles");               // System32 event/audit logs
        paths.push_back(wd + "\\debug");                            // Kernel debug logs (WPP/trace)

#elif defined(__APPLE__)
        char* home = std::getenv("HOME");
        if (home) {
            std::string h(home);
            paths.push_back(h + "/Library/Caches");
            paths.push_back(h + "/Library/Logs");
            paths.push_back(h + "/Library/Application Support/CrashReporter");
            paths.push_back(h + "/Library/Saved Application State");
            paths.push_back(h + "/Library/Containers");             // per-app sandbox cache
            paths.push_back(h + "/.Trash");
            paths.push_back(h + "/.npm/_cacache");
            paths.push_back(h + "/.pip/cache");
            paths.push_back(h + "/.cache");                         // XDG cache on macOS too
        }
        paths.push_back("/private/tmp");
        paths.push_back("/tmp");
        paths.push_back("/private/var/folders");                     // macOS per-user tmp/cache
        paths.push_back("/System/Volumes/Data/.Spotlight-V100");     // Spotlight index cache
        paths.push_back("/private/var/log");

#else
        // Linux
        char* home = std::getenv("HOME");
        if (home) {
            std::string h(home);
            paths.push_back(h + "/.cache");
            paths.push_back(h + "/.local/share/Trash");
            paths.push_back(h + "/.thumbnails");
            paths.push_back(h + "/.npm/_cacache");
            paths.push_back(h + "/.pip/cache");
            paths.push_back(h + "/.gradle/caches");
            paths.push_back(h + "/.m2/repository");                  // Maven cache
            paths.push_back(h + "/.cargo/registry/cache");
            paths.push_back(h + "/.docker/tmp");
            paths.push_back(h + "/.yarn/cache");
        }
        paths.push_back("/tmp");
        paths.push_back("/var/tmp");
        paths.push_back("/var/cache/apt/archives");
        paths.push_back("/var/cache/apt");
        paths.push_back("/var/cache/yum");
        paths.push_back("/var/cache/dnf");
        paths.push_back("/var/cache/pacman/pkg");
        paths.push_back("/var/log");                                  // log rotation cleanup
#endif

        return paths;
    }

    // Always blocked — these will never be allowed regardless of sandbox flag.
    // NOTE: The bare "c:\windows" root prefix is intentionally NOT in this list
    // (it would over-block safe subdirectories like Logs/LogFiles/Prefetch that
    // Disk Cleanup is meant to clean). The granular system32/syswow64/system
    // entries below protect the truly sensitive OS files.
    static std::vector<std::string> GetCriticalBlockedPaths() {
        return {
#ifdef _WIN32
            "c:\\windows\\system32",
            "c:\\windows\\syswow64",
            "c:\\windows\\system",
            "c:\\program files",
            "c:\\program files (x86)",
            "c:\\programdata",
            "c:\\system volume information",
            "c:\\$recycle.bin",
            "c:\\boot",
            "c:\\efi",
            "c:\\",
            "d:\\",
            "e:\\"
#else
            "/",
            "/usr",
            "/usr/bin",
            "/usr/sbin",
            "/usr/lib",
            "/usr/lib64",
            "/usr/include",
            "/usr/local",
            "/bin",
            "/sbin",
            "/lib",
            "/lib64",
            "/lib32",
            "/etc",
            "/etc/pam.d",
            "/etc/systemd",
            "/boot",
            "/sys",
            "/proc",
            "/dev",
            "/run",
            "/var/lib",
#ifdef __APPLE__
            "/system",
            "/system/library",
            "/system/driverkit",
            "/var/db",
            "/private/var/db",
            "/applications",
            "/library"
#endif
#endif
        };
    }

    // High-risk but conditionally blockable with --no-path-protection
    static std::vector<std::string> GetDangerousPaths() {
        std::vector<std::string> paths;
#ifdef _WIN32
        char* userProfile = nullptr;
        size_t len = 0;
        _dupenv_s(&userProfile, &len, "USERPROFILE");
        if (userProfile) {
            std::string up(userProfile);
            std::transform(up.begin(), up.end(), up.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            paths.push_back(up + "\\documents");
            paths.push_back(up + "\\desktop");
            paths.push_back(up + "\\pictures");
            paths.push_back(up + "\\music");
            paths.push_back(up + "\\videos");
            paths.push_back(up + "\\downloads");
            free(userProfile);
        }
        paths.push_back("c:\\users");
#else
        char* home = std::getenv("HOME");
        if (home) {
            std::string h(home);
            std::transform(h.begin(), h.end(), h.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            paths.push_back(h + "/documents");
            paths.push_back(h + "/desktop");
            paths.push_back(h + "/pictures");
            paths.push_back(h + "/music");
            paths.push_back(h + "/videos");
            paths.push_back(h + "/downloads");
        }
        paths.push_back("/home");
        paths.push_back("/root");
        paths.push_back("/var");
        paths.push_back("/opt");
#endif
        return paths;
    }
};
