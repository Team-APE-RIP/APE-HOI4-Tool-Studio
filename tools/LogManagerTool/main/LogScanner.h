//-------------------------------------------------------------------------------------
// LogScanner.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LOGSCANNER_H
#define LOGSCANNER_H

#include "LogEntry.h"
#include "LogFileSystem.h"
#include <vector>

namespace LogManager {

// Log file scanner class
class LogScanner {
public:
    LogScanner() = default;

    // Set file system implementation (must be called before scanning)
    void setFileSystem(IFileSystem* fileSystem) { m_fileSystem = fileSystem; }

    // Scan for available log files in the game directory
    std::vector<LogFileRecord> scanLogFiles() const;

private:
    IFileSystem* m_fileSystem = nullptr;
};

} // namespace LogManager

#endif // LOGSCANNER_H
