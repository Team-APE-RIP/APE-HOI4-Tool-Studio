//-------------------------------------------------------------------------------------
// ImageCodec.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LUMORPHA_IMAGE_CODEC_H
#define LUMORPHA_IMAGE_CODEC_H

#include "../Core/ImageBuffer.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace Lumorpha {

enum class ImageFormat : std::uint32_t {
    Auto = 0,
    Png = 1,
    Jpeg = 2,
    Bmp = 3,
    Tiff = 4,
    Gif = 5,
    Tga = 6,
    Dds = 7,
    Hdr = 8
};

ImageBuffer decodeImage(const std::uint8_t* data, std::size_t size, ImageFormat hint, std::string* errorMessage);
bool encodeImage(const ImageBuffer& image, ImageFormat format, std::vector<std::uint8_t>& output, std::string* errorMessage);
const char* supportedFormatsJson();

} // namespace Lumorpha

#endif // LUMORPHA_IMAGE_CODEC_H
