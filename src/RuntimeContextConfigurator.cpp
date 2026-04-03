//-------------------------------------------------------------------------------------
// RuntimeContextConfigurator.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "RuntimeContextConfigurator.h"

#include "ConfigManager.h"
#include "FileManager.h"
#include "PluginManager.h"
#include "PluginRuntimeContext.h"
#include "ToolRuntimeContext.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace {

QString cleanRelativePathForLogging(const QString& relativePath) {
    QString normalized = QDir::cleanPath(relativePath).replace('\\', '/');
    if (normalized == ".") {
        return QString();
    }
    return normalized;
}

bool resolveRuntimeRootPath(ToolRuntimeContext::FileRoot root, QString* outPath, QString* errorMessage) {
    if (!outPath) {
        return false;
    }

    ConfigManager& config = ConfigManager::instance();
    QString rootPath;

    switch (root) {
    case ToolRuntimeContext::FileRoot::Game:
        rootPath = config.getGamePath();
        break;
    case ToolRuntimeContext::FileRoot::Mod:
        rootPath = config.getModPath();
        break;
    case ToolRuntimeContext::FileRoot::Doc:
        rootPath = config.getDocPath();
        break;
    default:
        if (errorMessage) {
            *errorMessage = "Unknown file root.";
        }
        return false;
    }

    if (rootPath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("File root %1 is not configured.")
                .arg(ToolRuntimeContext::fileRootToString(root));
        }
        return false;
    }

    *outPath = QDir::cleanPath(rootPath);
    return true;
}

bool resolveAuthorizedAbsolutePath(ToolRuntimeContext::FileRoot root,
                                   const QString& relativePath,
                                   QString* outAbsolutePath,
                                   QString* outDisplayRelativePath,
                                   QString* errorMessage) {
    if (!outAbsolutePath) {
        return false;
    }

    QString rootPath;
    if (!resolveRuntimeRootPath(root, &rootPath, errorMessage)) {
        return false;
    }

    const QString cleanedRelativePath = cleanRelativePathForLogging(relativePath);
    const QString normalizedRelativePath = QDir::cleanPath(cleanedRelativePath);

    if (normalizedRelativePath.startsWith("../") || normalizedRelativePath == "..") {
        if (errorMessage) {
            *errorMessage = "Path traversal is not allowed.";
        }
        return false;
    }

    const QDir rootDir(rootPath);
    const QString absolutePath = QDir::cleanPath(
        normalizedRelativePath.isEmpty() ? rootDir.absolutePath() : rootDir.filePath(normalizedRelativePath)
    );
    const QString relativeToRoot = QDir::cleanPath(rootDir.relativeFilePath(absolutePath));

    if (relativeToRoot.startsWith("../") || relativeToRoot == "..") {
        if (errorMessage) {
            *errorMessage = "Resolved path is outside the authorized root.";
        }
        return false;
    }

    *outAbsolutePath = absolutePath;
    if (outDisplayRelativePath) {
        *outDisplayRelativePath = cleanRelativePathForLogging(relativeToRoot == "." ? QString() : relativeToRoot);
    }
    return true;
}

bool resolveAuthorizedAbsolutePath(PluginRuntimeContext::FileRoot root,
                                   const QString& relativePath,
                                   QString* outAbsolutePath,
                                   QString* outDisplayRelativePath,
                                   QString* errorMessage) {
    ToolRuntimeContext::FileRoot toolRoot = ToolRuntimeContext::FileRoot::Unknown;
    switch (root) {
    case PluginRuntimeContext::FileRoot::Game:
        toolRoot = ToolRuntimeContext::FileRoot::Game;
        break;
    case PluginRuntimeContext::FileRoot::Mod:
        toolRoot = ToolRuntimeContext::FileRoot::Mod;
        break;
    case PluginRuntimeContext::FileRoot::Doc:
        toolRoot = ToolRuntimeContext::FileRoot::Doc;
        break;
    default:
        if (errorMessage) {
            *errorMessage = "Unknown file root.";
        }
        return false;
    }

    return resolveAuthorizedAbsolutePath(toolRoot, relativePath, outAbsolutePath, outDisplayRelativePath, errorMessage);
}

bool resolveEffectiveAbsolutePath(const QString& relativePath,
                                  QString* outAbsolutePath,
                                  QString* outDisplayRelativePath,
                                  QString* errorMessage) {
    if (!outAbsolutePath) {
        return false;
    }

    const QString cleanedRelativePath = cleanRelativePathForLogging(relativePath);
    const QString normalizedRelativePath = QDir::cleanPath(cleanedRelativePath);

    if (normalizedRelativePath.isEmpty() || normalizedRelativePath == ".") {
        if (errorMessage) {
            *errorMessage = "Effective relative path is empty.";
        }
        return false;
    }

    if (normalizedRelativePath.startsWith("../") || normalizedRelativePath == "..") {
        if (errorMessage) {
            *errorMessage = "Path traversal is not allowed.";
        }
        return false;
    }

    const QMap<QString, FileDetails> effectiveFiles = FileManager::instance().getEffectiveFiles();
    const auto it = effectiveFiles.constFind(normalizedRelativePath);
    if (it == effectiveFiles.constEnd()) {
        if (errorMessage) {
            *errorMessage = QString("Effective file does not exist: %1").arg(normalizedRelativePath);
        }
        return false;
    }

    const QString absolutePath = QDir::cleanPath(it.value().absPath);
    if (absolutePath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("Effective file path is unavailable: %1").arg(normalizedRelativePath);
        }
        return false;
    }

    *outAbsolutePath = absolutePath;
    if (outDisplayRelativePath) {
        *outDisplayRelativePath = normalizedRelativePath;
    }
    return true;
}

PluginRuntimeContext::EffectiveFileSource mapPluginEffectiveFileSource(const QString& source) {
    if (source.compare("Game", Qt::CaseInsensitive) == 0) {
        return PluginRuntimeContext::EffectiveFileSource::Game;
    }
    if (source.compare("Mod", Qt::CaseInsensitive) == 0) {
        return PluginRuntimeContext::EffectiveFileSource::Mod;
    }
    if (source.compare("DLC", Qt::CaseInsensitive) == 0) {
        return PluginRuntimeContext::EffectiveFileSource::Dlc;
    }
    return PluginRuntimeContext::EffectiveFileSource::Unknown;
}

bool isToolWriteRootAllowed(ToolRuntimeContext::FileRoot root, QString* errorMessage) {
    if (root == ToolRuntimeContext::FileRoot::Mod) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = "Write operations are only allowed for the Mod root.";
    }
    return false;
}

} // namespace

void configureToolRuntimeContext() {
    ToolRuntimeContext& context = ToolRuntimeContext::instance();

    context.setPluginBinaryPathResolver([](const QString& pluginName, QString* outPath, QString* errorMessage) {
        return PluginManager::instance().getPluginBinaryPath(pluginName, outPath, errorMessage);
    });

    context.setBinaryFileReader([](ToolRuntimeContext::FileRoot root, const QString& relativePath) {
        QString absolutePath;
        QString displayRelativePath;
        QString errorMessage;
        if (!resolveAuthorizedAbsolutePath(root, relativePath, &absolutePath, &displayRelativePath, &errorMessage)) {
            return ToolRuntimeContext::FileReadResult{false, QByteArray(), errorMessage};
        }

        QFile file(absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return ToolRuntimeContext::FileReadResult{
                false,
                QByteArray(),
                QString("Failed to open file for reading: %1").arg(displayRelativePath)
            };
        }

        return ToolRuntimeContext::FileReadResult{true, file.readAll(), QString()};
    });

    context.setTextFileReader([](ToolRuntimeContext::FileRoot root, const QString& relativePath) {
        const ToolRuntimeContext::FileReadResult binaryResult =
            ToolRuntimeContext::instance().readFile(root, relativePath);

        if (!binaryResult.success) {
            return ToolRuntimeContext::TextReadResult{false, QString(), binaryResult.errorMessage};
        }

        return ToolRuntimeContext::TextReadResult{
            true,
            QString::fromUtf8(binaryResult.content),
            QString()
        };
    });

    context.setEffectiveBinaryFileReader([](const QString& relativePath) {
        QString absolutePath;
        QString displayRelativePath;
        QString errorMessage;
        if (!resolveEffectiveAbsolutePath(relativePath, &absolutePath, &displayRelativePath, &errorMessage)) {
            return ToolRuntimeContext::FileReadResult{false, QByteArray(), errorMessage};
        }

        QFile file(absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return ToolRuntimeContext::FileReadResult{
                false,
                QByteArray(),
                QString("Failed to open effective file for reading: %1").arg(displayRelativePath)
            };
        }

        return ToolRuntimeContext::FileReadResult{true, file.readAll(), QString()};
    });

    context.setEffectiveTextFileReader([](const QString& relativePath) {
        const ToolRuntimeContext::FileReadResult binaryResult =
            ToolRuntimeContext::instance().readEffectiveFile(relativePath);

        if (!binaryResult.success) {
            return ToolRuntimeContext::TextReadResult{false, QString(), binaryResult.errorMessage};
        }

        return ToolRuntimeContext::TextReadResult{
            true,
            QString::fromUtf8(binaryResult.content),
            QString()
        };
    });

    context.setBinaryFileWriter([](ToolRuntimeContext::FileRoot root, const QString& relativePath, const QByteArray& content) {
        QString errorMessage;
        if (!isToolWriteRootAllowed(root, &errorMessage)) {
            return ToolRuntimeContext::FileWriteResult{false, errorMessage};
        }

        QString absolutePath;
        QString displayRelativePath;
        if (!resolveAuthorizedAbsolutePath(root, relativePath, &absolutePath, &displayRelativePath, &errorMessage)) {
            return ToolRuntimeContext::FileWriteResult{false, errorMessage};
        }

        const QFileInfo fileInfo(absolutePath);
        QDir parentDir = fileInfo.dir();
        if (!parentDir.exists() && !parentDir.mkpath(".")) {
            return ToolRuntimeContext::FileWriteResult{
                false,
                QString("Failed to create parent directory for %1").arg(displayRelativePath)
            };
        }

        QFile file(absolutePath);
        if (!file.open(QIODevice::WriteOnly)) {
            return ToolRuntimeContext::FileWriteResult{
                false,
                QString("Failed to open file for writing: %1").arg(displayRelativePath)
            };
        }

        if (file.write(content) != content.size()) {
            return ToolRuntimeContext::FileWriteResult{
                false,
                QString("Failed to write complete file content: %1").arg(displayRelativePath)
            };
        }

        return ToolRuntimeContext::FileWriteResult{true, QString()};
    });

    context.setTextFileWriter([](ToolRuntimeContext::FileRoot root, const QString& relativePath, const QString& content) {
        return ToolRuntimeContext::instance().writeFile(root, relativePath, content.toUtf8());
    });

    context.setPathRemover([](ToolRuntimeContext::FileRoot root, const QString& relativePath) {
        QString errorMessage;
        if (!isToolWriteRootAllowed(root, &errorMessage)) {
            return ToolRuntimeContext::FileWriteResult{false, errorMessage};
        }

        QString absolutePath;
        QString displayRelativePath;
        if (!resolveAuthorizedAbsolutePath(root, relativePath, &absolutePath, &displayRelativePath, &errorMessage)) {
            return ToolRuntimeContext::FileWriteResult{false, errorMessage};
        }

        QFileInfo info(absolutePath);
        if (!info.exists()) {
            return ToolRuntimeContext::FileWriteResult{true, QString()};
        }

        if (info.isDir()) {
            QDir dir(absolutePath);
            if (!dir.removeRecursively()) {
                return ToolRuntimeContext::FileWriteResult{
                    false,
                    QString("Failed to remove directory: %1").arg(displayRelativePath)
                };
            }
            return ToolRuntimeContext::FileWriteResult{true, QString()};
        }

        if (!QFile::remove(absolutePath)) {
            return ToolRuntimeContext::FileWriteResult{
                false,
                QString("Failed to remove file: %1").arg(displayRelativePath)
            };
        }

        return ToolRuntimeContext::FileWriteResult{true, QString()};
    });

    context.setDirectoryEnsurer([](ToolRuntimeContext::FileRoot root, const QString& relativePath) {
        QString errorMessage;
        if (!isToolWriteRootAllowed(root, &errorMessage)) {
            return ToolRuntimeContext::FileWriteResult{false, errorMessage};
        }

        QString absolutePath;
        QString displayRelativePath;
        if (!resolveAuthorizedAbsolutePath(root, relativePath, &absolutePath, &displayRelativePath, &errorMessage)) {
            return ToolRuntimeContext::FileWriteResult{false, errorMessage};
        }

        QDir dir(absolutePath);
        if (dir.exists()) {
            return ToolRuntimeContext::FileWriteResult{true, QString()};
        }

        if (!QDir().mkpath(absolutePath)) {
            return ToolRuntimeContext::FileWriteResult{
                false,
                QString("Failed to create directory: %1").arg(displayRelativePath)
            };
        }

        return ToolRuntimeContext::FileWriteResult{true, QString()};
    });

    context.setDirectoryLister([](ToolRuntimeContext::FileRoot root, const QString& relativePath, bool recursive) {
        QString absolutePath;
        QString displayRelativePath;
        QString errorMessage;
        if (!resolveAuthorizedAbsolutePath(root, relativePath, &absolutePath, &displayRelativePath, &errorMessage)) {
            return ToolRuntimeContext::DirectoryListResult{false, {}, errorMessage};
        }

        const QFileInfo rootInfo(absolutePath);
        if (!rootInfo.exists()) {
            return ToolRuntimeContext::DirectoryListResult{
                false,
                {},
                QString("Directory does not exist: %1").arg(displayRelativePath)
            };
        }

        if (!rootInfo.isDir()) {
            return ToolRuntimeContext::DirectoryListResult{
                false,
                {},
                QString("Path is not a directory: %1").arg(displayRelativePath)
            };
        }

        QList<ToolRuntimeContext::DirectoryEntry> entries;
        if (recursive) {
            QDirIterator it(
                absolutePath,
                QDir::NoDotAndDotDot | QDir::AllEntries,
                QDirIterator::Subdirectories
            );

            while (it.hasNext()) {
                it.next();
                const QFileInfo info = it.fileInfo();
                ToolRuntimeContext::DirectoryEntry entry;
                entry.relativePath = cleanRelativePathForLogging(QDir(absolutePath).relativeFilePath(info.absoluteFilePath()));
                entry.name = info.fileName();
                entry.isDirectory = info.isDir();
                entry.size = entry.isDirectory ? -1 : info.size();
                entry.lastModifiedUtc = info.lastModified().toUTC();
                entries.append(entry);
            }
        } else {
            const QFileInfoList infoList = QDir(absolutePath).entryInfoList(
                QDir::NoDotAndDotDot | QDir::AllEntries,
                QDir::Name | QDir::DirsFirst
            );

            for (const QFileInfo& info : infoList) {
                ToolRuntimeContext::DirectoryEntry entry;
                entry.relativePath = cleanRelativePathForLogging(
                    displayRelativePath.isEmpty() ? info.fileName() : displayRelativePath + "/" + info.fileName()
                );
                entry.name = info.fileName();
                entry.isDirectory = info.isDir();
                entry.size = entry.isDirectory ? -1 : info.size();
                entry.lastModifiedUtc = info.lastModified().toUTC();
                entries.append(entry);
            }
        }

        return ToolRuntimeContext::DirectoryListResult{true, entries, QString()};
    });
}

void configurePluginRuntimeContext() {
    PluginRuntimeContext& context = PluginRuntimeContext::instance();

    context.setBinaryFileReader([](PluginRuntimeContext::FileRoot root, const QString& relativePath) {
        QString absolutePath;
        QString displayRelativePath;
        QString errorMessage;
        if (!resolveAuthorizedAbsolutePath(root, relativePath, &absolutePath, &displayRelativePath, &errorMessage)) {
            return PluginRuntimeContext::FileReadResult{false, QByteArray(), errorMessage};
        }

        QFile file(absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return PluginRuntimeContext::FileReadResult{
                false,
                QByteArray(),
                QString("Failed to open file for reading: %1").arg(displayRelativePath)
            };
        }

        return PluginRuntimeContext::FileReadResult{true, file.readAll(), QString()};
    });

    context.setTextFileReader([](PluginRuntimeContext::FileRoot root, const QString& relativePath) {
        const PluginRuntimeContext::FileReadResult binaryResult =
            PluginRuntimeContext::instance().readFile(root, relativePath);

        if (!binaryResult.success) {
            return PluginRuntimeContext::TextReadResult{false, QString(), binaryResult.errorMessage};
        }

        return PluginRuntimeContext::TextReadResult{
            true,
            QString::fromUtf8(binaryResult.content),
            QString()
        };
    });

    context.setEffectiveBinaryFileReader([](const QString& relativePath) {
        QString absolutePath;
        QString displayRelativePath;
        QString errorMessage;
        if (!resolveEffectiveAbsolutePath(relativePath, &absolutePath, &displayRelativePath, &errorMessage)) {
            return PluginRuntimeContext::FileReadResult{false, QByteArray(), errorMessage};
        }

        QFile file(absolutePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return PluginRuntimeContext::FileReadResult{
                false,
                QByteArray(),
                QString("Failed to open effective file for reading: %1").arg(displayRelativePath)
            };
        }

        return PluginRuntimeContext::FileReadResult{true, file.readAll(), QString()};
    });

    context.setEffectiveTextFileReader([](const QString& relativePath) {
        const PluginRuntimeContext::FileReadResult binaryResult =
            PluginRuntimeContext::instance().readEffectiveFile(relativePath);

        if (!binaryResult.success) {
            return PluginRuntimeContext::TextReadResult{false, QString(), binaryResult.errorMessage};
        }

        return PluginRuntimeContext::TextReadResult{
            true,
            QString::fromUtf8(binaryResult.content),
            QString()
        };
    });

    context.setEffectiveFileEnumerator([]() {
        const QMap<QString, FileDetails> effectiveFiles = FileManager::instance().getEffectiveFiles();

        PluginRuntimeContext::EffectiveFileListResult result;
        result.success = true;
        result.entries.reserve(effectiveFiles.size());

        for (auto it = effectiveFiles.constBegin(); it != effectiveFiles.constEnd(); ++it) {
            PluginRuntimeContext::EffectiveFileEntry entry;
            entry.logicalPath = it.key();
            entry.source = mapPluginEffectiveFileSource(it.value().source);
            result.entries.append(entry);
        }

        return result;
    });
}