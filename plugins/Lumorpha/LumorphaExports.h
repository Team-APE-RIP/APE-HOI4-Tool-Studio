//-------------------------------------------------------------------------------------
// LumorphaExports.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LUMORPHA_EXPORTS_H
#define LUMORPHA_EXPORTS_H

#include "LumorphaBridgeTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

APE_LUMORPHA_EXPORT const char* APE_Lumorpha_GetPluginName();
APE_LUMORPHA_EXPORT const char* APE_Lumorpha_GetLastError();
APE_LUMORPHA_EXPORT const char* APE_Lumorpha_GetSupportedFormatsJson();

APE_LUMORPHA_EXPORT int APE_Lumorpha_DecodeImage(
    const unsigned char* data,
    uint32_t size,
    uint32_t formatHint,
    LumorphaImageData* outImage
);

APE_LUMORPHA_EXPORT int APE_Lumorpha_EncodeImage(
    const LumorphaImageView* image,
    uint32_t outputFormat,
    uint32_t encodeFlags,
    unsigned char** outBytes,
    uint32_t* outSize
);

APE_LUMORPHA_EXPORT int APE_Lumorpha_ResizeImage(
    const LumorphaImageView* image,
    uint32_t targetWidth,
    uint32_t targetHeight,
    uint32_t filter,
    LumorphaImageData* outImage
);

APE_LUMORPHA_EXPORT int APE_Lumorpha_CropImage(
    const LumorphaImageView* image,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    LumorphaImageData* outImage
);

APE_LUMORPHA_EXPORT int APE_Lumorpha_CropResizeImage(
    const LumorphaImageView* image,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t targetWidth,
    uint32_t targetHeight,
    uint32_t filter,
    LumorphaImageData* outImage
);

APE_LUMORPHA_EXPORT void APE_Lumorpha_FreeImage(LumorphaImageData* image);
APE_LUMORPHA_EXPORT void APE_Lumorpha_FreeBytes(unsigned char* bytes);

#ifdef __cplusplus
}
#endif

#endif // LUMORPHA_EXPORTS_H
