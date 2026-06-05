//-------------------------------------------------------------------------------------
// ImageCodec.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// This file integrates Microsoft DirectXTex without modifying the original
// DirectXTex source files or removing their original MIT license headers.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ImageCodec.h"

#include "DirectXTex.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

#ifdef _WIN32
#include <objbase.h>
#include <wincodec.h>
#endif

namespace Lumorpha {

namespace {

std::string hresultText(HRESULT hr, const char* action) {
    std::ostringstream stream;
    stream << action << " failed with HRESULT 0x"
           << std::hex << std::uppercase << static_cast<unsigned long>(hr) << ".";
    return stream.str();
}

#ifdef _WIN32
bool ensureComInitialized(std::string* errorMessage) {
    thread_local HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(result)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = hresultText(result, "COM initialization");
    }
    return false;
}
#endif

bool startsWith(const std::uint8_t* data, std::size_t size, const char* signature, std::size_t signatureSize) {
    return size >= signatureSize && std::memcmp(data, signature, signatureSize) == 0;
}

ImageFormat detectFormat(const std::uint8_t* data, std::size_t size) {
    if (!data || size == 0) {
        return ImageFormat::Auto;
    }

    static constexpr std::uint8_t pngSignature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (size >= sizeof(pngSignature) && std::memcmp(data, pngSignature, sizeof(pngSignature)) == 0) {
        return ImageFormat::Png;
    }
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xD8) {
        return ImageFormat::Jpeg;
    }
    if (startsWith(data, size, "BM", 2)) {
        return ImageFormat::Bmp;
    }
    if (startsWith(data, size, "DDS ", 4)) {
        return ImageFormat::Dds;
    }
    if (startsWith(data, size, "GIF87a", 6) || startsWith(data, size, "GIF89a", 6)) {
        return ImageFormat::Gif;
    }
    if (startsWith(data, size, "II*\0", 4) || startsWith(data, size, "MM\0*", 4)) {
        return ImageFormat::Tiff;
    }
    if (startsWith(data, size, "#?RADIANCE", 10) || startsWith(data, size, "#?RGBE", 6)) {
        return ImageFormat::Hdr;
    }
    return ImageFormat::Auto;
}

bool loadWithFormat(const std::uint8_t* data,
                    std::size_t size,
                    ImageFormat format,
                    DirectX::TexMetadata* metadata,
                    DirectX::ScratchImage& image,
                    std::string* errorMessage) {
    HRESULT hr = E_FAIL;

    switch (format) {
    case ImageFormat::Dds:
        hr = DirectX::LoadFromDDSMemory(data, size, DirectX::DDS_FLAGS_NONE, metadata, image);
        break;
    case ImageFormat::Tga:
        hr = DirectX::LoadFromTGAMemory(data, size, DirectX::TGA_FLAGS_NONE, metadata, image);
        break;
    case ImageFormat::Hdr:
        hr = DirectX::LoadFromHDRMemory(data, size, metadata, image);
        break;
    case ImageFormat::Png:
    case ImageFormat::Jpeg:
    case ImageFormat::Bmp:
    case ImageFormat::Tiff:
    case ImageFormat::Gif:
#ifdef _WIN32
        hr = DirectX::LoadFromWICMemory(data, size, DirectX::WIC_FLAGS_NONE, metadata, image);
#else
        hr = E_NOTIMPL;
#endif
        break;
    case ImageFormat::Auto:
    default:
        hr = E_INVALIDARG;
        break;
    }

    if (FAILED(hr)) {
        if (errorMessage) {
            *errorMessage = hresultText(hr, "Image decode");
        }
        return false;
    }

    return true;
}

bool loadAuto(const std::uint8_t* data,
              std::size_t size,
              DirectX::TexMetadata* metadata,
              DirectX::ScratchImage& image,
              std::string* errorMessage) {
    const ImageFormat detected = detectFormat(data, size);
    if (detected != ImageFormat::Auto) {
        return loadWithFormat(data, size, detected, metadata, image, errorMessage);
    }

    const ImageFormat fallbackOrder[] = {
        ImageFormat::Png,
        ImageFormat::Jpeg,
        ImageFormat::Bmp,
        ImageFormat::Tiff,
        ImageFormat::Gif,
        ImageFormat::Tga,
        ImageFormat::Dds,
        ImageFormat::Hdr
    };

    std::string lastError;
    for (ImageFormat format : fallbackOrder) {
        DirectX::TexMetadata localMetadata{};
        DirectX::ScratchImage localImage;
        if (loadWithFormat(data, size, format, &localMetadata, localImage, &lastError)) {
            if (metadata) {
                *metadata = localMetadata;
            }
            image = std::move(localImage);
            return true;
        }
    }

    if (errorMessage) {
        *errorMessage = lastError.empty() ? "Unsupported image format." : lastError;
    }
    return false;
}

ImageBuffer imageBufferFromDirectXImage(const DirectX::Image& source, std::string* errorMessage) {
    if (!imageDimensionsAreSafe(static_cast<int>(source.width), static_cast<int>(source.height))) {
        if (errorMessage) {
            *errorMessage = "Image dimensions are too large.";
        }
        return {};
    }

    const DirectX::Image* readImage = &source;
    DirectX::ScratchImage convertedImage;

    if (source.format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        HRESULT hr = E_FAIL;
        if (DirectX::IsCompressed(source.format)) {
            DirectX::ScratchImage decompressedImage;
            hr = DirectX::Decompress(source, DXGI_FORMAT_R8G8B8A8_UNORM, decompressedImage);
            if (SUCCEEDED(hr)) {
                convertedImage = std::move(decompressedImage);
                readImage = convertedImage.GetImage(0, 0, 0);
            }
        } else {
            hr = DirectX::Convert(
                source,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                DirectX::TEX_FILTER_DEFAULT,
                DirectX::TEX_THRESHOLD_DEFAULT,
                convertedImage
            );
            if (SUCCEEDED(hr)) {
                readImage = convertedImage.GetImage(0, 0, 0);
            }
        }

        if (FAILED(hr) || readImage == nullptr) {
            if (errorMessage) {
                *errorMessage = hresultText(hr, "Pixel conversion");
            }
            return {};
        }
    }

    ImageBuffer result;
    result.width = static_cast<int>(readImage->width);
    result.height = static_cast<int>(readImage->height);
    result.pixels.resize(checkedPixelCount(result.width, result.height));

    for (int y = 0; y < result.height; ++y) {
        const auto* sourceRow = readImage->pixels + static_cast<std::size_t>(y) * readImage->rowPitch;
        for (int x = 0; x < result.width; ++x) {
            const int sourceOffset = x * 4;
            result.pixels[static_cast<std::size_t>(y * result.width + x)] = {
                sourceRow[sourceOffset],
                sourceRow[sourceOffset + 1],
                sourceRow[sourceOffset + 2],
                sourceRow[sourceOffset + 3]
            };
        }
    }

    return result;
}

bool imageToDirectXBytes(const ImageBuffer& image, std::vector<std::uint8_t>& output) {
    if (!isValidImage(image)) {
        return false;
    }

    const std::size_t pixelCount = checkedPixelCount(image.width, image.height);
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4U) {
        return false;
    }

    output.resize(pixelCount * 4U);
    for (std::size_t i = 0; i < pixelCount; ++i) {
        const Rgba8 pixel = image.pixels[i];
        output[i * 4U + 0U] = pixel.r;
        output[i * 4U + 1U] = pixel.g;
        output[i * 4U + 2U] = pixel.b;
        output[i * 4U + 3U] = pixel.a;
    }
    return true;
}

#ifdef _WIN32
bool wicCodecForFormat(ImageFormat format, DirectX::WICCodecs* codec) {
    switch (format) {
    case ImageFormat::Png:
        *codec = DirectX::WIC_CODEC_PNG;
        return true;
    case ImageFormat::Jpeg:
        *codec = DirectX::WIC_CODEC_JPEG;
        return true;
    case ImageFormat::Bmp:
        *codec = DirectX::WIC_CODEC_BMP;
        return true;
    case ImageFormat::Tiff:
        *codec = DirectX::WIC_CODEC_TIFF;
        return true;
    case ImageFormat::Gif:
        *codec = DirectX::WIC_CODEC_GIF;
        return true;
    default:
        return false;
    }
}
#endif

bool copyBlob(const DirectX::Blob& blob, std::vector<std::uint8_t>& output) {
    const std::size_t size = blob.GetBufferSize();
    const std::uint8_t* begin = blob.GetConstBufferPointer();
    if (!begin || size == 0) {
        return false;
    }
    output.assign(begin, begin + size);
    return true;
}

} // namespace

ImageBuffer decodeImage(const std::uint8_t* data, std::size_t size, ImageFormat hint, std::string* errorMessage) {
    if (!data || size == 0) {
        if (errorMessage) {
            *errorMessage = "Image input buffer is empty.";
        }
        return {};
    }

#ifdef _WIN32
    if (!ensureComInitialized(errorMessage)) {
        return {};
    }
#endif

    DirectX::TexMetadata metadata{};
    DirectX::ScratchImage scratchImage;
    const bool loaded = hint == ImageFormat::Auto
        ? loadAuto(data, size, &metadata, scratchImage, errorMessage)
        : loadWithFormat(data, size, hint, &metadata, scratchImage, errorMessage);
    if (!loaded) {
        return {};
    }

    const DirectX::Image* image = scratchImage.GetImage(0, 0, 0);
    if (!image) {
        if (errorMessage) {
            *errorMessage = "Decoded image does not contain a readable frame.";
        }
        return {};
    }

    return imageBufferFromDirectXImage(*image, errorMessage);
}

bool encodeImage(const ImageBuffer& image, ImageFormat format, std::vector<std::uint8_t>& output, std::string* errorMessage) {
    output.clear();
    if (!isValidImage(image)) {
        if (errorMessage) {
            *errorMessage = "Image is invalid.";
        }
        return false;
    }
    if (format == ImageFormat::Auto) {
        format = ImageFormat::Png;
    }

#ifdef _WIN32
    if (!ensureComInitialized(errorMessage)) {
        return false;
    }
#endif

    std::vector<std::uint8_t> rgbaBytes;
    if (!imageToDirectXBytes(image, rgbaBytes)) {
        if (errorMessage) {
            *errorMessage = "Failed to prepare image pixels for encoding.";
        }
        return false;
    }

    DirectX::Image directImage{};
    directImage.width = static_cast<std::size_t>(image.width);
    directImage.height = static_cast<std::size_t>(image.height);
    directImage.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    directImage.rowPitch = static_cast<std::size_t>(image.width) * 4U;
    directImage.slicePitch = directImage.rowPitch * static_cast<std::size_t>(image.height);
    directImage.pixels = rgbaBytes.data();

    DirectX::Blob blob;
    HRESULT hr = E_FAIL;

    switch (format) {
    case ImageFormat::Tga:
        hr = DirectX::SaveToTGAMemory(directImage, DirectX::TGA_FLAGS_NONE, blob);
        break;
    case ImageFormat::Dds:
        hr = DirectX::SaveToDDSMemory(directImage, DirectX::DDS_FLAGS_NONE, blob);
        break;
#ifdef _WIN32
    case ImageFormat::Png:
    case ImageFormat::Jpeg:
    case ImageFormat::Bmp:
    case ImageFormat::Tiff:
    case ImageFormat::Gif: {
        DirectX::WICCodecs codec = DirectX::WIC_CODEC_PNG;
        if (!wicCodecForFormat(format, &codec)) {
            if (errorMessage) {
                *errorMessage = "Unsupported WIC output format.";
            }
            return false;
        }
        hr = DirectX::SaveToWICMemory(
            directImage,
            DirectX::WIC_FLAGS_NONE,
            DirectX::GetWICCodec(codec),
            blob,
            nullptr
        );
        break;
    }
#endif
    case ImageFormat::Hdr:
    default:
        if (errorMessage) {
            *errorMessage = "Unsupported output format for RGBA8 encoding.";
        }
        return false;
    }

    if (FAILED(hr) || !copyBlob(blob, output)) {
        if (errorMessage) {
            *errorMessage = hresultText(hr, "Image encode");
        }
        return false;
    }

    return true;
}

const char* supportedFormatsJson() {
    return "{\"decode\":[\"png\",\"jpeg\",\"bmp\",\"tiff\",\"gif\",\"tga\",\"dds\",\"hdr\"],"
           "\"encode\":[\"png\",\"jpeg\",\"bmp\",\"tiff\",\"gif\",\"tga\",\"dds\"],"
           "\"pixelFormat\":\"rgba8\","
           "\"resizeFilters\":[\"auto\",\"nearest\",\"linear\",\"cubic\",\"box\",\"triangle\",\"lanczos3\"]}";
}

} // namespace Lumorpha
