//-------------------------------------------------------------------------------------
// PluginRuntimeContext.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "PluginRuntimeContext.h"

PluginRuntimeContext& PluginRuntimeContext::instance() {
    static PluginRuntimeContext instance;
    return instance;
}

void PluginRuntimeContext::setBinaryFileReader(BinaryFileReader reader) {
    m_binaryFileReader = std::move(reader);
}

PluginRuntimeContext::FileReadResult PluginRuntimeContext::readFile(FileRoot root, const QString& relativePath) const {
    if (!m_binaryFileReader) {
        return {false, QByteArray(), "Binary file reader is not available."};
    }

    return m_binaryFileReader(root, relativePath);
}

void PluginRuntimeContext::setTextFileReader(TextFileReader reader) {
    m_textFileReader = std::move(reader);
}

PluginRuntimeContext::TextReadResult PluginRuntimeContext::readTextFile(FileRoot root, const QString& relativePath) const {
    if (!m_textFileReader) {
        return {false, QString(), "Text file reader is not available."};
    }

    return m_textFileReader(root, relativePath);
}

void PluginRuntimeContext::setEffectiveBinaryFileReader(EffectiveBinaryFileReader reader) {
    m_effectiveBinaryFileReader = std::move(reader);
}

PluginRuntimeContext::FileReadResult PluginRuntimeContext::readEffectiveFile(const QString& relativePath) const {
    if (!m_effectiveBinaryFileReader) {
        return {false, QByteArray(), "Effective binary file reader is not available."};
    }

    return m_effectiveBinaryFileReader(relativePath);
}

void PluginRuntimeContext::setEffectiveTextFileReader(EffectiveTextFileReader reader) {
    m_effectiveTextFileReader = std::move(reader);
}

PluginRuntimeContext::TextReadResult PluginRuntimeContext::readEffectiveTextFile(const QString& relativePath) const {
    if (!m_effectiveTextFileReader) {
        return {false, QString(), "Effective text file reader is not available."};
    }

    return m_effectiveTextFileReader(relativePath);
}

void PluginRuntimeContext::setEffectiveFileEnumerator(EffectiveFileEnumerator enumerator) {
    m_effectiveFileEnumerator = std::move(enumerator);
}

PluginRuntimeContext::EffectiveFileListResult PluginRuntimeContext::listEffectiveFiles() const {
    if (!m_effectiveFileEnumerator) {
        return {false, {}, "Effective file enumerator is not available."};
    }

    return m_effectiveFileEnumerator();
}

QString PluginRuntimeContext::fileRootToString(FileRoot root) {
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

PluginRuntimeContext::FileRoot PluginRuntimeContext::fileRootFromString(const QString& value) {
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