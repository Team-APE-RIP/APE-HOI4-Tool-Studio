//-------------------------------------------------------------------------------------
// FlagFileSystem.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FLAGFILESYSTEM_H
#define FLAGFILESYSTEM_H

#include "FlagTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace FlagManager {

struct EffectiveFileEntry {
    std::string logicalPath;
    std::string source;
};

struct EffectiveFileListResult {
    bool success = false;
    std::vector<EffectiveFileEntry> entries;
    std::string errorMessage;
};

struct FileReadResult {
    bool success = false;
    std::vector<std::uint8_t> content;
    std::string errorMessage;
};

struct FileWriteResult {
    bool success = false;
    std::string errorMessage;
};

class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    virtual EffectiveFileListResult listEffectiveFiles() const = 0;
    virtual FileReadResult readEffectiveFile(const std::string& logicalPath) const = 0;
    virtual FileReadResult readModFile(const std::string& logicalPath) const = 0;
    virtual FileWriteResult ensureModDirectory(const std::string& logicalPath) const = 0;
    virtual FileWriteResult writeModFile(const std::string& logicalPath, const std::vector<std::uint8_t>& content) const = 0;
};

} // namespace FlagManager

#endif // FLAGFILESYSTEM_H
