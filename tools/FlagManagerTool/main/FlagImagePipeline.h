//-------------------------------------------------------------------------------------
// FlagImagePipeline.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FLAGIMAGEPIPELINE_H
#define FLAGIMAGEPIPELINE_H

#include "FlagTypes.h"

#include <cstdint>
#include <vector>

namespace FlagManager {

class IImagePipeline {
public:
    virtual ~IImagePipeline() = default;

    virtual FlagImage cropResizeImage(const FlagImage& image,
                                      const Rect& crop,
                                      int targetWidth,
                                      int targetHeight) const = 0;
    virtual std::vector<std::uint8_t> encodeTga32(const FlagImage& image) const = 0;
};

} // namespace FlagManager

#endif // FLAGIMAGEPIPELINE_H
