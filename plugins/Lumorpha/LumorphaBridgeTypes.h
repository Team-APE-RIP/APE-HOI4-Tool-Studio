//-------------------------------------------------------------------------------------
// LumorphaBridgeTypes.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LUMORPHA_BRIDGE_TYPES_H
#define LUMORPHA_BRIDGE_TYPES_H

#include <stdint.h>

#ifdef _WIN32
#define APE_LUMORPHA_EXPORT extern "C" __declspec(dllexport)
#else
#define APE_LUMORPHA_EXPORT extern "C"
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum LumorphaStatusCode {
    APE_LUMORPHA_STATUS_OK = 0,
    APE_LUMORPHA_STATUS_INVALID_ARGUMENT = 1,
    APE_LUMORPHA_STATUS_UNSUPPORTED_FORMAT = 2,
    APE_LUMORPHA_STATUS_DECODE_FAILED = 3,
    APE_LUMORPHA_STATUS_ENCODE_FAILED = 4,
    APE_LUMORPHA_STATUS_BUFFER_TOO_LARGE = 5,
    APE_LUMORPHA_STATUS_INTERNAL_ERROR = 6
};

enum LumorphaPixelFormat {
    APE_LUMORPHA_PIXEL_RGBA8 = 1
};

enum LumorphaImageFormat {
    APE_LUMORPHA_FORMAT_AUTO = 0,
    APE_LUMORPHA_FORMAT_PNG = 1,
    APE_LUMORPHA_FORMAT_JPEG = 2,
    APE_LUMORPHA_FORMAT_BMP = 3,
    APE_LUMORPHA_FORMAT_TIFF = 4,
    APE_LUMORPHA_FORMAT_GIF = 5,
    APE_LUMORPHA_FORMAT_TGA = 6,
    APE_LUMORPHA_FORMAT_DDS = 7,
    APE_LUMORPHA_FORMAT_HDR = 8
};

enum LumorphaResizeFilter {
    APE_LUMORPHA_FILTER_AUTO = 0,
    APE_LUMORPHA_FILTER_NEAREST = 1,
    APE_LUMORPHA_FILTER_LINEAR = 2,
    APE_LUMORPHA_FILTER_CUBIC = 3,
    APE_LUMORPHA_FILTER_BOX = 4,
    APE_LUMORPHA_FILTER_TRIANGLE = 5,
    APE_LUMORPHA_FILTER_LANCZOS3 = 6
};

typedef struct LumorphaImageView {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t pixelFormat;
    const unsigned char* pixels;
} LumorphaImageView;

typedef struct LumorphaImageData {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t pixelFormat;
    uint32_t byteSize;
    unsigned char* pixels;
} LumorphaImageData;

#ifdef __cplusplus
}
#endif

#endif // LUMORPHA_BRIDGE_TYPES_H
