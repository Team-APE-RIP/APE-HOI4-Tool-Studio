//-------------------------------------------------------------------------------------
// LogParser.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LOGPARSER_H
#define LOGPARSER_H

#include "LogEntry.h"
#include <string>
#include <vector>

namespace LogManager {

// Log file parser class
class LogParser {
public:
    LogParser() = default;

    // Parse log file content into structured entries
    std::vector<LogEntry> parseLogContent(const std::string& content) const;

    // Generate normalized key for log entry (for comparison and deduplication)
    std::string normalizeLogEntryKey(const std::string& category, const std::string& message) const;
};

} // namespace LogManager

#endif // LOGPARSER_H
