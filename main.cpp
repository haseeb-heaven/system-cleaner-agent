#include "include/Cleaner.hpp"
#include "include/Logger.hpp"
#include "include/ProcessManager.hpp"
#include "include/ContentInspector.hpp"
#include "include/ConfigManager.hpp"
#include "include/TUI.hpp"
#include "include/AgentEngine.hpp"
#include "include/SmartScheduler.hpp"
#include "include/LocalLLMBrain.hpp"
#include "include/SecurityGuard.hpp"
#include "include/AgentQueryLanguage.hpp"
#include "include/TaskHistory.hpp"
#include "include/DeepScanner.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>
#include <sstream>

void PrintHeader() {
    TUI::PrintBanner();
}

void PrintHelp() {
    PrintHeader();
    std::cout << "\033[1mUSAGE:\033[0m\n"
              << "  system-cleaner-agent [COMMAND] [FLAGS]\n\n"
              << "\033[1mCOMMANDS:\033[0m\n"
              << "  \033[36mchat\033[0m         Launch Interactive Local LLM ReAct Prompt Shell (try 'menu' for queries).\n"
              << "  \033[36magent\033[0m        Launch Autonomous ReAct Agent Loop (Thought->Action->Observation).\n"
              << "  \033[36mdaemon\033[0m       Run Smart Background Scheduler & Memory Threshold Monitoring Daemon.\n"
              << "  \033[36mscan\033[0m         Analyze system/drive targets and report cleanable storage.\n"
              << "  \033[36mclean\033[0m        Execute multi-threaded cleanup using active policy rules.\n"
              << "  \033[36mdeep-clean\033[0m   Perform full system cache cleanup + empty OS Recycle Bin / Trash.\n"
              << "  \033[36maql\033[0m          Execute a single Agent Query Language (AQL) statement (e.g. aql \"CLEAN ...\").\n"
              << "  \033[36mhistory\033[0m      Show the unified Task Library (all clean/scan/AQL/agent/daemon tasks).\n"
              << "  \033[36mtui\033[0m          Launch interactive Terminal User Interface (TUI) - default mode.\n"
              << "  \033[36mtest\033[0m         Execute automated engine diagnostic & unit test suite.\n"
              << "  \033[36mversion\033[0m      Display version, engine build, and architecture details.\n"
              << "  \033[36mhelp\033[0m         Show this help and usage specification.\n\n"
              << "\033[1mSMART SCHEDULER & THRESHOLD MONITORING:\033[0m\n"
              << "  \033[33m--mem-threshold <pct>\033[0m Automatic cleanup trigger when system RAM exceeds percentage (e.g. 80%).\n"
              << "  \033[33m--disk-threshold <pct>\033[0m Automatic cleanup trigger when Disk space exceeds percentage (e.g. 90%).\n"
              << "  \033[33m--schedule <rule>\033[0m     Set folder-specific rule (e.g. \"D:/Temp:15m:mem>80%\").\n"
              << "  \033[33m--interval <dur>\033[0m      Check interval duration for daemon monitor (default: 15s).\n\n"
              << "\033[1mAGENTIC REACT OPTIONS:\033[0m\n"
              << "  \033[33m--task <goal>\033[0m        Specify custom natural language goal for ReAct loop.\n"
              << "  \033[33m--agent\033[0m              Enable autonomous reasoning and action trajectory.\n\n"
              << "\033[1mFILTERING & TARGET SELECTION:\033[0m\n"
              << "  \033[33m--category <list>\033[0m     Target categories: system, browser, dev, messaging, app.\n"
              << "  \033[33m--exclude-category <c>\033[0m Exclude target categories from operation.\n"
              << "  \033[33m--only-ext <ext1,ext2>\033[0m Only operate on matching file extensions (e.g. .log,.tmp).\n"
              << "  \033[33m--exclude-ext <exts>\033[0m   Protect specific extensions from deletion (e.g. .py,.cpp).\n"
              << "  \033[33m--older-than <dur>\033[0m     Filter files older than duration (e.g. 30m, 24h, 7d, 30d).\n"
              << "  \033[33m--min-size <size>\033[0m      Minimum file size threshold (e.g. 10MB, 100MB, 1GB).\n"
              << "  \033[33m--max-size <size>\033[0m      Maximum file size threshold.\n"
              << "  \033[33m--strategy <mode>\033[0m      Inspection policy: smart (default), force, safe.\n\n"
              << "\033[1mLOCATION & SCOPE:\033[0m\n"
              << "  \033[33m--path <p1,p2>\033[0m         Specify custom directory path(s) to process.\n"
              << "  \033[33m--drive <drives>\033[0m       Target specific drive(s) (e.g. C:\\, D:\\) or 'all'.\n"
              << "  \033[33m--project-root <dir>\033[0m   Root directory for recursive dev build cache discovery.\n\n"
              << "\033[1mEXECUTION CONTROL:\033[0m\n"
              << "  \033[33m--mode <mode>\033[0m          Execution mode: light, deep, full, shred (secure wipe).\n"
              << "  \033[33m--dry-run\033[0m              Preview operational results without disk state mutation.\n"
              << "  \033[33m--recycle-bin\033[0m          Purge Windows Recycle Bin / OS Trash via Shell API.\n"
              << "  \033[33m--kill-locks <bool>\033[0m    Release file lock handles before operation (default: true).\n"
              << "  \033[33m--threads <N>\033[0m          Worker thread count for parallel processing (default: auto).\n"
              << "  \033[33m--json-report <file>\033[0m   Export structured execution results to JSON report.\n"
              << "  \033[33m--cron <duration>\033[0m      Run daemon service on recurring schedule (e.g. 10m, 1h).\n"
              << "  \033[33m--verbose\033[0m              Enable detailed trace logging.\n\n"
              << "\033[1mAGENT QUERY LANGUAGE (AQL) SYNTAX:\033[0m\n"
              << "  Use the \033[36maql\033[0m command or the chat/TUI mode to run natural-language-style queries:\n\n"
              << "  \033[1;33mCOMMANDS:\033[0m\n"
              << "    \033[32mCLEAN\033[0m   <path> WHERE <condition>      Clean files in <path> matching condition\n"
              << "    \033[32mSCAN\033[0m    <path> WHERE <condition>      Preview cleanable items (no deletion)\n"
              << "    \033[32mSHRED\033[0m   <path> WHERE <condition>      Secure-wipe (zero-overwrite) + delete\n"
              << "    \033[32mKILL\033[0m    PROCESS WHERE <condition>     Terminate processes matching condition\n"
              << "    \033[32mMONITOR\033[0m WHERE <condition> EVERY <dur> Watch thresholds & auto-clean\n"
              << "    \033[32mPURGE\033[0m   RECYCLE_BIN                   Empty OS Recycle Bin / Trash\n\n"
              << "  \033[1;33mCONDITIONS:\033[0m\n"
              << "    FREE_DISK < 500MB | 2GB | 10GB            Free space below threshold\n"
              << "    RAM > 80%  (or any %)                     Memory usage percentage\n"
              << "    SIZE > 10MB | 1GB                         Minimum file size\n"
              << "    AGE  > 24H | 7D | 30D                     File age threshold\n"
              << "    EXT IN ('.log', '.tmp')                   Match extension list\n\n"
              << "  \033[1;33mAQL EXAMPLE QUERIES (try these!):\033[0m\n"
              << "    \033[36maql \"CLEAN 'D:/Temp' WHERE FREE_DISK < 1GB\"\033[0m\n"
              << "    \033[36maql \"SCAN 'C:/Windows/Temp' WHERE AGE > 1H\"\033[0m\n"
              << "    \033[36maql \"SHRED 'D:/Temp' WHERE SIZE > 10MB\"\033[0m\n"
              << "    \033[36maql \"KILL PROCESS WHERE RAM > 200MB\"\033[0m\n"
              << "    \033[36maql \"MONITOR WHERE RAM > 80% EVERY 15S\"\033[0m\n"
              << "    \033[36maql \"PURGE RECYCLE_BIN\"\033[0m\n"
              << "    \033[36maql \"CLEAN 'C:/Users' WHERE EXT IN ('.log', '.tmp') AND AGE > 7D\"\033[0m\n\n"
              << "\033[1mTASK LIBRARY / PROCESS HISTORY:\033[0m\n"
              << "  Every launched operation (AQL queries, deep-clean, scan, shred, daemon, agent loop,\n"
              << "  RAM kill, recycle bin purge, etc.) is automatically tracked in a unified in-memory\n"
              << "  Task Library.  View it via:\n"
              << "    \033[36mhistory\033[0m        CLI view of all recorded tasks (status, timing, results)\n"
              << "    OpenTUI -> 'Task Library / History' menu  (live auto-refresh + filters)\n\n"
              << "\033[1mPRODUCTION EXAMPLES:\033[0m\n"
              << "  system-cleaner-agent chat\n"
              << "  system-cleaner-agent daemon --mem-threshold 80% --interval 15s\n"
              << "  system-cleaner-agent agent --task \"Perform clean code on D:/Temp when mem>80%\"\n"
              << "  system-cleaner-agent scan --dry-run\n"
              << "  system-cleaner-agent deep-clean --recycle-bin\n"
              << "  system-cleaner-agent aql \"KILL PROCESS WHERE RAM > 200MB\"\n"
              << "  system-cleaner-agent aql \"MONITOR WHERE RAM > 80% EVERY 15S\"\n"
              << "  system-cleaner-agent history\n";
}



std::vector<std::string> SplitString(const std::string& str, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

void RunInteractiveChatShell() {
    PrintHeader();
    std::cout << "\033[1;36m[Cleaner Agent Prompt Shell Active - Enter query, type 'menu' for pre-made queries, or 'exit']\033[0m\n\n";

    while (true) {
        std::cout << "\033[1;33mCleaner Agent > \033[0m";
        std::string input;
        if (!std::getline(std::cin, input)) break;

        std::string lowerInput = input;
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        if (lowerInput == "exit" || lowerInput == "quit" || lowerInput == "q") {
            std::cout << "\033[32mExiting Cleaner Agent Shell. Goodbye!\033[0m\n";
            break;
        }

        if (lowerInput == "menu" || lowerInput == "queries" || lowerInput == "select" || input.empty()) {
            input = TUI::SelectAgentQuery();
            std::cout << "\033[1;33mExecuting Selected Goal: " << input << "\033[0m\n\n";
        } else if (lowerInput == "help" || lowerInput == "?") {
            PrintHelp();
            std::cout << "\n\033[90m(Press Enter to return to the chat shell...)\033[0m";
            std::cin.get();
            continue;
        } else if (lowerInput == "history" || lowerInput == "tasks") {
            std::cout << "\033[1;36m=== Task Library / Process History ===\033[0m\n";
            std::cout << TaskHistory::Instance().HeaderSummary() << "\n\n";
            auto tasks = TaskHistory::Instance().Snapshot();
            if (tasks.empty()) {
                std::cout << "\033[1;33m  No tasks yet - run any clean / scan / shred / aql query first.\033[0m\n";
            } else {
                std::reverse(tasks.begin(), tasks.end());
                for (auto& t : tasks) {
                    bool isLive = (t.status == TaskStatus::Running || t.status == TaskStatus::Queued);
                    std::string timeStr = TaskHistoryNS::FormatTimestamp(isLive ? t.startedAt : t.finishedAt);
                    std::string durStr  = TaskHistoryNS::FormatDuration(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t.startedAt));
                    std::string detail = isLive ? (t.progressMsg.empty() ? TaskHistoryNS::StatusLabel(t.status) : t.progressMsg) : (t.resultSummary.empty() ? TaskHistoryNS::StatusLabel(t.status) : t.resultSummary);
                    if (detail.size() > 60) detail = detail.substr(0, 57) + "...";
                    std::cout << "  [" << std::setw(3) << std::setfill('0') << t.id << "] " << std::setfill(' ')
                              << TaskHistoryNS::StatusColor(t.status) << std::left << std::setw(8) << TaskHistoryNS::StatusLabel(t.status) << std::right << "\033[0m "
                              << std::left << std::setw(8) << t.category << std::right << " "
                              << "\033[1;33m" << std::left << std::setw(10) << timeStr << std::right << "\033[0m "
                              << "\033[90m(" << std::left << std::setw(7) << durStr << std::right << ")\033[0m "
                              << "\033[1;97m" << std::left << std::setw(28) << t.name << std::right << "\033[0m "
                              << "\033[1;36m" << detail << "\033[0m\n";
                }
            }
            std::cout << "\n\033[90m(Press Enter to return to the chat shell...)\033[0m";
            std::cin.get();
            continue;
        }

        // Register this chat goal in the unified Task Library
        uint64_t chatTaskId = TaskHistory::Instance().Register("CHAT", "Chat: " + input, input, "Chat Shell");
        TaskHistory::Instance().MarkRunning(chatTaskId);
        TaskHistory::Instance().UpdateProgress(chatTaskId, "ReAct agent reasoning...");
        AgentEngine agent(input);
        agent.RunReActLoop(true);    }
}

void ExportJsonReport(const std::string& jsonPath, const std::vector<TargetReport>& reports) {
    try {
        std::ofstream jsonFile(jsonPath);
        if (!jsonFile.is_open()) return;

        jsonFile << "{\n";
        jsonFile << "  \"engine\": \"system-cleaner-agent v5.6.0 (C++17 ReAct Agentic Engine)\",\n";
        jsonFile << "  \"targets\": [\n";

        uintmax_t grandTotal = 0;
        for (size_t i = 0; i < reports.size(); ++i) {
            const auto& r = reports[i];
            grandTotal += r.sizeBytes;
            jsonFile << "    {\n";
            jsonFile << "      \"name\": \"" << r.name << "\",\n";
            jsonFile << "      \"category\": \"" << r.category << "\",\n";
            jsonFile << "      \"path\": \"" << r.path.string() << "\",\n";
            jsonFile << "      \"size_bytes\": " << r.sizeBytes << ",\n";
            jsonFile << "      \"file_count\": " << r.fileCount << ",\n";
            jsonFile << "      \"protected_files_preserved\": " << (r.skippedProtectedFiles ? "true" : "false") << "\n";
            jsonFile << "    }" << (i + 1 < reports.size() ? "," : "") << "\n";
        }

        jsonFile << "  ],\n";
        jsonFile << "  \"total_cleanable_bytes\": " << grandTotal << ",\n";
        jsonFile << "  \"total_cleanable_formatted\": \"" << Cleaner::FormatSize(grandTotal) << "\"\n";
        jsonFile << "}\n";

        Logger::Instance().Info("JSON execution report saved: " + jsonPath);
    } catch (const std::exception& e) {
        Logger::Instance().Error("JSON export failed: " + std::string(e.what()));
    }
}

long long ParseDuration(const std::string& durationStr) {
    if (durationStr.empty()) return 0;
    char unit = durationStr.back();
    std::string valueStr = durationStr.substr(0, durationStr.size() - 1);
    try {
        long long value = std::stoll(valueStr);
        if (unit == 's' || unit == 'S') return value;
        if (unit == 'm' || unit == 'M') return value * 60;
        if (unit == 'h' || unit == 'H') return value * 3600;
    } catch (...) {}
    return 0;
}

std::vector<std::string> PreprocessArgs(int argc, char* argv[]) {
    std::vector<std::string> tokens;
    for (int i = 1; i < argc; ++i) {
        std::string raw = argv[i];
        while (!raw.empty() && (raw.front() == '"' || raw.front() == '\'')) raw.erase(0, 1);
        while (!raw.empty() && (raw.back() == '"' || raw.back() == '\'')) raw.pop_back();

        size_t flagPos = raw.find(" --");
        if (flagPos != std::string::npos) {
            std::string head = raw.substr(0, flagPos);
            while (!head.empty() && (head.back() == '"' || head.back() == '\'')) head.pop_back();
            if (!head.empty()) tokens.push_back(head);

            std::string tail = raw.substr(flagPos + 1);
            std::stringstream ss(tail);
            std::string sub;
            while (ss >> sub) {
                while (!sub.empty() && (sub.front() == '"' || sub.front() == '\'')) sub.erase(0, 1);
                while (!sub.empty() && (sub.back() == '"' || sub.back() == '\'')) sub.pop_back();
                if (!sub.empty()) tokens.push_back(sub);
            }
        } else {
            if (!raw.empty()) tokens.push_back(raw);
        }
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    auto args = PreprocessArgs(argc, argv);

    bool verbose = false;
    for (const auto& arg : args) {
        if (arg == "--verbose" || arg == "-v") verbose = true;
    }

    Logger::Instance().Init("system-cleaner-agent.log", verbose);
    LoadTUISettings();
    TaskHistory::Instance().LoadFromFile();
    auto resumedTaskIds = TaskHistory::Instance().ResumeUnfinishedTasksOnStartup();
    if (!resumedTaskIds.empty()) {
        Logger::Instance().Info("TaskHistory: Loaded & resumed " + std::to_string(resumedTaskIds.size()) + " unfinished task(s) from persistent configuration file.");
    }

    Cleaner cleaner;

    if (args.empty()) {
        TUI::RunInteractiveMenu(cleaner);
        return 0;
    }

    std::string cmd = args[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        PrintHelp();
        return 0;
    }

    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        std::cout << "system-cleaner-agent v5.6.0 (C++17 Autonomous ReAct Agentic Engine - 64-bit Architecture)\n";
        return 0;
    }

    if (cmd == "aql") {
        std::string queryStr = (args.size() > 1) ? args[1] : "CLEAN 'C:\\Temp' WHERE FREE_DISK < 500MB";
        AQLQuery q = AQLEngine::Parse(queryStr);
        AQLEngine::Execute(q, cleaner, true);
        return 0;
    }

    if (cmd == "history" || cmd == "--history" || cmd == "tasks") {
        // Print the unified Task Library to stdout and exit
        TUI::PrintBanner();
        std::cout << TaskHistory::Instance().HeaderSummary() << std::endl << std::endl;
        std::vector<TaskEntry> tasks = TaskHistory::Instance().Snapshot();
        if (tasks.empty()) {
            std::cout << "\033[1;33m  No tasks recorded yet. Run any clean / scan / shred / AQL / agent / daemon command first.\033[0m" << std::endl;
            std::cout << "\033[90m  Tip: launch the OpenTUI with no args for a beautiful live Task Library view.\033[0m" << std::endl;
        } else {
            std::cout << "\033[1;37m" << std::left
                      << std::setw(6)  << "ID"
                      << std::setw(11) << "STATUS"
                      << std::setw(10) << "CATEGORY"
                      << std::setw(11) << "TIME"
                      << std::setw(9)  << "DUR"
                      << std::setw(26) << "NAME"
                      << std::setw(40) << "DETAIL / RESULT"
                      << "\033[0m" << std::endl;
            std::cout << "\033[90m" << std::string(113, char(45)) << "\033[0m" << std::endl;
            std::reverse(tasks.begin(), tasks.end());
            for (auto& t : tasks) {
                bool isLive = (t.status == TaskStatus::Running || t.status == TaskStatus::Queued);
                std::string timeStr = TaskHistoryNS::FormatTimestamp(isLive ? t.startedAt : t.finishedAt);
                std::string durStr  = TaskHistoryNS::FormatDuration(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - t.startedAt));
                std::string name = t.name; if (name.size() > 24) name = name.substr(0, 21) + "...";
                std::string detail = isLive ? (t.progressMsg.empty() ? TaskHistoryNS::StatusLabel(t.status) : t.progressMsg) : (t.resultSummary.empty() ? TaskHistoryNS::StatusLabel(t.status) : t.resultSummary);
                if (detail.size() > 38) detail = detail.substr(0, 35) + "...";
                std::cout << "\033[90m[" << std::setw(3) << std::setfill('0') << t.id << "]\033[0m " << std::setfill(' ')
                          << TaskHistoryNS::StatusColor(t.status) << std::left << std::setw(10) << TaskHistoryNS::StatusLabel(t.status) << std::right << "\033[0m "
                          << std::left << std::setw(10) << t.category << std::right << " "
                          << "\033[1;33m" << std::left << std::setw(10) << timeStr << std::right << "\033[0m "
                          << "\033[90m" << std::left << std::setw(8) << durStr << std::right << "\033[0m "
                          << "\033[1;97m" << std::left << std::setw(25) << name << std::right << "\033[0m "
                          << "\033[1;36m" << std::left << std::setw(38) << detail << std::right << "\033[0m";
                if (t.bytesFreed > 0) std::cout << "  \033[1;32mfreed " << Cleaner::FormatSize(t.bytesFreed) << "\033[0m";
                std::cout << std::endl;
            }
        }
        return 0;
    }

    if (cmd == "chat" || cmd == "--chat") {
        RunInteractiveChatShell();
        return 0;
    }

    if (cmd == "tui" || cmd == "--tui" || cmd == "--interactive" || cmd == "-i") {
        TUI::RunInteractiveMenu(cleaner);
        return 0;
    }

    InspectionConfig cfg;
    CleanMode cleanMode = CleanMode::Light;

    bool dryRun = false;
    bool recycleBin = false;
    bool killLocks = true;
    bool sandboxMode = true;       // Sandbox ON by default — no real deletions unless disabled
    bool pathProtection = true;    // Dangerous path guard ON by default
    long long cronIntervalSeconds = 0;
    long long daemonCheckIntervalSeconds = 15;
    double memThresholdPercent  = 0.0;
    double diskThresholdPercent  = 0.0;
    uintmax_t diskFreeBelowBytes = 0;      // e.g. 500 MB — trigger when free space drops below this
    fs::path monitorDrive        = "C:\\"; // drive to watch for free-space threshold
    std::vector<ScheduleRule> scheduleRules;

    size_t threadCount = 0;
    size_t topN = 20;
    size_t deepDepth = 0;
    bool multiDrive = false;
    std::string jsonReportPath = "";
    std::string agentTaskGoal = "Perform autonomous system optimization and storage cleanup";
    bool runAgentLoop = (cmd == "agent" || cmd == "--agent" || cmd == "-a");
    bool runDaemon = (cmd == "daemon" || cmd == "--daemon");

    std::vector<fs::path> customPaths;
    std::vector<fs::path> targetDrives;

    for (size_t i = 0; i < args.size(); ++i) {
        std::string arg = args[i];
        std::string lowerArg = arg;
        std::transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);

        if (lowerArg == "--dry-run") {
            dryRun = true;
        } else if (lowerArg == "--no-sandbox") {
            sandboxMode = false;
        } else if (lowerArg == "--no-path-protection") {
            pathProtection = false;
        } else if (lowerArg == "--agent") {
            runAgentLoop = true;
        } else if (lowerArg == "--top" && i + 1 < args.size()) {
            try { topN = std::stoul(args[++i]); } catch (...) {}
        } else if (lowerArg == "--depth" && i + 1 < args.size()) {
            try { deepDepth = std::stoul(args[++i]); } catch (...) {}
        } else if (lowerArg == "--multi-drive") {
            multiDrive = true;
        } else if (lowerArg == "--task" && i + 1 < args.size()) {
            agentTaskGoal = args[++i];
            runAgentLoop = true;
        } else if (lowerArg == "--mem-threshold" && i + 1 < args.size()) {
            std::string val = args[++i];
            if (val.back() == '%') val.pop_back();
            try { memThresholdPercent = std::stod(val); } catch (...) {}
        } else if (lowerArg == "--disk-threshold" && i + 1 < args.size()) {
            std::string val = args[++i];
            if (val.back() == '%') val.pop_back();
            try { diskThresholdPercent = std::stod(val); } catch (...) {}
        } else if (lowerArg == "--disk-free-below" && i + 1 < args.size()) {
            diskFreeBelowBytes = ContentInspector::ParseSizeToBytes(args[++i]);
        } else if (lowerArg == "--monitor-drive" && i + 1 < args.size()) {
            monitorDrive = SmartScheduler::NormalizeDrivePath(args[++i]);
        } else if (lowerArg == "--schedule" && i + 1 < args.size()) {
            scheduleRules.push_back(SmartScheduler::ParseRuleString(args[++i]));
        } else if (lowerArg == "--interval" && i + 1 < args.size()) {
            daemonCheckIntervalSeconds = ParseDuration(args[++i]);
            if (daemonCheckIntervalSeconds <= 0) daemonCheckIntervalSeconds = 15;
        } else if (lowerArg == "--recycle-bin") {
            recycleBin = true;
        } else if (lowerArg == "--kill-locks" && i + 1 < args.size()) {
            std::string val = args[++i];
            killLocks = (val == "true" || val == "1" || val == "yes");
        } else if (lowerArg == "--cron" && i + 1 < args.size()) {
            cronIntervalSeconds = ParseDuration(args[++i]);
        } else if (lowerArg == "--threads" && i + 1 < args.size()) {
            try { threadCount = std::stoul(args[++i]); } catch (...) {}
        } else if (lowerArg == "--json-report" && i + 1 < args.size()) {
            jsonReportPath = args[++i];
        } else if (lowerArg == "--path" && i + 1 < args.size()) {
            auto pathList = SplitString(args[++i], ',');
            for (const auto& p : pathList) customPaths.push_back(fs::path(p));
        } else if (lowerArg == "--drive" && i + 1 < args.size()) {
            auto driveList = SplitString(args[++i], ',');
            for (const auto& d : driveList) targetDrives.push_back(fs::path(d));
        } else if (lowerArg == "--project-root" && i + 1 < args.size()) {
            cleaner.SetProjectRoot(fs::path(args[++i]));
        } else if (lowerArg == "--mode" && i + 1 < args.size()) {
            std::string mStr = args[++i];
            std::transform(mStr.begin(), mStr.end(), mStr.begin(), ::tolower);
            if (mStr == "light") cleanMode = CleanMode::Light;
            else if (mStr == "deep") cleanMode = CleanMode::Deep;
            else if (mStr == "full") cleanMode = CleanMode::Full;
            else if (mStr == "shred") cleanMode = CleanMode::Shred;
        } else if (lowerArg == "--strategy" && i + 1 < args.size()) {
            cfg.strategy = args[++i];
            std::transform(cfg.strategy.begin(), cfg.strategy.end(), cfg.strategy.begin(), ::tolower);
        } else if (lowerArg == "--category" && i + 1 < args.size()) {
            auto catList = SplitString(args[++i], ',');
            for (auto c : catList) {
                std::transform(c.begin(), c.end(), c.begin(), ::tolower);
                cfg.includeCategories.insert(c);
            }
        } else if (lowerArg == "--exclude-category" && i + 1 < args.size()) {
            auto catList = SplitString(args[++i], ',');
            for (auto c : catList) {
                std::transform(c.begin(), c.end(), c.begin(), ::tolower);
                cfg.excludeCategories.insert(c);
            }
        } else if (lowerArg == "--only-ext" && i + 1 < args.size()) {
            auto extList = SplitString(args[++i], ',');
            for (auto e : extList) {
                if (e.front() != '.') e = "." + e;
                std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                cfg.onlyExts.insert(e);
            }
        } else if (lowerArg == "--exclude-ext" && i + 1 < args.size()) {
            auto extList = SplitString(args[++i], ',');
            for (auto e : extList) {
                if (e.front() != '.') e = "." + e;
                std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                cfg.excludeExts.insert(e);
            }
        } else if (lowerArg == "--older-than" && i + 1 < args.size()) {
            cfg.minAgeMinutes = ContentInspector::ParseDurationToMinutes(args[++i]);
        } else if (lowerArg == "--min-size" && i + 1 < args.size()) {
            cfg.minSizeBytes = ContentInspector::ParseSizeToBytes(args[++i]);
        } else if (lowerArg == "--max-size" && i + 1 < args.size()) {
            cfg.maxSizeBytes = ContentInspector::ParseSizeToBytes(args[++i]);
        }
    }

    if (cmd == "test" || cmd == "--test") {
        std::cout << "Executing Engine Unit Tests...\n";
#ifdef _WIN32
        system("unit_tests.exe");
#else
        system("./unit_tests");
#endif
        return 0;
    }

    if (cmd == "deep-scan") {
        PrintHeader();
        std::string scanRoot = customPaths.empty() ? "." : customPaths[0].string();
        DeepScanFilter filter;
        filter.topN = topN > 0 ? topN : 20;
        filter.maxDepth = deepDepth;
        filter.minSizeBytes = cfg.minSizeBytes;
        filter.excludeExts = cfg.excludeExts;

        if (multiDrive) {
            Logger::Instance().Info("Running Multi-Drive Parallel Deep Scan...");
            auto driveResults = SmartScheduler::ScanAllDrivesSimultaneously(90.0);
            std::cout << "\n\033[1;33m+--[ Multi-Drive Deep Storage Matrix ]-------------------------------------------+\033[0m\n";
            for (const auto& dr : driveResults) {
                std::string badge = dr.isWarning ? "\033[1;31m [WARNING: LOW SPACE > 90%]\033[0m" : "\033[1;32m [OK]\033[0m";
                std::cout << "  Drive " << std::left << std::setw(8) << dr.driveName
                          << " " << dr.usageBar
                          << "  (Free: " << Cleaner::FormatSize(dr.freeBytes) << " / " << Cleaner::FormatSize(dr.totalBytes) << ")"
                          << badge << "\n";
            }
            std::cout << "\033[1;33m+--------------------------------------------------------------------------------+\033[0m\n\n";
        }

        Logger::Instance().Info("Deep Scanning directory tree: " + scanRoot);
        auto rootNode = DeepScanner::ScanDirectory(scanRoot, filter);
        auto topNodes = DeepScanner::GetTopN(rootNode, filter.topN);

        std::cout << "\n\033[1;36m=== Deep Disk Scan Summary ('" << scanRoot << "') ===\033[0m\n";
        std::cout << "Total Size: " << Cleaner::FormatSize(rootNode ? rootNode->sizeBytes : 0)
                  << " | Total Files: " << (rootNode ? rootNode->fileCount : 0)
                  << " | Total Dirs: " << (rootNode ? rootNode->dirCount : 0) << "\n\n";

        std::cout << "\033[1;33mTop " << topNodes.size() << " Largest Items:\033[0m\n";
        uintmax_t rootTotal = rootNode ? rootNode->sizeBytes : 0;
        for (size_t idx = 0; idx < topNodes.size(); ++idx) {
            std::string bar = DeepScanner::RenderVisualSizeBar(topNodes[idx]->sizeBytes, rootTotal, 12);
            std::cout << "  #" << std::setw(2) << (idx + 1) << "  "
                      << std::left << std::setw(45) << (topNodes[idx]->name + (topNodes[idx]->isDirectory ? "/" : ""))
                      << " " << bar << "\n";
        }

        if (!jsonReportPath.empty()) {
            DeepScanner::ExportToJson(scanRoot, rootNode, jsonReportPath);
        }
        return 0;
    }

    cleaner.SetMode(cleanMode);
    cleaner.SetDryRun(dryRun);
    cleaner.SetSandbox(sandboxMode);
    cleaner.SetDangerousPathProtection(pathProtection);
    cleaner.SetEmptyRecycleBin(recycleBin);
    cleaner.SetKillLockingProcesses(killLocks);
    cleaner.SetCustomPaths(customPaths);
    cleaner.SetTargetDrives(targetDrives);
    cleaner.SetInspectionConfig(cfg);
    if (threadCount > 0) cleaner.SetMaxThreads(threadCount);

    if (runDaemon) {
        SmartScheduler::RunDaemonService(cleaner, scheduleRules,
            memThresholdPercent, diskThresholdPercent,
            daemonCheckIntervalSeconds, dryRun,
            diskFreeBelowBytes, monitorDrive);
        return 0;
    }

    if (runAgentLoop) {
        AgentEngine agent(agentTaskGoal);
        if (!customPaths.empty()) {
            agent.SetCustomTargetPaths(customPaths);
        }
        if (memThresholdPercent > 0.0) agent.SetMemThreshold(memThresholdPercent);
        if (diskThresholdPercent > 0.0) agent.SetDiskThreshold(diskThresholdPercent);
        agent.RunReActLoop(dryRun);
        return 0;
    }

    if (cmd == "deep-clean") {
        recycleBin = true;
        cleanMode = CleanMode::Deep;
        cmd = "clean";
    }

    PrintHeader();
    Logger::Instance().Info("Execution mode: " + cmd + (dryRun ? " [DRY-RUN]" : ""));

    auto executeTask = [&]() {
        if (cmd == "scan") {
            auto reports = cleaner.Scan();
            if (!jsonReportPath.empty()) {
                ExportJsonReport(jsonReportPath, reports);
            }
        } else if (cmd == "clean") {
            cleaner.Clean();
        } else {
            std::cout << "Invalid command: " << cmd << "\n\n";
            PrintHelp();
            exit(1);
        }
    };

    if (cronIntervalSeconds > 0) {
        Logger::Instance().Info("Running daemon service every " + std::to_string(cronIntervalSeconds) + " seconds.");
        while (true) {
            executeTask();
            Logger::Instance().Info("Service sleeping for " + std::to_string(cronIntervalSeconds) + " seconds...");
            std::this_thread::sleep_for(std::chrono::seconds(cronIntervalSeconds));
        }
    } else {
        executeTask();
    }

    return 0;
}
