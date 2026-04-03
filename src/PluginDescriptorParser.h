//-------------------------------------------------------------------------------------
// PluginDescriptorParser.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef PLUGINDESCRIPTORPARSER_H
#define PLUGINDESCRIPTORPARSER_H

#include <QString>
#include <QStringList>

struct PluginInfo {
    QString id;
    QString name;
    QString version;
    QString compatibleVersion;
    QString author;
    QString directoryPath;
    QString libraryPath;
    QString descriptorPath;
    QString licensePath;

    bool isValid() const {
        return !id.isEmpty() && !name.isEmpty() && !directoryPath.isEmpty() && !descriptorPath.isEmpty();
    }
};

namespace PluginDescriptorParser {
bool parseDescriptorFile(const QString& filePath, PluginInfo& outInfo, QString* errorMessage = nullptr);
}

#endif // PLUGINDESCRIPTORPARSER_H