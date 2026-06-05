//-------------------------------------------------------------------------------------
// ToolProxyInterface.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLPROXYINTERFACE_H
#define TOOLPROXYINTERFACE_H

#include <QObject>
#include <QProcess>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QMap>
#include <functional>
#include "ToolInterface.h"
#include "ToolIpcProtocol.h"

// Proxy class that implements ToolInterface but delegates to subprocess
class ToolProxyInterface : public QObject, public ToolInterface {
    Q_OBJECT
    Q_INTERFACES(ToolInterface)
public:
    explicit ToolProxyInterface(const QString& toolPath, const QString& toolDir, QObject* parent = nullptr);
    ~ToolProxyInterface();
    
    // ToolInterface implementation
    QString id() const override { return m_toolInfo.id; }
    QString name() const override { return m_toolInfo.name; }
    QString description() const override { return m_toolInfo.description; }
    QString version() const override { return m_toolInfo.version; }
    QString compatibleVersion() const override { return m_toolInfo.compatibleVersion; }
    QString author() const override { return m_toolInfo.author; }
    QStringList dependencies() const override { return m_toolInfo.dependencies; }
    QJsonObject metaData() const { return m_metaData; }
    
    void setMetaData(const QJsonObject& metaData) override;
    QIcon icon() const override;
    void initialize() override;
    
    // Scripted UI Resources (REQUIRED)
    ToolGuiResourceDescriptor guiResourceDescriptor() const override;
    ToolWorkerDescriptor workerDescriptor() const override;
    
    // Worker Session Lifecycle
    void initializeWorkerSession() override;
    ToolUiStatePacket initialUiState() const override;
    ToolUiStatePacket handleUiAction(const ToolUiActionRequest& request) override;

    void loadLanguage(const QString& lang) override;
    QMap<QString, QString> localizedStrings() const override { return m_localizedStrings; }
    void applyTheme() override;
    
    // Process management
    bool startProcess();
    void stopProcess();
    void forceKillProcess();
    void discardProcess();
    bool waitForProcessStopped(int timeoutMs);
    bool isProcessRunning() const;
    
    // Check if tool info is loaded (from descriptor.apehts pre-scan)
    bool isInfoLoaded() const { return m_infoLoaded; }
    void preloadInfo(); // Load basic info from descriptor.apehts without starting process

signals:
    void processStarted();
    void processStopped();
    void processCrashed(const QString& error);
    void statePacketUpdated(const QJsonObject& statePacket);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onHeartbeatTimeout();

private:
    void handleMessage(const ToolIpc::Message& msg);
    void handleDataRequest(const ToolIpc::Message& msg);
    void processAvailableMessages();
    void sendMessage(ToolIpc::MessageType type, const QJsonObject& payload = QJsonObject(), quint32 requestId = 0);
    quint32 nextRequestId() { return ++m_requestIdCounter; }
    bool isWorkerSessionReady() const;
    void stopHeartbeatTimers();
    void clearPendingRequests();
    void markSessionAvailable();
    void handleSessionUnavailable(const QString& reason, bool terminateProcess = false);
    void requestInitialStateAsync();
    bool sendStateRequest(ToolIpc::MessageType type,
                          const QJsonObject& payload = QJsonObject(),
                          const QString& reason = QString());
    void setCachedLifecycleState(const QString& status, const QString& message, bool notify);
    static ToolUiStatePacket parseStatePacket(const QJsonObject& jsonObject);
    
    // Request-response handling
    using ResponseCallback = std::function<void(const ToolIpc::Message&)>;
    void sendRequest(ToolIpc::MessageType type, const QJsonObject& payload, ResponseCallback callback);
    
    QString m_toolPath;
    QString m_toolDir;
    QString m_serverName;
    
    QProcess* m_process;
    QLocalServer* m_server;
    QLocalSocket* m_socket;
    QByteArray m_buffer;
    
    QTimer* m_heartbeatTimer;
    QTimer* m_heartbeatTimeoutTimer;
    
    QJsonObject m_metaData;
    ToolIpc::ToolInfo m_toolInfo;
    bool m_infoLoaded;
    bool m_processReady;
    bool m_stopping = false;
    bool m_sessionUnavailable = false;
    bool m_initialStateQueryPending = false;
    QString m_sessionUnavailableReason;
    
    quint32 m_requestIdCounter;
    QMap<quint32, ResponseCallback> m_pendingRequests;
    ToolUiStatePacket m_cachedStatePacket;
    QMap<QString, QString> m_localizedStrings;
    QString m_currentLanguageCode;
    QString m_currentGameLanguageCode;
    QJsonObject m_currentGameLanguageNames;

};

#endif // TOOLPROXYINTERFACE_H
