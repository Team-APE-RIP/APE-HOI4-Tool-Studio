//-------------------------------------------------------------------------------------
// FileManagerCore.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "FileManagerCore.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <utility>

namespace FileManager {
namespace {

struct BuildNode {
    std::string displayName;
    std::string relativePath;
    FileSource source = FileSource::Unknown;
    bool isDirectory = true;
    std::map<std::string, std::unique_ptr<BuildNode>> children;
};

std::string normalizePath(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    while (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t separator = path.find('/', start);
        const std::size_t end = separator == std::string::npos ? path.size() : separator;
        if (end > start) {
            parts.push_back(path.substr(start, end - start));
        }
        if (separator == std::string::npos) {
            break;
        }
        start = separator + 1;
    }
    return parts;
}

std::string fileNameFromPath(const std::string& path) {
    const std::size_t separator = path.find_last_of('/');
    if (separator == std::string::npos) {
        return path;
    }
    return path.substr(separator + 1);
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool containsCaseInsensitive(const std::string& value, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    return lowerAscii(value).find(lowerAscii(needle)) != std::string::npos;
}

bool recordMatchesFilter(const FileRecord& record, const std::string& filterText) {
    if (filterText.empty()) {
        return true;
    }
    return containsCaseInsensitive(record.displayName, filterText)
        || containsCaseInsensitive(record.relativePath, filterText)
        || containsCaseInsensitive(fileSourceName(record.source), filterText);
}

std::string childSortKey(bool isDirectory, const std::string& displayName) {
    return std::string(isDirectory ? "0:" : "1:") + lowerAscii(displayName) + ":" + displayName;
}

void addRecordToTree(BuildNode* root, const FileRecord& record) {
    if (!root) {
        return;
    }

    const std::vector<std::string> parts = splitPath(record.relativePath);
    if (parts.empty()) {
        return;
    }

    BuildNode* parent = root;
    std::string currentPath;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        const bool isFile = index + 1 == parts.size();
        if (!currentPath.empty()) {
            currentPath += "/";
        }
        currentPath += parts[index];

        const std::string key = childSortKey(!isFile, parts[index]);
        std::unique_ptr<BuildNode>& child = parent->children[key];
        if (!child) {
            child = std::make_unique<BuildNode>();
            child->displayName = parts[index];
            child->relativePath = currentPath;
            child->isDirectory = !isFile;
        }

        child->isDirectory = !isFile;
        child->relativePath = currentPath;
        child->displayName = parts[index];
        if (isFile) {
            child->source = record.source;
        }
        parent = child.get();
    }
}

bool nodeMatchesFilter(const BuildNode& node, const std::string& filterText) {
    if (filterText.empty()) {
        return true;
    }

    if (containsCaseInsensitive(node.displayName, filterText)
        || containsCaseInsensitive(node.relativePath, filterText)) {
        return true;
    }

    if (!node.isDirectory) {
        return containsCaseInsensitive(fileSourceName(node.source), filterText);
    }

    return false;
}

TreeRow makeTreeRow(const BuildNode& node,
                    int depth,
                    const std::set<std::string>& expandedDirectories,
                    const std::string& selectedPath,
                    bool forceExpanded) {
    TreeRow row;
    row.rowId = node.relativePath;
    row.displayName = node.displayName;
    row.relativePath = node.relativePath;
    row.source = node.source;
    row.isDirectory = node.isDirectory;
    row.expanded = node.isDirectory
        && (forceExpanded || expandedDirectories.count(node.relativePath) > 0);
    row.hasChildren = !node.children.empty();
    row.selected = selectedPath == node.relativePath;
    row.depth = depth;
    row.childCount = node.children.size();
    return row;
}

bool appendFilteredRows(const BuildNode& node,
                        int depth,
                        const std::set<std::string>& expandedDirectories,
                        const std::string& selectedPath,
                        const std::string& filterText,
                        std::vector<TreeRow>* rows) {
    std::vector<TreeRow> childRows;
    for (const auto& childPair : node.children) {
        appendFilteredRows(*childPair.second, depth + 1, expandedDirectories, selectedPath, filterText, &childRows);
    }

    const bool includeNode = nodeMatchesFilter(node, filterText) || !childRows.empty();
    if (!includeNode) {
        return false;
    }

    rows->push_back(makeTreeRow(node, depth, expandedDirectories, selectedPath, true));
    rows->insert(rows->end(), childRows.begin(), childRows.end());
    return true;
}

void appendVisibleRows(const BuildNode& node,
                       int depth,
                       const std::set<std::string>& expandedDirectories,
                       const std::string& selectedPath,
                       std::vector<TreeRow>* rows) {
    const bool expanded = node.isDirectory && expandedDirectories.count(node.relativePath) > 0;
    rows->push_back(makeTreeRow(node, depth, expandedDirectories, selectedPath, false));
    if (!expanded) {
        return;
    }

    for (const auto& childPair : node.children) {
        appendVisibleRows(*childPair.second, depth + 1, expandedDirectories, selectedPath, rows);
    }
}

} // namespace

std::string fileSourceKey(FileSource source) {
    switch (source) {
    case FileSource::Game:
        return "SourceGame";
    case FileSource::Mod:
        return "SourceMod";
    case FileSource::Dlc:
        return "SourceDLC";
    default:
        return "SourceUnknown";
    }
}

std::string fileSourceName(FileSource source) {
    switch (source) {
    case FileSource::Game:
        return "Game";
    case FileSource::Mod:
        return "Mod";
    case FileSource::Dlc:
        return "DLC";
    default:
        return "Unknown";
    }
}

void FileManagerCore::setFileSystem(IFileSystem* fileSystem) {
    m_fileSystem = fileSystem;
}

bool FileManagerCore::initialize() {
    m_allRows.clear();
    m_expandedDirectories.clear();
    m_filterText.clear();
    m_selectedPath.clear();
    clearError();
    return true;
}

bool FileManagerCore::refresh() {
    if (!m_fileSystem) {
        setError("File system bridge is not available.");
        return false;
    }

    EffectiveFileListResult result = m_fileSystem->listEffectiveFiles({}, {});
    if (!result.success) {
        setError(result.errorMessage.empty() ? "Failed to list effective files." : result.errorMessage);
        return false;
    }

    std::vector<FileRecord> nextRows;
    nextRows.reserve(result.entries.size());
    for (const EffectiveFileEntry& entry : result.entries) {
        const std::string logicalPath = normalizePath(entry.logicalPath);
        if (logicalPath.empty()) {
            continue;
        }

        FileRecord record;
        record.rowId = logicalPath;
        record.displayName = fileNameFromPath(logicalPath);
        record.relativePath = logicalPath;
        record.source = entry.source;
        nextRows.push_back(std::move(record));
    }

    std::sort(nextRows.begin(), nextRows.end(), [](const FileRecord& left, const FileRecord& right) {
        return lowerAscii(left.relativePath) < lowerAscii(right.relativePath);
    });

    m_allRows = std::move(nextRows);
    if (!m_selectedPath.empty()
        && !fileExists(m_selectedPath)
        && !directoryExists(m_selectedPath)) {
        m_selectedPath.clear();
    }

    for (auto iterator = m_expandedDirectories.begin(); iterator != m_expandedDirectories.end();) {
        if (directoryExists(*iterator)) {
            ++iterator;
        } else {
            iterator = m_expandedDirectories.erase(iterator);
        }
    }

    clearError();
    return true;
}

void FileManagerCore::setSearchText(const std::string& text) {
    m_filterText = text;
}

bool FileManagerCore::selectNode(const std::string& relativePath) {
    const std::string normalizedPath = normalizePath(relativePath);
    if (normalizedPath.empty()) {
        m_selectedPath.clear();
        clearError();
        return true;
    }

    if (!fileExists(normalizedPath) && !directoryExists(normalizedPath)) {
        setError("Selected path is not present in the effective file tree.");
        return false;
    }

    m_selectedPath = normalizedPath;
    clearError();
    return true;
}

bool FileManagerCore::toggleDirectory(const std::string& relativePath) {
    const std::string normalizedPath = normalizePath(relativePath);
    if (normalizedPath.empty() || !directoryExists(normalizedPath)) {
        setError("Selected path is not a directory in the effective file tree.");
        return false;
    }

    const auto expandedIt = m_expandedDirectories.find(normalizedPath);
    if (expandedIt == m_expandedDirectories.end()) {
        m_expandedDirectories.insert(normalizedPath);
    } else {
        m_expandedDirectories.erase(expandedIt);
    }

    clearError();
    return true;
}

StateSnapshot FileManagerCore::buildState() const {
    StateSnapshot state;
    state.rows = filteredRows();
    state.treeRows = buildTreeRows();
    state.filterText = m_filterText;
    state.selectedPath = m_selectedPath;
    state.totalCount = m_allRows.size();
    state.filteredCount = state.rows.size();
    state.visibleCount = state.treeRows.size();
    state.lastError = m_lastError;

    if (!m_selectedPath.empty()) {
        FileRecord selectedRecord;
        if (fileExists(m_selectedPath, &selectedRecord)) {
            state.selectedFile = selectedRecord;
            state.selectedDisplayName = selectedRecord.displayName;
            state.hasSelection = true;
            state.hasSelectedFile = true;
        } else if (directoryExists(m_selectedPath)) {
            state.selectedDisplayName = fileNameFromPath(m_selectedPath);
            state.hasSelection = true;
            state.selectedIsDirectory = true;
        }
    }

    return state;
}

const std::string& FileManagerCore::lastError() const {
    return m_lastError;
}

std::vector<FileRecord> FileManagerCore::filteredRows() const {
    std::vector<FileRecord> rows;
    rows.reserve(m_allRows.size());
    for (const FileRecord& record : m_allRows) {
        if (recordMatchesFilter(record, m_filterText)) {
            rows.push_back(record);
        }
    }
    return rows;
}

std::vector<TreeRow> FileManagerCore::buildTreeRows() const {
    BuildNode root;
    root.isDirectory = true;
    for (const FileRecord& record : m_allRows) {
        addRecordToTree(&root, record);
    }

    std::vector<TreeRow> rows;
    if (!m_filterText.empty()) {
        for (const auto& childPair : root.children) {
            appendFilteredRows(*childPair.second, 0, m_expandedDirectories, m_selectedPath, m_filterText, &rows);
        }
        return rows;
    }

    for (const auto& childPair : root.children) {
        appendVisibleRows(*childPair.second, 0, m_expandedDirectories, m_selectedPath, &rows);
    }
    return rows;
}

bool FileManagerCore::directoryExists(const std::string& relativePath) const {
    const std::string normalizedPath = normalizePath(relativePath);
    if (normalizedPath.empty()) {
        return false;
    }

    const std::string childPrefix = normalizedPath + "/";
    return std::any_of(m_allRows.begin(), m_allRows.end(), [&childPrefix](const FileRecord& record) {
        return record.relativePath.rfind(childPrefix, 0) == 0;
    });
}

bool FileManagerCore::fileExists(const std::string& relativePath, FileRecord* outRecord) const {
    const std::string normalizedPath = normalizePath(relativePath);
    const auto recordIt = std::find_if(m_allRows.begin(), m_allRows.end(), [&normalizedPath](const FileRecord& record) {
        return record.relativePath == normalizedPath;
    });
    if (recordIt == m_allRows.end()) {
        return false;
    }

    if (outRecord) {
        *outRecord = *recordIt;
    }
    return true;
}

void FileManagerCore::setError(const std::string& message) {
    m_lastError = message;
}

void FileManagerCore::clearError() {
    m_lastError.clear();
}

} // namespace FileManager
