#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERR
};

class Logger {
    std::ofstream file;
    std::mutex mtx;
    std::string logFilePath;
    bool verbose = false;
    bool colorEnabled = true;
    bool tuiActive = false;

    std::string CurrentTime() {
        auto now = std::chrono::system_clock::now();
        std::time_t in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf{};
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        char str[100];
        std::strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", &buf);
        return str;
    }

    void CheckLogSizeLimit() {
        try {
            if (!logFilePath.empty() && std::filesystem::exists(logFilePath)) {
                if (std::filesystem::file_size(logFilePath) > 5 * 1024 * 1024) { // 5 MB rotation limit
                    file.close();
                    std::filesystem::remove(logFilePath);
                    file.open(logFilePath, std::ios::out);
                }
            }
        } catch (...) {}
    }

public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void Init(const std::string& filepath, bool enableVerbose = false, bool enableColor = true) {
        logFilePath = filepath;
        verbose = enableVerbose;
        colorEnabled = enableColor;
        CheckLogSizeLimit();
        if (file.is_open()) file.close();
        file.open(filepath, std::ios::app);
        if (!file.is_open()) {
            file.open("system-cleaner-agent.log", std::ios::app);
        }
    }

    void SetVerbose(bool v) { verbose = v; }
    void SetColor(bool c) { colorEnabled = c; }
    void SetTUIActive(bool active) { tuiActive = active; }

    void Log(LogLevel level, const std::string& msg, bool consoleOnly = false) {
        std::lock_guard<std::mutex> lock(mtx);
        std::string timeStr = CurrentTime();
        std::string lvlStr;
        std::string colorCode;

        switch (level) {
            case LogLevel::DEBUG: lvlStr = "DEBUG"; colorCode = "\033[36m"; break; // Cyan
            case LogLevel::INFO:  lvlStr = "INFO "; colorCode = "\033[32m"; break; // Green
            case LogLevel::WARN:  lvlStr = "WARN "; colorCode = "\033[33m"; break; // Yellow
            case LogLevel::ERR:   lvlStr = "ERROR"; colorCode = "\033[31m"; break; // Red
        }

        std::string formattedLog = "[" + timeStr + "] [" + lvlStr + "] " + msg;

        if (!tuiActive) {
            if (colorEnabled) {
                std::cout << colorCode << formattedLog << "\033[0m" << std::endl;
            } else {
                std::cout << formattedLog << std::endl;
            }
        }

        if (!consoleOnly) {
            if (!file.is_open()) {
                file.open(logFilePath.empty() ? "system-cleaner-agent.log" : logFilePath, std::ios::app);
            }
            if (file.is_open()) {
                file << formattedLog << "\n" << std::flush;
            }
        }
    }

    void Info(const std::string& msg) { Log(LogLevel::INFO, msg); }
    void Warn(const std::string& msg) { Log(LogLevel::WARN, msg); }
    void Error(const std::string& msg) { Log(LogLevel::ERR, msg); }
    void Debug(const std::string& msg) {
        if (verbose) Log(LogLevel::DEBUG, msg);
    }
};
