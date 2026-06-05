//-------------------------------------------------------------------------------------
// ImageBuffer.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LUMORPHA_IMAGE_BUFFER_H
#define LUMORPHA_IMAGE_BUFFER_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Lumorpha {

struct Rgba8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
};

struct ImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<Rgba8> pixels;
};

bool isValidImage(const ImageBuffer& image);
bool imageDimensionsAreSafe(int width, int height);
std::size_t checkedPixelCount(int width, int height);

} // namespace Lumorpha

#endif // LUMORPHA_IMAGE_BUFFER_H
