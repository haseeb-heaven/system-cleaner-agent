#pragma once

#include "Cleaner.hpp"
#include "ContentInspector.hpp"
#include "SmartScheduler.hpp"
#include "Logger.hpp"
#include "ProcessManager.hpp"
#include "TaskHistory.hpp"


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

        // Parse unquoted target process/folder tokens (e.g. SELECT chrome.exe FROM PROCESS, KILL notepad.exe)
        std::stringstream ss(queryStr);
        std::string token;
        while (ss >> token) {
            std::string tLower = token;
            std::transform(tLower.begin(), tLower.end(), tLower.begin(), ::tolower);
            if (tLower.length() >= 4 && tLower.substr(tLower.length() - 4) == ".exe") {
                query.targetPaths.push_back(fs::path(token));
            }
        }

        // Expand safe OS aliases (TEMP_C, TEMP_D, TEMP_X, APPDATA, CACHE) across Windows, Linux, macOS
#ifdef _WIN32
        char userProfile[MAX_PATH] = {};
        char winDir[MAX_PATH] = {};
        ExpandEnvironmentStringsA("%USERPROFILE%", userProfile, MAX_PATH);
        ExpandEnvironmentStringsA("%WINDIR%", winDir, MAX_PATH);
        std::string up(userProfile);
        std::string wd(winDir);

        if (upper.find("TEMP_C") != std::string::npos) {
            query.targetPaths.push_back(fs::path(up + "\\AppData\\Local\\Temp"));
            query.targetPaths.push_back(fs::path(wd + "\\Temp"));
        }
        if (upper.find("TEMP_D") != std::string::npos) {
            query.targetPaths.push_back(fs::path("D:\\Temp"));
            query.targetPaths.push_back(fs::path("D:\\Cache"));
        }
        if (upper.find("APPDATA") != std::string::npos) {
            query.targetPaths.push_back(fs::path(up + "\\AppData\\Local\\Temp"));
            query.targetPaths.push_back(fs::path(up + "\\AppData\\Local\\Caches"));
            query.targetPaths.push_back(fs::path(up + "\\AppData\\Roaming\\npm-cache"));
        }
        if (upper.find("CACHE") != std::string::npos) {
            query.targetPaths.push_back(fs::path(up + "\\AppData\\Local\\Google\\Chrome\\User Data\\Default\\Cache"));
            query.targetPaths.push_back(fs::path(up + "\\AppData\\Local\\Microsoft\\Edge\\User Data\\Default\\Cache"));
            query.targetPaths.push_back(fs::path(up + "\\AppData\\Local\\Mozilla\\Firefox\\Profiles"));
        }
#else
        char* home = std::getenv("HOME");
        std::string h = home ? std::string(home) : "/tmp";
        if (upper.find("TEMP_C") != std::string::npos || upper.find("TEMP") != std::string::npos) {
            query.targetPaths.push_back(fs::path("/tmp"));
            query.targetPaths.push_back(fs::path("/var/tmp"));
        }
        if (upper.find("APPDATA") != std::string::npos || upper.find("CACHE") != std::string::npos) {
            query.targetPaths.push_back(fs::path(h + "/.cache"));
#ifdef __APPLE__
            query.targetPaths.push_back(fs::path(h + "/Library/Caches"));
#endif
        }
#endif

        // Extract WHERE conditions
        std::string lower = queryStr;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // FREE_DISK < 500MB / DISK_C < 500MB / disk_free < 500mb / 500mb
        if (lower.find("free_disk") != std::string::npos ||
            lower.find("disk_free") != std::string::npos ||
            lower.find("disk_c") != std::string::npos ||
            lower.find("disk_d") != std::string::npos ||
            lower.find("free") != std::string::npos ||
            lower.find("500") != std::string::npos) {
            size_t pos = lower.find("<");
            if (pos != std::string::npos) {
                std::string valStr = lower.substr(pos + 1);
                std::stringstream valSS(valStr);
                std::string tToken;
                if (valSS >> tToken) {
                    query.diskFreeBelowBytes = ContentInspector::ParseSizeToBytes(tToken);
                }
            } else {
                query.diskFreeBelowBytes = ContentInspector::ParseSizeToBytes("500mb");
            }
        }

        // RAM > 80% / RAM > 200MB / memory > 80%
        if (lower.find("ram") != std::string::npos || lower.find("mem") != std::string::npos) {
            size_t pos = lower.find(">");
            if (pos != std::string::npos) {
                std::string valStr = lower.substr(pos + 1);
                std::stringstream valSS(valStr);
                std::string valToken;
                if (valSS >> valToken) {
                    if (valToken.back() == '%') {
                        valToken.pop_back();
                        try { query.ramThresholdPercent = std::stod(valToken); } catch (...) { query.ramThresholdPercent = 80.0; }
                    } else {
                        query.minSizeBytes = ContentInspector::ParseSizeToBytes(valToken);
                    }
                }
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
                std::string tToken;
                if (valSS >> tToken) {
                    query.minSizeBytes = ContentInspector::ParseSizeToBytes(tToken);
                }
            }
        }

        // AGE > 24H
        if (lower.find("age") != std::string::npos || lower.find("older") != std::string::npos) {
            size_t pos = lower.find(">");
            if (pos != std::string::npos) {
                std::string valStr = lower.substr(pos + 1);
                std::stringstream valSS(valStr);
                std::string tToken;
                if (valSS >> tToken) {
                    query.minAgeMinutes = ContentInspector::ParseDurationToMinutes(tToken);
                }
            }
        }

        // EVERY 15S / INTERVAL 15S
        if (lower.find("every") != std::string::npos || lower.find("interval") != std::string::npos) {
            size_t pos = lower.find("every");
            if (pos == std::string::npos) pos = lower.find("interval");
            std::string valStr = lower.substr(pos + 5);
            std::stringstream valSS(valStr);
            std::string tToken;
            if (valSS >> tToken) {
                query.intervalSeconds = ContentInspector::ParseDurationToMinutes(tToken) * 60;
                if (query.intervalSeconds <= 0) {
                    try { query.intervalSeconds = std::stoll(tToken); } catch (...) { query.intervalSeconds = 15; }
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
            result.suggestedHint = "Examples:\n  - SELECT chrome.exe FROM PROCESS\n  - KILL notepad.exe FROM PROCESS WHERE RAM > 200MB\n  - CLEAN TEMP_C WHERE DISK_C < 500MB";
            return result;
        }

        std::string upper = queryStr;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        std::stringstream ss(upper);
        std::string firstWord;
        ss >> firstWord;

        std::string cleanWord = "";
        for (char ch : firstWord) {
            if (std::isalpha(static_cast<unsigned char>(ch))) cleanWord += ch;
        }

        static const std::set<std::string> validCmds = {
            "CLEAN", "SCAN", "SHRED", "MONITOR", "PURGE", "KILL", "SELECT", "WIPE", "WATCH", "EMPTY"
        };

        if (validCmds.count(cleanWord) > 0) {
            firstWord = cleanWord;
        }

        if (validCmds.count(firstWord) == 0) {
            result.isValid = false;
            result.errorMessage = "Unrecognized AQL command verb '" + firstWord + "'. Query MUST start with CLEAN, SCAN, SHRED, PURGE, KILL, SELECT, MONITOR, or WIPE.";
            result.suggestedHint = "Correct Syntax:\n  - SELECT chrome.exe FROM PROCESS\n  - KILL notepad.exe FROM PROCESS WHERE RAM > 200MB\n  - CLEAN TEMP_C WHERE DISK_C < 500MB";
            return result;
        }

        if (firstWord == "KILL") {
            bool hasTargetProc = (upper.find(".EXE") != std::string::npos || upper.find("'") != std::string::npos || upper.find("PROCESS") != std::string::npos);
            if (!hasTargetProc) {
                result.isValid = false;
                result.errorMessage = "Invalid AQL KILL query. Target process (e.g. chrome.exe or 'notepad.exe') or 'FROM PROCESS' clause missing.";
                result.suggestedHint = "Correct Syntax:\n  - KILL chrome.exe WHERE RAM > 200MB\n  - KILL notepad.exe FROM PROCESS WHERE RAM > 80%";
                return result;
            }
        } else if (firstWord == "SELECT") {
            bool hasSource = (upper.find("FROM") != std::string::npos || upper.find(".EXE") != std::string::npos || upper.find("'") != std::string::npos);
            if (!hasSource) {
                result.isValid = false;
                result.errorMessage = "Invalid AQL SELECT query. Source clause (FROM PROCESS or FROM STORAGE) missing.";
                result.suggestedHint = "Correct Syntax:\n  - SELECT chrome.exe FROM PROCESS\n  - SELECT 'C:\\Temp' FROM STORAGE";
                return result;
            }
        } else if (firstWord == "CLEAN" || firstWord == "SHRED" || firstWord == "WIPE") {
            bool hasPathOrAlias = (upper.find("'") != std::string::npos ||
                                   upper.find("TEMP") != std::string::npos ||
                                   upper.find("APPDATA") != std::string::npos ||
                                   upper.find("CACHE") != std::string::npos ||
                                   upper.find("RECYCLE") != std::string::npos);
            if (!hasPathOrAlias) {
                result.isValid = false;
                result.errorMessage = "Invalid AQL " + firstWord + " query. Single-quoted path '...' or valid alias (TEMP_C, APPDATA, CACHE, RECYCLE_BIN) missing.";
                result.suggestedHint = "Correct Syntax:\n  - CLEAN 'C:\\Users\\hasee\\AppData\\Local\\Temp' WHERE FREE_DISK < 500MB\n  - CLEAN TEMP_C WHERE DISK_C < 500MB\n  - CLEAN APPDATA WHERE DISK_FREE < 1GB\n  - SHRED 'C:\\Temp' WHERE SIZE > 10MB";
                return result;
            }
        }

        return result;
    }

    static void Execute(const AQLQuery& q, Cleaner& cleaner, bool dryRun = true) {
        ValidationResult valRes = Validate(q.rawQuery);
        if (!valRes.isValid) {
            // Route through Logger (TUI-safe: suppressed in console when the
            // TUI is active) and record a Failed task in the library so the
            // error is visible to the user in both the log file and the Task
            // Library, without corrupting a live menu frame.
            Logger::Instance().Error(std::string("[AQL SYNTAX ERROR] ") + valRes.errorMessage);
            Logger::Instance().Warn(std::string("[AQL HINT]\n") + valRes.suggestedHint);
            uint64_t errId = TaskHistory::Instance().Register("AQL", "AQL Syntax Error", q.rawQuery, "AQL Engine");
            TaskHistory::Instance().MarkFailed(errId, "AQL SYNTAX ERROR: " + valRes.errorMessage);
            return;
        }

        cleaner.SetDryRun(dryRun);

        // Register this query in the unified Task Library
        std::string aqlName = "AQL " + q.command;
        if (!q.targetPaths.empty()) {
            aqlName += " (" + q.targetPaths.front().string() + ")";
        }
        uint64_t taskId = TaskHistory::Instance().Register("AQL", aqlName, q.rawQuery, "AQL Engine");
        TaskHistory::Instance().MarkRunning(taskId);
        TaskHistory::Instance().UpdateProgress(taskId, "Executing AQL " + q.command + "...");

        bool isFutureCronTrigger = (q.rawQuery.find("WHEN") != std::string::npos ||
                                    q.rawQuery.find("when") != std::string::npos ||
                                    q.rawQuery.find("EVERY") != std::string::npos ||
                                    q.rawQuery.find("every") != std::string::npos ||
                                    q.command == "MONITOR");

        std::cout << "\033[1;33m+--[ Agent Query Language (AQL) Execution ]-------------------------+\033[0m\n";
        std::cout << "\033[1;36m|  Query:   " << q.rawQuery << "\033[0m\n";
        std::cout << "\033[1;36m|  Command: " << q.command;
        if (q.diskFreeBelowBytes > 0) std::cout << "  |  FREE_DISK < " << Cleaner::FormatSize(q.diskFreeBelowBytes);
        if (q.ramThresholdPercent > 0) std::cout << "  |  RAM > " << static_cast<int>(q.ramThresholdPercent) << "%";
        if (q.minSizeBytes > 0) std::cout << "  |  RAM Cutoff > " << Cleaner::FormatSize(q.minSizeBytes);
        std::cout << "\033[0m\n";
        std::cout << "\033[1;33m+------------------------------------------------------------------+\033[0m\n\n";

        if (isFutureCronTrigger) {
            std::cout << "\033[1;36m[AQL CRON DAEMON ACTIVATED] Query contains future trigger 'WHEN' -> Registered as continuous Cron Daemon Job.\033[0m\n";
            TaskHistory::Instance().UpdateProgress(taskId, "Cron Daemon Watching: " + q.rawQuery);
            Logger::Instance().Info("[AQL Cron Daemon] Activated future trigger for: " + q.rawQuery);

            Cleaner* cleanerPtr = &cleaner;
            auto cronTask = [q, cleanerPtr, dryRun, taskId]() {
                while (true) {
                    TaskEntry currentTask;
                    if (!TaskHistory::Instance().GetTask(taskId, currentTask) ||
                        currentTask.status == TaskStatus::Cancelled ||
                        currentTask.status == TaskStatus::Failed) {
                        break;
                    }

                    bool conditionMet = false;
                    if (q.ramThresholdPercent > 0.0) {
                        double curRam = SmartScheduler::GetMemoryUsagePercent();
                        if (curRam >= q.ramThresholdPercent) conditionMet = true;
                    }
                    if (q.diskFreeBelowBytes > 0) {
                        uintmax_t curFree = SmartScheduler::GetDiskFreeBytes(q.drive);
                        if (curFree <= q.diskFreeBelowBytes) conditionMet = true;
                    }
                    if (q.minSizeBytes > 0) {
                        if (!q.targetPaths.empty()) {
                            auto groups = ProcessManager::GetAggregatedProcessGroups(0);
                            uintmax_t procRam = 0;
                            for (const auto& tp : q.targetPaths) {
                                std::string tpLower = tp.string();
                                std::transform(tpLower.begin(), tpLower.end(), tpLower.begin(), ::tolower);
                                for (const auto& grp : groups) {
                                    std::string gLower = grp.processName;
                                    std::transform(gLower.begin(), gLower.end(), gLower.begin(), ::tolower);
                                    if (gLower.find(tpLower) != std::string::npos || tpLower.find(gLower) != std::string::npos) {
                                        procRam += grp.totalMemoryUsageBytes;
                                    }
                                }
                            }
                            if (procRam >= q.minSizeBytes) conditionMet = true;
                        } else {
                            auto candidates = ProcessManager::GetHighMemoryCandidates(q.minSizeBytes);
                            if (!candidates.empty()) conditionMet = true;
                        }
                    }

                    if (conditionMet) {
                        Logger::Instance().Info("[AQL CRON TRIGGER MATCHED] Executing action for: " + q.rawQuery);
                        if (q.command == "KILL") {
                            if (!q.targetPaths.empty()) {
                                for (const auto& tp : q.targetPaths) {
                                    size_t kCount = GTLIBC::GTLibc::KillProcessByName(tp.string(), !dryRun, true);
                                    Logger::Instance().Info("[AQL CRON PROCESS KILL] Terminated " + std::to_string(kCount) + " instance(s) of " + tp.string());
                                }
                            } else {
                                bool allowKill = false;
                                size_t kCount = ProcessManager::KillHighMemoryProcesses(q.minSizeBytes > 0 ? q.minSizeBytes : (200ULL * 1024 * 1024), !dryRun, allowKill);
                                Logger::Instance().Info("[AQL CRON PROCESS KILL] High-RAM process cleanup killed " + std::to_string(kCount) + " process(es)");
                            }
                        } else if (q.command == "CLEAN") {
                            if (cleanerPtr) cleanerPtr->Clean();
                        } else if (q.command == "PURGE" || q.command == "EMPTY") {
                            if (cleanerPtr) cleanerPtr->EmptyWindowsRecycleBin();
                        }
                        TaskHistory::Instance().MarkCompleted(taskId, "Cron trigger executed for: " + q.rawQuery);
                        if (q.rawQuery.find("EVERY") == std::string::npos && q.rawQuery.find("every") == std::string::npos) {
                            break;
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(q.intervalSeconds > 0 ? q.intervalSeconds : 3));
                }
            };
            std::thread cronThread(cronTask);
            cronThread.detach();
            return;
        }

        if (q.command == "SELECT" || q.command == "SCAN") {
            if (!q.targetPaths.empty()) {
                for (const auto& tp : q.targetPaths) {
                    std::string target = tp.string();
                    std::string tLower = target;
                    std::transform(tLower.begin(), tLower.end(), tLower.begin(), ::tolower);
                    if (tLower.find(".exe") != std::string::npos || q.rawQuery.find("PROCESS") != std::string::npos) {
                        auto groups = ProcessManager::GetAggregatedProcessGroups(0);
                        bool found = false;
                        for (const auto& grp : groups) {
                            std::string gLower = grp.processName;
                            std::transform(gLower.begin(), gLower.end(), gLower.begin(), ::tolower);
                            if (gLower.find(tLower) != std::string::npos || tLower.find(gLower) != std::string::npos) {
                                found = true;
                                std::cout << "\033[1;32m[AQL SELECT PROCESS] " << grp.processName << " (" << grp.instanceCount << " procs) - Total RAM: " << Cleaner::FormatSize(grp.totalMemoryUsageBytes) << "\033[0m\n";
                            }
                        }
                        if (!found) {
                            std::cout << "\033[1;33m[AQL SELECT PROCESS] No running process matching '" << target << "' detected.\033[0m\n";
                        }
                    } else {
                        cleaner.SetCustomPaths({tp});
                        cleaner.SetDryRun(true);
                        cleaner.Scan();
                    }
                }
            } else {
                cleaner.SetDryRun(true);
                cleaner.Scan();
            }
            TaskHistory::Instance().MarkCompleted(taskId, "AQL " + q.command + " finished.", 0, 0, 0, 0);
        } else if (q.command == "CLEAN") {
            if (!q.targetPaths.empty()) {
                cleaner.SetCustomPaths(q.targetPaths);
            }
            if (q.diskFreeBelowBytes > 0) {
                uintmax_t curFree = SmartScheduler::GetDiskFreeBytes(q.drive);
                std::cout << "\033[1;32m[AQL] Evaluating DISK_FREE condition on " << q.drive.string()
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
            TaskHistory::Instance().MarkCompleted(taskId, "AQL CLEAN finished.", 0, 0, 0, 0);
        } else if (q.command == "SHRED" || q.command == "WIPE") {
            if (!q.targetPaths.empty()) cleaner.SetCustomPaths(q.targetPaths);
            cleaner.SetMode(CleanMode::Shred);
            cleaner.Clean();
            TaskHistory::Instance().MarkCompleted(taskId, "AQL SHRED finished.", 0, 0, 0, 0);
        } else if (q.command == "PURGE" || q.command == "EMPTY") {
            cleaner.SetEmptyRecycleBin(true);
            cleaner.EmptyWindowsRecycleBin();
            TaskHistory::Instance().MarkCompleted(taskId, "AQL PURGE finished.", 0, 0, 0, 0);
        } else if (q.command == "KILL") {
            if (!q.targetPaths.empty()) {
                for (const auto& tp : q.targetPaths) {
                    std::string targetProc = tp.string();
                    std::string tpLower = targetProc;
                    std::transform(tpLower.begin(), tpLower.end(), tpLower.begin(), ::tolower);

                    // Check per-process RAM threshold (WHERE RAM > 200MB)
                    if (q.minSizeBytes > 0) {
                        auto groups = ProcessManager::GetAggregatedProcessGroups(0);
                        uintmax_t procRam = 0;
                        for (const auto& grp : groups) {
                            std::string gLower = grp.processName;
                            std::transform(gLower.begin(), gLower.end(), gLower.begin(), ::tolower);
                            if (gLower.find(tpLower) != std::string::npos || tpLower.find(gLower) != std::string::npos) {
                                procRam += grp.totalMemoryUsageBytes;
                            }
                        }
                        if (procRam < q.minSizeBytes) {
                            std::cout << "\033[1;33m[AQL RAM GUARD] Process '" << targetProc << "' RAM (" << Cleaner::FormatSize(procRam)
                                      << ") is below threshold (" << Cleaner::FormatSize(q.minSizeBytes) << "). Skipped.\033[0m\n";
                            TaskHistory::Instance().MarkCompleted(taskId, "RAM " + Cleaner::FormatSize(procRam) + " < " + Cleaner::FormatSize(q.minSizeBytes) + ". Skipped.", 0, 0, 0, 0);
                            return;
                        }
                    }

                    // Check system RAM percentage threshold (WHERE RAM > 80%)
                    if (q.ramThresholdPercent > 0.0) {
                        double curRam = SmartScheduler::GetMemoryUsagePercent();
                        if (curRam < q.ramThresholdPercent) {
                            std::cout << "\033[1;33m[AQL RAM GUARD] System RAM (" << static_cast<int>(curRam)
                                      << "%) is below threshold (" << static_cast<int>(q.ramThresholdPercent) << "%). Skipped.\033[0m\n";
                            TaskHistory::Instance().MarkCompleted(taskId, "RAM " + std::to_string((int)curRam) + "% < " + std::to_string((int)q.ramThresholdPercent) + "%. Skipped.", 0, 0, 0, 0);
                            return;
                        }
                    }

                    std::cout << "\033[1;36m[AQL PROCESS KILL] Terminating target process: " << targetProc << "...\033[0m\n";
                    size_t kCount = GTLIBC::GTLibc::KillProcessByName(targetProc, !dryRun, true);
                    if (kCount > 0) {
                        std::cout << "\033[1;32m[AQL SUCCESS] Terminated " << kCount << " instance(s) of " << targetProc << "\033[0m\n";
                        TaskHistory::Instance().MarkCompleted(taskId, "Terminated " + std::to_string(kCount) + " instance(s) of " + targetProc, 0, 0, 0, kCount);
                    } else {
                        std::cout << "\033[1;33m[AQL NOTICE] No running processes matched target: " << targetProc << "\033[0m\n";
                        TaskHistory::Instance().MarkCompleted(taskId, "No active processes matched " + targetProc, 0, 0, 0, 0);
                    }
                }
            } else {
                uintmax_t ramCutoff = (q.minSizeBytes > 0) ? q.minSizeBytes : (200ULL * 1024 * 1024);
                auto candidates = ProcessManager::GetHighMemoryCandidates(ramCutoff);
                std::cout << "\033[1;36m[AQL PROCESS SCAN] Found " << candidates.size() << " process(es) matching RAM threshold.\033[0m\n";
                for (const auto& proc : candidates) {
                    std::string statusStr = proc.isProtected ? "\033[1;32m[PROTECTED APP - PRESERVED]\033[0m" : "\033[1;33m[PERMISSION REQUIRED]\033[0m";
                    std::cout << "  - " << proc.processName << " (PID: " << proc.pid << ", RAM: " << Cleaner::FormatSize(proc.memoryUsageBytes) << ") -> " << statusStr << "\n";
                }
                bool allowKill = false;
                ProcessManager::KillHighMemoryProcesses(ramCutoff, !dryRun, allowKill);
                TaskHistory::Instance().MarkCompleted(taskId, "AQL KILL high-RAM scan finished.", 0, 0, 0, 0);
            }
        } else if (q.command == "MONITOR") {
            SmartScheduler::RunDaemonService(cleaner, {}, q.ramThresholdPercent, q.diskThresholdPercent, q.intervalSeconds, dryRun, q.diskFreeBelowBytes, q.drive);
            TaskHistory::Instance().MarkCompleted(taskId, "AQL MONITOR service finished.", 0, 0, 0, 0);
        }
    }
};
