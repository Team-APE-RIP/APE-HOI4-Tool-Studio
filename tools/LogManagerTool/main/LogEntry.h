//-------------------------------------------------------------------------------------
// LogEntry.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LOGENTRY_H
#define LOGENTRY_H

#include <string>
#include <vector>

namespace LogManager {

// Sort mode enumeration
enum class SortMode {
    ByTime = 0,
    ByCategory = 1
};

// Single log entry structure
struct LogEntry {
    std::string systemTime;
    std::string gameTime;
    std::string category;
    std::string message;
    std::string normalizedKey;
    bool isHighPriority = false;
    int originalIndex = -1;
};

// Log file record structure
struct LogFileRecord {
    std::string displayName;
    std::string sourcePath;
    bool isLatest = false;
    bool isLoaded = false;
    std::vector<LogEntry> entries;
};

// Compare row structure for side-by-side comparison
struct CompareRow {
    std::string normalizedKey;
    std::string category;
    bool isHighPriority = false;
    bool hasLeft = false;
    bool hasRight = false;
    LogEntry leftEntry;
    LogEntry rightEntry;
};

// Loading state snapshot for UI rendering
struct LoadingStateSnapshot {
    bool active = false;
    double progress = -1.0;
    std::string text;
};

// Statistics snapshot for UI rendering
struct StatisticsSnapshot {
    int fileCount = 0;
    int totalCount = 0;
    int filteredCount = 0;
};

// State snapshot for UI rendering
struct StateSnapshot {
    std::string currentFileName;
    std::string compareFileName;
    std::string searchText;
    std::string viewMode;
    std::string activeModelId;
    SortMode sortMode = SortMode::ByTime;
    bool isCompareMode = false;
    bool hasCurrentFile = false;
    bool hasCompareFile = false;
    bool hasFiles = false;
    std::vector<LogFileRecord> files;
    std::vector<LogEntry> entries;
    std::vector<CompareRow> compareRows;
    LoadingStateSnapshot loading;
    StatisticsSnapshot statistics;
};

} // namespace LogManager

#endif // LOGENTRY_H
