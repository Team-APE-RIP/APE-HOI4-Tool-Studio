//-------------------------------------------------------------------------------------
// ExternalPackageManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ExternalPackageManager.h"

#include "CustomMessageBox.h"
#include "LocalizationManager.h"
#include "PluginDescriptorParser.h"
#include "ToolDescriptorParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QStandardPaths>
#include <QWidget>

namespace {
QString normalizePath(const QString& path) {
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool hasImageFile(const QString& directoryPath) {
    const QDir dir(directoryPath);
    const QStringList imageFiles = dir.entryList(
        QStringList() << "cover.png" << "cover.jpg" << "cover.jpeg" << "cover.webp" << "cover.bmp",
        QDir::Files
    );
    return !imageFiles.isEmpty();
}
}

ExternalPackageManager::PendingRequest ExternalPackageManager::createPendingRequestFromPath(const QString& path) {
    PendingRequest request;
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return request;
    }

    const QString suffix = info.suffix().trimmed().toLower();
    if (suffix == "apehts") {
        request.type = RequestType::ToolDescriptor;
        request.descriptorPath = normalizePath(path);
    } else if (suffix == "htsplugin") {
        request.type = RequestType::PluginDescriptor;
        request.descriptorPath = normalizePath(path);
    }

    return request;
}

bool ExternalPackageManager::savePendingRestartRequest(const PendingRequest& request, QString* errorMessage) {
    LocalizationManager& loc = LocalizationManager::instance();

    if (!request.isValid()) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "SavePendingRestartRequestFailed");
        }
        return false;
    }

    const QString filePath = pendingRestartRequestFilePath();
    const QFileInfo fileInfo(filePath);
    if (!ensureDirectoryExists(fileInfo.dir().absolutePath(), errorMessage)) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "SavePendingRestartRequestFailed");
        }
        return false;
    }

    QJsonObject root;
    root.insert("type", requestTypeToString(request.type));
    root.insert("descriptorPath", request.descriptorPath);
    root.insert("overwriteApproved", request.overwriteApproved);

    const QJsonDocument document(root);
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "SavePendingRestartRequestFailed");
        }
        file.close();
        return false;
    }

    file.close();

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

ExternalPackageManager::PendingRequest ExternalPackageManager::takePendingRestartRequest(QString* errorMessage) {
    PendingRequest request;
    const QString filePath = pendingRestartRequestFilePath();

    QFile file(filePath);
    if (!file.exists()) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return request;
    }

    LocalizationManager& loc = LocalizationManager::instance();

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "LoadPendingRestartRequestFailed");
        }
        clearPendingRestartRequest();
        return request;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "LoadPendingRestartRequestFailed");
        }
        clearPendingRestartRequest();
        return request;
    }

    const QJsonObject root = document.object();
    request.type = requestTypeFromString(root.value("type").toString().trimmed());
    request.descriptorPath = normalizePath(root.value("descriptorPath").toString().trimmed());
    request.overwriteApproved = root.value("overwriteApproved").toBool(false);

    clearPendingRestartRequest();

    if (!request.isValid()) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "LoadPendingRestartRequestFailed");
        }
        return PendingRequest();
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return request;
}

void ExternalPackageManager::clearPendingRestartRequest() {
    const QString filePath = pendingRestartRequestFilePath();
    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }
}

ExternalPackageManager::ImportResult ExternalPackageManager::importToolPackage(const QString& descriptorPath,
                                                                               QWidget* parent,
                                                                               bool skipOverwritePrompt) {
    ImportResult result;
    LocalizationManager& loc = LocalizationManager::instance();

    const QString normalizedDescriptorPath = normalizePath(descriptorPath);
    const QFileInfo descriptorInfo(normalizedDescriptorPath);
    const QString sourceDirPath = descriptorInfo.dir().absolutePath();

    QJsonObject metaData;
    QString errorMessage;
    if (!ToolDescriptorParser::parseDescriptorFile(normalizedDescriptorPath, metaData, &errorMessage)) {
        result.errorMessage = loc.getString("ExternalPackage", "ToolDescriptorParseFailed");
        return result;
    }

    if (!toolPackageIsComplete(sourceDirPath, metaData, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    const QString toolName = metaData.value("id").toString().trimmed();
    if (toolName.isEmpty()) {
        result.errorMessage = loc.getString("ExternalPackage", "ToolDescriptorMissingName");
        return result;
    }

    const QString installedDescriptorPath = buildInstalledToolDescriptorPath(toolName);
    result.importedId = toolName;
    result.importedName = toolName;
    result.installedDescriptorPath = installedDescriptorPath;
    result.targetDirectoryPath = QFileInfo(installedDescriptorPath).dir().absolutePath();

    if (isSamePath(normalizedDescriptorPath, installedDescriptorPath)) {
        result.success = true;
        result.alreadyInstalled = true;
        result.useInstalledCopy = true;
        return result;
    }

    const QString targetRootPath = appToolsRootPath();
    if (!ensureDirectoryExists(targetRootPath, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    const QString targetDirectoryPath = QDir(targetRootPath).filePath(toolName);
    result.targetDirectoryPath = targetDirectoryPath;

    if (QDir(targetDirectoryPath).exists()) {
        if (!skipOverwritePrompt) {
            bool shouldContinue = true;
            if (!confirmOverwriteIfNeeded(targetDirectoryPath,
                                          loc.getString("ExternalPackage", "OverwriteToolTitle"),
                                          loc.getString("ExternalPackage", "OverwriteToolMessage").arg(toolName),
                                          parent,
                                          &shouldContinue)) {
                result.errorMessage = loc.getString("ExternalPackage", "ConfirmOverwriteFailed");
                return result;
            }

            if (!shouldContinue) {
                result.success = true;
                result.cancelled = true;
                result.useInstalledCopy = true;
                return result;
            }

            result.requiresRestart = true;
            return result;
        }

        if (!removePathRecursively(targetDirectoryPath, &errorMessage)) {
            result.errorMessage = errorMessage;
            return result;
        }
    }

    if (!copyDirectoryRecursively(sourceDirPath, targetDirectoryPath, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    result.success = true;
    return result;
}

ExternalPackageManager::ImportResult ExternalPackageManager::importPluginPackage(const QString& descriptorPath,
                                                                                 QWidget* parent,
                                                                                 bool skipOverwritePrompt) {
    ImportResult result;
    LocalizationManager& loc = LocalizationManager::instance();

    const QString normalizedDescriptorPath = normalizePath(descriptorPath);
    const QFileInfo descriptorInfo(normalizedDescriptorPath);
    const QString sourceDirPath = descriptorInfo.dir().absolutePath();

    PluginInfo info;
    QString errorMessage;
    if (!PluginDescriptorParser::parseDescriptorFile(normalizedDescriptorPath, info, &errorMessage)) {
        result.errorMessage = loc.getString("ExternalPackage", "PluginDescriptorParseFailed");
        return result;
    }

    if (!pluginPackageIsComplete(sourceDirPath, info.name, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    const QString installedDescriptorPath = buildInstalledPluginDescriptorPath(info.name);
    result.importedId = info.id;
    result.importedName = info.name;
    result.installedDescriptorPath = installedDescriptorPath;
    result.targetDirectoryPath = QFileInfo(installedDescriptorPath).dir().absolutePath();

    if (isSamePath(normalizedDescriptorPath, installedDescriptorPath)) {
        result.success = true;
        result.alreadyInstalled = true;
        result.useInstalledCopy = true;
        return result;
    }

    const QString targetRootPath = appPluginsRootPath();
    if (!ensureDirectoryExists(targetRootPath, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    const QString targetDirectoryPath = QDir(targetRootPath).filePath(info.name);
    result.targetDirectoryPath = targetDirectoryPath;

    if (QDir(targetDirectoryPath).exists()) {
        if (!skipOverwritePrompt) {
            bool shouldContinue = true;
            if (!confirmOverwriteIfNeeded(targetDirectoryPath,
                                          loc.getString("ExternalPackage", "OverwritePluginTitle"),
                                          loc.getString("ExternalPackage", "OverwritePluginMessage").arg(info.name),
                                          parent,
                                          &shouldContinue)) {
                result.errorMessage = loc.getString("ExternalPackage", "ConfirmOverwriteFailed");
                return result;
            }

            if (!shouldContinue) {
                result.cancelled = true;
                return result;
            }

            result.requiresRestart = true;
            return result;
        }

        if (!removePathRecursively(targetDirectoryPath, &errorMessage)) {
            result.errorMessage = errorMessage;
            return result;
        }
    }

    if (!copyDirectoryRecursively(sourceDirPath, targetDirectoryPath, &errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    result.success = true;
    return result;
}

QString ExternalPackageManager::pendingRestartRequestFilePath() {
    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return QDir(tempRoot + "/APE-HOI4-Tool-Studio").filePath("pending_external_request.json");
}

QString ExternalPackageManager::requestTypeToString(RequestType type) {
    switch (type) {
    case RequestType::ToolDescriptor:
        return "tool";
    case RequestType::PluginDescriptor:
        return "plugin";
    case RequestType::None:
    default:
        return "none";
    }
}

ExternalPackageManager::RequestType ExternalPackageManager::requestTypeFromString(const QString& value) {
    if (value == "tool") {
        return RequestType::ToolDescriptor;
    }

    if (value == "plugin") {
        return RequestType::PluginDescriptor;
    }

    return RequestType::None;
}

QString ExternalPackageManager::appToolsRootPath() {
    return findAppManagedRoot("tools");
}

QString ExternalPackageManager::appPluginsRootPath() {
    return findAppManagedRoot("plugins");
}

QString ExternalPackageManager::buildInstalledToolDescriptorPath(const QString& toolName) {
    return QDir(appToolsRootPath()).filePath(toolName + "/descriptor.apehts");
}

QString ExternalPackageManager::buildInstalledPluginDescriptorPath(const QString& pluginName) {
    return QDir(appPluginsRootPath()).filePath(pluginName + "/descriptor.htsplugin");
}

bool ExternalPackageManager::isSamePath(const QString& left, const QString& right) {
    return normalizePath(left).compare(normalizePath(right), Qt::CaseInsensitive) == 0;
}

QString ExternalPackageManager::findAppManagedRoot(const QString& childDirectoryName) {
    QDir appDir(QCoreApplication::applicationDirPath());
    if (appDir.exists(childDirectoryName)) {
        return appDir.filePath(childDirectoryName);
    }

    QDir parentDir = appDir;
    if (parentDir.cdUp() && parentDir.exists(childDirectoryName)) {
        return parentDir.filePath(childDirectoryName);
    }

    return appDir.filePath(childDirectoryName);
}

bool ExternalPackageManager::ensureDirectoryExists(const QString& path, QString* errorMessage) {
    QDir dir;
    if (dir.mkpath(path)) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = LocalizationManager::instance()
                            .getString("ExternalPackage", "CreateDirectoryFailed")
                            .arg(QDir::toNativeSeparators(path));
    }
    return false;
}

bool ExternalPackageManager::copyFileWithOverwrite(const QString& sourcePath,
                                                   const QString& targetPath,
                                                   QString* errorMessage) {
    QFileInfo targetInfo(targetPath);
    if (!ensureDirectoryExists(targetInfo.dir().absolutePath(), errorMessage)) {
        return false;
    }

    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        if (errorMessage) {
            *errorMessage = LocalizationManager::instance()
                                .getString("ExternalPackage", "RemoveExistingFileFailed")
                                .arg(QDir::toNativeSeparators(targetPath));
        }
        return false;
    }

    if (!QFile::copy(sourcePath, targetPath)) {
        if (errorMessage) {
            *errorMessage = LocalizationManager::instance()
                                .getString("ExternalPackage", "CopyFileFailed")
                                .arg(QDir::toNativeSeparators(sourcePath),
                                     QDir::toNativeSeparators(targetPath));
        }
        return false;
    }

    return true;
}

bool ExternalPackageManager::copyDirectoryRecursively(const QString& sourcePath,
                                                      const QString& targetPath,
                                                      QString* errorMessage) {
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isDir()) {
        if (errorMessage) {
            *errorMessage = LocalizationManager::instance()
                                .getString("ExternalPackage", "SourceDirectoryMissing")
                                .arg(QDir::toNativeSeparators(sourcePath));
        }
        return false;
    }

    if (!ensureDirectoryExists(targetPath, errorMessage)) {
        return false;
    }

    QDir sourceDir(sourcePath);
    const QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo& entry : entries) {
        const QString sourceEntryPath = entry.absoluteFilePath();
        const QString targetEntryPath = QDir(targetPath).filePath(entry.fileName());

        if (entry.isDir()) {
            if (!copyDirectoryRecursively(sourceEntryPath, targetEntryPath, errorMessage)) {
                return false;
            }
        } else {
            if (!copyFileWithOverwrite(sourceEntryPath, targetEntryPath, errorMessage)) {
                return false;
            }
        }
    }

    return true;
}

bool ExternalPackageManager::removePathRecursively(const QString& path, QString* errorMessage) {
    QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }

    if (info.isDir()) {
        QDir dir(path);
        if (dir.removeRecursively()) {
            return true;
        }
    } else if (QFile::remove(path)) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = LocalizationManager::instance()
                            .getString("ExternalPackage", "RemovePathFailed")
                            .arg(QDir::toNativeSeparators(path));
    }
    return false;
}

bool ExternalPackageManager::directoryContainsLibrary(const QString& directoryPath) {
    return !findFirstLibraryFile(directoryPath).isEmpty();
}

QString ExternalPackageManager::findFirstLibraryFile(const QString& directoryPath) {
    const QDir dir(directoryPath);
    const QStringList libraries = dir.entryList(QStringList() << "*.dll" << "*.so" << "*.dylib", QDir::Files);
    if (libraries.isEmpty()) {
        return QString();
    }
    return dir.filePath(libraries.first());
}

bool ExternalPackageManager::toolPackageIsComplete(const QString& directoryPath,
                                                   const QJsonObject& metaData,
                                                   QString* errorMessage) {
    LocalizationManager& loc = LocalizationManager::instance();
    const QString toolName = metaData.value("id").toString().trimmed();

    if (toolName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "ToolDescriptorMissingName");
        }
        return false;
    }

    const QString descriptorPath = QDir(directoryPath).filePath("descriptor.apehts");
    if (!QFile::exists(descriptorPath)) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "ToolDescriptorFileMissing");
        }
        return false;
    }

    if (!directoryContainsLibrary(directoryPath)) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "ToolRuntimeLibraryMissing").arg(toolName);
        }
        return false;
    }

    if (!hasImageFile(directoryPath)) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "ToolCoverMissing").arg(toolName);
        }
        return false;
    }

    const QDir localisationDir(QDir(directoryPath).filePath("localisation"));
    bool hasLocalisationFiles = false;
    if (localisationDir.exists()) {
        const QFileInfoList languageDirs = localisationDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& languageDir : languageDirs) {
            const QDir languagePath(languageDir.absoluteFilePath());
            if (languagePath.exists("strings.yml") || !languagePath.entryList(QStringList() << "*.yml", QDir::Files).isEmpty()) {
                hasLocalisationFiles = true;
                break;
            }
        }
    }

    if (!hasLocalisationFiles) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "ToolLocalizationMissing").arg(toolName);
        }
        return false;
    }

    return true;
}

bool ExternalPackageManager::pluginPackageIsComplete(const QString& directoryPath,
                                                     const QString& pluginName,
                                                     QString* errorMessage) {
    LocalizationManager& loc = LocalizationManager::instance();
    const QString descriptorPath = QDir(directoryPath).filePath("descriptor.htsplugin");

    if (!QFile::exists(descriptorPath)) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "PluginDescriptorFileMissing");
        }
        return false;
    }

    if (pluginName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "PluginDescriptorMissingName");
        }
        return false;
    }

    if (!directoryContainsLibrary(directoryPath)) {
        if (errorMessage) {
            *errorMessage = loc.getString("ExternalPackage", "PluginRuntimeLibraryMissing").arg(pluginName);
        }
        return false;
    }

    return true;
}

bool ExternalPackageManager::confirmOverwriteIfNeeded(const QString& targetDirectoryPath,
                                                      const QString& title,
                                                      const QString& message,
                                                      QWidget* parent,
                                                      bool* outShouldContinue) {
    if (outShouldContinue) {
        *outShouldContinue = true;
    }

    if (!QDir(targetDirectoryPath).exists()) {
        return true;
    }

    const QMessageBox::StandardButton button = CustomMessageBox::question(parent, title, message);
    if (outShouldContinue) {
        *outShouldContinue = (button == QMessageBox::Yes);
    }
    return true;
}