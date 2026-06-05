//-------------------------------------------------------------------------------------
// PluginAbi.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef PLUGINABI_H
#define PLUGINABI_H

#include <stdint.h>

#ifdef _WIN32
#ifdef __cplusplus
#define APE_PLUGIN_ABI_EXPORT extern "C" __declspec(dllexport)
#else
#define APE_PLUGIN_ABI_EXPORT __declspec(dllexport)
#endif
#else
#ifdef __cplusplus
#define APE_PLUGIN_ABI_EXPORT extern "C"
#else
#define APE_PLUGIN_ABI_EXPORT
#endif
#endif

#define APE_PLUGIN_ABI_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ApePluginAbiContentType {
    APE_PLUGIN_ABI_CONTENT_NONE = 0,
    APE_PLUGIN_ABI_CONTENT_JSON_UTF8 = 1,
    APE_PLUGIN_ABI_CONTENT_BINARY = 2,
    APE_PLUGIN_ABI_CONTENT_BINARY_ENVELOPE = 3
} ApePluginAbiContentType;

typedef enum ApePluginAbiStatus {
    APE_PLUGIN_ABI_STATUS_OK = 0,
    APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT = 1,
    APE_PLUGIN_ABI_STATUS_UNSUPPORTED_ABI = 2,
    APE_PLUGIN_ABI_STATUS_UNSUPPORTED_OPERATION = 3,
    APE_PLUGIN_ABI_STATUS_UNAUTHORIZED = 4,
    APE_PLUGIN_ABI_STATUS_PLUGIN_UNAVAILABLE = 5,
    APE_PLUGIN_ABI_STATUS_PLUGIN_ERROR = 6,
    APE_PLUGIN_ABI_STATUS_BUFFER_TOO_LARGE = 7,
    APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR = 8
} ApePluginAbiStatus;

typedef struct ApePluginAbiBuffer {
    const uint8_t* data;
    uint64_t size;
} ApePluginAbiBuffer;

typedef struct ApePluginAbiRequest {
    uint32_t abiVersion;
    const char* pluginNameUtf8;
    const char* operationUtf8;
    uint32_t contentType;
    uint32_t flags;
    ApePluginAbiBuffer payload;
} ApePluginAbiRequest;

typedef struct ApePluginAbiResponse {
    uint32_t abiVersion;
    uint32_t status;
    uint32_t contentType;
    uint32_t flags;
    uint8_t* payload;
    uint64_t payloadSize;
    char* errorUtf8;
} ApePluginAbiResponse;

typedef int (*ApePluginInvokeFn)(const ApePluginAbiRequest*, ApePluginAbiResponse*);
typedef void (*ApePluginFreeResponseFn)(ApePluginAbiResponse*);
typedef const char* (*ApePluginGetNameFn)(void);
typedef uint32_t (*ApePluginGetAbiVersionFn)(void);

#ifdef __cplusplus
}
#endif

#endif // PLUGINABI_H
