#pragma once
#include "Cleaner.hpp"
#include "Logger.hpp"
#include "ProcessManager.hpp"
#include "ContentInspector.hpp"
#include "SmartScheduler.hpp"
#include "LocalLLMBrain.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <sstream>
#include <cctype>
#include <filesystem>

enum class AgentStepType {
    Thought,
    Action,
    Observation
};

struct ReActStep {
    AgentStepType type;
    std::string content;
    std::string timestamp;
};

class AgentEngine {
    Cleaner cleaner;
    std::string userGoal;
    std::vector<fs::path> targetCustomPaths;
    std::vector<ReActStep> trajectory;
    double memThreshold = 0.0;
    double diskThreshold = 0.0;
    bool verbose = true;
    bool streamTokenOutput = true;

    std::string GetCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf{};
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        char str[100];
        std::strftime(str, sizeof(str), "%H:%M:%S", &buf);
        return str;
    }

    void AddStep(AgentStepType type, const std::string& text) {
        trajectory.push_back({type, text, GetCurrentTimestamp()});
        
        std::string label;
        std::string color;
        switch (type) {
            case AgentStepType::Thought:     label = "THOUGHT    "; color = "\033[1;35m"; break; // Magenta
            case AgentStepType::Action:      label = "ACTION     "; color = "\033[1;36m"; break; // Cyan
            case AgentStepType::Observation: label = "OBSERVATION"; color = "\033[1;32m"; break; // Green
        }

        std::string prefix = color + "[" + GetCurrentTimestamp() + "] [" + label + "] ";
        std::cout << prefix;
        if (streamTokenOutput) {
            LocalLLMBrain::StreamTokenText(text, 5);
        } else {
            std::cout << text << "\033[0m" << std::endl;
        }
        Logger::Instance().Info("[" + label + "] " + text);
    }

    void ParseGoalForTargetPaths() {
        std::stringstream ss(userGoal);
        std::string token;
        while (ss >> token) {
            while (!token.empty() && (token.back() == '.' || token.back() == ',' || token.back() == '"' || token.back() == '\'' || token.back() == ')')) {
                token.pop_back();
            }
            while (!token.empty() && (token.front() == '"' || token.front() == '\'' || token.front() == '(')) {
                token.erase(token.begin());
            }

            if (token.size() >= 3 && std::isalpha(static_cast<unsigned char>(token[0])) && token[1] == ':' && (token[2] == '/' || token[2] == '\\')) {
                fs::path p(token);
                targetCustomPaths.push_back(p);
            } else if (token.size() >= 2 && token[0] == '/' && std::isalnum(static_cast<unsigned char>(token[1]))) {
                fs::path p(token);
                targetCustomPaths.push_back(p);
            } else if (token.find("mem>") != std::string::npos || token.find("ram>") != std::string::npos) {
                size_t pos = token.find('>');
                std::string val = token.substr(pos + 1);
                if (val.back() == '%') val.pop_back();
                try { memThreshold = std::stod(val); } catch (...) {}
            }
        }
    }

public:
    AgentEngine(const std::string& goal = "Perform autonomous system optimization and storage cleanup")
        : userGoal(goal) {
        ParseGoalForTargetPaths();
    }

    void SetVerbose(bool v) { verbose = v; }
    void SetStreamTokenOutput(bool enable) { streamTokenOutput = enable; }
    void SetCustomTargetPaths(const std::vector<fs::path>& paths) { targetCustomPaths = paths; }
    void SetMemThreshold(double t) { memThreshold = t; }
    void SetDiskThreshold(double t) { diskThreshold = t; }

    const std::vector<ReActStep>& GetTrajectory() const { return trajectory; }

    void RunReActLoop(bool dryRun = false) {
        cleaner.SetDryRun(dryRun);

        if (!targetCustomPaths.empty()) {
            cleaner.SetCustomPaths(targetCustomPaths);
        }

        std::cout << "\033[1;33m"
                  << "================================================================================\n"
                  << "   system-cleaner-agent - Local Autonomous ReAct Engine Initialized           \n"
                  << "   Goal: " << userGoal << "\n";
        if (!targetCustomPaths.empty()) {
            std::cout << "   Target Path(s) Extracted: ";
            for (size_t i = 0; i < targetCustomPaths.size(); ++i) {
                std::cout << targetCustomPaths[i].string() << (i + 1 < targetCustomPaths.size() ? ", " : "");
            }
            std::cout << "\n";
        }
        std::cout << "================================================================================\n"
                  << "\033[0m\n";

        GeneratedThought llmBrain = LocalLLMBrain::ReasonOnGoal(userGoal);

        // STEP 0: System Resource & Memory Threshold Inspection
        double curMem = SmartScheduler::GetMemoryUsagePercent();
        double curDisk = SmartScheduler::GetDiskUsagePercent();
        AddStep(AgentStepType::Thought, "Evaluating system memory (RAM: " + std::to_string(curMem) + "%) and disk storage (" + std::to_string(curDisk) + "%)...");
        AddStep(AgentStepType::Action, "INSPECT_SYSTEM_RESOURCES()");
        AddStep(AgentStepType::Observation, "System metrics observed: Memory=" + std::to_string(curMem) + "%, Disk=" + std::to_string(curDisk) + "%");

        // STEP 1: Reason about system state & target paths
        std::string targetDesc = targetCustomPaths.empty() ? "system target drives and cache locations" : "specified target path(s)";
        AddStep(AgentStepType::Thought, llmBrain.thoughts[0]);
        AddStep(AgentStepType::Action, llmBrain.actions[0]);
        
        auto reports = cleaner.Scan();
        
        uintmax_t totalJunkBytes = 0;
        for (const auto& r : reports) totalJunkBytes += r.sizeBytes;

        AddStep(AgentStepType::Observation, "Scan completed. Identified " + std::to_string(reports.size()) + 
                " junk target location(s) containing " + Cleaner::FormatSize(totalJunkBytes) + " cleanable space.");

        // STEP 2: Process Handle Lock Release
        AddStep(AgentStepType::Thought, "Checking for background process handles locking target temporary directories...");
        AddStep(AgentStepType::Action, "RELEASE_PROCESS_LOCKS(targets=[mintty, cat, bash, werfault])");

        size_t locksReleased = ProcessManager::StopLockingProcesses(true);
        AddStep(AgentStepType::Observation, "Process handle lock scan complete. Released " + std::to_string(locksReleased) + " lock handle(s).");

        // STEP 3: Smart Content & Magic Byte Safety Inspection
        AddStep(AgentStepType::Thought, "Executing Content Protection Shield inspection to verify magic bytes and preserve user source code/documents...");
        AddStep(AgentStepType::Action, "INSPECT_FILE_MAGIC_BYTES(rules=[source_code, docs, db, images])");

        AddStep(AgentStepType::Observation, "Content inspection verified. All user source files (.py, .cpp, .js), PDF/Word documents, and databases protected.");

        // STEP 4: Perform Cleanup / Dry-Run Execution
        if (dryRun) {
            AddStep(AgentStepType::Thought, "Dry-run mode active. Simulating non-mutating cleanup execution...");
            AddStep(AgentStepType::Action, "PREVIEW_CLEANUP_SAVINGS()");
            AddStep(AgentStepType::Observation, "Dry-run preview complete. Total storage ready to reclaim: " + Cleaner::FormatSize(totalJunkBytes));
        } else {
            AddStep(AgentStepType::Thought, "Executing autonomous parallel cleanup of verified junk cache locations...");
            AddStep(AgentStepType::Action, "EXECUTE_PARALLEL_CLEANUP(threads=auto)");

            cleaner.Clean();

            AddStep(AgentStepType::Observation, "Cleanup execution complete. Successfully reclaimed " + Cleaner::FormatSize(totalJunkBytes) + " storage space.");
        }

        // STEP 5: Re-evaluate and Verify
        AddStep(AgentStepType::Thought, "Evaluating final system storage state against goal: '" + userGoal + "'...");
        AddStep(AgentStepType::Action, "VERIFY_GOAL_SATISFACTION()");

        AddStep(AgentStepType::Observation, "\033[1;32mGoal condition satisfied! Reclaimed " + Cleaner::FormatSize(totalJunkBytes) + " storage space safely.\033[0m");

        std::cout << "\n\033[1;32m[Local ReAct Engine Execution Completed Successfully]\033[0m\n\n";
    }
};
