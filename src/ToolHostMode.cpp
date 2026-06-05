//-------------------------------------------------------------------------------------
// ToolHostMode.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolHostMode.h"
#include "ToolIpcProtocol.h"
#include "FileManager.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include "ToolDescriptorParser.h"
#include "PluginRuntimeContext.h"
#include "ToolRuntimeContext.h"
#include "ToolWorkerInterface.h"

#include <QApplication>
#include <QLibrary>
#include <QLocalSocket>
#include <QDir>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QList>
#include <QEventLoop>
#include <QThread>
#include <QElapsedTimer>
#include <QDateTime>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
using WorkerCreateFn = ToolWorkerHandle (*)(const char*);
using WorkerDestroyFn = void (*)(ToolWorkerHandle);
using WorkerInitializeFn = ToolWorkerResult (*)(ToolWorkerHandle, const char*);
using WorkerHandleActionFn = const char* (*)(ToolWorkerHandle, const char*, const char*, const char*, ToolWorkerResult*);
using WorkerGetStateFn = const char* (*)(ToolWorkerHandle, ToolWorkerResult*);
using WorkerGetLastErrorFn = const char* (*)(ToolWorkerHandle);
using WorkerFreeStringFn = void (*)(const char*);
using WorkerGetVersionFn = const char* (*)();

QJsonObject jsonObjectFromUtf8(const char* jsonText) {
    if (!jsonText || *jsonText == '\0') {
        return QJsonObject();
    }

    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(jsonText));
    return document.isObject() ? document.object() : QJsonObject();
}

QJsonObject stringMapToJsonObject(const QMap<QString, QString>& strings) {
    QVariantMap variantMap;
    for (auto iterator = strings.constBegin(); iterator != strings.constEnd(); ++iterator) {
        variantMap.insert(iterator.key(), iterator.value());
    }
    return QJsonObject::fromVariantMap(variantMap);
}

ToolRuntimeContext::FileRoot toToolFileRoot(PluginRuntimeContext::FileRoot root) {
    switch (root) {
    case PluginRuntimeContext::FileRoot::Game:
        return ToolRuntimeContext::FileRoot::Game;
    case PluginRuntimeContext::FileRoot::Mod:
        return ToolRuntimeContext::FileRoot::Mod;
    case PluginRuntimeContext::FileRoot::Doc:
        return ToolRuntimeContext::FileRoot::Doc;
    default:
        return ToolRuntimeContext::FileRoot::Unknown;
    }
}

PluginRuntimeContext::EffectiveFileSource toPluginEffectiveFileSource(ToolRuntimeContext::EffectiveFileSource source) {
    switch (source) {
    case ToolRuntimeContext::EffectiveFileSource::Game:
        return PluginRuntimeContext::EffectiveFileSource::Game;
    case ToolRuntimeContext::EffectiveFileSource::Mod:
        return PluginRuntimeContext::EffectiveFileSource::Mod;
    case ToolRuntimeContext::EffectiveFileSource::Dlc:
        return PluginRuntimeContext::EffectiveFileSource::Dlc;
    default:
        return PluginRuntimeContext::EffectiveFileSource::Unknown;
    }
}
} // namespace

class ToolHostApp : public QObject {
    Q_OBJECT
public:
    ToolHostApp(const QString& serverName, const QString& toolPath, QObject* parent = nullptr)
        : QObject(parent)
        , m_serverName(serverName)
        , m_toolPath(toolPath)
        , m_requestId(0)
        , m_dataReady(false)
        , m_connectRetryCount(0)
        , m_connectedOnce(false)
        , m_retryScheduled(false)
        , m_shutdownRequested(false)
    {
        m_socket = new QLocalSocket(this);
        connect(m_socket, &QLocalSocket::connected, this, &ToolHostApp::onConnected);
        connect(m_socket, &QLocalSocket::disconnected, this, &ToolHostApp::onDisconnected);
        connect(m_socket, &QLocalSocket::readyRead, this, &ToolHostApp::onReadyRead);
        connect(m_socket, &QLocalSocket::errorOccurred, this, &ToolHostApp::onError);
        
        m_heartbeatTimer = new QTimer(this);
        connect(m_heartbeatTimer, &QTimer::timeout, this, &ToolHostApp::sendHeartbeat);
    }
    
    bool loadTool() {
        QFileInfo fi(m_toolPath);
        const QString metadataPath = fi.absolutePath() + "/descriptor.apehts";
        QJsonObject metaData;
        QString errorMessage;
        if (!ToolDescriptorParser::parseDescriptorFile(metadataPath, metaData, &errorMessage)) {
            qCritical() << errorMessage;
            return false;
        }

        m_toolInfo.id = metaData.value("id").toString();
        m_toolInfo.name = metaData.value("name").toString();
        m_toolInfo.version = metaData.value("version").toString();
        m_toolInfo.compatibleVersion = metaData.value("compatibleVersion").toString();
        m_toolInfo.author = metaData.value("author").toString();
        m_toolInfo.dependencies = ToolDescriptorParser::extractDependencies(metaData);

        ToolRuntimeContext::instance().setPluginInvoker(
            [this](const ToolRuntimeContext::PluginInvokeRequest& request) {
                return requestInvokePlugin(request);
            }
        );
        ToolRuntimeContext::instance().setMatchingTextFileReader(
            [this](ToolRuntimeContext::FileRoot root,
                   const QString& relativePath,
                   const QString& regexPattern,
                   bool recursive) {
                return requestMatchingTextFiles(root, relativePath, regexPattern, recursive);
            }
        );
        ToolRuntimeContext::instance().setBinaryFileReader(
            [this](ToolRuntimeContext::FileRoot root, const QString& relativePath) {
                return requestBinaryFile(root, relativePath);
            }
        );
        ToolRuntimeContext::instance().setTextFileReader(
            [this](ToolRuntimeContext::FileRoot root, const QString& relativePath) {
                return requestTextFile(root, relativePath);
            }
        );
        ToolRuntimeContext::instance().setEffectiveBinaryFileReader(
            [this](const QString& relativePath) {
                return requestEffectiveBinaryFile(relativePath);
            }
        );
        ToolRuntimeContext::instance().setEffectiveTextFileReader(
            [this](const QString& relativePath) {
                return requestEffectiveTextFile(relativePath);
            }
        );
        ToolRuntimeContext::instance().setEffectiveTextFilesReader(
            [this](const QString& relativeRoot, const QString& suffixFilter) {
                return requestEffectiveTextFiles(relativeRoot, suffixFilter);
            }
        );
        ToolRuntimeContext::instance().setEffectiveFileEnumerator(
            [this](const QString& relativeRoot, const QString& suffixFilter) {
                return requestListEffectiveFiles(relativeRoot, suffixFilter);
            }
        );
        ToolRuntimeContext::instance().setBinaryFileWriter(
            [this](ToolRuntimeContext::FileRoot root, const QString& relativePath, const QByteArray& content) {
                return requestWriteBinaryFile(root, relativePath, content);
            }
        );
        ToolRuntimeContext::instance().setTextFileWriter(
            [this](ToolRuntimeContext::FileRoot root, const QString& relativePath, const QString& content) {
                return requestWriteTextFile(root, relativePath, content);
            }
        );
        ToolRuntimeContext::instance().setPathRemover(
            [this](ToolRuntimeContext::FileRoot root, const QString& relativePath) {
                return requestRemovePath(root, relativePath);
            }
        );
        ToolRuntimeContext::instance().setDirectoryEnsurer(
            [this](ToolRuntimeContext::FileRoot root, const QString& relativePath) {
                return requestEnsureDirectory(root, relativePath);
            }
        );
        ToolRuntimeContext::instance().setDirectoryLister(
            [this](ToolRuntimeContext::FileRoot root, const QString& relativePath, bool recursive) {
                return requestListDirectory(root, relativePath, recursive);
            }
        );
        PluginRuntimeContext::instance().setBinaryFileReader(
            [this](PluginRuntimeContext::FileRoot root, const QString& relativePath) {
                const ToolRuntimeContext::FileReadResult runtimeResult =
                    requestBinaryFile(toToolFileRoot(root), relativePath);
                return PluginRuntimeContext::FileReadResult{
                    runtimeResult.success,
                    runtimeResult.content,
                    runtimeResult.errorMessage
                };
            }
        );
        PluginRuntimeContext::instance().setTextFileReader(
            [this](PluginRuntimeContext::FileRoot root, const QString& relativePath) {
                const ToolRuntimeContext::TextReadResult runtimeResult =
                    requestTextFile(toToolFileRoot(root), relativePath);
                return PluginRuntimeContext::TextReadResult{
                    runtimeResult.success,
                    runtimeResult.content,
                    runtimeResult.errorMessage
                };
            }
        );
        PluginRuntimeContext::instance().setEffectiveBinaryFileReader(
            [this](const QString& relativePath) {
                const ToolRuntimeContext::FileReadResult runtimeResult = requestEffectiveBinaryFile(relativePath);
                return PluginRuntimeContext::FileReadResult{
                    runtimeResult.success,
                    runtimeResult.content,
                    runtimeResult.errorMessage
                };
            }
        );
        PluginRuntimeContext::instance().setEffectiveTextFileReader(
            [this](const QString& relativePath) {
                const ToolRuntimeContext::TextReadResult runtimeResult = requestEffectiveTextFile(relativePath);
                return PluginRuntimeContext::TextReadResult{
                    runtimeResult.success,
                    runtimeResult.content,
                    runtimeResult.errorMessage
                };
            }
        );
        PluginRuntimeContext::instance().setEffectiveTextFilesReader(
            [this](const QString& relativeRoot, const QString& suffixFilter) {
                const ToolRuntimeContext::MatchingTextFilesResult runtimeResult =
                    requestEffectiveTextFiles(relativeRoot, suffixFilter);
                PluginRuntimeContext::MatchingTextFilesResult result;
                result.success = runtimeResult.success;
                result.errorMessage = runtimeResult.errorMessage;
                result.entries.reserve(runtimeResult.entries.size());
                for (const ToolRuntimeContext::TextFileMatchEntry& runtimeEntry : runtimeResult.entries) {
                    PluginRuntimeContext::TextFileMatchEntry entry;
                    entry.relativePath = runtimeEntry.relativePath;
                    entry.name = runtimeEntry.name;
                    entry.content = runtimeEntry.content;
                    result.entries.append(std::move(entry));
                }
                return result;
            }
        );
        PluginRuntimeContext::instance().setEffectiveFileEnumerator(
            [this](const QString& relativeRoot, const QString& suffixFilter) {
                const ToolRuntimeContext::EffectiveFileListResult runtimeResult =
                    requestListEffectiveFiles(relativeRoot, suffixFilter);
                PluginRuntimeContext::EffectiveFileListResult result;
                result.success = runtimeResult.success;
                result.errorMessage = runtimeResult.errorMessage;
                result.entries.reserve(runtimeResult.entries.size());
                for (const ToolRuntimeContext::EffectiveFileEntry& runtimeEntry : runtimeResult.entries) {
                    PluginRuntimeContext::EffectiveFileEntry entry;
                    entry.logicalPath = runtimeEntry.logicalPath;
                    entry.source = toPluginEffectiveFileSource(runtimeEntry.source);
                    entry.lastModifiedMs = runtimeEntry.lastModifiedMs;
                    result.entries.append(entry);
                }
                return result;
            }
        );

        m_workerLibrary.setFileName(m_toolPath);
        if (!m_workerLibrary.load()) {
            qCritical() << "Failed to load worker library:" << m_workerLibrary.errorString();
            return false;
        }

        m_workerCreate = reinterpret_cast<WorkerCreateFn>(m_workerLibrary.resolve("ToolWorker_Create"));
        m_workerDestroy = reinterpret_cast<WorkerDestroyFn>(m_workerLibrary.resolve("ToolWorker_Destroy"));
        m_workerInitialize = reinterpret_cast<WorkerInitializeFn>(m_workerLibrary.resolve("ToolWorker_Initialize"));
        m_workerHandleAction = reinterpret_cast<WorkerHandleActionFn>(m_workerLibrary.resolve("ToolWorker_HandleAction"));
        m_workerGetCurrentState = reinterpret_cast<WorkerGetStateFn>(m_workerLibrary.resolve("ToolWorker_GetCurrentState"));
        m_workerGetInitialState = reinterpret_cast<WorkerGetStateFn>(m_workerLibrary.resolve("ToolWorker_GetInitialState"));
        m_workerGetLastError = reinterpret_cast<WorkerGetLastErrorFn>(m_workerLibrary.resolve("ToolWorker_GetLastError"));
        m_workerFreeString = reinterpret_cast<WorkerFreeStringFn>(m_workerLibrary.resolve("ToolWorker_FreeString"));
        m_workerGetVersion = reinterpret_cast<WorkerGetVersionFn>(m_workerLibrary.resolve("ToolWorker_GetVersion"));

        if (!m_workerCreate || !m_workerDestroy || !m_workerInitialize || !m_workerHandleAction
            || !m_workerGetCurrentState || !m_workerGetInitialState || !m_workerFreeString) {
            qCritical() << "Worker library is missing required ToolWorkerInterface exports:" << m_toolPath;
            m_workerLibrary.unload();
            return false;
        }

        const QByteArray toolIdUtf8 = m_toolInfo.id.toUtf8();
        m_workerHandle = m_workerCreate(toolIdUtf8.constData());
        if (!m_workerHandle) {
            qCritical() << "Worker create failed for tool:" << m_toolInfo.id;
            m_workerLibrary.unload();
            return false;
        }

        const QString languageCode = ConfigManager::instance().getLanguage();
        QJsonObject initConfig = ConfigManager::instance().toJson();
        initConfig[QStringLiteral("toolDirectory")] = fi.absolutePath();
        initConfig[QStringLiteral("language")] = languageCode;
        initConfig[QStringLiteral("localizedStrings")] = stringMapToJsonObject(
            LocalizationManager::instance().loadToolStrings(fi.absolutePath(), languageCode)
        );
        const QByteArray initJson = QJsonDocument(initConfig).toJson(QJsonDocument::Compact);
        const ToolWorkerResult initResult = m_workerInitialize(m_workerHandle, initJson.constData());
        if (initResult != TOOL_WORKER_SUCCESS) {
            const QString workerError = m_workerGetLastError
                ? QString::fromUtf8(m_workerGetLastError(m_workerHandle))
                : QStringLiteral("Unknown worker initialization error");
            qCritical() << "Worker initialize failed:" << workerError;
            m_workerDestroy(m_workerHandle);
            m_workerHandle = nullptr;
            m_workerLibrary.unload();
            return false;
        }

        m_workerMode = true;
        if (m_workerGetVersion && m_toolInfo.version.isEmpty()) {
            m_toolInfo.version = QString::fromUtf8(m_workerGetVersion());
        }
        qDebug() << "Tool loaded as worker library:" << m_toolInfo.id;
        return true;
    }
    
    void connectToServer() {
        if (m_shutdownRequested) {
            return;
        }

        if (m_socket->state() == QLocalSocket::ConnectedState
            || m_socket->state() == QLocalSocket::ConnectingState) {
            return;
        }

        qDebug() << "Connecting to server:" << m_serverName;
        m_socket->connectToServer(m_serverName);
    }

private slots:
    void onConnected() {
        qDebug() << "Connected to main process";
        m_connectedOnce = true;
        m_connectRetryCount = 0;
        m_retryScheduled = false;
        m_heartbeatTimer->start(ToolIpc::HEARTBEAT_INTERVAL_MS);
        
        // Send ready signal with tool info FIRST (don't block)
        ToolIpc::ToolInfo info = m_toolInfo;
        
        QJsonObject payload;
        payload["toolInfo"] = info.toJson();
        
        sendMessage(ToolIpc::MessageType::Ready, payload);
        
        // Request data asynchronously (non-blocking)
        requestInitialDataAsync();
    }
    
    void onDisconnected() {
        m_heartbeatTimer->stop();

        if (m_shutdownRequested) {
            qDebug() << "Disconnected from main process during shutdown, exiting...";
            QApplication::quit();
            return;
        }

        qDebug() << "Disconnected from main process, exiting...";
        QApplication::quit();
    }
    
    void onError(QLocalSocket::LocalSocketError error) {
        qCritical() << "Socket error:" << error << m_socket->errorString();

        if (m_shutdownRequested) {
            return;
        }

        const bool isRetryableError =
            error == QLocalSocket::ServerNotFoundError ||
            error == QLocalSocket::ConnectionRefusedError;

        if (!isRetryableError) {
            if (!m_connectedOnce) {
                qCritical() << "Tool host failed before establishing IPC, exiting.";
                QApplication::quit();
            }
            return;
        }

        if (m_connectedOnce) {
            qCritical() << "IPC server disappeared after connection, exiting tool host.";
            QApplication::quit();
            return;
        }

        constexpr int kMaxInitialConnectRetries = 4;
        constexpr int kRetryDelayMs = 150;

        if (m_retryScheduled) {
            return;
        }

        if (m_connectRetryCount >= kMaxInitialConnectRetries) {
            qCritical() << "IPC connection retries exhausted, exiting tool host.";
            QApplication::quit();
            return;
        }

        ++m_connectRetryCount;
        m_retryScheduled = true;
        qWarning() << "Retrying IPC connection"
                   << m_connectRetryCount
                   << "/"
                   << kMaxInitialConnectRetries;

        QTimer::singleShot(kRetryDelayMs, this, [this]() {
            m_retryScheduled = false;
            connectToServer();
        });
    }
    
    void onReadyRead() {
        processAvailableMessages();
    }

    void processAvailableMessages() {
        if (!m_socket) {
            return;
        }

        m_buffer.append(m_socket->readAll());
        
        while (m_buffer.size() >= 4) {
            quint32 msgLen;
            memcpy(&msgLen, m_buffer.constData(), sizeof(msgLen));
            
            if (m_buffer.size() < 4 + static_cast<int>(msgLen)) {
                break; // Wait for more data
            }
            
            QByteArray msgData = m_buffer.mid(4, msgLen);
            m_buffer.remove(0, 4 + msgLen);
            
            ToolIpc::Message msg = ToolIpc::Message::deserialize(msgData);
            handleMessage(msg);
        }
    }
    
    void sendHeartbeat() {
        sendMessage(ToolIpc::MessageType::Heartbeat);
    }

private:
    void handleMessage(const ToolIpc::Message& msg) {
        switch (msg.type) {
        case ToolIpc::MessageType::HeartbeatAck:
            break;
            
        case ToolIpc::MessageType::LoadLanguage:
            handleLoadLanguage(msg);
            break;
            
        case ToolIpc::MessageType::ApplyTheme:
            handleApplyTheme(msg);
            break;
            
        case ToolIpc::MessageType::GetToolInfo:
            handleGetToolInfo(msg);
            break;

        case ToolIpc::MessageType::UiAction:
            handleUiActionMessage(msg);
            break;

        case ToolIpc::MessageType::StateQuery:
            handleStateQueryMessage(msg);
            break;
            
        case ToolIpc::MessageType::Shutdown:
            qDebug() << "Shutdown requested";
            handleShutdown();
            break;
            
        case ToolIpc::MessageType::ConfigResponse:
        case ToolIpc::MessageType::FileIndexResponse:
        case ToolIpc::MessageType::InvokePluginResponse:
        case ToolIpc::MessageType::ReadMatchingTextFilesResponse:
        case ToolIpc::MessageType::ReadBinaryFileResponse:
        case ToolIpc::MessageType::ReadTextFileResponse:
        case ToolIpc::MessageType::ReadEffectiveBinaryFileResponse:
        case ToolIpc::MessageType::ReadEffectiveTextFileResponse:
        case ToolIpc::MessageType::ReadEffectiveTextFilesResponse:
        case ToolIpc::MessageType::WriteBinaryFileResponse:
        case ToolIpc::MessageType::WriteTextFileResponse:
        case ToolIpc::MessageType::RemovePathResponse:
        case ToolIpc::MessageType::EnsureDirectoryResponse:
        case ToolIpc::MessageType::ListDirectoryResponse:
        case ToolIpc::MessageType::ListEffectiveFilesResponse:
            handleDataResponse(msg);
            break;
            
        default:
            qWarning() << "Unknown message type:" << static_cast<int>(msg.type);
            break;
        }
    }
    
    void handleLoadLanguage(const ToolIpc::Message& msg) {
        const QString lang = msg.payload["language"].toString();

        if (m_workerMode) {
            if (!m_workerHandle || !m_workerHandleAction) {
                return;
            }

            const QByteArray actionTypeUtf8 = QByteArrayLiteral("load_language");
            QJsonObject argumentsObject;
            argumentsObject[QStringLiteral("language")] = lang;
            argumentsObject[QStringLiteral("gameLanguage")] =
                msg.payload.value(QStringLiteral("gameLanguage")).toString(ConfigManager::instance().getGameLanguage());
            argumentsObject[QStringLiteral("gameLanguageNames")] =
                msg.payload.value(QStringLiteral("gameLanguageNames")).toObject(
                    ConfigManager::instance().toJson().value(QStringLiteral("gameLanguageNames")).toObject());
            argumentsObject[QStringLiteral("localizedStrings")] = stringMapToJsonObject(
                LocalizationManager::instance().loadToolStrings(QFileInfo(m_toolPath).absolutePath(), lang)
            );
            const QByteArray argumentsUtf8 =
                QJsonDocument(argumentsObject).toJson(QJsonDocument::Compact);

            ToolWorkerResult result = TOOL_WORKER_ERROR_UNKNOWN;
            const char* stateJson = m_workerHandleAction(
                m_workerHandle,
                actionTypeUtf8.constData(),
                "",
                argumentsUtf8.constData(),
                &result
            );
            if (stateJson && m_workerFreeString) {
                m_workerFreeString(stateJson);
            }
            if (result != TOOL_WORKER_SUCCESS && m_workerGetLastError) {
                Logger::instance().logWarning(
                    "ToolHost",
                    QStringLiteral("Worker language update failed: %1").arg(QString::fromUtf8(m_workerGetLastError(m_workerHandle)))
                );
            }
            return;
        }
    }
    
    void handleApplyTheme(const ToolIpc::Message& msg) {
        Q_UNUSED(msg);
    }
    
    void handleGetToolInfo(const ToolIpc::Message& msg) {
        ToolIpc::ToolInfo info = m_toolInfo;
        
        QJsonObject payload;
        payload["toolInfo"] = info.toJson();
        
        sendMessage(ToolIpc::MessageType::ToolInfoResponse, payload, msg.requestId);
    }

    void handleUiActionMessage(const ToolIpc::Message& msg) {
        QJsonObject payload;

        if (m_workerMode) {
            if (!m_workerHandle || !m_workerHandleAction) {
                payload["success"] = false;
                payload["error"] = QStringLiteral("Worker handle is not available.");
                sendMessage(ToolIpc::MessageType::UiActionResponse, payload, msg.requestId);
                return;
            }

            const QByteArray actionTypeUtf8 = msg.payload.value("actionType").toString().toUtf8();
            const QByteArray targetIdUtf8 = msg.payload.value("targetId").toString().toUtf8();
            const QByteArray argumentsUtf8 =
                QJsonDocument(msg.payload.value("arguments").toObject()).toJson(QJsonDocument::Compact);
            const quint32 requestId = msg.requestId;

            QTimer::singleShot(0, this, [this, requestId, actionTypeUtf8, targetIdUtf8, argumentsUtf8]() {
                sendWorkerUiActionResponse(requestId, actionTypeUtf8, targetIdUtf8, argumentsUtf8);
            });
            return;
        }

        payload["success"] = false;
        payload["error"] = QStringLiteral("Worker handle is not available.");
        sendMessage(ToolIpc::MessageType::UiActionResponse, payload, msg.requestId);
    }

    void sendWorkerUiActionResponse(quint32 requestId,
                                    const QByteArray& actionTypeUtf8,
                                    const QByteArray& targetIdUtf8,
                                    const QByteArray& argumentsUtf8) {
        QJsonObject payload;
        if (!m_workerMode || m_shutdownRequested || !m_workerHandle || !m_workerHandleAction) {
            payload["success"] = false;
            payload["error"] = QStringLiteral("Worker handle is not available.");
            sendMessage(ToolIpc::MessageType::UiActionResponse, payload, requestId);
            return;
        }

        if (!m_socket || m_socket->state() != QLocalSocket::ConnectedState) {
            return;
        }

        ToolWorkerResult result = TOOL_WORKER_ERROR_UNKNOWN;
        const char* stateJson = m_workerHandleAction(
            m_workerHandle,
            actionTypeUtf8.constData(),
            targetIdUtf8.constData(),
            argumentsUtf8.constData(),
            &result
        );

        payload["success"] = (result == TOOL_WORKER_SUCCESS);
        payload["state"] = jsonObjectFromUtf8(stateJson);
        if (result != TOOL_WORKER_SUCCESS) {
            payload["error"] = m_workerGetLastError
                ? QString::fromUtf8(m_workerGetLastError(m_workerHandle))
                : QStringLiteral("Worker action failed.");
        }
        if (stateJson && m_workerFreeString) {
            m_workerFreeString(stateJson);
        }
        sendMessage(ToolIpc::MessageType::UiActionResponse, payload, requestId);
    }

    void handleStateQueryMessage(const ToolIpc::Message& msg) {
        QJsonObject payload;

        if (m_workerMode) {
            if (!m_workerHandle || !m_workerGetCurrentState) {
                payload["success"] = false;
                payload["error"] = QStringLiteral("Worker state query is unavailable.");
                sendMessage(ToolIpc::MessageType::StateQueryResponse, payload, msg.requestId);
                return;
            }

            const bool initialQuery = msg.payload.value("initial").toBool(false);
            if (initialQuery && !m_dataReady) {
                m_pendingInitialStateQueries.append(msg);
                return;
            }

            ToolWorkerResult result = TOOL_WORKER_ERROR_UNKNOWN;
            const char* stateJson = nullptr;
            if (initialQuery && m_workerGetInitialState) {
                stateJson = m_workerGetInitialState(m_workerHandle, &result);
            } else {
                stateJson = m_workerGetCurrentState(m_workerHandle, &result);
            }
            payload["success"] = (result == TOOL_WORKER_SUCCESS);
            payload["state"] = jsonObjectFromUtf8(stateJson);
            if (result != TOOL_WORKER_SUCCESS) {
                payload["error"] = m_workerGetLastError
                    ? QString::fromUtf8(m_workerGetLastError(m_workerHandle))
                    : QStringLiteral("Worker state query failed.");
            }
            if (stateJson && m_workerFreeString) {
                m_workerFreeString(stateJson);
            }
            sendMessage(ToolIpc::MessageType::StateQueryResponse, payload, msg.requestId);
            return;
        }

        payload["success"] = false;
        payload["error"] = QStringLiteral("Worker state query is unavailable.");
        sendMessage(ToolIpc::MessageType::StateQueryResponse, payload, msg.requestId);
    }

    void processPendingInitialStateQueries() {
        if (!m_dataReady || m_shutdownRequested || m_pendingInitialStateQueries.isEmpty()) {
            return;
        }

        const QList<ToolIpc::Message> pendingQueries = m_pendingInitialStateQueries;
        m_pendingInitialStateQueries.clear();
        for (const ToolIpc::Message& pendingQuery : pendingQueries) {
            handleStateQueryMessage(pendingQuery);
        }
    }

    ToolRuntimeContext::MatchingTextFilesResult requestMatchingTextFiles(ToolRuntimeContext::FileRoot root,
                                                                         const QString& relativePath,
                                                                         const QString& regexPattern,
                                                                         bool recursive) {
        ToolRuntimeContext::MatchingTextFilesResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["root"] = ToolRuntimeContext::fileRootToString(root);
        payload["relativePath"] = relativePath;
        payload["regexPattern"] = regexPattern;
        payload["recursive"] = recursive;

        m_matchingTextFilesRequestCompleted = false;
        m_matchingTextFilesRequestResult = ToolRuntimeContext::MatchingTextFilesResult{};
        m_matchingTextFilesRequestId = requestId;

        sendMessage(ToolIpc::MessageType::ReadMatchingTextFiles, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_matchingTextFilesRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_matchingTextFilesRequestCompleted) {
            m_matchingTextFilesRequestId = 0;
            result.errorMessage = QString("Timed out while reading matching text files: %1").arg(relativePath);
            return result;
        }

        m_matchingTextFilesRequestId = 0;
        return m_matchingTextFilesRequestResult;
    }

    ToolRuntimeContext::PluginInvokeResponse requestInvokePlugin(const ToolRuntimeContext::PluginInvokeRequest& request) {
        ToolRuntimeContext::PluginInvokeResponse result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["pluginName"] = request.pluginName;
        payload["operation"] = request.operation;
        payload["contentType"] = static_cast<int>(request.contentType);
        payload["flags"] = static_cast<int>(request.flags);
        payload["payloadBase64"] = QString::fromLatin1(request.payload.toBase64());

        m_pluginInvokeRequestCompleted = false;
        m_pluginInvokeRequestResult = ToolRuntimeContext::PluginInvokeResponse{};
        m_pluginInvokeRequestId = requestId;

        sendMessage(ToolIpc::MessageType::InvokePlugin, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_pluginInvokeRequestCompleted && timer.elapsed() < 30000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_pluginInvokeRequestCompleted) {
            m_pluginInvokeRequestId = 0;
            result.errorMessage = QStringLiteral("Timed out while invoking plugin operation: %1").arg(request.operation);
            return result;
        }

        m_pluginInvokeRequestId = 0;
        return m_pluginInvokeRequestResult;
    }

    ToolRuntimeContext::FileReadResult requestBinaryFile(ToolRuntimeContext::FileRoot root, const QString& relativePath) {
        ToolRuntimeContext::FileReadResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["root"] = ToolRuntimeContext::fileRootToString(root);
        payload["relativePath"] = relativePath;

        m_binaryReadRequestCompleted = false;
        m_binaryReadRequestResult = ToolRuntimeContext::FileReadResult{};
        m_binaryReadRequestId = requestId;

        sendMessage(ToolIpc::MessageType::ReadBinaryFile, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_binaryReadRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_binaryReadRequestCompleted) {
            m_binaryReadRequestId = 0;
            result.errorMessage = QString("Timed out while reading binary file: %1").arg(relativePath);
            return result;
        }

        m_binaryReadRequestId = 0;
        return m_binaryReadRequestResult;
    }

    ToolRuntimeContext::TextReadResult requestTextFile(ToolRuntimeContext::FileRoot root, const QString& relativePath) {
        ToolRuntimeContext::TextReadResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["root"] = ToolRuntimeContext::fileRootToString(root);
        payload["relativePath"] = relativePath;

        m_textReadRequestCompleted = false;
        m_textReadRequestResult = ToolRuntimeContext::TextReadResult{};
        m_textReadRequestId = requestId;

        sendMessage(ToolIpc::MessageType::ReadTextFile, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_textReadRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_textReadRequestCompleted) {
            m_textReadRequestId = 0;
            result.errorMessage = QString("Timed out while reading text file: %1").arg(relativePath);
            return result;
        }

        m_textReadRequestId = 0;
        return m_textReadRequestResult;
    }

    ToolRuntimeContext::FileReadResult requestEffectiveBinaryFile(const QString& relativePath) {
        ToolRuntimeContext::FileReadResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["relativePath"] = relativePath;

        m_effectiveBinaryReadRequestCompleted = false;
        m_effectiveBinaryReadRequestResult = ToolRuntimeContext::FileReadResult{};
        m_effectiveBinaryReadRequestId = requestId;

        sendMessage(ToolIpc::MessageType::ReadEffectiveBinaryFile, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_effectiveBinaryReadRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_effectiveBinaryReadRequestCompleted) {
            m_effectiveBinaryReadRequestId = 0;
            result.errorMessage = QString("Timed out while reading effective binary file: %1").arg(relativePath);
            return result;
        }

        m_effectiveBinaryReadRequestId = 0;
        return m_effectiveBinaryReadRequestResult;
    }

    ToolRuntimeContext::TextReadResult requestEffectiveTextFile(const QString& relativePath) {
        ToolRuntimeContext::TextReadResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["relativePath"] = relativePath;

        m_effectiveTextReadRequestCompleted = false;
        m_effectiveTextReadRequestResult = ToolRuntimeContext::TextReadResult{};
        m_effectiveTextReadRequestId = requestId;

        sendMessage(ToolIpc::MessageType::ReadEffectiveTextFile, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_effectiveTextReadRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_effectiveTextReadRequestCompleted) {
            m_effectiveTextReadRequestId = 0;
            result.errorMessage = QString("Timed out while reading effective text file: %1").arg(relativePath);
            return result;
        }

        m_effectiveTextReadRequestId = 0;
        return m_effectiveTextReadRequestResult;
    }

    ToolRuntimeContext::MatchingTextFilesResult requestEffectiveTextFiles(const QString& relativeRoot,
                                                                          const QString& suffixFilter) {
        ToolRuntimeContext::MatchingTextFilesResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        if (!relativeRoot.trimmed().isEmpty()) {
            payload.insert(QStringLiteral("relativeRoot"), relativeRoot);
        }
        if (!suffixFilter.trimmed().isEmpty()) {
            payload.insert(QStringLiteral("suffixFilter"), suffixFilter);
        }

        m_effectiveTextFilesRequestCompleted = false;
        m_effectiveTextFilesRequestResult = ToolRuntimeContext::MatchingTextFilesResult{};
        m_effectiveTextFilesRequestId = requestId;

        sendMessage(ToolIpc::MessageType::ReadEffectiveTextFiles, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_effectiveTextFilesRequestCompleted && timer.elapsed() < 10000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_effectiveTextFilesRequestCompleted) {
            m_effectiveTextFilesRequestId = 0;
            result.errorMessage = QStringLiteral("Timed out while reading effective text files.");
            return result;
        }

        m_effectiveTextFilesRequestId = 0;
        return m_effectiveTextFilesRequestResult;
    }

    ToolRuntimeContext::FileWriteResult requestWriteBinaryFile(ToolRuntimeContext::FileRoot root, const QString& relativePath, const QByteArray& content) {
        ToolRuntimeContext::FileWriteResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["root"] = ToolRuntimeContext::fileRootToString(root);
        payload["relativePath"] = relativePath;
        payload["contentBase64"] = QString::fromLatin1(content.toBase64());

        m_writeRequestCompleted = false;
        m_writeRequestResult = ToolRuntimeContext::FileWriteResult{};
        m_writeRequestId = requestId;

        sendMessage(ToolIpc::MessageType::WriteBinaryFile, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_writeRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_writeRequestCompleted) {
            m_writeRequestId = 0;
            result.errorMessage = QString("Timed out while writing binary file: %1").arg(relativePath);
            return result;
        }

        m_writeRequestId = 0;
        return m_writeRequestResult;
    }

    ToolRuntimeContext::FileWriteResult requestWriteTextFile(ToolRuntimeContext::FileRoot root, const QString& relativePath, const QString& content) {
        ToolRuntimeContext::FileWriteResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["root"] = ToolRuntimeContext::fileRootToString(root);
        payload["relativePath"] = relativePath;
        payload["content"] = content;

        m_writeRequestCompleted = false;
        m_writeRequestResult = ToolRuntimeContext::FileWriteResult{};
        m_writeRequestId = requestId;

        sendMessage(ToolIpc::MessageType::WriteTextFile, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_writeRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_writeRequestCompleted) {
            m_writeRequestId = 0;
            result.errorMessage = QString("Timed out while writing text file: %1").arg(relativePath);
            return result;
        }

        m_writeRequestId = 0;
        return m_writeRequestResult;
    }

    ToolRuntimeContext::FileWriteResult requestRemovePath(ToolRuntimeContext::FileRoot root, const QString& relativePath) {
        ToolRuntimeContext::FileWriteResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["root"] = ToolRuntimeContext::fileRootToString(root);
        payload["relativePath"] = relativePath;

        m_writeRequestCompleted = false;
        m_writeRequestResult = ToolRuntimeContext::FileWriteResult{};
        m_writeRequestId = requestId;

        sendMessage(ToolIpc::MessageType::RemovePath, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_writeRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_writeRequestCompleted) {
            m_writeRequestId = 0;
            result.errorMessage = QString("Timed out while removing path: %1").arg(relativePath);
            return result;
        }

        m_writeRequestId = 0;
        return m_writeRequestResult;
    }

    ToolRuntimeContext::FileWriteResult requestEnsureDirectory(ToolRuntimeContext::FileRoot root, const QString& relativePath) {
        ToolRuntimeContext::FileWriteResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["root"] = ToolRuntimeContext::fileRootToString(root);
        payload["relativePath"] = relativePath;

        m_writeRequestCompleted = false;
        m_writeRequestResult = ToolRuntimeContext::FileWriteResult{};
        m_writeRequestId = requestId;

        sendMessage(ToolIpc::MessageType::EnsureDirectory, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_writeRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_writeRequestCompleted) {
            m_writeRequestId = 0;
            result.errorMessage = QString("Timed out while ensuring directory: %1").arg(relativePath);
            return result;
        }

        m_writeRequestId = 0;
        return m_writeRequestResult;
    }

    ToolRuntimeContext::DirectoryListResult requestListDirectory(ToolRuntimeContext::FileRoot root, const QString& relativePath, bool recursive) {
        ToolRuntimeContext::DirectoryListResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        payload["root"] = ToolRuntimeContext::fileRootToString(root);
        payload["relativePath"] = relativePath;
        payload["recursive"] = recursive;

        m_directoryListRequestCompleted = false;
        m_directoryListRequestResult = ToolRuntimeContext::DirectoryListResult{};
        m_directoryListRequestId = requestId;

        sendMessage(ToolIpc::MessageType::ListDirectory, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_directoryListRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_directoryListRequestCompleted) {
            m_directoryListRequestId = 0;
            result.errorMessage = QString("Timed out while listing directory: %1").arg(relativePath);
            return result;
        }

        m_directoryListRequestId = 0;
        return m_directoryListRequestResult;
    }

    ToolRuntimeContext::EffectiveFileListResult requestListEffectiveFiles(const QString& relativeRoot = QString(),
                                                                          const QString& suffixFilter = QString()) {
        ToolRuntimeContext::EffectiveFileListResult result;
        if (m_socket->state() != QLocalSocket::ConnectedState) {
            result.errorMessage = "IPC socket is not connected.";
            return result;
        }

        const quint32 requestId = ++m_requestId;
        QJsonObject payload;
        if (!relativeRoot.trimmed().isEmpty()) {
            payload.insert(QStringLiteral("relativeRoot"), relativeRoot);
        }
        if (!suffixFilter.trimmed().isEmpty()) {
            payload.insert(QStringLiteral("suffixFilter"), suffixFilter);
        }

        m_effectiveFileListRequestCompleted = false;
        m_effectiveFileListRequestResult = ToolRuntimeContext::EffectiveFileListResult{};
        m_effectiveFileListRequestId = requestId;

        sendMessage(ToolIpc::MessageType::ListEffectiveFiles, payload, requestId);

        QElapsedTimer timer;
        timer.start();
        while (!m_effectiveFileListRequestCompleted && timer.elapsed() < 5000) {
            processAvailableMessages();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            processAvailableMessages();
            QThread::msleep(10);
        }

        if (!m_effectiveFileListRequestCompleted) {
            m_effectiveFileListRequestId = 0;
            result.errorMessage = QStringLiteral("Timed out while listing effective files.");
            return result;
        }

        m_effectiveFileListRequestId = 0;
        return m_effectiveFileListRequestResult;
    }

    void handleDataResponse(const ToolIpc::Message& msg) {
        switch (msg.type) {
        case ToolIpc::MessageType::ConfigResponse:
            ConfigManager::instance().setFromJson(msg.payload);
            qDebug() << "Received config data from main process";
            m_configReceived = true;
            break;

        case ToolIpc::MessageType::FileIndexResponse:
            FileManager::instance().setFromJson(msg.payload);
            qDebug() << "Received file index data from main process";
            m_fileIndexReceived = true;
            break;

        case ToolIpc::MessageType::InvokePluginResponse:
            if (msg.requestId == m_pluginInvokeRequestId) {
                m_pluginInvokeRequestCompleted = true;
                m_pluginInvokeRequestResult.success = msg.payload.value("success").toBool();
                m_pluginInvokeRequestResult.status = static_cast<quint32>(msg.payload.value("status").toInt());
                m_pluginInvokeRequestResult.contentType =
                    static_cast<ToolRuntimeContext::PluginPayloadContentType>(msg.payload.value("contentType").toInt());
                m_pluginInvokeRequestResult.flags = static_cast<quint32>(msg.payload.value("flags").toInt());
                m_pluginInvokeRequestResult.payload =
                    QByteArray::fromBase64(msg.payload.value("payloadBase64").toString().toLatin1());
                m_pluginInvokeRequestResult.errorMessage = msg.payload.value("error").toString();
            }
            break;

        case ToolIpc::MessageType::ReadMatchingTextFilesResponse:
            if (msg.requestId == m_matchingTextFilesRequestId) {
                m_matchingTextFilesRequestCompleted = true;
                m_matchingTextFilesRequestResult.success = msg.payload.value("success").toBool();
                m_matchingTextFilesRequestResult.errorMessage = msg.payload.value("error").toString();
                m_matchingTextFilesRequestResult.entries.clear();

                const QJsonArray entries = msg.payload.value("entries").toArray();
                for (const QJsonValue& value : entries) {
                    const QJsonObject object = value.toObject();
                    ToolRuntimeContext::TextFileMatchEntry entry;
                    entry.relativePath = object.value("relativePath").toString();
                    entry.name = object.value("name").toString();
                    entry.content = object.value("content").toString();
                    m_matchingTextFilesRequestResult.entries.append(entry);
                }
            }
            break;

        case ToolIpc::MessageType::ReadBinaryFileResponse:
            if (msg.requestId == m_binaryReadRequestId) {
                m_binaryReadRequestCompleted = true;
                m_binaryReadRequestResult.success = msg.payload.value("success").toBool();
                m_binaryReadRequestResult.errorMessage = msg.payload.value("error").toString();
                m_binaryReadRequestResult.content =
                    QByteArray::fromBase64(msg.payload.value("contentBase64").toString().toLatin1());
            }
            break;

        case ToolIpc::MessageType::ReadTextFileResponse:
            if (msg.requestId == m_textReadRequestId) {
                m_textReadRequestCompleted = true;
                m_textReadRequestResult.success = msg.payload.value("success").toBool();
                m_textReadRequestResult.errorMessage = msg.payload.value("error").toString();
                m_textReadRequestResult.content = msg.payload.value("content").toString();
            }
            break;

        case ToolIpc::MessageType::ReadEffectiveBinaryFileResponse:
            if (msg.requestId == m_effectiveBinaryReadRequestId) {
                m_effectiveBinaryReadRequestCompleted = true;
                m_effectiveBinaryReadRequestResult.success = msg.payload.value("success").toBool();
                m_effectiveBinaryReadRequestResult.errorMessage = msg.payload.value("error").toString();
                m_effectiveBinaryReadRequestResult.content =
                    QByteArray::fromBase64(msg.payload.value("contentBase64").toString().toLatin1());
            }
            break;

        case ToolIpc::MessageType::ReadEffectiveTextFileResponse:
            if (msg.requestId == m_effectiveTextReadRequestId) {
                m_effectiveTextReadRequestCompleted = true;
                m_effectiveTextReadRequestResult.success = msg.payload.value("success").toBool();
                m_effectiveTextReadRequestResult.errorMessage = msg.payload.value("error").toString();
                m_effectiveTextReadRequestResult.content = msg.payload.value("content").toString();
            }
            break;

        case ToolIpc::MessageType::ReadEffectiveTextFilesResponse:
            if (msg.requestId == m_effectiveTextFilesRequestId) {
                m_effectiveTextFilesRequestCompleted = true;
                m_effectiveTextFilesRequestResult.success = msg.payload.value("success").toBool();
                m_effectiveTextFilesRequestResult.errorMessage = msg.payload.value("error").toString();
                m_effectiveTextFilesRequestResult.entries.clear();

                const QJsonArray entries = msg.payload.value("entries").toArray();
                for (const QJsonValue& value : entries) {
                    const QJsonObject object = value.toObject();
                    ToolRuntimeContext::TextFileMatchEntry entry;
                    entry.relativePath = object.value("relativePath").toString();
                    entry.name = object.value("name").toString();
                    entry.content = object.value("content").toString();
                    m_effectiveTextFilesRequestResult.entries.append(entry);
                }
            }
            break;

        case ToolIpc::MessageType::WriteBinaryFileResponse:
        case ToolIpc::MessageType::WriteTextFileResponse:
        case ToolIpc::MessageType::RemovePathResponse:
        case ToolIpc::MessageType::EnsureDirectoryResponse:
            if (msg.requestId == m_writeRequestId) {
                m_writeRequestCompleted = true;
                m_writeRequestResult.success = msg.payload.value("success").toBool();
                m_writeRequestResult.errorMessage = msg.payload.value("error").toString();
            }
            break;

        case ToolIpc::MessageType::ListDirectoryResponse:
            if (msg.requestId == m_directoryListRequestId) {
                m_directoryListRequestCompleted = true;
                m_directoryListRequestResult.success = msg.payload.value("success").toBool();
                m_directoryListRequestResult.errorMessage = msg.payload.value("error").toString();
                m_directoryListRequestResult.entries.clear();

                const QJsonArray entries = msg.payload.value("entries").toArray();
                for (const QJsonValue& value : entries) {
                    const QJsonObject object = value.toObject();
                    ToolRuntimeContext::DirectoryEntry entry;
                    entry.relativePath = object.value("relativePath").toString();
                    entry.name = object.value("name").toString();
                    entry.isDirectory = object.value("isDirectory").toBool();
                    entry.size = static_cast<qint64>(object.value("size").toDouble(-1));
                    entry.lastModifiedUtc = QDateTime::fromString(
                        object.value("lastModifiedUtc").toString(),
                        Qt::ISODateWithMs
                    );
                    m_directoryListRequestResult.entries.append(entry);
                }
            }
            break;

        case ToolIpc::MessageType::ListEffectiveFilesResponse:
            if (msg.requestId == m_effectiveFileListRequestId) {
                m_effectiveFileListRequestCompleted = true;
                m_effectiveFileListRequestResult.success = msg.payload.value("success").toBool();
                m_effectiveFileListRequestResult.errorMessage = msg.payload.value("error").toString();
                m_effectiveFileListRequestResult.entries.clear();

                const QJsonArray entries = msg.payload.value("entries").toArray();
                for (const QJsonValue& value : entries) {
                    const QJsonObject object = value.toObject();
                    ToolRuntimeContext::EffectiveFileEntry entry;
                    entry.logicalPath = object.value("logicalPath").toString();
                    entry.source = ToolRuntimeContext::effectiveFileSourceFromString(object.value("source").toString());
                    entry.lastModifiedMs = object.value("lastModifiedMs").toString().toLongLong();
                    m_effectiveFileListRequestResult.entries.append(entry);
                }
            }
            break;

        default:
            break;
        }

        const bool becameDataReady = !m_dataReady && m_configReceived && m_fileIndexReceived;
        if (m_configReceived && m_fileIndexReceived) {
            m_dataReady = true;
            if (m_dataWaitLoop) {
                m_dataWaitLoop->quit();
            }
        }

        if (becameDataReady) {
            QTimer::singleShot(0, this, [this]() {
                processPendingInitialStateQueries();
            });
        }
    }
    
    void handleShutdown() {
        qDebug() << "Handling shutdown - hiding widgets first";
        m_shutdownRequested = true;
        
        
        m_heartbeatTimer->stop();

        if (m_workerMode && m_workerDestroy && m_workerHandle) {
            m_workerDestroy(m_workerHandle);
            m_workerHandle = nullptr;
        }
        if (m_workerMode && m_workerLibrary.isLoaded()) {
            m_workerLibrary.unload();
        }
        
        if (m_socket->state() == QLocalSocket::ConnectedState) {
            m_socket->disconnectFromServer();
        }
        
        QApplication::quit();
    }
    
    void requestInitialDataAsync() {
        m_configReceived = false;
        m_fileIndexReceived = m_workerMode;
        m_dataReady = false;
        
        sendMessage(ToolIpc::MessageType::GetConfig);
        if (!m_workerMode) {
            sendMessage(ToolIpc::MessageType::GetFileIndex);
        }
        
        qDebug() << "Requested initial data from main process (async), workerMode=" << m_workerMode;
    }
    
    void sendMessage(ToolIpc::MessageType type, const QJsonObject& payload = QJsonObject(), quint32 requestId = 0) {
        ToolIpc::Message msg = ToolIpc::createMessage(type, requestId, payload);
        m_socket->write(msg.serialize());
        m_socket->flush();
    }
    
private:
    QString m_serverName;
    QString m_toolPath;
    QLocalSocket* m_socket;
    QTimer* m_heartbeatTimer;
    QByteArray m_buffer;
    ToolIpc::ToolInfo m_toolInfo;
    
    quint32 m_requestId = 0;

    bool m_workerMode = false;
    QLibrary m_workerLibrary;
    ToolWorkerHandle m_workerHandle = nullptr;
    WorkerCreateFn m_workerCreate = nullptr;
    WorkerDestroyFn m_workerDestroy = nullptr;
    WorkerInitializeFn m_workerInitialize = nullptr;
    WorkerHandleActionFn m_workerHandleAction = nullptr;
    WorkerGetStateFn m_workerGetCurrentState = nullptr;
    WorkerGetStateFn m_workerGetInitialState = nullptr;
    WorkerGetLastErrorFn m_workerGetLastError = nullptr;
    WorkerFreeStringFn m_workerFreeString = nullptr;
    WorkerGetVersionFn m_workerGetVersion = nullptr;
    
    bool m_configReceived = false;
    bool m_fileIndexReceived = false;
    bool m_dataReady = false;
    int m_connectRetryCount = 0;
    bool m_connectedOnce = false;
    bool m_retryScheduled = false;
    bool m_shutdownRequested = false;
    bool m_pluginInvokeRequestCompleted = false;
    quint32 m_pluginInvokeRequestId = 0;
    ToolRuntimeContext::PluginInvokeResponse m_pluginInvokeRequestResult;

    bool m_matchingTextFilesRequestCompleted = false;
    quint32 m_matchingTextFilesRequestId = 0;
    ToolRuntimeContext::MatchingTextFilesResult m_matchingTextFilesRequestResult;

    bool m_binaryReadRequestCompleted = false;
    quint32 m_binaryReadRequestId = 0;
    ToolRuntimeContext::FileReadResult m_binaryReadRequestResult;

    bool m_textReadRequestCompleted = false;
    quint32 m_textReadRequestId = 0;
    ToolRuntimeContext::TextReadResult m_textReadRequestResult;

    bool m_effectiveBinaryReadRequestCompleted = false;
    quint32 m_effectiveBinaryReadRequestId = 0;
    ToolRuntimeContext::FileReadResult m_effectiveBinaryReadRequestResult;

    bool m_effectiveTextReadRequestCompleted = false;
    quint32 m_effectiveTextReadRequestId = 0;
    ToolRuntimeContext::TextReadResult m_effectiveTextReadRequestResult;

    bool m_effectiveTextFilesRequestCompleted = false;
    quint32 m_effectiveTextFilesRequestId = 0;
    ToolRuntimeContext::MatchingTextFilesResult m_effectiveTextFilesRequestResult;

    bool m_writeRequestCompleted = false;
    quint32 m_writeRequestId = 0;
    ToolRuntimeContext::FileWriteResult m_writeRequestResult;

    bool m_directoryListRequestCompleted = false;
    quint32 m_directoryListRequestId = 0;
    ToolRuntimeContext::DirectoryListResult m_directoryListRequestResult;

    bool m_effectiveFileListRequestCompleted = false;
    quint32 m_effectiveFileListRequestId = 0;
    ToolRuntimeContext::EffectiveFileListResult m_effectiveFileListRequestResult;

    QEventLoop* m_dataWaitLoop = nullptr;
    QList<ToolIpc::Message> m_pendingInitialStateQueries;
};

int runToolHostMode(const QString& serverName, const QString& toolPath, const QString& toolName, const QString& logFilePath) {
    // Set log file path first so all logs go to the same file as main process
    if (!logFilePath.isEmpty()) {
        Logger::instance().setLogFilePath(logFilePath);
    }
    
    Logger::instance().logInfo("ToolHost", "Running in tool host mode");
    Logger::instance().logInfo("ToolHost", QString("Server: %1").arg(serverName));
    Logger::instance().logInfo("ToolHost", QString("Tool: %1").arg(toolPath));
    Logger::instance().logInfo("ToolHost", QString("Tool Name: %1").arg(toolName));
    
    ToolHostApp hostApp(serverName, toolPath);
    
    if (!hostApp.loadTool()) {
        return 1;
    }
    
    hostApp.connectToServer();
    
    return qApp->exec();
}

#include "ToolHostMode.moc"
