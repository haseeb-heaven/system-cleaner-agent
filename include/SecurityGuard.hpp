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
            case ThreatLevel::Critical:    color = "\033[1;31m"; label = "[CRITICAL]  "; break;
            case ThreatLevel::Dangerous:   color = "\033[1;33m"; label = "[DANGEROUS] "; break;
            case ThreatLevel::Suspicious:  color = "\033[1;36m"; label = "[SANDBOX]   "; break;
            default:                       color = "\033[1;32m"; label = "[SAFE]      "; break;
        }
        std::cout << color << label << report.reason << "\033[0m\n";
    }

private:
    // Always blocked — these will never be allowed regardless of sandbox flag
    static std::vector<std::string> GetCriticalBlockedPaths() {
        return {
#ifdef _WIN32
            "c:\\windows",
            "c:\\windows\\system32",
            "c:\\windows\\syswow64",
            "c:\\program files",
            "c:\\program files (x86)",
            "c:\\programdata",
            "c:\\",
            "d:\\",
#else
            "/",
            "/usr",
            "/usr/bin",
            "/usr/lib",
            "/usr/local",
            "/bin",
            "/sbin",
            "/lib",
            "/lib64",
            "/etc",
            "/boot",
            "/sys",
            "/proc",
            "/dev",
            "/run",
#ifdef __APPLE__
            "/system",
            "/library",
            "/applications",
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
