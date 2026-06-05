//-------------------------------------------------------------------------------------
// FlagTypes.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FLAGTYPES_H
#define FLAGTYPES_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace FlagManager {

enum class ToolMode {
    Manage,
    New
};

enum class FlagStatus {
    Complete,
    MissingDefault,
    Partial,
    Cosmetic,
    Unknown
};

enum class PendingExportKind {
    None,
    Current,
    All
};

struct Rect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

struct FlagImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

struct FlagVariantRecord {
    std::string name;
    bool hasLarge = false;
    bool hasMedium = false;
    bool hasSmall = false;
    std::string largePath;
    std::string mediumPath;
    std::string smallPath;
};

struct TagRecord {
    std::string tag;
    bool cosmetic = false;
    bool hasDefault = false;
    bool complete = false;
    FlagStatus status = FlagStatus::Unknown;
    std::string previewPath;
    std::vector<FlagVariantRecord> variants;
};

struct TagListRow {
    std::string tag;
    bool cosmetic = false;
    bool hasDefault = false;
    bool complete = false;
    FlagStatus status = FlagStatus::Unknown;
    std::string previewPath;
};

struct ImportItem {
    std::string id;
    std::string sourceKey;
    std::string fileName;
    std::string name;
    std::shared_ptr<FlagImage> image;
    Rect crop;
};

struct ManageVariantDisplay {
    std::string name;
    bool hasLarge = false;
    bool hasMedium = false;
    bool hasSmall = false;
    FlagStatus status = FlagStatus::Unknown;
    std::string previewPath;
    std::string tooltip;
};

struct Snapshot {
    ToolMode mode = ToolMode::Manage;
    int sizeIndex = 0;
    std::string searchText;
    std::string selectedTag;
    std::string selectedImportId;
    std::string statusText;
    bool hasSelection = false;
    bool canExportCurrent = false;
    bool canExportAll = false;
    bool pendingOverwrite = false;
    std::vector<std::string> pendingOverwriteFiles;
    std::string lastError;
    std::vector<TagListRow> tags;
    std::vector<ManageVariantDisplay> selectedTagVariants;
    std::vector<ImportItem> imports;
    std::vector<std::string> selectedImportIds;
};

struct ImportedImage {
    std::string sourceKey;
    std::string fileName;
    FlagImage image;
};

struct ExportResult {
    bool success = false;
    bool pendingOverwrite = false;
    std::string errorMessage;
};

} // namespace FlagManager

#endif // FLAGTYPES_H
