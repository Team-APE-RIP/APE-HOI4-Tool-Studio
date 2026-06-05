//-------------------------------------------------------------------------------------
// PluginAbiBroker.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "PluginAbiBroker.h"

#include "Logger.h"
#include "PluginManager.h"

#include <QReadLocker>
#include <QRegularExpression>
#include <QWriteLocker>
#include <QtAlgorithms>
#include <limits>

PluginAbiBroker& PluginAbiBroker::instance() {
    static PluginAbiBroker broker;
    return broker;
}

PluginAbiBroker::ContentType PluginAbiBroker::toContentType(quint32 value) {
    switch (value) {
    case APE_PLUGIN_ABI_CONTENT_JSON_UTF8:
        return ContentType::JsonUtf8;
    case APE_PLUGIN_ABI_CONTENT_BINARY:
        return ContentType::Binary;
    case APE_PLUGIN_ABI_CONTENT_BINARY_ENVELOPE:
        return ContentType::BinaryEnvelope;
    default:
        return ContentType::None;
    }
}

QString PluginAbiBroker::sanitizePluginError(const QString& message) {
    QString sanitized = message;
    sanitized.replace(QRegularExpression(QStringLiteral("[A-Za-z]:[\\\\/][^\\r\\n\\t ]+")), QStringLiteral("[path]"));
    sanitized.replace(QRegularExpression(QStringLiteral("\\bplugins[\\\\/][^\\r\\n\\t ]+")), QStringLiteral("plugins/[redacted]"));
    sanitized.replace(QRegularExpression(QStringLiteral("\\btools[\\\\/][^\\r\\n\\t ]+")), QStringLiteral("tools/[redacted]"));
    return sanitized.trimmed().isEmpty() ? QStringLiteral("Plugin operation failed.") : sanitized;
}

PluginAbiBroker::LoadedPlugin* PluginAbiBroker::loadPluginLocked(const QString& pluginName, QString* errorMessage) {
    if (m_plugins.contains(pluginName)) {
        return m_plugins.value(pluginName);
    }

    const PluginInfo info = PluginManager::instance().getPlugin(pluginName);
    if (!info.isValid() || info.libraryPath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Plugin is unavailable.");
        }
        return nullptr;
    }

    auto* loaded = new LoadedPlugin;
    loaded->name = pluginName;
    loaded->library.setFileName(info.libraryPath);
    if (!loaded->library.load()) {
        const QString loadError = sanitizePluginError(loaded->library.errorString());
        delete loaded;
        if (errorMessage) {
            *errorMessage = loadError;
        }
        return nullptr;
    }

    loaded->invoke = reinterpret_cast<ApePluginInvokeFn>(loaded->library.resolve("APE_Plugin_Invoke"));
    loaded->freeResponse = reinterpret_cast<ApePluginFreeResponseFn>(loaded->library.resolve("APE_Plugin_FreeResponse"));
    loaded->getName = reinterpret_cast<ApePluginGetNameFn>(loaded->library.resolve("APE_Plugin_GetName"));
    loaded->getAbiVersion = reinterpret_cast<ApePluginGetAbiVersionFn>(loaded->library.resolve("APE_Plugin_GetAbiVersion"));

    if (!loaded->invoke || !loaded->freeResponse || !loaded->getName || !loaded->getAbiVersion) {
        delete loaded;
        if (errorMessage) {
            *errorMessage = QStringLiteral("Plugin ABI entry point is unavailable.");
        }
        return nullptr;
    }

    if (loaded->getAbiVersion() != APE_PLUGIN_ABI_VERSION) {
        delete loaded;
        if (errorMessage) {
            *errorMessage = QStringLiteral("Plugin ABI version is unsupported.");
        }
        return nullptr;
    }

    const char* reportedName = loaded->getName();
    if (!reportedName || pluginName != QString::fromUtf8(reportedName)) {
        delete loaded;
        if (errorMessage) {
            *errorMessage = QStringLiteral("Plugin ABI identity does not match descriptor.");
        }
        return nullptr;
    }

    m_plugins.insert(pluginName, loaded);
    return loaded;
}

PluginAbiBroker::Response PluginAbiBroker::invoke(const Request& request) {
    Response result;
    result.contentType = request.contentType;

    const QString pluginName = request.pluginName.trimmed();
    const QString operation = request.operation.trimmed();
    if (pluginName.isEmpty() || operation.isEmpty()) {
        result.status = APE_PLUGIN_ABI_STATUS_INVALID_ARGUMENT;
        result.errorMessage = QStringLiteral("Plugin name or operation is empty.");
        return result;
    }

    if (!request.authorizedDependencies.contains(pluginName, Qt::CaseSensitive)) {
        result.status = APE_PLUGIN_ABI_STATUS_UNAUTHORIZED;
        result.errorMessage = QStringLiteral("Plugin operation is not authorized.");
        return result;
    }

    LoadedPlugin* plugin = nullptr;
    {
        QReadLocker reader(&m_lock);
        plugin = m_plugins.value(pluginName, nullptr);
    }
    if (!plugin) {
        QWriteLocker writer(&m_lock);
        QString loadError;
        plugin = loadPluginLocked(pluginName, &loadError);
        if (!plugin) {
            result.status = APE_PLUGIN_ABI_STATUS_PLUGIN_UNAVAILABLE;
            result.errorMessage = sanitizePluginError(loadError);
            return result;
        }
    }

    const QByteArray pluginNameUtf8 = pluginName.toUtf8();
    const QByteArray operationUtf8 = operation.toUtf8();

    ApePluginAbiRequest abiRequest;
    abiRequest.abiVersion = APE_PLUGIN_ABI_VERSION;
    abiRequest.pluginNameUtf8 = pluginNameUtf8.constData();
    abiRequest.operationUtf8 = operationUtf8.constData();
    abiRequest.contentType = static_cast<quint32>(request.contentType);
    abiRequest.flags = request.flags;
    abiRequest.payload.data = reinterpret_cast<const uint8_t*>(request.payload.constData());
    abiRequest.payload.size = static_cast<uint64_t>(request.payload.size());

    ApePluginAbiResponse abiResponse;
    abiResponse.abiVersion = APE_PLUGIN_ABI_VERSION;
    abiResponse.status = APE_PLUGIN_ABI_STATUS_INTERNAL_ERROR;
    abiResponse.contentType = APE_PLUGIN_ABI_CONTENT_NONE;
    abiResponse.flags = 0;
    abiResponse.payload = nullptr;
    abiResponse.payloadSize = 0;
    abiResponse.errorUtf8 = nullptr;

    const int invokeResult = plugin->invoke(&abiRequest, &abiResponse);
    result.status = abiResponse.status;
    result.contentType = toContentType(abiResponse.contentType);
    result.flags = abiResponse.flags;
    if (abiResponse.payload && abiResponse.payloadSize > 0) {
        if (abiResponse.payloadSize <= static_cast<uint64_t>(std::numeric_limits<int>::max())) {
            result.payload = QByteArray(reinterpret_cast<const char*>(abiResponse.payload), static_cast<int>(abiResponse.payloadSize));
        } else {
            result.status = APE_PLUGIN_ABI_STATUS_BUFFER_TOO_LARGE;
            result.errorMessage = QStringLiteral("Plugin response is too large.");
        }
    }
    if (abiResponse.errorUtf8) {
        result.errorMessage = sanitizePluginError(QString::fromUtf8(abiResponse.errorUtf8));
    }
    result.success = invokeResult == 0 && abiResponse.status == APE_PLUGIN_ABI_STATUS_OK;
    if (!result.success && result.errorMessage.isEmpty()) {
        result.errorMessage = QStringLiteral("Plugin operation failed.");
    }

    plugin->freeResponse(&abiResponse);
    return result;
}

void PluginAbiBroker::clearCache() {
    QWriteLocker writer(&m_lock);
    qDeleteAll(m_plugins);
    m_plugins.clear();
}
