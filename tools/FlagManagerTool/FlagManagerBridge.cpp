//-------------------------------------------------------------------------------------
// FlagManagerBridge.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "FlagManagerBridge.h"

#include "../../src/ToolRuntimeContext.h"

#include <QByteArray>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSize>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <utility>
#include <vector>

namespace FlagManagerBridge {

std::unique_ptr<WorkerSession> g_legacySession;
std::string g_legacyLastError;
std::string g_legacySerializedState;

namespace {
using FlagManager::EffectiveFileEntry;
using FlagManager::EffectiveFileListResult;
using FlagManager::FileReadResult;
using FlagManager::FileWriteResult;
using FlagManager::FlagImage;
using FlagManager::FlagManagerCore;
using FlagManager::FlagStatus;
using FlagManager::IFileSystem;
using FlagManager::IImagePipeline;
using FlagManager::ImportedImage;
using FlagManager::ImportItem;
using FlagManager::ManageVariantDisplay;
using FlagManager::Rect;
using FlagManager::Snapshot;
using FlagManager::TagListRow;
using FlagManager::TagRecord;
using FlagManager::ToolMode;

constexpr int kSmallFlagIconWidth = 10;
constexpr int kSmallFlagIconHeight = 7;
constexpr int kImportThumbnailWidth = 32;
constexpr int kImportThumbnailHeight = 32;
constexpr int kImportPreviewMaxDimension = 1024;
constexpr int kPreviewCacheEntryLimit = 512;
constexpr int kImportThumbnailCacheEntryLimit = 256;

enum LumorphaPixelFormatBridge {
    APE_LUMORPHA_PIXEL_RGBA8 = 1
};

enum LumorphaImageFormatBridge {
    APE_LUMORPHA_FORMAT_AUTO = 0,
    APE_LUMORPHA_FORMAT_PNG = 1,
    APE_LUMORPHA_FORMAT_JPEG = 2,
    APE_LUMORPHA_FORMAT_BMP = 3,
    APE_LUMORPHA_FORMAT_TIFF = 4,
    APE_LUMORPHA_FORMAT_GIF = 5,
    APE_LUMORPHA_FORMAT_TGA = 6,
    APE_LUMORPHA_FORMAT_DDS = 7,
    APE_LUMORPHA_FORMAT_HDR = 8
};

enum LumorphaResizeFilterBridge {
    APE_LUMORPHA_FILTER_AUTO = 0,
    APE_LUMORPHA_FILTER_NEAREST = 1,
    APE_LUMORPHA_FILTER_LINEAR = 2,
    APE_LUMORPHA_FILTER_CUBIC = 3,
    APE_LUMORPHA_FILTER_BOX = 4,
    APE_LUMORPHA_FILTER_TRIANGLE = 5,
    APE_LUMORPHA_FILTER_LANCZOS3 = 6
};

QString normalizeRuntimePath(QString value) {
    value.replace('\\', '/');
    value = QDir::cleanPath(value.trimmed());
    return value == QStringLiteral(".") ? QString() : value;
}

std::string toStdString(const QString& value) {
    return value.toUtf8().toStdString();
}

QString fromStdString(const std::string& value) {
    return QString::fromUtf8(value.c_str());
}

QString localizedFallback(const QString& key) {
    static QMap<QString, QString> fallbacks;
    if (fallbacks.isEmpty()) {
        fallbacks.insert(QStringLiteral("Name"), QStringLiteral("Flag Manager"));
        fallbacks.insert(QStringLiteral("Description"), QStringLiteral("Manage and create flags."));
        fallbacks.insert(QStringLiteral("TabManage"), QStringLiteral("Manage"));
        fallbacks.insert(QStringLiteral("TabNew"), QStringLiteral("Create"));
        fallbacks.insert(QStringLiteral("Tags"), QStringLiteral("TAG"));
        fallbacks.insert(QStringLiteral("Files"), QStringLiteral("Files"));
        fallbacks.insert(QStringLiteral("FlagName"), QStringLiteral("Flag Name:"));
        fallbacks.insert(QStringLiteral("Crop"), QStringLiteral("Crop:"));
        fallbacks.insert(QStringLiteral("Export"), QStringLiteral("Export Current"));
        fallbacks.insert(QStringLiteral("ExportAll"), QStringLiteral("Export All"));
        fallbacks.insert(QStringLiteral("ImportFiles"), QStringLiteral("Import Files"));
        fallbacks.insert(QStringLiteral("BrowserPlaceholder"), QStringLiteral("Select a TAG to view flags."));
        fallbacks.insert(QStringLiteral("NoImage"), QStringLiteral("No Image"));
        fallbacks.insert(QStringLiteral("Missing"), QStringLiteral("MISSING"));
        fallbacks.insert(QStringLiteral("ColFlagName"), QStringLiteral("Flag Name"));
        fallbacks.insert(QStringLiteral("ColFileName"), QStringLiteral("File Name"));
        fallbacks.insert(QStringLiteral("L"), QStringLiteral("L"));
        fallbacks.insert(QStringLiteral("T"), QStringLiteral("T"));
        fallbacks.insert(QStringLiteral("R"), QStringLiteral("R"));
        fallbacks.insert(QStringLiteral("B"), QStringLiteral("B"));
        fallbacks.insert(QStringLiteral("SizeLarge"), QStringLiteral("Large"));
        fallbacks.insert(QStringLiteral("SizeMedium"), QStringLiteral("Medium"));
        fallbacks.insert(QStringLiteral("SizeSmall"), QStringLiteral("Small"));
        fallbacks.insert(QStringLiteral("RemoveFromList"), QStringLiteral("Remove from List"));
        fallbacks.insert(QStringLiteral("FillName"), QStringLiteral("Fill Name from File"));
        fallbacks.insert(QStringLiteral("ConfirmOverwriteTitle"), QStringLiteral("Confirm Overwrite"));
        fallbacks.insert(QStringLiteral("ConfirmOverwrite"), QStringLiteral("The following files already exist. Do you want to overwrite them?\n\n%1"));
        fallbacks.insert(QStringLiteral("SelectAll"), QStringLiteral("Select All"));
        fallbacks.insert(QStringLiteral("DeselectAll"), QStringLiteral("Deselect All"));
        fallbacks.insert(QStringLiteral("Ready"), QStringLiteral("Ready"));
        fallbacks.insert(QStringLiteral("NoFlags"), QStringLiteral("No flags to display."));
    }
    return fallbacks.value(key, key);
}

QString localizedString(const WorkerSession* session, const QString& key) {
    if (!session) {
        return localizedFallback(key);
    }
    return session->localizedStrings.value(key, localizedFallback(key));
}

QMap<QString, QString> parseMetaFile(const QString& filePath) {
    QMap<QString, QString> parsed;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return parsed;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#endif
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(QStringLiteral("//"))) {
            continue;
        }

        const int equalsIndex = line.indexOf('=');
        if (equalsIndex <= 0) {
            continue;
        }

        const QString key = line.left(equalsIndex).trimmed();
        QString value = line.mid(equalsIndex + 1).trimmed();
        if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\''))) {
            value = value.mid(1, value.length() - 2);
        }
        parsed.insert(key, value);
    }
    return parsed;
}

QString resolveLanguageCode(const QString& localisationRoot, const QString& requestedValue) {
    const QString normalized = requestedValue.trimmed();
    QDir root(localisationRoot);
    if (!normalized.isEmpty() && root.exists(normalized)) {
        return normalized;
    }

    const QStringList languageDirectories = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& dirName : languageDirectories) {
        const QMap<QString, QString> meta = parseMetaFile(root.filePath(dirName + QStringLiteral("/meta.htsl")));
        if (normalized == meta.value(QStringLiteral("lang")) || normalized == meta.value(QStringLiteral("text"))) {
            return dirName;
        }
    }

    if (root.exists(QStringLiteral("en_US"))) {
        return QStringLiteral("en_US");
    }
    return languageDirectories.isEmpty() ? QStringLiteral("en_US") : languageDirectories.first();
}

bool loadLocalizedStringsFromJson(WorkerSession* session, const QJsonObject& localizedStringsObject) {
    if (!session || localizedStringsObject.isEmpty()) {
        return false;
    }

    session->localizedStrings.clear();
    for (auto iterator = localizedStringsObject.begin(); iterator != localizedStringsObject.end(); ++iterator) {
        if (iterator->isString()) {
            session->localizedStrings.insert(iterator.key(), iterator->toString());
        }
    }
    return !session->localizedStrings.isEmpty();
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

std::vector<std::string> loadTagsFromPluginRuntime() {
    std::vector<std::string> tags;

    const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
        QStringLiteral("APEHOI4Parser"),
        QStringLiteral("hoi4Parser.listCountryTags"),
        ToolRuntimeContext::PluginPayloadContentType::JsonUtf8,
        QByteArray()
    );
    if (!response.success) {
        return tags;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(response.payload);
    if (!doc.isObject()) {
        return tags;
    }

    const QJsonArray entries = doc.object().value(QStringLiteral("entries")).toArray();
    tags.reserve(static_cast<std::size_t>(entries.size()));
    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();
        if (entry.value(QStringLiteral("isDynamic")).toBool(false)) {
            continue;
        }
        const QString tag = entry.value(QStringLiteral("tag")).toString().trimmed();
        if (!tag.isEmpty()) {
            tags.push_back(tag.toUtf8().toStdString());
        }
    }

    return tags;
}

bool ensureCoreInitialized(WorkerSession* session) {
    if (!session) {
        return false;
    }
    if (session->coreInitialized) {
        return true;
    }

    session->core.setKnownTags(loadTagsFromPluginRuntime());
    if (!session->core.initialize()) {
        setSessionError(session, fromStdString(session->core.lastError()));
        return false;
    }

    session->coreInitialized = true;
    clearSessionError(session);
    return true;
}

QString stableSha1Hex(const QString& value) {
    return QString::fromLatin1(
        QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha1).toHex()
    );
}

QSize boundedPreviewSize(const FlagImage& image, int maxDimension) {
    if (!FlagManager::isValidImage(image) || maxDimension <= 0) {
        return {};
    }
    if (image.width <= maxDimension && image.height <= maxDimension) {
        return {};
    }
    return QSize(image.width, image.height).scaled(QSize(maxDimension, maxDimension), Qt::KeepAspectRatio);
}

void appendU32(QByteArray* payload, std::uint32_t value) {
    if (!payload) {
        return;
    }
    payload->append(reinterpret_cast<const char*>(&value), static_cast<int>(sizeof(value)));
}

bool readU32(const unsigned char*& cursor, const unsigned char* end, std::uint32_t* outValue) {
    if (!outValue || !cursor || !end || end < cursor || static_cast<std::size_t>(end - cursor) < sizeof(std::uint32_t)) {
        return false;
    }
    std::uint32_t value = 0;
    std::memcpy(&value, cursor, sizeof(value));
    cursor += sizeof(value);
    *outValue = value;
    return true;
}

QByteArray imageEnvelopeFromFlagImage(const FlagImage& image) {
    if (!FlagManager::isValidImage(image)) {
        return {};
    }

    const std::uint64_t pixelCount = static_cast<std::uint64_t>(image.width) * static_cast<std::uint64_t>(image.height);
    const std::uint64_t byteSize = pixelCount * 4ULL;
    if (byteSize > std::numeric_limits<std::uint32_t>::max()
        || byteSize > static_cast<std::uint64_t>(std::numeric_limits<int>::max() - 20)) {
        return {};
    }

    QByteArray rgbaBytes;
    rgbaBytes.resize(static_cast<int>(byteSize));
    auto* output = reinterpret_cast<unsigned char*>(rgbaBytes.data());
    for (std::uint64_t i = 0; i < pixelCount; ++i) {
        const std::uint32_t pixel = image.pixels[static_cast<std::size_t>(i)];
        output[i * 4ULL + 0ULL] = FlagManager::rgbaRed(pixel);
        output[i * 4ULL + 1ULL] = FlagManager::rgbaGreen(pixel);
        output[i * 4ULL + 2ULL] = FlagManager::rgbaBlue(pixel);
        output[i * 4ULL + 3ULL] = FlagManager::rgbaAlpha(pixel);
    }

    QByteArray payload;
    payload.reserve(20 + rgbaBytes.size());
    appendU32(&payload, static_cast<std::uint32_t>(image.width));
    appendU32(&payload, static_cast<std::uint32_t>(image.height));
    appendU32(&payload, static_cast<std::uint32_t>(image.width * 4));
    appendU32(&payload, APE_LUMORPHA_PIXEL_RGBA8);
    appendU32(&payload, static_cast<std::uint32_t>(byteSize));
    payload.append(rgbaBytes);
    return payload;
}

bool flagImageFromEnvelope(const QByteArray& payload, FlagImage* outImage) {
    if (!outImage) {
        return false;
    }
    *outImage = {};

    const auto* cursor = reinterpret_cast<const unsigned char*>(payload.constData());
    const auto* end = cursor ? cursor + payload.size() : nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    std::uint32_t pixelFormat = 0;
    std::uint32_t byteSize = 0;
    if (!readU32(cursor, end, &width)
        || !readU32(cursor, end, &height)
        || !readU32(cursor, end, &stride)
        || !readU32(cursor, end, &pixelFormat)
        || !readU32(cursor, end, &byteSize)) {
        return false;
    }

    if (width == 0
        || height == 0
        || width > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        || height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())
        || pixelFormat != APE_LUMORPHA_PIXEL_RGBA8
        || stride < width * 4U
        || !cursor
        || !end
        || static_cast<std::uint64_t>(end - cursor) < byteSize) {
        return false;
    }

    const std::uint64_t minimumBytes = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(height - 1U)
        + static_cast<std::uint64_t>(width) * 4ULL;
    const std::uint64_t pixelCount = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (byteSize < minimumBytes || pixelCount > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    FlagImage result;
    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.pixels.resize(static_cast<std::size_t>(pixelCount));

    for (int y = 0; y < result.height; ++y) {
        const unsigned char* row = cursor + static_cast<std::uint64_t>(y) * stride;
        for (int x = 0; x < result.width; ++x) {
            const int offset = x * 4;
            result.pixels[static_cast<std::size_t>(y * result.width + x)] = FlagManager::makeRgba(
                row[offset],
                row[offset + 1],
                row[offset + 2],
                row[offset + 3]
            );
        }
    }

    *outImage = std::move(result);
    return FlagManager::isValidImage(*outImage);
}

class LumorphaClient {
public:
    bool decodeImage(const QByteArray& data, std::uint32_t formatHint, FlagImage* outImage) {
        if (!outImage) {
            return false;
        }
        *outImage = {};
        if (data.isEmpty()) {
            m_lastError = QStringLiteral("Image data is empty.");
            return false;
        }

        QByteArray payload;
        payload.reserve(static_cast<int>(sizeof(std::uint32_t)) + data.size());
        appendU32(&payload, formatHint);
        payload.append(data);

        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("Lumorpha"),
            QStringLiteral("lumorpha.decodeImage"),
            ToolRuntimeContext::PluginPayloadContentType::Binary,
            payload
        );
        if (!response.success) {
            setLastErrorFromResponse(response, QStringLiteral("Lumorpha decode failed."));
            return false;
        }

        if (response.contentType != ToolRuntimeContext::PluginPayloadContentType::BinaryEnvelope
            || !flagImageFromEnvelope(response.payload, outImage)) {
            m_lastError = QStringLiteral("Lumorpha returned an invalid image.");
            return false;
        }
        m_lastError.clear();
        return true;
    }

    bool cropResizeImage(const FlagImage& image, const Rect& crop, int targetWidth, int targetHeight, FlagImage* outImage) {
        if (!outImage) {
            return false;
        }
        *outImage = {};
        if (!FlagManager::isValidImage(image)) {
            m_lastError = QStringLiteral("Source image is invalid.");
            return false;
        }
        if (crop.left < 0
            || crop.top < 0
            || crop.right < crop.left
            || crop.bottom < crop.top
            || targetWidth <= 0
            || targetHeight <= 0) {
            m_lastError = QStringLiteral("Invalid crop range.");
            return false;
        }

        QByteArray payload = imageEnvelopeFromFlagImage(image);
        if (payload.isEmpty()) {
            m_lastError = QStringLiteral("Source image is invalid.");
            return false;
        }
        appendU32(&payload, static_cast<std::uint32_t>(crop.left));
        appendU32(&payload, static_cast<std::uint32_t>(crop.top));
        appendU32(&payload, static_cast<std::uint32_t>(crop.right - crop.left + 1));
        appendU32(&payload, static_cast<std::uint32_t>(crop.bottom - crop.top + 1));
        appendU32(&payload, static_cast<std::uint32_t>(targetWidth));
        appendU32(&payload, static_cast<std::uint32_t>(targetHeight));
        appendU32(&payload, APE_LUMORPHA_FILTER_LANCZOS3);

        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("Lumorpha"),
            QStringLiteral("lumorpha.cropResizeImage"),
            ToolRuntimeContext::PluginPayloadContentType::BinaryEnvelope,
            payload
        );
        if (!response.success) {
            setLastErrorFromResponse(response, QStringLiteral("Lumorpha crop-resize failed."));
            return false;
        }

        if (response.contentType != ToolRuntimeContext::PluginPayloadContentType::BinaryEnvelope
            || !flagImageFromEnvelope(response.payload, outImage)) {
            m_lastError = QStringLiteral("Lumorpha returned an invalid image.");
            return false;
        }
        m_lastError.clear();
        return true;
    }

    bool resizeImage(const FlagImage& image, const QSize& targetSize, FlagImage* outImage) {
        if (!outImage) {
            return false;
        }
        *outImage = {};
        if (!FlagManager::isValidImage(image) || !targetSize.isValid()) {
            m_lastError = QStringLiteral("Invalid resize arguments.");
            return false;
        }

        QByteArray payload = imageEnvelopeFromFlagImage(image);
        if (payload.isEmpty()) {
            m_lastError = QStringLiteral("Source image is invalid.");
            return false;
        }
        appendU32(&payload, static_cast<std::uint32_t>(targetSize.width()));
        appendU32(&payload, static_cast<std::uint32_t>(targetSize.height()));
        appendU32(&payload, APE_LUMORPHA_FILTER_BOX);

        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("Lumorpha"),
            QStringLiteral("lumorpha.resizeImage"),
            ToolRuntimeContext::PluginPayloadContentType::BinaryEnvelope,
            payload
        );
        if (!response.success) {
            setLastErrorFromResponse(response, QStringLiteral("Lumorpha resize failed."));
            return false;
        }

        if (response.contentType != ToolRuntimeContext::PluginPayloadContentType::BinaryEnvelope
            || !flagImageFromEnvelope(response.payload, outImage)) {
            m_lastError = QStringLiteral("Lumorpha returned an invalid image.");
            return false;
        }
        m_lastError.clear();
        return true;
    }

    QByteArray encodeImage(const FlagImage& image, std::uint32_t outputFormat) {
        if (!FlagManager::isValidImage(image)) {
            m_lastError = QStringLiteral("Source image is invalid.");
            return {};
        }

        QByteArray payload = imageEnvelopeFromFlagImage(image);
        if (payload.isEmpty()) {
            m_lastError = QStringLiteral("Source image is invalid.");
            return {};
        }
        appendU32(&payload, outputFormat);
        appendU32(&payload, 0);

        const ToolRuntimeContext::PluginInvokeResponse response = invokePluginOperation(
            QStringLiteral("Lumorpha"),
            QStringLiteral("lumorpha.encodeImage"),
            ToolRuntimeContext::PluginPayloadContentType::BinaryEnvelope,
            payload
        );
        if (!response.success
            || response.contentType != ToolRuntimeContext::PluginPayloadContentType::Binary
            || response.payload.isEmpty()) {
            setLastErrorFromResponse(response, QStringLiteral("Lumorpha encode failed."));
            return {};
        }

        m_lastError.clear();
        return response.payload;
    }

    QString lastError() const {
        return m_lastError;
    }

private:
    void setLastErrorFromResponse(const ToolRuntimeContext::PluginInvokeResponse& response, const QString& fallback) {
        m_lastError = response.errorMessage.trimmed();
        if (m_lastError.isEmpty()) {
            m_lastError = fallback;
        }
    }

    QString m_lastError;
};

class LumorphaImagePipeline final : public IImagePipeline {
public:
    FlagImage cropResizeImage(const FlagImage& image,
                              const Rect& crop,
                              int targetWidth,
                              int targetHeight) const override {
        FlagImage result;
        if (!m_client.cropResizeImage(image, crop, targetWidth, targetHeight, &result)) {
            return {};
        }
        return result;
    }

    std::vector<std::uint8_t> encodeTga32(const FlagImage& image) const override {
        const QByteArray encoded = m_client.encodeImage(image, APE_LUMORPHA_FORMAT_TGA);
        if (encoded.isEmpty()) {
            return {};
        }
        const auto* begin = reinterpret_cast<const std::uint8_t*>(encoded.constData());
        return std::vector<std::uint8_t>(begin, begin + encoded.size());
    }

private:
    mutable LumorphaClient m_client;
};

FlagImage loadImageFileWithLumorpha(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QString extension = QFileInfo(path).suffix().toLower();
    std::uint32_t hint = APE_LUMORPHA_FORMAT_AUTO;
    if (extension == QStringLiteral("tga")) {
        hint = APE_LUMORPHA_FORMAT_TGA;
    } else if (extension == QStringLiteral("dds")) {
        hint = APE_LUMORPHA_FORMAT_DDS;
    } else if (extension == QStringLiteral("png")) {
        hint = APE_LUMORPHA_FORMAT_PNG;
    } else if (extension == QStringLiteral("jpg") || extension == QStringLiteral("jpeg")) {
        hint = APE_LUMORPHA_FORMAT_JPEG;
    } else if (extension == QStringLiteral("bmp")) {
        hint = APE_LUMORPHA_FORMAT_BMP;
    } else if (extension == QStringLiteral("tif") || extension == QStringLiteral("tiff")) {
        hint = APE_LUMORPHA_FORMAT_TIFF;
    } else if (extension == QStringLiteral("gif")) {
        hint = APE_LUMORPHA_FORMAT_GIF;
    } else if (extension == QStringLiteral("hdr")) {
        hint = APE_LUMORPHA_FORMAT_HDR;
    }

    LumorphaClient client;
    FlagImage image;
    if (!client.decodeImage(file.readAll(), hint, &image)) {
        return {};
    }
    return image;
}

QString pngBase64FromFlagImage(const FlagImage& image, const QSize& targetSize = QSize()) {
    if (!FlagManager::isValidImage(image)) {
        return {};
    }

    LumorphaClient client;
    FlagImage renderImage = image;
    if (targetSize.isValid()) {
        FlagImage resized;
        if (client.resizeImage(image, targetSize, &resized) && FlagManager::isValidImage(resized)) {
            renderImage = std::move(resized);
        }
    }

    const QByteArray png = client.encodeImage(renderImage, APE_LUMORPHA_FORMAT_PNG);
    return png.isEmpty() ? QString() : QString::fromLatin1(png.toBase64());
}

QImage tgaImageFromData(const QByteArray& data) {
    if (data.size() < 18) {
        return {};
    }

    const auto* bytes = reinterpret_cast<const uchar*>(data.constData());
    const int idLength = bytes[0];
    const int colorMapType = bytes[1];
    const int imageType = bytes[2];
    const int width = bytes[12] | (bytes[13] << 8);
    const int height = bytes[14] | (bytes[15] << 8);
    const int bpp = bytes[16];
    const int descriptor = bytes[17];

    if (colorMapType != 0 || (imageType != 2 && imageType != 10)) {
        return QImage::fromData(data);
    }
    if ((bpp != 24 && bpp != 32) || width <= 0 || height <= 0) {
        return {};
    }

    const int bytesPerPixel = bpp / 8;
    const int pixelDataOffset = 18 + idLength;
    if (pixelDataOffset < 0 || pixelDataOffset >= data.size()) {
        return {};
    }
    if (height > 0 && width > std::numeric_limits<int>::max() / height) {
        return {};
    }

    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        return {};
    }
    image.fill(0);

    const uchar* pixelData = bytes + pixelDataOffset;
    const int pixelCount = width * height;
    const int maxDataSize = data.size() - pixelDataOffset;

    auto writePixel = [&](int currentPixel, uchar blue, uchar green, uchar red, uchar alpha) {
        const int x = currentPixel % width;
        const int sourceY = currentPixel / width;
        const int destY = (descriptor & 0x20) ? sourceY : (height - 1 - sourceY);
        uchar* line = image.scanLine(destY);
        const int offset = x * 4;
        line[offset] = red;
        line[offset + 1] = green;
        line[offset + 2] = blue;
        line[offset + 3] = alpha;
    };

    if (imageType == 2) {
        for (int currentPixel = 0; currentPixel < pixelCount; ++currentPixel) {
            const int srcIndex = currentPixel * bytesPerPixel;
            if (srcIndex + bytesPerPixel > maxDataSize) {
                break;
            }
            writePixel(
                currentPixel,
                pixelData[srcIndex],
                pixelData[srcIndex + 1],
                pixelData[srcIndex + 2],
                bytesPerPixel == 4 ? pixelData[srcIndex + 3] : 255
            );
        }
        return image;
    }

    int currentPixel = 0;
    int dataIndex = 0;
    while (currentPixel < pixelCount && dataIndex < maxDataSize) {
        const uchar header = pixelData[dataIndex++];
        const int count = (header & 0x7F) + 1;

        if ((header & 0x80) != 0) {
            if (dataIndex + bytesPerPixel > maxDataSize) {
                break;
            }
            const uchar blue = pixelData[dataIndex];
            const uchar green = pixelData[dataIndex + 1];
            const uchar red = pixelData[dataIndex + 2];
            const uchar alpha = bytesPerPixel == 4 ? pixelData[dataIndex + 3] : 255;
            dataIndex += bytesPerPixel;
            for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                writePixel(currentPixel, blue, green, red, alpha);
            }
        } else {
            for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                if (dataIndex + bytesPerPixel > maxDataSize) {
                    break;
                }
                writePixel(
                    currentPixel,
                    pixelData[dataIndex],
                    pixelData[dataIndex + 1],
                    pixelData[dataIndex + 2],
                    bytesPerPixel == 4 ? pixelData[dataIndex + 3] : 255
                );
                dataIndex += bytesPerPixel;
            }
        }
    }

    return image;
}

QString pngBase64FromTgaBytes(const QByteArray& data, const QSize& targetSize = QSize()) {
    QImage image = tgaImageFromData(data);
    if (image.isNull()) {
        image = QImage::fromData(data, "TGA");
    }
    if (image.isNull()) {
        return {};
    }

    if (targetSize.isValid() && image.size() != targetSize) {
        image = image.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        if (image.isNull()) {
            return {};
        }
    }

    QByteArray png;
    QBuffer buffer(&png);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
        return {};
    }

    return QString::fromLatin1(png.toBase64());
}

QString importThumbnailCacheKey(const ImportItem& item) {
    return QStringLiteral("%1|%2x%3")
        .arg(fromStdString(item.id))
        .arg(item.image ? item.image->width : 0)
        .arg(item.image ? item.image->height : 0);
}

QString importThumbnailBase64(WorkerSession* session, const ImportItem& item) {
    if (!session || !item.image || !FlagManager::isValidImage(*item.image)) {
        return {};
    }

    const QString cacheKey = importThumbnailCacheKey(item);
    const auto cached = session->importThumbnailBase64Cache.constFind(cacheKey);
    if (cached != session->importThumbnailBase64Cache.constEnd()) {
        return cached.value();
    }

    const QString base64 = pngBase64FromFlagImage(*item.image, QSize(kImportThumbnailWidth, kImportThumbnailHeight));
    if (!base64.isEmpty()) {
        if (session->importThumbnailBase64Cache.size() >= kImportThumbnailCacheEntryLimit) {
            session->importThumbnailBase64Cache.clear();
        }
        session->importThumbnailBase64Cache.insert(cacheKey, base64);
    }
    return base64;
}

QString currentImportPreviewBase64(WorkerSession* session, const ImportItem& item) {
    if (!session || !item.image || !FlagManager::isValidImage(*item.image)) {
        if (session) {
            session->currentImportPreviewId.clear();
            session->currentImportPreviewBase64.clear();
        }
        return {};
    }

    const QString importId = fromStdString(item.id);
    if (session->currentImportPreviewId == importId && !session->currentImportPreviewBase64.isEmpty()) {
        return session->currentImportPreviewBase64;
    }

    const QSize targetSize = boundedPreviewSize(*item.image, kImportPreviewMaxDimension);
    const QString base64 = pngBase64FromFlagImage(*item.image, targetSize);
    session->currentImportPreviewId = importId;
    session->currentImportPreviewBase64 = base64;
    return base64;
}

void clearImportPreviewCaches(WorkerSession* session) {
    if (!session) {
        return;
    }
    session->importThumbnailBase64Cache.clear();
    session->currentImportPreviewId.clear();
    session->currentImportPreviewBase64.clear();
}

void releaseRetiredImports(WorkerSession* session) {
    if (!session) {
        return;
    }

    std::vector<ImportItem> retiredImports = session->core.takeRetiredImports();
    retiredImports.clear();
}

QString previewCacheKey(const std::string& relativePath, const QSize& targetSize) {
    const QString sizeKey = targetSize.isValid()
        ? QStringLiteral("%1x%2").arg(targetSize.width()).arg(targetSize.height())
        : QStringLiteral("native");
    return QStringLiteral("%1|%2").arg(fromStdString(relativePath), sizeKey);
}

QString effectivePngBase64(WorkerSession* session,
                           const std::string& relativePath,
                           const QSize& targetSize = QSize()) {
    if (!session || !session->fileSystem || relativePath.empty()) {
        return {};
    }

    const bool cacheable = targetSize == QSize(kSmallFlagIconWidth, kSmallFlagIconHeight);
    const QString cacheKey = previewCacheKey(relativePath, targetSize);
    if (cacheable) {
        const auto cached = session->previewBase64Cache.constFind(cacheKey);
        if (cached != session->previewBase64Cache.constEnd()) {
            return cached.value();
        }
    }

    const FileReadResult readResult = session->fileSystem->readEffectiveFile(relativePath);
    if (!readResult.success) {
        return {};
    }

    QByteArray content(reinterpret_cast<const char*>(readResult.content.data()), static_cast<int>(readResult.content.size()));
    auto cachePreview = [&](const QString& base64, const QSize& renderedSize) {
        if (base64.isEmpty()) {
            return base64;
        }
        if (cacheable) {
            if (session->previewBase64Cache.size() >= kPreviewCacheEntryLimit) {
                session->previewBase64Cache.clear();
                session->previewSizeCache.clear();
            }
            session->previewBase64Cache.insert(cacheKey, base64);
            session->previewSizeCache.insert(cacheKey, renderedSize);
        }
        return base64;
    };
    auto fallbackPreview = [&]() {
        return cachePreview(
            pngBase64FromTgaBytes(content, targetSize),
            targetSize.isValid() ? targetSize : QSize()
        );
    };

    LumorphaClient client;
    FlagImage image;
    if (!client.decodeImage(content, APE_LUMORPHA_FORMAT_TGA, &image)) {
        return fallbackPreview();
    }
    if (!FlagManager::isValidImage(image)) {
        return fallbackPreview();
    }

    const QSize sourceSize(image.width, image.height);
    FlagImage renderImage = image;
    if (targetSize.isValid()) {
        FlagImage resized;
        if (client.resizeImage(image, targetSize, &resized) && FlagManager::isValidImage(resized)) {
            renderImage = std::move(resized);
        }
    }

    const QByteArray png = client.encodeImage(renderImage, APE_LUMORPHA_FORMAT_PNG);
    if (png.isEmpty()) {
        return fallbackPreview();
    }
    const QString base64 = QString::fromLatin1(png.toBase64());
    return cachePreview(base64, targetSize.isValid() ? QSize(renderImage.width, renderImage.height) : sourceSize);
}

QSize effectivePngSize(WorkerSession* session,
                       const std::string& relativePath,
                       const QSize& targetSize = QSize()) {
    if (!session || relativePath.empty()) {
        return {};
    }

    if (targetSize.isValid()) {
        const QString cacheKey = previewCacheKey(relativePath, targetSize);
        const auto loaded = session->previewSizeCache.constFind(cacheKey);
        if (loaded != session->previewSizeCache.constEnd()) {
            return loaded.value();
        }

        if (!effectivePngBase64(session, relativePath, targetSize).isEmpty()) {
            const auto refreshed = session->previewSizeCache.constFind(cacheKey);
            if (refreshed != session->previewSizeCache.constEnd()) {
                return refreshed.value();
            }
        }
    }

    return {};
}

class ToolRuntimeFileSystem final : public IFileSystem {
public:
    EffectiveFileListResult listEffectiveFiles() const override {
        const ToolRuntimeContext::EffectiveFileListResult runtimeResult =
            ToolRuntimeContext::instance().listEffectiveFiles(QStringLiteral("gfx/flags"), QStringLiteral(".tga"));

        EffectiveFileListResult result;
        result.success = runtimeResult.success;
        result.errorMessage = toStdString(runtimeResult.errorMessage);
        result.entries.reserve(static_cast<std::size_t>(runtimeResult.entries.size()));
        if (!runtimeResult.success) {
            return result;
        }

        for (const ToolRuntimeContext::EffectiveFileEntry& runtimeEntry : runtimeResult.entries) {
            EffectiveFileEntry entry;
            entry.logicalPath = toStdString(runtimeEntry.logicalPath);
            entry.source = toStdString(ToolRuntimeContext::effectiveFileSourceToString(runtimeEntry.source));
            result.entries.push_back(std::move(entry));
        }
        return result;
    }

    FileReadResult readEffectiveFile(const std::string& logicalPath) const override {
        const ToolRuntimeContext::FileReadResult runtimeResult =
            ToolRuntimeContext::instance().readEffectiveFile(fromStdString(logicalPath));
        return fromRuntimeReadResult(runtimeResult);
    }

    FileReadResult readModFile(const std::string& logicalPath) const override {
        const ToolRuntimeContext::FileReadResult runtimeResult =
            ToolRuntimeContext::instance().readFile(
                ToolRuntimeContext::FileRoot::Mod,
                fromStdString(logicalPath)
            );
        return fromRuntimeReadResult(runtimeResult);
    }

    FileWriteResult ensureModDirectory(const std::string& logicalPath) const override {
        const ToolRuntimeContext::FileWriteResult runtimeResult =
            ToolRuntimeContext::instance().ensureDirectory(
                ToolRuntimeContext::FileRoot::Mod,
                fromStdString(logicalPath)
            );
        return {runtimeResult.success, toStdString(runtimeResult.errorMessage)};
    }

    FileWriteResult writeModFile(const std::string& logicalPath, const std::vector<std::uint8_t>& content) const override {
        const QByteArray bytes(reinterpret_cast<const char*>(content.data()), static_cast<int>(content.size()));
        const ToolRuntimeContext::FileWriteResult runtimeResult =
            ToolRuntimeContext::instance().writeFile(
                ToolRuntimeContext::FileRoot::Mod,
                fromStdString(logicalPath),
                bytes
            );
        return {runtimeResult.success, toStdString(runtimeResult.errorMessage)};
    }

private:
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

QJsonObject buildColumn(const QString& key, const QString& title, int width = -1, bool stretch = false, bool hidden = false) {
    QJsonObject column;
    column[QStringLiteral("key")] = key;
    column[QStringLiteral("id")] = key;
    column[QStringLiteral("title")] = title;
    column[QStringLiteral("text")] = title;
    if (width > 0) {
        column[QStringLiteral("width")] = width;
    }
    if (stretch) {
        column[QStringLiteral("stretch")] = true;
    }
    if (hidden) {
        column[QStringLiteral("hidden")] = true;
    }
    return column;
}

QJsonObject buildButton(const QString& actionId,
                        const QString& text,
                        bool checked = false,
                        bool enabled = true,
                        int width = 92,
                        const QString& shortcut = QString()) {
    QJsonObject object;
    object[QStringLiteral("actionId")] = actionId;
    object[QStringLiteral("text")] = text;
    object[QStringLiteral("checked")] = checked;
    object[QStringLiteral("enabled")] = enabled;
    object[QStringLiteral("variant")] = QStringLiteral("toolbar");
    object[QStringLiteral("width")] = width;
    if (!shortcut.isEmpty()) {
        object[QStringLiteral("shortcut")] = shortcut;
    }
    return object;
}

QJsonArray buildTopbarButtons(const QJsonObject& first,
                              const QJsonObject& second = QJsonObject(),
                              const QJsonObject& third = QJsonObject(),
                              const QJsonObject& fourth = QJsonObject()) {
    QJsonArray array;
    if (!first.isEmpty()) {
        array.append(first);
    }
    if (!second.isEmpty()) {
        array.append(second);
    }
    if (!third.isEmpty()) {
        array.append(third);
    }
    if (!fourth.isEmpty()) {
        array.append(fourth);
    }
    return array;
}

QString statusColor(FlagStatus status) {
    switch (status) {
    case FlagStatus::MissingDefault:
        return QStringLiteral("#DC2626");
    case FlagStatus::Partial:
        return QStringLiteral("#D97706");
    default:
        return {};
    }
}

QJsonObject serializeRect(const Rect& rect) {
    QJsonObject object;
    object[QStringLiteral("left")] = rect.left;
    object[QStringLiteral("top")] = rect.top;
    object[QStringLiteral("right")] = rect.right;
    object[QStringLiteral("bottom")] = rect.bottom;
    return object;
}

Rect rectFromJson(const QJsonObject& object, const Rect& fallback) {
    Rect rect = fallback;
    if (object.contains(QStringLiteral("left"))) {
        rect.left = object.value(QStringLiteral("left")).toInt(rect.left);
    }
    if (object.contains(QStringLiteral("top"))) {
        rect.top = object.value(QStringLiteral("top")).toInt(rect.top);
    }
    if (object.contains(QStringLiteral("right"))) {
        rect.right = object.value(QStringLiteral("right")).toInt(rect.right);
    }
    if (object.contains(QStringLiteral("bottom"))) {
        rect.bottom = object.value(QStringLiteral("bottom")).toInt(rect.bottom);
    }
    return rect;
}

QJsonObject serializeImportItem(WorkerSession* session, const ImportItem& item, bool selected) {
    QJsonObject object;
    object[QStringLiteral("id")] = fromStdString(item.id);
    object[QStringLiteral("fileName")] = fromStdString(item.fileName);
    object[QStringLiteral("name")] = fromStdString(item.name);
    object[QStringLiteral("width")] = item.image ? item.image->width : 0;
    object[QStringLiteral("height")] = item.image ? item.image->height : 0;
    object[QStringLiteral("crop")] = serializeRect(item.crop);
    object[QStringLiteral("selected")] = selected;
    if (item.image) {
        object[QStringLiteral("previewBase64")] = currentImportPreviewBase64(session, item);
    } else {
        object[QStringLiteral("previewBase64")] = QString();
    }
    return object;
}

QJsonObject buildTagListModel(WorkerSession* session, const Snapshot& state) {
    QJsonObject model;
    model[QStringLiteral("id")] = QStringLiteral("tag_list");
    model[QStringLiteral("title")] = localizedString(session, QStringLiteral("Tags"));
    model[QStringLiteral("headerHidden")] = true;
    model[QStringLiteral("selectionMode")] = QStringLiteral("single");

    QJsonArray columns;
    columns.append(buildColumn(QStringLiteral("tag"), localizedString(session, QStringLiteral("Tags")), -1, true));
    model[QStringLiteral("columns")] = columns;

    QJsonArray rows;
    for (const TagListRow& tag : state.tags) {
        QJsonObject row;
        row[QStringLiteral("id")] = fromStdString(tag.tag);
        row[QStringLiteral("rowId")] = fromStdString(tag.tag);

        QJsonObject values;
        values[QStringLiteral("tag")] = fromStdString(tag.tag);
        values[QStringLiteral("displayName")] = fromStdString(tag.tag);
        row[QStringLiteral("values")] = values;

        QJsonArray cells;
        QJsonObject cell;
        cell[QStringLiteral("value")] = fromStdString(tag.tag);
        cells.append(cell);
        row[QStringLiteral("cells")] = cells;

        QJsonObject rowState;
        rowState[QStringLiteral("selectAction")] = QStringLiteral("select_tag");
        rowState[QStringLiteral("status")] = static_cast<int>(tag.status);
        rowState[QStringLiteral("selected")] = tag.tag == state.selectedTag;
        const QString color = statusColor(tag.status);
        if (!color.isEmpty()) {
            rowState[QStringLiteral("textColor")] = color;
        }
        if (!tag.previewPath.empty()) {
            rowState[QStringLiteral("iconEffectivePath")] = fromStdString(tag.previewPath);
            rowState[QStringLiteral("iconWidth")] = kSmallFlagIconWidth;
            rowState[QStringLiteral("iconHeight")] = kSmallFlagIconHeight;
        }
        row[QStringLiteral("state")] = rowState;

        rows.append(row);
    }
    model[QStringLiteral("rows")] = rows;

    QJsonArray selection;
    if (!state.selectedTag.empty()) {
        selection.append(fromStdString(state.selectedTag));
    }
    model[QStringLiteral("selection")] = selection;
    return model;
}

QJsonObject buildImportListModel(WorkerSession* session, const Snapshot& state) {
    QJsonObject model;
    model[QStringLiteral("id")] = QStringLiteral("import_list");
    model[QStringLiteral("title")] = localizedString(session, QStringLiteral("Files"));
    model[QStringLiteral("headerHidden")] = false;
    model[QStringLiteral("selectionMode")] = QStringLiteral("extended");

    QJsonArray columns;
    columns.append(buildColumn(QStringLiteral("name"), localizedString(session, QStringLiteral("ColFlagName")), 94));
    columns.append(buildColumn(QStringLiteral("fileName"), localizedString(session, QStringLiteral("ColFileName")), -1, true));
    model[QStringLiteral("columns")] = columns;

    QJsonArray actions;
    QJsonObject fill;
    fill[QStringLiteral("actionId")] = QStringLiteral("fill_name");
    fill[QStringLiteral("text")] = localizedString(session, QStringLiteral("FillName"));
    actions.append(fill);
    QJsonObject remove;
    remove[QStringLiteral("actionId")] = QStringLiteral("remove_import");
    remove[QStringLiteral("text")] = localizedString(session, QStringLiteral("RemoveFromList"));
    actions.append(remove);
    model[QStringLiteral("contextActions")] = actions;

    QJsonArray rows;
    for (const ImportItem& item : state.imports) {
        const bool selected = std::find(state.selectedImportIds.begin(), state.selectedImportIds.end(), item.id)
            != state.selectedImportIds.end();

        QJsonObject row;
        row[QStringLiteral("id")] = fromStdString(item.id);
        row[QStringLiteral("rowId")] = fromStdString(item.id);

        QJsonObject values;
        values[QStringLiteral("name")] = fromStdString(item.name);
        values[QStringLiteral("displayName")] = fromStdString(item.name.empty() ? item.fileName : item.name);
        values[QStringLiteral("fileName")] = fromStdString(item.fileName);
        row[QStringLiteral("values")] = values;

        QJsonArray cells;
        QJsonObject nameCell;
        nameCell[QStringLiteral("value")] = fromStdString(item.name);
        cells.append(nameCell);
        QJsonObject fileCell;
        fileCell[QStringLiteral("value")] = fromStdString(item.fileName);
        cells.append(fileCell);
        row[QStringLiteral("cells")] = cells;

        QJsonObject rowState;
        rowState[QStringLiteral("selectAction")] = QStringLiteral("select_import");
        rowState[QStringLiteral("selected")] = selected || item.id == state.selectedImportId;
        if (item.image) {
            rowState[QStringLiteral("iconBase64")] = importThumbnailBase64(session, item);
            const QSize sourceSize(item.image->width, item.image->height);
            const QSize scaledSize = sourceSize.scaled(QSize(kImportThumbnailWidth, kImportThumbnailHeight), Qt::KeepAspectRatio);
            rowState[QStringLiteral("iconWidth")] = scaledSize.width();
            rowState[QStringLiteral("iconHeight")] = scaledSize.height();
        }
        row[QStringLiteral("state")] = rowState;
        rows.append(row);
    }
    model[QStringLiteral("rows")] = rows;

    QJsonArray selection;
    for (const std::string& id : state.selectedImportIds) {
        selection.append(fromStdString(id));
    }
    model[QStringLiteral("selection")] = selection;
    return model;
}

QJsonArray buildFlagCards(WorkerSession* session, const Snapshot& state) {
    QJsonArray cards;
    if (!session || !session->fileSystem) {
        return cards;
    }

    for (const ManageVariantDisplay& variant : state.selectedTagVariants) {
        QJsonObject card;
        card[QStringLiteral("name")] = fromStdString(variant.name);
        card[QStringLiteral("hasLarge")] = variant.hasLarge;
        card[QStringLiteral("hasMedium")] = variant.hasMedium;
        card[QStringLiteral("hasSmall")] = variant.hasSmall;
        card[QStringLiteral("missing")] = variant.previewPath.empty();
        if (!variant.previewPath.empty()) {
            const QString imageBase64 = effectivePngBase64(session, variant.previewPath);
            if (!imageBase64.isEmpty()) {
                card[QStringLiteral("imageBase64")] = imageBase64;
            }
        }
        cards.append(card);
    }
    return cards;
}

const ImportItem* selectedImport(const Snapshot& state) {
    for (const ImportItem& item : state.imports) {
        if (item.id == state.selectedImportId) {
            return &item;
        }
    }
    return nullptr;
}

const ImportItem* importById(const Snapshot& state, const std::string& importId) {
    for (const ImportItem& item : state.imports) {
        if (item.id == importId) {
            return &item;
        }
    }
    return nullptr;
}

QJsonObject buildViewState(WorkerSession* session, const Snapshot& state) {
    QJsonObject object;
    object[QStringLiteral("mode")] = state.mode == ToolMode::New ? QStringLiteral("new") : QStringLiteral("manage");
    object[QStringLiteral("sizeIndex")] = state.sizeIndex;
    object[QStringLiteral("selectedTag")] = fromStdString(state.selectedTag);
    object[QStringLiteral("selectedImportId")] = fromStdString(state.selectedImportId);
    object[QStringLiteral("canExportCurrent")] = state.canExportCurrent;
    object[QStringLiteral("canExportAll")] = state.canExportAll;
    object[QStringLiteral("hasSelection")] = state.hasSelection;
    object[QStringLiteral("pendingOverwrite")] = state.pendingOverwrite;
    object[QStringLiteral("loadingActive")] = false;
    object[QStringLiteral("loadingText")] = QString();
    object[QStringLiteral("statusText")] = state.statusText.empty()
        ? localizedString(session, QStringLiteral("Ready"))
        : fromStdString(state.statusText);
    object[QStringLiteral("lastError")] = fromStdString(state.lastError);
    object[QStringLiteral("flagCards")] = state.mode == ToolMode::Manage
        ? buildFlagCards(session, state)
        : QJsonArray();
    object[QStringLiteral("importCount")] = static_cast<int>(state.imports.size());
    object[QStringLiteral("flagCount")] = state.mode == ToolMode::Manage
        ? static_cast<int>(state.selectedTagVariants.size())
        : 0;

    QJsonArray overwriteFiles;
    for (const std::string& path : state.pendingOverwriteFiles) {
        overwriteFiles.append(fromStdString(path));
    }
    object[QStringLiteral("pendingOverwriteFiles")] = overwriteFiles;

    const ImportItem* importItem = selectedImport(state);
    if (importItem) {
        object[QStringLiteral("currentImport")] = serializeImportItem(session, *importItem, true);
    } else {
        if (session) {
            session->currentImportPreviewId.clear();
            session->currentImportPreviewBase64.clear();
        }
        object[QStringLiteral("currentImport")] = QJsonObject();
    }
    return object;
}

QJsonObject buildSidebarState(WorkerSession* session, const Snapshot& state) {
    const bool createMode = state.mode == ToolMode::New;
    QJsonObject sidebar;
    sidebar[QStringLiteral("visible")] = true;
    sidebar[QStringLiteral("title")] = localizedString(session, createMode ? QStringLiteral("Files") : QStringLiteral("Tags"));
    sidebar[QStringLiteral("activeMode")] = createMode ? QStringLiteral("import_list") : QStringLiteral("tag_list");
    sidebar[QStringLiteral("modelId")] = createMode ? QStringLiteral("import_list") : QStringLiteral("tag_list");
    sidebar[QStringLiteral("searchEnabled")] = true;
    sidebar[QStringLiteral("selectAllEnabled")] = createMode;

    QJsonArray modeOrder;
    modeOrder.append(QStringLiteral("__default__"));
    modeOrder.append(QStringLiteral("__search__"));
    sidebar[QStringLiteral("modeOrder")] = modeOrder;

    QJsonArray searchableColumns;
    QJsonArray searchableColumnLabels;
    if (createMode) {
        searchableColumns.append(0);
        searchableColumns.append(1);
        searchableColumnLabels.append(localizedString(session, QStringLiteral("ColFlagName")));
        searchableColumnLabels.append(localizedString(session, QStringLiteral("ColFileName")));
    } else {
        searchableColumns.append(0);
        searchableColumnLabels.append(localizedString(session, QStringLiteral("Tags")));
    }
    sidebar[QStringLiteral("searchableColumns")] = searchableColumns;
    sidebar[QStringLiteral("searchableColumnLabels")] = searchableColumnLabels;
    return sidebar;
}

QJsonObject buildTopbarState(WorkerSession* session, const Snapshot& state) {
    QJsonObject topbar;
    topbar[QStringLiteral("visible")] = true;
    topbar[QStringLiteral("currentPageId")] = QStringLiteral("main");

    const bool createMode = state.mode == ToolMode::New;
    topbar[QStringLiteral("leftButtons")] = buildTopbarButtons(
        buildButton(QStringLiteral("set_mode_manage"), localizedString(session, QStringLiteral("TabManage")), !createMode, true, 92),
        buildButton(QStringLiteral("set_mode_new"), localizedString(session, QStringLiteral("TabNew")), createMode, true, 92)
    );

    if (createMode) {
        topbar[QStringLiteral("rightButtons")] = buildTopbarButtons(
            buildButton(QStringLiteral("import_files"), localizedString(session, QStringLiteral("ImportFiles")), false, true, 104, QStringLiteral("Ctrl+O")),
            buildButton(QStringLiteral("export_current"), localizedString(session, QStringLiteral("Export")), false, state.canExportCurrent, 118, QStringLiteral("Ctrl+S")),
            buildButton(QStringLiteral("export_all"), localizedString(session, QStringLiteral("ExportAll")), false, state.canExportAll, 104, QStringLiteral("Ctrl+Shift+S")),
            buildButton(
                state.selectedImportIds.size() == state.imports.size() && !state.imports.empty()
                    ? QStringLiteral("deselect_all")
                    : QStringLiteral("select_all"),
                state.selectedImportIds.size() == state.imports.size() && !state.imports.empty()
                    ? localizedString(session, QStringLiteral("DeselectAll"))
                    : localizedString(session, QStringLiteral("SelectAll")),
                false,
                !state.imports.empty(),
                96
            )
        );
    } else {
        topbar[QStringLiteral("rightButtons")] = buildTopbarButtons(
            buildButton(QStringLiteral("set_size_0"), localizedString(session, QStringLiteral("SizeLarge")), state.sizeIndex == 0, true, 84, QStringLiteral("L")),
            buildButton(QStringLiteral("set_size_1"), localizedString(session, QStringLiteral("SizeMedium")), state.sizeIndex == 1, true, 84, QStringLiteral("M")),
            buildButton(QStringLiteral("set_size_2"), localizedString(session, QStringLiteral("SizeSmall")), state.sizeIndex == 2, true, 84, QStringLiteral("S"))
        );
    }
    return topbar;
}

QJsonArray buildListModels(WorkerSession* session, const Snapshot& state) {
    QJsonArray models;
    if (state.mode == ToolMode::New) {
        models.append(buildImportListModel(session, state));
    } else {
        models.append(buildTagListModel(session, state));
    }
    return models;
}

std::map<std::string, std::string> toStringMap(const QJsonObject& object) {
    std::map<std::string, std::string> result;
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (iterator->isString()) {
            result.emplace(toStdString(iterator.key()), toStdString(iterator->toString()));
        } else if (iterator->isBool()) {
            result.emplace(toStdString(iterator.key()), iterator->toBool() ? "true" : "false");
        } else if (iterator->isDouble()) {
            result.emplace(toStdString(iterator.key()), QByteArray::number(iterator->toDouble()).toStdString());
        } else {
            result.emplace(toStdString(iterator.key()), QJsonDocument(iterator->toObject()).toJson(QJsonDocument::Compact).toStdString());
        }
    }
    return result;
}

QString normalizedActionName(QString value) {
    value = value.trimmed().toLower();
    if (value == QStringLiteral("set_mode_manage")) {
        return QStringLiteral("set_mode");
    }
    if (value == QStringLiteral("set_mode_new")) {
        return QStringLiteral("set_mode");
    }
    if (value.startsWith(QStringLiteral("set_size_"))) {
        return QStringLiteral("set_size");
    }
    return value;
}

ToolWorkerResult applyCoreResult(WorkerSession* session, bool success);

ToolWorkerResult applyEditorStateFromArguments(WorkerSession* session,
                                               const char* targetId,
                                               const QJsonObject& arguments) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }

    const bool hasEditorState = arguments.value(QStringLiteral("hasEditorState")).toBool(false)
        || arguments.contains(QStringLiteral("editorName"))
        || arguments.contains(QStringLiteral("editorCrop"));
    if (!hasEditorState) {
        return TOOL_WORKER_SUCCESS;
    }

    QString importId = arguments.value(QStringLiteral("editorImportId")).toString().trimmed();
    if (importId.isEmpty()) {
        importId = QString::fromUtf8(targetId ? targetId : "").trimmed();
    }
    if (importId.isEmpty()) {
        return TOOL_WORKER_SUCCESS;
    }

    if (arguments.contains(QStringLiteral("editorName"))) {
        if (!session->core.updateImportName(
                toStdString(importId),
                toStdString(arguments.value(QStringLiteral("editorName")).toString()))) {
            return applyCoreResult(session, false);
        }
    }

    if (arguments.value(QStringLiteral("editorCrop")).isObject()) {
        const Snapshot snapshot = session->core.buildSnapshot();
        Rect fallback;
        if (const ImportItem* item = importById(snapshot, toStdString(importId))) {
            fallback = item->crop;
        }
        const Rect rect = rectFromJson(arguments.value(QStringLiteral("editorCrop")).toObject(), fallback);
        if (!session->core.updateImportCrop(toStdString(importId), rect)) {
            return applyCoreResult(session, false);
        }
    }

    clearSessionError(session);
    return TOOL_WORKER_SUCCESS;
}

ToolWorkerResult applyCoreResult(WorkerSession* session, bool success) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }
    if (!success) {
        setSessionError(session, fromStdString(session->core.lastError()));
        return TOOL_WORKER_ERROR_ACTION_FAILED;
    }
    clearSessionError(session);
    return TOOL_WORKER_SUCCESS;
}
} // namespace

ToolWorkerHandle createWorkerHandle(const char* toolId) {
    Q_UNUSED(toolId);
    WorkerSession* session = new (std::nothrow) WorkerSession();
    return reinterpret_cast<ToolWorkerHandle>(session);
}

void destroyWorkerHandle(ToolWorkerHandle handle) {
    delete sessionFromHandle(handle);
}

WorkerSession* sessionFromHandle(ToolWorkerHandle handle) {
    return reinterpret_cast<WorkerSession*>(handle);
}

void setSessionError(WorkerSession* session, const QString& message) {
    if (session) {
        session->lastError = toStdString(message);
    }
}

void clearSessionError(WorkerSession* session) {
    if (session) {
        session->lastError.clear();
    }
}

char* allocateCString(const QByteArray& utf8) {
    char* result = static_cast<char*>(std::malloc(static_cast<std::size_t>(utf8.size()) + 1U));
    if (!result) {
        return nullptr;
    }
    std::memcpy(result, utf8.constData(), static_cast<std::size_t>(utf8.size()));
    result[utf8.size()] = '\0';
    return result;
}

QJsonObject parseJsonObject(const char* jsonText) {
    if (!jsonText || *jsonText == '\0') {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(jsonText));
    return document.isObject() ? document.object() : QJsonObject();
}

QJsonObject buildStatePacket(WorkerSession* session) {
    QJsonObject packet;
    packet[QStringLiteral("pageId")] = QStringLiteral("main");
    packet[QStringLiteral("currentPage")] = QStringLiteral("main");

    if (!session || !ensureCoreInitialized(session)) {
        packet[QStringLiteral("modeId")] = QStringLiteral("manage");
        packet[QStringLiteral("viewState")] = QJsonObject();
        packet[QStringLiteral("sidebarState")] = QJsonObject();
        packet[QStringLiteral("topbarState")] = QJsonObject();
        packet[QStringLiteral("runtimeVariables")] = QJsonObject();
        packet[QStringLiteral("listModels")] = QJsonArray();
        packet[QStringLiteral("models")] = QJsonObject();
        packet[QStringLiteral("patches")] = QJsonArray();
        if (session && !session->lastError.empty()) {
            packet[QStringLiteral("error")] = fromStdString(session->lastError);
        }
        return packet;
    }

    const Snapshot state = session->core.buildSnapshot();
    const QJsonArray listModels = buildListModels(session, state);

    packet[QStringLiteral("modeId")] = state.mode == ToolMode::New ? QStringLiteral("new") : QStringLiteral("manage");
    packet[QStringLiteral("viewState")] = buildViewState(session, state);
    packet[QStringLiteral("sidebarState")] = buildSidebarState(session, state);
    packet[QStringLiteral("topbarState")] = buildTopbarState(session, state);
    packet[QStringLiteral("runtimeVariables")] = QJsonObject();
    packet[QStringLiteral("listModels")] = listModels;
    packet[QStringLiteral("models")] = QJsonObject();
    packet[QStringLiteral("patches")] = QJsonArray();
    packet[QStringLiteral("values")] = packet[QStringLiteral("viewState")].toObject();
    return packet;
}

char* serializeStatePacket(WorkerSession* session) {
    const QByteArray stateJson = QJsonDocument(buildStatePacket(session)).toJson(QJsonDocument::Compact);
    if (session) {
        session->lastSerializedState = stateJson.toStdString();
    }
    return allocateCString(stateJson);
}

ToolWorkerResult initializeSession(WorkerSession* session, const char* configJson) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }

    const QJsonObject config = parseJsonObject(configJson);
    session->toolDirectoryPath = config.value(QStringLiteral("toolDirectory")).toString();
    if (session->toolDirectoryPath.trimmed().isEmpty()) {
        session->toolDirectoryPath = QDir::currentPath();
    }

    session->currentLanguageCode = resolveLanguageCode(
        QDir(session->toolDirectoryPath).filePath(QStringLiteral("localisation")),
        config.value(QStringLiteral("language")).toString()
    );
    if (!loadLocalizedStringsFromJson(session, config.value(QStringLiteral("localizedStrings")).toObject())) {
        session->localizedStrings.clear();
    }

    session->fileSystem = std::make_unique<ToolRuntimeFileSystem>();
    session->imagePipeline = std::make_unique<LumorphaImagePipeline>();
    session->core.setFileSystem(session->fileSystem.get());
    session->core.setImagePipeline(session->imagePipeline.get());
    session->coreInitialized = false;
    session->previewBase64Cache.clear();
    session->previewSizeCache.clear();
    clearImportPreviewCaches(session);
    session->managePreviewWarmupLimit = 0;
    session->lastError.clear();
    session->lastSerializedState.clear();

    clearSessionError(session);
    return TOOL_WORKER_SUCCESS;
}

ToolWorkerResult applyActionInternal(WorkerSession* session,
                                     const char* actionType,
                                     const char* targetId,
                                     const char* argumentsJson) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }

    const QJsonObject arguments = parseJsonObject(argumentsJson);
    const QString action = normalizedActionName(QString::fromUtf8(actionType ? actionType : ""));

    if (action == QStringLiteral("load_language")) {
        const QString language = arguments.value(QStringLiteral("language")).toString();
        session->currentLanguageCode = resolveLanguageCode(
            QDir(session->toolDirectoryPath).filePath(QStringLiteral("localisation")),
            language
        );
        if (arguments.value(QStringLiteral("localizedStrings")).isObject()) {
            loadLocalizedStringsFromJson(session, arguments.value(QStringLiteral("localizedStrings")).toObject());
        }
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (!ensureCoreInitialized(session)) {
        return TOOL_WORKER_ERROR_INITIALIZATION_FAILED;
    }

    if (action == QStringLiteral("refresh") || action == QStringLiteral("refresh_flags")) {
        session->core.setKnownTags(loadTagsFromPluginRuntime());
        session->previewBase64Cache.clear();
        session->previewSizeCache.clear();
        if (session->core.buildSnapshot().mode != ToolMode::New) {
            clearImportPreviewCaches(session);
        }
        session->managePreviewWarmupLimit = 0;
        return applyCoreResult(session, session->core.refreshFlags());
    }

    if (action == QStringLiteral("import_files")) {
        const QJsonArray paths = arguments.value(QStringLiteral("paths")).toArray();
        std::vector<ImportedImage> importedImages;
        importedImages.reserve(static_cast<std::size_t>(paths.size()));
        for (const QJsonValue& value : paths) {
            const QString path = value.toString();
            if (path.trimmed().isEmpty()) {
                continue;
            }
            FlagImage image = loadImageFileWithLumorpha(path);
            if (!FlagManager::isValidImage(image)) {
                continue;
            }

            ImportedImage imported;
            imported.sourceKey = toStdString(
                stableSha1Hex(QStringLiteral("import-source:%1").arg(normalizeRuntimePath(path)))
            );
            imported.fileName = toStdString(QFileInfo(path).fileName());
            imported.image = std::move(image);
            importedImages.push_back(std::move(imported));
        }
        return applyCoreResult(session, session->core.importFiles(std::move(importedImages)));
    }

    if (action == QStringLiteral("update_import_name")) {
        const QString id = arguments.value(QStringLiteral("id")).toString(
            QString::fromUtf8(targetId ? targetId : "")
        );
        return applyCoreResult(
            session,
            session->core.updateImportName(toStdString(id), toStdString(arguments.value(QStringLiteral("name")).toString()))
        );
    }

    if (action == QStringLiteral("update_import_crop")) {
        const QString id = arguments.value(QStringLiteral("id")).toString(
            QString::fromUtf8(targetId ? targetId : "")
        );
        const Snapshot snapshot = session->core.buildSnapshot();
        Rect fallback;
        if (const ImportItem* item = selectedImport(snapshot)) {
            fallback = item->crop;
        }
        const Rect rect = rectFromJson(arguments.value(QStringLiteral("crop")).toObject(), fallback);
        return applyCoreResult(session, session->core.updateImportCrop(toStdString(id), rect));
    }

    if (action == QStringLiteral("export_current")) {
        const ToolWorkerResult editorResult = applyEditorStateFromArguments(session, targetId, arguments);
        if (editorResult != TOOL_WORKER_SUCCESS) {
            return editorResult;
        }
        const FlagManager::ExportResult result = session->core.exportCurrent();
        if (result.pendingOverwrite) {
            clearSessionError(session);
            return TOOL_WORKER_SUCCESS;
        }
        if (!result.success) {
            setSessionError(session, fromStdString(result.errorMessage));
            return TOOL_WORKER_ERROR_ACTION_FAILED;
        }
        session->previewBase64Cache.clear();
        session->previewSizeCache.clear();
        clearImportPreviewCaches(session);
        session->managePreviewWarmupLimit = 0;
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == QStringLiteral("export_all")) {
        const ToolWorkerResult editorResult = applyEditorStateFromArguments(session, targetId, arguments);
        if (editorResult != TOOL_WORKER_SUCCESS) {
            return editorResult;
        }
        const FlagManager::ExportResult result = session->core.exportAll();
        if (result.pendingOverwrite) {
            clearSessionError(session);
            return TOOL_WORKER_SUCCESS;
        }
        if (!result.success) {
            setSessionError(session, fromStdString(result.errorMessage));
            return TOOL_WORKER_ERROR_ACTION_FAILED;
        }
        session->previewBase64Cache.clear();
        session->previewSizeCache.clear();
        clearImportPreviewCaches(session);
        session->managePreviewWarmupLimit = 0;
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == QStringLiteral("confirm_overwrite")) {
        if (!session->core.confirmPendingOverwrite()) {
            return applyCoreResult(session, false);
        }
        session->previewBase64Cache.clear();
        session->previewSizeCache.clear();
        clearImportPreviewCaches(session);
        session->managePreviewWarmupLimit = 0;
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == QStringLiteral("cancel_overwrite")) {
        session->core.cancelPendingOverwrite();
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == QStringLiteral("set_mode")) {
        const ToolWorkerResult editorResult = applyEditorStateFromArguments(session, targetId, arguments);
        if (editorResult != TOOL_WORKER_SUCCESS) {
            return editorResult;
        }
    }

    std::map<std::string, std::string> params = toStringMap(arguments);
    if (targetId && *targetId) {
        params["targetId"] = targetId;
    }
    if (QString::fromUtf8(actionType ? actionType : "") == QStringLiteral("set_mode_manage")) {
        params["mode"] = "manage";
    } else if (QString::fromUtf8(actionType ? actionType : "") == QStringLiteral("set_mode_new")) {
        params["mode"] = "new";
    } else if (QString::fromUtf8(actionType ? actionType : "").startsWith(QStringLiteral("set_size_"))) {
        params["sizeIndex"] = toStdString(QString::fromUtf8(actionType).mid(QStringLiteral("set_size_").size()));
    }

    const ToolMode previousMode = session->core.buildSnapshot().mode;
    const ToolWorkerResult result = applyCoreResult(session, session->core.handleAction(toStdString(action), params));
    const ToolMode currentMode = session->core.buildSnapshot().mode;
    if (result == TOOL_WORKER_SUCCESS
        && (action == QStringLiteral("remove_import")
            || action == QStringLiteral("remove_selected")
            || action == QStringLiteral("remove_from_list"))) {
        clearImportPreviewCaches(session);
    }
    if (previousMode != currentMode) {
        session->previewBase64Cache.clear();
        session->previewSizeCache.clear();
        if (currentMode != ToolMode::New) {
            clearImportPreviewCaches(session);
        }
        session->managePreviewWarmupLimit = 0;
        if (result == TOOL_WORKER_SUCCESS && currentMode == ToolMode::Manage) {
            return applyCoreResult(session, session->core.refreshFlags());
        }
    }
    return result;
}

ToolWorkerResult initializeWorkerHandle(ToolWorkerHandle handle, const char* configJson) {
    return initializeSession(sessionFromHandle(handle), configJson);
}

const char* handleWorkerAction(ToolWorkerHandle handle,
                               const char* actionType,
                               const char* targetId,
                               const char* argumentsJson,
                               ToolWorkerResult* outResult) {
    WorkerSession* session = sessionFromHandle(handle);
    if (!session) {
        if (outResult) {
            *outResult = TOOL_WORKER_ERROR_INVALID_HANDLE;
        }
        return nullptr;
    }

    if (session->actionInProgress) {
        setSessionError(session, QStringLiteral("Flag Manager action is already running."));
        if (outResult) {
            *outResult = TOOL_WORKER_ERROR_ACTION_FAILED;
        }
        return serializeStatePacket(session);
    }

    session->actionInProgress = true;
    const ToolWorkerResult result = applyActionInternal(session, actionType, targetId, argumentsJson);
    session->actionInProgress = false;
    if (outResult) {
        *outResult = result;
    }
    char* state = serializeStatePacket(session);
    releaseRetiredImports(session);
    return state;
}

const char* getWorkerCurrentState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    WorkerSession* session = sessionFromHandle(handle);
    if (!session) {
        if (outResult) {
            *outResult = TOOL_WORKER_ERROR_INVALID_HANDLE;
        }
        return nullptr;
    }
    if (!ensureCoreInitialized(session)) {
        if (outResult) {
            *outResult = TOOL_WORKER_ERROR_INITIALIZATION_FAILED;
        }
        return serializeStatePacket(session);
    }
    if (outResult) {
        *outResult = TOOL_WORKER_SUCCESS;
    }
    return serializeStatePacket(session);
}

const char* getWorkerInitialState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    WorkerSession* session = sessionFromHandle(handle);
    if (!session) {
        if (outResult) {
            *outResult = TOOL_WORKER_ERROR_INVALID_HANDLE;
        }
        return nullptr;
    }
    const bool success = ensureCoreInitialized(session);
    if (outResult) {
        *outResult = success ? TOOL_WORKER_SUCCESS : TOOL_WORKER_ERROR_INITIALIZATION_FAILED;
    }
    return serializeStatePacket(session);
}

const char* getWorkerLastError(ToolWorkerHandle handle) {
    WorkerSession* session = sessionFromHandle(handle);
    return session ? session->lastError.c_str() : "Invalid worker handle.";
}

char* legacyCurrentStateJson(bool* ok) {
    if (!g_legacySession) {
        if (ok) {
            *ok = false;
        }
        const QByteArray payload = QJsonDocument(buildStatePacket(nullptr)).toJson(QJsonDocument::Compact);
        g_legacySerializedState = payload.toStdString();
        return allocateCString(payload);
    }

    if (ok) {
        *ok = true;
    }
    const QByteArray payload = QJsonDocument(buildStatePacket(g_legacySession.get())).toJson(QJsonDocument::Compact);
    g_legacySerializedState = payload.toStdString();
    return allocateCString(payload);
}

int initializeLegacyWorker(void* runtimeContext) {
    Q_UNUSED(runtimeContext);
    g_legacySession.reset(new WorkerSession());
    QJsonObject config;
    config[QStringLiteral("toolDirectory")] = QDir::currentPath();
    config[QStringLiteral("language")] = QStringLiteral("en_US");
    const QByteArray configJson = QJsonDocument(config).toJson(QJsonDocument::Compact);
    const ToolWorkerResult result = initializeSession(g_legacySession.get(), configJson.constData());
    if (result != TOOL_WORKER_SUCCESS) {
        g_legacyLastError = g_legacySession ? g_legacySession->lastError : std::string("Legacy worker initialization failed.");
        return 1;
    }
    g_legacyLastError.clear();
    return 0;
}

void shutdownLegacyWorker() {
    g_legacySession.reset();
    g_legacyLastError.clear();
    g_legacySerializedState.clear();
}

const char* getLegacyInitialState() {
    return legacyCurrentStateJson();
}

const char* handleLegacyAction(const char* actionJson) {
    if (!g_legacySession) {
        g_legacyLastError = "Legacy worker session is not initialized.";
        return legacyCurrentStateJson();
    }

    const QJsonObject object = parseJsonObject(actionJson);
    const QByteArray actionType = object.value(QStringLiteral("actionType")).toString().toUtf8();
    const QByteArray targetId = object.value(QStringLiteral("targetId")).toString().toUtf8();
    const QByteArray arguments = QJsonDocument(object.value(QStringLiteral("arguments")).toObject()).toJson(QJsonDocument::Compact);
    const ToolWorkerResult result = applyActionInternal(
        g_legacySession.get(),
        actionType.constData(),
        targetId.constData(),
        arguments.constData()
    );
    if (result == TOOL_WORKER_SUCCESS) {
        g_legacyLastError.clear();
    } else {
        g_legacyLastError = g_legacySession->lastError;
    }
    const char* state = legacyCurrentStateJson();
    releaseRetiredImports(g_legacySession.get());
    return state;
}

} // namespace FlagManagerBridge

extern "C" {

TOOL_WORKER_API ToolWorkerHandle ToolWorker_Create(const char* toolId) {
    return FlagManagerBridge::createWorkerHandle(toolId);
}

TOOL_WORKER_API void ToolWorker_Destroy(ToolWorkerHandle handle) {
    FlagManagerBridge::destroyWorkerHandle(handle);
}

TOOL_WORKER_API ToolWorkerResult ToolWorker_Initialize(ToolWorkerHandle handle, const char* configJson) {
    return FlagManagerBridge::initializeWorkerHandle(handle, configJson);
}

TOOL_WORKER_API const char* ToolWorker_HandleAction(
    ToolWorkerHandle handle,
    const char* actionType,
    const char* targetId,
    const char* argumentsJson,
    ToolWorkerResult* outResult
) {
    return FlagManagerBridge::handleWorkerAction(handle, actionType, targetId, argumentsJson, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetCurrentState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    return FlagManagerBridge::getWorkerCurrentState(handle, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetInitialState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    return FlagManagerBridge::getWorkerInitialState(handle, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetLastError(ToolWorkerHandle handle) {
    return FlagManagerBridge::getWorkerLastError(handle);
}

TOOL_WORKER_API void ToolWorker_FreeString(const char* value) {
    std::free(const_cast<char*>(value));
}

TOOL_WORKER_API const char* ToolWorker_GetVersion() {
    return "3.2.0";
}

TOOL_WORKER_API const char* tool_worker_get_version() {
    return ToolWorker_GetVersion();
}

TOOL_WORKER_API int tool_worker_initialize(void* runtimeContext) {
    return FlagManagerBridge::initializeLegacyWorker(runtimeContext);
}

TOOL_WORKER_API void tool_worker_shutdown() {
    FlagManagerBridge::shutdownLegacyWorker();
}

TOOL_WORKER_API const char* tool_worker_get_initial_state() {
    return FlagManagerBridge::getLegacyInitialState();
}

TOOL_WORKER_API const char* tool_worker_handle_action(const char* actionJson) {
    return FlagManagerBridge::handleLegacyAction(actionJson);
}

TOOL_WORKER_API void tool_worker_free_string(char* value) {
    std::free(value);
}

} // extern "C"
