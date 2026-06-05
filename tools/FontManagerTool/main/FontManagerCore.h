//-------------------------------------------------------------------------------------
// FontManagerCore.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FONTMANAGER_FONTMANAGERCORE_H
#define FONTMANAGER_FONTMANAGERCORE_H

#include "FontFileSystem.h"
#include "FontTypes.h"

#include <map>
#include <string>
#include <vector>

namespace FontManager {

class FontManagerCore {
public:
    void setFileSystem(IFontFileSystem* fileSystem);
    void setDryadAtlas(IDryadAtlasService* dryadAtlas);

    bool initialize();
    bool refreshFonts();
    bool setMode(ToolMode mode);
    bool selectFont(const std::string& id);
    bool selectImport(const std::string& id);
    bool setPreviewText(const std::string& text);
    bool importTtfFiles(std::vector<ImportedFont> imports);
    bool updateSettings(const FontGenerationSettings& settings);
    ExportResult exportSelectedImport();
    bool confirmPendingOverwrite();
    void cancelPendingOverwrite();
    bool handleAction(const std::string& actionType, const std::map<std::string, std::string>& params);

    Snapshot buildSnapshot() const;
    const std::string& lastError() const noexcept { return m_lastError; }

private:
    struct PendingExport {
        GeneratedFontPackage package;
        std::vector<std::string> overwriteFiles;
        bool active = false;
    };

    bool refreshFonts(bool failOnError);
    ExistingFont* findFont(const std::string& id);
    const ExistingFont* findFont(const std::string& id) const;
    ImportedFont* findImport(const std::string& id);
    const ImportedFont* findImport(const std::string& id) const;
    bool rebuildPreview();
    bool writePackage(const GeneratedFontPackage& package);
    std::vector<std::string> collectOverwriteFiles(const GeneratedFontPackage& package) const;
    static std::string nextImportId(int value);
    static std::string normalizeAction(std::string value);
    static std::string trim(std::string value);
    static std::string sanitizeAtlasName(std::string value);

    IFontFileSystem* m_fileSystem = nullptr;
    IDryadAtlasService* m_dryadAtlas = nullptr;
    ToolMode m_mode = ToolMode::Manage;
    std::vector<ExistingFont> m_fonts;
    std::vector<ImportedFont> m_imports;
    std::vector<FontTextColor> m_globalTextColors;
    std::string m_selectedFontId;
    std::string m_selectedImportId;
    std::string m_previewText = "The quick brown fox §Rred §Ggreen §!normal\nHOI4 §Ybitmap font§!";
    FontGenerationSettings m_settings;
    std::string m_previewBase64;
    int m_previewWidth = 0;
    int m_previewHeight = 0;
    int m_nextImportId = 1;
    std::string m_lastError;
    std::string m_statusText;
    PendingExport m_pendingExport;
};

} // namespace FontManager

#endif // FONTMANAGER_FONTMANAGERCORE_H
