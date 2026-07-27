#pragma once
#include "Logger.hpp"
#include <filesystem>
#include <string>
#include <algorithm>
#include <set>
#include <vector>
#include <fstream>
#include <chrono>
#include <cctype>
#include <regex>

namespace fs = std::filesystem;

enum class FileJunkType {
    DefiniteJunk,      // Verified temporary / log / cache file -> safe to delete
    ProtectedUserFile, // Source code, document, database, or project asset -> NEVER DELETE!
    Unknown            // Requires age / location fallback
};

struct InspectionConfig {
    std::set<std::string> includeCategories;
    std::set<std::string> excludeCategories;
    std::set<std::string> onlyExts;
    std::set<std::string> excludeExts;
    std::vector<std::string> includePatterns;
    std::vector<std::string> excludePatterns;
    uintmax_t minSizeBytes = 0;
    uintmax_t maxSizeBytes = 0; // 0 = unlimited
    long long minAgeMinutes = 60; // 1 hour default
    std::string strategy = "smart"; // smart, force, safe
};

class ContentInspector {
public:
    static bool IsHexHashFilename(const std::string& name) {
        if (name.length() != 40 && name.length() != 64) return false;
        return std::all_of(name.begin(), name.end(), [](unsigned char c) {
            return std::isxdigit(c);
        });
    }

    static std::string ReadMagicBytes(const fs::path& filePath, size_t count = 16) {
        try {
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) return "";
            std::string buffer(count, '\0');
            file.read(&buffer[0], count);
            buffer.resize(file.gcount());
            return buffer;
        } catch (...) {
            return "";
        }
    }

    static long long ParseDurationToMinutes(const std::string& durStr) {
        if (durStr.empty()) return 0;
        char unit = durStr.back();
        std::string numStr = durStr.substr(0, durStr.size() - 1);
        try {
            long long val = std::stoll(numStr);
            if (unit == 'm' || unit == 'M') return val;
            if (unit == 'h' || unit == 'H') return val * 60;
            if (unit == 'd' || unit == 'D') return val * 60 * 24;
            if (unit == 'w' || unit == 'W') return val * 60 * 24 * 7;
        } catch (...) {}
        return 0;
    }

    static uintmax_t ParseSizeToBytes(const std::string& sizeStr) {
        if (sizeStr.empty()) return 0;
        std::string s = sizeStr;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        uintmax_t mult = 1;
        if (s.back() == 'b') s.pop_back();
        if (s.empty()) return 0;
        char unit = s.back();
        if (unit == 'k') { mult = 1024; s.pop_back(); }
        else if (unit == 'm') { mult = 1024 * 1024; s.pop_back(); }
        else if (unit == 'g') { mult = 1024 * 1024 * 1024; s.pop_back(); }
        try {
            return std::stoull(s) * mult;
        } catch (...) {}
        return 0;
    }

    static FileJunkType InspectFile(const fs::path& filePath, const InspectionConfig& config) {
        std::error_code ec;
        if (!fs::exists(filePath, ec) || fs::is_symlink(filePath, ec)) {
            return FileJunkType::Unknown;
        }

        std::string ext = filePath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        std::string filename = filePath.filename().string();
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

        std::string fullPath = filePath.string();
        std::transform(fullPath.begin(), fullPath.end(), fullPath.begin(), ::tolower);

        // Size filtering
        uintmax_t fsize = fs::file_size(filePath, ec);
        if (!ec) {
            if (config.minSizeBytes > 0 && fsize < config.minSizeBytes) return FileJunkType::ProtectedUserFile;
            if (config.maxSizeBytes > 0 && fsize > config.maxSizeBytes) return FileJunkType::ProtectedUserFile;
        }

        // Only/Exclude Ext filters
        if (!config.onlyExts.empty() && config.onlyExts.count(ext) == 0) {
            return FileJunkType::ProtectedUserFile;
        }
        if (!config.excludeExts.empty() && config.excludeExts.count(ext) > 0) {
            return FileJunkType::ProtectedUserFile;
        }

        // Pattern matching
        for (const auto& pat : config.excludePatterns) {
            std::string pLower = pat;
            std::transform(pLower.begin(), pLower.end(), pLower.begin(), ::tolower);
            if (fullPath.find(pLower) != std::string::npos) return FileJunkType::ProtectedUserFile;
        }

        // Force strategy bypasses content rules
        if (config.strategy == "force") {
            return FileJunkType::DefiniteJunk;
        }

        // Safe strategy: strictly conservative
        if (config.strategy == "safe") {
            static const std::set<std::string> strictSafeExts = { ".tmp", ".temp", ".log", ".dmp" };
            if (strictSafeExts.count(ext) == 0) return FileJunkType::ProtectedUserFile;
        }

        // Protected Extensions
        static const std::set<std::string> protectedExts = {
            ".py", ".cpp", ".c", ".h", ".hpp", ".cs", ".java", ".js", ".ts", ".tsx", ".jsx",
            ".html", ".css", ".json", ".sql", ".sh", ".bat", ".cmd", ".ps1", ".md",
            ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx", ".zip", ".rar", ".7z",
            ".tar", ".gz", ".png", ".jpg", ".jpeg", ".svg", ".mp4", ".mkv", ".mp3", ".wav",
            ".key", ".pem", ".crt", ".db", ".sqlite", ".sln", ".vcxproj", ".rs", ".go", ".kt"
        };

        if (protectedExts.count(ext) > 0) {
            if (fullPath.find("__pycache__") == std::string::npos &&
                fullPath.find("node_modules") == std::string::npos &&
                fullPath.find("code cache") == std::string::npos &&
                fullPath.find("gpu-cache") == std::string::npos) {
                return FileJunkType::ProtectedUserFile;
            }
        }

        // Definite Junk Extensions
        static const std::set<std::string> junkExts = {
            ".tmp", ".temp", ".bak", ".old", ".log", ".dmp", ".mdmp", ".wer", ".chk",
            ".crash", ".swp", ".swo", ".part", ".crdownload", ".pyc", ".pyo", ".coverage",
            ".obj", ".o", ".pch", ".ipch", ".idb", ".pdb", ".ilk", ".tlog", ".lastbuildstate"
        };

        if (junkExts.count(ext) > 0 ||
            filename.find("thumbcache") != std::string::npos ||
            filename.find("iconcache") != std::string::npos) {
            return FileJunkType::DefiniteJunk;
        }

        if (IsHexHashFilename(filename)) {
            return FileJunkType::DefiniteJunk;
        }

        // Magic Bytes Check
        std::string magic = ReadMagicBytes(filePath, 16);
        if (magic.size() >= 4) {
            if (magic.rfind("%PDF", 0) == 0) return FileJunkType::ProtectedUserFile;
            if (magic.size() >= 4 && magic[0] == '\x89' && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') return FileJunkType::ProtectedUserFile;
            if (magic.size() >= 3 && static_cast<unsigned char>(magic[0]) == 0xFF && static_cast<unsigned char>(magic[1]) == 0xD8 && static_cast<unsigned char>(magic[2]) == 0xFF) return FileJunkType::ProtectedUserFile;
            if (magic.rfind("SQLite format 3", 0) == 0) return FileJunkType::ProtectedUserFile;
        }

        // Age check
        if (config.minAgeMinutes > 0) {
            try {
                auto lastWrite = fs::last_write_time(filePath);
                auto now = fs::file_time_type::clock::now();
                auto ageMin = std::chrono::duration_cast<std::chrono::minutes>(now - lastWrite).count();
                if (ageMin < config.minAgeMinutes) {
                    return FileJunkType::ProtectedUserFile;
                }
            } catch (...) {}
        }

        return FileJunkType::DefiniteJunk;
    }

    static bool ContainsProtectedUserFiles(const fs::path& dirPath, const InspectionConfig& config) {
        try {
            std::error_code ec;
            auto options = fs::directory_options::skip_permission_denied;
            auto it = fs::recursive_directory_iterator(dirPath, options, ec);
            if (ec) return true;

            while (it != fs::recursive_directory_iterator()) {
                try {
                    std::error_code entry_ec;
                    if (it->is_regular_file(entry_ec)) {
                        if (InspectFile(it->path(), config) == FileJunkType::ProtectedUserFile) {
                            return true;
                        }
                    }
                } catch (...) {}
                it.increment(ec);
                if (ec) ec.clear();
            }
        } catch (...) {}
        return false;
    }
};
