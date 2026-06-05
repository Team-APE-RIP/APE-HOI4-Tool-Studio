//-------------------------------------------------------------------------------------
// ToolWorkerInterface.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLWORKERINTERFACE_H
#define TOOLWORKERINTERFACE_H

#ifdef _WIN32
    #ifdef TOOL_WORKER_EXPORTS
        #define TOOL_WORKER_API __declspec(dllexport)
    #else
        #define TOOL_WORKER_API __declspec(dllimport)
    #endif
#else
    #define TOOL_WORKER_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to worker instance
typedef void* ToolWorkerHandle;

// Result codes
typedef enum {
    TOOL_WORKER_SUCCESS = 0,
    TOOL_WORKER_ERROR_INVALID_HANDLE = 1,
    TOOL_WORKER_ERROR_INVALID_ARGUMENT = 2,
    TOOL_WORKER_ERROR_INITIALIZATION_FAILED = 3,
    TOOL_WORKER_ERROR_ACTION_FAILED = 4,
    TOOL_WORKER_ERROR_OUT_OF_MEMORY = 5,
    TOOL_WORKER_ERROR_UNKNOWN = 99
} ToolWorkerResult;

// Lifecycle management
TOOL_WORKER_API ToolWorkerHandle ToolWorker_Create(const char* toolId);
TOOL_WORKER_API void ToolWorker_Destroy(ToolWorkerHandle handle);
TOOL_WORKER_API ToolWorkerResult ToolWorker_Initialize(ToolWorkerHandle handle, const char* configJson);

// Action handling
// Returns JSON string containing the result state packet
// Caller must free the returned string using ToolWorker_FreeString
TOOL_WORKER_API const char* ToolWorker_HandleAction(
    ToolWorkerHandle handle,
    const char* actionType,
    const char* targetId,
    const char* argumentsJson,
    ToolWorkerResult* outResult
);

// State query
// Returns JSON string containing current state
// Caller must free the returned string using ToolWorker_FreeString
TOOL_WORKER_API const char* ToolWorker_GetCurrentState(
    ToolWorkerHandle handle,
    ToolWorkerResult* outResult
);

// Get initial UI state
// Returns JSON string containing initial state packet
// Caller must free the returned string using ToolWorker_FreeString
TOOL_WORKER_API const char* ToolWorker_GetInitialState(
    ToolWorkerHandle handle,
    ToolWorkerResult* outResult
);

// Error handling
TOOL_WORKER_API const char* ToolWorker_GetLastError(ToolWorkerHandle handle);

// Memory management
TOOL_WORKER_API void ToolWorker_FreeString(const char* str);

// Version info
TOOL_WORKER_API const char* ToolWorker_GetVersion();

#ifdef __cplusplus
}
#endif

#endif // TOOLWORKERINTERFACE_H
