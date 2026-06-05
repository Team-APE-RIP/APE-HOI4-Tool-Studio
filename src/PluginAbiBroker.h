//-------------------------------------------------------------------------------------
// PluginAbiBroker.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef PLUGINABIBROKER_H
#define PLUGINABIBROKER_H

#include "PluginAbi.h"

#include <QByteArray>
#include <QHash>
#include <QLibrary>
#include <QMutex>
#include <QReadWriteLock>
#include <QString>
#include <QStringList>

class PluginAbiBroker {
public:
    enum class ContentType : quint32 {
        None = APE_PLUGIN_ABI_CONTENT_NONE,
        JsonUtf8 = APE_PLUGIN_ABI_CONTENT_JSON_UTF8,
        Binary = APE_PLUGIN_ABI_CONTENT_BINARY,
        BinaryEnvelope = APE_PLUGIN_ABI_CONTENT_BINARY_ENVELOPE
    };

    struct Request {
        QString pluginName;
        QString operation;
        ContentType contentType = ContentType::None;
        QByteArray payload;
        quint32 flags = 0;
        QStringList authorizedDependencies;
    };

    struct Response {
        bool success = false;
        quint32 status = APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR;
        ContentType contentType = ContentType::None;
        QByteArray payload;
        QString errorMessage;
        quint32 flags = 0;
    };

    static PluginAbiBroker& instance();

    Response invoke(const Request& request);
    void clearCache();

private:
    struct LoadedPlugin {
        QString name;
        QLibrary library;
        ApePluginInvokeFn invoke = nullptr;
        ApePluginFreeResponseFn freeResponse = nullptr;
        ApePluginGetNameFn getName = nullptr;
        ApePluginGetAbiVersionFn getAbiVersion = nullptr;
        QMutex callMutex;
    };

    PluginAbiBroker() = default;

    LoadedPlugin* loadPluginLocked(const QString& pluginName, QString* errorMessage);
    static QString sanitizePluginError(const QString& message);
    static ContentType toContentType(quint32 value);

    QReadWriteLock m_lock;
    QHash<QString, LoadedPlugin*> m_plugins;
};

#endif // PLUGINABIBROKER_H
