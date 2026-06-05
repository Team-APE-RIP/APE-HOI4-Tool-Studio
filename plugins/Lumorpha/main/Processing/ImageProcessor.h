//-------------------------------------------------------------------------------------
// ImageProcessor.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LUMORPHA_IMAGE_PROCESSOR_H
#define LUMORPHA_IMAGE_PROCESSOR_H

#include "../Core/ImageBuffer.h"

#include <cstdint>

namespace Lumorpha {

enum class ResizeFilter : std::uint32_t {
    Auto = 0,
    Nearest = 1,
    Linear = 2,
    Cubic = 3,
    Box = 4,
    Triangle = 5,
    Lanczos3 = 6
};

struct CropRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

bool isValidCrop(const ImageBuffer& image, const CropRect& rect);
ImageBuffer cropImage(const ImageBuffer& image, const CropRect& rect);
ImageBuffer resizeImage(const ImageBuffer& image, int targetWidth, int targetHeight, ResizeFilter filter);
ImageBuffer cropResizeImage(const ImageBuffer& image,
                            const CropRect& rect,
                            int targetWidth,
                            int targetHeight,
                            ResizeFilter filter);

} // namespace Lumorpha

#endif // LUMORPHA_IMAGE_PROCESSOR_H
