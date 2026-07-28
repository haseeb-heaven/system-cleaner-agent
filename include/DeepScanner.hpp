#pragma once

#include "Cleaner.hpp"
#include "Logger.hpp"
#include "SecurityGuard.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <map>
#include <iomanip>
#include <functional>
#include <chrono>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/statvfs.h>
#endif

namespace fs = std::filesystem;

struct DeepScanNode {
    fs::path path;
    std::string name;
    uintmax_t sizeBytes = 0;       // Cumulative size (all nested files/subdirs)
    uintmax_t selfSizeBytes = 0;   // Direct files size
    size_t fileCount = 0;
    size_t dirCount = 0;
    bool isDirectory = false;
    bool expanded = false;
    DeepScanNode* parent = nullptr;
    std::vector<std::shared_ptr<DeepScanNode>> children;
};

struct DeepScanFilter {
    uintmax_t minSizeBytes = 0;
    size_t maxDepth = 0;           // 0 = unlimited
    std::set<std::string> excludeExts;
    size_t topN = 20;
};

class DeepScanner {
public:
    static std::shared_ptr<DeepScanNode> ScanDirectory(
        const fs::path& rootPath,
        const DeepScanFilter& filter = {},
        std::function<void(const std::string& progressMsg)> progressCb = nullptr)
    {
        std::error_code ec;
        fs::path canonicalRoot = fs::weakly_canonical(rootPath, ec);
        if (ec || canonicalRoot.empty()) canonicalRoot = rootPath;

        auto rootNode = std::make_shared<DeepScanNode>();
        rootNode->path = canonicalRoot;
        rootNode->name = canonicalRoot.filename().string();
        if (rootNode->name.empty()) rootNode->name = canonicalRoot.string();
        rootNode->isDirectory = fs::is_directory(canonicalRoot, ec);

        if (!rootNode->isDirectory) {
            if (fs::exists(canonicalRoot, ec)) {
                rootNode->selfSizeBytes = fs::file_size(canonicalRoot, ec);
                rootNode->sizeBytes = rootNode->selfSizeBytes;
                rootNode->fileCount = 1;
            }
            return rootNode;
        }

        std::map<std::string, std::shared_ptr<DeepScanNode>> nodeMap;
        nodeMap[canonicalRoot.string()] = rootNode;

        auto options = fs::directory_options::skip_permission_denied;
        auto it = fs::recursive_directory_iterator(canonicalRoot, options, ec);
        if (ec) return rootNode;

        size_t scannedCount = 0;
        std::error_code entryEc;

        while (it != fs::recursive_directory_iterator()) {
            try {
                entryEc.clear();
                fs::path currentPath = it->path();
                int currentDepth = it.depth() + 1; // 1-indexed relative depth

                if (filter.maxDepth > 0 && static_cast<size_t>(currentDepth) > filter.maxDepth) {
                    it.disable_recursion_pending();
                    it.increment(entryEc);
                    continue;
                }

                std::string ext = currentPath.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
                if (!ext.empty() && filter.excludeExts.count(ext) > 0) {
                    if (it->is_directory(entryEc)) it.disable_recursion_pending();
                    it.increment(entryEc);
                    continue;
                }

                scannedCount++;
                if (progressCb && scannedCount % 200 == 0) {
                    progressCb("Scanning " + currentPath.filename().string() + " (" + std::to_string(scannedCount) + " items)...");
                }

                bool isDir = it->is_directory(entryEc);
                uintmax_t fSize = 0;
                if (!isDir && it->is_regular_file(entryEc)) {
                    fSize = it->file_size(entryEc);
                    if (fSize < filter.minSizeBytes) {
                        it.increment(entryEc);
                        continue;
                    }
                }

                auto node = std::make_shared<DeepScanNode>();
                node->path = currentPath;
                node->name = currentPath.filename().string();
                node->isDirectory = isDir;
                if (!isDir) {
                    node->selfSizeBytes = fSize;
                    node->sizeBytes = fSize;
                    node->fileCount = 1;
                } else {
                    node->dirCount = 1;
                }

                fs::path parentPath = currentPath.parent_path();
                std::string pKey = parentPath.string();
                auto pIt = nodeMap.find(pKey);
                if (pIt != nodeMap.end()) {
                    node->parent = pIt->second.get();
                    pIt->second->children.push_back(node);
                } else {
                    node->parent = rootNode.get();
                    rootNode->children.push_back(node);
                }

                if (isDir) {
                    nodeMap[currentPath.string()] = node;
                }

            } catch (...) {}
            it.increment(entryEc);
            if (entryEc) entryEc.clear();
        }

        CalculateCumulativeSizes(rootNode);

        return rootNode;
    }

private:
    static void CalculateCumulativeSizes(std::shared_ptr<DeepScanNode> node) {
        if (!node) return;
        
        uintmax_t childSizeSum = 0;
        size_t subFiles = node->fileCount;
        size_t subDirs = node->isDirectory ? 1 : 0;

        for (auto& child : node->children) {
            CalculateCumulativeSizes(child);
            childSizeSum += child->sizeBytes;
            subFiles += child->fileCount;
            subDirs += child->dirCount;
        }

        node->sizeBytes = node->selfSizeBytes + childSizeSum;
        node->fileCount = subFiles;
        node->dirCount = subDirs;

        std::sort(node->children.begin(), node->children.end(),
            [](const std::shared_ptr<DeepScanNode>& a, const std::shared_ptr<DeepScanNode>& b) {
                return a->sizeBytes > b->sizeBytes;
            });
    }

public:
    static void FlattenTree(std::shared_ptr<DeepScanNode> node, std::vector<std::shared_ptr<DeepScanNode>>& list) {
        if (!node) return;
        list.push_back(node);
        for (const auto& child : node->children) {
            FlattenTree(child, list);
        }
    }

    static std::vector<std::shared_ptr<DeepScanNode>> GetTopN(std::shared_ptr<DeepScanNode> root, size_t N = 20) {
        std::vector<std::shared_ptr<DeepScanNode>> allNodes;
        FlattenTree(root, allNodes);
        
        std::sort(allNodes.begin(), allNodes.end(), [](const std::shared_ptr<DeepScanNode>& a, const std::shared_ptr<DeepScanNode>& b) {
            return a->sizeBytes > b->sizeBytes;
        });

        if (allNodes.size() > N) {
            allNodes.resize(N);
        }
        return allNodes;
    }

    static std::string RenderVisualSizeBar(uintmax_t nodeSize, uintmax_t totalSize, size_t barWidth = 10) {
        if (totalSize == 0) return std::string(barWidth, ' ');
        double pct = (static_cast<double>(nodeSize) / static_cast<double>(totalSize)) * 100.0;
        size_t filled = static_cast<size_t>((pct / 100.0) * barWidth);
        if (filled > barWidth) filled = barWidth;
        if (filled == 0 && nodeSize > 0) filled = 1;

        std::string bar = "";
        for (size_t i = 0; i < filled; ++i) bar += "█";
        for (size_t i = filled; i < barWidth; ++i) bar += "░";

        std::stringstream ss;
        ss << bar << "  " << std::setw(9) << Cleaner::FormatSize(nodeSize) << "  "
           << std::fixed << std::setprecision(1) << std::setw(5) << pct << "%";
        return ss.str();
    }

    static void ExportToJson(const fs::path& rootPath, std::shared_ptr<DeepScanNode> rootNode, const std::string& jsonFilePath) {
        try {
            std::ofstream jsonFile(jsonFilePath);
            if (!jsonFile.is_open()) {
                Logger::Instance().Error("DeepScanner: Failed to open JSON report path: " + jsonFilePath);
                return;
            }

            jsonFile << "{\n";
            jsonFile << "  \"engine\": \"system-cleaner-agent v5.6.0 Deep Disk Scan Engine\",\n";
            jsonFile << "  \"root_path\": \"" << rootPath.string() << "\",\n";
            jsonFile << "  \"total_size_bytes\": " << (rootNode ? rootNode->sizeBytes : 0) << ",\n";
            jsonFile << "  \"total_size_formatted\": \"" << Cleaner::FormatSize(rootNode ? rootNode->sizeBytes : 0) << "\",\n";
            jsonFile << "  \"total_files\": " << (rootNode ? rootNode->fileCount : 0) << ",\n";
            jsonFile << "  \"total_dirs\": " << (rootNode ? rootNode->dirCount : 0) << ",\n";
            jsonFile << "  \"top_nodes\": [\n";

            auto topNodes = GetTopN(rootNode, 50);
            for (size_t i = 0; i < topNodes.size(); ++i) {
                const auto& n = topNodes[i];
                jsonFile << "    {\n";
                jsonFile << "      \"path\": \"" << n->path.string() << "\",\n";
                jsonFile << "      \"name\": \"" << n->name << "\",\n";
                jsonFile << "      \"is_directory\": " << (n->isDirectory ? "true" : "false") << ",\n";
                jsonFile << "      \"size_bytes\": " << n->sizeBytes << ",\n";
                jsonFile << "      \"size_formatted\": \"" << Cleaner::FormatSize(n->sizeBytes) << "\",\n";
                jsonFile << "      \"file_count\": " << n->fileCount << ",\n";
                jsonFile << "      \"dir_count\": " << n->dirCount << "\n";
                jsonFile << "    }" << (i + 1 < topNodes.size() ? "," : "") << "\n";
            }

            jsonFile << "  ]\n";
            jsonFile << "}\n";
            jsonFile.close();

            Logger::Instance().Info("DeepScanner: Exported JSON report to " + jsonFilePath);
        } catch (const std::exception& e) {
            Logger::Instance().Error("DeepScanner: JSON export error: " + std::string(e.what()));
        }
    }

    static void QueryDriveFreeSpace(const fs::path& drivePath, uintmax_t& outTotalBytes, uintmax_t& outFreeBytes) {
        outTotalBytes = 0;
        outFreeBytes = 0;
#ifdef _WIN32
        std::wstring wPath = drivePath.wstring();
        ULARGE_INTEGER freeBytesAvailableToCaller, totalNumberOfBytes, totalNumberOfFreeBytes;
        if (GetDiskFreeSpaceExW(wPath.c_str(), &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
            outTotalBytes = totalNumberOfBytes.QuadPart;
            outFreeBytes = freeBytesAvailableToCaller.QuadPart;
            return;
        }
#else
        struct statvfs stat;
        if (statvfs(drivePath.string().c_str(), &stat) == 0) {
            outTotalBytes = static_cast<uintmax_t>(stat.f_blocks) * stat.f_frsize;
            outFreeBytes = static_cast<uintmax_t>(stat.f_bavail) * stat.f_frsize;
            return;
        }
#endif
        std::error_code ec;
        fs::space_info si = fs::space(drivePath, ec);
        if (!ec) {
            outTotalBytes = si.capacity;
            outFreeBytes = si.available;
        }
    }
};
