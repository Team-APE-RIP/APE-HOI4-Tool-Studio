//-------------------------------------------------------------------------------------
// FontFileSystem.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FONTMANAGER_FONTFILESYSTEM_H
#define FONTMANAGER_FONTFILESYSTEM_H

#include "FontTypes.h"

#include <string>
#include <vector>

namespace FontManager {

struct ExistingFontListResult {
    bool success = false;
    std::vector<ExistingFont> fonts;
    std::vector<FontTextColor> globalTextColors;
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

class IFontFileSystem {
public:
    virtual ~IFontFileSystem() = default;

    virtual ExistingFontListResult listExistingFonts() const = 0;
    virtual FileReadResult readEffectiveFile(const std::string& logicalPath) const = 0;
    virtual FileReadResult readModFile(const std::string& logicalPath) const = 0;
    virtual FileWriteResult ensureModDirectory(const std::string& logicalPath) const = 0;
    virtual FileWriteResult writeModFile(const std::string& logicalPath, const std::vector<std::uint8_t>& content) const = 0;
};

class IDryadAtlasService {
public:
    virtual ~IDryadAtlasService() = default;

    virtual bool readTtfInfo(const std::vector<std::uint8_t>& ttfBytes,
                             std::string* outFamilyName,
                             int* outGlyphCount,
                             std::string* outError) const = 0;
    virtual bool generateFont(const ImportedFont& imported,
                              const FontGenerationSettings& settings,
                              GeneratedFontPackage* outPackage,
                              std::string* outError) const = 0;
    virtual bool renderExistingFont(const ExistingFont& font,
                                    const std::string& previewText,
                                    std::string* outPngBase64,
                                    int* outWidth,
                                    int* outHeight,
                                    std::string* outError) const = 0;
    virtual bool renderGeneratedFontPreview(const ImportedFont& imported,
                                            const FontGenerationSettings& settings,
                                            const std::string& previewText,
                                            std::string* outPngBase64,
                                            int* outWidth,
                                            int* outHeight,
                                            std::string* outError) const = 0;
    virtual bool renderImportedTtfPreview(const ImportedFont& imported,
                                          const FontGenerationSettings& settings,
                                          const std::vector<FontTextColor>& globalTextColors,
                                          const std::string& previewText,
                                          std::string* outPngBase64,
                                          int* outWidth,
                                          int* outHeight,
                                          std::string* outError) const = 0;
};

} // namespace FontManager

#endif // FONTMANAGER_FONTFILESYSTEM_H
