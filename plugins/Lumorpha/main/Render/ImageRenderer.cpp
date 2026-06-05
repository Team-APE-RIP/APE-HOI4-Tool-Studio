//-------------------------------------------------------------------------------------
// ImageRenderer.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ImageRenderer.h"

#include "../Codecs/ImageCodec.h"

namespace Lumorpha {

bool renderPng(const ImageBuffer& image,
               int targetWidth,
               int targetHeight,
               ResizeFilter filter,
               std::vector<std::uint8_t>& output,
               std::string* errorMessage) {
    if (!isValidImage(image)) {
        if (errorMessage) {
            *errorMessage = "Image is invalid.";
        }
        return false;
    }

    const bool shouldResize = targetWidth > 0
        && targetHeight > 0
        && (targetWidth != image.width || targetHeight != image.height);
    const ImageBuffer rendered = shouldResize
        ? resizeImage(image, targetWidth, targetHeight, filter)
        : image;

    return encodeImage(rendered, ImageFormat::Png, output, errorMessage);
}

} // namespace Lumorpha
