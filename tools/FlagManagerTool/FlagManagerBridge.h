//-------------------------------------------------------------------------------------
// FlagManagerBridge.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FLAGMANAGERBRIDGE_H
#define FLAGMANAGERBRIDGE_H

#include "../../src/ToolWorkerInterface.h"
#include "main/FlagFileSystem.h"
#include "main/FlagManagerCore.h"

#include <QMap>
#include <QJsonObject>
#include <QSize>
#include <QString>

#include <memory>
#include <string>

namespace FlagManagerBridge {

struct WorkerSession {
    FlagManager::FlagManagerCore core;
    std::unique_ptr<FlagManager::IFileSystem> fileSystem;
    std::unique_ptr<FlagManager::IImagePipeline> imagePipeline;
    QString toolDirectoryPath;
    QString currentLanguageCode;
    QMap<QString, QString> localizedStrings;
    QMap<QString, QString> previewBase64Cache;
    QMap<QString, QSize> previewSizeCache;
    QMap<QString, QString> importThumbnailBase64Cache;
    QString currentImportPreviewId;
    QString currentImportPreviewBase64;
    int managePreviewWarmupLimit = 0;
    bool coreInitialized = false;
    bool actionInProgress = false;
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

} // namespace FlagManagerBridge

#endif // FLAGMANAGERBRIDGE_H
