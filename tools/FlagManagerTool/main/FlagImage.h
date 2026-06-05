//-------------------------------------------------------------------------------------
// FlagImage.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FLAGIMAGE_H
#define FLAGIMAGE_H

#include "FlagTypes.h"

#include <cstdint>
#include <vector>

namespace FlagManager {

bool isValidImage(const FlagImage& image);
bool isValidCrop(const FlagImage& image, const Rect& crop);
std::uint32_t makeRgba(std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255);
std::uint8_t rgbaRed(std::uint32_t pixel);
std::uint8_t rgbaGreen(std::uint32_t pixel);
std::uint8_t rgbaBlue(std::uint32_t pixel);
std::uint8_t rgbaAlpha(std::uint32_t pixel);

FlagImage cropImage(const FlagImage& image, const Rect& crop);
FlagImage resizeImage(const FlagImage& image, int targetWidth, int targetHeight);
FlagImage resizeCropImage(const FlagImage& image, const Rect& crop, int targetWidth, int targetHeight);
FlagImage decodeTga(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> encodeTga32(const FlagImage& image);

} // namespace FlagManager

#endif // FLAGIMAGE_H
