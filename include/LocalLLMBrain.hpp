#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>

struct GeneratedThought {
    std::string intent;
    std::vector<std::string> thoughts;
    std::vector<std::string> actions;
    double confidenceScore = 0.95;
};

class LocalLLMBrain {
public:
    static void StreamTokenText(const std::string& text, int delayMs = 8) {
        for (char c : text) {
            std::cout << c << std::flush;
            if (delayMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
        }
        std::cout << "\033[0m" << std::endl;
    }

    static GeneratedThought ReasonOnGoal(const std::string& userGoal) {
        GeneratedThought result;
        // Fix: use std::transform with begin iterator as output, not a returned string
        std::string lowerGoal = userGoal;
        std::transform(lowerGoal.begin(), lowerGoal.end(), lowerGoal.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (lowerGoal.find("shred") != std::string::npos ||
            lowerGoal.find("secure") != std::string::npos ||
            lowerGoal.find("wipe") != std::string::npos) {
            result.intent = "SECURE_SHRED_CLEANUP";
            result.thoughts = {
                "User requested secure data wipe strategy. Analyzing target paths for zero-overwrite shredding policy...",
                "Scanning file handles to prevent active process locks during shred operations...",
                "Executing multi-pass binary zero overwrite on matched temporary files..."
            };
            result.actions = {
                "SET_EXECUTION_MODE(mode=shred)",
                "RELEASE_PROCESS_LOCKS()",
                "EXECUTE_SECURE_SHRED_WIPE()"
            };
        } else if (lowerGoal.find("recycle") != std::string::npos ||
                   lowerGoal.find("trash") != std::string::npos) {
            result.intent = "PURGE_OS_TRASH";
            result.thoughts = {
                "User goal targets system Recycle Bin / Trash storage reclamation...",
                "Querying OS Shell API for Recycle Bin item count and total size...",
                "Invoking OS Shell purge routine without confirmation prompts..."
            };
            result.actions = {
                "INSPECT_RECYCLE_BIN_USAGE()",
                "EMPTY_OS_RECYCLE_BIN()"
            };
        } else if (lowerGoal.find("disk") != std::string::npos ||
                   lowerGoal.find("free") != std::string::npos ||
                   lowerGoal.find("less than") != std::string::npos ||
                   lowerGoal.find("below") != std::string::npos ||
                   lowerGoal.find("space") != std::string::npos ||
                   lowerGoal.find("500") != std::string::npos) {
            result.intent = "DISK_FREE_THRESHOLD_MONITOR";
            result.thoughts = {
                "User goal specifies drive free space threshold rule (e.g. clean cache if free space < 500 MB)...",
                "Querying OS filesystem storage status structures for drive capacity and available free bytes...",
                "Evaluating current free space against requested trigger threshold and preparing targeted cleanup..."
            };
            result.actions = {
                "INSPECT_SYSTEM_RESOURCES()",
                "EVALUATE_DISK_FREE_THRESHOLD()",
                "EXECUTE_TARGETED_CLEANUP()"
            };
        } else if (lowerGoal.find("mem") != std::string::npos ||
                   lowerGoal.find("ram") != std::string::npos ||
                   lowerGoal.find("threshold") != std::string::npos) {
            result.intent = "MEMORY_THRESHOLD_MONITOR";
            result.thoughts = {
                "User goal includes RAM / memory threshold parameters...",
                "Evaluating system RAM usage via native OS kernel status structures...",
                "Setting up active daemon monitoring to trigger cleanup automatically when threshold is breached..."
            };
            result.actions = {
                "INSPECT_SYSTEM_RESOURCES()",
                "MONITOR_MEMORY_THRESHOLD()"
            };
        } else {
            result.intent = "STORAGE_OPTIMIZATION";
            result.thoughts = {
                "Goal requests general storage optimization and temporary cache cleaning...",
                "Analyzing target drives and system cache locations for cleanable storage junk...",
                "Checking for background process handles locking target temporary directories...",
                "Executing Content Protection Shield to verify magic bytes and preserve user source code/documents...",
                "Executing autonomous parallel cleanup of verified junk cache locations..."
            };
            result.actions = {
                "INSPECT_SYSTEM_RESOURCES()",
                "SCAN_SYSTEM_TARGETS(mode=multi_threaded)",
                "RELEASE_PROCESS_LOCKS(targets=[mintty, cat, bash, werfault])",
                "INSPECT_FILE_MAGIC_BYTES(rules=[source_code, docs, db, images])",
                "EXECUTE_PARALLEL_CLEANUP(threads=auto)"
            };
        }

        return result;
    }
};
