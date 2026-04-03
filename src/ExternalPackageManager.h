//-------------------------------------------------------------------------------------
// ExternalPackageManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef EXTERNALPACKAGEMANAGER_H
#define EXTERNALPACKAGEMANAGER_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

class QWidget;

class ExternalPackageManager {
public:
    enum class RequestType {
        None,
        ToolDescriptor,
        PluginDescriptor
    };

    struct PendingRequest {
        RequestType type = RequestType::None;
        QString descriptorPath;
        bool overwriteApproved = false;

        bool isValid() const {
            return type != RequestType::None && !descriptorPath.trimmed().isEmpty();
        }
    };

    struct ImportResult {
        bool success = false;
        bool cancelled = false;
        bool alreadyInstalled = false;
        bool useInstalledCopy = false;
        bool requiresRestart = false;
        QString importedId;
        QString importedName;
        QString targetDirectoryPath;
        QString installedDescriptorPath;
        QString errorMessage;
    };

    static PendingRequest createPendingRequestFromPath(const QString& path);
    static bool savePendingRestartRequest(const PendingRequest& request, QString* errorMessage = nullptr);
    static PendingRequest takePendingRestartRequest(QString* errorMessage = nullptr);
    static void clearPendingRestartRequest();
    static ImportResult importToolPackage(const QString& descriptorPath,
                                          QWidget* parent,
                                          bool skipOverwritePrompt = false);
    static ImportResult importPluginPackage(const QString& descriptorPath,
                                            QWidget* parent,
                                            bool skipOverwritePrompt = false);

private:
    static QString pendingRestartRequestFilePath();
    static QString requestTypeToString(RequestType type);
    static RequestType requestTypeFromString(const QString& value);
    static QString appToolsRootPath();
    static QString appPluginsRootPath();
    static QString buildInstalledToolDescriptorPath(const QString& toolName);
    static QString buildInstalledPluginDescriptorPath(const QString& pluginName);
    static bool isSamePath(const QString& left, const QString& right);
    static QString findAppManagedRoot(const QString& childDirectoryName);
    static bool ensureDirectoryExists(const QString& path, QString* errorMessage);
    static bool copyFileWithOverwrite(const QString& sourcePath,
                                      const QString& targetPath,
                                      QString* errorMessage);
    static bool copyDirectoryRecursively(const QString& sourcePath,
                                         const QString& targetPath,
                                         QString* errorMessage);
    static bool removePathRecursively(const QString& path, QString* errorMessage);
    static bool directoryContainsLibrary(const QString& directoryPath);
    static QString findFirstLibraryFile(const QString& directoryPath);
    static bool toolPackageIsComplete(const QString& directoryPath,
                                      const QJsonObject& metaData,
                                      QString* errorMessage);
    static bool pluginPackageIsComplete(const QString& directoryPath,
                                        const QString& pluginName,
                                        QString* errorMessage);
    static bool confirmOverwriteIfNeeded(const QString& targetDirectoryPath,
                                         const QString& title,
                                         const QString& message,
                                         QWidget* parent,
                                         bool* outShouldContinue);
};

#endif // EXTERNALPACKAGEMANAGER_H