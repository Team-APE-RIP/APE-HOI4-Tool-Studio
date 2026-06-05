//-------------------------------------------------------------------------------------
// ImageBuffer.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ImageBuffer.h"

#include <limits>

namespace Lumorpha {

namespace {
constexpr int kMaxImageDimension = 32768;
constexpr std::size_t kMaxPixelCount = 268435456ULL;
} // namespace

bool imageDimensionsAreSafe(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (width > kMaxImageDimension || height > kMaxImageDimension) {
        return false;
    }
    return checkedPixelCount(width, height) <= kMaxPixelCount;
}

std::size_t checkedPixelCount(int width, int height) {
    if (width <= 0 || height <= 0) {
        return 0;
    }

    const auto safeWidth = static_cast<std::size_t>(width);
    const auto safeHeight = static_cast<std::size_t>(height);
    if (safeWidth > std::numeric_limits<std::size_t>::max() / safeHeight) {
        return 0;
    }
    return safeWidth * safeHeight;
}

bool isValidImage(const ImageBuffer& image) {
    if (!imageDimensionsAreSafe(image.width, image.height)) {
        return false;
    }
    return image.pixels.size() == checkedPixelCount(image.width, image.height);
}

} // namespace Lumorpha
