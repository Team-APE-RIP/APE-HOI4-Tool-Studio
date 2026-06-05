//-------------------------------------------------------------------------------------
// FlagImage.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "FlagImage.h"

#include <algorithm>
#include <cmath>

namespace FlagManager {

namespace {

std::uint32_t sampleClamped(const FlagImage& image, int x, int y) {
    if (!isValidImage(image)) {
        return makeRgba(0, 0, 0, 0);
    }
    x = std::clamp(x, 0, image.width - 1);
    y = std::clamp(y, 0, image.height - 1);
    return image.pixels[static_cast<std::size_t>(y * image.width + x)];
}

std::uint8_t blendChannel(std::uint8_t c00,
                          std::uint8_t c10,
                          std::uint8_t c01,
                          std::uint8_t c11,
                          double fx,
                          double fy) {
    const double top = static_cast<double>(c00) + (static_cast<double>(c10) - c00) * fx;
    const double bottom = static_cast<double>(c01) + (static_cast<double>(c11) - c01) * fx;
    const double value = top + (bottom - top) * fy;
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

} // namespace

bool isValidImage(const FlagImage& image) {
    return image.width > 0
        && image.height > 0
        && image.pixels.size() == static_cast<std::size_t>(image.width * image.height);
}

bool isValidCrop(const FlagImage& image, const Rect& crop) {
    if (!isValidImage(image)) {
        return false;
    }
    return crop.left >= 0
        && crop.top >= 0
        && crop.right < image.width
        && crop.bottom < image.height
        && crop.left < crop.right
        && crop.top < crop.bottom;
}

std::uint32_t makeRgba(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha) {
    return (static_cast<std::uint32_t>(alpha) << 24)
        | (static_cast<std::uint32_t>(red) << 16)
        | (static_cast<std::uint32_t>(green) << 8)
        | static_cast<std::uint32_t>(blue);
}

std::uint8_t rgbaRed(std::uint32_t pixel) {
    return static_cast<std::uint8_t>((pixel >> 16) & 0xFF);
}

std::uint8_t rgbaGreen(std::uint32_t pixel) {
    return static_cast<std::uint8_t>((pixel >> 8) & 0xFF);
}

std::uint8_t rgbaBlue(std::uint32_t pixel) {
    return static_cast<std::uint8_t>(pixel & 0xFF);
}

std::uint8_t rgbaAlpha(std::uint32_t pixel) {
    return static_cast<std::uint8_t>((pixel >> 24) & 0xFF);
}

FlagImage cropImage(const FlagImage& image, const Rect& crop) {
    if (!isValidCrop(image, crop)) {
        return {};
    }

    FlagImage result;
    result.width = crop.right - crop.left + 1;
    result.height = crop.bottom - crop.top + 1;
    result.pixels.resize(static_cast<std::size_t>(result.width * result.height));

    for (int y = 0; y < result.height; ++y) {
        const std::size_t sourceOffset = static_cast<std::size_t>((crop.top + y) * image.width + crop.left);
        const std::size_t destOffset = static_cast<std::size_t>(y * result.width);
        std::copy_n(image.pixels.begin() + static_cast<std::ptrdiff_t>(sourceOffset),
                    result.width,
                    result.pixels.begin() + static_cast<std::ptrdiff_t>(destOffset));
    }
    return result;
}

FlagImage resizeImage(const FlagImage& image, int targetWidth, int targetHeight) {
    if (!isValidImage(image) || targetWidth <= 0 || targetHeight <= 0) {
        return {};
    }

    FlagImage result;
    result.width = targetWidth;
    result.height = targetHeight;
    result.pixels.resize(static_cast<std::size_t>(targetWidth * targetHeight));

    const double scaleX = targetWidth > 1
        ? static_cast<double>(image.width - 1) / static_cast<double>(targetWidth - 1)
        : 0.0;
    const double scaleY = targetHeight > 1
        ? static_cast<double>(image.height - 1) / static_cast<double>(targetHeight - 1)
        : 0.0;

    for (int y = 0; y < targetHeight; ++y) {
        const double sourceY = scaleY * y;
        const int y0 = static_cast<int>(std::floor(sourceY));
        const int y1 = y0 + 1;
        const double fy = sourceY - y0;

        for (int x = 0; x < targetWidth; ++x) {
            const double sourceX = scaleX * x;
            const int x0 = static_cast<int>(std::floor(sourceX));
            const int x1 = x0 + 1;
            const double fx = sourceX - x0;

            const std::uint32_t p00 = sampleClamped(image, x0, y0);
            const std::uint32_t p10 = sampleClamped(image, x1, y0);
            const std::uint32_t p01 = sampleClamped(image, x0, y1);
            const std::uint32_t p11 = sampleClamped(image, x1, y1);

            result.pixels[static_cast<std::size_t>(y * targetWidth + x)] = makeRgba(
                blendChannel(rgbaRed(p00), rgbaRed(p10), rgbaRed(p01), rgbaRed(p11), fx, fy),
                blendChannel(rgbaGreen(p00), rgbaGreen(p10), rgbaGreen(p01), rgbaGreen(p11), fx, fy),
                blendChannel(rgbaBlue(p00), rgbaBlue(p10), rgbaBlue(p01), rgbaBlue(p11), fx, fy),
                blendChannel(rgbaAlpha(p00), rgbaAlpha(p10), rgbaAlpha(p01), rgbaAlpha(p11), fx, fy)
            );
        }
    }

    return result;
}

FlagImage resizeCropImage(const FlagImage& image, const Rect& crop, int targetWidth, int targetHeight) {
    if (!isValidCrop(image, crop) || targetWidth <= 0 || targetHeight <= 0) {
        return {};
    }

    FlagImage result;
    result.width = targetWidth;
    result.height = targetHeight;
    result.pixels.resize(static_cast<std::size_t>(targetWidth * targetHeight));

    const int cropWidth = crop.right - crop.left + 1;
    const int cropHeight = crop.bottom - crop.top + 1;
    const double scaleX = targetWidth > 1
        ? static_cast<double>(cropWidth - 1) / static_cast<double>(targetWidth - 1)
        : 0.0;
    const double scaleY = targetHeight > 1
        ? static_cast<double>(cropHeight - 1) / static_cast<double>(targetHeight - 1)
        : 0.0;

    for (int y = 0; y < targetHeight; ++y) {
        const double sourceY = static_cast<double>(crop.top) + scaleY * y;
        const int y0 = static_cast<int>(std::floor(sourceY));
        const int y1 = y0 + 1;
        const double fy = sourceY - y0;

        for (int x = 0; x < targetWidth; ++x) {
            const double sourceX = static_cast<double>(crop.left) + scaleX * x;
            const int x0 = static_cast<int>(std::floor(sourceX));
            const int x1 = x0 + 1;
            const double fx = sourceX - x0;

            const std::uint32_t p00 = sampleClamped(image, x0, y0);
            const std::uint32_t p10 = sampleClamped(image, x1, y0);
            const std::uint32_t p01 = sampleClamped(image, x0, y1);
            const std::uint32_t p11 = sampleClamped(image, x1, y1);

            result.pixels[static_cast<std::size_t>(y * targetWidth + x)] = makeRgba(
                blendChannel(rgbaRed(p00), rgbaRed(p10), rgbaRed(p01), rgbaRed(p11), fx, fy),
                blendChannel(rgbaGreen(p00), rgbaGreen(p10), rgbaGreen(p01), rgbaGreen(p11), fx, fy),
                blendChannel(rgbaBlue(p00), rgbaBlue(p10), rgbaBlue(p01), rgbaBlue(p11), fx, fy),
                blendChannel(rgbaAlpha(p00), rgbaAlpha(p10), rgbaAlpha(p01), rgbaAlpha(p11), fx, fy)
            );
        }
    }

    return result;
}

FlagImage decodeTga(const std::vector<std::uint8_t>& data) {
    if (data.size() < 18) {
        return {};
    }

    const int idLength = data[0];
    const int colorMapType = data[1];
    const int imageType = data[2];
    const int width = data[12] | (data[13] << 8);
    const int height = data[14] | (data[15] << 8);
    const int bpp = data[16];
    const int descriptor = data[17];

    if (colorMapType != 0 || (imageType != 2 && imageType != 10)) {
        return {};
    }
    if ((bpp != 24 && bpp != 32) || width <= 0 || height <= 0) {
        return {};
    }

    const int bytesPerPixel = bpp / 8;
    const int pixelDataOffset = 18 + idLength;
    if (pixelDataOffset < 0 || static_cast<std::size_t>(pixelDataOffset) >= data.size()) {
        return {};
    }

    FlagImage image;
    image.width = width;
    image.height = height;
    image.pixels.assign(static_cast<std::size_t>(width * height), makeRgba(0, 0, 0, 0));

    const std::uint8_t* pixelData = data.data() + pixelDataOffset;
    const int pixelCount = width * height;
    const int maxDataSize = static_cast<int>(data.size()) - pixelDataOffset;

    auto writePixel = [&](int currentPixel, std::uint8_t blue, std::uint8_t green, std::uint8_t red, std::uint8_t alpha) {
        const int x = currentPixel % width;
        const int sourceY = currentPixel / width;
        const int destY = (descriptor & 0x20) ? sourceY : (height - 1 - sourceY);
        image.pixels[static_cast<std::size_t>(destY * width + x)] = makeRgba(red, green, blue, alpha);
    };

    if (imageType == 2) {
        for (int currentPixel = 0; currentPixel < pixelCount; ++currentPixel) {
            const int srcIndex = currentPixel * bytesPerPixel;
            if (srcIndex + bytesPerPixel > maxDataSize) {
                break;
            }

            const std::uint8_t blue = pixelData[srcIndex];
            const std::uint8_t green = pixelData[srcIndex + 1];
            const std::uint8_t red = pixelData[srcIndex + 2];
            const std::uint8_t alpha = bytesPerPixel == 4 ? pixelData[srcIndex + 3] : 255;
            writePixel(currentPixel, blue, green, red, alpha);
        }
        return image;
    }

    int currentPixel = 0;
    int dataIndex = 0;
    while (currentPixel < pixelCount && dataIndex < maxDataSize) {
        const std::uint8_t header = pixelData[dataIndex++];
        const int count = (header & 0x7F) + 1;

        if (header & 0x80) {
            if (dataIndex + bytesPerPixel > maxDataSize) {
                break;
            }

            const std::uint8_t blue = pixelData[dataIndex];
            const std::uint8_t green = pixelData[dataIndex + 1];
            const std::uint8_t red = pixelData[dataIndex + 2];
            const std::uint8_t alpha = bytesPerPixel == 4 ? pixelData[dataIndex + 3] : 255;
            dataIndex += bytesPerPixel;

            for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                writePixel(currentPixel, blue, green, red, alpha);
            }
        } else {
            for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                if (dataIndex + bytesPerPixel > maxDataSize) {
                    break;
                }

                const std::uint8_t blue = pixelData[dataIndex];
                const std::uint8_t green = pixelData[dataIndex + 1];
                const std::uint8_t red = pixelData[dataIndex + 2];
                const std::uint8_t alpha = bytesPerPixel == 4 ? pixelData[dataIndex + 3] : 255;
                dataIndex += bytesPerPixel;
                writePixel(currentPixel, blue, green, red, alpha);
            }
        }
    }

    return image;
}

std::vector<std::uint8_t> encodeTga32(const FlagImage& image) {
    if (!isValidImage(image)) {
        return {};
    }

    std::vector<std::uint8_t> output;
    output.resize(18 + static_cast<std::size_t>(image.width * image.height * 4));
    output[2] = 2;
    output[12] = static_cast<std::uint8_t>(image.width & 0xFF);
    output[13] = static_cast<std::uint8_t>((image.width >> 8) & 0xFF);
    output[14] = static_cast<std::uint8_t>(image.height & 0xFF);
    output[15] = static_cast<std::uint8_t>((image.height >> 8) & 0xFF);
    output[16] = 32;
    output[17] = 0;

    std::size_t offset = 18;
    for (int y = image.height - 1; y >= 0; --y) {
        for (int x = 0; x < image.width; ++x) {
            const std::uint32_t pixel = image.pixels[static_cast<std::size_t>(y * image.width + x)];
            output[offset++] = rgbaBlue(pixel);
            output[offset++] = rgbaGreen(pixel);
            output[offset++] = rgbaRed(pixel);
            output[offset++] = rgbaAlpha(pixel);
        }
    }

    return output;
}

} // namespace FlagManager
