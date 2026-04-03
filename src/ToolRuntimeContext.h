//-------------------------------------------------------------------------------------
// ToolRuntimeContext.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLRUNTIMECONTEXT_H
#define TOOLRUNTIMECONTEXT_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QDateTime>
#include <functional>

class ToolRuntimeContext {
public:
    enum class FileRoot {
        Game,
        Mod,
        Doc,
        Unknown
    };

    struct FileReadResult {
        bool success = false;
        QByteArray content;
        QString errorMessage;
    };

    struct TextReadResult {
        bool success = false;
        QString content;
        QString errorMessage;
    };

    struct FileWriteResult {
        bool success = false;
        QString errorMessage;
    };

    struct DirectoryEntry {
        QString relativePath;
        QString name;
        bool isDirectory = false;
        qint64 size = -1;
        QDateTime lastModifiedUtc;
    };

    struct DirectoryListResult {
        bool success = false;
        QList<DirectoryEntry> entries;
        QString errorMessage;
    };

    using PluginBinaryPathResolver = std::function<bool(const QString&, QString*, QString*)>;
    using BinaryFileReader = std::function<FileReadResult(FileRoot, const QString&)>;
    using TextFileReader = std::function<TextReadResult(FileRoot, const QString&)>;
    using EffectiveBinaryFileReader = std::function<FileReadResult(const QString&)>;
    using EffectiveTextFileReader = std::function<TextReadResult(const QString&)>;
    using BinaryFileWriter = std::function<FileWriteResult(FileRoot, const QString&, const QByteArray&)>;
    using TextFileWriter = std::function<FileWriteResult(FileRoot, const QString&, const QString&)>;
    using PathRemover = std::function<FileWriteResult(FileRoot, const QString&)>;
    using DirectoryEnsurer = std::function<FileWriteResult(FileRoot, const QString&)>;
    using DirectoryLister = std::function<DirectoryListResult(FileRoot, const QString&, bool)>;

    static ToolRuntimeContext& instance();

    void setPluginBinaryPathResolver(PluginBinaryPathResolver resolver);
    bool requestAuthorizedPluginBinaryPath(const QString& pluginName, QString* outPath, QString* errorMessage = nullptr) const;

    void setBinaryFileReader(BinaryFileReader reader);
    FileReadResult readFile(FileRoot root, const QString& relativePath) const;

    void setTextFileReader(TextFileReader reader);
    TextReadResult readTextFile(FileRoot root, const QString& relativePath) const;

    void setEffectiveBinaryFileReader(EffectiveBinaryFileReader reader);
    FileReadResult readEffectiveFile(const QString& relativePath) const;

    void setEffectiveTextFileReader(EffectiveTextFileReader reader);
    TextReadResult readEffectiveTextFile(const QString& relativePath) const;

    void setBinaryFileWriter(BinaryFileWriter writer);
    FileWriteResult writeFile(FileRoot root, const QString& relativePath, const QByteArray& content) const;

    void setTextFileWriter(TextFileWriter writer);
    FileWriteResult writeTextFile(FileRoot root, const QString& relativePath, const QString& content) const;

    void setPathRemover(PathRemover remover);
    FileWriteResult removePath(FileRoot root, const QString& relativePath) const;

    void setDirectoryEnsurer(DirectoryEnsurer ensurer);
    FileWriteResult ensureDirectory(FileRoot root, const QString& relativePath) const;

    void setDirectoryLister(DirectoryLister lister);
    DirectoryListResult listDirectory(FileRoot root, const QString& relativePath, bool recursive = false) const;

    static QString fileRootToString(FileRoot root);
    static FileRoot fileRootFromString(const QString& value);

private:
    ToolRuntimeContext() = default;

    PluginBinaryPathResolver m_pluginBinaryPathResolver;
    BinaryFileReader m_binaryFileReader;
    TextFileReader m_textFileReader;
    EffectiveBinaryFileReader m_effectiveBinaryFileReader;
    EffectiveTextFileReader m_effectiveTextFileReader;
    BinaryFileWriter m_binaryFileWriter;
    TextFileWriter m_textFileWriter;
    PathRemover m_pathRemover;
    DirectoryEnsurer m_directoryEnsurer;
    DirectoryLister m_directoryLister;
};

#endif // TOOLRUNTIMECONTEXT_H