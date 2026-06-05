//-------------------------------------------------------------------------------------
// FontManagerBridge.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FONTMANAGERBRIDGE_H
#define FONTMANAGERBRIDGE_H

#include "../../src/ToolWorkerInterface.h"
#include "main/FontFileSystem.h"
#include "main/FontManagerCore.h"

#include <QJsonObject>
#include <QMap>
#include <QString>

#include <memory>
#include <string>

namespace FontManagerBridge {

struct WorkerSession {
    FontManager::FontManagerCore core;
    std::unique_ptr<FontManager::IFontFileSystem> fileSystem;
    std::unique_ptr<FontManager::IDryadAtlasService> dryadAtlas;
    QString toolDirectoryPath;
    QMap<QString, QString> localizedStrings;
    QString gameLanguage;
    QMap<QString, QString> gameLanguageNames;
    bool coreInitialized = false;
    bool actionInProgress = false;
    std::string lastError;
    std::string lastSerializedState;
};

ToolWorkerHandle createWorkerHandle(const char* toolId);
void destroyWorkerHandle(ToolWorkerHandle handle);
WorkerSession* sessionFromHandle(ToolWorkerHandle handle);

QJsonObject parseJsonObject(const char* jsonText);
QJsonObject buildStatePacket(WorkerSession* session);
char* serializeStatePacket(WorkerSession* session);
ToolWorkerResult initializeSession(WorkerSession* session, const char* configJson);
ToolWorkerResult applyActionInternal(WorkerSession* session,
                                     const char* actionType,
                                     const char* targetId,
                                     const char* argumentsJson);
char* allocateCString(const QByteArray& utf8);

} // namespace FontManagerBridge

#endif // FONTMANAGERBRIDGE_H
