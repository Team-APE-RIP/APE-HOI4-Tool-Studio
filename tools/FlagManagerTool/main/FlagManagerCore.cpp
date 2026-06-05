//-------------------------------------------------------------------------------------
// FlagManagerCore.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "FlagManagerCore.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <utility>

namespace FlagManager {

namespace {
constexpr int kLargeWidth = 82;
constexpr int kLargeHeight = 52;
constexpr int kMediumWidth = 41;
constexpr int kMediumHeight = 26;
constexpr int kSmallWidth = 10;
constexpr int kSmallHeight = 7;

struct ExportSize {
    int width = 0;
    int height = 0;
    const char* prefix = "";
};

constexpr ExportSize kExportSizes[] = {
    {kLargeWidth, kLargeHeight, ""},
    {kMediumWidth, kMediumHeight, "medium/"},
    {kSmallWidth, kSmallHeight, "small/"}
};

bool stringEndsWith(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size()
        && std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

std::string fileNameWithoutExtension(const std::string& fileName) {
    const std::size_t slash = fileName.find_last_of("/\\");
    const std::string leaf = slash == std::string::npos ? fileName : fileName.substr(slash + 1);
    const std::size_t dot = leaf.find_last_of('.');
    return dot == std::string::npos ? leaf : leaf.substr(0, dot);
}

bool containsString(const std::vector<std::string>& values, const std::string& needle) {
    return std::find(values.begin(), values.end(), needle) != values.end();
}
} // namespace

void FlagManagerCore::setFileSystem(IFileSystem* fileSystem) {
    m_fileSystem = fileSystem;
}

void FlagManagerCore::setImagePipeline(IImagePipeline* imagePipeline) {
    m_imagePipeline = imagePipeline;
}

void FlagManagerCore::setKnownTags(std::vector<std::string> tags) {
    m_knownTags.clear();
    m_knownTags.reserve(tags.size());

    for (std::string tag : tags) {
        tag = upperAscii(trim(std::move(tag)));
        if (!tag.empty() && !containsString(m_knownTags, tag)) {
            m_knownTags.push_back(std::move(tag));
        }
    }

    std::sort(m_knownTags.begin(), m_knownTags.end());
}

bool FlagManagerCore::initialize() {
    return refreshFlags();
}

bool FlagManagerCore::refreshFlags() {
    return rebuildManageData();
}

bool FlagManagerCore::setMode(ToolMode mode) {
    m_mode = mode;
    m_lastError.clear();
    return true;
}

bool FlagManagerCore::setSizeIndex(int sizeIndex) {
    if (sizeIndex < 0 || sizeIndex > 2) {
        m_lastError = "Unsupported flag size.";
        return false;
    }

    m_sizeIndex = sizeIndex;
    m_lastError.clear();
    return true;
}

bool FlagManagerCore::setSearchText(const std::string& text) {
    m_searchText = text;
    m_lastError.clear();
    return true;
}

bool FlagManagerCore::selectTag(const std::string& tag) {
    const std::string normalizedTag = upperAscii(trim(tag));
    if (m_tagRecords.find(normalizedTag) == m_tagRecords.end()) {
        m_lastError = "TAG is not available.";
        return false;
    }

    m_selectedTag = normalizedTag;
    m_lastError.clear();
    return true;
}

bool FlagManagerCore::selectImport(const std::string& importId) {
    if (importId.empty()) {
        m_selectedImportId.clear();
        clearSelection();
        return true;
    }

    if (!findImport(importId)) {
        m_lastError = "Imported image is not available.";
        return false;
    }

    m_selectedImportId = importId;
    setSelectionIds({importId});
    m_lastError.clear();
    return true;
}

bool FlagManagerCore::importFiles(std::vector<ImportedImage> files) {
    bool importedAny = false;
    std::vector<std::string> addedImportIds;
    addedImportIds.reserve(files.size());
    for (ImportedImage& file : files) {
        if (!isValidImage(file.image)) {
            continue;
        }

        bool duplicate = false;
        for (const ImportItem& item : m_imports) {
            if (!file.sourceKey.empty() && item.sourceKey == file.sourceKey) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }

        ImportItem item;
        item.id = nextImportId();
        item.sourceKey = std::move(file.sourceKey);
        item.fileName = std::move(file.fileName);
        item.image = std::make_shared<FlagImage>(std::move(file.image));
        item.crop = fullCropForImage(*item.image);
        m_imports.push_back(std::move(item));
        importedAny = true;
        addedImportIds.push_back(m_imports.back().id);
    }

    if (!addedImportIds.empty()) {
        for (const std::string& importId : addedImportIds) {
            addSelectionId(importId);
        }
        m_selectedImportId = addedImportIds.back();
    }

    if (!importedAny) {
        m_lastError = "No supported image was imported.";
        return false;
    }

    m_lastError.clear();
    return true;
}

bool FlagManagerCore::updateImportName(const std::string& importId, const std::string& name) {
    ImportItem* item = findImport(importId);
    if (!item) {
        m_lastError = "Imported image is not available.";
        return false;
    }

    const std::string trimmedName = trim(name);
    if (!trimmedName.empty() && !isNameUnique(m_imports, trimmedName, importId)) {
        m_lastError = "Flag name already exists.";
        return false;
    }

    item->name = trimmedName;
    m_lastError.clear();
    return true;
}

bool FlagManagerCore::updateImportCrop(const std::string& importId, const Rect& crop) {
    ImportItem* item = findImport(importId);
    if (!item) {
        m_lastError = "Imported image is not available.";
        return false;
    }

    if (!item->image || !isValidCrop(*item->image, crop)) {
        m_lastError = "Invalid crop range.";
        return false;
    }

    item->crop = crop;
    m_lastError.clear();
    return true;
}

bool FlagManagerCore::fillNamesFromFileName() {
    std::vector<std::string> ids = m_selectedImportIds;
    if (ids.empty() && !m_selectedImportId.empty()) {
        ids.push_back(m_selectedImportId);
    }
    if (ids.empty()) {
        m_lastError = "No imported image is selected.";
        return false;
    }

    bool changed = false;
    for (const std::string& id : ids) {
        ImportItem* item = findImport(id);
        if (!item) {
            continue;
        }

        const std::string candidate = trim(fileNameWithoutExtension(item->fileName));
        if (candidate.empty() || !isNameUnique(m_imports, candidate, id)) {
            continue;
        }

        item->name = candidate;
        changed = true;
    }

    if (!changed) {
        m_lastError = "No unique file name can be applied.";
        return false;
    }

    m_lastError.clear();
    return true;
}

bool FlagManagerCore::removeSelectedImports() {
    std::vector<std::string> ids = m_selectedImportIds;
    if (ids.empty() && !m_selectedImportId.empty()) {
        ids.push_back(m_selectedImportId);
    }
    if (ids.empty()) {
        m_lastError = "No imported image is selected.";
        return false;
    }

    m_imports.erase(
        std::remove_if(
            m_imports.begin(),
            m_imports.end(),
            [&ids](const ImportItem& item) {
                return containsString(ids, item.id);
            }
        ),
        m_imports.end()
    );

    m_selectedImportIds.clear();
    if (!m_imports.empty()) {
        m_selectedImportId = m_imports.front().id;
        setSelectionIds({m_selectedImportId});
    } else {
        m_selectedImportId.clear();
    }

    m_lastError.clear();
    return true;
}

bool FlagManagerCore::selectAllImports() {
    m_selectedImportIds.clear();
    m_selectedImportIds.reserve(m_imports.size());
    for (const ImportItem& item : m_imports) {
        m_selectedImportIds.push_back(item.id);
    }
    if (!m_imports.empty() && m_selectedImportId.empty()) {
        m_selectedImportId = m_imports.front().id;
    }
    m_lastError.clear();
    return true;
}

bool FlagManagerCore::deselectAllImports() {
    clearSelection();
    m_lastError.clear();
    return true;
}

ExportResult FlagManagerCore::exportCurrent() {
    if (m_selectedImportId.empty()) {
        return {false, false, "No imported image is selected."};
    }

    const std::vector<std::string> ids{m_selectedImportId};
    const std::vector<std::string> overwriteFiles = collectOverwriteFiles(ids);
    if (!overwriteFiles.empty()) {
        m_pendingExport = {PendingExportKind::Current, ids, overwriteFiles};
        return {false, true, ""};
    }

    return performExport(ids);
}

ExportResult FlagManagerCore::exportAll() {
    std::vector<std::string> ids;
    ids.reserve(m_imports.size());
    for (const ImportItem& item : m_imports) {
        if (isExportableImport(item)) {
            ids.push_back(item.id);
        }
    }
    if (ids.empty()) {
        return {false, false, "No imported image is ready for export."};
    }

    const std::vector<std::string> overwriteFiles = collectOverwriteFiles(ids);
    if (!overwriteFiles.empty()) {
        m_pendingExport = {PendingExportKind::All, ids, overwriteFiles};
        return {false, true, ""};
    }

    return performExport(ids);
}

bool FlagManagerCore::confirmPendingOverwrite() {
    if (m_pendingExport.kind == PendingExportKind::None) {
        m_lastError = "No pending export requires confirmation.";
        return false;
    }

    const std::vector<std::string> ids = m_pendingExport.importIds;
    m_pendingExport = {};
    const ExportResult result = performExport(ids);
    if (!result.success) {
        m_lastError = result.errorMessage;
    }
    return result.success;
}

void FlagManagerCore::cancelPendingOverwrite() {
    m_pendingExport = {};
}

bool FlagManagerCore::handleAction(const std::string& actionType,
                                   const std::map<std::string, std::string>& params) {
    const std::string action = normalizeText(actionType);
    if (action == "page_select" || action == "sidebar_button_click") {
        m_lastError.clear();
        return true;
    }
    if (action == "set_mode" || action == "switch_mode") {
        const std::string mode = normalizeText(params.count("mode") ? params.at("mode") : params.count("value") ? params.at("value") : params.count("targetId") ? params.at("targetId") : "");
        return setMode(mode == "new" || mode == "create" ? ToolMode::New : ToolMode::Manage);
    }
    if (action == "set_size" || action == "set_size_index") {
        const std::string value = params.count("sizeIndex") ? params.at("sizeIndex") : params.count("targetId") ? params.at("targetId") : params.count("value") ? params.at("value") : "0";
        return setSizeIndex(value == "medium" ? 1 : value == "small" ? 2 : std::clamp(std::atoi(value.c_str()), 0, 2));
    }
    if (action == "select_tag" || action == "on_tag_selected") {
        const std::string value = params.count("rowId") ? params.at("rowId") : params.count("targetId") ? params.at("targetId") : params.count("tag") ? params.at("tag") : "";
        return selectTag(value);
    }
    if (action == "select_import" || action == "on_import_selected") {
        const std::string value = params.count("rowId") ? params.at("rowId") : params.count("targetId") ? params.at("targetId") : params.count("id") ? params.at("id") : "";
        return selectImport(value);
    }
    if (action == "search" || action == "search_changed" || action == "set_search_text") {
        return setSearchText(params.count("text") ? params.at("text") : params.count("value") ? params.at("value") : "");
    }
    if (action == "fill_name" || action == "fill_name_from_file") {
        return fillNamesFromFileName();
    }
    if (action == "remove_import" || action == "remove_selected" || action == "remove_from_list") {
        return removeSelectedImports();
    }
    if (action == "select_all" || action == "select_all_imports") {
        return selectAllImports();
    }
    if (action == "deselect_all" || action == "deselect_all_imports") {
        return deselectAllImports();
    }

    m_lastError = "Unsupported Flag Manager action.";
    return false;
}

Snapshot FlagManagerCore::buildSnapshot() const {
    Snapshot snapshot;
    snapshot.mode = m_mode;
    snapshot.sizeIndex = m_sizeIndex;
    snapshot.searchText = m_searchText;
    snapshot.selectedTag = m_selectedTag;
    snapshot.selectedImportId = m_selectedImportId;
    snapshot.statusText = m_statusText;
    snapshot.canExportCurrent = m_mode == ToolMode::New && !m_selectedImportId.empty();
    snapshot.canExportAll = m_mode == ToolMode::New && !m_imports.empty();
    snapshot.pendingOverwrite = m_pendingExport.kind != PendingExportKind::None;
    snapshot.pendingOverwriteFiles = m_pendingExport.overwriteFiles;
    snapshot.lastError = m_lastError;
    snapshot.hasSelection = m_mode == ToolMode::New
        ? (!m_selectedImportId.empty() || !m_selectedImportIds.empty())
        : !m_selectedTag.empty();

    if (m_mode == ToolMode::Manage) {
        const ManageDisplayData manageData = buildManageDisplayData();
        snapshot.tags = manageData.tags;
        snapshot.selectedTagVariants = manageData.selectedTagVariants;
    } else {
        snapshot.imports = m_imports;
        snapshot.selectedImportIds = m_selectedImportIds;
    }
    return snapshot;
}

std::vector<ImportItem> FlagManagerCore::takeRetiredImports() {
    std::vector<ImportItem> retired;
    retired.swap(m_retiredImports);
    return retired;
}

bool FlagManagerCore::rebuildManageData() {
    if (!m_fileSystem) {
        m_lastError = "File system bridge is not available.";
        return false;
    }

    const EffectiveFileListResult fileListResult = m_fileSystem->listEffectiveFiles();
    if (!fileListResult.success) {
        m_lastError = fileListResult.errorMessage;
        return false;
    }

    m_tagRecords.clear();

    TagRecord cosmeticRecord;
    cosmeticRecord.tag = "COSMETIC";
    cosmeticRecord.cosmetic = true;
    cosmeticRecord.status = FlagStatus::Cosmetic;
    m_tagRecords[cosmeticRecord.tag] = cosmeticRecord;

    for (const std::string& tag : m_knownTags) {
        TagRecord record;
        record.tag = tag;
        record.cosmetic = false;
        m_tagRecords[tag] = record;
    }

    std::set<std::string> knownTagSet(m_knownTags.begin(), m_knownTags.end());

    for (const EffectiveFileEntry& entry : fileListResult.entries) {
        const std::string logicalPath = normalizePath(entry.logicalPath);
        std::string baseName;
        int sizeIndex = 0;
        if (!parseFlagPath(logicalPath, &baseName, &sizeIndex)) {
            continue;
        }

        const std::string tag = upperAscii(baseName.substr(0, std::min<std::size_t>(3, baseName.size())));
        const bool isCosmetic = knownTagSet.find(tag) == knownTagSet.end();
        const std::string recordKey = isCosmetic ? "COSMETIC" : tag;

        TagRecord& record = m_tagRecords[recordKey];
        if (record.tag.empty()) {
            record.tag = recordKey;
        }
        record.cosmetic = isCosmetic || recordKey == "COSMETIC";

        auto variantIt = std::find_if(
            record.variants.begin(),
            record.variants.end(),
            [&baseName](const FlagVariantRecord& variant) {
                return variant.name == baseName;
            }
        );
        if (variantIt == record.variants.end()) {
            FlagVariantRecord variant;
            variant.name = baseName;
            record.variants.push_back(std::move(variant));
            variantIt = std::prev(record.variants.end());
        }

        if (sizeIndex == 0) {
            variantIt->hasLarge = true;
            variantIt->largePath = logicalPath;
        } else if (sizeIndex == 1) {
            variantIt->hasMedium = true;
            variantIt->mediumPath = logicalPath;
        } else {
            variantIt->hasSmall = true;
            variantIt->smallPath = logicalPath;
        }
    }

    for (auto& pair : m_tagRecords) {
        TagRecord& record = pair.second;
        std::sort(
            record.variants.begin(),
            record.variants.end(),
            [](const FlagVariantRecord& left, const FlagVariantRecord& right) {
                return left.name < right.name;
            }
        );

        record.hasDefault = false;
        record.complete = true;
        record.previewPath.clear();
        for (const FlagVariantRecord& variant : record.variants) {
            if (upperAscii(variant.name) == record.tag) {
                record.hasDefault = true;
                if (!variant.smallPath.empty()) {
                    record.previewPath = variant.smallPath;
                } else if (!variant.largePath.empty()) {
                    record.previewPath = variant.largePath;
                } else if (!variant.mediumPath.empty()) {
                    record.previewPath = variant.mediumPath;
                }
            }
            if (!(variant.hasLarge && variant.hasMedium && variant.hasSmall)) {
                record.complete = false;
            }
        }
        record.status = statusFromRecord(record);
    }

    if (!m_selectedTag.empty() && m_tagRecords.find(m_selectedTag) == m_tagRecords.end()) {
        m_selectedTag.clear();
    }
    if (m_selectedTag.empty() && !m_tagRecords.empty()) {
        auto defaultIt = std::find_if(
            m_tagRecords.begin(),
            m_tagRecords.end(),
            [](const auto& pair) {
                return !pair.second.cosmetic;
            }
        );
        m_selectedTag = defaultIt != m_tagRecords.end()
            ? defaultIt->first
            : m_tagRecords.begin()->first;
    }

    m_lastError.clear();
    return true;
}

FlagManagerCore::ManageDisplayData FlagManagerCore::buildManageDisplayData() const {
    ManageDisplayData data;
    data.tags.reserve(m_tagRecords.size());

    const auto cosmeticIt = m_tagRecords.find("COSMETIC");
    if (cosmeticIt != m_tagRecords.end()) {
        const TagRecord& record = cosmeticIt->second;
        data.tags.push_back({
            record.tag,
            record.cosmetic,
            record.hasDefault,
            record.complete,
            record.status,
            record.previewPath
        });
    }

    for (const auto& pair : m_tagRecords) {
        if (pair.first == "COSMETIC") {
            continue;
        }
        const TagRecord& record = pair.second;
        data.tags.push_back({
            record.tag,
            record.cosmetic,
            record.hasDefault,
            record.complete,
            record.status,
            record.previewPath
        });
    }

    const TagRecord* selectedRecord = findTag(m_selectedTag);
    if (selectedRecord) {
        data.selectedTagVariants.reserve(selectedRecord->variants.size());
        for (const FlagVariantRecord& variant : selectedRecord->variants) {
            ManageVariantDisplay display;
            display.name = variant.name;
            display.hasLarge = variant.hasLarge;
            display.hasMedium = variant.hasMedium;
            display.hasSmall = variant.hasSmall;
            display.status = (variant.hasLarge && variant.hasMedium && variant.hasSmall)
                ? FlagStatus::Complete
                : FlagStatus::Partial;
            if (m_sizeIndex == 0) {
                display.previewPath = variant.largePath;
            } else if (m_sizeIndex == 1) {
                display.previewPath = variant.mediumPath;
            } else {
                display.previewPath = variant.smallPath;
            }
            display.tooltip = statusText(display.status);
            data.selectedTagVariants.push_back(std::move(display));
        }
    }

    return data;
}

bool FlagManagerCore::setSelectionIds(const std::vector<std::string>& ids) {
    m_selectedImportIds.clear();
    for (const std::string& id : ids) {
        if (findImport(id) && !containsString(m_selectedImportIds, id)) {
            m_selectedImportIds.push_back(id);
        }
    }
    return true;
}

bool FlagManagerCore::addSelectionId(const std::string& id) {
    if (!findImport(id)) {
        return false;
    }
    if (!containsString(m_selectedImportIds, id)) {
        m_selectedImportIds.push_back(id);
    }
    return true;
}

void FlagManagerCore::clearSelection() {
    m_selectedImportIds.clear();
}

bool FlagManagerCore::ensureSelectedImportExists() {
    if (m_selectedImportId.empty()) {
        return false;
    }
    if (findImport(m_selectedImportId)) {
        return true;
    }
    m_selectedImportId.clear();
    return false;
}

ImportItem* FlagManagerCore::findImport(const std::string& id) {
    auto it = std::find_if(
        m_imports.begin(),
        m_imports.end(),
        [&id](const ImportItem& item) {
            return item.id == id;
        }
    );
    return it == m_imports.end() ? nullptr : &(*it);
}

const ImportItem* FlagManagerCore::findImport(const std::string& id) const {
    auto it = std::find_if(
        m_imports.begin(),
        m_imports.end(),
        [&id](const ImportItem& item) {
            return item.id == id;
        }
    );
    return it == m_imports.end() ? nullptr : &(*it);
}

TagRecord* FlagManagerCore::findTag(const std::string& tag) {
    auto it = m_tagRecords.find(tag);
    return it == m_tagRecords.end() ? nullptr : &it->second;
}

const TagRecord* FlagManagerCore::findTag(const std::string& tag) const {
    auto it = m_tagRecords.find(tag);
    return it == m_tagRecords.end() ? nullptr : &it->second;
}

std::string FlagManagerCore::normalizeText(const std::string& value) {
    std::string result = trim(value);
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        }
    );
    return result;
}

std::string FlagManagerCore::normalizePath(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    while (value.find("//") != std::string::npos) {
        value.replace(value.find("//"), 2, "/");
    }
    if (!value.empty() && value.front() == '/') {
        value.erase(value.begin());
    }
    return value;
}

std::string FlagManagerCore::upperAscii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        }
    );
    return value;
}

std::string FlagManagerCore::trim(std::string value) {
    auto notSpace = [](unsigned char ch) {
        return std::isspace(ch) == 0;
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool FlagManagerCore::parseFlagPath(const std::string& logicalPath, std::string* outBaseName, int* outSizeIndex) {
    const std::string normalizedPath = normalizePath(logicalPath);
    const std::string lowerPath = normalizeText(normalizedPath);
    if (!stringEndsWith(lowerPath, ".tga") || lowerPath.rfind("gfx/flags/", 0) != 0) {
        return false;
    }

    std::string subPath = normalizedPath.substr(std::string("gfx/flags/").size());
    std::string lowerSubPath = normalizeText(subPath);
    int sizeIndex = 0;
    if (lowerSubPath.rfind("medium/", 0) == 0) {
        sizeIndex = 1;
        subPath = subPath.substr(std::string("medium/").size());
    } else if (lowerSubPath.rfind("small/", 0) == 0) {
        sizeIndex = 2;
        subPath = subPath.substr(std::string("small/").size());
    }

    const std::string baseName = fileNameWithoutExtension(subPath);
    if (baseName.empty()) {
        return false;
    }

    if (outBaseName) {
        *outBaseName = baseName;
    }
    if (outSizeIndex) {
        *outSizeIndex = sizeIndex;
    }
    return true;
}

FlagStatus FlagManagerCore::statusFromRecord(const TagRecord& tagRecord) {
    if (tagRecord.cosmetic) {
        return FlagStatus::Cosmetic;
    }
    if (!tagRecord.hasDefault) {
        return FlagStatus::MissingDefault;
    }
    if (!tagRecord.complete) {
        return FlagStatus::Partial;
    }
    return FlagStatus::Complete;
}

std::string FlagManagerCore::statusText(FlagStatus status) {
    switch (status) {
    case FlagStatus::Complete:
        return "Complete";
    case FlagStatus::MissingDefault:
        return "Missing default flag";
    case FlagStatus::Partial:
        return "Missing one or more flag sizes";
    case FlagStatus::Cosmetic:
        return "Cosmetic";
    default:
        return "Unknown";
    }
}

std::string FlagManagerCore::statusColor(FlagStatus status) {
    switch (status) {
    case FlagStatus::MissingDefault:
        return "#DC2626";
    case FlagStatus::Partial:
        return "#D97706";
    default:
        return "";
    }
}

std::string FlagManagerCore::previewColor(FlagStatus status) {
    switch (status) {
    case FlagStatus::MissingDefault:
        return "#DC2626";
    case FlagStatus::Partial:
        return "#D97706";
    case FlagStatus::Cosmetic:
        return "#007AFF";
    default:
        return "#16A34A";
    }
}

Rect FlagManagerCore::fullCropForImage(const FlagImage& image) {
    if (!isValidImage(image)) {
        return {};
    }
    return {0, 0, image.width - 1, image.height - 1};
}

bool FlagManagerCore::isNameUnique(const std::vector<ImportItem>& imports,
                                   const std::string& name,
                                   const std::string& excludeId) {
    for (const ImportItem& item : imports) {
        if (item.id != excludeId && item.name == name) {
            return false;
        }
    }
    return true;
}

bool FlagManagerCore::isExportableImport(const ImportItem& item) {
    return item.image
        && !item.name.empty()
        && isValidCrop(*item.image, item.crop);
}

std::string FlagManagerCore::exportValidationError(const ImportItem& item) {
    if (item.name.empty()) {
        return "Flag name is empty.";
    }
    if (!item.image || !isValidCrop(*item.image, item.crop)) {
        return "Invalid crop range.";
    }
    return "Imported image is not ready for export.";
}

ExportResult FlagManagerCore::performExport(const std::vector<std::string>& importIds) {
    if (!m_fileSystem) {
        return {false, false, "File system bridge is not available."};
    }

    struct ExportWorkItem {
        std::string id;
        ImportItem item;
    };

    std::map<std::string, ImportItem> importsById;
    for (const ImportItem& item : m_imports) {
        importsById.emplace(item.id, item);
    }

    std::vector<ExportWorkItem> workItems;
    workItems.reserve(importIds.size());
    std::string firstValidationError;
    for (const std::string& id : importIds) {
        const auto itemIt = importsById.find(id);
        if (itemIt == importsById.end()) {
            continue;
        }

        const ImportItem& item = itemIt->second;
        if (!isExportableImport(item)) {
            if (firstValidationError.empty()) {
                firstValidationError = exportValidationError(item);
            }
            continue;
        }
        workItems.push_back({id, item});
    }

    if (workItems.empty()) {
        return {false, false, firstValidationError.empty()
            ? "No imported image was exported."
            : firstValidationError};
    }

    const ExportResult directoryResult = ensureExportDirectories();
    if (!directoryResult.success) {
        return directoryResult;
    }

    std::vector<std::string> exportedIds;
    exportedIds.reserve(workItems.size());
    for (const ExportWorkItem& workItem : workItems) {
        const ExportResult result = exportSingleImport(workItem.item);
        if (!result.success) {
            return result;
        }
        exportedIds.push_back(workItem.id);
    }

    if (exportedIds.empty()) {
        return {false, false, "No imported image was exported."};
    }

    const std::set<std::string> exportedIdSet(exportedIds.begin(), exportedIds.end());

    std::vector<ImportItem> retainedImports;
    retainedImports.reserve(m_imports.size());
    for (ImportItem& item : m_imports) {
        if (exportedIdSet.find(item.id) != exportedIdSet.end()) {
            m_retiredImports.push_back(std::move(item));
        } else {
            retainedImports.push_back(std::move(item));
        }
    }
    m_imports = std::move(retainedImports);

    m_selectedImportIds.erase(
        std::remove_if(
            m_selectedImportIds.begin(),
            m_selectedImportIds.end(),
            [&exportedIdSet](const std::string& id) {
                return exportedIdSet.find(id) != exportedIdSet.end();
            }
        ),
        m_selectedImportIds.end()
    );

    if (!findImport(m_selectedImportId)) {
        m_selectedImportId = m_imports.empty() ? std::string() : m_imports.front().id;
        m_selectedImportIds.clear();
        if (!m_selectedImportId.empty()) {
            m_selectedImportIds.push_back(m_selectedImportId);
        }
    }

    m_statusText = "Export complete.";
    m_lastError.clear();
    return {true, false, ""};
}

ExportResult FlagManagerCore::ensureExportDirectories() {
    if (!m_fileSystem) {
        return {false, false, "File system bridge is not available."};
    }

    const FileWriteResult ensureRootResult = m_fileSystem->ensureModDirectory("gfx/flags");
    if (!ensureRootResult.success) {
        return {false, false, ensureRootResult.errorMessage};
    }

    for (const ExportSize& size : kExportSizes) {
        if (size.prefix[0] == '\0') {
            continue;
        }

        const FileWriteResult ensureDirResult =
            m_fileSystem->ensureModDirectory(std::string("gfx/flags/") + size.prefix);
        if (!ensureDirResult.success) {
            return {false, false, ensureDirResult.errorMessage};
        }
    }

    return {true, false, ""};
}

ExportResult FlagManagerCore::exportSingleImport(const ImportItem& item) {
    if (!isExportableImport(item)) {
        return {false, false, exportValidationError(item)};
    }

    for (const ExportSize& size : kExportSizes) {
        const std::string relativeDir = std::string("gfx/flags/") + size.prefix;
        const std::string relativePath = relativeDir + item.name + ".tga";

        const FlagImage resized = m_imagePipeline
            ? m_imagePipeline->cropResizeImage(*item.image, item.crop, size.width, size.height)
            : FlagManager::resizeCropImage(*item.image, item.crop, size.width, size.height);
        const std::vector<std::uint8_t> encoded = m_imagePipeline
            ? m_imagePipeline->encodeTga32(resized)
            : FlagManager::encodeTga32(resized);
        if (encoded.empty()) {
            return {false, false, "Failed to encode TGA image."};
        }

        const FileWriteResult writeResult = m_fileSystem->writeModFile(relativePath, encoded);
        if (!writeResult.success) {
            return {false, false, writeResult.errorMessage};
        }
    }

    return {true, false, ""};
}

std::vector<std::string> FlagManagerCore::collectOverwriteFiles(const std::vector<std::string>& importIds) const {
    std::vector<std::string> files;
    if (!m_fileSystem) {
        return files;
    }

    std::map<std::string, const ImportItem*> importsById;
    for (const ImportItem& item : m_imports) {
        importsById.emplace(item.id, &item);
    }

    for (const std::string& id : importIds) {
        const auto itemIt = importsById.find(id);
        if (itemIt == importsById.end() || !isExportableImport(*itemIt->second)) {
            continue;
        }
        const ImportItem* item = itemIt->second;
        for (const ExportSize& size : kExportSizes) {
            const std::string relativePath = std::string("gfx/flags/") + size.prefix + item->name + ".tga";
            const FileReadResult readResult = m_fileSystem->readModFile(relativePath);
            if (readResult.success) {
                files.push_back(relativePath);
            }
        }
    }
    return files;
}

std::string FlagManagerCore::nextImportId() {
    return "import_" + std::to_string(m_nextImportId++);
}

} // namespace FlagManager
