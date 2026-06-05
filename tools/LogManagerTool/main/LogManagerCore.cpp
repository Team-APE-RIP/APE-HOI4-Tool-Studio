//-------------------------------------------------------------------------------------
// LogManagerCore.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "LogManagerCore.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <map>
#include <utility>

namespace LogManager {

namespace {

// Convert string to lowercase.
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

// Trim whitespace from string.
std::string trim(const std::string& value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    if (begin == value.end()) {
        return std::string();
    }

    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();

    return std::string(begin, end);
}

// Replace all occurrences in string.
std::string replaceAll(std::string value, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return value;
    }

    std::size_t position = 0;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.length(), to);
        position += to.length();
    }
    return value;
}

// Normalize newlines.
std::string normalizeNewlines(std::string value) {
    value = replaceAll(std::move(value), "\r\n", "\n");
    value = replaceAll(std::move(value), "\r", "\n");
    return value;
}

// Normalize search text.
std::string normalizeSearchText(const std::string& value) {
    return toLower(trim(normalizeNewlines(value)));
}

// Case-insensitive string comparison.
bool caseInsensitiveLess(const std::string& left, const std::string& right) {
    const std::string loweredLeft = toLower(left);
    const std::string loweredRight = toLower(right);
    if (loweredLeft != loweredRight) {
        return loweredLeft < loweredRight;
    }
    return left < right;
}

// Extract first non-empty parameter from map.
std::string firstNonEmptyParam(const std::map<std::string, std::string>& params,
                               std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const auto iterator = params.find(key);
        if (iterator != params.end() && !iterator->second.empty()) {
            return iterator->second;
        }
    }
    return std::string();
}

// Parse sort mode value from string.
bool parseSortModeValue(const std::string& value, SortMode* outMode) {
    if (!outMode) {
        return false;
    }

    const std::string normalized = toLower(trim(value));
    if (normalized.empty()
        || normalized == "0"
        || normalized == "time"
        || normalized == "by_time"
        || normalized == "sort_time"
        || normalized == "chronological") {
        *outMode = SortMode::ByTime;
        return true;
    }

    if (normalized == "1"
        || normalized == "category"
        || normalized == "by_category"
        || normalized == "sort_category") {
        *outMode = SortMode::ByCategory;
        return true;
    }

    return false;
}

// Normalize requested view mode.
std::string normalizeViewModeValue(const std::string& value) {
    const std::string normalized = toLower(trim(value));
    if (normalized.empty()) {
        return std::string();
    }

    if (normalized == "normal"
        || normalized == "default"
        || normalized == "entries"
        || normalized == "single"
        || normalized == "list") {
        return "normal";
    }

    if (normalized == "compare"
        || normalized == "comparison"
        || normalized == "diff"
        || normalized == "dual") {
        return "compare";
    }

    return std::string();
}

} // namespace

bool LogManagerCore::initialize() {
    if (!m_fileSystem) {
        setError("File system not configured.");
        return false;
    }

    m_scanner.setFileSystem(m_fileSystem);
    m_files.clear();
    m_currentFileName.clear();
    m_compareFileName.clear();
    m_searchText.clear();
    m_sortMode = SortMode::ByTime;
    clearError();
    return true;
}

bool LogManagerCore::refreshFiles(bool loadSelectedFile) {
    std::map<std::string, LogFileRecord> cachedLoadedFiles;
    for (const LogFileRecord& record : m_files) {
        if (!record.isLoaded || record.isLatest) {
            continue;
        }
        cachedLoadedFiles.insert_or_assign(record.displayName, record);
    }

    std::vector<LogFileRecord> discoveredFiles = m_scanner.scanLogFiles();

    for (LogFileRecord& record : discoveredFiles) {
        if (record.isLatest) {
            continue;
        }

        const auto cachedIterator = cachedLoadedFiles.find(record.displayName);
        if (cachedIterator == cachedLoadedFiles.end()) {
            continue;
        }
        if (cachedIterator->second.sourcePath != record.sourcePath) {
            continue;
        }

        record.isLoaded = true;
        record.entries = cachedIterator->second.entries;
    }

    m_files = std::move(discoveredFiles);

    if (!m_currentFileName.empty() && !findFile(m_currentFileName)) {
        m_currentFileName.clear();
    }
    if (!m_compareFileName.empty() && !findFile(m_compareFileName)) {
        m_compareFileName.clear();
    }

    if (!ensureDefaultSelection()) {
        clearError();
        return true;
    }

    if (loadSelectedFile) {
        if (!m_currentFileName.empty() && !loadFile(m_currentFileName)) {
            return false;
        }
        if (!m_compareFileName.empty() && !loadFile(m_compareFileName)) {
            m_compareFileName.clear();
        }
    }

    clearError();
    return true;
}

bool LogManagerCore::selectFile(const std::string& displayName) {
    if (displayName.empty()) {
        setError("Current file name is empty.");
        return false;
    }
    if (!findFile(displayName)) {
        setError("Requested log file was not found: " + displayName);
        return false;
    }
    if (!loadFile(displayName)) {
        return false;
    }

    m_currentFileName = displayName;
    m_compareFileName.clear();
    clearError();
    return true;
}

bool LogManagerCore::startCompare(const std::string& displayName) {
    if (m_currentFileName.empty()) {
        setError("Cannot enter compare mode without a current file.");
        return false;
    }
    if (displayName.empty()) {
        setError("Compare file name is empty.");
        return false;
    }
    if (displayName == m_currentFileName) {
        setError("Compare file cannot be the same as the current file.");
        return false;
    }
    if (!findFile(displayName)) {
        setError("Requested compare file was not found: " + displayName);
        return false;
    }
    if (!loadFile(m_currentFileName) || !loadFile(displayName)) {
        return false;
    }

    m_compareFileName = displayName;
    clearError();
    return true;
}

void LogManagerCore::stopCompare() {
    m_compareFileName.clear();
    clearError();
}

bool LogManagerCore::switchViewMode(const std::string& viewMode, const std::string& compareDisplayName) {
    const std::string normalizedViewMode = normalizeViewModeValue(viewMode);
    if (normalizedViewMode.empty()) {
        setError("Unsupported view mode: " + viewMode);
        return false;
    }

    if (normalizedViewMode == "normal") {
        stopCompare();
        return true;
    }

    if (normalizedViewMode == "compare") {
        if (!compareDisplayName.empty()) {
            return startCompare(compareDisplayName);
        }
        if (isCompareMode()) {
            clearError();
            return true;
        }
        setError("Compare view mode requires a compare file.");
        return false;
    }

    setError("Unsupported view mode: " + viewMode);
    return false;
}

void LogManagerCore::setSearchText(const std::string& text) {
    m_searchText = text;
    clearError();
}

void LogManagerCore::setSortMode(SortMode mode) {
    m_sortMode = mode;
    clearError();
}

StateSnapshot LogManagerCore::buildState() const {
    StateSnapshot snapshot;
    snapshot.currentFileName = m_currentFileName;
    snapshot.compareFileName = m_compareFileName;
    snapshot.searchText = m_searchText;
    snapshot.sortMode = m_sortMode;
    snapshot.isCompareMode = isCompareMode();
    snapshot.viewMode = snapshot.isCompareMode ? "compare" : "normal";
    snapshot.activeModelId = snapshot.isCompareMode ? "compare_entries" : "log_entries";
    snapshot.files = m_files;
    snapshot.hasFiles = !m_files.empty();

    const LogFileRecord* currentFile = findFile(m_currentFileName);
    const LogFileRecord* compareFile = findFile(m_compareFileName);
    snapshot.hasCurrentFile = (currentFile != nullptr && currentFile->isLoaded);
    snapshot.hasCompareFile = (compareFile != nullptr && compareFile->isLoaded);

    // OPTIMIZATION: Only build entries and compare rows if files are actually loaded.
    // This prevents unnecessary processing when displaying the file list before content is loaded.
    if (snapshot.hasCurrentFile || snapshot.hasCompareFile) {
        snapshot.entries = sortEntries(buildFilteredEntries());
        snapshot.compareRows = buildCompareRows();
    }

    snapshot.statistics.fileCount = static_cast<int>(m_files.size());
    if (snapshot.isCompareMode) {
        snapshot.statistics.totalCount = snapshot.hasCompareFile
            ? static_cast<int>(buildCompareRows(std::string()).size())
            : 0;
        snapshot.statistics.filteredCount = static_cast<int>(snapshot.compareRows.size());
    } else {
        snapshot.statistics.totalCount = (currentFile && currentFile->isLoaded)
            ? static_cast<int>(currentFile->entries.size())
            : 0;
        snapshot.statistics.filteredCount = static_cast<int>(snapshot.entries.size());
    }

    return snapshot;
}

bool LogManagerCore::handleAction(const std::string& actionType,
                                  const std::map<std::string, std::string>& params) {
    const std::string normalizedAction = toLower(trim(actionType));
    const std::string normalizedTargetId = toLower(trim(firstNonEmptyParam(
        params,
        {"targetId", "target", "function", "action", "command", "id"}
    )));

    if (normalizedAction == "initialize"
        || normalizedAction == "refresh"
        || normalizedAction == "refresh_files"
        || normalizedAction == "refresh_logs"
        || normalizedAction == "reload") {
        return refreshFiles();
    }

    if (normalizedAction == "button_click"
        || normalizedAction == "function_click"
        || normalizedAction == "trigger_action"
        || normalizedAction == "context_menu_action"
        || normalizedAction == "menu_action") {
        if (normalizedTargetId == "error_log::sort_time"
            || normalizedTargetId == "manage::sort_time"
            || normalizedTargetId == "sort_time") {
            setSortMode(SortMode::ByTime);
            return true;
        }
        if (normalizedTargetId == "error_log::sort_category"
            || normalizedTargetId == "manage::sort_category"
            || normalizedTargetId == "sort_category") {
            setSortMode(SortMode::ByCategory);
            return true;
        }
        if (normalizedTargetId == "error_log::refresh"
            || normalizedTargetId == "manage::refresh"
            || normalizedTargetId == "refresh"
            || normalizedTargetId == "refresh_logs") {
            return refreshFiles();
        }
        if (normalizedTargetId == "error_log::stop_compare"
            || normalizedTargetId == "manage::stop_compare"
            || normalizedTargetId == "stop_compare"
            || normalizedTargetId == "clear_compare") {
            stopCompare();
            return true;
        }
        if (normalizedTargetId == "error_log::view_normal"
            || normalizedTargetId == "manage::view_normal"
            || normalizedTargetId == "view_normal") {
            return switchViewMode("normal");
        }
        if (normalizedTargetId == "error_log::view_compare"
            || normalizedTargetId == "manage::view_compare"
            || normalizedTargetId == "view_compare"
            || normalizedTargetId == "compare_mode") {
            const std::string compareTarget = firstNonEmptyParam(
                params,
                {"compareFile", "compareTarget", "targetFile", "displayName", "file", "rowId"}
            );
            return switchViewMode("compare", compareTarget);
        }
        if (normalizedTargetId == "error_log::toggle_compare_mode"
            || normalizedTargetId == "manage::toggle_compare_mode"
            || normalizedTargetId == "toggle_compare_mode") {
            if (isCompareMode()) {
                return switchViewMode("normal");
            }
            const std::string compareTarget = firstNonEmptyParam(
                params,
                {"compareFile", "compareTarget", "targetFile", "displayName", "file", "rowId"}
            );
            return switchViewMode("compare", compareTarget);
        }
    }

    if (normalizedAction == "page_select" || normalizedAction == "sidebar_button_click") {
        clearError();
        return true;
    }

    if (normalizedAction == "text_changed"
        || normalizedAction == "search"
        || normalizedAction == "search_changed"
        || normalizedAction == "on_search_changed"
        || normalizedAction == "set_search_text") {
        setSearchText(firstNonEmptyParam(params, {"text", "value", "searchText", "search", "query"}));
        return true;
    }

    if (normalizedAction == "sort_time") {
        setSortMode(SortMode::ByTime);
        return true;
    }

    if (normalizedAction == "sort_category") {
        setSortMode(SortMode::ByCategory);
        return true;
    }

    if (normalizedAction == "set_sort_mode" || normalizedAction == "sort" || normalizedAction == "sort_toggle") {
        SortMode mode = SortMode::ByTime;
        if (!parseSortModeValue(firstNonEmptyParam(params, {"sortMode", "mode", "value", "sort"}), &mode)) {
            setError("Unsupported sort mode.");
            return false;
        }
        setSortMode(mode);
        return true;
    }

    if (normalizedAction == "list_select"
        || normalizedAction == "select_file"
        || normalizedAction == "on_file_selected"
        || normalizedAction == "file_selected") {
        const std::string fileName = firstNonEmptyParam(
            params,
            {"rowId", "displayName", "file", "selectedFile", "fileName", "selected", "value"}
        );
        return selectFile(fileName);
    }

    if (normalizedAction == "compare"
        || normalizedAction == "start_compare"
        || normalizedAction == "compare_selected"
        || normalizedAction == "compare_file") {
        const std::string fileName = firstNonEmptyParam(
            params,
            {"compareFile", "compareTarget", "targetFile", "rowId", "displayName", "file", "value"}
        );
        return startCompare(fileName);
    }

    if (normalizedAction == "stop_compare" || normalizedAction == "clear_compare") {
        stopCompare();
        return true;
    }

    if (normalizedAction == "switch_view_mode"
        || normalizedAction == "set_view_mode"
        || normalizedAction == "toggle_view_mode"
        || normalizedAction == "toggle_compare_mode") {
        std::string requestedViewMode = firstNonEmptyParam(params, {"viewMode", "mode", "value"});
        if (requestedViewMode.empty() && normalizedAction == "toggle_compare_mode") {
            requestedViewMode = isCompareMode() ? "normal" : "compare";
        }
        const std::string compareTarget = firstNonEmptyParam(
            params,
            {"compareFile", "compareTarget", "targetFile", "displayName", "file", "rowId"}
        );
        return switchViewMode(requestedViewMode, compareTarget);
    }

    setError("Unsupported action: " + actionType);
    return false;
}

bool LogManagerCore::ensureDefaultSelection() {
    if (m_files.empty()) {
        m_currentFileName.clear();
        m_compareFileName.clear();
        return false;
    }

    if (m_currentFileName.empty()) {
        m_currentFileName = m_files.front().displayName;
    }

    return true;
}

bool LogManagerCore::loadFile(const std::string& displayName) {
    LogFileRecord* record = findFile(displayName);
    if (!record) {
        setError("Requested log file was not found: " + displayName);
        return false;
    }
    if (record->isLoaded && !record->isLatest) {
        clearError();
        return true;
    }

    std::string content;
    if (!readTextFile(record->sourcePath, &content)) {
        setError("Failed to read log file: " + record->sourcePath);
        return false;
    }

    record->entries = m_parser.parseLogContent(content);
    record->isLoaded = true;
    clearError();
    return true;
}

bool LogManagerCore::readTextFile(const std::string& relativePath, std::string* outContent) const {
    if (!outContent) {
        return false;
    }

    if (!m_fileSystem) {
        outContent->clear();
        return false;
    }

    const TextReadResult result = m_fileSystem->readTextFile(relativePath);
    if (!result.success) {
        outContent->clear();
        return false;
    }

    *outContent = result.content;
    return true;
}

std::vector<LogEntry> LogManagerCore::buildFilteredEntries() const {
    const LogFileRecord* currentFile = findFile(m_currentFileName);
    if (!currentFile || !currentFile->isLoaded) {
        return {};
    }

    const std::string normalizedSearch = normalizeSearchText(m_searchText);
    if (normalizedSearch.empty()) {
        return currentFile->entries;
    }

    std::vector<LogEntry> filteredEntries;
    filteredEntries.reserve(currentFile->entries.size());
    for (const LogEntry& entry : currentFile->entries) {
        if (matchesSearch(entry, normalizedSearch)) {
            filteredEntries.push_back(entry);
        }
    }
    return filteredEntries;
}

std::vector<LogEntry> LogManagerCore::sortEntries(const std::vector<LogEntry>& entries) const {
    std::vector<LogEntry> sortedEntries = entries;
    std::sort(sortedEntries.begin(), sortedEntries.end(), [this](const LogEntry& left, const LogEntry& right) {
        if (m_sortMode == SortMode::ByTime) {
            return left.originalIndex < right.originalIndex;
        }

        if (left.isHighPriority != right.isHighPriority) {
            return left.isHighPriority > right.isHighPriority;
        }
        if (caseInsensitiveLess(left.category, right.category)) {
            return true;
        }
        if (caseInsensitiveLess(right.category, left.category)) {
            return false;
        }
        return left.originalIndex < right.originalIndex;
    });
    return sortedEntries;
}

std::vector<CompareRow> LogManagerCore::buildCompareRows() const {
    return buildCompareRows(normalizeSearchText(m_searchText));
}

std::vector<CompareRow> LogManagerCore::buildCompareRows(const std::string& normalizedSearch) const {
    const LogFileRecord* currentFile = findFile(m_currentFileName);
    const LogFileRecord* compareFile = findFile(m_compareFileName);
    if (!currentFile || !compareFile || !currentFile->isLoaded || !compareFile->isLoaded) {
        return {};
    }

    std::map<std::string, CompareRow> compareRowMap;
    std::vector<std::string> insertionOrder;

    auto appendEntry = [&compareRowMap, &insertionOrder](const LogEntry& entry, bool isLeft) {
        auto iterator = compareRowMap.find(entry.normalizedKey);
        if (iterator == compareRowMap.end()) {
            CompareRow row;
            row.normalizedKey = entry.normalizedKey;
            row.category = entry.category;
            row.isHighPriority = entry.isHighPriority;
            compareRowMap.insert_or_assign(entry.normalizedKey, row);
            insertionOrder.push_back(entry.normalizedKey);
            iterator = compareRowMap.find(entry.normalizedKey);
        }

        CompareRow& row = iterator->second;
        if (row.category.empty()) {
            row.category = entry.category;
        }
        row.isHighPriority = row.isHighPriority || entry.isHighPriority;

        if (isLeft && !row.hasLeft) {
            row.hasLeft = true;
            row.leftEntry = entry;
        }
        if (!isLeft && !row.hasRight) {
            row.hasRight = true;
            row.rightEntry = entry;
        }
    };

    for (const LogEntry& entry : currentFile->entries) {
        appendEntry(entry, true);
    }
    for (const LogEntry& entry : compareFile->entries) {
        appendEntry(entry, false);
    }

    std::vector<CompareRow> rows;
    rows.reserve(compareRowMap.size());
    for (const std::string& key : insertionOrder) {
        const CompareRow& row = compareRowMap.at(key);
        if (normalizedSearch.empty() || compareRowMatchesSearch(row, normalizedSearch)) {
            rows.push_back(row);
        }
    }

    std::sort(rows.begin(), rows.end(), [this](const CompareRow& left, const CompareRow& right) {
        if (m_sortMode == SortMode::ByCategory) {
            if (left.isHighPriority != right.isHighPriority) {
                return left.isHighPriority > right.isHighPriority;
            }
            if (caseInsensitiveLess(left.category, right.category)) {
                return true;
            }
            if (caseInsensitiveLess(right.category, left.category)) {
                return false;
            }
        }

        const int leftIndex = left.hasLeft ? left.leftEntry.originalIndex : left.rightEntry.originalIndex;
        const int rightIndex = right.hasLeft ? right.leftEntry.originalIndex : right.rightEntry.originalIndex;
        return leftIndex < rightIndex;
    });

    return rows;
}

bool LogManagerCore::matchesSearch(const LogEntry& entry, const std::string& normalizedSearch) const {
    if (normalizedSearch.empty()) {
        return true;
    }

    const std::string haystack = toLower(
        entry.systemTime + "\n"
        + entry.gameTime + "\n"
        + entry.category + "\n"
        + normalizeNewlines(entry.message)
    );
    return haystack.find(normalizedSearch) != std::string::npos;
}

bool LogManagerCore::compareRowMatchesSearch(const CompareRow& row, const std::string& normalizedSearch) const {
    if (normalizedSearch.empty()) {
        return true;
    }

    std::string haystack = row.category + "\n" + row.normalizedKey;
    if (row.hasLeft) {
        haystack += "\n" + row.leftEntry.systemTime;
        haystack += "\n" + row.leftEntry.gameTime;
        haystack += "\n" + normalizeNewlines(row.leftEntry.message);
    }
    if (row.hasRight) {
        haystack += "\n" + row.rightEntry.systemTime;
        haystack += "\n" + row.rightEntry.gameTime;
        haystack += "\n" + normalizeNewlines(row.rightEntry.message);
    }

    return toLower(haystack).find(normalizedSearch) != std::string::npos;
}

LogFileRecord* LogManagerCore::findFile(const std::string& displayName) {
    for (LogFileRecord& file : m_files) {
        if (file.displayName == displayName) {
            return &file;
        }
    }
    return nullptr;
}

const LogFileRecord* LogManagerCore::findFile(const std::string& displayName) const {
    for (const LogFileRecord& file : m_files) {
        if (file.displayName == displayName) {
            return &file;
        }
    }
    return nullptr;
}

void LogManagerCore::setError(const std::string& message) {
    m_lastError = message;
}

void LogManagerCore::clearError() {
    m_lastError.clear();
}

} // namespace LogManager
