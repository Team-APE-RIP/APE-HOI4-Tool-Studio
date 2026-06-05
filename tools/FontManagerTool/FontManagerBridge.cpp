//-------------------------------------------------------------------------------------
// FontManagerBridge.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "FontManagerBridge.h"

#include "../../src/ToolRuntimeContext.h"

#include <QBuffer>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace FontManagerBridge {

namespace {
using FontManager::ExistingFont;
using FontManager::ExistingFontListResult;
using FontManager::ExportFile;
using FontManager::FileReadResult;
using FontManager::FileWriteResult;
using FontManager::FontGenerationSettings;
using FontManager::GeneratedFontPackage;
using FontManager::IDryadAtlasService;
using FontManager::IFontFileSystem;
using FontManager::ImportedFont;
using FontManager::Snapshot;
using FontManager::ToolMode;

QString localizedFallback(const QString& key) {
    static QMap<QString, QString> fallbacks;
    if (fallbacks.isEmpty()) {
        fallbacks.insert(QStringLiteral("Name"), QStringLiteral("Font Manager"));
        fallbacks.insert(QStringLiteral("TabManage"), QStringLiteral("Manage"));
        fallbacks.insert(QStringLiteral("TabNew"), QStringLiteral("Create"));
        fallbacks.insert(QStringLiteral("Fonts"), QStringLiteral("Fonts"));
        fallbacks.insert(QStringLiteral("Files"), QStringLiteral("Files"));
        fallbacks.insert(QStringLiteral("Glyphs"), QStringLiteral("Glyphs"));
        fallbacks.insert(QStringLiteral("Pages"), QStringLiteral("Pages"));
        fallbacks.insert(QStringLiteral("ImportTtf"), QStringLiteral("Import"));
        fallbacks.insert(QStringLiteral("Export"), QStringLiteral("Export"));
        fallbacks.insert(QStringLiteral("Refresh"), QStringLiteral("Refresh"));
        fallbacks.insert(QStringLiteral("PreviewText"), QStringLiteral("Preview Text"));
        fallbacks.insert(QStringLiteral("FontColors"), QStringLiteral("Colors"));
        fallbacks.insert(QStringLiteral("AddFontColor"), QStringLiteral("Add color"));
        fallbacks.insert(QStringLiteral("RemoveFontColor"), QStringLiteral("Remove color"));
        fallbacks.insert(QStringLiteral("FontName"), QStringLiteral("Font Name"));
        fallbacks.insert(QStringLiteral("SizePx"), QStringLiteral("Size"));
        fallbacks.insert(QStringLiteral("HeightPercent"), QStringLiteral("Height %"));
        fallbacks.insert(QStringLiteral("Bold"), QStringLiteral("Bold"));
        fallbacks.insert(QStringLiteral("Italic"), QStringLiteral("Italic"));
        fallbacks.insert(QStringLiteral("Underline"), QStringLiteral("Line"));
        fallbacks.insert(QStringLiteral("Smoothing"), QStringLiteral("Smooth"));
        fallbacks.insert(QStringLiteral("ClearType"), QStringLiteral("Subpx"));
        fallbacks.insert(QStringLiteral("ForceOffsetsZero"), QStringLiteral("Zero"));
        fallbacks.insert(QStringLiteral("BoldTip"), QStringLiteral("Rasterize glyphs with a bold face."));
        fallbacks.insert(QStringLiteral("ItalicTip"), QStringLiteral("Rasterize glyphs with an italic face."));
        fallbacks.insert(QStringLiteral("UnderlineTip"), QStringLiteral("Draw underline strokes into exported glyphs."));
        fallbacks.insert(QStringLiteral("SmoothingTip"), QStringLiteral("Use antialiasing while rasterizing the source font."));
        fallbacks.insert(QStringLiteral("ClearTypeTip"), QStringLiteral("Use subpixel rendering when supported by the rasterizer."));
        fallbacks.insert(QStringLiteral("ForceOffsetsZeroTip"), QStringLiteral("Force exported glyph x/y offsets to zero for HOI4 compatibility."));
        fallbacks.insert(QStringLiteral("Padding"), QStringLiteral("Padding"));
        fallbacks.insert(QStringLiteral("PaddingUpTip"), QStringLiteral("Top padding in pixels."));
        fallbacks.insert(QStringLiteral("PaddingRightTip"), QStringLiteral("Right padding in pixels."));
        fallbacks.insert(QStringLiteral("PaddingDownTip"), QStringLiteral("Bottom padding in pixels."));
        fallbacks.insert(QStringLiteral("PaddingLeftTip"), QStringLiteral("Left padding in pixels."));
        fallbacks.insert(QStringLiteral("Spacing"), QStringLiteral("Spacing"));
        fallbacks.insert(QStringLiteral("SpacingHorizontalTip"), QStringLiteral("Extra horizontal spacing between glyphs."));
        fallbacks.insert(QStringLiteral("SpacingVerticalTip"), QStringLiteral("Extra vertical spacing between glyph rows."));
        fallbacks.insert(QStringLiteral("Outline"), QStringLiteral("Outline"));
        fallbacks.insert(QStringLiteral("Texture"), QStringLiteral("Texture"));
        fallbacks.insert(QStringLiteral("NoPreview"), QStringLiteral("No Preview"));
        fallbacks.insert(QStringLiteral("ConfirmOverwriteTitle"), QStringLiteral("Confirm Overwrite"));
        fallbacks.insert(QStringLiteral("ConfirmOverwrite"), QStringLiteral("The following files already exist. Do you want to overwrite them?\n\n%1"));
        fallbacks.insert(QStringLiteral("Ready"), QStringLiteral("Ready"));
    }
    return fallbacks.value(key, key);
}

QString localizedString(const WorkerSession* session, const QString& key) {
    return session ? session->localizedStrings.value(key, localizedFallback(key)) : localizedFallback(key);
}

std::string toStdString(const QString& value) {
    return value.toUtf8().toStdString();
}

QString fromStdString(const std::string& value) {
    return QString::fromUtf8(value.c_str());
}

QString normalizeRuntimePath(QString value) {
    value.replace('\\', '/');
    value = QDir::cleanPath(value.trimmed());
    return value == QStringLiteral(".") ? QString() : value;
}

ToolRuntimeContext::PluginInvokeResponse invokePluginOperation(
    const QString& pluginName,
    const QString& operation,
    ToolRuntimeContext::PluginPayloadContentType contentType,
    const QByteArray& payload
) {
    ToolRuntimeContext::PluginInvokeRequest request;
    request.pluginName = pluginName;
    request.operation = operation;
    request.contentType = contentType;
    request.payload = payload;
    return ToolRuntimeContext::instance().invokePlugin(request);
}

QByteArray bytesToBase64(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) {
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size())).toBase64();
}

std::vector<std::uint8_t> base64ToBytes(const QString& value) {
    const QByteArray decoded = QByteArray::fromBase64(value.toLatin1());
    const auto* begin = reinterpret_cast<const std::uint8_t*>(decoded.constData());
    return std::vector<std::uint8_t>(begin, begin + decoded.size());
}

QString stableSha1Hex(const QString& value) {
    return QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha1).toHex());
}

std::string sanitizeOutputName(std::string value) {
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
    while (!result.empty() && std::isspace(static_cast<unsigned char>(result.front())) != 0) {
        result.erase(result.begin());
    }
    while (!result.empty() && (std::isspace(static_cast<unsigned char>(result.back())) != 0 || result.back() == '.')) {
        result.pop_back();
    }
    return result.empty() ? "ape_font" : result;
}

std::string registrationNameForSettings(const FontGenerationSettings& settings) {
    return sanitizeOutputName(settings.atlasName) + std::to_string(std::max(1, settings.sizePx));
}

std::vector<std::string> fntPageFiles(const std::string& fntText) {
    std::vector<std::string> pages;
    std::istringstream stream(fntText);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("page ", 0) != 0) {
            continue;
        }
        const std::size_t filePos = line.find("file=");
        if (filePos == std::string::npos) {
            continue;
        }
        std::size_t valueStart = filePos + 5;
        while (valueStart < line.size() && std::isspace(static_cast<unsigned char>(line[valueStart])) != 0) {
            ++valueStart;
        }
        std::string fileName;
        if (valueStart < line.size() && line[valueStart] == '"') {
            ++valueStart;
            const std::size_t valueEnd = line.find('"', valueStart);
            fileName = line.substr(valueStart, valueEnd == std::string::npos ? std::string::npos : valueEnd - valueStart);
        } else {
            std::size_t valueEnd = valueStart;
            while (valueEnd < line.size() && std::isspace(static_cast<unsigned char>(line[valueEnd])) == 0) {
                ++valueEnd;
            }
            fileName = line.substr(valueStart, valueEnd - valueStart);
        }
        if (!fileName.empty()) {
            pages.push_back(fileName);
        }
    }
    if (pages.empty()) {
        pages.push_back({});
    }
    return pages;
}

std::string replaceExtension(std::string value, const std::string& extension) {
    std::replace(value.begin(), value.end(), '\\', '/');
    const std::size_t slashPos = value.find_last_of('/');
    const std::size_t dotPos = value.find_last_of('.');
    if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
        value.resize(dotPos);
    }
    return value + extension;
}

std::vector<std::string> splitFontFiles(const char* value) {
    std::vector<std::string> parts;
    if (!value) {
        return parts;
    }
    QStringList items = QString::fromUtf8(value).split(';', Qt::SkipEmptyParts);
    for (const QString& item : items) {
        parts.push_back(toStdString(item.trimmed()));
    }
    return parts;
}

std::vector<std::string> splitSemicolonList(const QString& value) {
    std::vector<std::string> parts;
    const QStringList items = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    parts.reserve(static_cast<std::size_t>(items.size()));
    for (const QString& item : items) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty()) {
            parts.push_back(toStdString(trimmed));
        }
    }
    return parts;
}

FontManager::FontRgbColor colorFromPackedString(const QString& value, const FontManager::FontRgbColor& fallback) {
    QString text = value.trimmed();
    if ((text.startsWith(QLatin1Char('"')) && text.endsWith(QLatin1Char('"')))
        || (text.startsWith(QLatin1Char('\'')) && text.endsWith(QLatin1Char('\'')))) {
        text = text.mid(1, text.size() - 2).trimmed();
    }
    if (text.isEmpty()) {
        return fallback;
    }

    int base = 10;
    QByteArray encoded;
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        base = 16;
        encoded = text.mid(2).toLatin1();
    } else {
        encoded = text.toLatin1();
    }

    char* endPtr = nullptr;
    const unsigned long long packed = std::strtoull(encoded.constData(), &endPtr, base);
    if (endPtr == encoded.constData()) {
        return fallback;
    }

    FontManager::FontRgbColor color;
    color.red = static_cast<int>((packed >> 16) & 0xFFu);
    color.green = static_cast<int>((packed >> 8) & 0xFFu);
    color.blue = static_cast<int>(packed & 0xFFu);
    return color;
}

std::vector<FontManager::FontTextColor> textColorsFromString(const QString& value) {
    std::vector<FontManager::FontTextColor> colors;
    const QStringList entries = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    colors.reserve(static_cast<std::size_t>(entries.size()));
    for (const QString& entry : entries) {
        const int equalsPos = entry.indexOf(QLatin1Char('='));
        if (equalsPos <= 0) {
            continue;
        }
        const QString code = entry.left(equalsPos).trimmed();
        const QStringList components = entry.mid(equalsPos + 1).split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (code.isEmpty() || components.size() < 3) {
            continue;
        }

        bool okRed = false;
        bool okGreen = false;
        bool okBlue = false;
        FontManager::FontTextColor color;
        color.code = toStdString(code);
        color.color.red = std::clamp(components.at(0).trimmed().toInt(&okRed), 0, 255);
        color.color.green = std::clamp(components.at(1).trimmed().toInt(&okGreen), 0, 255);
        color.color.blue = std::clamp(components.at(2).trimmed().toInt(&okBlue), 0, 255);
        if (okRed && okGreen && okBlue) {
            colors.push_back(std::move(color));
        }
    }
    return colors;
}

QJsonArray colorToJson(const FontManager::FontRgbColor& color) {
    QJsonArray values;
    values.append(std::clamp(color.red, 0, 255));
    values.append(std::clamp(color.green, 0, 255));
    values.append(std::clamp(color.blue, 0, 255));
    return values;
}

QJsonObject textColorsToJson(const std::vector<FontManager::FontTextColor>& textColors) {
    QJsonObject object;
    for (const FontManager::FontTextColor& textColor : textColors) {
        if (!textColor.code.empty()) {
            object[QString::fromUtf8(textColor.code.c_str())] = colorToJson(textColor.color);
        }
    }
    return object;
}

QJsonObject mergedTextColorsToJson(const std::vector<FontManager::FontTextColor>& globalTextColors,
                                   const std::vector<FontManager::FontTextColor>& localTextColors) {
    QJsonObject object = textColorsToJson(globalTextColors);
    for (const FontManager::FontTextColor& textColor : localTextColors) {
        if (!textColor.code.empty()) {
            object[QString::fromUtf8(textColor.code.c_str())] = colorToJson(textColor.color);
        }
    }
    return object;
}

QJsonArray textColorsToJsonArray(const std::vector<FontManager::FontTextColor>& textColors) {
    QJsonArray array;
    for (const FontManager::FontTextColor& textColor : textColors) {
        if (textColor.code.empty()) {
            continue;
        }
        QJsonObject object;
        object[QStringLiteral("code")] = QString::fromUtf8(textColor.code.c_str());
        object[QStringLiteral("red")] = std::clamp(textColor.color.red, 0, 255);
        object[QStringLiteral("green")] = std::clamp(textColor.color.green, 0, 255);
        object[QStringLiteral("blue")] = std::clamp(textColor.color.blue, 0, 255);
        array.append(object);
    }
    return array;
}

std::vector<FontManager::FontTextColor> textColorsFromJsonArray(const QJsonValue& value,
                                                                const std::vector<FontManager::FontTextColor>& fallback) {
    if (!value.isArray()) {
        return fallback;
    }

    std::vector<FontManager::FontTextColor> colors;
    const QJsonArray array = value.toArray();
    colors.reserve(static_cast<std::size_t>(array.size()));
    for (const QJsonValue& rowValue : array) {
        const QJsonObject row = rowValue.toObject();
        const QString code = row.value(QStringLiteral("code")).toString().trimmed();
        if (code.isEmpty()) {
            continue;
        }

        FontManager::FontTextColor textColor;
        textColor.code = toStdString(code.left(1));
        textColor.color.red = std::clamp(row.value(QStringLiteral("red")).toInt(0), 0, 255);
        textColor.color.green = std::clamp(row.value(QStringLiteral("green")).toInt(255), 0, 255);
        textColor.color.blue = std::clamp(row.value(QStringLiteral("blue")).toInt(0), 0, 255);
        colors.push_back(std::move(textColor));
    }
    return colors;
}

QString localizedGameLanguageName(const WorkerSession* session, const QString& languageKey) {
    if (!session) {
        return languageKey;
    }
    return session->gameLanguageNames.value(languageKey, languageKey);
}

QString displayNameForFont(const WorkerSession* session,
                           const QString& name,
                           const std::vector<std::string>& languages) {
    if (languages.empty()) {
        return name;
    }

    QStringList localizedNames;
    for (const std::string& language : languages) {
        const QString languageKey = fromStdString(language).trimmed();
        if (!languageKey.isEmpty()) {
            localizedNames.append(localizedGameLanguageName(session, languageKey));
        }
    }
    localizedNames.removeDuplicates();
    if (localizedNames.isEmpty()) {
        return name;
    }
    return QStringLiteral("%1(%2)").arg(name, localizedNames.join(QStringLiteral(", ")));
}

class ToolRuntimeFontFileSystem final : public IFontFileSystem {
public:
    explicit ToolRuntimeFontFileSystem(const WorkerSession* session)
        : m_session(session) {}

    ExistingFontListResult listExistingFonts() const override {
        ExistingFontListResult result;
        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("APEHOI4Parser"),
            QStringLiteral("hoi4Parser.listFonts"),
            ToolRuntimeContext::PluginPayloadContentType::JsonUtf8,
            QByteArray()
        );
        if (!response.success) {
            result.success = false;
            result.errorMessage = toStdString(response.errorMessage.trimmed().isEmpty()
                ? QStringLiteral("APEHOI4Parser font ABI is not available.")
                : response.errorMessage);
            return result;
        }

        const QJsonDocument document = QJsonDocument::fromJson(response.payload);
        if (!document.isObject()) {
            result.success = false;
            result.errorMessage = "APEHOI4Parser returned an invalid font list.";
            return result;
        }

        result.globalTextColors = textColorsFromString(document.object().value(QStringLiteral("globalTextColors")).toString());
        const QJsonArray entries = document.object().value(QStringLiteral("entries")).toArray();
        std::set<std::string> seen;
        result.fonts.reserve(static_cast<std::size_t>(entries.size()));
        for (const QJsonValue& value : entries) {
            const QJsonObject entry = value.toObject();
            ExistingFont font;
            font.name = toStdString(entry.value(QStringLiteral("name")).toString());
            font.baseColor = colorFromPackedString(entry.value(QStringLiteral("color")).toString(), font.baseColor);
            const std::string path = toStdString(entry.value(QStringLiteral("path")).toString());
            if (!path.empty()) {
                font.fontFileBases.push_back(path);
            }
            const QByteArray fontFilesUtf8 = entry.value(QStringLiteral("fontFiles")).toString().toUtf8();
            std::vector<std::string> fontFiles = splitFontFiles(fontFilesUtf8.constData());
            font.fontFileBases.insert(font.fontFileBases.end(), fontFiles.begin(), fontFiles.end());
            font.languages = splitSemicolonList(entry.value(QStringLiteral("languages")).toString());
            font.textColors = textColorsFromString(entry.value(QStringLiteral("textColors")).toString());
            font.id = font.name;
            if (!font.languages.empty()) {
                font.id += "::";
                for (const std::string& language : font.languages) {
                    font.id += language;
                    font.id.push_back(';');
                }
            }
            font.displayName = toStdString(displayNameForFont(m_session, fromStdString(font.name), font.languages));
            if (font.id.empty() || (font.fontFileBases.empty() && font.languages.empty()) || seen.find(font.id) != seen.end()) {
                continue;
            }
            seen.insert(font.id);
            result.fonts.push_back(std::move(font));
        }
        result.success = true;
        return result;
    }

    FileReadResult readEffectiveFile(const std::string& logicalPath) const override {
        const ToolRuntimeContext::FileReadResult runtimeResult =
            ToolRuntimeContext::instance().readEffectiveFile(fromStdString(logicalPath));
        return fromRuntimeReadResult(runtimeResult);
    }

    FileReadResult readModFile(const std::string& logicalPath) const override {
        const ToolRuntimeContext::FileReadResult runtimeResult =
            ToolRuntimeContext::instance().readFile(ToolRuntimeContext::FileRoot::Mod, fromStdString(logicalPath));
        return fromRuntimeReadResult(runtimeResult);
    }

    FileWriteResult ensureModDirectory(const std::string& logicalPath) const override {
        const ToolRuntimeContext::FileWriteResult runtimeResult =
            ToolRuntimeContext::instance().ensureDirectory(ToolRuntimeContext::FileRoot::Mod, fromStdString(logicalPath));
        return {runtimeResult.success, toStdString(runtimeResult.errorMessage)};
    }

    FileWriteResult writeModFile(const std::string& logicalPath, const std::vector<std::uint8_t>& content) const override {
        const QByteArray bytes(reinterpret_cast<const char*>(content.data()), static_cast<int>(content.size()));
        const ToolRuntimeContext::FileWriteResult runtimeResult =
            ToolRuntimeContext::instance().writeFile(ToolRuntimeContext::FileRoot::Mod, fromStdString(logicalPath), bytes);
        return {runtimeResult.success, toStdString(runtimeResult.errorMessage)};
    }

private:
    const WorkerSession* m_session = nullptr;

    static FileReadResult fromRuntimeReadResult(const ToolRuntimeContext::FileReadResult& runtimeResult) {
        FileReadResult result;
        result.success = runtimeResult.success;
        result.errorMessage = toStdString(runtimeResult.errorMessage);
        if (runtimeResult.success) {
            const auto* begin = reinterpret_cast<const std::uint8_t*>(runtimeResult.content.constData());
            result.content.assign(begin, begin + runtimeResult.content.size());
        }
        return result;
    }
};

class DryadAtlasRuntimeService final : public IDryadAtlasService {
public:
    DryadAtlasRuntimeService(IFontFileSystem* fileSystem)
        : m_fileSystem(fileSystem) {}

    bool readTtfInfo(const std::vector<std::uint8_t>& ttfBytes,
                     std::string* outFamilyName,
                     int* outGlyphCount,
                     std::string* outError) const override {
        QJsonObject request;
        request[QStringLiteral("ttfBase64")] = QString::fromLatin1(bytesToBase64(ttfBytes));
        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("DryadAtlas"),
            QStringLiteral("dryadAtlas.getTtfInfo"),
            ToolRuntimeContext::PluginPayloadContentType::JsonUtf8,
            QJsonDocument(request).toJson(QJsonDocument::Compact)
        );
        if (!response.success) {
            setResponseError(outError, response, "Failed to read TTF info.");
            return false;
        }

        const QJsonDocument document = QJsonDocument::fromJson(response.payload);
        if (!document.isObject()) {
            setOutError(outError, "DryadAtlas returned invalid TTF info.");
            return false;
        }

        const QJsonObject object = document.object();
        if (outFamilyName) {
            *outFamilyName = toStdString(object.value(QStringLiteral("family")).toString());
        }
        if (outGlyphCount) {
            *outGlyphCount = object.value(QStringLiteral("glyphCount")).toInt(0);
        }
        return true;
    }

    bool generateFont(const ImportedFont& imported,
                      const FontGenerationSettings& settings,
                      GeneratedFontPackage* outPackage,
                      std::string* outError) const override {
        if (!outPackage) {
            setOutError(outError, "Output font package pointer is null.");
            return false;
        }

        QJsonObject request = settingsToJson(settings, imported.familyName);
        request[QStringLiteral("textColors")] = textColorsToJson(settings.textColors);
        request[QStringLiteral("ttfBase64")] = QString::fromLatin1(bytesToBase64(imported.ttfBytes));
        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("DryadAtlas"),
            QStringLiteral("dryadAtlas.generateFromTtf"),
            ToolRuntimeContext::PluginPayloadContentType::JsonUtf8,
            QJsonDocument(request).toJson(QJsonDocument::Compact)
        );
        if (!response.success) {
            setResponseError(outError, response, "Failed to generate font atlas.");
            return false;
        }

        const QJsonDocument document = QJsonDocument::fromJson(response.payload);
        if (!document.isObject()) {
            setOutError(outError, "DryadAtlas returned an invalid font atlas package.");
            return false;
        }

        const QJsonObject result = document.object();
        outPackage->atlasName = toStdString(result.value(QStringLiteral("atlasName")).toString());
        outPackage->outputName = toStdString(result.value(QStringLiteral("outputName")).toString());
        outPackage->registrationName = toStdString(result.value(QStringLiteral("registrationName")).toString());
        if (outPackage->outputName.empty()) {
            outPackage->outputName = outPackage->atlasName;
        }
        if (outPackage->registrationName.empty()) {
            outPackage->registrationName = outPackage->outputName + std::to_string(std::max(1, settings.sizePx));
        }
        outPackage->fontName = toStdString(result.value(QStringLiteral("fontName")).toString());
        outPackage->glyphCount = result.value(QStringLiteral("glyphCount")).toInt(0);
        outPackage->pageCount = result.value(QStringLiteral("pageCount")).toInt(0);
        outPackage->files.clear();

        auto appendFiles = [&](const QJsonArray& files) {
            for (const QJsonValue& value : files) {
                const QJsonObject object = value.toObject();
                ExportFile file;
                file.relativePath = toStdString(object.value(QStringLiteral("path")).toString());
                file.content = base64ToBytes(object.value(QStringLiteral("contentBase64")).toString());
                if (!file.relativePath.empty() && !file.content.empty()) {
                    outPackage->files.push_back(std::move(file));
                }
            }
        };
        appendFiles(result.value(QStringLiteral("fntFiles")).toArray());
        appendFiles(result.value(QStringLiteral("ddsFiles")).toArray());
        ExportFile gfxFile;
        gfxFile.relativePath = toStdString(result.value(QStringLiteral("gfxPath")).toString());
        gfxFile.content = base64ToBytes(result.value(QStringLiteral("gfxContentBase64")).toString());
        if (!gfxFile.relativePath.empty() && !gfxFile.content.empty()) {
            outPackage->files.push_back(std::move(gfxFile));
        }

        if (outPackage->files.empty()) {
            setOutError(outError, "DryadAtlas returned no exportable files.");
            return false;
        }
        return true;
    }

    bool renderExistingFont(const ExistingFont& font,
                            const std::string& previewText,
                            std::string* outPngBase64,
                            int* outWidth,
                            int* outHeight,
                            std::string* outError) const override {
        if (!m_fileSystem || !outPngBase64) {
            setOutError(outError, "DryadAtlas render arguments are invalid.");
            return false;
        }

        QJsonObject request;
        request[QStringLiteral("text")] = fromStdString(previewText);
        request[QStringLiteral("defaultColor")] = colorToJson(font.baseColor);
        request[QStringLiteral("textColors")] = textColorsToJson(font.textColors);
        QStringList cacheKeyParts;
        cacheKeyParts.append(fromStdString(font.id));
        for (const std::string& base : font.fontFileBases) {
            cacheKeyParts.append(fromStdString(normalizeFontBase(base)));
        }
        const QString cacheKey = cacheKeyParts.join(QLatin1Char('\n'));
        QJsonArray fontFiles;
        if (m_existingFontCacheKey == cacheKey && !m_existingFontFilesCache.isEmpty()) {
            fontFiles = m_existingFontFilesCache;
        } else {
            for (const std::string& base : font.fontFileBases) {
                const std::string normalizedBase = normalizeFontBase(base);
                const FileReadResult fnt = m_fileSystem->readEffectiveFile(normalizedBase + ".fnt");
                if (!fnt.success) {
                    continue;
                }
                const std::string fntText(reinterpret_cast<const char*>(fnt.content.data()), fnt.content.size());
                QJsonObject object;
                object[QStringLiteral("fntText")] = QString::fromUtf8(fntText.data(), static_cast<int>(fntText.size()));
                QJsonArray ddsPages;
                const QString baseDir = QFileInfo(fromStdString(normalizedBase)).path();
                const std::vector<std::string> pageFiles = fntPageFiles(fntText);
                for (const std::string& pageFile : pageFiles) {
                    QStringList candidates;
                    QString pageFileName;
                    if (pageFile.empty()) {
                        candidates.append(fromStdString(normalizedBase + ".dds"));
                        pageFileName = QFileInfo(candidates.constFirst()).fileName();
                    } else {
                        const QString pageFileText = fromStdString(pageFile);
                        const QString normalizedPageFile = normalizeRuntimePath(pageFileText);
                        pageFileName = QFileInfo(pageFileText).fileName();
                        if (normalizedPageFile.contains(QLatin1Char('/'))) {
                            candidates.append(normalizedPageFile);
                            candidates.append(fromStdString(replaceExtension(toStdString(normalizedPageFile), ".dds")));
                        }
                        candidates.append(normalizeRuntimePath(baseDir + QStringLiteral("/") + pageFileText));
                        candidates.append(fromStdString(replaceExtension(toStdString(normalizeRuntimePath(baseDir + QStringLiteral("/") + pageFileText)), ".dds")));
                        candidates.append(normalizeRuntimePath(baseDir + QStringLiteral("/") + pageFileName));
                        candidates.append(fromStdString(replaceExtension(toStdString(normalizeRuntimePath(baseDir + QStringLiteral("/") + pageFileName)), ".dds")));
                        candidates.append(fromStdString(normalizedBase + ".dds"));
                    }
                    FileReadResult dds;
                    std::set<QString> seenCandidates;
                    for (const QString& candidate : candidates) {
                        const QString normalizedCandidate = normalizeRuntimePath(candidate);
                        if (normalizedCandidate.isEmpty() || seenCandidates.find(normalizedCandidate) != seenCandidates.end()) {
                            continue;
                        }
                        seenCandidates.insert(normalizedCandidate);
                        dds = m_fileSystem->readEffectiveFile(toStdString(normalizedCandidate));
                        if (dds.success) {
                            break;
                        }
                    }
                    if (!dds.success) {
                        continue;
                    }
                    QJsonObject page;
                    page[QStringLiteral("file")] = pageFile.empty() ? pageFileName : fromStdString(pageFile);
                    page[QStringLiteral("base64")] = QString::fromLatin1(bytesToBase64(dds.content));
                    ddsPages.append(page);
                }
                if (ddsPages.isEmpty()) {
                    continue;
                }
                object[QStringLiteral("ddsPages")] = ddsPages;
                fontFiles.append(object);
            }
            m_existingFontCacheKey = cacheKey;
            m_existingFontFilesCache = fontFiles;
        }
        request[QStringLiteral("fontFiles")] = fontFiles;

        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("DryadAtlas"),
            QStringLiteral("dryadAtlas.renderText"),
            ToolRuntimeContext::PluginPayloadContentType::JsonUtf8,
            QJsonDocument(request).toJson(QJsonDocument::Compact)
        );
        if (!response.success) {
            setResponseError(outError, response, "Failed to render font preview.");
            return false;
        }

        const QJsonDocument document = QJsonDocument::fromJson(response.payload);
        if (!document.isObject()) {
            setOutError(outError, "DryadAtlas returned an invalid font preview.");
            return false;
        }

        const QJsonObject result = document.object();
        const QString pngBase64 = result.value(QStringLiteral("pngBase64")).toString();
        if (pngBase64.isEmpty()) {
            setOutError(outError, "DryadAtlas returned an empty font preview.");
            return false;
        }
        *outPngBase64 = toStdString(pngBase64);
        if (outWidth) {
            *outWidth = result.value(QStringLiteral("width")).toInt(0);
        }
        if (outHeight) {
            *outHeight = result.value(QStringLiteral("height")).toInt(0);
        }
        return true;
    }

    bool renderGeneratedFontPreview(const ImportedFont& imported,
                                    const FontGenerationSettings& settings,
                                    const std::string& previewText,
                                    std::string* outPngBase64,
                                    int* outWidth,
                                    int* outHeight,
                                    std::string* outError) const override {
        GeneratedFontPackage package;
        if (!generateFont(imported, settings, &package, outError)) {
            return false;
        }

        QJsonObject request;
        request[QStringLiteral("text")] = fromStdString(previewText);
        request[QStringLiteral("textColors")] = textColorsToJson(settings.textColors);
        QJsonArray fontFiles;

        std::map<std::string, std::vector<std::uint8_t>> ddsByFileName;
        for (const ExportFile& file : package.files) {
            if (file.relativePath.size() > 4 && file.relativePath.substr(file.relativePath.size() - 4) == ".dds") {
                const std::string fileName = QFileInfo(fromStdString(file.relativePath)).fileName().toStdString();
                ddsByFileName[fileName] = file.content;
                ddsByFileName[replaceExtension(fileName, ".dds")] = file.content;
            }
        }

        for (const ExportFile& file : package.files) {
            if (file.relativePath.size() <= 4 || file.relativePath.substr(file.relativePath.size() - 4) != ".fnt") {
                continue;
            }
            const std::string fntText(reinterpret_cast<const char*>(file.content.data()), file.content.size());
            QJsonObject object;
            object[QStringLiteral("fntText")] = QString::fromUtf8(fntText.data(), static_cast<int>(fntText.size()));
            QJsonArray ddsPages;
            for (const std::string& pageFile : fntPageFiles(fntText)) {
                const std::string pageFileName = QFileInfo(fromStdString(pageFile)).fileName().toStdString();
                const auto ddsIt = ddsByFileName.find(pageFileName);
                if (ddsIt == ddsByFileName.end()) {
                    continue;
                }
                QJsonObject page;
                page[QStringLiteral("file")] = QString::fromStdString(pageFile.empty() ? pageFileName : pageFile);
                page[QStringLiteral("base64")] = QString::fromLatin1(bytesToBase64(ddsIt->second));
                ddsPages.append(page);
            }
            if (!ddsPages.isEmpty()) {
                object[QStringLiteral("ddsPages")] = ddsPages;
                fontFiles.append(object);
            }
        }

        request[QStringLiteral("fontFiles")] = fontFiles;
        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("DryadAtlas"),
            QStringLiteral("dryadAtlas.renderText"),
            ToolRuntimeContext::PluginPayloadContentType::JsonUtf8,
            QJsonDocument(request).toJson(QJsonDocument::Compact)
        );
        if (!response.success) {
            setResponseError(outError, response, "Failed to render generated font preview.");
            return false;
        }

        const QJsonDocument document = QJsonDocument::fromJson(response.payload);
        if (!document.isObject()) {
            setOutError(outError, "DryadAtlas returned an invalid generated font preview.");
            return false;
        }

        const QJsonObject result = document.object();
        const QString pngBase64 = result.value(QStringLiteral("pngBase64")).toString();
        if (pngBase64.isEmpty()) {
            setOutError(outError, "DryadAtlas returned an empty generated font preview.");
            return false;
        }
        *outPngBase64 = toStdString(pngBase64);
        if (outWidth) {
            *outWidth = result.value(QStringLiteral("width")).toInt(0);
        }
        if (outHeight) {
            *outHeight = result.value(QStringLiteral("height")).toInt(0);
        }
        return true;
    }

    bool renderImportedTtfPreview(const ImportedFont& imported,
                                  const FontGenerationSettings& settings,
                                  const std::vector<FontManager::FontTextColor>& globalTextColors,
                                  const std::string& previewText,
                                  std::string* outPngBase64,
                                  int* outWidth,
                                  int* outHeight,
                                  std::string* outError) const override {
        if (!outPngBase64) {
            setOutError(outError, "DryadAtlas TTF preview arguments are invalid.");
            return false;
        }

        QJsonObject request = settingsToJson(settings, imported.familyName);
        request[QStringLiteral("textColors")] = mergedTextColorsToJson(globalTextColors, settings.textColors);
        request[QStringLiteral("text")] = fromStdString(previewText);
        request[QStringLiteral("ttfBase64")] = QString::fromLatin1(bytesToBase64(imported.ttfBytes));
        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("DryadAtlas"),
            QStringLiteral("dryadAtlas.renderTtfPreview"),
            ToolRuntimeContext::PluginPayloadContentType::JsonUtf8,
            QJsonDocument(request).toJson(QJsonDocument::Compact)
        );
        if (!response.success) {
            setResponseError(outError, response, "Failed to render imported TTF preview.");
            return false;
        }

        const QJsonDocument document = QJsonDocument::fromJson(response.payload);
        if (!document.isObject()) {
            setOutError(outError, "DryadAtlas returned an invalid imported TTF preview.");
            return false;
        }

        const QJsonObject result = document.object();
        const QString pngBase64 = result.value(QStringLiteral("pngBase64")).toString();
        if (pngBase64.isEmpty()) {
            setOutError(outError, "DryadAtlas returned an empty imported TTF preview.");
            return false;
        }
        *outPngBase64 = toStdString(pngBase64);
        if (outWidth) {
            *outWidth = result.value(QStringLiteral("width")).toInt(0);
        }
        if (outHeight) {
            *outHeight = result.value(QStringLiteral("height")).toInt(0);
        }
        return true;
    }

private:
    static QJsonObject settingsToJson(const FontGenerationSettings& settings, const std::string& familyName) {
        QJsonObject object;
        object[QStringLiteral("atlasName")] = fromStdString(settings.atlasName);
        object[QStringLiteral("outputName")] = fromStdString(sanitizeOutputName(settings.atlasName));
        object[QStringLiteral("registrationName")] = fromStdString(registrationNameForSettings(settings));
        object[QStringLiteral("fontName")] = fromStdString(familyName);
        object[QStringLiteral("sizePx")] = settings.sizePx;
        object[QStringLiteral("heightPercent")] = settings.heightPercent;
        object[QStringLiteral("bold")] = settings.bold;
        object[QStringLiteral("italic")] = settings.italic;
        object[QStringLiteral("underline")] = settings.underline;
        object[QStringLiteral("smoothing")] = settings.smoothing;
        object[QStringLiteral("clearType")] = settings.clearType;
        object[QStringLiteral("forceOffsetsZero")] = settings.forceOffsetsZero;
        object[QStringLiteral("paddingUp")] = settings.paddingUp;
        object[QStringLiteral("paddingRight")] = settings.paddingRight;
        object[QStringLiteral("paddingDown")] = settings.paddingDown;
        object[QStringLiteral("paddingLeft")] = settings.paddingLeft;
        object[QStringLiteral("spacingHorizontal")] = settings.spacingHorizontal;
        object[QStringLiteral("spacingVertical")] = settings.spacingVertical;
        object[QStringLiteral("outlineThickness")] = settings.outlineThickness;
        object[QStringLiteral("textureWidth")] = settings.textureWidth;
        object[QStringLiteral("textureHeight")] = settings.textureHeight;
        return object;
    }

    static std::string normalizeFontBase(std::string value) {
        std::replace(value.begin(), value.end(), '\\', '/');
        const std::string fnt = ".fnt";
        const std::string dds = ".dds";
        if (value.size() > fnt.size() && value.substr(value.size() - fnt.size()) == fnt) {
            value.resize(value.size() - fnt.size());
        }
        if (value.size() > dds.size() && value.substr(value.size() - dds.size()) == dds) {
            value.resize(value.size() - dds.size());
        }
        return value;
    }

    static void setOutError(std::string* outError, const std::string& message) {
        if (outError) {
            *outError = message;
        }
    }

    static void setResponseError(std::string* outError,
                                 const ToolRuntimeContext::PluginInvokeResponse& response,
                                 const char* fallback) {
        const QString error = response.errorMessage.trimmed();
        setOutError(outError, error.isEmpty() ? std::string(fallback ? fallback : "DryadAtlas operation failed.") : toStdString(error));
    }

    IFontFileSystem* m_fileSystem = nullptr;
    mutable QString m_existingFontCacheKey;
    mutable QJsonArray m_existingFontFilesCache;
};

FontGenerationSettings settingsFromJson(const QJsonObject& object, const FontGenerationSettings& fallback) {
    FontGenerationSettings settings = fallback;
    if (object.contains(QStringLiteral("atlasName"))) settings.atlasName = toStdString(object.value(QStringLiteral("atlasName")).toString());
    if (object.contains(QStringLiteral("sizePx"))) settings.sizePx = object.value(QStringLiteral("sizePx")).toInt(settings.sizePx);
    if (object.contains(QStringLiteral("heightPercent"))) settings.heightPercent = object.value(QStringLiteral("heightPercent")).toInt(settings.heightPercent);
    if (object.contains(QStringLiteral("bold"))) settings.bold = object.value(QStringLiteral("bold")).toBool(settings.bold);
    if (object.contains(QStringLiteral("italic"))) settings.italic = object.value(QStringLiteral("italic")).toBool(settings.italic);
    if (object.contains(QStringLiteral("underline"))) settings.underline = object.value(QStringLiteral("underline")).toBool(settings.underline);
    if (object.contains(QStringLiteral("smoothing"))) settings.smoothing = object.value(QStringLiteral("smoothing")).toBool(settings.smoothing);
    if (object.contains(QStringLiteral("clearType"))) settings.clearType = object.value(QStringLiteral("clearType")).toBool(settings.clearType);
    if (object.contains(QStringLiteral("forceOffsetsZero"))) settings.forceOffsetsZero = object.value(QStringLiteral("forceOffsetsZero")).toBool(settings.forceOffsetsZero);
    if (object.contains(QStringLiteral("paddingUp"))) settings.paddingUp = object.value(QStringLiteral("paddingUp")).toInt(settings.paddingUp);
    if (object.contains(QStringLiteral("paddingRight"))) settings.paddingRight = object.value(QStringLiteral("paddingRight")).toInt(settings.paddingRight);
    if (object.contains(QStringLiteral("paddingDown"))) settings.paddingDown = object.value(QStringLiteral("paddingDown")).toInt(settings.paddingDown);
    if (object.contains(QStringLiteral("paddingLeft"))) settings.paddingLeft = object.value(QStringLiteral("paddingLeft")).toInt(settings.paddingLeft);
    if (object.contains(QStringLiteral("spacingHorizontal"))) settings.spacingHorizontal = object.value(QStringLiteral("spacingHorizontal")).toInt(settings.spacingHorizontal);
    if (object.contains(QStringLiteral("spacingVertical"))) settings.spacingVertical = object.value(QStringLiteral("spacingVertical")).toInt(settings.spacingVertical);
    if (object.contains(QStringLiteral("outlineThickness"))) settings.outlineThickness = object.value(QStringLiteral("outlineThickness")).toInt(settings.outlineThickness);
    settings.textureWidth = object.value(QStringLiteral("textureWidth")).toInt(2048);
    settings.textureHeight = object.value(QStringLiteral("textureHeight")).toInt(4096);
    if (object.contains(QStringLiteral("textColors"))) settings.textColors = textColorsFromJsonArray(object.value(QStringLiteral("textColors")), settings.textColors);
    return settings;
}

QJsonObject buildColumn(const QString& key, const QString& title, int width = -1, bool stretch = false) {
    QJsonObject column;
    column[QStringLiteral("key")] = key;
    column[QStringLiteral("id")] = key;
    column[QStringLiteral("title")] = title;
    column[QStringLiteral("text")] = title;
    if (width > 0) column[QStringLiteral("width")] = width;
    if (stretch) column[QStringLiteral("stretch")] = true;
    return column;
}

QJsonObject buildListModel(WorkerSession* session, const Snapshot& state) {
    const bool createMode = state.mode == ToolMode::New;
    QJsonObject model;
    model[QStringLiteral("id")] = createMode ? QStringLiteral("import_list") : QStringLiteral("font_list");
    model[QStringLiteral("title")] = localizedString(session, createMode ? QStringLiteral("Files") : QStringLiteral("Fonts"));
    model[QStringLiteral("headerHidden")] = false;
    model[QStringLiteral("selectionMode")] = QStringLiteral("single");

    QJsonArray columns;
    columns.append(buildColumn(QStringLiteral("name"), localizedString(session, createMode ? QStringLiteral("Files") : QStringLiteral("Fonts")), -1, true));
    if (createMode) {
        columns.append(buildColumn(QStringLiteral("glyphCount"), localizedString(session, QStringLiteral("Glyphs")), 70));
    } else {
        columns.append(buildColumn(QStringLiteral("pages"), localizedString(session, QStringLiteral("Pages")), 60));
    }
    model[QStringLiteral("columns")] = columns;

    QJsonArray rows;
    if (createMode) {
        for (const ImportedFont& imported : state.imports) {
            QJsonObject row;
            row[QStringLiteral("id")] = fromStdString(imported.id);
            row[QStringLiteral("rowId")] = fromStdString(imported.id);
            QJsonObject values;
            values[QStringLiteral("name")] = fromStdString(imported.familyName.empty() ? imported.fileName : imported.familyName);
            values[QStringLiteral("displayName")] = values[QStringLiteral("name")];
            values[QStringLiteral("glyphCount")] = imported.glyphCount;
            row[QStringLiteral("values")] = values;
            QJsonArray cells;
            cells.append(QJsonObject{{QStringLiteral("value"), values[QStringLiteral("name")]}});
            cells.append(QJsonObject{{QStringLiteral("value"), imported.glyphCount}});
            row[QStringLiteral("cells")] = cells;
            QJsonObject rowState;
            rowState[QStringLiteral("selectAction")] = QStringLiteral("select_import");
            rowState[QStringLiteral("selected")] = imported.id == state.selectedImportId;
            row[QStringLiteral("state")] = rowState;
            rows.append(row);
        }
    } else {
        for (const ExistingFont& font : state.fonts) {
            QJsonObject row;
            row[QStringLiteral("id")] = fromStdString(font.id);
            row[QStringLiteral("rowId")] = fromStdString(font.id);
            QJsonObject values;
            values[QStringLiteral("name")] = fromStdString(font.name);
            values[QStringLiteral("displayName")] = fromStdString(font.displayName.empty() ? font.name : font.displayName);
            values[QStringLiteral("pages")] = static_cast<int>(font.fontFileBases.size());
            row[QStringLiteral("values")] = values;
            QJsonArray cells;
            cells.append(QJsonObject{{QStringLiteral("value"), values[QStringLiteral("displayName")]}});
            cells.append(QJsonObject{{QStringLiteral("value"), static_cast<int>(font.fontFileBases.size())}});
            row[QStringLiteral("cells")] = cells;
            QJsonObject rowState;
            rowState[QStringLiteral("selectAction")] = QStringLiteral("select_font");
            rowState[QStringLiteral("selected")] = font.id == state.selectedFontId;
            row[QStringLiteral("state")] = rowState;
            rows.append(row);
        }
    }
    model[QStringLiteral("rows")] = rows;
    QJsonArray selection;
    if (createMode && !state.selectedImportId.empty()) selection.append(fromStdString(state.selectedImportId));
    if (!createMode && !state.selectedFontId.empty()) selection.append(fromStdString(state.selectedFontId));
    model[QStringLiteral("selection")] = selection;
    return model;
}

QJsonObject settingsToJson(const FontGenerationSettings& settings) {
    QJsonObject object;
    object[QStringLiteral("atlasName")] = fromStdString(settings.atlasName);
    object[QStringLiteral("sizePx")] = settings.sizePx;
    object[QStringLiteral("heightPercent")] = settings.heightPercent;
    object[QStringLiteral("bold")] = settings.bold;
    object[QStringLiteral("italic")] = settings.italic;
    object[QStringLiteral("underline")] = settings.underline;
    object[QStringLiteral("smoothing")] = settings.smoothing;
    object[QStringLiteral("clearType")] = settings.clearType;
    object[QStringLiteral("forceOffsetsZero")] = settings.forceOffsetsZero;
    object[QStringLiteral("paddingUp")] = settings.paddingUp;
    object[QStringLiteral("paddingRight")] = settings.paddingRight;
    object[QStringLiteral("paddingDown")] = settings.paddingDown;
    object[QStringLiteral("paddingLeft")] = settings.paddingLeft;
    object[QStringLiteral("spacingHorizontal")] = settings.spacingHorizontal;
    object[QStringLiteral("spacingVertical")] = settings.spacingVertical;
    object[QStringLiteral("outlineThickness")] = settings.outlineThickness;
    object[QStringLiteral("textureWidth")] = settings.textureWidth;
    object[QStringLiteral("textureHeight")] = settings.textureHeight;
    object[QStringLiteral("textColors")] = textColorsToJsonArray(settings.textColors);
    return object;
}

QJsonObject buildViewState(WorkerSession* session, const Snapshot& state) {
    QJsonObject object;
    object[QStringLiteral("mode")] = state.mode == ToolMode::New ? QStringLiteral("new") : QStringLiteral("manage");
    object[QStringLiteral("selectedFontId")] = fromStdString(state.selectedFontId);
    object[QStringLiteral("selectedImportId")] = fromStdString(state.selectedImportId);
    object[QStringLiteral("previewText")] = fromStdString(state.previewText);
    object[QStringLiteral("previewBase64")] = fromStdString(state.previewBase64);
    object[QStringLiteral("previewWidth")] = state.previewWidth;
    object[QStringLiteral("previewHeight")] = state.previewHeight;
    object[QStringLiteral("settings")] = settingsToJson(state.settings);
    object[QStringLiteral("canExport")] = state.canExport;
    object[QStringLiteral("pendingOverwrite")] = state.pendingOverwrite;
    object[QStringLiteral("statusText")] = state.statusText.empty() ? localizedString(session, QStringLiteral("Ready")) : fromStdString(state.statusText);
    object[QStringLiteral("lastError")] = fromStdString(state.lastError);
    QJsonArray overwriteFiles;
    for (const std::string& path : state.pendingOverwriteFiles) overwriteFiles.append(fromStdString(path));
    object[QStringLiteral("pendingOverwriteFiles")] = overwriteFiles;
    return object;
}

QJsonObject buildSidebarState(WorkerSession* session, const Snapshot& state) {
    const bool createMode = state.mode == ToolMode::New;
    QJsonObject sidebar;
    sidebar[QStringLiteral("visible")] = true;
    sidebar[QStringLiteral("title")] = localizedString(session, createMode ? QStringLiteral("Files") : QStringLiteral("Fonts"));
    sidebar[QStringLiteral("activeMode")] = createMode ? QStringLiteral("import_list") : QStringLiteral("font_list");
    sidebar[QStringLiteral("modelId")] = createMode ? QStringLiteral("import_list") : QStringLiteral("font_list");
    sidebar[QStringLiteral("searchEnabled")] = true;
    sidebar[QStringLiteral("selectAllEnabled")] = false;
    QJsonArray modeOrder;
    modeOrder.append(QStringLiteral("__default__"));
    modeOrder.append(QStringLiteral("__search__"));
    sidebar[QStringLiteral("modeOrder")] = modeOrder;
    QJsonArray searchableColumns;
    searchableColumns.append(0);
    sidebar[QStringLiteral("searchableColumns")] = searchableColumns;
    QJsonArray labels;
    labels.append(localizedString(session, createMode ? QStringLiteral("Files") : QStringLiteral("Fonts")));
    sidebar[QStringLiteral("searchableColumnLabels")] = labels;
    return sidebar;
}

QJsonObject buildTopbarState(WorkerSession* session, const Snapshot& state) {
    QJsonObject topbar;
    topbar[QStringLiteral("visible")] = true;
    topbar[QStringLiteral("currentPageId")] = QStringLiteral("main");

    QJsonArray pageOrder;
    pageOrder.append(QStringLiteral("main"));
    topbar[QStringLiteral("pageOrder")] = pageOrder;

    QJsonArray functionOrder;
    functionOrder.append(QStringLiteral("main::set_mode_manage"));
    functionOrder.append(QStringLiteral("main::set_mode_new"));
    if (state.mode == ToolMode::New) {
        functionOrder.append(QStringLiteral("main::import_ttf"));
        functionOrder.append(QStringLiteral("main::export_font"));
    }
    topbar[QStringLiteral("functionOrder")] = functionOrder;

    topbar[QStringLiteral("activeFunction")] =
        state.mode == ToolMode::New ? QStringLiteral("set_mode_new") : QStringLiteral("set_mode_manage");
    topbar[QStringLiteral("leftButtons")] = QJsonArray{
        QJsonObject{
            {QStringLiteral("actionId"), QStringLiteral("set_mode_manage")},
            {QStringLiteral("text"), localizedString(session, QStringLiteral("TabManage"))},
            {QStringLiteral("checked"), state.mode != ToolMode::New},
            {QStringLiteral("variant"), QStringLiteral("toolbar")},
            {QStringLiteral("width"), 92}
        },
        QJsonObject{
            {QStringLiteral("actionId"), QStringLiteral("set_mode_new")},
            {QStringLiteral("text"), localizedString(session, QStringLiteral("TabNew"))},
            {QStringLiteral("checked"), state.mode == ToolMode::New},
            {QStringLiteral("variant"), QStringLiteral("toolbar")},
            {QStringLiteral("width"), 92}
        }
    };

    QJsonArray rightButtons;
    if (state.mode == ToolMode::New) {
        rightButtons.append(QJsonObject{
            {QStringLiteral("actionId"), QStringLiteral("import_ttf")},
            {QStringLiteral("text"), localizedString(session, QStringLiteral("ImportTtf"))},
            {QStringLiteral("shortcut"), QStringLiteral("Ctrl+O")},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("width"), 104}
        });
        rightButtons.append(QJsonObject{
            {QStringLiteral("actionId"), QStringLiteral("export_font")},
            {QStringLiteral("text"), localizedString(session, QStringLiteral("Export"))},
            {QStringLiteral("shortcut"), QStringLiteral("Ctrl+S")},
            {QStringLiteral("enabled"), state.canExport},
            {QStringLiteral("width"), 88}
        });
    }
    topbar[QStringLiteral("rightButtons")] = rightButtons;
    return topbar;
}

QJsonObject buildLegacyModelsObject(const QJsonArray& listModels) {
    QJsonObject models;
    for (const QJsonValue& value : listModels) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject model = value.toObject();
        const QString id = model.value(QStringLiteral("id")).toString().trimmed();
        if (!id.isEmpty()) {
            models[id] = model;
        }
    }
    return models;
}

void setSessionError(WorkerSession* session, const QString& message) {
    if (session) session->lastError = toStdString(message);
}

void clearSessionError(WorkerSession* session) {
    if (session) session->lastError.clear();
}

bool ensureCoreInitialized(WorkerSession* session) {
    if (!session) return false;
    if (session->coreInitialized) return true;
    if (!session->core.initialize()) {
        setSessionError(session, fromStdString(session->core.lastError()));
        return false;
    }
    session->coreInitialized = true;
    clearSessionError(session);
    return true;
}

ToolWorkerResult applyCoreResult(WorkerSession* session, bool ok) {
    if (ok) {
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }
    setSessionError(session, fromStdString(session->core.lastError()));
    return TOOL_WORKER_ERROR_ACTION_FAILED;
}

QMap<QString, QString> localizedStringsFromJson(const QJsonObject& object) {
    QMap<QString, QString> strings;
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it->isString()) strings.insert(it.key(), it->toString());
    }
    return strings;
}

} // namespace

ToolWorkerHandle createWorkerHandle(const char* toolId) {
    Q_UNUSED(toolId);
    try {
        return reinterpret_cast<ToolWorkerHandle>(new WorkerSession());
    } catch (...) {
        return nullptr;
    }
}

void destroyWorkerHandle(ToolWorkerHandle handle) {
    delete sessionFromHandle(handle);
}

WorkerSession* sessionFromHandle(ToolWorkerHandle handle) {
    return reinterpret_cast<WorkerSession*>(handle);
}

char* allocateCString(const QByteArray& utf8) {
    char* result = static_cast<char*>(std::malloc(static_cast<std::size_t>(utf8.size()) + 1U));
    if (!result) return nullptr;
    std::memcpy(result, utf8.constData(), static_cast<std::size_t>(utf8.size()));
    result[utf8.size()] = '\0';
    return result;
}

QJsonObject parseJsonObject(const char* jsonText) {
    if (!jsonText || *jsonText == '\0') return {};
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(jsonText));
    return document.isObject() ? document.object() : QJsonObject();
}

QJsonObject buildStatePacket(WorkerSession* session) {
    QJsonObject packet;
    packet[QStringLiteral("pageId")] = QStringLiteral("main");
    packet[QStringLiteral("currentPage")] = QStringLiteral("main");

    if (!session) {
        packet[QStringLiteral("modeId")] = QStringLiteral("manage");
        packet[QStringLiteral("viewState")] = QJsonObject();
        packet[QStringLiteral("sidebarState")] = QJsonObject();
        packet[QStringLiteral("topbarState")] = QJsonObject();
        packet[QStringLiteral("runtimeVariables")] = QJsonObject();
        packet[QStringLiteral("listModels")] = QJsonArray();
        packet[QStringLiteral("models")] = QJsonObject();
        packet[QStringLiteral("patches")] = QJsonArray();
        if (session && !session->lastError.empty()) packet[QStringLiteral("error")] = fromStdString(session->lastError);
        return packet;
    }

    ensureCoreInitialized(session);

    const Snapshot state = session->core.buildSnapshot();
    QJsonArray listModels;
    listModels.append(buildListModel(session, state));
    packet[QStringLiteral("modeId")] = state.mode == ToolMode::New ? QStringLiteral("new") : QStringLiteral("manage");
    packet[QStringLiteral("viewState")] = buildViewState(session, state);
    packet[QStringLiteral("sidebarState")] = buildSidebarState(session, state);
    packet[QStringLiteral("topbarState")] = buildTopbarState(session, state);
    packet[QStringLiteral("runtimeVariables")] = QJsonObject();
    packet[QStringLiteral("listModels")] = listModels;
    packet[QStringLiteral("models")] = buildLegacyModelsObject(listModels);
    packet[QStringLiteral("patches")] = QJsonArray();
    packet[QStringLiteral("values")] = packet[QStringLiteral("viewState")].toObject();
    return packet;
}

char* serializeStatePacket(WorkerSession* session) {
    const QByteArray stateJson = QJsonDocument(buildStatePacket(session)).toJson(QJsonDocument::Compact);
    if (session) session->lastSerializedState = stateJson.toStdString();
    return allocateCString(stateJson);
}

ToolWorkerResult initializeSession(WorkerSession* session, const char* configJson) {
    if (!session) return TOOL_WORKER_ERROR_INVALID_HANDLE;
    const QJsonObject config = parseJsonObject(configJson);
    session->toolDirectoryPath = config.value(QStringLiteral("toolDirectory")).toString();
    if (session->toolDirectoryPath.trimmed().isEmpty()) session->toolDirectoryPath = QDir::currentPath();
    session->localizedStrings = localizedStringsFromJson(config.value(QStringLiteral("localizedStrings")).toObject());
    session->gameLanguage = config.value(QStringLiteral("gameLanguage")).toString(QStringLiteral("l_english"));
    session->gameLanguageNames = localizedStringsFromJson(config.value(QStringLiteral("gameLanguageNames")).toObject());
    session->fileSystem = std::make_unique<ToolRuntimeFontFileSystem>(session);
    session->dryadAtlas = std::make_unique<DryadAtlasRuntimeService>(session->fileSystem.get());
    session->core.setFileSystem(session->fileSystem.get());
    session->core.setDryadAtlas(session->dryadAtlas.get());
    session->coreInitialized = false;
    session->lastError.clear();
    return TOOL_WORKER_SUCCESS;
}

ToolWorkerResult applyActionInternal(WorkerSession* session,
                                     const char* actionType,
                                     const char* targetId,
                                     const char* argumentsJson) {
    if (!session) return TOOL_WORKER_ERROR_INVALID_HANDLE;
    const QJsonObject arguments = parseJsonObject(argumentsJson);
    const QString action = QString::fromUtf8(actionType ? actionType : "");

    if (action == QStringLiteral("load_language")) {
        session->localizedStrings = localizedStringsFromJson(arguments.value(QStringLiteral("localizedStrings")).toObject());
        session->gameLanguage = arguments.value(QStringLiteral("gameLanguage")).toString(session->gameLanguage);
        const QJsonObject gameLanguageNames = arguments.value(QStringLiteral("gameLanguageNames")).toObject();
        if (!gameLanguageNames.isEmpty()) {
            session->gameLanguageNames = localizedStringsFromJson(gameLanguageNames);
        }
        if (session->coreInitialized) {
            session->core.refreshFonts();
        }
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (!ensureCoreInitialized(session)) return TOOL_WORKER_ERROR_INITIALIZATION_FAILED;

    if (action == QStringLiteral("import_ttf")) {
        const QJsonArray paths = arguments.value(QStringLiteral("paths")).toArray();
        std::vector<ImportedFont> imports;
        QStringList importErrors;
        imports.reserve(static_cast<std::size_t>(paths.size()));
        for (const QJsonValue& value : paths) {
            const QString path = value.toString();
            QFile file(path);
            const QString fileName = QFileInfo(path).fileName();
            if (!file.open(QIODevice::ReadOnly)) {
                importErrors.append(QStringLiteral("%1: failed to read file.").arg(fileName));
                continue;
            }
            const QByteArray bytes = file.readAll();
            ImportedFont imported;
            imported.sourceKey = toStdString(stableSha1Hex(normalizeRuntimePath(path)));
            imported.fileName = toStdString(fileName);
            const auto* begin = reinterpret_cast<const std::uint8_t*>(bytes.constData());
            imported.ttfBytes.assign(begin, begin + bytes.size());
            if (session->dryadAtlas) {
                std::string error;
                if (!session->dryadAtlas->readTtfInfo(imported.ttfBytes, &imported.familyName, &imported.glyphCount, &error)) {
                    importErrors.append(QStringLiteral("%1: %2").arg(fileName, fromStdString(error)));
                    continue;
                }
            }
            if (imported.glyphCount <= 0) {
                importErrors.append(QStringLiteral("%1: no usable glyphs were discovered.").arg(fileName));
                continue;
            }
            imports.push_back(std::move(imported));
        }
        if (imports.empty() && !importErrors.isEmpty()) {
            setSessionError(session, importErrors.join(QStringLiteral("\n")));
            return TOOL_WORKER_ERROR_ACTION_FAILED;
        }
        return applyCoreResult(session, session->core.importTtfFiles(std::move(imports)));
    }

    if (action == QStringLiteral("update_settings")) {
        const Snapshot snapshot = session->core.buildSnapshot();
        return applyCoreResult(session, session->core.updateSettings(settingsFromJson(arguments, snapshot.settings)));
    }

    if (action == QStringLiteral("set_preview_text")) {
        return applyCoreResult(session, session->core.setPreviewText(toStdString(arguments.value(QStringLiteral("text")).toString())));
    }

    if (action == QStringLiteral("export_font")) {
        const Snapshot snapshot = session->core.buildSnapshot();
        session->core.updateSettings(settingsFromJson(arguments.value(QStringLiteral("settings")).toObject(), snapshot.settings));
        const FontManager::ExportResult result = session->core.exportSelectedImport();
        if (result.pendingOverwrite) {
            clearSessionError(session);
            return TOOL_WORKER_SUCCESS;
        }
        if (!result.success) {
            setSessionError(session, fromStdString(result.errorMessage));
            return TOOL_WORKER_ERROR_ACTION_FAILED;
        }
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == QStringLiteral("confirm_overwrite")) {
        return applyCoreResult(session, session->core.confirmPendingOverwrite());
    }
    if (action == QStringLiteral("cancel_overwrite")) {
        session->core.cancelPendingOverwrite();
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    std::map<std::string, std::string> params;
    if (targetId && *targetId) params["targetId"] = targetId;
    for (auto it = arguments.begin(); it != arguments.end(); ++it) {
        if (it->isString()) params[toStdString(it.key())] = toStdString(it->toString());
    }
    return applyCoreResult(session, session->core.handleAction(toStdString(action), params));
}

} // namespace FontManagerBridge

extern "C" {

TOOL_WORKER_API ToolWorkerHandle ToolWorker_Create(const char* toolId) {
    return FontManagerBridge::createWorkerHandle(toolId);
}

TOOL_WORKER_API void ToolWorker_Destroy(ToolWorkerHandle handle) {
    FontManagerBridge::destroyWorkerHandle(handle);
}

TOOL_WORKER_API ToolWorkerResult ToolWorker_Initialize(ToolWorkerHandle handle, const char* configJson) {
    return FontManagerBridge::initializeSession(FontManagerBridge::sessionFromHandle(handle), configJson);
}

TOOL_WORKER_API const char* ToolWorker_HandleAction(
    ToolWorkerHandle handle,
    const char* actionType,
    const char* targetId,
    const char* argumentsJson,
    ToolWorkerResult* outResult
) {
    FontManagerBridge::WorkerSession* session = FontManagerBridge::sessionFromHandle(handle);
    if (!session) {
        if (outResult) *outResult = TOOL_WORKER_ERROR_INVALID_HANDLE;
        return nullptr;
    }
    if (session->actionInProgress) {
        if (outResult) *outResult = TOOL_WORKER_ERROR_ACTION_FAILED;
        return FontManagerBridge::serializeStatePacket(session);
    }
    session->actionInProgress = true;
    const ToolWorkerResult result = FontManagerBridge::applyActionInternal(session, actionType, targetId, argumentsJson);
    session->actionInProgress = false;
    if (outResult) *outResult = result;
    return FontManagerBridge::serializeStatePacket(session);
}

TOOL_WORKER_API const char* ToolWorker_GetCurrentState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    FontManagerBridge::WorkerSession* session = FontManagerBridge::sessionFromHandle(handle);
    if (!session) {
        if (outResult) *outResult = TOOL_WORKER_ERROR_INVALID_HANDLE;
        return nullptr;
    }
    if (outResult) *outResult = TOOL_WORKER_SUCCESS;
    return FontManagerBridge::serializeStatePacket(session);
}

TOOL_WORKER_API const char* ToolWorker_GetInitialState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    return ToolWorker_GetCurrentState(handle, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetLastError(ToolWorkerHandle handle) {
    FontManagerBridge::WorkerSession* session = FontManagerBridge::sessionFromHandle(handle);
    return session ? session->lastError.c_str() : "Invalid worker handle.";
}

TOOL_WORKER_API void ToolWorker_FreeString(const char* value) {
    std::free(const_cast<char*>(value));
}

TOOL_WORKER_API const char* ToolWorker_GetVersion() {
    return "1.0.0";
}

TOOL_WORKER_API const char* tool_worker_get_version() {
    return ToolWorker_GetVersion();
}

TOOL_WORKER_API int tool_worker_initialize(void* runtimeContext) {
    Q_UNUSED(runtimeContext);
    return 0;
}

TOOL_WORKER_API void tool_worker_shutdown() {
}

TOOL_WORKER_API const char* tool_worker_get_initial_state() {
    return "{}";
}

TOOL_WORKER_API const char* tool_worker_handle_action(const char* actionJson) {
    Q_UNUSED(actionJson);
    return "{}";
}

TOOL_WORKER_API void tool_worker_free_string(char* value) {
    std::free(value);
}

} // extern "C"
