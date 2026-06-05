//-------------------------------------------------------------------------------------
// LumorphaPluginExports.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "LumorphaExports.h"

#include "../../src/PluginAbi.h"

#include "main/Codecs/ImageCodec.h"
#include "main/Processing/ImageProcessor.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <new>
#include <string>
#include <vector>

namespace {

thread_local std::string g_lastError;

void clearError() {
    g_lastError.clear();
}

void setError(const std::string& message) {
    g_lastError = message;
}

Lumorpha::ImageFormat toImageFormat(std::uint32_t value) {
    switch (value) {
    case APE_LUMORPHA_FORMAT_PNG:
        return Lumorpha::ImageFormat::Png;
    case APE_LUMORPHA_FORMAT_JPEG:
        return Lumorpha::ImageFormat::Jpeg;
    case APE_LUMORPHA_FORMAT_BMP:
        return Lumorpha::ImageFormat::Bmp;
    case APE_LUMORPHA_FORMAT_TIFF:
        return Lumorpha::ImageFormat::Tiff;
    case APE_LUMORPHA_FORMAT_GIF:
        return Lumorpha::ImageFormat::Gif;
    case APE_LUMORPHA_FORMAT_TGA:
        return Lumorpha::ImageFormat::Tga;
    case APE_LUMORPHA_FORMAT_DDS:
        return Lumorpha::ImageFormat::Dds;
    case APE_LUMORPHA_FORMAT_HDR:
        return Lumorpha::ImageFormat::Hdr;
    case APE_LUMORPHA_FORMAT_AUTO:
    default:
        return Lumorpha::ImageFormat::Auto;
    }
}

Lumorpha::ResizeFilter toResizeFilter(std::uint32_t value) {
    switch (value) {
    case APE_LUMORPHA_FILTER_NEAREST:
        return Lumorpha::ResizeFilter::Nearest;
    case APE_LUMORPHA_FILTER_LINEAR:
        return Lumorpha::ResizeFilter::Linear;
    case APE_LUMORPHA_FILTER_CUBIC:
        return Lumorpha::ResizeFilter::Cubic;
    case APE_LUMORPHA_FILTER_BOX:
        return Lumorpha::ResizeFilter::Box;
    case APE_LUMORPHA_FILTER_TRIANGLE:
        return Lumorpha::ResizeFilter::Triangle;
    case APE_LUMORPHA_FILTER_LANCZOS3:
        return Lumorpha::ResizeFilter::Lanczos3;
    case APE_LUMORPHA_FILTER_AUTO:
    default:
        return Lumorpha::ResizeFilter::Auto;
    }
}

bool copyToOutputImage(const Lumorpha::ImageBuffer& image, LumorphaImageData* outImage) {
    if (!outImage) {
        setError("Output image pointer is null.");
        return false;
    }
    std::memset(outImage, 0, sizeof(*outImage));

    if (!Lumorpha::isValidImage(image)) {
        setError("Image result is invalid.");
        return false;
    }

    const std::size_t pixelCount = Lumorpha::checkedPixelCount(image.width, image.height);
    if (pixelCount > std::numeric_limits<std::uint32_t>::max() / 4U) {
        setError("Image buffer is too large.");
        return false;
    }

    const std::size_t byteCount = pixelCount * 4U;
    auto* pixels = static_cast<unsigned char*>(std::malloc(byteCount));
    if (!pixels) {
        setError("Failed to allocate output image.");
        return false;
    }

    for (std::size_t i = 0; i < pixelCount; ++i) {
        const Lumorpha::Rgba8 pixel = image.pixels[i];
        pixels[i * 4U + 0U] = pixel.r;
        pixels[i * 4U + 1U] = pixel.g;
        pixels[i * 4U + 2U] = pixel.b;
        pixels[i * 4U + 3U] = pixel.a;
    }

    outImage->width = static_cast<std::uint32_t>(image.width);
    outImage->height = static_cast<std::uint32_t>(image.height);
    outImage->stride = static_cast<std::uint32_t>(image.width * 4);
    outImage->pixelFormat = APE_LUMORPHA_PIXEL_RGBA8;
    outImage->byteSize = static_cast<std::uint32_t>(byteCount);
    outImage->pixels = pixels;
    return true;
}

Lumorpha::ImageBuffer imageFromView(const LumorphaImageView* view) {
    if (!view || !view->pixels) {
        setError("Image view is null.");
        return {};
    }
    if (view->pixelFormat != APE_LUMORPHA_PIXEL_RGBA8) {
        setError("Only RGBA8 image views are supported.");
        return {};
    }
    if (view->width == 0 || view->height == 0) {
        setError("Image view dimensions or stride are invalid.");
        return {};
    }
    if (!Lumorpha::imageDimensionsAreSafe(static_cast<int>(view->width), static_cast<int>(view->height))) {
        setError("Image view dimensions are too large.");
        return {};
    }
    if (view->stride < view->width * 4U) {
        setError("Image view dimensions or stride are invalid.");
        return {};
    }

    Lumorpha::ImageBuffer image;
    image.width = static_cast<int>(view->width);
    image.height = static_cast<int>(view->height);
    image.pixels.resize(Lumorpha::checkedPixelCount(image.width, image.height));

    for (int y = 0; y < image.height; ++y) {
        const unsigned char* row = view->pixels + static_cast<std::size_t>(y) * view->stride;
        for (int x = 0; x < image.width; ++x) {
            const int offset = x * 4;
            image.pixels[static_cast<std::size_t>(y * image.width + x)] = {
                row[offset],
                row[offset + 1],
                row[offset + 2],
                row[offset + 3]
            };
        }
    }

    return image;
}

int statusFromImageError(bool unsupportedFormat) {
    return unsupportedFormat ? APE_LUMORPHA_STATUS_UNSUPPORTED_FORMAT : APE_LUMORPHA_STATUS_INTERNAL_ERROR;
}

} // namespace

APE_LUMORPHA_EXPORT const char* APE_Lumorpha_GetPluginName() {
    return "Lumorpha";
}

APE_LUMORPHA_EXPORT const char* APE_Lumorpha_GetLastError() {
    return g_lastError.c_str();
}

APE_LUMORPHA_EXPORT const char* APE_Lumorpha_GetSupportedFormatsJson() {
    return Lumorpha::supportedFormatsJson();
}

APE_LUMORPHA_EXPORT int APE_Lumorpha_DecodeImage(
    const unsigned char* data,
    std::uint32_t size,
    std::uint32_t formatHint,
    LumorphaImageData* outImage
) {
    if (!data || size == 0 || !outImage) {
        setError("Invalid decode arguments.");
        return APE_LUMORPHA_STATUS_INVALID_ARGUMENT;
    }

    try {
        std::string errorMessage;
        const Lumorpha::ImageBuffer image = Lumorpha::decodeImage(data, size, toImageFormat(formatHint), &errorMessage);
        if (!Lumorpha::isValidImage(image)) {
            setError(errorMessage.empty() ? "Failed to decode image." : errorMessage);
            return APE_LUMORPHA_STATUS_DECODE_FAILED;
        }
        if (!copyToOutputImage(image, outImage)) {
            return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
        }
        clearError();
        return APE_LUMORPHA_STATUS_OK;
    } catch (const std::bad_alloc&) {
        setError("Failed to allocate decode buffers.");
        return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
    } catch (const std::exception& ex) {
        setError(ex.what());
        return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
    } catch (...) {
        setError("Unexpected decode failure.");
        return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
    }
}

APE_LUMORPHA_EXPORT int APE_Lumorpha_EncodeImage(
    const LumorphaImageView* image,
    std::uint32_t outputFormat,
    std::uint32_t encodeFlags,
    unsigned char** outBytes,
    std::uint32_t* outSize
) {
    (void)encodeFlags;
    if (!outBytes || !outSize) {
        setError("Output byte buffer pointer is null.");
        return APE_LUMORPHA_STATUS_INVALID_ARGUMENT;
    }
    *outBytes = nullptr;
    *outSize = 0;

    try {
        const Lumorpha::ImageBuffer source = imageFromView(image);
        if (!Lumorpha::isValidImage(source)) {
            return APE_LUMORPHA_STATUS_INVALID_ARGUMENT;
        }

        std::vector<std::uint8_t> encoded;
        std::string errorMessage;
        if (!Lumorpha::encodeImage(source, toImageFormat(outputFormat), encoded, &errorMessage)) {
            setError(errorMessage.empty() ? "Failed to encode image." : errorMessage);
            return statusFromImageError(!errorMessage.empty() && errorMessage.find("Unsupported") != std::string::npos);
        }
        if (encoded.size() > std::numeric_limits<std::uint32_t>::max()) {
            setError("Encoded image is too large.");
            return APE_LUMORPHA_STATUS_BUFFER_TOO_LARGE;
        }

        auto* bytes = static_cast<unsigned char*>(std::malloc(encoded.size()));
        if (!bytes) {
            setError("Failed to allocate encoded image buffer.");
            return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
        }
        std::memcpy(bytes, encoded.data(), encoded.size());
        *outBytes = bytes;
        *outSize = static_cast<std::uint32_t>(encoded.size());
        clearError();
        return APE_LUMORPHA_STATUS_OK;
    } catch (const std::bad_alloc&) {
        setError("Failed to allocate encode buffers.");
        return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
    } catch (const std::exception& ex) {
        setError(ex.what());
        return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
    } catch (...) {
        setError("Unexpected encode failure.");
        return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
    }
}

APE_LUMORPHA_EXPORT int APE_Lumorpha_ResizeImage(
    const LumorphaImageView* image,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight,
    std::uint32_t filter,
    LumorphaImageData* outImage
) {
    if (!outImage || targetWidth == 0 || targetHeight == 0) {
        setError("Invalid resize arguments.");
        return APE_LUMORPHA_STATUS_INVALID_ARGUMENT;
    }

    try {
        const Lumorpha::ImageBuffer source = imageFromView(image);
        if (!Lumorpha::isValidImage(source)) {
            return APE_LUMORPHA_STATUS_INVALID_ARGUMENT;
        }
        const Lumorpha::ImageBuffer resized = Lumorpha::resizeImage(
            source,
            static_cast<int>(targetWidth),
            static_cast<int>(targetHeight),
            toResizeFilter(filter)
        );
        if (!copyToOutputImage(resized, outImage)) {
            return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
        }
        clearError();
        return APE_LUMORPHA_STATUS_OK;
    } catch (...) {
        setError("Unexpected resize failure.");
        return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
    }
}

APE_LUMORPHA_EXPORT int APE_Lumorpha_CropImage(
    const LumorphaImageView* image,
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t width,
    std::uint32_t height,
    LumorphaImageData* outImage
) {
    if (!outImage || width == 0 || height == 0) {
        setError("Invalid crop arguments.");
        return APE_LUMORPHA_STATUS_INVALID_ARGUMENT;
    }

    try {
        const Lumorpha::ImageBuffer source = imageFromView(image);
        if (!Lumorpha::isValidImage(source)) {
            return APE_LUMORPHA_STATUS_INVALID_ARGUMENT;
        }
        const Lumorpha::CropRect rect{
            static_cast<int>(x),
            static_cast<int>(y),
            static_cast<int>(width),
            static_cast<int>(height)
        };
        const Lumorpha::ImageBuffer cropped = Lumorpha::cropImage(source, rect);
        if (!copyToOutputImage(cropped, outImage)) {
            return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
        }
        clearError();
        return APE_LUMORPHA_STATUS_OK;
    } catch (...) {
        setError("Unexpected crop failure.");
        return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
    }
}

APE_LUMORPHA_EXPORT int APE_Lumorpha_CropResizeImage(
    const LumorphaImageView* image,
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight,
    std::uint32_t filter,
    LumorphaImageData* outImage
) {
    if (!outImage || width == 0 || height == 0 || targetWidth == 0 || targetHeight == 0) {
        setError("Invalid crop-resize arguments.");
        return APE_LUMORPHA_STATUS_INVALID_ARGUMENT;
    }

    try {
        const Lumorpha::ImageBuffer source = imageFromView(image);
        if (!Lumorpha::isValidImage(source)) {
            return APE_LUMORPHA_STATUS_INVALID_ARGUMENT;
        }
        const Lumorpha::CropRect rect{
            static_cast<int>(x),
            static_cast<int>(y),
            static_cast<int>(width),
            static_cast<int>(height)
        };
        const Lumorpha::ImageBuffer result = Lumorpha::cropResizeImage(
            source,
            rect,
            static_cast<int>(targetWidth),
            static_cast<int>(targetHeight),
            toResizeFilter(filter)
        );
        if (!copyToOutputImage(result, outImage)) {
            return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
        }
        clearError();
        return APE_LUMORPHA_STATUS_OK;
    } catch (...) {
        setError("Unexpected crop-resize failure.");
        return APE_LUMORPHA_STATUS_INTERNAL_ERROR;
    }
}

APE_LUMORPHA_EXPORT void APE_Lumorpha_FreeImage(LumorphaImageData* image) {
    if (!image) {
        return;
    }
    std::free(image->pixels);
    std::memset(image, 0, sizeof(*image));
}

APE_LUMORPHA_EXPORT void APE_Lumorpha_FreeBytes(unsigned char* bytes) {
    std::free(bytes);
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

bool readU32(const std::uint8_t*& cursor, const std::uint8_t* end, std::uint32_t* outValue) {
    if (!outValue || !cursor || cursor + sizeof(std::uint32_t) > end) {
        return false;
    }
    std::uint32_t value = 0;
    std::memcpy(&value, cursor, sizeof(value));
    cursor += sizeof(value);
    *outValue = value;
    return true;
}

void appendU32(std::vector<std::uint8_t>* payload, std::uint32_t value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    payload->insert(payload->end(), bytes, bytes + sizeof(value));
}

bool setAbiPayload(ApePluginAbiResponse* response, const std::uint8_t* payload, std::size_t size, std::uint32_t contentType) {
    if (!response) {
        return false;
    }
    response->contentType = contentType;
    response->payloadSize = static_cast<std::uint64_t>(size);
    if (size == 0) {
        response->payload = nullptr;
        return true;
    }
    auto* bytes = static_cast<std::uint8_t*>(std::malloc(size));
    if (!bytes) {
        return false;
    }
    std::memcpy(bytes, payload, size);
    response->payload = bytes;
    return true;
}

bool setAbiPayload(ApePluginAbiResponse* response, const std::vector<std::uint8_t>& payload, std::uint32_t contentType) {
    return setAbiPayload(response, payload.data(), payload.size(), contentType);
}

void setAbiError(ApePluginAbiResponse* response, std::uint32_t status, const char* message) {
    if (!response) {
        return;
    }
    const char* safeMessage = message && message[0] ? message : "Lumorpha operation failed.";
    const std::size_t size = std::strlen(safeMessage);
    char* text = static_cast<char*>(std::malloc(size + 1));
    if (text) {
        std::memcpy(text, safeMessage, size);
        text[size] = '\0';
    }
    response->status = status;
    response->errorUtf8 = text;
}

std::uint32_t abiStatusFromLumorphaStatus(int status) {
    switch (status) {
    case APE_LUMORPHA_STATUS_OK:
        return APE_PLUGIN_ABI_STATUS_OK;
    case APE_LUMORPHA_STATUS_INVALID_ARGUMENT:
        return APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT;
    case APE_LUMORPHA_STATUS_UNSUPPORTED_FORMAT:
        return APE_PLUGIN_ABI_STATUS_UNSUPPORTED_OPERATION;
    case APE_LUMORPHA_STATUS_BUFFER_TOO_LARGE:
        return APE_PLUGIN_ABI_STATUS_BUFFER_TOO_LARGE;
    default:
        return APE_PLUGIN_ABI_STATUS_PLUGIN_ERROR;
    }
}

bool appendImageData(std::vector<std::uint8_t>* payload, const LumorphaImageData& image) {
    if (!payload || !image.pixels || image.byteSize == 0) {
        return false;
    }
    appendU32(payload, image.width);
    appendU32(payload, image.height);
    appendU32(payload, image.stride);
    appendU32(payload, image.pixelFormat);
    appendU32(payload, image.byteSize);
    payload->insert(payload->end(), image.pixels, image.pixels + image.byteSize);
    return true;
}

bool readImageView(const std::uint8_t*& cursor, const std::uint8_t* end, LumorphaImageView* outImage) {
    if (!outImage) {
        return false;
    }
    std::uint32_t byteSize = 0;
    if (!readU32(cursor, end, &outImage->width)
        || !readU32(cursor, end, &outImage->height)
        || !readU32(cursor, end, &outImage->stride)
        || !readU32(cursor, end, &outImage->pixelFormat)
        || !readU32(cursor, end, &byteSize)) {
        return false;
    }
    if (cursor + byteSize > end) {
        return false;
    }
    outImage->pixels = cursor;
    cursor += byteSize;
    return true;
}

int finishLumorphaImageResponse(ApePluginAbiResponse* response, int status, LumorphaImageData* image) {
    if (status != APE_LUMORPHA_STATUS_OK) {
        setAbiError(response, abiStatusFromLumorphaStatus(status), APE_Lumorpha_GetLastError());
        return 1;
    }
    std::vector<std::uint8_t> payload;
    if (!appendImageData(&payload, *image) || !setAbiPayload(response, payload, APE_PLUGIN_ABI_CONTENT_BINARY_ENVELOPE)) {
        APE_Lumorpha_FreeImage(image);
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR, "Failed to allocate Lumorpha response.");
        return 1;
    }
    APE_Lumorpha_FreeImage(image);
    response->status = APE_PLUGIN_ABI_STATUS_OK;
    return 0;
}

int invokeLumorphaDecodeImage(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    const std::uint8_t* cursor = request->payload.data;
    const std::uint8_t* end = cursor ? cursor + request->payload.size : nullptr;
    std::uint32_t formatHint = APE_LUMORPHA_FORMAT_AUTO;
    if (!readU32(cursor, end, &formatHint) || cursor >= end) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, "Invalid Lumorpha decode payload.");
        return 1;
    }

    LumorphaImageData image{};
    const int status = APE_Lumorpha_DecodeImage(cursor, static_cast<std::uint32_t>(end - cursor), formatHint, &image);
    return finishLumorphaImageResponse(response, status, &image);
}

int invokeLumorphaEncodeImage(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    const std::uint8_t* cursor = request->payload.data;
    const std::uint8_t* end = cursor ? cursor + request->payload.size : nullptr;
    LumorphaImageView image{};
    std::uint32_t outputFormat = APE_LUMORPHA_FORMAT_PNG;
    std::uint32_t encodeFlags = 0;
    if (!readImageView(cursor, end, &image)
        || !readU32(cursor, end, &outputFormat)
        || !readU32(cursor, end, &encodeFlags)) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, "Invalid Lumorpha encode payload.");
        return 1;
    }

    unsigned char* bytes = nullptr;
    std::uint32_t size = 0;
    const int status = APE_Lumorpha_EncodeImage(&image, outputFormat, encodeFlags, &bytes, &size);
    if (status != APE_LUMORPHA_STATUS_OK) {
        setAbiError(response, abiStatusFromLumorphaStatus(status), APE_Lumorpha_GetLastError());
        return 1;
    }
    if (!setAbiPayload(response, bytes, size, APE_PLUGIN_ABI_CONTENT_BINARY)) {
        APE_Lumorpha_FreeBytes(bytes);
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR, "Failed to allocate Lumorpha encoded response.");
        return 1;
    }
    APE_Lumorpha_FreeBytes(bytes);
    response->status = APE_PLUGIN_ABI_STATUS_OK;
    return 0;
}

int invokeLumorphaResizeImage(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    const std::uint8_t* cursor = request->payload.data;
    const std::uint8_t* end = cursor ? cursor + request->payload.size : nullptr;
    LumorphaImageView image{};
    std::uint32_t targetWidth = 0;
    std::uint32_t targetHeight = 0;
    std::uint32_t filter = APE_LUMORPHA_FILTER_AUTO;
    if (!readImageView(cursor, end, &image)
        || !readU32(cursor, end, &targetWidth)
        || !readU32(cursor, end, &targetHeight)
        || !readU32(cursor, end, &filter)) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, "Invalid Lumorpha resize payload.");
        return 1;
    }

    LumorphaImageData resized{};
    const int status = APE_Lumorpha_ResizeImage(&image, targetWidth, targetHeight, filter, &resized);
    return finishLumorphaImageResponse(response, status, &resized);
}

int invokeLumorphaCropImage(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    const std::uint8_t* cursor = request->payload.data;
    const std::uint8_t* end = cursor ? cursor + request->payload.size : nullptr;
    LumorphaImageView image{};
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    if (!readImageView(cursor, end, &image)
        || !readU32(cursor, end, &x)
        || !readU32(cursor, end, &y)
        || !readU32(cursor, end, &width)
        || !readU32(cursor, end, &height)) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, "Invalid Lumorpha crop payload.");
        return 1;
    }

    LumorphaImageData cropped{};
    const int status = APE_Lumorpha_CropImage(&image, x, y, width, height, &cropped);
    return finishLumorphaImageResponse(response, status, &cropped);
}

int invokeLumorphaCropResizeImage(const ApePluginAbiRequest* request, ApePluginAbiResponse* response) {
    const std::uint8_t* cursor = request->payload.data;
    const std::uint8_t* end = cursor ? cursor + request->payload.size : nullptr;
    LumorphaImageView image{};
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t targetWidth = 0;
    std::uint32_t targetHeight = 0;
    std::uint32_t filter = APE_LUMORPHA_FILTER_AUTO;
    if (!readImageView(cursor, end, &image)
        || !readU32(cursor, end, &x)
        || !readU32(cursor, end, &y)
        || !readU32(cursor, end, &width)
        || !readU32(cursor, end, &height)
        || !readU32(cursor, end, &targetWidth)
        || !readU32(cursor, end, &targetHeight)
        || !readU32(cursor, end, &filter)) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, "Invalid Lumorpha crop-resize payload.");
        return 1;
    }

    LumorphaImageData result{};
    const int status = APE_Lumorpha_CropResizeImage(&image, x, y, width, height, targetWidth, targetHeight, filter, &result);
    return finishLumorphaImageResponse(response, status, &result);
}

} // namespace

APE_PLUGIN_ABI_EXPORT const char* APE_Plugin_GetName(void) {
    return "Lumorpha";
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
        setAbiError(response, APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT, "Invalid Lumorpha ABI request.");
        return 1;
    }
    if (request->abiVersion != APE_PLUGIN_ABI_VERSION) {
        setAbiError(response, APE_PLUGIN_ABI_STATUS_UNSUPPORTED_ABI, "Unsupported Lumorpha ABI version.");
        return 1;
    }

    const std::string operation(request->operationUtf8);
    if (operation == "lumorpha.decodeImage") {
        return invokeLumorphaDecodeImage(request, response);
    }
    if (operation == "lumorpha.encodeImage") {
        return invokeLumorphaEncodeImage(request, response);
    }
    if (operation == "lumorpha.resizeImage") {
        return invokeLumorphaResizeImage(request, response);
    }
    if (operation == "lumorpha.cropImage") {
        return invokeLumorphaCropImage(request, response);
    }
    if (operation == "lumorpha.cropResizeImage") {
        return invokeLumorphaCropResizeImage(request, response);
    }

    setAbiError(response, APE_PLUGIN_ABI_STATUS_UNSUPPORTED_OPERATION, "Unsupported Lumorpha operation.");
    return 1;
}
