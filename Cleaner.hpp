#pragma once
#include "Logger.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <cstdlib>
#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;

struct Target {
    fs::path path;
    bool isRecursive;
    std::string name;
};

class Cleaner {
    std::vector<Target> targets;

    std::string GetEnv(const char* name) {
#ifdef _WIN32
        char* val = nullptr;
        size_t len = 0;
        _dupenv_s(&val, &len, name);
        std::string res = val ? val : "";
        if (val) free(val);
        return res;
#else
        char* val = std::getenv(name);
        return val ? std::string(val) : "";
#endif
    }

    void FindRecursiveTargets(const fs::path& root, const std::vector<std::string>& dirNames, std::vector<fs::path>& found) {
        try {
            auto options = fs::directory_options::skip_permission_denied;
            for (auto it = fs::recursive_directory_iterator(root, options); it != fs::recursive_directory_iterator(); ++it) {
                try {
                    if (it->is_directory()) {
                        std::string currentDirName = it->path().filename().string();
                        if (std::find(dirNames.begin(), dirNames.end(), currentDirName) != dirNames.end()) {
                            found.push_back(it->path());
                            it.disable_recursion_pending();
                        }
                    }
                } catch (const std::system_error&) {
                    // Ignore access errors on individual files/dirs
                }
            }
        } catch (const std::exception& e) {
            Logger::Instance().Warn("Error traversing " + root.string() + ": " + std::string(e.what()));
        }
    }

    uintmax_t GetDirectorySize(const fs::path& dir) {
        uintmax_t size = 0;
        try {
            auto options = fs::directory_options::skip_permission_denied;
            for (const auto& entry : fs::recursive_directory_iterator(dir, options)) {
                try {
                    if (entry.is_regular_file()) {
                        std::error_code ec;
                        uintmax_t fsize = fs::file_size(entry, ec);
                        if (!ec) size += fsize;
                    }
                } catch(...) {}
            }
        } catch (const std::exception& e) {
            Logger::Instance().Warn("Could not calculate size for " + dir.string() + ": " + std::string(e.what()));
        }
        return size;
    }

    uintmax_t CleanDirectory(const fs::path& dir, bool removeRoot) {
        uintmax_t freed = 0;
        try {
            auto options = fs::directory_options::skip_permission_denied;
            for (const auto& entry : fs::directory_iterator(dir, options)) {
                try {
                    std::error_code ec;
                    if (entry.is_regular_file(ec)) {
                        uintmax_t fsize = fs::file_size(entry, ec);
                        if (!ec && fs::remove(entry, ec)) {
                            freed += fsize;
                        }
                    } else if (entry.is_directory(ec)) {
                        uintmax_t dsize = GetDirectorySize(entry.path());
                        std::uintmax_t removedCount = fs::remove_all(entry, ec);
                        if (!ec && removedCount != static_cast<std::uintmax_t>(-1)) {
                            freed += dsize;
                        }
                    }
                } catch (const std::exception&) {}
            }
            if (removeRoot) {
                std::error_code ec;
                fs::remove(dir, ec);
            }
        } catch (const std::exception& e) {
            Logger::Instance().Warn("Could not fully clean " + dir.string() + ": " + std::string(e.what()));
        }
        return freed;
    }

public:
    Cleaner() {
        std::string temp = GetEnv("TEMP");
        std::string winDir = GetEnv("WINDIR");
        std::string localAppData = GetEnv("LOCALAPPDATA");
        std::string userProfile = GetEnv("USERPROFILE");

        if (!temp.empty()) targets.push_back({temp, false, "Windows User Temp"});
        if (!winDir.empty()) targets.push_back({winDir + "\\Temp", false, "Windows System Temp"});
        if (!localAppData.empty()) {
            targets.push_back({localAppData + "\\npm-cache", false, "npm cache"});
            targets.push_back({localAppData + "\\uv\\cache", false, "uv cache"});
            targets.push_back({localAppData + "\\pip\\Cache", false, "pip cache"});
            targets.push_back({localAppData + "\\Yarn\\Cache", false, "Yarn cache"});
        }
        if (!userProfile.empty()) {
            targets.push_back({userProfile + "\\.cache", false, ".cache folder"});
            targets.push_back({userProfile + "\\.npm", false, ".npm folder"});
            // Deep targets (recursive)
            targets.push_back({userProfile, true, "User Profile Project Caches"});
        }
    }

    void Scan() {
        Logger::Instance().Info("Starting scan...");
        uintmax_t grandTotal = 0;
        
        for (const auto& target : targets) {
            if (!target.isRecursive) {
                if (fs::exists(target.path)) {
                    uintmax_t size = GetDirectorySize(target.path);
                    grandTotal += size;
                    Logger::Instance().Info("Found: " + target.path.string() + " | Size: " + std::to_string(size / (1024 * 1024)) + " MB");
                }
            } else {
                Logger::Instance().Info("Searching for project caches recursively in " + target.path.string() + "...");
                std::vector<std::string> targetNames = {
                    "node_modules", "venv", ".venv", "env", "__pycache__", 
                    ".pytest_cache", ".next", ".nuxt", ".cache", ".sass-cache", 
                    "dist", "build"
                };
                std::vector<fs::path> foundDirs;
                FindRecursiveTargets(target.path, targetNames, foundDirs);
                
                uintmax_t recursiveTotal = 0;
                for (const auto& dir : foundDirs) {
                    uintmax_t size = GetDirectorySize(dir);
                    recursiveTotal += size;
                }
                grandTotal += recursiveTotal;
                Logger::Instance().Info("Found " + std::to_string(foundDirs.size()) + " project cache directories | Total Size: " + std::to_string(recursiveTotal / (1024 * 1024)) + " MB");
            }
        }
        Logger::Instance().Info("Total space that can be freed: " + std::to_string(grandTotal / (1024 * 1024)) + " MB");
    }

    void Clean() {
        Logger::Instance().Info("Starting cleanup...");
        uintmax_t totalFreed = 0;
        
        for (const auto& target : targets) {
            if (!target.isRecursive) {
                if (fs::exists(target.path)) {
                    Logger::Instance().Info("Cleaning " + target.path.string());
                    totalFreed += CleanDirectory(target.path, false);
                }
            } else {
                Logger::Instance().Info("Cleaning project caches recursively in " + target.path.string() + "...");
                std::vector<std::string> targetNames = {
                    "node_modules", "venv", ".venv", "env", "__pycache__", 
                    ".pytest_cache", ".next", ".nuxt", ".cache", ".sass-cache", 
                    "dist", "build"
                };
                std::vector<fs::path> foundDirs;
                FindRecursiveTargets(target.path, targetNames, foundDirs);
                
                for (const auto& dir : foundDirs) {
                    Logger::Instance().Info("Removing " + dir.string());
                    totalFreed += CleanDirectory(dir, true);
                }
            }
        }
        Logger::Instance().Info("Cleanup completed. Total freed: " + std::to_string(totalFreed / (1024 * 1024)) + " MB");
    }
};
