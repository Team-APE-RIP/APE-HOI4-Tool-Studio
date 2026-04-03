//-------------------------------------------------------------------------------------
// ToolRuntimeContext.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolRuntimeContext.h"

ToolRuntimeContext& ToolRuntimeContext::instance() {
    static ToolRuntimeContext instance;
    return instance;
}

void ToolRuntimeContext::setPluginBinaryPathResolver(PluginBinaryPathResolver resolver) {
    m_pluginBinaryPathResolver = std::move(resolver);
}

bool ToolRuntimeContext::requestAuthorizedPluginBinaryPath(const QString& pluginName, QString* outPath, QString* errorMessage) const {
    if (!m_pluginBinaryPathResolver) {
        if (errorMessage) {
            *errorMessage = "Plugin runtime context resolver is not available.";
        }
        return false;
    }

    return m_pluginBinaryPathResolver(pluginName, outPath, errorMessage);
}

void ToolRuntimeContext::setBinaryFileReader(BinaryFileReader reader) {
    m_binaryFileReader = std::move(reader);
}

ToolRuntimeContext::FileReadResult ToolRuntimeContext::readFile(FileRoot root, const QString& relativePath) const {
    if (!m_binaryFileReader) {
        return {false, QByteArray(), "Binary file reader is not available."};
    }

    return m_binaryFileReader(root, relativePath);
}

void ToolRuntimeContext::setTextFileReader(TextFileReader reader) {
    m_textFileReader = std::move(reader);
}

ToolRuntimeContext::TextReadResult ToolRuntimeContext::readTextFile(FileRoot root, const QString& relativePath) const {
    if (!m_textFileReader) {
        return {false, QString(), "Text file reader is not available."};
    }

    return m_textFileReader(root, relativePath);
}

void ToolRuntimeContext::setEffectiveBinaryFileReader(EffectiveBinaryFileReader reader) {
    m_effectiveBinaryFileReader = std::move(reader);
}

ToolRuntimeContext::FileReadResult ToolRuntimeContext::readEffectiveFile(const QString& relativePath) const {
    if (!m_effectiveBinaryFileReader) {
        return {false, QByteArray(), "Effective binary file reader is not available."};
    }

    return m_effectiveBinaryFileReader(relativePath);
}

void ToolRuntimeContext::setEffectiveTextFileReader(EffectiveTextFileReader reader) {
    m_effectiveTextFileReader = std::move(reader);
}

ToolRuntimeContext::TextReadResult ToolRuntimeContext::readEffectiveTextFile(const QString& relativePath) const {
    if (!m_effectiveTextFileReader) {
        return {false, QString(), "Effective text file reader is not available."};
    }

    return m_effectiveTextFileReader(relativePath);
}

void ToolRuntimeContext::setBinaryFileWriter(BinaryFileWriter writer) {
    m_binaryFileWriter = std::move(writer);
}

ToolRuntimeContext::FileWriteResult ToolRuntimeContext::writeFile(FileRoot root, const QString& relativePath, const QByteArray& content) const {
    if (!m_binaryFileWriter) {
        return {false, "Binary file writer is not available."};
    }

    return m_binaryFileWriter(root, relativePath, content);
}

void ToolRuntimeContext::setTextFileWriter(TextFileWriter writer) {
    m_textFileWriter = std::move(writer);
}

ToolRuntimeContext::FileWriteResult ToolRuntimeContext::writeTextFile(FileRoot root, const QString& relativePath, const QString& content) const {
    if (!m_textFileWriter) {
        return {false, "Text file writer is not available."};
    }

    return m_textFileWriter(root, relativePath, content);
}

void ToolRuntimeContext::setPathRemover(PathRemover remover) {
    m_pathRemover = std::move(remover);
}

ToolRuntimeContext::FileWriteResult ToolRuntimeContext::removePath(FileRoot root, const QString& relativePath) const {
    if (!m_pathRemover) {
        return {false, "Path remover is not available."};
    }

    return m_pathRemover(root, relativePath);
}

void ToolRuntimeContext::setDirectoryEnsurer(DirectoryEnsurer ensurer) {
    m_directoryEnsurer = std::move(ensurer);
}

ToolRuntimeContext::FileWriteResult ToolRuntimeContext::ensureDirectory(FileRoot root, const QString& relativePath) const {
    if (!m_directoryEnsurer) {
        return {false, "Directory ensurer is not available."};
    }

    return m_directoryEnsurer(root, relativePath);
}

void ToolRuntimeContext::setDirectoryLister(DirectoryLister lister) {
    m_directoryLister = std::move(lister);
}

ToolRuntimeContext::DirectoryListResult ToolRuntimeContext::listDirectory(FileRoot root, const QString& relativePath, bool recursive) const {
    if (!m_directoryLister) {
        return {false, {}, "Directory lister is not available."};
    }

    return m_directoryLister(root, relativePath, recursive);
}

QString ToolRuntimeContext::fileRootToString(FileRoot root) {
    switch (root) {
    case FileRoot::Game:
        return "Game";
    case FileRoot::Mod:
        return "Mod";
    case FileRoot::Doc:
        return "Doc";
    default:
        return "Unknown";
    }
}

ToolRuntimeContext::FileRoot ToolRuntimeContext::fileRootFromString(const QString& value) {
    if (value.compare("Game", Qt::CaseInsensitive) == 0) {
        return FileRoot::Game;
    }
    if (value.compare("Mod", Qt::CaseInsensitive) == 0) {
        return FileRoot::Mod;
    }
    if (value.compare("Doc", Qt::CaseInsensitive) == 0) {
        return FileRoot::Doc;
    }
    return FileRoot::Unknown;
}