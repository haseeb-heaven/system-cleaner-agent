#pragma once

// =============================================================================
// TaskHistory.hpp - Unified Task Library / Process Manager
// =============================================================================
//
// A thread-safe, in-memory registry of every operation the cleaner has
// launched (AQL queries, deep-clean, scan, shred, daemon, RAM kill, recycle
// bin purge, agent loops, custom chat goals, etc.).  Any subsystem can
// register a task, update its progress, mark it completed/failed, and the
// Task Library TUI screen can render a beautiful, unified view of all
// current + historical tasks with status, timing, and result data.
// =============================================================================

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace TaskHistoryNS {

inline std::string GetDefaultTasksFilePath() {
#ifdef _WIN32
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData && std::string(localAppData).length() > 0) {
        std::error_code ec;
        std::filesystem::path appDir = std::filesystem::path(localAppData) / "system-cleaner-agent";
        std::filesystem::create_directories(appDir, ec);
        return (appDir / "cleaner_tasks.json").string();
    }
#endif
    return "cleaner_tasks.json";
}

inline std::string EscapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

inline std::string UnescapeJsonString(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            char next = input[i + 1];
            if (next == '"') { out += '"'; i++; }
            else if (next == '\\') { out += '\\'; i++; }
            else if (next == 'n') { out += '\n'; i++; }
            else if (next == 'r') { out += '\r'; i++; }
            else if (next == 't') { out += '\t'; i++; }
            else { out += input[i]; }
        } else {
            out += input[i];
        }
    }
    return out;
}

enum class Status {
    Queued,
    Running,
    Paused,
    Completed,
    Failed,
    Cancelled,
    Unknown
};

inline std::string StatusLabel(Status s) {
    switch (s) {
        case Status::Queued:    return "QUEUED";
        case Status::Running:   return "RUNNING";
        case Status::Paused:    return "PAUSED";
        case Status::Completed: return "OK";
        case Status::Failed:    return "FAILED";
        case Status::Cancelled: return "KILLED";
        default:                return "?";
    }
}

inline const char* StatusColor(Status s) {
    switch (s) {
        case Status::Queued:    return "\033[1;33m";
        case Status::Running:   return "\033[1;36m";
        case Status::Paused:    return "\033[1;35m";
        case Status::Completed: return "\033[1;32m";
        case Status::Failed:    return "\033[1;31m";
        case Status::Cancelled: return "\033[1;90m";
        default:                return "\033[1;37m";
    }
}

struct TaskEntry {
    uint64_t id = 0;
    std::string category;
    std::string name;
    std::string command;
    std::string source;
    Status      status = Status::Queued;
    std::string progressMsg;
    std::string resultSummary;
    uintmax_t   bytesFreed = 0;
    size_t      filesProcessed = 0;
    size_t      filesSkipped = 0;
    size_t      processesHandled = 0;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point finishedAt;
    std::thread::id threadId;
    double      percent = 0.0;
};

inline std::string FormatDuration(std::chrono::milliseconds ms) {
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(ms).count();
    if (secs < 60) return std::to_string(secs) + "s";
    int m = static_cast<int>(secs / 60);
    int s = static_cast<int>(secs % 60);
    if (m < 60) return std::to_string(m) + "m " + std::to_string(s) + "s";
    int h = m / 60;
    m = m % 60;
    return std::to_string(h) + "h " + std::to_string(m) + "m";
}

inline std::string FormatTimestamp(std::chrono::system_clock::time_point tp) {
    if (tp.time_since_epoch().count() == 0) return "--";
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return std::string(buf);
}

}

namespace TaskHistoryNS {

class TaskHistoryRegistry {
public:
    static TaskHistoryRegistry& Instance() {
        static TaskHistoryRegistry inst;
        return inst;
    }

    void SetFilePath(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx_);
        tasksFilePath = path;
    }

    std::string GetFilePath() const {
        if (!tasksFilePath.empty()) return tasksFilePath;
        return GetDefaultTasksFilePath();
    }

    void SaveToFile(const std::string& path = "") const {
        std::lock_guard<std::mutex> lock(mtx_);
        SaveToFileUnlocked(path);
    }

    void LoadFromFile(const std::string& path = "") {
        std::lock_guard<std::mutex> lock(mtx_);
        std::string targetPath = path.empty() ? GetFilePath() : path;
        if (!std::filesystem::exists(targetPath)) return;

        try {
            std::ifstream file(targetPath);
            if (!file.is_open()) return;

            std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            size_t nextIdPos = jsonStr.find("\"nextId\":");
            if (nextIdPos != std::string::npos) {
                std::stringstream ss(jsonStr.substr(nextIdPos + 9));
                uint64_t nid = 0;
                if (ss >> nid) nextId = nid;
            }

            tasks.clear();
            size_t arrStart = jsonStr.find("\"tasks\":");
            if (arrStart == std::string::npos) return;

            size_t pos = arrStart;
            while ((pos = jsonStr.find('{', pos)) != std::string::npos) {
                size_t objEnd = jsonStr.find('}', pos);
                if (objEnd == std::string::npos) break;

                std::string block = jsonStr.substr(pos, objEnd - pos + 1);
                pos = objEnd + 1;

                if (block.find("\"id\":") == std::string::npos) continue;

                TaskEntry t;
                auto getValInt = [&](const std::string& key) -> int64_t {
                    size_t k = block.find("\"" + key + "\":");
                    if (k == std::string::npos) return 0;
                    std::stringstream ss(block.substr(k + key.length() + 3));
                    int64_t val = 0;
                    ss >> val;
                    return val;
                };

                auto getValStr = [&](const std::string& key) -> std::string {
                    size_t k = block.find("\"" + key + "\":");
                    if (k == std::string::npos) return "";
                    size_t q1 = block.find('"', k + key.length() + 3);
                    if (q1 == std::string::npos) return "";
                    size_t q2 = q1 + 1;
                    while (q2 < block.size()) {
                        if (block[q2] == '"' && block[q2 - 1] != '\\') break;
                        q2++;
                    }
                    if (q2 >= block.size()) return "";
                    return UnescapeJsonString(block.substr(q1 + 1, q2 - q1 - 1));
                };

                auto getValDouble = [&](const std::string& key) -> double {
                    size_t k = block.find("\"" + key + "\":");
                    if (k == std::string::npos) return 0.0;
                    std::stringstream ss(block.substr(k + key.length() + 3));
                    double val = 0.0;
                    ss >> val;
                    return val;
                };

                t.id = getValInt("id");
                t.category = getValStr("category");
                t.name = getValStr("name");
                t.command = getValStr("command");
                t.source = getValStr("source");
                int statusInt = static_cast<int>(getValInt("status"));
                t.status = static_cast<Status>(statusInt);
                t.progressMsg = getValStr("progressMsg");
                t.resultSummary = getValStr("resultSummary");
                t.bytesFreed = static_cast<uintmax_t>(getValInt("bytesFreed"));
                t.filesProcessed = static_cast<size_t>(getValInt("filesProcessed"));
                t.filesSkipped = static_cast<size_t>(getValInt("filesSkipped"));
                t.processesHandled = static_cast<size_t>(getValInt("processesHandled"));

                int64_t startSec = getValInt("startSec");
                int64_t finishSec = getValInt("finishSec");
                if (startSec > 0) t.startedAt = std::chrono::system_clock::time_point(std::chrono::seconds(startSec));
                if (finishSec > 0) t.finishedAt = std::chrono::system_clock::time_point(std::chrono::seconds(finishSec));
                t.percent = getValDouble("percent");

                if (t.id > 0) {
                    tasks.push_back(t);
                    if (t.id > nextId) nextId = t.id;
                }
            }
        } catch (...) {}
    }

    std::vector<uint64_t> ResumeUnfinishedTasksOnStartup() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<uint64_t> cleanedIds;
        for (auto& t : tasks) {
            if (t.status == Status::Running || t.status == Status::Queued || t.status == Status::Paused) {
                t.status = Status::Cancelled;
                t.finishedAt = std::chrono::system_clock::now();
                t.resultSummary = "Interrupted by application restart.";
                t.progressMsg = "Stopped.";
                cleanedIds.push_back(t.id);
            }
        }
        if (!cleanedIds.empty()) {
            SaveToFileUnlocked();
        }
        return cleanedIds;
    }

    uint64_t Register(const std::string& category,
                      const std::string& name,
                      const std::string& command,
                      const std::string& source) {
        std::lock_guard<std::mutex> lock(mtx_);
        TaskEntry e;
        e.id        = ++nextId;
        e.category  = category;
        e.name      = name;
        e.command   = command;
        e.source    = source;
        e.status    = Status::Queued;
        e.startedAt = std::chrono::system_clock::now();
        e.threadId  = std::this_thread::get_id();
        tasks.push_back(e);
        if (tasks.size() > 200) {
            tasks.erase(tasks.begin(), tasks.begin() + 50);
        }
        SaveToFileUnlocked();
        return e.id;
    }

    void MarkRunning(uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return;
        e->status = Status::Running;
        e->startedAt = std::chrono::system_clock::now();
        e->threadId  = std::this_thread::get_id();
        SaveToFileUnlocked();
    }

    void UpdateProgress(uint64_t id, const std::string& msg, double percent = -1.0) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return;
        e->progressMsg = msg;
        if (percent >= 0.0) e->percent = percent;
        SaveToFileUnlocked();
    }

    void MarkCompleted(uint64_t id, const std::string& summary = "",
                       uintmax_t bytesFreed = 0, size_t filesProcessed = 0,
                       size_t filesSkipped = 0, size_t processesHandled = 0) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return;
        e->status        = Status::Completed;
        e->finishedAt    = std::chrono::system_clock::now();
        e->resultSummary = summary;
        e->bytesFreed    = bytesFreed;
        e->filesProcessed  = filesProcessed;
        e->filesSkipped    = filesSkipped;
        e->processesHandled= processesHandled;
        e->percent       = 100.0;
        if (e->progressMsg.empty()) e->progressMsg = "Completed.";
        SaveToFileUnlocked();
    }

    void MarkFailed(uint64_t id, const std::string& errorMsg) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return;
        e->status        = Status::Failed;
        e->finishedAt    = std::chrono::system_clock::now();
        e->resultSummary = "Error: " + errorMsg;
        SaveToFileUnlocked();
    }

    void MarkCancelled(uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return;
        e->status        = Status::Cancelled;
        e->finishedAt    = std::chrono::system_clock::now();
        e->resultSummary = "Cancelled by user/system.";
        SaveToFileUnlocked();
    }

    bool PauseTask(uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e || (e->status != Status::Running && e->status != Status::Queued)) return false;
        e->status = Status::Paused;
        e->progressMsg = "Paused by user action.";
        SaveToFileUnlocked();
        return true;
    }

    bool ResumeTask(uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e || e->status != Status::Paused) return false;
        e->status = Status::Running;
        e->progressMsg = "Resumed running...";
        SaveToFileUnlocked();
        return true;
    }

    bool KillTask(uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return false;
        e->status        = Status::Cancelled;
        e->finishedAt    = std::chrono::system_clock::now();
        e->resultSummary = "Killed / Terminated by user.";
        SaveToFileUnlocked();
        return true;
    }

    bool GetTask(uint64_t id, TaskEntry& outTask) const {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto& t : tasks) {
            if (t.id == id) {
                outTask = t;
                return true;
            }
        }
        return false;
    }

    std::vector<TaskEntry> Snapshot() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return tasks;
    }

    size_t RunningCount() const {
        std::lock_guard<std::mutex> lock(mtx_);
        size_t c = 0;
        for (const auto& t : tasks) if (t.status == Status::Running || t.status == Status::Queued) ++c;
        return c;
    }

    size_t TotalCount() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return tasks.size();
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        tasks.clear();
        SaveToFileUnlocked();
    }

    std::string HeaderSummary() const {
        std::lock_guard<std::mutex> lock(mtx_);
        size_t running = 0, done = 0, failed = 0;
        for (const auto& t : tasks) {
            if (t.status == Status::Running || t.status == Status::Queued) ++running;
            else if (t.status == Status::Completed) ++done;
            else if (t.status == Status::Failed) ++failed;
        }
        std::ostringstream ss;
        ss << "[Task Library] "
           << running << " running | "
           << done << " done | "
           << failed << " failed | "
           << "total " << tasks.size();
        return ss.str();
    }

    static std::string RenderRow(const TaskEntry& t) {
        std::ostringstream ss;
        auto now    = std::chrono::system_clock::now();
        auto elapsed= std::chrono::duration_cast<std::chrono::milliseconds>(now - t.startedAt);
        bool isLive = (t.status == Status::Running || t.status == Status::Queued);

        std::string timeStr = FormatTimestamp(isLive ? t.startedAt : t.finishedAt);
        std::string durStr  = FormatDuration(elapsed);

        ss << "[" << std::setw(3) << std::setfill('0') << t.id << std::setfill(' ') << "] "
           << StatusColor(t.status) << StatusLabel(t.status) << " "
           << "\033[0m"
           << t.category << " "
           << timeStr << " ("
           << durStr << ") ";

        std::string name = t.name;
        if (name.size() > 24) name = name.substr(0, 21) + "...";
        ss << name << " ";

        std::string detail = isLive
            ? (t.progressMsg.empty() ? StatusLabel(t.status) : t.progressMsg)
            : (t.resultSummary.empty() ? StatusLabel(t.status) : t.resultSummary);
        if (detail.size() > 50) detail = detail.substr(0, 47) + "...";
        ss << detail;

        if (isLive && t.percent > 0.0 && t.percent < 100.0) {
            int barWidth = 12;
            int filled = static_cast<int>((t.percent / 100.0) * barWidth);
            ss << "  ";
            for (int i = 0; i < filled; ++i) ss << "#";
            for (int i = filled; i < barWidth; ++i) ss << "-";
            ss << " " << static_cast<int>(t.percent) << "%";
        } else if (t.bytesFreed > 0) {
            auto fmt = [](uintmax_t b) -> std::string {
                const char* units[] = {"B","KB","MB","GB","TB"};
                double v = static_cast<double>(b);
                int u = 0;
                while (v >= 1024.0 && u < 4) { v /= 1024.0; ++u; }
                std::ostringstream o;
                o << std::fixed << std::setprecision(2) << v << " " << units[u];
                return o.str();
            };
            ss << "  freed " << fmt(t.bytesFreed);
        }
        return ss.str();
    }

    static void RenderHeader(std::ostringstream& ss) {
        ss << "[ ID] STATUS    CATEGORY  TIME      DUR    NAME                      DETAIL";
        ss << "\n";
    }

    static void RenderFullTable(std::vector<TaskEntry>& tasks, std::ostringstream& ss) {
        if (tasks.empty()) {
            ss << "  (no tasks recorded yet - run any clean, scan, agent, or AQL query to populate)\n";
            return;
        }
        // Show in reverse-chronological order (newest first)
        std::vector<TaskEntry> ordered = tasks;
        std::reverse(ordered.begin(), ordered.end());

        RenderHeader(ss);
        for (auto& t : ordered) {
            ss << RenderRow(t) << "\n";
        }
    }

private:
    TaskEntry* Find(uint64_t id) {
        for (auto& t : tasks) if (t.id == id) return &t;
        return nullptr;
    }

    void SaveToFileUnlocked(const std::string& path = "") const {
        std::string targetPath = path.empty() ? GetFilePath() : path;
        try {
            std::ofstream out(targetPath);
            if (!out.is_open()) return;

            out << "{\n";
            out << "  \"nextId\": " << nextId << ",\n";
            out << "  \"tasks\": [\n";

            for (size_t i = 0; i < tasks.size(); ++i) {
                const auto& t = tasks[i];
                auto startSec = std::chrono::duration_cast<std::chrono::seconds>(t.startedAt.time_since_epoch()).count();
                auto finishSec = std::chrono::duration_cast<std::chrono::seconds>(t.finishedAt.time_since_epoch()).count();

                out << "    {\n";
                out << "      \"id\": " << t.id << ",\n";
                out << "      \"category\": \"" << EscapeJsonString(t.category) << "\",\n";
                out << "      \"name\": \"" << EscapeJsonString(t.name) << "\",\n";
                out << "      \"command\": \"" << EscapeJsonString(t.command) << "\",\n";
                out << "      \"source\": \"" << EscapeJsonString(t.source) << "\",\n";
                out << "      \"status\": " << static_cast<int>(t.status) << ",\n";
                out << "      \"progressMsg\": \"" << EscapeJsonString(t.progressMsg) << "\",\n";
                out << "      \"resultSummary\": \"" << EscapeJsonString(t.resultSummary) << "\",\n";
                out << "      \"bytesFreed\": " << t.bytesFreed << ",\n";
                out << "      \"filesProcessed\": " << t.filesProcessed << ",\n";
                out << "      \"filesSkipped\": " << t.filesSkipped << ",\n";
                out << "      \"processesHandled\": " << t.processesHandled << ",\n";
                out << "      \"startSec\": " << startSec << ",\n";
                out << "      \"finishSec\": " << finishSec << ",\n";
                out << "      \"percent\": " << t.percent << "\n";
                out << "    }" << (i + 1 < tasks.size() ? "," : "") << "\n";
            }

            out << "  ]\n";
            out << "}\n";
            out.close();
        } catch (...) {}
    }

    std::string tasksFilePath;
    mutable std::mutex mtx_;
    std::vector<TaskEntry> tasks;
    uint64_t nextId = 0;
};

class TaskScope {
public:
    TaskScope(uint64_t id, const std::string& name) : id_(id), name_(name) {}
    ~TaskScope() {
        if (completed_) return;
        try {
            TaskHistoryRegistry::Instance().MarkFailed(id_, std::string("Worker scope ended unexpectedly: ") + name_);
        } catch (...) {}
    }

    void Complete(const std::string& summary = "",
                  uintmax_t bytesFreed = 0,
                  size_t filesProcessed = 0,
                  size_t filesSkipped = 0,
                  size_t processesHandled = 0) {
        TaskHistoryRegistry::Instance().MarkCompleted(id_, summary, bytesFreed, filesProcessed, filesSkipped, processesHandled);
        completed_ = true;
    }
    void Fail(const std::string& err) {
        TaskHistoryRegistry::Instance().MarkFailed(id_, err);
        completed_ = true;
    }
    void Cancel() {
        TaskHistoryRegistry::Instance().MarkCancelled(id_);
        completed_ = true;
    }

private:
    uint64_t id_;
    std::string name_;
    bool completed_ = false;
};

} // namespace TaskHistoryNS

using TaskHistory = TaskHistoryNS::TaskHistoryRegistry;
using TaskScope   = TaskHistoryNS::TaskScope;
using TaskEntry   = TaskHistoryNS::TaskEntry;
using TaskStatus  = TaskHistoryNS::Status;
