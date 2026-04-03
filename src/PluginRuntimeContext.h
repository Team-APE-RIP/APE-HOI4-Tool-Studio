//-------------------------------------------------------------------------------------
// PluginRuntimeContext.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef PLUGINRUNTIMECONTEXT_H
#define PLUGINRUNTIMECONTEXT_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <functional>

class PluginRuntimeContext {
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

    struct EffectiveFileEntry {
        QString logicalPath;
        EffectiveFileSource source = EffectiveFileSource::Unknown;
    };

    struct EffectiveFileListResult {
        bool success = false;
        QList<EffectiveFileEntry> entries;
        QString errorMessage;
    };

    using BinaryFileReader = std::function<FileReadResult(FileRoot, const QString&)>;
    using TextFileReader = std::function<TextReadResult(FileRoot, const QString&)>;
    using EffectiveBinaryFileReader = std::function<FileReadResult(const QString&)>;
    using EffectiveTextFileReader = std::function<TextReadResult(const QString&)>;
    using EffectiveFileEnumerator = std::function<EffectiveFileListResult()>;

    static PluginRuntimeContext& instance();

    void setBinaryFileReader(BinaryFileReader reader);
    FileReadResult readFile(FileRoot root, const QString& relativePath) const;

    void setTextFileReader(TextFileReader reader);
    TextReadResult readTextFile(FileRoot root, const QString& relativePath) const;

    void setEffectiveBinaryFileReader(EffectiveBinaryFileReader reader);
    FileReadResult readEffectiveFile(const QString& relativePath) const;

    void setEffectiveTextFileReader(EffectiveTextFileReader reader);
    TextReadResult readEffectiveTextFile(const QString& relativePath) const;

    void setEffectiveFileEnumerator(EffectiveFileEnumerator enumerator);
    EffectiveFileListResult listEffectiveFiles() const;

    static QString fileRootToString(FileRoot root);
    static FileRoot fileRootFromString(const QString& value);

private:
    PluginRuntimeContext() = default;

    BinaryFileReader m_binaryFileReader;
    TextFileReader m_textFileReader;
    EffectiveBinaryFileReader m_effectiveBinaryFileReader;
    EffectiveTextFileReader m_effectiveTextFileReader;
    EffectiveFileEnumerator m_effectiveFileEnumerator;
};

#endif // PLUGINRUNTIMECONTEXT_H