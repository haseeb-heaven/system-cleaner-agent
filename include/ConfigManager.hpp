#pragma once
#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include "Logger.hpp"
#include "gtlibc.hpp"

namespace fs = std::filesystem;

struct AppConfig {
    bool sandboxMode = false;
    bool pathProtection = true;
    bool dryRun = false;
    bool killLocks = true;
    bool enableLogging = true;
    std::string logLevel = "INFO"; // INFO, WARN, ERROR, VERBOSE
    int monitorIntervalSec = 5;
    size_t ramThresholdMB = 200;
    std::string customPathsStr = "C:\\Users\\hasee\\AppData\\Local\\Temp";
    std::string tuiThemeEngine = "OpenTUI"; // OpenTUI, TermOx, FTXUI
    std::string tuiColorScheme = "Cyan Matrix"; // Preset Palette
    std::string tuiFgColor = "Default"; // Default, Cyan, Electric Magenta, Amber Gold, Emerald Green, Neon Pink, Bright White, Yellow, Royal Blue
    std::string tuiBgColor = "Default"; // Default, Black, Navy Blue, Electric Magenta, Amber Gold, Emerald Green, Dark Slate, Charcoal Gray
    std::vector<std::string> customProtectedProcesses;
};

class ConfigManager {
public:
    static std::string GetConfigFilePath() {
        // Look for cleaner_config.json in current directory or user home directory
        std::string filename = "cleaner_config.json";
        if (fs::exists(filename)) return filename;

#ifdef _WIN32
        const char* localAppData = std::getenv("LOCALAPPDATA");
        if (localAppData && std::string(localAppData).length() > 0) {
            fs::path appDir = fs::path(localAppData) / "system-cleaner-agent";
            std::error_code ec;
            fs::create_directories(appDir, ec);
            return (appDir / filename).string();
        }
#endif
        return filename;
    }

    static AppConfig Load(const std::string& path = "") {
        AppConfig cfg;
        std::string filePath = path.empty() ? GetConfigFilePath() : path;

        if (!fs::exists(filePath)) {
            Logger::Instance().Info("Config file not found at " + filePath + ". Creating default cleaner_config.json.");
            Save(cfg, filePath);
            return cfg;
        }

        std::ifstream file(filePath);
        if (!file.is_open()) {
            Logger::Instance().Warn("Could not open config file " + filePath + ". Using default settings.");
            return cfg;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        // Helper Lambda to extract string value by key from JSON
        auto getJsonBool = [&](const std::string& key, bool defaultVal) -> bool {
            size_t pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return defaultVal;
            size_t colon = content.find(':', pos);
            if (colon == std::string::npos) return defaultVal;
            std::string valStr = content.substr(colon + 1, 20);
            if (valStr.find("true") != std::string::npos) return true;
            if (valStr.find("false") != std::string::npos) return false;
            return defaultVal;
        };

        auto getJsonInt = [&](const std::string& key, int defaultVal) -> int {
            size_t pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return defaultVal;
            size_t colon = content.find(':', pos);
            if (colon == std::string::npos) return defaultVal;
            try {
                return std::stoi(content.substr(colon + 1));
            } catch (...) {
                return defaultVal;
            }
        };

        auto getJsonString = [&](const std::string& key, const std::string& defaultVal) -> std::string {
            size_t pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return defaultVal;
            size_t startQuote = content.find('"', content.find(':', pos));
            if (startQuote == std::string::npos) return defaultVal;
            size_t endQuote = content.find('"', startQuote + 1);
            if (endQuote == std::string::npos) return defaultVal;
            std::string strVal = content.substr(startQuote + 1, endQuote - startQuote - 1);
            std::string unescaped = "";
            for (size_t i = 0; i < strVal.length(); ++i) {
                if (strVal[i] == '\\' && i + 1 < strVal.length() && strVal[i+1] == '\\') {
                    unescaped += '\\';
                    i++;
                } else {
                    unescaped += strVal[i];
                }
            }
            return unescaped;
        };

        cfg.sandboxMode = getJsonBool("sandboxMode", false);
        cfg.pathProtection = getJsonBool("pathProtection", true);
        cfg.dryRun = getJsonBool("dryRun", false);
        cfg.killLocks = getJsonBool("killLocks", true);
        cfg.enableLogging = getJsonBool("enableLogging", true);
        cfg.logLevel = getJsonString("logLevel", "INFO");
        cfg.monitorIntervalSec = getJsonInt("monitorIntervalSec", 5);
        cfg.ramThresholdMB = static_cast<size_t>(getJsonInt("ramThresholdMB", 200));
        cfg.customPathsStr = getJsonString("customPathsStr", "C:\\Users\\hasee\\AppData\\Local\\Temp");
        cfg.tuiThemeEngine = getJsonString("tuiThemeEngine", "OpenTUI");
        cfg.tuiColorScheme = getJsonString("tuiColorScheme", "Cyan Matrix");
        cfg.tuiFgColor = getJsonString("tuiFgColor", "Default");
        cfg.tuiBgColor = getJsonString("tuiBgColor", "Default");

        // Parse customProtectedProcesses array
        size_t arrayPos = content.find("\"customProtectedProcesses\"");
        if (arrayPos != std::string::npos) {
            size_t openBracket = content.find('[', arrayPos);
            size_t closeBracket = content.find(']', openBracket);
            if (openBracket != std::string::npos && closeBracket != std::string::npos) {
                std::string arrStr = content.substr(openBracket + 1, closeBracket - openBracket - 1);
                std::stringstream ss(arrStr);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    size_t s1 = item.find('"');
                    size_t s2 = item.find('"', s1 + 1);
                    if (s1 != std::string::npos && s2 != std::string::npos) {
                        std::string proc = item.substr(s1 + 1, s2 - s1 - 1);
                        if (!proc.empty()) {
                            cfg.customProtectedProcesses.push_back(proc);
                            GTLIBC::GTLibc::AddCustomProtectedProcess(proc);
                        }
                    }
                }
            }
        }

        Logger::Instance().Info("Successfully loaded persistent configuration from " + filePath);
        return cfg;
    }

    static bool Save(const AppConfig& cfg, const std::string& path = "") {
        std::string filePath = path.empty() ? GetConfigFilePath() : path;
        std::ofstream file(filePath);
        if (!file.is_open()) {
            Logger::Instance().Warn("Failed to open config file for writing: " + filePath);
            return false;
        }

        std::ostringstream json;
        json << "{\n";
        json << "  \"sandboxMode\": " << (cfg.sandboxMode ? "true" : "false") << ",\n";
        json << "  \"pathProtection\": " << (cfg.pathProtection ? "true" : "false") << ",\n";
        json << "  \"dryRun\": " << (cfg.dryRun ? "true" : "false") << ",\n";
        json << "  \"killLocks\": " << (cfg.killLocks ? "true" : "false") << ",\n";
        json << "  \"enableLogging\": " << (cfg.enableLogging ? "true" : "false") << ",\n";
        json << "  \"logLevel\": \"" << cfg.logLevel << "\",\n";
        json << "  \"monitorIntervalSec\": " << cfg.monitorIntervalSec << ",\n";
        json << "  \"ramThresholdMB\": " << cfg.ramThresholdMB << ",\n";
        
        // Escape backslashes for JSON path strings
        std::string escapedPath = "";
        for (char c : cfg.customPathsStr) {
            if (c == '\\') escapedPath += "\\\\";
            else escapedPath += c;
        }
        json << "  \"customPathsStr\": \"" << escapedPath << "\",\n";
        json << "  \"tuiThemeEngine\": \"" << cfg.tuiThemeEngine << "\",\n";
        json << "  \"tuiColorScheme\": \"" << cfg.tuiColorScheme << "\",\n";
        json << "  \"tuiFgColor\": \"" << cfg.tuiFgColor << "\",\n";
        json << "  \"tuiBgColor\": \"" << cfg.tuiBgColor << "\",\n";

        json << "  \"customProtectedProcesses\": [";
        for (size_t i = 0; i < cfg.customProtectedProcesses.size(); ++i) {
            json << "\"" << cfg.customProtectedProcesses[i] << "\"";
            if (i + 1 < cfg.customProtectedProcesses.size()) json << ", ";
        }
        json << "]\n";
        json << "}\n";

        file << json.str();
        file.close();
        Logger::Instance().Info("Successfully saved persistent configuration to " + filePath);
        return true;
    }
};
