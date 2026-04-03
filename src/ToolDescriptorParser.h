//-------------------------------------------------------------------------------------
// ToolDescriptorParser.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLDESCRIPTORPARSER_H
#define TOOLDESCRIPTORPARSER_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace ToolDescriptorParser {
bool parseDescriptorFile(const QString& filePath, QJsonObject& outMetaData, QString* errorMessage = nullptr);
QStringList extractDependencies(const QJsonObject& metaData);
}

#endif // TOOLDESCRIPTORPARSER_H