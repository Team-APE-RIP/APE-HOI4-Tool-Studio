//-------------------------------------------------------------------------------------
// FontManagerCore.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "FontManagerCore.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <utility>

namespace FontManager {
namespace {

std::string trimTextColorCode(std::string value) {
    auto notSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::vector<FontTextColor> sanitizeTextColors(std::vector<FontTextColor> colors) {
    std::vector<FontTextColor> result;
    result.reserve(colors.size());
    for (FontTextColor& textColor : colors) {
        FontTextColor sanitized;
        sanitized.code = trimTextColorCode(std::move(textColor.code));
        if (sanitized.code.empty()) {
            continue;
        }
        sanitized.color.red = std::clamp(textColor.color.red, 0, 255);
        sanitized.color.green = std::clamp(textColor.color.green, 0, 255);
        sanitized.color.blue = std::clamp(textColor.color.blue, 0, 255);
        result.push_back(std::move(sanitized));
    }
    return result;
}

} // namespace

void FontManagerCore::setFileSystem(IFontFileSystem* fileSystem) {
    m_fileSystem = fileSystem;
}

void FontManagerCore::setDryadAtlas(IDryadAtlasService* dryadAtlas) {
    m_dryadAtlas = dryadAtlas;
}

bool FontManagerCore::initialize() {
    return refreshFonts(false);
}

bool FontManagerCore::refreshFonts() {
    return refreshFonts(true);
}

bool FontManagerCore::refreshFonts(bool failOnError) {
    if (!m_fileSystem) {
        m_lastError = "File bridge is not available.";
        m_fonts.clear();
        m_selectedFontId.clear();
        m_previewBase64.clear();
        m_previewWidth = 0;
        m_previewHeight = 0;
        return !failOnError;
    }

    const ExistingFontListResult result = m_fileSystem->listExistingFonts();
    if (!result.success) {
        m_lastError = result.errorMessage;
        m_fonts.clear();
        m_selectedFontId.clear();
        m_previewBase64.clear();
        m_previewWidth = 0;
        m_previewHeight = 0;
        return !failOnError;
    }

    m_fonts = result.fonts;
    m_globalTextColors = result.globalTextColors;
    std::sort(
        m_fonts.begin(),
        m_fonts.end(),
        [](const ExistingFont& left, const ExistingFont& right) {
            const std::string& leftName = left.displayName.empty() ? left.name : left.displayName;
            const std::string& rightName = right.displayName.empty() ? right.name : right.displayName;
            return leftName < rightName;
        }
    );

    const bool hadManageSelection = m_mode == ToolMode::Manage && !m_selectedFontId.empty();
    bool selectedDefaultFont = false;
    if (!m_selectedFontId.empty() && !findFont(m_selectedFontId)) {
        m_selectedFontId.clear();
    }
    if (m_selectedFontId.empty() && !m_fonts.empty()) {
        m_selectedFontId = m_fonts.front().id;
        selectedDefaultFont = m_mode == ToolMode::Manage;
    }
    m_lastError.clear();
    if (m_mode == ToolMode::New || hadManageSelection || selectedDefaultFont) {
        rebuildPreview();
    } else {
        m_previewBase64.clear();
        m_previewWidth = 0;
        m_previewHeight = 0;
    }
    return true;
}

bool FontManagerCore::setMode(ToolMode mode) {
    m_mode = mode;
    m_lastError.clear();
    rebuildPreview();
    return true;
}

bool FontManagerCore::selectFont(const std::string& id) {
    if (!findFont(id)) {
        m_lastError = "Font is not available.";
        return false;
    }
    m_selectedFontId = id;
    m_mode = ToolMode::Manage;
    m_lastError.clear();
    rebuildPreview();
    return true;
}

bool FontManagerCore::selectImport(const std::string& id) {
    if (id.empty()) {
        m_selectedImportId.clear();
        rebuildPreview();
        return true;
    }
    if (!findImport(id)) {
        m_lastError = "Imported TTF is not available.";
        return false;
    }
    m_selectedImportId = id;
    m_mode = ToolMode::New;
    m_lastError.clear();
    rebuildPreview();
    return true;
}

bool FontManagerCore::setPreviewText(const std::string& text) {
    m_previewText = text;
    m_lastError.clear();
    rebuildPreview();
    return true;
}

bool FontManagerCore::importTtfFiles(std::vector<ImportedFont> imports) {
    bool importedAny = false;
    for (ImportedFont& imported : imports) {
        if (imported.ttfBytes.empty()) {
            continue;
        }
        bool duplicate = false;
        for (const ImportedFont& existing : m_imports) {
            if (!imported.sourceKey.empty() && existing.sourceKey == imported.sourceKey) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        imported.id = nextImportId(m_nextImportId++);
        if (imported.familyName.empty()) {
            imported.familyName = imported.fileName;
        }
        if (m_settings.atlasName == "ape_font") {
            m_settings.atlasName = sanitizeAtlasName(imported.familyName);
        }
        m_selectedImportId = imported.id;
        m_imports.push_back(std::move(imported));
        importedAny = true;
    }

    if (!importedAny) {
        m_lastError = "No supported TTF file was imported.";
        return false;
    }

    m_mode = ToolMode::New;
    m_lastError.clear();
    rebuildPreview();
    return true;
}

bool FontManagerCore::updateSettings(const FontGenerationSettings& settings) {
    m_settings = settings;
    m_settings.atlasName = sanitizeAtlasName(m_settings.atlasName);
    m_settings.sizePx = std::clamp(m_settings.sizePx, 1, 256);
    m_settings.heightPercent = std::clamp(m_settings.heightPercent, 10, 400);
    m_settings.outlineThickness = std::clamp(m_settings.outlineThickness, 0, 32);
    m_settings.paddingUp = std::clamp(m_settings.paddingUp, 0, 128);
    m_settings.paddingRight = std::clamp(m_settings.paddingRight, 0, 128);
    m_settings.paddingDown = std::clamp(m_settings.paddingDown, 0, 128);
    m_settings.paddingLeft = std::clamp(m_settings.paddingLeft, 0, 128);
    m_settings.spacingHorizontal = std::clamp(m_settings.spacingHorizontal, 0, 128);
    m_settings.spacingVertical = std::clamp(m_settings.spacingVertical, 0, 128);
    m_settings.textureWidth = std::clamp(m_settings.textureWidth, 64, 4096);
    m_settings.textureHeight = std::clamp(m_settings.textureHeight, 64, 8192);
    m_settings.textColors = sanitizeTextColors(std::move(m_settings.textColors));
    m_lastError.clear();
    rebuildPreview();
    return true;
}

ExportResult FontManagerCore::exportSelectedImport() {
    if (!m_dryadAtlas) {
        return {false, false, "DryadAtlas bridge is not available."};
    }
    const ImportedFont* imported = findImport(m_selectedImportId);
    if (!imported) {
        return {false, false, "No imported TTF is selected."};
    }

    GeneratedFontPackage package;
    std::string error;
    if (!m_dryadAtlas->generateFont(*imported, m_settings, &package, &error)) {
        return {false, false, error.empty() ? "Failed to generate HOI4 font files." : error};
    }

    const std::vector<std::string> overwriteFiles = collectOverwriteFiles(package);
    if (!overwriteFiles.empty()) {
        m_pendingExport.package = std::move(package);
        m_pendingExport.overwriteFiles = overwriteFiles;
        m_pendingExport.active = true;
        return {false, true, ""};
    }

    if (!writePackage(package)) {
        return {false, false, m_lastError};
    }
    return {true, false, ""};
}

bool FontManagerCore::confirmPendingOverwrite() {
    if (!m_pendingExport.active) {
        m_lastError = "No pending font export requires confirmation.";
        return false;
    }
    const GeneratedFontPackage package = std::move(m_pendingExport.package);
    m_pendingExport = {};
    return writePackage(package);
}

void FontManagerCore::cancelPendingOverwrite() {
    m_pendingExport = {};
}

bool FontManagerCore::handleAction(const std::string& actionType, const std::map<std::string, std::string>& params) {
    const std::string action = normalizeAction(actionType);
    if (action == "set_mode_manage") {
        return setMode(ToolMode::Manage);
    }
    if (action == "set_mode_new") {
        return setMode(ToolMode::New);
    }
    if (action == "select_font") {
        const auto it = params.find("targetId");
        return selectFont(it != params.end() ? it->second : "");
    }
    if (action == "select_import") {
        const auto it = params.find("targetId");
        return selectImport(it != params.end() ? it->second : "");
    }
    if (action == "set_preview_text") {
        const auto it = params.find("text");
        return setPreviewText(it != params.end() ? it->second : "");
    }
    if (action == "refresh") {
        return refreshFonts();
    }

    m_lastError = "Unsupported Font Manager action.";
    return false;
}

Snapshot FontManagerCore::buildSnapshot() const {
    Snapshot snapshot;
    snapshot.mode = m_mode;
    snapshot.fonts = m_fonts;
    snapshot.imports = m_imports;
    snapshot.selectedFontId = m_selectedFontId;
    snapshot.selectedImportId = m_selectedImportId;
    snapshot.previewText = m_previewText;
    snapshot.previewBase64 = m_previewBase64;
    snapshot.previewWidth = m_previewWidth;
    snapshot.previewHeight = m_previewHeight;
    snapshot.settings = m_settings;
    snapshot.canExport = m_mode == ToolMode::New && !m_selectedImportId.empty();
    snapshot.pendingOverwrite = m_pendingExport.active;
    snapshot.pendingOverwriteFiles = m_pendingExport.overwriteFiles;
    snapshot.statusText = m_statusText;
    snapshot.lastError = m_lastError;
    return snapshot;
}

ExistingFont* FontManagerCore::findFont(const std::string& id) {
    auto it = std::find_if(m_fonts.begin(), m_fonts.end(), [&id](const ExistingFont& font) { return font.id == id; });
    return it == m_fonts.end() ? nullptr : &(*it);
}

const ExistingFont* FontManagerCore::findFont(const std::string& id) const {
    auto it = std::find_if(m_fonts.begin(), m_fonts.end(), [&id](const ExistingFont& font) { return font.id == id; });
    return it == m_fonts.end() ? nullptr : &(*it);
}

ImportedFont* FontManagerCore::findImport(const std::string& id) {
    auto it = std::find_if(m_imports.begin(), m_imports.end(), [&id](const ImportedFont& font) { return font.id == id; });
    return it == m_imports.end() ? nullptr : &(*it);
}

const ImportedFont* FontManagerCore::findImport(const std::string& id) const {
    auto it = std::find_if(m_imports.begin(), m_imports.end(), [&id](const ImportedFont& font) { return font.id == id; });
    return it == m_imports.end() ? nullptr : &(*it);
}

bool FontManagerCore::rebuildPreview() {
    m_previewBase64.clear();
    m_previewWidth = 0;
    m_previewHeight = 0;
    if (!m_dryadAtlas) {
        return true;
    }
    std::string error;
    if (m_mode == ToolMode::Manage) {
        const ExistingFont* font = findFont(m_selectedFontId);
        if (!font) {
            return true;
        }
        if (!m_dryadAtlas->renderExistingFont(*font, m_previewText, &m_previewBase64, &m_previewWidth, &m_previewHeight, &error)) {
            m_lastError = error;
            return false;
        }
        return true;
    }

    const ImportedFont* imported = findImport(m_selectedImportId);
    if (!imported) {
        return true;
    }
    if (!m_dryadAtlas->renderImportedTtfPreview(*imported, m_settings, m_globalTextColors, m_previewText, &m_previewBase64, &m_previewWidth, &m_previewHeight, &error)) {
        m_lastError = error;
        return false;
    }
    return true;
}

bool FontManagerCore::writePackage(const GeneratedFontPackage& package) {
    if (!m_fileSystem) {
        m_lastError = "File bridge is not available.";
        return false;
    }
    const FileWriteResult fontDir = m_fileSystem->ensureModDirectory("gfx/fonts");
    if (!fontDir.success) {
        m_lastError = fontDir.errorMessage;
        return false;
    }
    const std::string outputName = package.outputName.empty() ? package.atlasName : package.outputName;
    const std::string registrationName = package.registrationName.empty() ? package.atlasName : package.registrationName;
    const FileWriteResult namedFontDir = m_fileSystem->ensureModDirectory("gfx/fonts/" + outputName);
    if (!namedFontDir.success) {
        m_lastError = namedFontDir.errorMessage;
        return false;
    }
    const FileWriteResult interfaceDir = m_fileSystem->ensureModDirectory("interface/fonts");
    if (!interfaceDir.success) {
        m_lastError = interfaceDir.errorMessage;
        return false;
    }
    for (const ExportFile& file : package.files) {
        ExportFile outputFile = file;
        if (outputFile.relativePath.rfind("interface/fonts/", 0) == 0) {
            const FileReadResult existing = m_fileSystem->readModFile(outputFile.relativePath);
            if (existing.success && !existing.content.empty()) {
                const std::string existingText(
                    reinterpret_cast<const char*>(existing.content.data()),
                    existing.content.size()
                );
                const std::string newText(
                    reinterpret_cast<const char*>(outputFile.content.data()),
                    outputFile.content.size()
                );
                if (existingText.find("name = \"" + registrationName + "\"") == std::string::npos) {
                    const std::size_t insertPos = existingText.rfind('}');
                    const std::size_t blockStart = newText.find("    bitmapfont");
                    const std::size_t blockEnd = newText.rfind('}');
                    if (insertPos != std::string::npos && blockStart != std::string::npos && blockEnd != std::string::npos && blockEnd > blockStart) {
                        std::string mergedText = existingText.substr(0, insertPos);
                        if (!mergedText.empty() && mergedText.back() != '\n') {
                            mergedText.push_back('\n');
                        }
                        mergedText += newText.substr(blockStart, blockEnd - blockStart);
                        mergedText += "}\n";
                        outputFile.content.assign(mergedText.begin(), mergedText.end());
                    }
                } else {
                    continue;
                }
            }
        }
        const FileWriteResult result = m_fileSystem->writeModFile(outputFile.relativePath, outputFile.content);
        if (!result.success) {
            m_lastError = result.errorMessage;
            return false;
        }
    }
    m_statusText = "Font export complete.";
    m_lastError.clear();
    refreshFonts();
    return true;
}

std::vector<std::string> FontManagerCore::collectOverwriteFiles(const GeneratedFontPackage& package) const {
    std::vector<std::string> files;
    if (!m_fileSystem) {
        return files;
    }
    for (const ExportFile& file : package.files) {
        if (file.relativePath.rfind("interface/fonts/", 0) == 0) {
            continue;
        }
        const FileReadResult result = m_fileSystem->readModFile(file.relativePath);
        if (result.success) {
            files.push_back(file.relativePath);
        }
    }
    return files;
}

std::string FontManagerCore::nextImportId(int value) {
    return "font_import_" + std::to_string(value);
}

std::string FontManagerCore::normalizeAction(std::string value) {
    value = trim(std::move(value));
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
    );
    return value;
}

std::string FontManagerCore::trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string FontManagerCore::sanitizeAtlasName(std::string value) {
    value = trim(std::move(value));
    std::string result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::iscntrl(uch) != 0 || ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*') {
            result.push_back('_');
        } else {
            result.push_back(ch);
        }
    }
    while (!result.empty() && (std::isspace(static_cast<unsigned char>(result.back())) != 0 || result.back() == '.')) {
        result.pop_back();
    }
    return result.empty() ? "ape_font" : result;
}

} // namespace FontManager
