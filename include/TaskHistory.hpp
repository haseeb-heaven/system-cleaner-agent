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
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace TaskHistoryNS {

enum class Status {
    Queued,
    Running,
    Completed,
    Failed,
    Cancelled,
    Unknown
};

inline std::string StatusLabel(Status s) {
    switch (s) {
        case Status::Queued:    return "QUEUED";
        case Status::Running:   return "RUNNING";
        case Status::Completed: return "OK";
        case Status::Failed:    return "FAILED";
        case Status::Cancelled: return "CANCEL";
        default:                return "?";
    }
}

inline const char* StatusColor(Status s) {
    switch (s) {
        case Status::Queued:    return "\033[1;33m";
        case Status::Running:   return "\033[1;36m";
        case Status::Completed: return "\033[1;32m";
        case Status::Failed:    return "\033[1;31m";
        case Status::Cancelled: return "\033[1;35m";
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
        return e.id;
    }

    void MarkRunning(uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return;
        e->status = Status::Running;
        e->startedAt = std::chrono::system_clock::now();
        e->threadId  = std::this_thread::get_id();
    }

    void UpdateProgress(uint64_t id, const std::string& msg, double percent = -1.0) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return;
        e->progressMsg = msg;
        if (percent >= 0.0) e->percent = percent;
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
    }

    void MarkFailed(uint64_t id, const std::string& errorMsg) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return;
        e->status        = Status::Failed;
        e->finishedAt    = std::chrono::system_clock::now();
        e->resultSummary = "Error: " + errorMsg;
    }

    void MarkCancelled(uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto* e = Find(id);
        if (!e) return;
        e->status        = Status::Cancelled;
        e->finishedAt    = std::chrono::system_clock::now();
        e->resultSummary = "Cancelled by user/system.";
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

        ss << "[" << std::setw(3) << std::setfill('0') << t.id << "] "
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
