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
#include <QList>
#include <functional>

class ToolRuntimeContext {
public:
    enum class FileRoot {
        Game,
        Mod,
        Doc,
        Unknown
    };

    enum class EffectiveFileSource {
        Game,
        Mod,
        Dlc,
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

    struct TextFileMatchEntry {
        QString relativePath;
        QString name;
        QString content;
    };

    struct EffectiveFileEntry {
        QString logicalPath;
        EffectiveFileSource source = EffectiveFileSource::Unknown;
        qint64 lastModifiedMs = 0;
    };

    struct MatchingTextFilesResult {
        bool success = false;
        QList<TextFileMatchEntry> entries;
        QString errorMessage;
    };

    struct EffectiveFileListResult {
        bool success = false;
        QList<EffectiveFileEntry> entries;
        QString errorMessage;
    };

    enum class PluginPayloadContentType {
        None = 0,
        JsonUtf8 = 1,
        Binary = 2,
        BinaryEnvelope = 3
    };

    struct PluginInvokeRequest {
        QString pluginName;
        QString operation;
        PluginPayloadContentType contentType = PluginPayloadContentType::None;
        QByteArray payload;
        quint32 flags = 0;
    };

    struct PluginInvokeResponse {
        bool success = false;
        PluginPayloadContentType contentType = PluginPayloadContentType::None;
        QByteArray payload;
        QString errorMessage;
        quint32 status = 0;
        quint32 flags = 0;
    };

    using PluginInvoker = std::function<PluginInvokeResponse(const PluginInvokeRequest&)>;
    using MatchingTextFileReader = std::function<MatchingTextFilesResult(FileRoot, const QString&, const QString&, bool)>;
    using BinaryFileReader = std::function<FileReadResult(FileRoot, const QString&)>;
    using TextFileReader = std::function<TextReadResult(FileRoot, const QString&)>;
    using EffectiveBinaryFileReader = std::function<FileReadResult(const QString&)>;
    using EffectiveTextFileReader = std::function<TextReadResult(const QString&)>;
    using EffectiveFileEnumerator = std::function<EffectiveFileListResult(const QString&, const QString&)>;
    using EffectiveTextFilesReader = std::function<MatchingTextFilesResult(const QString&, const QString&)>;
    using BinaryFileWriter = std::function<FileWriteResult(FileRoot, const QString&, const QByteArray&)>;
    using TextFileWriter = std::function<FileWriteResult(FileRoot, const QString&, const QString&)>;
    using PathRemover = std::function<FileWriteResult(FileRoot, const QString&)>;
    using DirectoryEnsurer = std::function<FileWriteResult(FileRoot, const QString&)>;
    using DirectoryLister = std::function<DirectoryListResult(FileRoot, const QString&, bool)>;

    static ToolRuntimeContext& instance();

    void setPluginInvoker(PluginInvoker invoker);
    PluginInvokeResponse invokePlugin(const PluginInvokeRequest& request) const;

    void setMatchingTextFileReader(MatchingTextFileReader reader);
    MatchingTextFilesResult readMatchingTextFiles(FileRoot root,
                                                  const QString& relativePath,
                                                  const QString& regexPattern,
                                                  bool recursive) const;

    void setBinaryFileReader(BinaryFileReader reader);
    FileReadResult readFile(FileRoot root, const QString& relativePath) const;

    void setTextFileReader(TextFileReader reader);
    TextReadResult readTextFile(FileRoot root, const QString& relativePath) const;

    void setEffectiveBinaryFileReader(EffectiveBinaryFileReader reader);
    FileReadResult readEffectiveFile(const QString& relativePath) const;

    void setEffectiveTextFileReader(EffectiveTextFileReader reader);
    TextReadResult readEffectiveTextFile(const QString& relativePath) const;

    void setEffectiveFileEnumerator(EffectiveFileEnumerator enumerator);
    EffectiveFileListResult listEffectiveFiles(const QString& relativeRoot = QString(),
                                                const QString& suffixFilter = QString()) const;

    void setEffectiveTextFilesReader(EffectiveTextFilesReader reader);
    MatchingTextFilesResult readEffectiveTextFiles(const QString& relativeRoot = QString(),
                                                   const QString& suffixFilter = QString()) const;

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
    static QString effectiveFileSourceToString(EffectiveFileSource source);
    static EffectiveFileSource effectiveFileSourceFromString(const QString& value);

private:
    ToolRuntimeContext() = default;

    PluginInvoker m_pluginInvoker;
    MatchingTextFileReader m_matchingTextFileReader;
    BinaryFileReader m_binaryFileReader;
    TextFileReader m_textFileReader;
    EffectiveBinaryFileReader m_effectiveBinaryFileReader;
    EffectiveTextFileReader m_effectiveTextFileReader;
    EffectiveFileEnumerator m_effectiveFileEnumerator;
    EffectiveTextFilesReader m_effectiveTextFilesReader;
    BinaryFileWriter m_binaryFileWriter;
    TextFileWriter m_textFileWriter;
    PathRemover m_pathRemover;
    DirectoryEnsurer m_directoryEnsurer;
    DirectoryLister m_directoryLister;
};

#endif // TOOLRUNTIMECONTEXT_H
