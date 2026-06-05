//-------------------------------------------------------------------------------------
// ImageRenderer.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LUMORPHA_IMAGE_RENDERER_H
#define LUMORPHA_IMAGE_RENDERER_H

#include "../Core/ImageBuffer.h"
#include "../Processing/ImageProcessor.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Lumorpha {

bool renderPng(const ImageBuffer& image,
               int targetWidth,
               int targetHeight,
               ResizeFilter filter,
               std::vector<std::uint8_t>& output,
               std::string* errorMessage);

} // namespace Lumorpha

#endif // LUMORPHA_IMAGE_RENDERER_H
