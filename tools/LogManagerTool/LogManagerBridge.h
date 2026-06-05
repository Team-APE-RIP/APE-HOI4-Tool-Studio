//-------------------------------------------------------------------------------------
// LogManagerBridge.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LOGMANAGERBRIDGE_H
#define LOGMANAGERBRIDGE_H

#include "../../src/ToolWorkerInterface.h"
#include "main/LogManagerCore.h"
#include "main/LogFileSystem.h"

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <map>
#include <memory>
#include <string>

namespace LogManagerBridge {

// Worker session structure
struct WorkerSession {
    LogManager::LogManagerCore core;
    std::unique_ptr<LogManager::IFileSystem> fileSystem;
    QString toolDirectoryPath;
    QString currentLanguageCode;
    QMap<QString, QString> localizedStrings;
    std::string lastError;
    std::string lastSerializedState;
};

// Global legacy session for backward compatibility
extern std::unique_ptr<WorkerSession> g_legacySession;
extern std::string g_legacyLastError;
extern std::string g_legacySerializedState;

// Worker handle management
ToolWorkerHandle createWorkerHandle(const char* toolId);
void destroyWorkerHandle(ToolWorkerHandle handle);
WorkerSession* sessionFromHandle(ToolWorkerHandle handle);

// State building and serialization
QJsonObject buildStatePacket(WorkerSession* session);
char* serializeStatePacket(WorkerSession* session);

// JSON parsing
QJsonObject parseJsonObject(const char* jsonText);
std::map<std::string, std::string> toStringMap(const QJsonObject& object);

// Session operations
ToolWorkerResult initializeSession(WorkerSession* session, const char* configJson);
ToolWorkerResult applyActionInternal(WorkerSession* session,
                                     const char* actionType,
                                     const char* targetId,
                                     const char* argumentsJson);

// Worker interface implementations
ToolWorkerResult initializeWorkerHandle(ToolWorkerHandle handle, const char* configJson);
const char* handleWorkerAction(ToolWorkerHandle handle,
                               const char* actionType,
                               const char* targetId,
                               const char* argumentsJson,
                               ToolWorkerResult* outResult);
const char* getWorkerCurrentState(ToolWorkerHandle handle, ToolWorkerResult* outResult);
const char* getWorkerInitialState(ToolWorkerHandle handle, ToolWorkerResult* outResult);
const char* getWorkerLastError(ToolWorkerHandle handle);

// Legacy worker interface
int initializeLegacyWorker(void* runtimeContext);
void shutdownLegacyWorker();
const char* getLegacyInitialState();
const char* handleLegacyAction(const char* actionJson);
char* legacyCurrentStateJson(bool* ok = nullptr);

// Helper functions
void setSessionError(WorkerSession* session, const QString& message);
void clearSessionError(WorkerSession* session);
char* allocateCString(const QByteArray& utf8);

} // namespace LogManagerBridge

#endif // LOGMANAGERBRIDGE_H
