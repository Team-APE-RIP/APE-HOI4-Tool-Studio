//-------------------------------------------------------------------------------------
// LogFileSystem.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LOGFILESYSTEM_H
#define LOGFILESYSTEM_H

#include <string>
#include <vector>

namespace LogManager {

// Directory entry structure
struct DirectoryEntry {
    std::string relativePath;
    std::string name;
    bool isDirectory;
};

// Directory listing result
struct DirectoryListResult {
    bool success;
    std::vector<DirectoryEntry> entries;
};

// Text file read result
struct TextReadResult {
    bool success;
    std::string content;
};

// Abstract file system interface
class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    
    // List directory contents
    virtual DirectoryListResult listDirectory(const std::string& relativePath, bool recursive) const = 0;
    
    // Read text file
    virtual TextReadResult readTextFile(const std::string& relativePath) const = 0;
};

} // namespace LogManager

#endif // LOGFILESYSTEM_H
