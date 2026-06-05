//-------------------------------------------------------------------------------------
// LogScanner.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "LogScanner.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace LogManager {

namespace {
constexpr const char* kLatestLogInternalName = "__LATEST__";

// Convert string to lowercase
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

// Case-insensitive string comparison
bool caseInsensitiveLess(const std::string& left, const std::string& right) {
    const std::string loweredLeft = toLower(left);
    const std::string loweredRight = toLower(right);
    if (loweredLeft != loweredRight) {
        return loweredLeft < loweredRight;
    }
    return left < right;
}

// Normalize path separators for portable matching.
std::string normalizePathSeparators(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

// Normalize crash log source path relative to the document root.
std::string normalizeCrashSourcePath(const std::string& relativePath) {
    std::string normalizedPath = normalizePathSeparators(relativePath);
    const std::string prefix = "crashes/";
    if (normalizedPath.compare(0, prefix.size(), prefix) == 0) {
        return normalizedPath;
    }
    return prefix + normalizedPath;
}

// Extract crash directory display name from a recursive crash log path.
bool extractCrashDisplayName(const std::string& relativePath, std::string* outDisplayName) {
    if (!outDisplayName) {
        return false;
    }

    std::string normalizedPath = normalizePathSeparators(relativePath);
    const std::string prefix = "crashes/";
    const std::string suffix = "/logs/error.log";
    if (normalizedPath.compare(0, prefix.size(), prefix) == 0) {
        normalizedPath = normalizedPath.substr(prefix.size());
    }
    if (normalizedPath.size() <= suffix.size()) {
        return false;
    }
    if (normalizedPath.compare(normalizedPath.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }

    const std::string displayName = normalizedPath.substr(0, normalizedPath.size() - suffix.size());
    if (displayName.empty() || displayName.find('/') != std::string::npos) {
        return false;
    }

    *outDisplayName = displayName;
    return true;
}

} // namespace

std::vector<LogFileRecord> LogScanner::scanLogFiles() const {
    std::vector<LogFileRecord> files;
    std::vector<LogFileRecord> historyFiles;

    if (!m_fileSystem) {
        return files;
    }

    // Scan for latest log file in logs directory
    const DirectoryListResult logsResult = m_fileSystem->listDirectory("logs", false);

    if (logsResult.success) {
        for (const DirectoryEntry& entry : logsResult.entries) {
            if (entry.isDirectory) {
                continue;
            }
            if (toLower(entry.name) != "error.log") {
                continue;
            }

            LogFileRecord latestRecord;
            latestRecord.displayName = kLatestLogInternalName;
            latestRecord.sourcePath = entry.relativePath;
            latestRecord.isLatest = true;
            files.push_back(std::move(latestRecord));
            break;
        }
    }

    // Scan historical crash logs by checking only top-level crash folders.
    // This avoids a full recursive walk over every dump file inside the crashes directory.
    const DirectoryListResult crashesResult = m_fileSystem->listDirectory("crashes", false);

    if (crashesResult.success) {
        std::map<std::string, std::string> discoveredCrashLogs;
        for (const DirectoryEntry& entry : crashesResult.entries) {
            if (!entry.isDirectory) {
                continue;
            }

            const std::string crashLogsPath = normalizePathSeparators(entry.relativePath) + "/logs";
            const DirectoryListResult crashLogsResult = m_fileSystem->listDirectory(crashLogsPath, false);
            if (!crashLogsResult.success) {
                continue;
            }

            for (const DirectoryEntry& logEntry : crashLogsResult.entries) {
                if (logEntry.isDirectory) {
                    continue;
                }
                if (toLower(logEntry.name) != "error.log") {
                    continue;
                }

                const std::string displayName = entry.name;
                if (displayName.empty()) {
                    continue;
                }

                discoveredCrashLogs.insert_or_assign(displayName, normalizeCrashSourcePath(logEntry.relativePath));
                break;
            }
        }

        for (const auto& pair : discoveredCrashLogs) {
            LogFileRecord historyRecord;
            historyRecord.displayName = pair.first;
            historyRecord.sourcePath = pair.second;
            historyRecord.isLatest = false;
            historyFiles.push_back(std::move(historyRecord));
        }
    }

    // Sort historical files by name
    std::sort(historyFiles.begin(), historyFiles.end(), [](const LogFileRecord& left, const LogFileRecord& right) {
        return caseInsensitiveLess(left.displayName, right.displayName);
    });

    files.insert(files.end(), historyFiles.begin(), historyFiles.end());
    return files;
}

} // namespace LogManager
