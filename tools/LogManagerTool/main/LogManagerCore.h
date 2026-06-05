//-------------------------------------------------------------------------------------
// LogManagerCore.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LOGMANAGERCORE_H
#define LOGMANAGERCORE_H

#include "LogEntry.h"
#include "LogFileSystem.h"
#include "LogParser.h"
#include "LogScanner.h"

#include <map>
#include <string>
#include <vector>

namespace LogManager {

// Main coordinator class for log management operations
class LogManagerCore {
public:
    LogManagerCore() = default;

    // Set file system implementation (must be called before initialize)
    void setFileSystem(IFileSystem* fileSystem) {
        m_fileSystem = fileSystem;
        m_scanner.setFileSystem(fileSystem);
    }

    // Initialize the core state without performing the initial scan.
    bool initialize();

    // Refresh the list of available log files
    bool refreshFiles(bool loadSelectedFile = true);

    // File selection and comparison operations
    bool selectFile(const std::string& displayName);
    bool startCompare(const std::string& displayName);
    void stopCompare();
    bool switchViewMode(const std::string& viewMode,
                        const std::string& compareDisplayName = std::string());

    // Search and sort operations
    void setSearchText(const std::string& text);
    void setSortMode(SortMode mode);

    // State accessors
    SortMode sortMode() const noexcept { return m_sortMode; }
    const std::string& searchText() const noexcept { return m_searchText; }
    const std::string& currentFileName() const noexcept { return m_currentFileName; }
    const std::string& compareFileName() const noexcept { return m_compareFileName; }
    bool isCompareMode() const noexcept { return !m_compareFileName.empty(); }
    const std::vector<LogFileRecord>& files() const noexcept { return m_files; }

    // Build complete state snapshot for UI
    StateSnapshot buildState() const;

    // Handle UI actions
    bool handleAction(const std::string& actionType, const std::map<std::string, std::string>& params);

    // Error handling
    const std::string& lastError() const noexcept { return m_lastError; }

private:
    // Helper methods
    bool ensureDefaultSelection();
    bool loadFile(const std::string& displayName);
    bool readTextFile(const std::string& relativePath, std::string* outContent) const;

    // Filtering and sorting
    std::vector<LogEntry> buildFilteredEntries() const;
    std::vector<LogEntry> sortEntries(const std::vector<LogEntry>& entries) const;
    std::vector<CompareRow> buildCompareRows() const;
    std::vector<CompareRow> buildCompareRows(const std::string& normalizedSearch) const;
    bool matchesSearch(const LogEntry& entry, const std::string& normalizedSearch) const;
    bool compareRowMatchesSearch(const CompareRow& row, const std::string& normalizedSearch) const;

    // File lookup
    LogFileRecord* findFile(const std::string& displayName);
    const LogFileRecord* findFile(const std::string& displayName) const;

    // Error management
    void setError(const std::string& message);
    void clearError();

    // Member variables
    IFileSystem* m_fileSystem = nullptr;
    LogScanner m_scanner;
    LogParser m_parser;
    std::vector<LogFileRecord> m_files;
    std::string m_currentFileName;
    std::string m_compareFileName;
    std::string m_searchText;
    SortMode m_sortMode = SortMode::ByTime;
    std::string m_lastError;
};

} // namespace LogManager

#endif // LOGMANAGERCORE_H
