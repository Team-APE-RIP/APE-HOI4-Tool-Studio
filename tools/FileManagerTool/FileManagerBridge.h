//-------------------------------------------------------------------------------------
// FileManagerBridge.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FILEMANAGERBRIDGE_H
#define FILEMANAGERBRIDGE_H

#include "../../src/ToolWorkerInterface.h"
#include "main/FileManagerCore.h"

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>

#include <memory>
#include <string>

namespace FileManagerBridge {

struct WorkerSession {
    FileManager::FileManagerCore core;
    std::unique_ptr<FileManager::IFileSystem> fileSystem;
    QMap<QString, QString> localizedStrings;
    std::string lastError;
    std::string lastSerializedState;
};

extern std::unique_ptr<WorkerSession> g_legacySession;
extern std::string g_legacyLastError;
extern std::string g_legacySerializedState;

ToolWorkerHandle createWorkerHandle(const char* toolId);
void destroyWorkerHandle(ToolWorkerHandle handle);
WorkerSession* sessionFromHandle(ToolWorkerHandle handle);

QJsonObject buildStatePacket(WorkerSession* session);
char* serializeStatePacket(WorkerSession* session);
QJsonObject parseJsonObject(const char* jsonText);
ToolWorkerResult initializeSession(WorkerSession* session, const char* configJson);
ToolWorkerResult applyActionInternal(WorkerSession* session,
                                     const char* actionType,
                                     const char* targetId,
                                     const char* argumentsJson);

ToolWorkerResult initializeWorkerHandle(ToolWorkerHandle handle, const char* configJson);
const char* handleWorkerAction(ToolWorkerHandle handle,
                               const char* actionType,
                               const char* targetId,
                               const char* argumentsJson,
                               ToolWorkerResult* outResult);
const char* getWorkerCurrentState(ToolWorkerHandle handle, ToolWorkerResult* outResult);
const char* getWorkerInitialState(ToolWorkerHandle handle, ToolWorkerResult* outResult);
const char* getWorkerLastError(ToolWorkerHandle handle);

int initializeLegacyWorker(void* runtimeContext);
void shutdownLegacyWorker();
const char* getLegacyInitialState();
const char* handleLegacyAction(const char* actionJson);
char* legacyCurrentStateJson(bool* ok = nullptr);

void setSessionError(WorkerSession* session, const QString& message);
void clearSessionError(WorkerSession* session);
char* allocateCString(const QByteArray& utf8);

} // namespace FileManagerBridge

#endif // FILEMANAGERBRIDGE_H
