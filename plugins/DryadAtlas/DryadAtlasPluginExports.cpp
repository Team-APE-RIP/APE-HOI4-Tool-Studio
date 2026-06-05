//-------------------------------------------------------------------------------------
// DryadAtlasPluginExports.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "../../src/PluginAbi.h"

#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef _WIN32
#define APE_DRYAD_EXPORT extern "C" __declspec(dllexport)
#else
#define APE_DRYAD_EXPORT extern "C"
#endif

namespace {

constexpr int kDefaultAtlasWidth = 2048;
constexpr int kDefaultAtlasHeight = 4096;
constexpr int kMaxPreviewWidth = 4096;
constexpr int kMaxPreviewHeight = 2048;

QByteArray g_errorBuffer;

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
};

struct Image {
    int width = 0;
    int height = 0;
    std::vector<Rgba> pixels;
};

struct GlyphBitmap {
    std::uint32_t codepoint = 0;
    int width = 0;
    int height = 0;
    int xoffset = 0;
    int yoffset = 0;
    int xadvance = 0;
    std::vector<std::uint8_t> alpha;
};

struct PackedGlyph {
    std::uint32_t codepoint = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int xoffset = 0;
    int yoffset = 0;
    int xadvance = 0;
    int page = 0;
};

using TextColorMap = std::map<std::uint32_t, QColor>;

static TextColorMap textColorsFromJson(const QJsonObject& object);

struct GenerateOptions {
    std::string atlasName = "ape_font";
    std::string outputName = "ape_font";
    std::string registrationName = "ape_font16";
    std::string fontName;
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
    int textureWidth = kDefaultAtlasWidth;
    int textureHeight = kDefaultAtlasHeight;
    TextColorMap textColors;
};

struct FntChar {
    std::uint32_t id = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int xoffset = 0;
    int yoffset = 0;
    int xadvance = 0;
    int page = 0;
    int chnl = 4;
};

struct FontAtlas {
    int lineHeight = 18;
    int base = 14;
    std::map<std::uint32_t, FntChar> chars;
    std::vector<Image> pages;
};

std::mutex g_renderCacheMutex;
QByteArray g_renderCacheKey;
std::shared_ptr<const FontAtlas> g_renderCacheAtlas;

static void setError(const QByteArray& message) {
    g_errorBuffer = message;
}

static void clearError() {
    g_errorBuffer.clear();
}

static char* allocateCString(const QByteArray& utf8) {
    char* result = static_cast<char*>(std::malloc(static_cast<std::size_t>(utf8.size()) + 1U));
    if (!result) {
        return nullptr;
    }
    std::memcpy(result, utf8.constData(), static_cast<std::size_t>(utf8.size()));
    result[utf8.size()] = '\0';
    return result;
}

static std::uint16_t readU16BE(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

static std::uint32_t readU32BE(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24)
        | (static_cast<std::uint32_t>(data[1]) << 16)
        | (static_cast<std::uint32_t>(data[2]) << 8)
        | static_cast<std::uint32_t>(data[3]);
}

static std::uint32_t readU32LE(const std::uint8_t* data) {
    return static_cast<std::uint32_t>(data[0])
        | (static_cast<std::uint32_t>(data[1]) << 8)
        | (static_cast<std::uint32_t>(data[2]) << 16)
        | (static_cast<std::uint32_t>(data[3]) << 24);
}

static std::uint32_t makeFourCc(char a, char b, char c, char d) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a))
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}

static void writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

static void writeZeroes(std::vector<std::uint8_t>& out, int count) {
    out.insert(out.end(), static_cast<std::size_t>(count), 0);
}

static std::string sanitizeAtlasName(std::string value) {
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
    if (result.empty()) {
        result = "ape_font";
    }
    return result;
}

static std::wstring utf8ToWide(const std::string& value) {
#ifdef _WIN32
    if (value.empty()) {
        return {};
    }
    const int count = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), count);
    return result;
#else
    std::wstring result;
    for (char ch : value) {
        result.push_back(static_cast<unsigned char>(ch));
    }
    return result;
#endif
}

static std::string wideToUtf8(const std::wstring& value) {
#ifdef _WIN32
    if (value.empty()) {
        return {};
    }
    const int count = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
#else
    std::string result;
    for (wchar_t ch : value) {
        result.push_back(static_cast<char>(ch));
    }
    return result;
#endif
}

static std::wstring decodeUtf16BE(const std::uint8_t* data, std::size_t size) {
    std::wstring result;
    if (!data || size < 2) {
        return result;
    }
    result.reserve(size / 2);
    for (std::size_t i = 0; i + 1 < size; i += 2) {
        result.push_back(static_cast<wchar_t>(readU16BE(data + i)));
    }
    return result;
}

struct TtfTable {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

static std::uint32_t firstFontOffset(const std::uint8_t* data, int size) {
    if (!data || size < 12) {
        return 0;
    }
    if (std::memcmp(data, "ttcf", 4) != 0) {
        return 0;
    }
    const std::uint32_t fontCount = readU32BE(data + 8);
    if (fontCount == 0 || 12u + fontCount * 4u > static_cast<std::uint32_t>(size)) {
        return 0;
    }
    const std::uint32_t offset = readU32BE(data + 12);
    return offset < static_cast<std::uint32_t>(size) ? offset : 0;
}

static bool findTtfTable(const std::uint8_t* data, int size, const char tag[4], TtfTable* outTable) {
    if (!data || size < 12 || !outTable) {
        return false;
    }
    const std::uint32_t fontOffset = firstFontOffset(data, size);
    if (fontOffset > static_cast<std::uint32_t>(size) - 12u) {
        return false;
    }
    const std::uint8_t* directory = data + fontOffset;
    const std::uint16_t tableCount = readU16BE(directory + 4);
    const std::uint32_t directoryEnd = fontOffset + 12u + static_cast<std::uint32_t>(tableCount) * 16u;
    if (directoryEnd > static_cast<std::uint32_t>(size)) {
        return false;
    }
    for (std::uint16_t i = 0; i < tableCount; ++i) {
        const std::uint8_t* record = directory + 12u + static_cast<std::uint32_t>(i) * 16u;
        if (std::memcmp(record, tag, 4) != 0) {
            continue;
        }
        const std::uint32_t offset = readU32BE(record + 8);
        const std::uint32_t length = readU32BE(record + 12);
        if (offset > static_cast<std::uint32_t>(size) || length > static_cast<std::uint32_t>(size) - offset) {
            return false;
        }
        outTable->offset = offset;
        outTable->length = length;
        return true;
    }
    return false;
}

static std::string readTtfFamilyName(const std::uint8_t* data, int size) {
    TtfTable table;
    if (!findTtfTable(data, size, "name", &table) || table.length < 6) {
        return {};
    }
    const std::uint8_t* name = data + table.offset;
    const std::uint16_t count = readU16BE(name + 2);
    const std::uint16_t stringOffset = readU16BE(name + 4);
    if (6u + static_cast<std::uint32_t>(count) * 12u > table.length || stringOffset >= table.length) {
        return {};
    }

    std::string fallback;
    for (std::uint16_t i = 0; i < count; ++i) {
        const std::uint8_t* rec = name + 6u + static_cast<std::uint32_t>(i) * 12u;
        const std::uint16_t platformId = readU16BE(rec);
        const std::uint16_t nameId = readU16BE(rec + 6);
        const std::uint16_t length = readU16BE(rec + 8);
        const std::uint16_t offset = readU16BE(rec + 10);
        if (nameId != 1 || static_cast<std::uint32_t>(stringOffset) + offset + length > table.length) {
            continue;
        }
        const std::uint8_t* text = name + stringOffset + offset;
        if (platformId == 0 || platformId == 3) {
            const std::string decoded = wideToUtf8(decodeUtf16BE(text, length));
            if (!decoded.empty()) {
                return decoded;
            }
        } else if (platformId == 1 && fallback.empty()) {
            fallback.assign(reinterpret_cast<const char*>(text), reinterpret_cast<const char*>(text + length));
        }
    }
    return fallback;
}

static std::vector<std::uint32_t> enumerateTtfCodepoints(const std::uint8_t* data, int size) {
    std::set<std::uint32_t> codepoints;
    TtfTable table;
    if (!findTtfTable(data, size, "cmap", &table) || table.length < 4) {
        return {};
    }
    const std::uint8_t* cmap = data + table.offset;
    const std::uint16_t subtableCount = readU16BE(cmap + 2);
    if (4u + static_cast<std::uint32_t>(subtableCount) * 8u > table.length) {
        return {};
    }

    auto parseFormat4 = [&](const std::uint8_t* subtable, std::uint32_t subtableLength) {
        if (subtableLength < 16) {
            return;
        }
        const std::uint16_t length = readU16BE(subtable + 2);
        if (length > subtableLength || length < 16) {
            return;
        }
        const std::uint16_t segCount = readU16BE(subtable + 6) / 2;
        const std::uint32_t endCodesOffset = 14;
        const std::uint32_t startCodesOffset = endCodesOffset + static_cast<std::uint32_t>(segCount) * 2u + 2u;
        const std::uint32_t idDeltaOffset = startCodesOffset + static_cast<std::uint32_t>(segCount) * 2u;
        const std::uint32_t idRangeOffset = idDeltaOffset + static_cast<std::uint32_t>(segCount) * 2u;
        if (idRangeOffset + static_cast<std::uint32_t>(segCount) * 2u > length) {
            return;
        }
        for (std::uint16_t s = 0; s < segCount; ++s) {
            const std::uint16_t start = readU16BE(subtable + startCodesOffset + s * 2u);
            const std::uint16_t end = readU16BE(subtable + endCodesOffset + s * 2u);
            const std::uint16_t rangeOffset = readU16BE(subtable + idRangeOffset + s * 2u);
            if (start == 0xFFFFu && end == 0xFFFFu) {
                continue;
            }
            if (end < start) {
                continue;
            }
            for (std::uint32_t cp = start; cp <= end; ++cp) {
                if (cp > 0xFFFFu) {
                    break;
                }
                if (rangeOffset == 0) {
                    codepoints.insert(cp);
                    continue;
                }
                const std::uint32_t glyphOffset = idRangeOffset + s * 2u + rangeOffset + (cp - start) * 2u;
                if (glyphOffset + 2u <= length && readU16BE(subtable + glyphOffset) != 0) {
                    codepoints.insert(cp);
                }
            }
        }
    };

    auto parseFormat12 = [&](const std::uint8_t* subtable, std::uint32_t subtableLength) {
        if (subtableLength < 16) {
            return;
        }
        const std::uint32_t length = readU32BE(subtable + 4);
        if (length > subtableLength || length < 16) {
            return;
        }
        const std::uint32_t groupCount = readU32BE(subtable + 12);
        if (16u + groupCount * 12u > length) {
            return;
        }
        for (std::uint32_t g = 0; g < groupCount; ++g) {
            const std::uint8_t* group = subtable + 16u + g * 12u;
            const std::uint32_t start = readU32BE(group);
            const std::uint32_t end = readU32BE(group + 4);
            if (end < start) {
                continue;
            }
            for (std::uint32_t cp = start; cp <= end && cp <= 0xFFFFu; ++cp) {
                codepoints.insert(cp);
            }
        }
    };

    auto parseFormat13 = parseFormat12;

    auto parseFormat0 = [&](const std::uint8_t* subtable, std::uint32_t subtableLength) {
        if (subtableLength < 262) {
            return;
        }
        const std::uint16_t length = readU16BE(subtable + 2);
        if (length > subtableLength || length < 262) {
            return;
        }
        for (std::uint32_t cp = 0; cp < 256; ++cp) {
            if (subtable[6 + cp] != 0) {
                codepoints.insert(cp);
            }
        }
    };

    auto parseFormat6 = [&](const std::uint8_t* subtable, std::uint32_t subtableLength) {
        if (subtableLength < 10) {
            return;
        }
        const std::uint16_t length = readU16BE(subtable + 2);
        if (length > subtableLength || length < 10) {
            return;
        }
        const std::uint16_t firstCode = readU16BE(subtable + 6);
        const std::uint16_t entryCount = readU16BE(subtable + 8);
        if (10u + static_cast<std::uint32_t>(entryCount) * 2u > length) {
            return;
        }
        for (std::uint32_t i = 0; i < entryCount; ++i) {
            if (readU16BE(subtable + 10u + i * 2u) != 0) {
                codepoints.insert(static_cast<std::uint32_t>(firstCode) + i);
            }
        }
    };

    auto parseFormat10 = [&](const std::uint8_t* subtable, std::uint32_t subtableLength) {
        if (subtableLength < 20) {
            return;
        }
        const std::uint32_t length = readU32BE(subtable + 4);
        if (length > subtableLength || length < 20) {
            return;
        }
        const std::uint32_t startCode = readU32BE(subtable + 12);
        const std::uint32_t entryCount = readU32BE(subtable + 16);
        if (20u + entryCount * 2u > length) {
            return;
        }
        for (std::uint32_t i = 0; i < entryCount; ++i) {
            if (readU16BE(subtable + 20u + i * 2u) != 0) {
                const std::uint32_t cp = startCode + i;
                if (cp <= 0xFFFFu) {
                    codepoints.insert(cp);
                }
            }
        }
    };

    for (std::uint16_t i = 0; i < subtableCount; ++i) {
        const std::uint8_t* rec = cmap + 4u + static_cast<std::uint32_t>(i) * 8u;
        const std::uint32_t offset = readU32BE(rec + 4);
        if (offset + 2u > table.length) {
            continue;
        }
        const std::uint8_t* subtable = cmap + offset;
        const std::uint32_t subtableLength = table.length - offset;
        const std::uint16_t format = readU16BE(subtable);
        if (format == 0) {
            parseFormat0(subtable, subtableLength);
        } else if (format == 4) {
            parseFormat4(subtable, subtableLength);
        } else if (format == 6) {
            parseFormat6(subtable, subtableLength);
        } else if (format == 10) {
            parseFormat10(subtable, subtableLength);
        } else if (format == 12) {
            parseFormat12(subtable, subtableLength);
        } else if (format == 13) {
            parseFormat13(subtable, subtableLength);
        }
    }

    for (auto it = codepoints.begin(); it != codepoints.end();) {
        if (*it < 32 || *it == 0x7Fu || (*it >= 0xD800u && *it <= 0xDFFFu)) {
            it = codepoints.erase(it);
        } else {
            ++it;
        }
    }
    codepoints.insert(32);
    codepoints.insert(63);
    return std::vector<std::uint32_t>(codepoints.begin(), codepoints.end());
}

static GenerateOptions optionsFromJson(const QJsonObject& object) {
    GenerateOptions options;
    options.atlasName = sanitizeAtlasName(object.value(QStringLiteral("atlasName")).toString(QStringLiteral("ape_font")).toStdString());
    options.outputName = sanitizeAtlasName(object.value(QStringLiteral("outputName")).toString(QString::fromStdString(options.atlasName)).toStdString());
    options.fontName = object.value(QStringLiteral("fontName")).toString().toStdString();
    options.sizePx = std::max(1, object.value(QStringLiteral("sizePx")).toInt(options.sizePx));
    options.heightPercent = std::max(10, object.value(QStringLiteral("heightPercent")).toInt(options.heightPercent));
    options.bold = object.value(QStringLiteral("bold")).toBool(false);
    options.italic = object.value(QStringLiteral("italic")).toBool(false);
    options.underline = object.value(QStringLiteral("underline")).toBool(false);
    options.smoothing = object.value(QStringLiteral("smoothing")).toBool(true);
    options.clearType = object.value(QStringLiteral("clearType")).toBool(true);
    options.forceOffsetsZero = object.value(QStringLiteral("forceOffsetsZero")).toBool(true);
    options.paddingUp = std::max(0, object.value(QStringLiteral("paddingUp")).toInt(options.paddingUp));
    options.paddingRight = std::max(0, object.value(QStringLiteral("paddingRight")).toInt(options.paddingRight));
    options.paddingDown = std::max(0, object.value(QStringLiteral("paddingDown")).toInt(options.paddingDown));
    options.paddingLeft = std::max(0, object.value(QStringLiteral("paddingLeft")).toInt(options.paddingLeft));
    options.spacingHorizontal = std::max(0, object.value(QStringLiteral("spacingHorizontal")).toInt(options.spacingHorizontal));
    options.spacingVertical = std::max(0, object.value(QStringLiteral("spacingVertical")).toInt(options.spacingVertical));
    options.outlineThickness = std::max(0, object.value(QStringLiteral("outlineThickness")).toInt(options.outlineThickness));
    options.textureWidth = std::max(64, object.value(QStringLiteral("textureWidth")).toInt(kDefaultAtlasWidth));
    options.textureHeight = std::max(64, object.value(QStringLiteral("textureHeight")).toInt(kDefaultAtlasHeight));
    options.textColors = textColorsFromJson(object);
    options.registrationName = sanitizeAtlasName(object.value(QStringLiteral("registrationName")).toString().toStdString());
    if (options.registrationName == "ape_font" && !object.contains(QStringLiteral("registrationName"))) {
        options.registrationName = options.outputName + std::to_string(options.sizePx);
    }
    return options;
}

static void applyOutline(GlyphBitmap& glyph, int radius) {
    if (radius <= 0 || glyph.width <= 0 || glyph.height <= 0 || glyph.alpha.empty()) {
        return;
    }
    std::vector<std::uint8_t> outlined = glyph.alpha;
    for (int y = 0; y < glyph.height; ++y) {
        for (int x = 0; x < glyph.width; ++x) {
            std::uint8_t best = glyph.alpha[static_cast<std::size_t>(y * glyph.width + x)];
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    const int sx = x + dx;
                    const int sy = y + dy;
                    if (sx < 0 || sy < 0 || sx >= glyph.width || sy >= glyph.height) {
                        continue;
                    }
                    best = std::max(best, glyph.alpha[static_cast<std::size_t>(sy * glyph.width + sx)]);
                }
            }
            outlined[static_cast<std::size_t>(y * glyph.width + x)] = best;
        }
    }
    glyph.alpha.swap(outlined);
}

#ifdef _WIN32
static GlyphBitmap rasterizeGlyph(HDC dc, std::uint32_t cp, int ascent, const GenerateOptions& options) {
    GlyphBitmap glyph;
    glyph.codepoint = cp;

    WCHAR wc = static_cast<WCHAR>(cp);
    ABC abc{};
    if (GetCharABCWidthsW(dc, wc, wc, &abc)) {
        glyph.xadvance = abc.abcA + static_cast<int>(abc.abcB) + abc.abcC;
    }
    if (glyph.xadvance <= 0) {
        SIZE size{};
        GetTextExtentPoint32W(dc, &wc, 1, &size);
        glyph.xadvance = std::max(1L, size.cx);
    }

    if (cp == 32) {
        glyph.width = 0;
        glyph.height = 0;
        glyph.xoffset = 0;
        glyph.yoffset = 0;
        return glyph;
    }

    MAT2 matrix{};
    matrix.eM11.value = 1;
    matrix.eM22.value = 1;
    GLYPHMETRICS metrics{};
    const UINT format = options.smoothing ? GGO_GRAY8_BITMAP : GGO_BITMAP;
    DWORD byteCount = GetGlyphOutlineW(dc, wc, format, &metrics, 0, nullptr, &matrix);
    if (byteCount == GDI_ERROR || byteCount == 0) {
        return glyph;
    }

    std::vector<std::uint8_t> buffer(byteCount);
    if (GetGlyphOutlineW(dc, wc, format, &metrics, byteCount, buffer.data(), &matrix) == GDI_ERROR) {
        glyph.width = 0;
        glyph.height = 0;
        glyph.alpha.clear();
        return glyph;
    }

    glyph.width = static_cast<int>(metrics.gmBlackBoxX);
    glyph.height = static_cast<int>(metrics.gmBlackBoxY);
    glyph.xoffset = options.forceOffsetsZero ? 0 : metrics.gmptGlyphOrigin.x;
    glyph.yoffset = options.forceOffsetsZero ? 0 : ascent - metrics.gmptGlyphOrigin.y;
    glyph.xadvance = std::max(glyph.xadvance, static_cast<int>(metrics.gmCellIncX));
    glyph.alpha.assign(static_cast<std::size_t>(std::max(0, glyph.width) * std::max(0, glyph.height)), 0);
    if (glyph.width <= 0 || glyph.height <= 0) {
        return glyph;
    }

    if (options.smoothing) {
        const int pitch = ((glyph.width + 3) / 4) * 4;
        for (int y = 0; y < glyph.height; ++y) {
            for (int x = 0; x < glyph.width; ++x) {
                const std::uint8_t value = buffer[static_cast<std::size_t>(y * pitch + x)];
                glyph.alpha[static_cast<std::size_t>(y * glyph.width + x)] =
                    static_cast<std::uint8_t>(std::min(255, static_cast<int>(value) * 4));
            }
        }
    } else {
        const int pitch = ((glyph.width + 31) / 32) * 4;
        for (int y = 0; y < glyph.height; ++y) {
            for (int x = 0; x < glyph.width; ++x) {
                const std::uint8_t mask = static_cast<std::uint8_t>(0x80u >> (x % 8));
                const bool on = (buffer[static_cast<std::size_t>(y * pitch + x / 8)] & mask) != 0;
                glyph.alpha[static_cast<std::size_t>(y * glyph.width + x)] = on ? 255 : 0;
            }
        }
    }

    applyOutline(glyph, options.outlineThickness);
    if (options.underline && glyph.height > 2) {
        const int underlineY = std::max(0, glyph.height - 2);
        for (int x = 0; x < glyph.width; ++x) {
            glyph.alpha[static_cast<std::size_t>(underlineY * glyph.width + x)] = 255;
        }
    }
    return glyph;
}
#endif

static std::vector<GlyphBitmap> rasterizeTtfCodepoints(const std::uint8_t* data,
                                                       int size,
                                                       GenerateOptions& options,
                                                       const std::vector<std::uint32_t>& codepoints,
                                                       int* outLineHeight,
                                                       int* outBase) {
#ifndef _WIN32
    Q_UNUSED(data);
    Q_UNUSED(size);
    Q_UNUSED(options);
    Q_UNUSED(codepoints);
    Q_UNUSED(outLineHeight);
    Q_UNUSED(outBase);
    setError("DryadAtlas font rasterization currently requires Windows GDI.");
    return {};
#else
    DWORD fontCount = 0;
    HANDLE fontResource = AddFontMemResourceEx(const_cast<std::uint8_t*>(data), static_cast<DWORD>(size), nullptr, &fontCount);
    if (!fontResource) {
        setError("Failed to register the imported TTF font.");
        return {};
    }

    const std::string parsedFamily = readTtfFamilyName(data, size);
    if (options.fontName.empty()) {
        options.fontName = parsedFamily;
    }

    const std::wstring family = utf8ToWide(options.fontName.empty() ? parsedFamily : options.fontName);
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        RemoveFontMemResourceEx(fontResource);
        setError("Failed to create a font rasterization context.");
        return {};
    }

    const int quality = options.smoothing
        ? (options.clearType ? CLEARTYPE_QUALITY : ANTIALIASED_QUALITY)
        : NONANTIALIASED_QUALITY;
    HFONT font = CreateFontW(
        -options.sizePx,
        0,
        0,
        0,
        options.bold ? FW_BOLD : FW_NORMAL,
        options.italic ? TRUE : FALSE,
        options.underline ? TRUE : FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        quality,
        DEFAULT_PITCH | FF_DONTCARE,
        family.empty() ? nullptr : family.c_str()
    );
    if (!font) {
        DeleteDC(dc);
        RemoveFontMemResourceEx(fontResource);
        setError("Failed to create a raster font.");
        return {};
    }

    HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    const int lineHeight = std::max(1, static_cast<int>(std::ceil(static_cast<double>(tm.tmHeight) * options.heightPercent / 100.0)));
    const int base = std::max(1L, tm.tmAscent);
    if (outLineHeight) {
        *outLineHeight = lineHeight;
    }
    if (outBase) {
        *outBase = base;
    }

    std::vector<GlyphBitmap> glyphs;
    glyphs.reserve(codepoints.size());
    for (std::uint32_t cp : codepoints) {
        if (cp > 0xFFFFu) {
            continue;
        }
        WORD glyphIndex = 0xFFFFu;
        const WCHAR wc = static_cast<WCHAR>(cp);
        if (GetGlyphIndicesW(dc, &wc, 1, &glyphIndex, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR
            && glyphIndex == 0xFFFFu
            && cp != 32
            && cp != 63) {
            continue;
        }
        GlyphBitmap glyph = rasterizeGlyph(dc, cp, base, options);
        if (glyph.xadvance <= 0) {
            glyph.xadvance = std::max(1, options.sizePx / 2);
        }
        glyphs.push_back(std::move(glyph));
    }

    SelectObject(dc, oldFont);
    DeleteObject(font);
    DeleteDC(dc);
    RemoveFontMemResourceEx(fontResource);
    return glyphs;
#endif
}

static std::vector<GlyphBitmap> rasterizeTtfGlyphs(const std::uint8_t* data, int size, GenerateOptions& options, int* outLineHeight, int* outBase) {
    return rasterizeTtfCodepoints(data, size, options, enumerateTtfCodepoints(data, size), outLineHeight, outBase);
}

static std::vector<Image> packGlyphs(const std::vector<GlyphBitmap>& glyphs,
                                     const GenerateOptions& options,
                                     std::vector<PackedGlyph>& outGlyphs) {
    std::vector<Image> pages;
    pages.push_back({options.textureWidth, options.textureHeight, std::vector<Rgba>(static_cast<std::size_t>(options.textureWidth * options.textureHeight), {0, 0, 0, 0})});

    int page = 0;
    int cursorX = 0;
    int cursorY = 0;
    int rowHeight = 0;

    for (const GlyphBitmap& glyph : glyphs) {
        const int cellWidth = glyph.width + options.paddingLeft + options.paddingRight;
        const int cellHeight = glyph.height + options.paddingUp + options.paddingDown;
        if (cellWidth > options.textureWidth || cellHeight > options.textureHeight) {
            continue;
        }

        if (cursorX > 0 && cursorX + cellWidth > options.textureWidth) {
            cursorX = 0;
            cursorY += rowHeight + options.spacingVertical;
            rowHeight = 0;
        }
        if (cursorY > 0 && cursorY + cellHeight > options.textureHeight) {
            pages.push_back({options.textureWidth, options.textureHeight, std::vector<Rgba>(static_cast<std::size_t>(options.textureWidth * options.textureHeight), {0, 0, 0, 0})});
            ++page;
            cursorX = 0;
            cursorY = 0;
            rowHeight = 0;
        }

        const int glyphX = cursorX + options.paddingLeft;
        const int glyphY = cursorY + options.paddingUp;
        Image& image = pages[static_cast<std::size_t>(page)];
        for (int y = 0; y < glyph.height; ++y) {
            for (int x = 0; x < glyph.width; ++x) {
                const std::uint8_t value = glyph.alpha[static_cast<std::size_t>(y * glyph.width + x)];
                if (value == 0) {
                    continue;
                }
                Rgba& pixel = image.pixels[static_cast<std::size_t>((glyphY + y) * image.width + glyphX + x)];
                pixel.r = 255;
                pixel.g = 255;
                pixel.b = 255;
                pixel.a = value;
            }
        }

        PackedGlyph packed;
        packed.codepoint = glyph.codepoint;
        packed.x = glyphX;
        packed.y = glyphY;
        packed.width = glyph.width;
        packed.height = glyph.height;
        packed.xoffset = glyph.xoffset;
        packed.yoffset = glyph.yoffset;
        packed.xadvance = glyph.xadvance;
        packed.page = page;
        outGlyphs.push_back(packed);

        cursorX += cellWidth + options.spacingHorizontal;
        rowHeight = std::max(rowHeight, cellHeight);
    }
    return pages;
}

static void encodeBc4Block(const std::uint8_t values[16], std::uint8_t out[8]) {
    std::uint8_t minValue = 255;
    std::uint8_t maxValue = 0;
    for (int i = 0; i < 16; ++i) {
        minValue = std::min(minValue, values[i]);
        maxValue = std::max(maxValue, values[i]);
    }
    out[0] = maxValue;
    out[1] = minValue;

    std::uint8_t palette[8]{};
    palette[0] = maxValue;
    palette[1] = minValue;
    if (maxValue > minValue) {
        palette[2] = static_cast<std::uint8_t>((6 * maxValue + minValue) / 7);
        palette[3] = static_cast<std::uint8_t>((5 * maxValue + 2 * minValue) / 7);
        palette[4] = static_cast<std::uint8_t>((4 * maxValue + 3 * minValue) / 7);
        palette[5] = static_cast<std::uint8_t>((3 * maxValue + 4 * minValue) / 7);
        palette[6] = static_cast<std::uint8_t>((2 * maxValue + 5 * minValue) / 7);
        palette[7] = static_cast<std::uint8_t>((maxValue + 6 * minValue) / 7);
    } else {
        palette[2] = static_cast<std::uint8_t>((4 * maxValue + minValue) / 5);
        palette[3] = static_cast<std::uint8_t>((3 * maxValue + 2 * minValue) / 5);
        palette[4] = static_cast<std::uint8_t>((2 * maxValue + 3 * minValue) / 5);
        palette[5] = static_cast<std::uint8_t>((maxValue + 4 * minValue) / 5);
        palette[6] = 0;
        palette[7] = 255;
    }

    std::uint64_t packed = 0;
    for (int i = 0; i < 16; ++i) {
        int bestIndex = 0;
        int bestDistance = 256;
        for (int j = 0; j < 8; ++j) {
            const int distance = std::abs(static_cast<int>(values[i]) - static_cast<int>(palette[j]));
            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = j;
            }
        }
        packed |= (static_cast<std::uint64_t>(bestIndex) & 0x7u) << (3 * i);
    }

    for (int i = 0; i < 6; ++i) {
        out[2 + i] = static_cast<std::uint8_t>((packed >> (8 * i)) & 0xFFu);
    }
}

static std::vector<std::uint8_t> encodeDdsDxt5(const Image& image) {
    const int blocksX = (image.width + 3) / 4;
    const int blocksY = (image.height + 3) / 4;
    const std::uint32_t linearSize = static_cast<std::uint32_t>(blocksX * blocksY * 16);

    std::vector<std::uint8_t> out;
    out.reserve(128u + linearSize);
    out.insert(out.end(), {'D', 'D', 'S', ' '});
    writeU32LE(out, 124);
    writeU32LE(out, 0x0002100Fu);
    writeU32LE(out, static_cast<std::uint32_t>(image.height));
    writeU32LE(out, static_cast<std::uint32_t>(image.width));
    writeU32LE(out, linearSize);
    writeU32LE(out, 0);
    writeU32LE(out, 0);
    writeZeroes(out, 44);
    writeU32LE(out, 32);
    writeU32LE(out, 0x00000004u);
    out.insert(out.end(), {'D', 'X', 'T', '5'});
    writeU32LE(out, 0);
    writeU32LE(out, 0);
    writeU32LE(out, 0);
    writeU32LE(out, 0);
    writeU32LE(out, 0);
    writeU32LE(out, 0x00001000u);
    writeU32LE(out, 0);
    writeU32LE(out, 0);
    writeU32LE(out, 0);
    writeU32LE(out, 0);

    for (int by = 0; by < blocksY; ++by) {
        for (int bx = 0; bx < blocksX; ++bx) {
            std::uint8_t values[16]{};
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    const int sx = std::min(image.width - 1, bx * 4 + x);
                    const int sy = std::min(image.height - 1, by * 4 + y);
                    values[y * 4 + x] = image.pixels[static_cast<std::size_t>(sy * image.width + sx)].a;
                }
            }
            std::uint8_t block[16]{};
            encodeBc4Block(values, block);
            block[8] = 0xFF;
            block[9] = 0xFF;
            block[10] = 0xFF;
            block[11] = 0xFF;
            block[12] = 0;
            block[13] = 0;
            block[14] = 0;
            block[15] = 0;
            out.insert(out.end(), block, block + 16);
        }
    }
    return out;
}

static std::uint8_t decodeBc4Value(const std::uint8_t block[8], int index) {
    std::uint8_t palette[8]{};
    palette[0] = block[0];
    palette[1] = block[1];
    if (palette[0] > palette[1]) {
        palette[2] = static_cast<std::uint8_t>((6 * palette[0] + palette[1]) / 7);
        palette[3] = static_cast<std::uint8_t>((5 * palette[0] + 2 * palette[1]) / 7);
        palette[4] = static_cast<std::uint8_t>((4 * palette[0] + 3 * palette[1]) / 7);
        palette[5] = static_cast<std::uint8_t>((3 * palette[0] + 4 * palette[1]) / 7);
        palette[6] = static_cast<std::uint8_t>((2 * palette[0] + 5 * palette[1]) / 7);
        palette[7] = static_cast<std::uint8_t>((palette[0] + 6 * palette[1]) / 7);
    } else {
        palette[2] = static_cast<std::uint8_t>((4 * palette[0] + palette[1]) / 5);
        palette[3] = static_cast<std::uint8_t>((3 * palette[0] + 2 * palette[1]) / 5);
        palette[4] = static_cast<std::uint8_t>((2 * palette[0] + 3 * palette[1]) / 5);
        palette[5] = static_cast<std::uint8_t>((palette[0] + 4 * palette[1]) / 5);
        palette[6] = 0;
        palette[7] = 255;
    }
    std::uint64_t packed = 0;
    for (int i = 0; i < 6; ++i) {
        packed |= static_cast<std::uint64_t>(block[2 + i]) << (8 * i);
    }
    const int paletteIndex = static_cast<int>((packed >> (3 * index)) & 0x7u);
    return palette[paletteIndex];
}

static std::uint16_t decodeRgb565(std::uint16_t value) {
    return value;
}

static Rgba colorFromRgb565(std::uint16_t value) {
    const int r5 = (value >> 11) & 0x1F;
    const int g6 = (value >> 5) & 0x3F;
    const int b5 = value & 0x1F;
    return {
        static_cast<std::uint8_t>((r5 * 255 + 15) / 31),
        static_cast<std::uint8_t>((g6 * 255 + 31) / 63),
        static_cast<std::uint8_t>((b5 * 255 + 15) / 31),
        255
    };
}

static std::uint8_t luminanceAlpha(const Rgba& color) {
    if (color.a > 0 && (color.r > 0 || color.g > 0 || color.b > 0)) {
        return color.a;
    }
    const int luminance = (static_cast<int>(color.r) * 77 + static_cast<int>(color.g) * 150 + static_cast<int>(color.b) * 29) >> 8;
    return static_cast<std::uint8_t>(std::max(luminance, static_cast<int>(color.a)));
}

static void decodeDxtColorBlock(const std::uint8_t* block, Rgba colors[16], bool dxt1Alpha) {
    const std::uint16_t c0 = static_cast<std::uint16_t>(block[0] | (block[1] << 8));
    const std::uint16_t c1 = static_cast<std::uint16_t>(block[2] | (block[3] << 8));
    Rgba palette[4]{};
    palette[0] = colorFromRgb565(decodeRgb565(c0));
    palette[1] = colorFromRgb565(decodeRgb565(c1));
    if (c0 > c1 || !dxt1Alpha) {
        palette[2] = {
            static_cast<std::uint8_t>((2 * palette[0].r + palette[1].r) / 3),
            static_cast<std::uint8_t>((2 * palette[0].g + palette[1].g) / 3),
            static_cast<std::uint8_t>((2 * palette[0].b + palette[1].b) / 3),
            255
        };
        palette[3] = {
            static_cast<std::uint8_t>((palette[0].r + 2 * palette[1].r) / 3),
            static_cast<std::uint8_t>((palette[0].g + 2 * palette[1].g) / 3),
            static_cast<std::uint8_t>((palette[0].b + 2 * palette[1].b) / 3),
            255
        };
    } else {
        palette[2] = {
            static_cast<std::uint8_t>((palette[0].r + palette[1].r) / 2),
            static_cast<std::uint8_t>((palette[0].g + palette[1].g) / 2),
            static_cast<std::uint8_t>((palette[0].b + palette[1].b) / 2),
            255
        };
        palette[3] = {0, 0, 0, 0};
    }

    const std::uint32_t indices = readU32LE(block + 4);
    for (int i = 0; i < 16; ++i) {
        colors[i] = palette[(indices >> (2 * i)) & 0x3u];
    }
}

static std::uint8_t dxt5AlphaValue(const std::uint8_t block[8], int index) {
    return decodeBc4Value(block, index);
}

static Image decodeDdsImage(const QByteArray& bytes) {
    Image image;
    if (bytes.size() < 128 || std::memcmp(bytes.constData(), "DDS ", 4) != 0) {
        return image;
    }
    const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.constData());
    const std::uint32_t height = readU32LE(data + 12);
    const std::uint32_t width = readU32LE(data + 16);
    const std::uint32_t pfFlags = readU32LE(data + 80);
    const std::uint32_t fourCc = readU32LE(data + 84);
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    if (image.width <= 0 || image.height <= 0) {
        image = {};
        return image;
    }
    image.pixels.assign(static_cast<std::size_t>(image.width * image.height), {});

    const std::uint32_t ati2 = makeFourCc('A', 'T', 'I', '2');
    const std::uint32_t bc5u = makeFourCc('B', 'C', '5', 'U');
    const std::uint32_t dxt1 = makeFourCc('D', 'X', 'T', '1');
    const std::uint32_t dxt3 = makeFourCc('D', 'X', 'T', '3');
    const std::uint32_t dxt5 = makeFourCc('D', 'X', 'T', '5');
    if ((pfFlags & 0x4u) != 0 && (fourCc == ati2 || fourCc == bc5u)) {
        const int blocksX = (image.width + 3) / 4;
        const int blocksY = (image.height + 3) / 4;
        const int required = 128 + blocksX * blocksY * 16;
        if (bytes.size() < required) {
            image = {};
            return image;
        }
        const std::uint8_t* blockData = data + 128;
        for (int by = 0; by < blocksY; ++by) {
            for (int bx = 0; bx < blocksX; ++bx) {
                const std::uint8_t* block = blockData + (by * blocksX + bx) * 16;
                for (int y = 0; y < 4; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        const int sx = bx * 4 + x;
                        const int sy = by * 4 + y;
                        if (sx >= image.width || sy >= image.height) {
                            continue;
                        }
                        const std::uint8_t value = decodeBc4Value(block, y * 4 + x);
                        image.pixels[static_cast<std::size_t>(sy * image.width + sx)] = {255, 255, 255, value};
                    }
                }
            }
        }
        return image;
    }
    if ((pfFlags & 0x4u) != 0 && (fourCc == dxt1 || fourCc == dxt3 || fourCc == dxt5)) {
        const int blocksX = (image.width + 3) / 4;
        const int blocksY = (image.height + 3) / 4;
        const int blockSize = fourCc == dxt1 ? 8 : 16;
        const int required = 128 + blocksX * blocksY * blockSize;
        if (bytes.size() < required) {
            image = {};
            return image;
        }
        const std::uint8_t* blockData = data + 128;
        for (int by = 0; by < blocksY; ++by) {
            for (int bx = 0; bx < blocksX; ++bx) {
                const std::uint8_t* block = blockData + (by * blocksX + bx) * blockSize;
                Rgba colors[16]{};
                const std::uint8_t* colorBlock = block;
                if (fourCc == dxt3 || fourCc == dxt5) {
                    colorBlock = block + 8;
                }
                decodeDxtColorBlock(colorBlock, colors, fourCc == dxt1);
                for (int y = 0; y < 4; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        const int sx = bx * 4 + x;
                        const int sy = by * 4 + y;
                        if (sx >= image.width || sy >= image.height) {
                            continue;
                        }
                        const int pixelIndex = y * 4 + x;
                        Rgba color = colors[pixelIndex];
                        if (fourCc == dxt3) {
                            const std::uint8_t packed = block[pixelIndex / 2];
                            const std::uint8_t nibble = (pixelIndex % 2) == 0 ? (packed & 0x0Fu) : (packed >> 4);
                            color.a = static_cast<std::uint8_t>(nibble * 17);
                        } else if (fourCc == dxt5) {
                            color.a = dxt5AlphaValue(block, pixelIndex);
                        }
                        const std::uint8_t alpha = (fourCc == dxt3 || fourCc == dxt5) ? color.a : luminanceAlpha(color);
                        image.pixels[static_cast<std::size_t>(sy * image.width + sx)] = {
                            color.r,
                            color.g,
                            color.b,
                            alpha
                        };
                    }
                }
            }
        }
        return image;
    }
    const std::uint32_t rgbBitCount = readU32LE(data + 88);
    const std::uint32_t rMask = readU32LE(data + 92);
    const std::uint32_t gMask = readU32LE(data + 96);
    const std::uint32_t bMask = readU32LE(data + 100);
    const std::uint32_t aMask = readU32LE(data + 104);
    if ((pfFlags & 0x40u) != 0 && rgbBitCount == 32) {
        const int required = 128 + image.width * image.height * 4;
        if (bytes.size() < required) {
            image = {};
            return image;
        }
        auto extract = [](std::uint32_t value, std::uint32_t mask) -> std::uint8_t {
            if (mask == 0) {
                return 0;
            }
            int shift = 0;
            while (((mask >> shift) & 1u) == 0u && shift < 32) {
                ++shift;
            }
            std::uint32_t componentMask = mask >> shift;
            std::uint32_t component = (value & mask) >> shift;
            if (componentMask == 0) {
                return 0;
            }
            return static_cast<std::uint8_t>((component * 255u + componentMask / 2u) / componentMask);
        };
        const std::uint8_t* pixelData = data + 128;
        for (int y = 0; y < image.height; ++y) {
            for (int x = 0; x < image.width; ++x) {
                const std::uint32_t value = readU32LE(pixelData + static_cast<std::size_t>(y * image.width + x) * 4u);
                const Rgba color{
                    extract(value, rMask),
                    extract(value, gMask),
                    extract(value, bMask),
                    aMask != 0 ? extract(value, aMask) : static_cast<std::uint8_t>(255)
                };
                image.pixels[static_cast<std::size_t>(y * image.width + x)] = {
                    color.r,
                    color.g,
                    color.b,
                    luminanceAlpha(color)
                };
            }
        }
        return image;
    }
    image = {};
    return image;
}

static std::string buildFntText(const GenerateOptions& options,
                                const std::vector<PackedGlyph>& glyphs,
                                int pageIndex,
                                int lineHeight,
                                int base) {
    std::ostringstream out;
    const std::string pageName = options.outputName + std::to_string(options.sizePx) + "_" + std::to_string(pageIndex);
    int charCount = 0;
    for (const PackedGlyph& glyph : glyphs) {
        if (glyph.page == pageIndex) {
            ++charCount;
        }
    }
    out << "info face=\"" << (options.fontName.empty() ? options.atlasName : options.fontName)
        << "\" size=" << options.sizePx
        << " bold=" << (options.bold ? 1 : 0)
        << " italic=" << (options.italic ? 1 : 0)
        << " charset=\"\" unicode=1 stretchH=" << options.heightPercent
        << " smooth=" << (options.smoothing ? 1 : 0)
        << " aa=1 padding=" << options.paddingUp << "," << options.paddingRight << "," << options.paddingDown << "," << options.paddingLeft
        << " spacing=" << options.spacingHorizontal << "," << options.spacingVertical
        << " outline=" << options.outlineThickness << "\r\n";
    out << "common lineHeight=" << lineHeight
        << " base=" << base
        << " scaleW=" << options.textureWidth
        << " scaleH=" << options.textureHeight
        << " pages=1 packed=0 alphaChnl=4 redChnl=0 greenChnl=0 blueChnl=4\r\n";
    out << "page id=0 file=\"" << pageName << ".dds\"\r\n";
    out << "chars count=" << charCount << "\r\n";
    for (const PackedGlyph& glyph : glyphs) {
        if (glyph.page != pageIndex) {
            continue;
        }
        out << "char id=" << glyph.codepoint
            << " x=" << glyph.x
            << " y=" << glyph.y
            << " width=" << glyph.width
            << " height=" << glyph.height
            << " xoffset=" << glyph.xoffset
            << " yoffset=" << glyph.yoffset
            << " xadvance=" << glyph.xadvance
            << " page=0 chnl=4\r\n";
    }
    out << "kernings count=0\r\n";
    return out.str();
}

static std::string utf8FromCodepoint(std::uint32_t cp) {
    if (cp <= 0x7Fu) {
        return std::string(1, static_cast<char>(cp));
    }
    if (cp <= 0x7FFu) {
        std::string result;
        result.push_back(static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu)));
        result.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        return result;
    }
    if (cp <= 0xFFFFu) {
        std::string result;
        result.push_back(static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu)));
        result.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        result.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        return result;
    }
    std::string result;
    result.push_back(static_cast<char>(0xF0u | ((cp >> 18) & 0x07u)));
    result.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
    result.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
    result.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    return result;
}

static std::string formatTextColorCode(std::uint32_t cp) {
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') || cp == '_') {
        return utf8FromCodepoint(cp);
    }

    std::string escaped;
    for (const char ch : utf8FromCodepoint(cp)) {
        if (ch == '"' || ch == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return "\"" + escaped + "\"";
}

static std::string buildGfxText(const GenerateOptions& options, int pageCount) {
    std::ostringstream out;
    out << "bitmapfonts = {\n";
    out << "    bitmapfont = {\n";
    out << "        name = \"" << options.registrationName << "\"\n";
    out << "        fontfiles = {\n";
    for (int i = 0; i < pageCount; ++i) {
        out << "            \"gfx/fonts/" << options.outputName << "/" << options.outputName << options.sizePx << "_" << i << "\"\n";
    }
    out << "        }\n";
    out << "        color = 0xffffffff\n";
    out << "        border_color = 0x00000000\n";
    if (!options.textColors.empty()) {
        out << "        textcolors = {\n";
        for (const auto& [codepoint, color] : options.textColors) {
            out << "            " << formatTextColorCode(codepoint)
                << " = { " << std::clamp(color.red(), 0, 255)
                << " " << std::clamp(color.green(), 0, 255)
                << " " << std::clamp(color.blue(), 0, 255)
                << " }\n";
        }
        out << "        }\n";
    }
    out << "    }\n";
    out << "}\n";
    return out.str();
}

static std::map<std::string, std::string> parseAttributes(const std::string& line) {
    std::map<std::string, std::string> attrs;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) == 0 && line[i] != '=') {
            ++i;
        }
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
            ++i;
        }
        std::size_t keyStart = i;
        while (i < line.size() && line[i] != '=' && std::isspace(static_cast<unsigned char>(line[i])) == 0) {
            ++i;
        }
        if (i >= line.size() || line[i] != '=') {
            ++i;
            continue;
        }
        const std::string key = line.substr(keyStart, i - keyStart);
        ++i;
        std::string value;
        if (i < line.size() && line[i] == '"') {
            ++i;
            const std::size_t valueStart = i;
            while (i < line.size() && line[i] != '"') {
                ++i;
            }
            value = line.substr(valueStart, i - valueStart);
            if (i < line.size()) {
                ++i;
            }
        } else {
            const std::size_t valueStart = i;
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) == 0) {
                ++i;
            }
            value = line.substr(valueStart, i - valueStart);
        }
        if (!key.empty()) {
            attrs[key] = value;
        }
    }
    return attrs;
}

static std::vector<std::uint32_t> decodeUtf8Codepoints(const QString& text) {
    std::vector<std::uint32_t> result;
    const auto ucs4 = text.toUcs4();
    result.reserve(static_cast<std::size_t>(ucs4.size()));
    for (uint cp : ucs4) {
        result.push_back(static_cast<std::uint32_t>(cp));
    }
    return result;
}

static std::string replaceExtension(std::string value, const std::string& extension) {
    std::replace(value.begin(), value.end(), '\\', '/');
    const std::size_t slashPos = value.find_last_of('/');
    const std::size_t dotPos = value.find_last_of('.');
    if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
        value.resize(dotPos);
    }
    return value + extension;
}

static std::string baseNameOf(const std::string& value) {
    const std::size_t slashPos = value.find_last_of("/\\");
    return slashPos == std::string::npos ? value : value.substr(slashPos + 1);
}

static void registerPageFileName(std::map<std::string, int>& pageFileToImageIndex, const std::string& fileName, int imageIndex) {
    if (fileName.empty()) {
        return;
    }
    const std::string baseName = baseNameOf(fileName);
    pageFileToImageIndex[fileName] = imageIndex;
    pageFileToImageIndex[baseName] = imageIndex;
    pageFileToImageIndex[replaceExtension(fileName, ".dds")] = imageIndex;
    pageFileToImageIndex[replaceExtension(baseName, ".dds")] = imageIndex;
}

static QColor colorFromJsonValue(const QJsonValue& value, const QColor& fallback) {
    const QJsonArray array = value.toArray();
    if (array.size() < 3) {
        return fallback;
    }
    return QColor(
        std::clamp(array.at(0).toInt(fallback.red()), 0, 255),
        std::clamp(array.at(1).toInt(fallback.green()), 0, 255),
        std::clamp(array.at(2).toInt(fallback.blue()), 0, 255),
        255
    );
}

static QColor defaultTextColorFromJson(const QJsonObject& object) {
    return colorFromJsonValue(object.value(QStringLiteral("defaultColor")), QColor(255, 255, 255, 255));
}

static TextColorMap textColorsFromJson(const QJsonObject& object) {
    TextColorMap colors;
    const QJsonObject textColors = object.value(QStringLiteral("textColors")).toObject();
    for (auto it = textColors.begin(); it != textColors.end(); ++it) {
        const auto codepoints = it.key().toUcs4();
        if (codepoints.isEmpty()) {
            continue;
        }
        colors[static_cast<std::uint32_t>(codepoints.front())] =
            colorFromJsonValue(it.value(), QColor(255, 255, 255, 255));
    }
    return colors;
}

static QColor textColorForCode(std::uint32_t cp, const QColor& defaultColor, const TextColorMap& textColors) {
    if (cp == '!') {
        return defaultColor;
    }
    const auto it = textColors.find(cp);
    return it == textColors.end() ? defaultColor : it->second;
}

static Rgba blendFontTexel(const Rgba& texel, const QColor& textColor) {
    return {
        static_cast<std::uint8_t>((static_cast<int>(texel.r) * textColor.red()) / 255),
        static_cast<std::uint8_t>((static_cast<int>(texel.g) * textColor.green()) / 255),
        static_cast<std::uint8_t>((static_cast<int>(texel.b) * textColor.blue()) / 255),
        texel.a
    };
}

static FontAtlas parseRenderRequest(const QJsonObject& object, QString* outText) {
    FontAtlas atlas;
    if (outText) {
        *outText = object.value(QStringLiteral("text")).toString(QStringLiteral("Preview §RRed §GGreen §!Normal"));
    }

    const QJsonArray fontFiles = object.value(QStringLiteral("fontFiles")).toArray();
    for (const QJsonValue& fileValue : fontFiles) {
        const QJsonObject fileObject = fileValue.toObject();
        const std::string fntText = fileObject.value(QStringLiteral("fntText")).toString().toStdString();
        std::map<int, int> pageToImageIndex;
        std::map<std::string, int> pageFileToImageIndex;
        const QJsonArray ddsPages = fileObject.value(QStringLiteral("ddsPages")).toArray();
        int fallbackImageIndex = -1;
        for (const QJsonValue& pageValue : ddsPages) {
            const QJsonObject pageObject = pageValue.toObject();
            const QByteArray ddsBytes = QByteArray::fromBase64(pageObject.value(QStringLiteral("base64")).toString().toLatin1());
            Image image = decodeDdsImage(ddsBytes);
            if (image.width <= 0 || image.height <= 0) {
                continue;
            }
            const int imageIndex = static_cast<int>(atlas.pages.size());
            atlas.pages.push_back(std::move(image));
            const std::string fileName = pageObject.value(QStringLiteral("file")).toString().toStdString();
            registerPageFileName(pageFileToImageIndex, fileName, imageIndex);
            if (fallbackImageIndex < 0) {
                fallbackImageIndex = imageIndex;
            }
        }

        std::istringstream stream(fntText);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.rfind("common ", 0) == 0) {
                const auto attrs = parseAttributes(line);
                if (attrs.count("lineHeight")) {
                    atlas.lineHeight = std::max(1, std::atoi(attrs.at("lineHeight").c_str()));
                }
                if (attrs.count("base")) {
                    atlas.base = std::max(1, std::atoi(attrs.at("base").c_str()));
                }
            } else if (line.rfind("page ", 0) == 0) {
                const auto attrs = parseAttributes(line);
                if (!attrs.count("id") || !attrs.count("file")) {
                    continue;
                }
                const int pageId = std::atoi(attrs.at("id").c_str());
                const std::string& fileName = attrs.at("file");
                auto imageIt = pageFileToImageIndex.find(fileName);
                if (imageIt == pageFileToImageIndex.end()) {
                    imageIt = pageFileToImageIndex.find(baseNameOf(fileName));
                }
                if (imageIt == pageFileToImageIndex.end()) {
                    imageIt = pageFileToImageIndex.find(replaceExtension(fileName, ".dds"));
                }
                if (imageIt == pageFileToImageIndex.end()) {
                    imageIt = pageFileToImageIndex.find(replaceExtension(baseNameOf(fileName), ".dds"));
                }
                if (imageIt != pageFileToImageIndex.end()) {
                    pageToImageIndex[pageId] = imageIt->second;
                } else if (fallbackImageIndex >= 0 && ddsPages.size() == 1) {
                    pageToImageIndex[pageId] = fallbackImageIndex;
                }
            } else if (line.rfind("char ", 0) == 0) {
                const auto attrs = parseAttributes(line);
                if (!attrs.count("id")) {
                    continue;
                }
                FntChar ch;
                ch.id = static_cast<std::uint32_t>(std::strtoul(attrs.at("id").c_str(), nullptr, 10));
                ch.x = attrs.count("x") ? std::atoi(attrs.at("x").c_str()) : 0;
                ch.y = attrs.count("y") ? std::atoi(attrs.at("y").c_str()) : 0;
                ch.width = attrs.count("width") ? std::atoi(attrs.at("width").c_str()) : 0;
                ch.height = attrs.count("height") ? std::atoi(attrs.at("height").c_str()) : 0;
                ch.xoffset = attrs.count("xoffset") ? std::atoi(attrs.at("xoffset").c_str()) : 0;
                ch.yoffset = attrs.count("yoffset") ? std::atoi(attrs.at("yoffset").c_str()) : 0;
                ch.xadvance = attrs.count("xadvance") ? std::atoi(attrs.at("xadvance").c_str()) : ch.width;
                const int pageId = attrs.count("page") ? std::atoi(attrs.at("page").c_str()) : 0;
                ch.page = pageToImageIndex.count(pageId) ? pageToImageIndex[pageId] : pageId;
                ch.chnl = attrs.count("chnl") ? std::atoi(attrs.at("chnl").c_str()) : 4;
                atlas.chars[ch.id] = ch;
            }
        }
    }
    return atlas;
}

static QByteArray renderTextToPng(const FontAtlas& atlas,
                                  const QString& text,
                                  const QColor& defaultColor,
                                  const TextColorMap& textColors,
                                  int* outWidth,
                                  int* outHeight) {
    if (atlas.pages.empty() || atlas.chars.empty()) {
        setError("Font atlas contains no renderable pages.");
        return {};
    }

    QColor currentColor = defaultColor;
    int cursorX = 0;
    int cursorY = 0;
    int width = 1;
    int height = std::max(1, atlas.lineHeight);
    const std::vector<std::uint32_t> cps = decodeUtf8Codepoints(text);

    for (std::size_t i = 0; i < cps.size(); ++i) {
        const std::uint32_t cp = cps[i];
        if (cp == 0x00A7u && i + 1 < cps.size()) {
            ++i;
            continue;
        }
        if (cp == '\n') {
            cursorX = 0;
            cursorY += atlas.lineHeight;
            height = std::max(height, cursorY + atlas.lineHeight);
            continue;
        }
        const auto it = atlas.chars.find(cp);
        const auto fallback = atlas.chars.find('?');
        if (it == atlas.chars.end() && fallback == atlas.chars.end()) {
            continue;
        }
        const FntChar& ch = it != atlas.chars.end() ? it->second : fallback->second;
        width = std::max(width, cursorX + ch.xoffset + ch.width);
        height = std::max(height, cursorY + ch.yoffset + ch.height);
        cursorX += ch.xadvance;
    }

    width = std::min(std::max(1, width + 8), kMaxPreviewWidth);
    height = std::min(std::max(1, height + 8), kMaxPreviewHeight);
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    cursorX = 0;
    cursorY = 0;
    currentColor = defaultColor;
    for (std::size_t i = 0; i < cps.size(); ++i) {
        const std::uint32_t cp = cps[i];
        if (cp == 0x00A7u && i + 1 < cps.size()) {
            currentColor = textColorForCode(cps[++i], defaultColor, textColors);
            continue;
        }
        if (cp == '\n') {
            cursorX = 0;
            cursorY += atlas.lineHeight;
            continue;
        }

        const auto it = atlas.chars.find(cp);
        const auto fallback = atlas.chars.find('?');
        if (it == atlas.chars.end() && fallback == atlas.chars.end()) {
            continue;
        }
        const FntChar& ch = it != atlas.chars.end() ? it->second : fallback->second;
        if (ch.page < 0 || ch.page >= static_cast<int>(atlas.pages.size())) {
            cursorX += ch.xadvance;
            continue;
        }

        const Image& page = atlas.pages[static_cast<std::size_t>(ch.page)];
        for (int y = 0; y < ch.height; ++y) {
            const int dstY = cursorY + ch.yoffset + y + 4;
            const int srcY = ch.y + y;
            if (dstY < 0 || dstY >= image.height() || srcY < 0 || srcY >= page.height) {
                continue;
            }
            std::uint8_t* dst = image.scanLine(dstY);
            for (int x = 0; x < ch.width; ++x) {
                const int dstX = cursorX + ch.xoffset + x + 4;
                const int srcX = ch.x + x;
                if (dstX < 0 || dstX >= image.width() || srcX < 0 || srcX >= page.width) {
                    continue;
                }
                const Rgba src = page.pixels[static_cast<std::size_t>(srcY * page.width + srcX)];
                const int alpha = src.a;
                if (alpha <= 0) {
                    continue;
                }
                const Rgba blended = blendFontTexel(src, currentColor);
                std::uint8_t* px = dst + dstX * 4;
                const int invAlpha = 255 - alpha;
                px[0] = static_cast<std::uint8_t>((static_cast<int>(blended.r) * alpha + px[0] * invAlpha) / 255);
                px[1] = static_cast<std::uint8_t>((static_cast<int>(blended.g) * alpha + px[1] * invAlpha) / 255);
                px[2] = static_cast<std::uint8_t>((static_cast<int>(blended.b) * alpha + px[2] * invAlpha) / 255);
                px[3] = static_cast<std::uint8_t>(std::min(255, alpha + px[3] * invAlpha / 255));
            }
        }
        cursorX += ch.xadvance;
    }

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        setError("Failed to encode rendered preview as PNG.");
        return {};
    }
    if (outWidth) {
        *outWidth = image.width();
    }
    if (outHeight) {
        *outHeight = image.height();
    }
    return pngBytes;
}

static std::vector<std::uint32_t> previewCodepointsForText(const QString& text) {
    std::set<std::uint32_t> uniqueCodepoints;
    const std::vector<std::uint32_t> cps = decodeUtf8Codepoints(text);
    for (std::size_t i = 0; i < cps.size(); ++i) {
        const std::uint32_t cp = cps[i];
        if (cp == 0x00A7u && i + 1 < cps.size()) {
            ++i;
            continue;
        }
        if (cp == '\n' || cp < 32 || cp == 0x7Fu || cp > 0xFFFFu) {
            continue;
        }
        uniqueCodepoints.insert(cp);
    }
    uniqueCodepoints.insert(32);
    uniqueCodepoints.insert(63);
    return std::vector<std::uint32_t>(uniqueCodepoints.begin(), uniqueCodepoints.end());
}

static QByteArray renderTtfPreviewToPng(const std::uint8_t* data,
                                        int size,
                                        const QJsonObject& object,
                                        int* outWidth,
                                        int* outHeight) {
    GenerateOptions options = optionsFromJson(object);
    QString text = object.value(QStringLiteral("text")).toString(QStringLiteral("Preview §RRed §GGreen §!Normal"));
    if (text.isEmpty()) {
        text = QStringLiteral("Preview");
    }

    int lineHeight = options.sizePx;
    int base = std::max(1, options.sizePx - 3);
    std::vector<GlyphBitmap> glyphs = rasterizeTtfCodepoints(
        data,
        size,
        options,
        previewCodepointsForText(text),
        &lineHeight,
        &base
    );
    if (glyphs.empty()) {
        if (g_errorBuffer.isEmpty()) {
            setError("No glyphs were rasterized for the font preview.");
        }
        return {};
    }

    std::map<std::uint32_t, std::size_t> glyphByCodepoint;
    for (std::size_t i = 0; i < glyphs.size(); ++i) {
        glyphByCodepoint[glyphs[i].codepoint] = i;
    }

    auto findGlyph = [&](std::uint32_t cp) -> const GlyphBitmap* {
        auto it = glyphByCodepoint.find(cp);
        if (it != glyphByCodepoint.end()) {
            return &glyphs[it->second];
        }
        it = glyphByCodepoint.find('?');
        return it != glyphByCodepoint.end() ? &glyphs[it->second] : nullptr;
    };

    int cursorX = 0;
    int cursorY = 0;
    int width = 1;
    int height = std::max(1, lineHeight);
    const std::vector<std::uint32_t> cps = decodeUtf8Codepoints(text);
    for (std::size_t i = 0; i < cps.size(); ++i) {
        const std::uint32_t cp = cps[i];
        if (cp == 0x00A7u && i + 1 < cps.size()) {
            ++i;
            continue;
        }
        if (cp == '\n') {
            cursorX = 0;
            cursorY += lineHeight;
            height = std::max(height, cursorY + lineHeight);
            continue;
        }
        const GlyphBitmap* glyph = findGlyph(cp);
        if (!glyph) {
            continue;
        }
        width = std::max(width, cursorX + glyph->xoffset + glyph->width);
        height = std::max(height, cursorY + glyph->yoffset + glyph->height);
        cursorX += glyph->xadvance;
    }

    width = std::min(std::max(1, width + 8), kMaxPreviewWidth);
    height = std::min(std::max(1, height + 8), kMaxPreviewHeight);
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    const QColor defaultColor = defaultTextColorFromJson(object);
    const TextColorMap textColors = textColorsFromJson(object);
    QColor currentColor = defaultColor;
    cursorX = 0;
    cursorY = 0;
    for (std::size_t i = 0; i < cps.size(); ++i) {
        const std::uint32_t cp = cps[i];
        if (cp == 0x00A7u && i + 1 < cps.size()) {
            currentColor = textColorForCode(cps[++i], defaultColor, textColors);
            continue;
        }
        if (cp == '\n') {
            cursorX = 0;
            cursorY += lineHeight;
            continue;
        }
        const GlyphBitmap* glyph = findGlyph(cp);
        if (!glyph) {
            continue;
        }
        for (int y = 0; y < glyph->height; ++y) {
            const int dstY = cursorY + glyph->yoffset + y + 4;
            if (dstY < 0 || dstY >= image.height()) {
                continue;
            }
            std::uint8_t* dst = image.scanLine(dstY);
            for (int x = 0; x < glyph->width; ++x) {
                const int dstX = cursorX + glyph->xoffset + x + 4;
                if (dstX < 0 || dstX >= image.width()) {
                    continue;
                }
                const int alpha = glyph->alpha[static_cast<std::size_t>(y * glyph->width + x)];
                if (alpha <= 0) {
                    continue;
                }
                std::uint8_t* px = dst + dstX * 4;
                const int invAlpha = 255 - alpha;
                px[0] = static_cast<std::uint8_t>((currentColor.red() * alpha + px[0] * invAlpha) / 255);
                px[1] = static_cast<std::uint8_t>((currentColor.green() * alpha + px[1] * invAlpha) / 255);
                px[2] = static_cast<std::uint8_t>((currentColor.blue() * alpha + px[2] * invAlpha) / 255);
                px[3] = static_cast<std::uint8_t>(std::min(255, alpha + px[3] * invAlpha / 255));
            }
        }
        cursorX += glyph->xadvance;
    }

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        setError("Failed to encode rendered TTF preview as PNG.");
        return {};
    }
    if (outWidth) {
        *outWidth = image.width();
    }
    if (outHeight) {
        *outHeight = image.height();
    }
    return pngBytes;
}

} // namespace

APE_DRYAD_EXPORT const char* APE_DryadAtlas_GetPluginName() {
    return "DryadAtlas";
}

APE_DRYAD_EXPORT const char* APE_DryadAtlas_GetLastError() {
    return g_errorBuffer.constData();
}

APE_DRYAD_EXPORT void APE_DryadAtlas_FreeString(const char* value) {
    std::free(const_cast<char*>(value));
}

APE_DRYAD_EXPORT void APE_DryadAtlas_FreeBytes(unsigned char* value) {
    std::free(value);
}

APE_DRYAD_EXPORT int APE_DryadAtlas_GetTtfInfo(const unsigned char* ttfData, int ttfSize, char** outJsonUtf8) {
    if (!ttfData || ttfSize <= 0 || !outJsonUtf8) {
        setError("Invalid TTF info arguments.");
        return 0;
    }
    QJsonObject result;
    const std::string family = readTtfFamilyName(ttfData, ttfSize);
    const std::vector<std::uint32_t> codepoints = enumerateTtfCodepoints(ttfData, ttfSize);
    result[QStringLiteral("family")] = QString::fromStdString(family);
    result[QStringLiteral("glyphCount")] = static_cast<int>(codepoints.size());
    const QByteArray json = QJsonDocument(result).toJson(QJsonDocument::Compact);
    *outJsonUtf8 = allocateCString(json);
    clearError();
    return *outJsonUtf8 ? 1 : 0;
}

APE_DRYAD_EXPORT int APE_DryadAtlas_GenerateFromTtf(
    const unsigned char* ttfData,
    int ttfSize,
    const char* optionsJsonUtf8,
    char** outJsonUtf8
) {
    if (!ttfData || ttfSize <= 0 || !optionsJsonUtf8 || !outJsonUtf8) {
        setError("Invalid font generation arguments.");
        return 0;
    }

    const QJsonDocument optionsDoc = QJsonDocument::fromJson(QByteArray(optionsJsonUtf8));
    if (!optionsDoc.isObject()) {
        setError("Font generation options must be a JSON object.");
        return 0;
    }

    GenerateOptions options = optionsFromJson(optionsDoc.object());
    int lineHeight = options.sizePx;
    int base = std::max(1, options.sizePx - 3);
    std::vector<GlyphBitmap> glyphs = rasterizeTtfGlyphs(ttfData, ttfSize, options, &lineHeight, &base);
    if (glyphs.empty()) {
        if (g_errorBuffer.isEmpty()) {
            setError("No glyphs were rasterized from the imported TTF.");
        }
        return 0;
    }

    std::vector<PackedGlyph> packedGlyphs;
    std::vector<Image> pages = packGlyphs(glyphs, options, packedGlyphs);
    if (pages.empty() || packedGlyphs.empty()) {
        setError("Failed to pack glyphs into font atlas pages.");
        return 0;
    }

    QJsonObject result;
    result[QStringLiteral("success")] = true;
    result[QStringLiteral("atlasName")] = QString::fromStdString(options.atlasName);
    result[QStringLiteral("outputName")] = QString::fromStdString(options.outputName);
    result[QStringLiteral("registrationName")] = QString::fromStdString(options.registrationName);
    result[QStringLiteral("fontName")] = QString::fromStdString(options.fontName);
    result[QStringLiteral("glyphCount")] = static_cast<int>(packedGlyphs.size());
    result[QStringLiteral("pageCount")] = static_cast<int>(pages.size());
    result[QStringLiteral("lineHeight")] = lineHeight;
    result[QStringLiteral("base")] = base;

    QJsonArray fntFiles;
    QJsonArray ddsFiles;
    std::vector<std::future<std::vector<std::uint8_t>>> ddsFutures;
    ddsFutures.reserve(pages.size());
    for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
        ddsFutures.push_back(std::async(std::launch::async, [&pages, i]() {
            return encodeDdsDxt5(pages[static_cast<std::size_t>(i)]);
        }));
    }
    for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
        const std::string baseName = options.outputName + std::to_string(options.sizePx) + "_" + std::to_string(i);
        const std::string logicalBase = "gfx/fonts/" + options.outputName + "/" + baseName;
        const std::string fntText = buildFntText(options, packedGlyphs, i, lineHeight, base);
        const std::vector<std::uint8_t> ddsBytes = ddsFutures[static_cast<std::size_t>(i)].get();

        QJsonObject fnt;
        fnt[QStringLiteral("path")] = QString::fromStdString(logicalBase + ".fnt");
        fnt[QStringLiteral("basePath")] = QString::fromStdString(logicalBase);
        fnt[QStringLiteral("contentBase64")] = QString::fromLatin1(QByteArray(fntText.data(), static_cast<int>(fntText.size())).toBase64());
        fntFiles.append(fnt);

        QJsonObject dds;
        dds[QStringLiteral("path")] = QString::fromStdString(logicalBase + ".dds");
        dds[QStringLiteral("contentBase64")] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(ddsBytes.data()), static_cast<int>(ddsBytes.size())).toBase64());
        ddsFiles.append(dds);
    }

    const std::string gfxText = buildGfxText(options, static_cast<int>(pages.size()));
    result[QStringLiteral("fntFiles")] = fntFiles;
    result[QStringLiteral("ddsFiles")] = ddsFiles;
    result[QStringLiteral("gfxPath")] = QString::fromStdString("interface/fonts/" + options.outputName + ".gfx");
    result[QStringLiteral("gfxContentBase64")] = QString::fromLatin1(QByteArray(gfxText.data(), static_cast<int>(gfxText.size())).toBase64());

    const QByteArray json = QJsonDocument(result).toJson(QJsonDocument::Compact);
    *outJsonUtf8 = allocateCString(json);
    if (!*outJsonUtf8) {
        setError("Failed to allocate font generation result.");
        return 0;
    }
    clearError();
    return 1;
}

APE_DRYAD_EXPORT int APE_DryadAtlas_RenderTtfPreview(
    const unsigned char* ttfData,
    int ttfSize,
    const char* requestJsonUtf8,
    unsigned char** outPngBytes,
    int* outPngSize,
    int* outWidth,
    int* outHeight
) {
    if (!ttfData || ttfSize <= 0 || !requestJsonUtf8 || !outPngBytes || !outPngSize || !outWidth || !outHeight) {
        setError("Invalid TTF preview arguments.");
        return 0;
    }
    const QJsonDocument requestDoc = QJsonDocument::fromJson(QByteArray(requestJsonUtf8));
    if (!requestDoc.isObject()) {
        setError("TTF preview request must be a JSON object.");
        return 0;
    }

    QByteArray pngBytes = renderTtfPreviewToPng(ttfData, ttfSize, requestDoc.object(), outWidth, outHeight);
    if (pngBytes.isEmpty()) {
        return 0;
    }

    unsigned char* bytes = static_cast<unsigned char*>(std::malloc(static_cast<std::size_t>(pngBytes.size())));
    if (!bytes) {
        setError("Failed to allocate TTF preview PNG bytes.");
        return 0;
    }
    std::memcpy(bytes, pngBytes.constData(), static_cast<std::size_t>(pngBytes.size()));
    *outPngBytes = bytes;
    *outPngSize = pngBytes.size();
    clearError();
    return 1;
}

APE_DRYAD_EXPORT int APE_DryadAtlas_RenderText(
    const char* requestJsonUtf8,
    unsigned char** outPngBytes,
    int* outPngSize,
    int* outWidth,
    int* outHeight
) {
    if (!requestJsonUtf8 || !outPngBytes || !outPngSize || !outWidth || !outHeight) {
        setError("Invalid render arguments.");
        return 0;
    }
    const QJsonDocument requestDoc = QJsonDocument::fromJson(QByteArray(requestJsonUtf8));
    if (!requestDoc.isObject()) {
        setError("Render request must be a JSON object.");
        return 0;
    }

    const QJsonObject requestObject = requestDoc.object();
    const QString text = requestObject.value(QStringLiteral("text")).toString(QStringLiteral("Preview §RRed §GGreen §!Normal"));
    const QByteArray fontFilesKey = QJsonDocument(requestObject.value(QStringLiteral("fontFiles")).toArray()).toJson(QJsonDocument::Compact);
    std::shared_ptr<const FontAtlas> atlas;
    {
        std::lock_guard<std::mutex> lock(g_renderCacheMutex);
        if (g_renderCacheAtlas && g_renderCacheKey == fontFilesKey) {
            atlas = g_renderCacheAtlas;
        }
    }
    if (!atlas) {
        FontAtlas parsedAtlas = parseRenderRequest(requestObject, nullptr);
        auto parsedAtlasPtr = std::make_shared<FontAtlas>(std::move(parsedAtlas));
        {
            std::lock_guard<std::mutex> lock(g_renderCacheMutex);
            g_renderCacheKey = fontFilesKey;
            g_renderCacheAtlas = parsedAtlasPtr;
        }
        atlas = std::move(parsedAtlasPtr);
    }

    const QColor defaultColor = defaultTextColorFromJson(requestObject);
    const TextColorMap textColors = textColorsFromJson(requestObject);
    QByteArray pngBytes = renderTextToPng(*atlas, text, defaultColor, textColors, outWidth, outHeight);
    if (pngBytes.isEmpty()) {
        return 0;
    }

    unsigned char* bytes = static_cast<unsigned char*>(std::malloc(static_cast<std::size_t>(pngBytes.size())));
    if (!bytes) {
        setError("Failed to allocate preview PNG bytes.");
        return 0;
    }
    std::memcpy(bytes, pngBytes.constData(), static_cast<std::size_t>(pngBytes.size()));
    *outPngBytes = bytes;
    *outPngSize = pngBytes.size();
    clearError();
    return 1;
}

namespace {

void clearAbiResponse(ApePluginAbiResponse* response) {
    if (!response) {
        return;
    }
    response->abiVersion = APE_PLUGIN_ABI_VERSION;
    response->status = APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR;
    response->contentType = APE_PLUGIN_ABI_CONTENT_NONE;
    response->flags = 0;
    response->payload = nullptr;
    response->payloadSize = 0;
    response->errorUtf8 = nullptr;
}

char* allocateAbiString(const QByteArray& text) {
    char* data = static_cast<char*>(std::malloc(static_cast<std::size_t>(text.size() + 1)));
    if (!data) {
        return nullptr;
    }
    std::memcpy(data, text.constData(), static_cast<std::size_t>(text.size()));
    data[text.size()] = '\0';
    return data;
}

bool setAbiPayload(ApePluginAbiResponse* response, const QByteArray& payload, std::uint32_t contentType) {
    if (!response) {
        return false;
    }
    response->contentType = contentType;
    response->payloadSize = static_cast<std::uint64_t>(payload.size());
    if (payload.isEmpty()) {
        response->payload = nullptr;
        return true;
    }
    auto* bytes = static_cast<std::uint8_t*>(std::malloc(static_cast<std::size_t>(payload.size())));
    if (!bytes) {
        return false;
    }
    std::memcpy(bytes, payload.constData(), static_cast<std::size_t>(payload.size()));
    response->payload = bytes;
    return true;
}

void setAbiError(ApePluginAbiResponse* response, std::uint32_t status, const QString& message) {
    if (!response) {
        return;
    }
    response->status = status;
    response->errorUtf8 = allocateAbiString(message.toUtf8());
}

QJsonObject requestObjectFromAbi(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    const QByteArray payload(
        reinterpret_cast<const char*>(request->payload.data),
        static_cast<int>(std::min<std::uint64_t>(request->payload.size, static_cast<std::uint64_t>(std::numeric_limits<int>::max())))
    );
    const QJsonDocument document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, QStringLiteral("DryadAtlas request must be a JSON object."));
        return {};
    }
    return document.object();
}

int invokeDryadGetTtfInfo(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    const QJsonObject object = requestObjectFromAbi(request, response);
    if (response->errorUtf8) {
        return 1;
    }
    const QByteArray ttfBytes = QByteArray::fromBase64(object.value(QStringLiteral("ttfBase64")).toString().toLatin1());
    char* jsonText = nullptr;
    if (!APE_DryadAtlas_GetTtfInfo(
            reinterpret_cast<const unsigned char*>(ttfBytes.constData()),
            ttfBytes.size(),
            &jsonText) || !jsonText) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_PLUGIN_ERROR, QString::fromUtf8(APE_DryadAtlas_GetLastError()));
        return 1;
    }
    const QByteArray payload(jsonText);
    APE_DryadAtlas_FreeString(jsonText);
    if (!setAbiPayload(response, payload, APE_PLUGIN_ABI_CONTENT_JSON_UTF8)) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR, QStringLiteral("Failed to allocate DryadAtlas response."));
        return 1;
    }
    response->status = APE_PLUGIN_ABI_STATUS_OK;
    return 0;
}

int invokeDryadGenerateFromTtf(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    QJsonObject object = requestObjectFromAbi(request, response);
    if (response->errorUtf8) {
        return 1;
    }
    const QByteArray ttfBytes = QByteArray::fromBase64(object.take(QStringLiteral("ttfBase64")).toString().toLatin1());
    const QByteArray optionsJson = QJsonDocument(object).toJson(QJsonDocument::Compact);
    char* jsonText = nullptr;
    if (!APE_DryadAtlas_GenerateFromTtf(
            reinterpret_cast<const unsigned char*>(ttfBytes.constData()),
            ttfBytes.size(),
            optionsJson.constData(),
            &jsonText) || !jsonText) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_PLUGIN_ERROR, QString::fromUtf8(APE_DryadAtlas_GetLastError()));
        return 1;
    }
    const QByteArray payload(jsonText);
    APE_DryadAtlas_FreeString(jsonText);
    if (!setAbiPayload(response, payload, APE_PLUGIN_ABI_CONTENT_JSON_UTF8)) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR, QStringLiteral("Failed to allocate DryadAtlas response."));
        return 1;
    }
    response->status = APE_PLUGIN_ABI_STATUS_OK;
    return 0;
}

int invokeDryadRenderTtfPreview(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    QJsonObject object = requestObjectFromAbi(request, response);
    if (response->errorUtf8) {
        return 1;
    }
    const QByteArray ttfBytes = QByteArray::fromBase64(object.take(QStringLiteral("ttfBase64")).toString().toLatin1());
    const QByteArray requestJson = QJsonDocument(object).toJson(QJsonDocument::Compact);
    unsigned char* pngBytes = nullptr;
    int pngSize = 0;
    int width = 0;
    int height = 0;
    if (!APE_DryadAtlas_RenderTtfPreview(
            reinterpret_cast<const unsigned char*>(ttfBytes.constData()),
            ttfBytes.size(),
            requestJson.constData(),
            &pngBytes,
            &pngSize,
            &width,
            &height) || !pngBytes || pngSize <= 0) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_PLUGIN_ERROR, QString::fromUtf8(APE_DryadAtlas_GetLastError()));
        return 1;
    }
    QJsonObject result;
    result[QStringLiteral("pngBase64")] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(pngBytes), pngSize).toBase64());
    result[QStringLiteral("width")] = width;
    result[QStringLiteral("height")] = height;
    APE_DryadAtlas_FreeBytes(pngBytes);
    if (!setAbiPayload(response, QJsonDocument(result).toJson(QJsonDocument::Compact), APE_PLUGIN_ABI_CONTENT_JSON_UTF8)) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR, QStringLiteral("Failed to allocate DryadAtlas response."));
        return 1;
    }
    response->status = APE_PLUGIN_ABI_STATUS_OK;
    return 0;
}

int invokeDryadRenderText(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    const QJsonObject object = requestObjectFromAbi(request, response);
    if (response->errorUtf8) {
        return 1;
    }
    const QByteArray requestJson = QJsonDocument(object).toJson(QJsonDocument::Compact);
    unsigned char* pngBytes = nullptr;
    int pngSize = 0;
    int width = 0;
    int height = 0;
    if (!APE_DryadAtlas_RenderText(requestJson.constData(), &pngBytes, &pngSize, &width, &height) || !pngBytes || pngSize <= 0) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_PLUGIN_ERROR, QString::fromUtf8(APE_DryadAtlas_GetLastError()));
        return 1;
    }
    QJsonObject result;
    result[QStringLiteral("pngBase64")] = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(pngBytes), pngSize).toBase64());
    result[QStringLiteral("width")] = width;
    result[QStringLiteral("height")] = height;
    APE_DryadAtlas_FreeBytes(pngBytes);
    if (!setAbiPayload(response, QJsonDocument(result).toJson(QJsonDocument::Compact), APE_PLUGIN_ABI_CONTENT_JSON_UTF8)) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR, QStringLiteral("Failed to allocate DryadAtlas response."));
        return 1;
    }
    response->status = APE_PLUGIN_ABI_STATUS_OK;
    return 0;
}

} // namespace

APE_PLUGIN_ABI_EXPORT const char* APE_Plugin_GetName(void) {
    return "DryadAtlas";
}

APE_PLUGIN_ABI_EXPORT std::uint32_t APE_Plugin_GetAbiVersion(void) {
    return APE_PLUGIN_ABI_VERSION;
}

APE_PLUGIN_ABI_EXPORT void APE_Plugin_FreeResponse(ApePluginAbiResponse* response) {
    if (!response) {
        return;
    }
    std::free(response->payload);
    std::free(response->errorUtf8);
    clearAbiResponse(response);
}

APE_PLUGIN_ABI_EXPORT int APE_Plugin_Invoke(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    clearAbiResponse(response);
    if (!request || !response || !request->operationUtf8) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, QStringLiteral("Invalid DryadAtlas ABI request."));
        return 1;
    }
    if (request->abiVersion != APE_PLUGIN_ABI_VERSION) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_UNSUPPORTED_ABI, QStringLiteral("Unsupported DryadAtlas ABI version."));
        return 1;
    }

    const QString operation = QString::fromUtf8(request->operationUtf8);
    if (operation == QStringLiteral("dryadAtlas.getTtfInfo")) {
        return invokeDryadGetTtfInfo(request, response);
    }
    if (operation == QStringLiteral("dryadAtlas.generateFromTtf")) {
        return invokeDryadGenerateFromTtf(request, response);
    }
    if (operation == QStringLiteral("dryadAtlas.renderTtfPreview")) {
        return invokeDryadRenderTtfPreview(request, response);
    }
    if (operation == QStringLiteral("dryadAtlas.renderText")) {
        return invokeDryadRenderText(request, response);
    }

    setAbiError(response, APE_PLUGIN_ABI_STATUS_UNSUPPORTED_OPERATION, QStringLiteral("Unsupported DryadAtlas operation."));
    return 1;
}
