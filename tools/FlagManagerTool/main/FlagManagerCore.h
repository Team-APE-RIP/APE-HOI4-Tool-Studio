//-------------------------------------------------------------------------------------
// FlagManagerCore.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FLAGMANAGERCORE_H
#define FLAGMANAGERCORE_H

#include "FlagFileSystem.h"
#include "FlagImage.h"
#include "FlagImagePipeline.h"
#include "FlagTypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace FlagManager {

class FlagManagerCore {
public:
    FlagManagerCore() = default;

    void setFileSystem(IFileSystem* fileSystem);
    void setImagePipeline(IImagePipeline* imagePipeline);
    void setKnownTags(std::vector<std::string> tags);

    bool initialize();
    bool refreshFlags();

    bool setMode(ToolMode mode);
    bool setSizeIndex(int sizeIndex);
    bool setSearchText(const std::string& text);
    bool selectTag(const std::string& tag);
    bool selectImport(const std::string& importId);

    bool importFiles(std::vector<ImportedImage> files);
    bool updateImportName(const std::string& importId, const std::string& name);
    bool updateImportCrop(const std::string& importId, const Rect& crop);
    bool fillNamesFromFileName();
    bool removeSelectedImports();
    bool selectAllImports();
    bool deselectAllImports();

    ExportResult exportCurrent();
    ExportResult exportAll();
    bool confirmPendingOverwrite();
    void cancelPendingOverwrite();

    bool handleAction(const std::string& actionType, const std::map<std::string, std::string>& params);

    Snapshot buildSnapshot() const;
    std::vector<ImportItem> takeRetiredImports();
    const std::string& lastError() const noexcept { return m_lastError; }

private:
    struct ExportContext {
        PendingExportKind kind = PendingExportKind::None;
        std::vector<std::string> importIds;
        std::vector<std::string> overwriteFiles;
    };

    struct ManageDisplayData {
        std::vector<TagListRow> tags;
        std::vector<ManageVariantDisplay> selectedTagVariants;
    };

    bool rebuildManageData();
    ManageDisplayData buildManageDisplayData() const;
    bool setSelectionIds(const std::vector<std::string>& ids);
    bool addSelectionId(const std::string& id);
    void clearSelection();
    bool ensureSelectedImportExists();
    ImportItem* findImport(const std::string& id);
    const ImportItem* findImport(const std::string& id) const;
    TagRecord* findTag(const std::string& tag);
    const TagRecord* findTag(const std::string& tag) const;
    static std::string normalizeText(const std::string& value);
    static std::string normalizePath(std::string value);
    static std::string upperAscii(std::string value);
    static std::string trim(std::string value);
    static bool parseFlagPath(const std::string& logicalPath, std::string* outBaseName, int* outSizeIndex);
    static FlagStatus statusFromRecord(const TagRecord& tagRecord);
    static std::string statusText(FlagStatus status);
    static std::string statusColor(FlagStatus status);
    static std::string previewColor(FlagStatus status);
    static Rect fullCropForImage(const FlagImage& image);
    static bool isNameUnique(const std::vector<ImportItem>& imports, const std::string& name, const std::string& excludeId);
    static bool isExportableImport(const ImportItem& item);
    static std::string exportValidationError(const ImportItem& item);
    ExportResult ensureExportDirectories();
    ExportResult performExport(const std::vector<std::string>& importIds);
    ExportResult exportSingleImport(const ImportItem& item);
    std::vector<std::string> collectOverwriteFiles(const std::vector<std::string>& importIds) const;
    std::string nextImportId();

    IFileSystem* m_fileSystem = nullptr;
    IImagePipeline* m_imagePipeline = nullptr;
    std::vector<std::string> m_knownTags;
    std::map<std::string, TagRecord> m_tagRecords;
    ToolMode m_mode = ToolMode::Manage;
    int m_sizeIndex = 0;
    std::string m_searchText;
    std::string m_selectedTag;
    std::vector<ImportItem> m_imports;
    std::string m_selectedImportId;
    std::vector<std::string> m_selectedImportIds;
    int m_nextImportId = 1;
    std::string m_lastError;
    std::string m_statusText;
    ExportContext m_pendingExport;
    std::vector<ImportItem> m_retiredImports;
};

} // namespace FlagManager

#endif // FLAGMANAGERCORE_H
