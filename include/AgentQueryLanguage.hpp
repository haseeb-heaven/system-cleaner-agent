#pragma once

#include "Cleaner.hpp"
#include "ContentInspector.hpp"
#include "SmartScheduler.hpp"
#include "Logger.hpp"
#include "ProcessManager.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// Agent Query Language (AQL) Structure
// Example AQL Queries:
//   CLEAN 'C:\Users\hasee\AppData\Local\Temp' WHERE FREE_DISK < 500MB
//   SCAN 'C:\' WHERE SIZE > 10MB AND EXT IN ('.tmp', '.log')
//   SHRED 'D:\Temp' WHERE AGE > 1H
//   MONITOR WHERE RAM > 80% EVERY 15S
//   PURGE RECYCLE_BIN
struct AQLQuery {
    std::string rawQuery;
    std::string command = "CLEAN"; // SCAN, CLEAN, SHRED, MONITOR, PURGE
    std::vector<fs::path> targetPaths;
    fs::path drive = "C:\\";
    uintmax_t diskFreeBelowBytes = 0;
    double ramThresholdPercent = 0.0;
    double diskThresholdPercent = 0.0;
    uintmax_t minSizeBytes = 0;
    long long minAgeMinutes = 0;
    std::vector<std::string> exts;
    long long intervalSeconds = 15;
    bool isParsed = false;
    std::string parseError;
};

class AQLEngine {
public:
    static AQLQuery Parse(const std::string& queryStr) {
        AQLQuery query;
        query.rawQuery = queryStr;

        std::string upper = queryStr;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        // Command detection
        if (upper.find("KILL") == 0 || upper.find("KILL ") != std::string::npos || upper.find("TERMINATE") != std::string::npos) {
            query.command = "KILL";
        } else if (upper.find("CLEAN") == 0 || upper.find("CLEAN ") != std::string::npos) {
            query.command = "CLEAN";
        } else if (upper.find("SCAN") == 0 || upper.find("SCAN ") != std::string::npos || upper.find("SELECT") == 0) {
            query.command = "SCAN";
        } else if (upper.find("SHRED") == 0 || upper.find("SHRED ") != std::string::npos || upper.find("WIPE") != std::string::npos) {
            query.command = "SHRED";
        } else if (upper.find("MONITOR") == 0 || upper.find("MONITOR ") != std::string::npos || upper.find("WATCH") != std::string::npos) {
            query.command = "MONITOR";
        } else if (upper.find("PURGE") == 0 || upper.find("EMPTY") != std::string::npos || upper.find("TRASH") != std::string::npos || upper.find("RECYCLE") != std::string::npos) {
            query.command = "PURGE";
        } else {
            query.command = "CLEAN";
        }

        // Extract single-quoted paths 'C:\Temp' or 'C:\'
        size_t startQuote = 0;
        while ((startQuote = queryStr.find('\'', startQuote)) != std::string::npos) {
            size_t endQuote = queryStr.find('\'', startQuote + 1);
            if (endQuote != std::string::npos) {
                std::string pathStr = queryStr.substr(startQuote + 1, endQuote - startQuote - 1);
                if (!pathStr.empty()) {
                    if (pathStr.size() <= 3 && pathStr.find(':') != std::string::npos) {
                        query.drive = SmartScheduler::NormalizeDrivePath(pathStr);
                    } else if (pathStr.front() == '.' && pathStr.size() <= 6) {
                        query.exts.push_back(pathStr);
                    } else {
                        query.targetPaths.push_back(fs::path(pathStr));
                    }
                }
                startQuote = endQuote + 1;
            } else {
                break;
            }
        }

        // Extract WHERE conditions
        std::string lower = queryStr;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // FREE_DISK < 500MB / disk_free < 500mb / free < 500mb / 500mb
        if (lower.find("free_disk") != std::string::npos ||
            lower.find("disk_free") != std::string::npos ||
            lower.find("free") != std::string::npos ||
            lower.find("500") != std::string::npos) {
            size_t pos = lower.find("<");
            if (pos != std::string::npos) {
                std::string valStr = lower.substr(pos + 1);
                std::stringstream valSS(valStr);
                std::string token;
                if (valSS >> token) {
                    query.diskFreeBelowBytes = ContentInspector::ParseSizeToBytes(token);
                }
            } else {
                query.diskFreeBelowBytes = ContentInspector::ParseSizeToBytes("500mb");
            }
        }

        // RAM > 80% / memory > 80%
        if (lower.find("ram") != std::string::npos || lower.find("mem") != std::string::npos) {
            size_t pos = lower.find(">");
            if (pos != std::string::npos) {
                std::string valStr = lower.substr(pos + 1);
                if (!valStr.empty() && valStr.back() == '%') valStr.pop_back();
                std::stringstream valSS(valStr);
                double val = 0.0;
                if (valSS >> val) query.ramThresholdPercent = val;
            } else {
                query.ramThresholdPercent = 80.0;
            }
        }

        // SIZE > 10MB
        if (lower.find("size") != std::string::npos) {
            size_t pos = lower.find(">");
            if (pos != std::string::npos) {
                std::string valStr = lower.substr(pos + 1);
                std::stringstream valSS(valStr);
                std::string token;
                if (valSS >> token) {
                    query.minSizeBytes = ContentInspector::ParseSizeToBytes(token);
                }
            }
        }

        // AGE > 24H
        if (lower.find("age") != std::string::npos || lower.find("older") != std::string::npos) {
            size_t pos = lower.find(">");
            if (pos != std::string::npos) {
                std::string valStr = lower.substr(pos + 1);
                std::stringstream valSS(valStr);
                std::string token;
                if (valSS >> token) {
                    query.minAgeMinutes = ContentInspector::ParseDurationToMinutes(token);
                }
            }
        }

        // EVERY 15S / INTERVAL 15S
        if (lower.find("every") != std::string::npos || lower.find("interval") != std::string::npos) {
            size_t pos = lower.find("every");
            if (pos == std::string::npos) pos = lower.find("interval");
            std::string valStr = lower.substr(pos + 5);
            std::stringstream valSS(valStr);
            std::string token;
            if (valSS >> token) {
                query.intervalSeconds = ContentInspector::ParseDurationToMinutes(token) * 60;
                if (query.intervalSeconds <= 0) {
                    try { query.intervalSeconds = std::stoll(token); } catch (...) { query.intervalSeconds = 15; }
                }
            }
        }

        query.isParsed = true;
        return query;
    }

    struct ValidationResult {
        bool isValid = true;
        std::string errorMessage;
        std::string suggestedHint;
    };

    static ValidationResult Validate(const std::string& queryStr) {
        ValidationResult result;
        if (queryStr.empty()) {
            result.isValid = false;
            result.errorMessage = "Empty AQL query string.";
            result.suggestedHint = "CLEAN 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE FREE_DISK < 500MB";
            return result;
        }

        std::string upper = queryStr;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        static const std::vector<std::string> validCmds = {
            "CLEAN", "SCAN", "SHRED", "MONITOR", "PURGE", "KILL", "SELECT", "WIPE", "WATCH", "EMPTY"
        };

        bool cmdFound = false;
        for (const auto& cmd : validCmds) {
            if (upper.find(cmd) != std::string::npos) {
                cmdFound = true;
                break;
            }
        }

        if (!cmdFound) {
            result.isValid = false;
            result.errorMessage = "Unrecognized AQL command in query: '" + queryStr + "'";
            result.suggestedHint = "Valid AQL format: KILL PROCESS WHERE RAM > 70%  |  CLEAN WHERE FREE_DISK < 500MB";
            return result;
        }

        return result;
    }

    static void Execute(const AQLQuery& q, Cleaner& cleaner, bool dryRun = true) {
        ValidationResult valRes = Validate(q.rawQuery);
        if (!valRes.isValid) {
            std::cout << "\033[1;31m[AQL SYNTAX ERROR] " << valRes.errorMessage << "\033[0m\n";
            std::cout << "\033[1;33m[AQL HINT] " << valRes.suggestedHint << "\033[0m\n\n";
            return;
        }

        cleaner.SetDryRun(dryRun);

        std::cout << "\033[1;33m+--[ Agent Query Language (AQL) Execution ]-------------------------+\033[0m\n";
        std::cout << "\033[1;36m|  Query:   " << q.rawQuery << "\033[0m\n";
        std::cout << "\033[1;36m|  Command: " << q.command;
        if (q.diskFreeBelowBytes > 0) std::cout << "  |  FREE_DISK < " << Cleaner::FormatSize(q.diskFreeBelowBytes);
        if (q.ramThresholdPercent > 0) std::cout << "  |  RAM > " << static_cast<int>(q.ramThresholdPercent) << "%";
        if (q.minSizeBytes > 0) std::cout << "  |  SIZE > " << Cleaner::FormatSize(q.minSizeBytes);
        std::cout << "\033[0m\n";
        std::cout << "\033[1;33m+------------------------------------------------------------------+\033[0m\n\n";

        if (!q.targetPaths.empty()) {
            cleaner.SetCustomPaths(q.targetPaths);
        }

        if (q.command == "SCAN") {
            cleaner.SetDryRun(true);
            cleaner.Scan();
        } else if (q.command == "CLEAN") {
            if (q.diskFreeBelowBytes > 0) {
                uintmax_t curFree = SmartScheduler::GetDiskFreeBytes(q.drive);
                std::cout << "\033[1;32m[AQL] Evaluating FREE_DISK condition on " << q.drive.string()
                          << ": Available free space = " << Cleaner::FormatSize(curFree)
                          << " (Trigger threshold = " << Cleaner::FormatSize(q.diskFreeBelowBytes) << ")\033[0m\n";
                if (curFree <= q.diskFreeBelowBytes) {
                    std::cout << "\033[1;33m[AQL TRIGGER MATCHED] Free space (" << Cleaner::FormatSize(curFree)
                              << ") <= threshold (" << Cleaner::FormatSize(q.diskFreeBelowBytes) << ") -> Executing AQL Clean!\033[0m\n";
                    cleaner.Clean();
                } else {
                    std::cout << "\033[1;32m[AQL STATUS] Free space (" << Cleaner::FormatSize(curFree)
                              << ") is above threshold (" << Cleaner::FormatSize(q.diskFreeBelowBytes) << "). No cleanup required.\033[0m\n";
                }
            } else {
                cleaner.Clean();
            }
        } else if (q.command == "SHRED") {
            cleaner.SetMode(CleanMode::Shred);
            cleaner.Clean();
        } else if (q.command == "PURGE") {
            cleaner.SetEmptyRecycleBin(true);
            cleaner.EmptyWindowsRecycleBin();
        } else if (q.command == "KILL") {
            uintmax_t ramCutoff = 0;
            if (q.ramThresholdPercent > 0) {
                uintmax_t totalRam = SmartScheduler::GetTotalMemoryBytes();
                ramCutoff = static_cast<uintmax_t>((q.ramThresholdPercent / 100.0) * totalRam);
                std::cout << "\033[1;32m[AQL] Evaluating RAM % threshold: " << static_cast<int>(q.ramThresholdPercent)
                          << "% of " << Cleaner::FormatSize(totalRam) << " = " << Cleaner::FormatSize(ramCutoff) << "\033[0m\n";
            } else if (q.minSizeBytes > 0) {
                ramCutoff = q.minSizeBytes;
            } else {
                ramCutoff = 200ULL * 1024 * 1024;
            }
            ProcessManager::KillHighMemoryProcesses(ramCutoff, !dryRun);
        } else if (q.command == "MONITOR") {
            SmartScheduler::RunDaemonService(cleaner, {}, q.ramThresholdPercent, q.diskThresholdPercent, q.intervalSeconds, dryRun, q.diskFreeBelowBytes, q.drive);
        }
    }
};
