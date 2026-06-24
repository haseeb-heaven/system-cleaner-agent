#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>

class Logger {
    std::ofstream file;
    std::mutex mtx;

    std::string CurrentTime() {
        auto now = std::chrono::system_clock::now();
        std::time_t in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm buf;
#ifdef _WIN32
        localtime_s(&buf, &in_time_t);
#else
        localtime_r(&in_time_t, &buf);
#endif
        char str[100];
        std::strftime(str, sizeof(str), "%Y-%m-%d %H:%M:%S", &buf);
        return str;
    }

public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void Init(const std::string& filepath) {
        file.open(filepath, std::ios::app);
    }

    void Log(const std::string& level, const std::string& msg) {
        std::lock_guard<std::mutex> lock(mtx);
        std::string timeStr = CurrentTime();
        std::string formatted = "[" + timeStr + "] [" + level + "] " + msg;
        std::cout << formatted << std::endl;
        if (file.is_open()) {
            file << formatted << std::endl;
        }
    }

    void Info(const std::string& msg) { Log("INFO", msg); }
    void Warn(const std::string& msg) { Log("WARN", msg); }
    void Error(const std::string& msg) { Log("ERROR", msg); }
};
