//-------------------------------------------------------------------------------------
// FileManagerCore.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FILEMANAGERCORE_H
#define FILEMANAGERCORE_H

#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace FileManager {

enum class FileSource {
    Game,
    Mod,
    Dlc,
    Unknown
};

struct EffectiveFileEntry {
    std::string logicalPath;
    FileSource source = FileSource::Unknown;
};

struct EffectiveFileListResult {
    bool success = false;
    std::vector<EffectiveFileEntry> entries;
    std::string errorMessage;
};

class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual EffectiveFileListResult listEffectiveFiles(const std::string& relativeRoot,
                                                       const std::string& suffixFilter) const = 0;
};

struct FileRecord {
    std::string rowId;
    std::string displayName;
    std::string relativePath;
    FileSource source = FileSource::Unknown;
};

struct TreeRow {
    std::string rowId;
    std::string displayName;
    std::string relativePath;
    FileSource source = FileSource::Unknown;
    bool isDirectory = false;
    bool expanded = false;
    bool hasChildren = false;
    bool selected = false;
    int depth = 0;
    std::size_t childCount = 0;
};

struct StateSnapshot {
    std::vector<FileRecord> rows;
    std::vector<TreeRow> treeRows;
    std::string filterText;
    std::string selectedPath;
    std::string selectedDisplayName;
    FileRecord selectedFile;
    bool hasSelection = false;
    bool hasSelectedFile = false;
    bool selectedIsDirectory = false;
    std::size_t totalCount = 0;
    std::size_t filteredCount = 0;
    std::size_t visibleCount = 0;
    std::string lastError;
};

class FileManagerCore {
public:
    void setFileSystem(IFileSystem* fileSystem);

    bool initialize();
    bool refresh();
    void setSearchText(const std::string& text);
    bool selectNode(const std::string& relativePath);
    bool toggleDirectory(const std::string& relativePath);

    StateSnapshot buildState() const;
    const std::string& lastError() const;

private:
    std::vector<FileRecord> filteredRows() const;
    std::vector<TreeRow> buildTreeRows() const;
    bool directoryExists(const std::string& relativePath) const;
    bool fileExists(const std::string& relativePath, FileRecord* outRecord = nullptr) const;
    void setError(const std::string& message);
    void clearError();

    IFileSystem* m_fileSystem = nullptr;
    std::vector<FileRecord> m_allRows;
    std::set<std::string> m_expandedDirectories;
    std::string m_filterText;
    std::string m_selectedPath;
    std::string m_lastError;
};

std::string fileSourceKey(FileSource source);
std::string fileSourceName(FileSource source);

} // namespace FileManager

#endif // FILEMANAGERCORE_H
