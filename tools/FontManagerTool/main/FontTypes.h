//-------------------------------------------------------------------------------------
// FontTypes.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FONTMANAGER_FONTTYPES_H
#define FONTMANAGER_FONTTYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace FontManager {

enum class ToolMode {
    Manage,
    New
};

struct FontRgbColor {
    int red = 255;
    int green = 255;
    int blue = 255;
};

struct FontTextColor {
    std::string code;
    FontRgbColor color;
};

struct ExistingFont {
    std::string id;
    std::string name;
    std::string displayName;
    FontRgbColor baseColor;
    std::vector<std::string> fontFileBases;
    std::vector<std::string> languages;
    std::vector<FontTextColor> textColors;
};

struct ImportedFont {
    std::string id;
    std::string sourceKey;
    std::string fileName;
    std::string familyName;
    std::vector<std::uint8_t> ttfBytes;
    int glyphCount = 0;
};

struct FontGenerationSettings {
    std::string atlasName = "ape_font";
    int sizePx = 16;
    int heightPercent = 100;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool smoothing = true;
    bool clearType = true;
    bool forceOffsetsZero = true;
    int paddingUp = 0;
    int paddingRight = 0;
    int paddingDown = 0;
    int paddingLeft = 0;
    int spacingHorizontal = 0;
    int spacingVertical = 0;
    int outlineThickness = 0;
    int textureWidth = 2048;
    int textureHeight = 4096;
    std::vector<FontTextColor> textColors;
};

struct ExportFile {
    std::string relativePath;
    std::vector<std::uint8_t> content;
};

struct GeneratedFontPackage {
    std::string atlasName;
    std::string outputName;
    std::string registrationName;
    std::string fontName;
    int glyphCount = 0;
    int pageCount = 0;
    std::vector<ExportFile> files;
};

struct ExportResult {
    bool success = false;
    bool pendingOverwrite = false;
    std::string errorMessage;
};

struct Snapshot {
    ToolMode mode = ToolMode::Manage;
    std::vector<ExistingFont> fonts;
    std::vector<ImportedFont> imports;
    std::string selectedFontId;
    std::string selectedImportId;
    std::string previewText;
    std::string previewBase64;
    int previewWidth = 0;
    int previewHeight = 0;
    FontGenerationSettings settings;
    bool canExport = false;
    bool pendingOverwrite = false;
    std::vector<std::string> pendingOverwriteFiles;
    std::string statusText;
    std::string lastError;
};

} // namespace FontManager

#endif // FONTMANAGER_FONTTYPES_H
