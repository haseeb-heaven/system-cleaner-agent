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

    // Helper to check if a directory name is sensitive or should be skipped during recursion
    bool ShouldSkipRecursion(const std::string& dirName) {
        static const std::vector<std::string> skipNames = {
            "appdata", "documents", "downloads", "desktop", "pictures", "music", "videos",
            "onedrive", "contacts", "searches", "links", "saved games", "favorites",
            ".git", ".github", ".vscode", ".idea", ".gradle", ".nuget", ".cargo", "node_modules",
            "venv", ".venv", "env", "target", "ipch", ".expo", ".svelte-kit", ".docusaurus",
            ".vercel", ".turbo"
        };
        std::string lowerName = dirName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        return std::find(skipNames.begin(), skipNames.end(), lowerName) != skipNames.end();
    }

    void FindRecursiveTargets(const fs::path& root, const std::vector<std::string>& dirNames, std::vector<fs::path>& found) {
        try {
            auto options = fs::directory_options::skip_permission_denied;
            std::error_code ec;
            auto it = fs::recursive_directory_iterator(root, options, ec);
            if (ec) {
                Logger::Instance().Warn("Error starting traversal of " + root.string() + ": " + ec.message());
                return;
            }

            while (it != fs::recursive_directory_iterator()) {
                try {
                    std::error_code entry_ec;
                    bool is_dir = it->is_directory(entry_ec);
                    if (!entry_ec && is_dir) {
                        std::string currentDirName = it->path().filename().string();
                        std::string lowerName = currentDirName;
                        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

                        if (ShouldSkipRecursion(lowerName)) {
                            // Don't traverse deeper into skipped/sensitive dirs
                            it.disable_recursion_pending();
                            
                            // If this was a targeted folder name, we still want to clean it
                            if (std::find(dirNames.begin(), dirNames.end(), lowerName) != dirNames.end()) {
                                found.push_back(it->path());
                            }
                            
                            it.increment(ec);
                            if (ec) ec.clear();
                            continue;
                        }
                        
                        if (std::find(dirNames.begin(), dirNames.end(), lowerName) != dirNames.end()) {
                            found.push_back(it->path());
                            it.disable_recursion_pending();
                        }
                    }
                } catch (...) {}

                it.increment(ec);
                if (ec) {
                    ec.clear();
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
            std::error_code ec;
            auto it = fs::recursive_directory_iterator(dir, options, ec);
            if (ec) return 0;
            
            while (it != fs::recursive_directory_iterator()) {
                try {
                    std::error_code entry_ec;
                    bool is_file = it->is_regular_file(entry_ec);
                    if (!entry_ec && is_file) {
                        uintmax_t fsize = fs::file_size(*it, entry_ec);
                        if (!entry_ec) size += fsize;
                    }
                } catch(...) {}
                it.increment(ec);
                if (ec) {
                    ec.clear();
                }
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
            std::error_code ec;
            auto it = fs::directory_iterator(dir, options, ec);
            if (ec) return 0;

            while (it != fs::directory_iterator()) {
                try {
                    std::error_code entry_ec;
                    bool is_file = it->is_regular_file(entry_ec);
                    if (!entry_ec && is_file) {
                        uintmax_t fsize = fs::file_size(*it, entry_ec);
                        if (!entry_ec && fs::remove(*it, entry_ec)) {
                            freed += fsize;
                        }
                    } else {
                        bool is_dir = it->is_directory(entry_ec);
                        if (!entry_ec && is_dir) {
                            uintmax_t dsize = GetDirectorySize(it->path());
                            fs::remove_all(it->path(), entry_ec);
                            if (!entry_ec) {
                                freed += dsize;
                            }
                        }
                    }
                } catch (...) {}
                it.increment(ec);
                if (ec) {
                    ec.clear();
                }
            }
            if (removeRoot) {
                std::error_code remove_ec;
                fs::remove(dir, remove_ec);
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
        std::string programData = GetEnv("PROGRAMDATA");
        std::string appData = GetEnv("APPDATA");

        if (!temp.empty()) targets.push_back({temp, false, "Windows User Temp"});
        if (!winDir.empty()) {
            targets.push_back({winDir + "\\Temp", false, "Windows System Temp"});
            targets.push_back({winDir + "\\SoftwareDistribution\\Download", false, "Windows Update Downloads"});
            targets.push_back({winDir + "\\Prefetch", false, "Windows Prefetch"});
        }
        if (!programData.empty()) {
            targets.push_back({programData + "\\Microsoft\\Windows\\WER\\ReportArchive", false, "WER Archive"});
            targets.push_back({programData + "\\Microsoft\\Windows\\WER\\ReportQueue", false, "WER Queue"});
        }
        if (!localAppData.empty()) {
            targets.push_back({localAppData + "\\npm-cache", false, "npm cache"});
            targets.push_back({localAppData + "\\uv\\cache", false, "uv cache"});
            targets.push_back({localAppData + "\\pip\\Cache", false, "pip cache"});
            targets.push_back({localAppData + "\\Yarn\\Cache", false, "Yarn cache"});
            targets.push_back({localAppData + "\\CrashDumps", false, "Crash Dumps"});
            targets.push_back({localAppData + "\\D3DSCache", false, "DirectX Shader Cache"});
            targets.push_back({localAppData + "\\go-build", false, "Go Build Cache"});
            
            // Web Browser Caches (Chrome, Edge)
            targets.push_back({localAppData + "\\Google\\Chrome\\User Data\\OptGuideOnDeviceModel", false, "Chrome On-Device AI Models"});
            targets.push_back({localAppData + "\\Google\\Chrome\\User Data\\Default\\Cache", false, "Chrome Web Cache"});
            targets.push_back({localAppData + "\\Google\\Chrome\\User Data\\Default\\Code Cache", false, "Chrome Code Cache"});
            targets.push_back({localAppData + "\\Google\\Chrome\\User Data\\Default\\GPUCache", false, "Chrome GPU Cache"});
            targets.push_back({localAppData + "\\Microsoft\\Edge\\User Data\\Default\\Cache", false, "Edge Web Cache"});
            targets.push_back({localAppData + "\\Microsoft\\Edge\\User Data\\Default\\Code Cache", false, "Edge Code Cache"});
            targets.push_back({localAppData + "\\Microsoft\\Edge\\User Data\\Default\\GPUCache", false, "Edge GPU Cache"});
            targets.push_back({localAppData + "\\Microsoft\\WinGet\\Packages", false, "WinGet Downloaded Packages"});
            
            // Other common app caches
            targets.push_back({localAppData + "\\Spotify\\Storage", false, "Spotify Cache"});
            targets.push_back({localAppData + "\\Composer\\cache", false, "Composer Cache"});
            targets.push_back({localAppData + "\\Cypress\\Cache", false, "Cypress Cache"});
        }
        if (!appData.empty()) {
            // Discord & Slack Cache
            targets.push_back({appData + "\\discord\\Cache", false, "Discord Cache"});
            targets.push_back({appData + "\\discord\\Code Cache", false, "Discord Code Cache"});
            targets.push_back({appData + "\\discord\\GPUCache", false, "Discord GPU Cache"});
            targets.push_back({appData + "\\Slack\\Cache", false, "Slack Cache"});
            targets.push_back({appData + "\\Slack\\Code Cache", false, "Slack Code Cache"});
            
            // VS Code Cache
            targets.push_back({appData + "\\Code\\Cache", false, "VS Code Cache"});
            targets.push_back({appData + "\\Code\\CachedData", false, "VS Code Cached Data"});
        }
        if (!userProfile.empty()) {
            targets.push_back({userProfile + "\\.cache", false, ".cache folder"});
            targets.push_back({userProfile + "\\.npm", false, ".npm folder"});
            targets.push_back({userProfile + "\\.nuget\\packages", false, "NuGet Cache"});
            targets.push_back({userProfile + "\\.cargo\\registry\\cache", false, "Cargo Cache"});
            targets.push_back({userProfile + "\\.gradle\\caches", false, "Gradle Cache"});
            
            // Safer deep cleanup of user projects (will automatically skip AppData, Documents, etc.)
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
                    "dist", "build", "target", "ipch", ".expo", ".svelte-kit", 
                    ".docusaurus", ".vercel", ".turbo"
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
                    "dist", "build", "target", "ipch", ".expo", ".svelte-kit", 
                    ".docusaurus", ".vercel", ".turbo"
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
