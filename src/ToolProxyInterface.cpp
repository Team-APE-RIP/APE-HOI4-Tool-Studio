//-------------------------------------------------------------------------------------
// ToolProxyInterface.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolProxyInterface.h"
#include "Logger.h"
#include "FileManager.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "ToolDescriptorParser.h"
#include "PluginAbiBroker.h"
#include "ToolRuntimeContext.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QStringConverter>
#include <QTextStream>
#include <QUuid>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
ToolRuntimeContext::FileRoot parseFileRootFromPayload(const QJsonObject& payload) {
    return ToolRuntimeContext::fileRootFromString(payload.value("root").toString());
}

QJsonObject makeFileReadResponsePayload(const ToolRuntimeContext::FileReadResult& result) {
    QJsonObject payload;
    payload["success"] = result.success;
    if (result.success) {
        payload["contentBase64"] = QString::fromLatin1(result.content.toBase64());
    } else {
        payload["error"] = result.errorMessage;
    }
    return payload;
}

QJsonObject makeTextReadResponsePayload(const ToolRuntimeContext::TextReadResult& result) {
    QJsonObject payload;
    payload["success"] = result.success;
    if (result.success) {
        payload["content"] = result.content;
    } else {
        payload["error"] = result.errorMessage;
    }
    return payload;
}

QJsonObject makeWriteResponsePayload(const ToolRuntimeContext::FileWriteResult& result) {
    QJsonObject payload;
    payload["success"] = result.success;
    if (!result.success) {
        payload["error"] = result.errorMessage;
    }
    return payload;
}

QJsonArray makeDirectoryEntriesJson(const QList<ToolRuntimeContext::DirectoryEntry>& entries) {
    QJsonArray array;
    for (const ToolRuntimeContext::DirectoryEntry& entry : entries) {
        QJsonObject object;
        object["relativePath"] = entry.relativePath;
        object["name"] = entry.name;
        object["isDirectory"] = entry.isDirectory;
        object["size"] = static_cast<qint64>(entry.size);
        object["lastModifiedUtc"] = entry.lastModifiedUtc.toString(Qt::ISODateWithMs);
        array.append(object);
    }
    return array;
}

QJsonArray makeMatchingTextFileEntriesJson(const QList<ToolRuntimeContext::TextFileMatchEntry>& entries) {
    QJsonArray array;
    for (const ToolRuntimeContext::TextFileMatchEntry& entry : entries) {
        QJsonObject object;
        object["relativePath"] = entry.relativePath;
        object["name"] = entry.name;
        object["content"] = entry.content;
        array.append(object);
    }
    return array;
}

QJsonArray makeEffectiveFileEntriesJson(const QList<ToolRuntimeContext::EffectiveFileEntry>& entries) {
    QJsonArray array;
    for (const ToolRuntimeContext::EffectiveFileEntry& entry : entries) {
        QJsonObject object;
        object["logicalPath"] = entry.logicalPath;
        object["source"] = ToolRuntimeContext::effectiveFileSourceToString(entry.source);
        object["lastModifiedMs"] = QString::number(entry.lastModifiedMs);
        array.append(object);
    }
    return array;
}

QJsonObject statePacketToJsonObject(const ToolUiStatePacket& packet) {
    QJsonObject object;
    object["pageId"] = packet.pageId;
    object["modeId"] = packet.modeId;
    object["viewState"] = QJsonObject::fromVariantMap(packet.viewState);
    object["sidebarState"] = QJsonObject::fromVariantMap(packet.sidebarState);
    object["topbarState"] = QJsonObject::fromVariantMap(packet.topbarState);
    object["runtimeVariables"] = QJsonObject::fromVariantMap(packet.runtimeVariables);
    object["listModels"] = QJsonArray::fromVariantList(packet.listModels);
    object["patches"] = QJsonArray::fromVariantList(packet.patches);
    return object;
}

QVariantMap toVariantMap(const QMap<QString, QString>& strings) {
    QVariantMap variantMap;
    for (auto iterator = strings.constBegin(); iterator != strings.constEnd(); ++iterator) {
        variantMap.insert(iterator.key(), iterator.value());
    }
    return variantMap;
}
} // namespace

// ============================================================================
// ToolProxyInterface Implementation
// ============================================================================

ToolProxyInterface::ToolProxyInterface(const QString& toolPath, const QString& toolDir, QObject* parent)
    : QObject(parent)
    , m_toolPath(toolPath)
    , m_toolDir(toolDir)
    , m_process(nullptr)
    , m_server(nullptr)
    , m_socket(nullptr)
    , m_heartbeatTimer(nullptr)
    , m_heartbeatTimeoutTimer(nullptr)
    , m_infoLoaded(false)
    , m_processReady(false)
    , m_requestIdCounter(0)
{
    // Generate unique server name
    m_serverName = ToolIpc::IPC_SERVER_PREFIX + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

ToolProxyInterface::~ToolProxyInterface() {
    stopProcess();
}

void ToolProxyInterface::setMetaData(const QJsonObject& metaData) {
    m_metaData = metaData;
    m_toolInfo.id = metaData.value("id").toString();
    m_toolInfo.name = metaData.value("name").toString();
    m_toolInfo.version = metaData.value("version").toString();
    m_toolInfo.compatibleVersion = metaData.value("compatibleVersion").toString();
    m_toolInfo.author = metaData.value("author").toString();
    m_toolInfo.dependencies = ToolDescriptorParser::extractDependencies(metaData);
    m_infoLoaded = true;
}

static QMap<QString, QString> parseSimpleYamlFile(const QString& path) {
    QMap<QString, QString> result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#endif
    bool insideRoot = false;

    auto unquoteValue = [](QString value) {
        value = value.trimmed();
        if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
            value = value.mid(1, value.size() - 2);
        }
        value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
        value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        return value;
    };

    while (!stream.atEnd()) {
        const QString rawLine = stream.readLine();
        const QString trimmed = rawLine.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            continue;
        }

        if (!insideRoot) {
            if (trimmed.startsWith("l_") && trimmed.endsWith(':')) {
                insideRoot = true;
            }
            continue;
        }

        if (!rawLine.startsWith(' ') && !rawLine.startsWith('\t')) {
            continue;
        }

        const int separatorIndex = trimmed.indexOf(':');
        if (separatorIndex <= 0) {
            continue;
        }

        const QString key = trimmed.left(separatorIndex).trimmed();
        const QString value = unquoteValue(trimmed.mid(separatorIndex + 1));
        result.insert(key, value);
    }

    return result;
}

static QMap<QString, QString> parseMetaFile(const QString& path) {
    QMap<QString, QString> result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#endif
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(QStringLiteral("//"))) {
            continue;
        }

        const int equalsIndex = line.indexOf('=');
        if (equalsIndex <= 0) {
            continue;
        }

        const QString key = line.left(equalsIndex).trimmed();
        QString value = line.mid(equalsIndex + 1).trimmed();
        if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\''))) {
            value = value.mid(1, value.length() - 2);
        }
        result.insert(key, value);
    }

    return result;
}

static QString resolveToolLanguageCode(const QString& toolDir, const QString& requestedValue) {
    const QString normalized = requestedValue.trimmed();
    const QString localisationRootPath = QDir(toolDir).filePath(QStringLiteral("localisation"));
    QDir localisationRoot(localisationRootPath);

    if (!normalized.isEmpty() && localisationRoot.exists(normalized)) {
        return normalized;
    }

    const QStringList languageDirectories = localisationRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& dirName : languageDirectories) {
        const QMap<QString, QString> meta = parseMetaFile(localisationRoot.filePath(dirName + QStringLiteral("/meta.htsl")));
        if (normalized == meta.value(QStringLiteral("lang"))
            || normalized == meta.value(QStringLiteral("text"))) {
            return dirName;
        }
    }

    if (localisationRoot.exists(QStringLiteral("en_US"))) {
        return QStringLiteral("en_US");
    }
    if (!languageDirectories.isEmpty()) {
        return languageDirectories.first();
    }
    return QStringLiteral("en_US");
}

void ToolProxyInterface::preloadInfo() {
    // Load basic info from descriptor.apehts without starting process
    QString metadataPath = m_toolDir + "/descriptor.apehts";
    QJsonObject metaData;
    QString errorMessage;
    if (ToolDescriptorParser::parseDescriptorFile(metadataPath, metaData, &errorMessage)) {
        setMetaData(metaData);
    } else {
        Logger::instance().logWarning("ToolProxyInterface", errorMessage);
        return;
    }

    LocalizationManager& localizationManager = LocalizationManager::instance();
    const QString currentLang = ConfigManager::instance().getLanguage();
    const QJsonObject configJson = ConfigManager::instance().toJson();
    m_currentLanguageCode = localizationManager.resolveToolLanguageCode(m_toolDir, currentLang);
    m_localizedStrings = localizationManager.loadToolStrings(m_toolDir, currentLang);
    m_currentGameLanguageCode = configJson.value(QStringLiteral("gameLanguage")).toString();
    m_currentGameLanguageNames = configJson.value(QStringLiteral("gameLanguageNames")).toObject();

    if (m_localizedStrings.contains("Name")) {
        m_toolInfo.name = m_localizedStrings.value("Name");
    }
    if (m_localizedStrings.contains("Description")) {
        m_toolInfo.description = m_localizedStrings.value("Description");
    }

    Logger::instance().logInfo(
        "ToolProxyInterface",
        "Preloaded info for tool: " + m_toolInfo.id + " with language: " + m_currentLanguageCode
    );
}

QIcon ToolProxyInterface::icon() const {
    QString coverPath = m_toolDir + "/cover.png";
    if (QFile::exists(coverPath)) {
        return QIcon(coverPath);
    }
    return QIcon::fromTheme("application-x-executable");
}

void ToolProxyInterface::initialize() {
    // For proxy, initialization happens when process starts
    // Preload info if not already done
    if (!m_infoLoaded) {
        preloadInfo();
    }
}

bool ToolProxyInterface::startProcess() {
    // Check if process is already running and healthy
    if (m_process && m_process->state() != QProcess::NotRunning) {
        if (!m_sessionUnavailable && !m_stopping) {
            Logger::instance().logInfo("ToolProxyInterface", "Process already running, reusing existing process session");
            return true;
        }

        Logger::instance().logWarning("ToolProxyInterface", "Discarding unavailable process before starting a new session");
        discardProcess();
    }

    // Only reset state if we're actually starting a new process
    const bool needsNewProcess = !m_process || m_process->state() == QProcess::NotRunning;
    
    if (needsNewProcess) {
        m_stopping = false;
        m_processReady = false;
        m_sessionUnavailable = false;
        m_sessionUnavailableReason.clear();
        m_initialStateQueryPending = false;
        m_pendingRequests.clear();
        m_buffer.clear();
        stopHeartbeatTimers();

        if (m_socket) {
            m_socket->abort();
            m_socket->deleteLater();
            m_socket = nullptr;
        }

        if (m_server) {
            m_server->close();
            delete m_server;
            m_server = nullptr;
        }

        if (m_process) {
            delete m_process;
            m_process = nullptr;
        }
    } else {
        // Process exists but not healthy, just return false
        Logger::instance().logError("ToolProxyInterface", "Process in inconsistent state, cannot start");
        return false;
    }
    
    // Create IPC server
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::WorldAccessOption);
    
    // Remove any existing server with the same name
    QLocalServer::removeServer(m_serverName);
    
    if (!m_server->listen(m_serverName)) {
        Logger::instance().logError("ToolProxyInterface", "Failed to start IPC server: " + m_server->errorString());
        delete m_server;
        m_server = nullptr;
        return false;
    }
    
    connect(m_server, &QLocalServer::newConnection, this, &ToolProxyInterface::onNewConnection);
    
    // Start ToolHost process
    m_process = new QProcess(this);
    connect(m_process, &QProcess::started, this, &ToolProxyInterface::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &ToolProxyInterface::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &ToolProxyInterface::onProcessError);
    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        Logger::instance().logInfo("ToolHost", QString::fromUtf8(m_process->readAllStandardOutput()));
    });
    connect(m_process, &QProcess::readyReadStandardError, [this]() {
        Logger::instance().logError("ToolHost", QString::fromUtf8(m_process->readAllStandardError()));
    });
    
    // Use the standalone ToolHost executable (non-GUI process)
    QString appDir = QCoreApplication::applicationDirPath();
    QString toolHostPath = QDir(appDir).filePath("ToolHost.exe");
    
    if (!QFile::exists(toolHostPath)) {
        Logger::instance().logError("ToolProxyInterface", "ToolHost.exe not found at: " + toolHostPath);
        if (m_process) {
            m_process->deleteLater();
            m_process = nullptr;
        }
        if (m_server) {
            m_server->close();
            m_server->deleteLater();
            m_server = nullptr;
        }
        return false;
    }

    QString workerPath = m_toolPath;
    const QString descriptorToolName = m_metaData.value(QStringLiteral("name")).toString(m_toolInfo.name).trimmed();
    
    QStringList args;
    args << m_serverName << workerPath << descriptorToolName;
    
    // Pass log file path to subprocess so logs are merged
    QString logPath = Logger::instance().logFilePath();
    if (!logPath.isEmpty()) {
        args << "--log-file" << logPath;
    }
    
    m_process->start(toolHostPath, args);
    
    return true;
}

bool ToolProxyInterface::waitForProcessStopped(int timeoutMs) {
    if (!m_process) {
        return true;
    }

    if (m_process->state() == QProcess::NotRunning) {
        return true;
    }

    return m_process->waitForFinished(timeoutMs);
}

void ToolProxyInterface::stopProcess() {
    if (m_stopping) {
        return;
    }

    Logger::instance().logInfo("ToolProxyInterface", QString("Stopping process for tool: %1").arg(m_toolInfo.id));
    
    m_stopping = true;
    m_processReady = false;
    m_sessionUnavailable = false;
    m_sessionUnavailableReason.clear();
    m_pendingRequests.clear();
    m_buffer.clear();
    
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        delete m_heartbeatTimer;
        m_heartbeatTimer = nullptr;
    }
    
    if (m_heartbeatTimeoutTimer) {
        m_heartbeatTimeoutTimer->stop();
        delete m_heartbeatTimeoutTimer;
        m_heartbeatTimeoutTimer = nullptr;
    }
    
    if (m_socket && m_socket->state() == QLocalSocket::ConnectedState) {
        Logger::instance().logInfo("ToolProxyInterface", "Sending shutdown message to tool process");
        sendMessage(ToolIpc::MessageType::Shutdown);
        m_socket->flush();
        m_socket->waitForBytesWritten(50);
        m_socket->disconnectFromServer();
        m_socket->waitForDisconnected(50);
    }

    if (m_process && m_process->state() != QProcess::NotRunning) {
        Logger::instance().logInfo("ToolProxyInterface", "Waiting for graceful process exit");
        if (!waitForProcessStopped(150)) {
            Logger::instance().logInfo("ToolProxyInterface", "Graceful exit timed out, terminating process");
            m_process->terminate();
            if (!waitForProcessStopped(200)) {
                Logger::instance().logWarning("ToolProxyInterface", "Terminate timed out, force killing process");
#ifdef Q_OS_WIN
                HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, m_process->processId());
                if (hProcess) {
                    TerminateProcess(hProcess, 1);
                    WaitForSingleObject(hProcess, 500);
                    CloseHandle(hProcess);
                } else {
                    m_process->kill();
                    waitForProcessStopped(500);
                }
#else
                m_process->kill();
                waitForProcessStopped(3000);
#endif
            }
        }
    }

    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    if (m_process) {
        delete m_process;
        m_process = nullptr;
        Logger::instance().logInfo("ToolProxyInterface", "Process stopped");
    }
    
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
    
    m_stopping = false;
    emit processStopped();
}

void ToolProxyInterface::forceKillProcess() {
    if (m_stopping) {
        return;
    }

    Logger::instance().logInfo("ToolProxyInterface", QString("Force killing process for tool: %1").arg(m_toolInfo.id));
    
    m_stopping = true;
    m_processReady = false;
    m_sessionUnavailable = false;
    m_sessionUnavailableReason.clear();
    m_pendingRequests.clear();
    m_buffer.clear();
    
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        delete m_heartbeatTimer;
        m_heartbeatTimer = nullptr;
    }
    if (m_heartbeatTimeoutTimer) {
        m_heartbeatTimeoutTimer->stop();
        delete m_heartbeatTimeoutTimer;
        m_heartbeatTimeoutTimer = nullptr;
    }
    
    if (m_socket && m_socket->state() == QLocalSocket::ConnectedState) {
        sendMessage(ToolIpc::MessageType::Shutdown);
        m_socket->flush();
        m_socket->waitForBytesWritten(20);
    }
    
    if (m_process && m_process->state() != QProcess::NotRunning) {
#ifdef Q_OS_WIN
        HANDLE hProcess = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, m_process->processId());
        if (hProcess) {
            TerminateProcess(hProcess, 1);
            WaitForSingleObject(hProcess, 500);
            CloseHandle(hProcess);
        } else {
            m_process->kill();
            waitForProcessStopped(500);
        }
#else
        m_process->kill();
        waitForProcessStopped(500);
#endif
    }

    if (m_socket) {
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    
    if (m_process) {
        delete m_process;
        m_process = nullptr;
    }
    
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }

    m_stopping = false;
    emit processStopped();
}

void ToolProxyInterface::discardProcess() {
    if (m_stopping) {
        return;
    }

    Logger::instance().logInfo(
        "ToolProxyInterface",
        QString("Discarding worker process for tool: %1").arg(m_toolInfo.id)
    );

    m_stopping = true;
    m_processReady = false;
    m_sessionUnavailable = false;
    m_initialStateQueryPending = false;
    m_sessionUnavailableReason.clear();
    m_pendingRequests.clear();
    m_buffer.clear();
    stopHeartbeatTimers();

    if (m_socket) {
        if (m_socket->state() == QLocalSocket::ConnectedState) {
            sendMessage(ToolIpc::MessageType::Shutdown);
            m_socket->flush();
        }
        m_socket->disconnect(this);
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }

    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }

    QProcess* discardedProcess = m_process;
    m_process = nullptr;

    if (discardedProcess) {
        discardedProcess->disconnect();
        discardedProcess->setParent(nullptr);

        if (discardedProcess->state() != QProcess::NotRunning) {
#ifdef Q_OS_WIN
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, discardedProcess->processId());
            if (hProcess) {
                TerminateProcess(hProcess, 1);
                CloseHandle(hProcess);
            } else {
                discardedProcess->kill();
            }
#else
            discardedProcess->kill();
#endif
            QTimer::singleShot(1000, discardedProcess, [discardedProcess]() {
                if (discardedProcess->state() != QProcess::NotRunning) {
                    discardedProcess->kill();
                }
            });
            connect(
                discardedProcess,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                discardedProcess,
                &QObject::deleteLater
            );
        } else {
            discardedProcess->deleteLater();
        }
    }

    m_stopping = false;
    emit processStopped();
}

bool ToolProxyInterface::isProcessRunning() const {
    return m_process
        && m_process->state() != QProcess::NotRunning
        && !m_stopping
        && !m_sessionUnavailable;
}

// Note: createWidget and createSidebarWidget methods removed
// ToolProxyInterface now uses the new scripted UI architecture

void ToolProxyInterface::loadLanguage(const QString& lang) {
    LocalizationManager& localizationManager = LocalizationManager::instance();
    const QString resolvedLanguageCode = localizationManager.resolveToolLanguageCode(m_toolDir, lang);
    const QMap<QString, QString> localizedStrings = localizationManager.loadToolStrings(m_toolDir, lang);
    const QJsonObject configJson = ConfigManager::instance().toJson();
    const QString gameLanguage = configJson.value(QStringLiteral("gameLanguage")).toString();
    const QJsonObject gameLanguageNames = configJson.value(QStringLiteral("gameLanguageNames")).toObject();
    const bool languageDidChange = m_currentLanguageCode != resolvedLanguageCode;
    const bool stringsDidChange = m_localizedStrings != localizedStrings;
    const bool gameLanguageDidChange = m_currentGameLanguageCode != gameLanguage;
    const bool gameLanguageNamesDidChange = m_currentGameLanguageNames != gameLanguageNames;

    m_currentLanguageCode = resolvedLanguageCode;
    m_localizedStrings = localizedStrings;
    m_currentGameLanguageCode = gameLanguage;
    m_currentGameLanguageNames = gameLanguageNames;

    Logger::instance().logInfo(
        "ToolProxyInterface",
        QString("loadLanguage called for %1, lang=%2, code=%3")
            .arg(m_toolInfo.id, lang, m_currentLanguageCode)
    );

    if (!m_localizedStrings.isEmpty()) {
        if (m_localizedStrings.contains("Name")) {
            m_toolInfo.name = m_localizedStrings.value("Name");
            Logger::instance().logInfo("ToolProxyInterface",
                QString("Updated name to: %1").arg(m_toolInfo.name));
        }
        if (m_localizedStrings.contains("Description")) {
            m_toolInfo.description = m_localizedStrings.value("Description");
            Logger::instance().logInfo("ToolProxyInterface",
                QString("Updated description to: %1").arg(m_toolInfo.description));
        }
    }

    if (!languageDidChange && !stringsDidChange && !gameLanguageDidChange && !gameLanguageNamesDidChange) {
        Logger::instance().logInfo(
            "ToolProxyInterface",
            QString("Skipping duplicate language update for %1, code=%2")
                .arg(m_toolInfo.id, m_currentLanguageCode)
        );
        return;
    }

    if (isProcessRunning()) {
        QJsonObject payload;
        payload["language"] = m_currentLanguageCode;
        payload["gameLanguage"] = m_currentGameLanguageCode;
        payload["gameLanguageNames"] = m_currentGameLanguageNames;
        payload["localizedStrings"] = QJsonObject::fromVariantMap(toVariantMap(m_localizedStrings));
        Logger::instance().logInfo(
            "ToolProxyInterface",
            QString("Sending LoadLanguage to worker for %1, code=%2")
                .arg(m_toolInfo.id, m_currentLanguageCode)
        );
        sendMessage(ToolIpc::MessageType::LoadLanguage, payload);
        Logger::instance().logInfo(
            "ToolProxyInterface",
            QString("Sent LoadLanguage to worker for %1, requesting state refresh")
                .arg(m_toolInfo.id)
        );
        sendStateRequest(
            ToolIpc::MessageType::StateQuery,
            QJsonObject{{QStringLiteral("initial"), false}, {QStringLiteral("reason"), QStringLiteral("language_change")}},
            QStringLiteral("language_change")
        );
        Logger::instance().logInfo(
            "ToolProxyInterface",
            QString("Queued language_change state request for %1")
                .arg(m_toolInfo.id)
        );
    }
}

void ToolProxyInterface::applyTheme() {
    if (isProcessRunning()) {
        sendMessage(ToolIpc::MessageType::ApplyTheme);
    }
}

void ToolProxyInterface::onNewConnection() {
    m_socket = m_server->nextPendingConnection();
    if (m_socket) {
        connect(m_socket, &QLocalSocket::readyRead, this, &ToolProxyInterface::onSocketReadyRead);
        connect(m_socket, &QLocalSocket::disconnected, this, &ToolProxyInterface::onSocketDisconnected);
        Logger::instance().logInfo("ToolProxyInterface", "Tool process connected");
    }
}

void ToolProxyInterface::onSocketReadyRead() {
    processAvailableMessages();
}

void ToolProxyInterface::processAvailableMessages() {
    if (!m_socket) {
        return;
    }

    m_buffer.append(m_socket->readAll());
    
    while (m_buffer.size() >= 4) {
        quint32 msgLen;
        memcpy(&msgLen, m_buffer.constData(), sizeof(msgLen));
        
        if (m_buffer.size() < 4 + static_cast<int>(msgLen)) {
            break;
        }
        
        QByteArray msgData = m_buffer.mid(4, msgLen);
        m_buffer.remove(0, 4 + msgLen);
        
        ToolIpc::Message msg = ToolIpc::Message::deserialize(msgData);
        handleMessage(msg);
    }
}

void ToolProxyInterface::onSocketDisconnected() {
    if (m_stopping) {
        Logger::instance().logInfo("ToolProxyInterface", "Tool process socket disconnected during shutdown");
        m_socket = nullptr;
        m_processReady = false;
        return;
    }

    if (m_sessionUnavailable) {
        Logger::instance().logWarning(
            "ToolProxyInterface",
            QString("Ignoring socket disconnect after unavailability for %1").arg(m_toolInfo.id)
        );
        m_socket = nullptr;
        m_processReady = false;
        return;
    }

    const bool processStillRunning = m_process && m_process->state() != QProcess::NotRunning;
    handleSessionUnavailable(QStringLiteral("Tool process disconnected unexpectedly."), processStillRunning);
}

void ToolProxyInterface::onProcessStarted() {
}

void ToolProxyInterface::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    m_processReady = false;

    if (m_stopping) {
        Logger::instance().logInfo("ToolProxyInterface", "Process finished during shutdown");
        return;
    }

    if (m_sessionUnavailable) {
        Logger::instance().logWarning(
            "ToolProxyInterface",
            QString("Process finished after session was already marked unavailable for %1").arg(m_toolInfo.id)
        );
        return;
    }

    if (exitStatus == QProcess::CrashExit) {
        handleSessionUnavailable(QString("Tool process crashed with exit code %1").arg(exitCode));
        return;
    }

    handleSessionUnavailable(QString("Tool process exited unexpectedly with exit code %1").arg(exitCode));
}

void ToolProxyInterface::onProcessError(QProcess::ProcessError error) {
    QString errorStr;
    switch (error) {
    case QProcess::FailedToStart: errorStr = "Failed to start"; break;
    case QProcess::Crashed: errorStr = "Crashed"; break;
    case QProcess::Timedout: errorStr = "Timed out"; break;
    case QProcess::WriteError: errorStr = "Write error"; break;
    case QProcess::ReadError: errorStr = "Read error"; break;
    default: errorStr = "Unknown error"; break;
    }

    if (m_stopping) {
        Logger::instance().logInfo("ToolProxyInterface", QString("Ignoring process error during shutdown: tool=%1, error=%2").arg(m_toolInfo.name, errorStr));
        return;
    }

    Logger::instance().logError("ToolProxyInterface", QString("Process error: tool=%1, error=%2, processState=%3")
        .arg(m_toolInfo.name, errorStr)
        .arg(m_process ? QString::number(m_process->state()) : "null"));
    const bool terminateProcess = m_process && m_process->state() != QProcess::NotRunning;
    handleSessionUnavailable(QString("Process error: %1").arg(errorStr), terminateProcess);
}

void ToolProxyInterface::onHeartbeatTimeout() {
    handleSessionUnavailable(QStringLiteral("Heartbeat timeout - tool process is unresponsive."), true);
}

void ToolProxyInterface::handleMessage(const ToolIpc::Message& msg) {
    const bool isWorkerResponse =
        msg.type == ToolIpc::MessageType::ToolInfoResponse ||
        msg.type == ToolIpc::MessageType::UiActionResponse ||
        msg.type == ToolIpc::MessageType::StateQueryResponse ||
        msg.type == ToolIpc::MessageType::Error;

    if (m_heartbeatTimeoutTimer && !m_sessionUnavailable && !m_stopping) {
        m_heartbeatTimeoutTimer->start(ToolIpc::HEARTBEAT_TIMEOUT_MS);
    }

    // Only response messages may consume pending request callbacks.
    // Tool-originated host data requests share the same IPC channel and may reuse request ids.
    if (isWorkerResponse && m_pendingRequests.contains(msg.requestId)) {
        auto callback = m_pendingRequests.take(msg.requestId);
        callback(msg);
        return;
    }
    
    switch (msg.type) {
    case ToolIpc::MessageType::Ready:
        {
            markSessionAvailable();
            m_processReady = true;
            
            // Update tool info from process
            if (msg.payload.contains("toolInfo")) {
                ToolIpc::ToolInfo info = ToolIpc::ToolInfo::fromJson(msg.payload["toolInfo"].toObject());
                // Only update name/description if not already loaded
                if (m_toolInfo.name.isEmpty()) {
                    m_toolInfo.name = info.name;
                }
                if (m_toolInfo.description.isEmpty()) {
                    m_toolInfo.description = info.description;
                }
            }
            
            // Start heartbeat
            stopHeartbeatTimers();
            m_heartbeatTimer = new QTimer(this);
            connect(m_heartbeatTimer, &QTimer::timeout, [this]() {
                sendMessage(ToolIpc::MessageType::HeartbeatAck);
            });
            
            m_heartbeatTimeoutTimer = new QTimer(this);
            m_heartbeatTimeoutTimer->setSingleShot(true);
            connect(m_heartbeatTimeoutTimer, &QTimer::timeout, this, &ToolProxyInterface::onHeartbeatTimeout);
            m_heartbeatTimeoutTimer->start(ToolIpc::HEARTBEAT_TIMEOUT_MS);
            
            Logger::instance().logInfo("ToolProxyInterface", "Tool process ready: " + m_toolInfo.id);
            emit processStarted();
            if (m_initialStateQueryPending) {
                requestInitialStateAsync();
            }
        }
        break;
        
    case ToolIpc::MessageType::Heartbeat:
        // Respond to heartbeat
        sendMessage(ToolIpc::MessageType::HeartbeatAck);
        // Reset timeout timer
        if (m_heartbeatTimeoutTimer) {
            m_heartbeatTimeoutTimer->start(ToolIpc::HEARTBEAT_TIMEOUT_MS);
        }
        break;

    case ToolIpc::MessageType::StateUpdate:
        {
            const QJsonObject stateObject = msg.payload.value(QStringLiteral("state")).toObject();
            m_cachedStatePacket = parseStatePacket(stateObject);
            int rowCount = 0;
            for (const QVariant& modelValue : m_cachedStatePacket.listModels) {
                const QVariantMap modelMap = modelValue.toMap();
                if (modelMap.value(QStringLiteral("id")).toString() == QStringLiteral("file_list")) {
                    rowCount = modelMap.value(QStringLiteral("rows")).toList().size();
                    break;
                }
            }
            Logger::instance().logInfo(
                "ToolProxyInterface",
                QString("[STATE_CHAIN] Received worker state update for %1: page=%2 listModels=%3 fileListRows=%4")
                    .arg(m_toolInfo.id)
                    .arg(m_cachedStatePacket.pageId)
                    .arg(m_cachedStatePacket.listModels.size())
                    .arg(rowCount)
            );
            emit statePacketUpdated(stateObject);
        }
        break;
        
    case ToolIpc::MessageType::GetConfig:
    case ToolIpc::MessageType::GetFileIndex:
    case ToolIpc::MessageType::InvokePlugin:
    case ToolIpc::MessageType::ReadMatchingTextFiles:
    case ToolIpc::MessageType::ReadBinaryFile:
    case ToolIpc::MessageType::ReadTextFile:
    case ToolIpc::MessageType::ReadEffectiveBinaryFile:
    case ToolIpc::MessageType::ReadEffectiveTextFile:
    case ToolIpc::MessageType::ReadEffectiveTextFiles:
    case ToolIpc::MessageType::WriteBinaryFile:
    case ToolIpc::MessageType::WriteTextFile:
    case ToolIpc::MessageType::RemovePath:
    case ToolIpc::MessageType::EnsureDirectory:
    case ToolIpc::MessageType::ListDirectory:
    case ToolIpc::MessageType::ListEffectiveFiles:
        // Handle data requests from tool
        handleDataRequest(msg);
        break;
        
    case ToolIpc::MessageType::ToolInfoResponse:
    case ToolIpc::MessageType::UiActionResponse:
    case ToolIpc::MessageType::StateQueryResponse:
    case ToolIpc::MessageType::Error:
        Logger::instance().logWarning(
            "ToolProxyInterface",
            QString("Unexpected response message type: %1 (requestId=%2)")
                .arg(static_cast<int>(msg.type))
                .arg(msg.requestId)
        );
        break;

    default:
        Logger::instance().logWarning("ToolProxyInterface",
            QString("Unhandled message type: %1").arg(static_cast<int>(msg.type)));
        break;
    }
}

void ToolProxyInterface::sendMessage(ToolIpc::MessageType type, const QJsonObject& payload, quint32 requestId) {
    if (!m_socket || m_socket->state() != QLocalSocket::ConnectedState) {
        return;
    }
    
    ToolIpc::Message msg = ToolIpc::createMessage(type, requestId, payload);
    m_socket->write(msg.serialize());
    m_socket->flush();
}

void ToolProxyInterface::sendRequest(ToolIpc::MessageType type, const QJsonObject& payload, ResponseCallback callback) {
    quint32 reqId = nextRequestId();
    m_pendingRequests.insert(reqId, callback);
    sendMessage(type, payload, reqId);
}

bool ToolProxyInterface::isWorkerSessionReady() const {
    return m_processReady
        && !m_sessionUnavailable
        && m_socket
        && m_socket->state() == QLocalSocket::ConnectedState;
}

void ToolProxyInterface::stopHeartbeatTimers() {
    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
        delete m_heartbeatTimer;
        m_heartbeatTimer = nullptr;
    }

    if (m_heartbeatTimeoutTimer) {
        m_heartbeatTimeoutTimer->stop();
        delete m_heartbeatTimeoutTimer;
        m_heartbeatTimeoutTimer = nullptr;
    }
}

void ToolProxyInterface::clearPendingRequests() {
    m_pendingRequests.clear();
}

void ToolProxyInterface::markSessionAvailable() {
    m_sessionUnavailable = false;
    m_sessionUnavailableReason.clear();
}

void ToolProxyInterface::setCachedLifecycleState(const QString& status, const QString& message, bool notify) {
    ToolUiStatePacket packet;
    packet.pageId = QStringLiteral("tool_lifecycle");
    packet.modeId = status;
    packet.viewState.insert(QStringLiteral("lifecycleStatus"), status);
    packet.viewState.insert(QStringLiteral("statusText"), message);
    packet.viewState.insert(QStringLiteral("loadingActive"), status == QStringLiteral("starting"));
    packet.viewState.insert(QStringLiteral("loadingText"), message);
    packet.viewState.insert(QStringLiteral("lastError"), status == QStringLiteral("error") ? message : QString());
    packet.runtimeVariables.insert(QStringLiteral("workerStatus"), status);
    packet.topbarState.insert(QStringLiteral("visible"), false);
    packet.sidebarState.insert(QStringLiteral("visible"), false);
    m_cachedStatePacket = packet;

    if (notify) {
        emit statePacketUpdated(statePacketToJsonObject(m_cachedStatePacket));
    }
}

void ToolProxyInterface::handleSessionUnavailable(const QString& reason, bool terminateProcess) {
    if (m_stopping) {
        return;
    }

    const QString effectiveReason = reason.trimmed().isEmpty()
        ? QStringLiteral("Worker session is unavailable.")
        : reason.trimmed();
    const bool firstNotification = !m_sessionUnavailable;

    if (m_sessionUnavailableReason.isEmpty()) {
        m_sessionUnavailableReason = effectiveReason;
    }
    m_sessionUnavailable = true;
    m_processReady = false;

    Logger::instance().logError(
        "ToolProxyInterface",
        QString("Worker session unavailable for %1: %2").arg(m_toolInfo.id, m_sessionUnavailableReason)
    );

    clearPendingRequests();
    stopHeartbeatTimers();
    m_initialStateQueryPending = false;
    setCachedLifecycleState(QStringLiteral("error"), m_sessionUnavailableReason, true);

    if (m_socket) {
        if (m_socket->state() != QLocalSocket::UnconnectedState) {
            m_socket->abort();
        }
        m_socket = nullptr;
    }

    if (terminateProcess && m_process && m_process->state() != QProcess::NotRunning) {
        Logger::instance().logWarning(
            "ToolProxyInterface",
            QString("Terminating unavailable worker process for %1").arg(m_toolInfo.id)
        );
        m_process->terminate();
        QTimer::singleShot(250, this, [this]() {
            if (m_process && m_process->state() != QProcess::NotRunning && m_sessionUnavailable && !m_stopping) {
                Logger::instance().logWarning(
                    "ToolProxyInterface",
                    QString("Force killing unavailable worker process for %1").arg(m_toolInfo.id)
                );
                m_process->kill();
            }
        });
    }

    if (firstNotification) {
        emit processCrashed(m_sessionUnavailableReason);
    }
}

void ToolProxyInterface::requestInitialStateAsync() {
    if (!isWorkerSessionReady()) {
        m_initialStateQueryPending = true;
        return;
    }

    m_initialStateQueryPending = false;
    sendStateRequest(
        ToolIpc::MessageType::StateQuery,
        QJsonObject{{QStringLiteral("initial"), true}},
        QStringLiteral("initial_state")
    );
}

bool ToolProxyInterface::sendStateRequest(ToolIpc::MessageType type,
                                          const QJsonObject& payload,
                                          const QString& reason) {
    if (!isWorkerSessionReady()) {
        if (!m_sessionUnavailable) {
            Logger::instance().logWarning(
                "ToolProxyInterface",
                QString("Cannot send worker state request for %1 because the session is not ready.")
                    .arg(m_toolInfo.id)
            );
        }
        return false;
    }

    const quint32 requestId = nextRequestId();
    const QString requestReason = reason.trimmed().isEmpty()
        ? QStringLiteral("state_request")
        : reason.trimmed();

    m_pendingRequests.insert(requestId, [this, type, requestId, requestReason](const ToolIpc::Message& response) {
        const bool success = response.payload.value(QStringLiteral("success")).toBool(false);
        const QString errorText = response.payload.value(QStringLiteral("error")).toString();
        const QJsonObject stateObject = response.payload.value(QStringLiteral("state")).toObject();

        if (!success || stateObject.isEmpty()) {
            Logger::instance().logWarning(
                "ToolProxyInterface",
                QString("Worker async state request failed for %1 (type=%2, requestId=%3, reason=%4): %5")
                    .arg(m_toolInfo.id)
                    .arg(static_cast<int>(type))
                    .arg(requestId)
                    .arg(requestReason, errorText)
            );
            return;
        }

        m_cachedStatePacket = parseStatePacket(stateObject);
        Logger::instance().logInfo(
            "ToolProxyInterface",
            QString("[STATE_CHAIN] Worker async state request succeeded for %1 (type=%2, requestId=%3, reason=%4): page=%5 listModels=%6")
                .arg(m_toolInfo.id)
                .arg(static_cast<int>(type))
                .arg(requestId)
                .arg(requestReason)
                .arg(m_cachedStatePacket.pageId)
                .arg(m_cachedStatePacket.listModels.size())
        );
        emit statePacketUpdated(stateObject);
    });

    Logger::instance().logInfo(
        "ToolProxyInterface",
        QString("Sending worker state request for %1 (type=%2, requestId=%3, reason=%4)")
            .arg(m_toolInfo.id)
            .arg(static_cast<int>(type))
            .arg(requestId)
            .arg(requestReason)
    );
    sendMessage(type, payload, requestId);

    const bool isUiStateRequest =
        type == ToolIpc::MessageType::StateQuery || type == ToolIpc::MessageType::UiAction;
    const int timeoutMs = isUiStateRequest ? 120000 : ToolIpc::PROCESS_START_TIMEOUT_MS;
    QTimer::singleShot(timeoutMs, this, [this, requestId, type, requestReason]() {
        if (!m_pendingRequests.contains(requestId)) {
            return;
        }

        m_pendingRequests.remove(requestId);
        if (m_stopping || m_sessionUnavailable) {
            return;
        }

        handleSessionUnavailable(
            QString("Timed out while waiting for worker response (type=%1, requestId=%2, reason=%3).")
                .arg(static_cast<int>(type))
                .arg(requestId)
                .arg(requestReason),
            true
        );
    });

    return true;
}

ToolUiStatePacket ToolProxyInterface::parseStatePacket(const QJsonObject& jsonObject) {
    ToolUiStatePacket packet;

    if (jsonObject.contains("pageId")) {
        const QVariantMap variantMap = QJsonDocument(jsonObject).toVariant().toMap();
        packet.pageId = variantMap.value("pageId").toString();
        packet.modeId = variantMap.value("modeId").toString();
        packet.viewState = variantMap.value("viewState").toMap();
        packet.sidebarState = variantMap.value("sidebarState").toMap();
        packet.topbarState = variantMap.value("topbarState").toMap();
        packet.runtimeVariables = variantMap.value("runtimeVariables").toMap();
        packet.listModels = variantMap.value("listModels").toList();
        packet.patches = variantMap.value("patches").toList();
        return packet;
    }

    packet.pageId = QStringLiteral("error_log");
    packet.modeId = QStringLiteral("default");

    packet.topbarState.insert("visible", true);
    packet.topbarState.insert("currentPageId", QStringLiteral("error_log"));
    packet.topbarState.insert("pageOrder", QStringList{QStringLiteral("error_log")});
    packet.topbarState.insert(
        "activeFunction",
        jsonObject.value("sortMode").toInt() == 1 ? QStringLiteral("error_log::sort_category")
                                                  : QStringLiteral("error_log::sort_time")
    );

    packet.sidebarState.insert("visible", true);
    packet.sidebarState.insert("title", QStringLiteral("Log Files"));
    packet.sidebarState.insert("activeMode", QStringLiteral("default"));
    packet.sidebarState.insert("modeOrder", QStringList{QStringLiteral("default")});
    packet.sidebarState.insert("searchEnabled", false);
    packet.sidebarState.insert("selectAllEnabled", false);

    packet.viewState.insert("currentFile", jsonObject.value("currentFile").toString());
    packet.viewState.insert("compareFile", jsonObject.value("compareFile").toString());
    packet.viewState.insert("searchText", jsonObject.value("searchText").toString());
    packet.viewState.insert("sortMode", jsonObject.value("sortMode").toInt());
    packet.viewState.insert("isCompareMode", jsonObject.value("isCompareMode").toBool());
    packet.viewState.insert("hasCurrentFile", jsonObject.value("hasCurrentFile").toBool());

    auto buildPreviewText = [](QString text) {
        text.replace("\r\n", "\n");
        text.replace('\r', '\n');
        text = text.simplified();
        if (text.length() > 180) {
            text = text.left(180) + QStringLiteral("...");
        }
        return text;
    };

    auto formatClipboardEntry = [](const QJsonObject& entryObject) {
        return QStringLiteral("[%1][%2][%3]: %4")
            .arg(entryObject.value("systemTime").toString(),
                 entryObject.value("gameTime").toString(),
                 entryObject.value("category").toString(),
                 entryObject.value("message").toString());
    };

    auto displayNameForFile = [](const QJsonObject& fileObject) {
        if (fileObject.value("isLatest").toBool()) {
            return QStringLiteral("Latest");
        }
        return fileObject.value("displayName").toString();
    };

    QVariantMap sidebarModel;
    sidebarModel.insert("id", QStringLiteral("logManager.files"));
    sidebarModel.insert("title", QStringLiteral("Log Files"));
    sidebarModel.insert("headerHidden", true);
    sidebarModel.insert("listSearch", true);
    sidebarModel.insert("selectAll", false);

    QVariantList sidebarColumns;
    QVariantMap sidebarColumn;
    sidebarColumn.insert("id", QStringLiteral("text"));
    sidebarColumn.insert("title", QStringLiteral("Log Files"));
    sidebarColumn.insert("stretch", 1);
    sidebarColumns.append(sidebarColumn);
    sidebarModel.insert("columns", sidebarColumns);

    QVariantList sidebarRows;
    const QJsonArray files = jsonObject.value("files").toArray();
    for (const QJsonValue& value : files) {
        const QJsonObject fileObject = value.toObject();
        QVariantMap row;
        row.insert("rowId", fileObject.value("displayName").toString());

        QVariantMap rowValues;
        rowValues.insert("text", displayNameForFile(fileObject));
        row.insert("values", rowValues);

        const QString displayName = fileObject.value("displayName").toString();
        const bool isCurrent = displayName == jsonObject.value("currentFile").toString();
        const bool isCompare = displayName == jsonObject.value("compareFile").toString();

        QVariantMap rowState;
        rowState.insert("isLatest", fileObject.value("isLatest").toBool());
        rowState.insert("isLoaded", fileObject.value("isLoaded").toBool());
        rowState.insert("isCurrent", isCurrent);
        rowState.insert("isCompare", isCompare);
        rowState.insert("selected", isCurrent);
        rowState.insert("canCompare", !isCurrent && !isCompare);
        rowState.insert("canStopCompare", isCompare);
        row.insert("state", rowState);
        sidebarRows.append(row);
    }
    sidebarModel.insert("rows", sidebarRows);
    packet.listModels.append(sidebarModel);

    QVariantMap mainModel;
    mainModel.insert("id", QStringLiteral("log_entries_list"));
    mainModel.insert("title", jsonObject.value("isCompareMode").toBool() ? QStringLiteral("Compare Mode") : QStringLiteral("Error Log"));
    mainModel.insert("headerHidden", false);
    mainModel.insert("listSearch", false);
    mainModel.insert("selectAll", false);

    QVariantList mainColumns;
    if (jsonObject.value("isCompareMode").toBool()) {
        QString currentDisplayName = jsonObject.value("currentFile").toString();
        QString compareDisplayName = jsonObject.value("compareFile").toString();
        for (const QJsonValue& fileValue : files) {
            const QJsonObject fileObject = fileValue.toObject();
            const QString displayName = fileObject.value("displayName").toString();
            if (displayName == currentDisplayName) {
                currentDisplayName = displayNameForFile(fileObject);
            }
            if (displayName == compareDisplayName) {
                compareDisplayName = displayNameForFile(fileObject);
            }
        }

        packet.viewState.insert("log_entries_title", QStringLiteral("Compare Mode: %1 ↔ %2").arg(currentDisplayName, compareDisplayName));

        QVariantMap leftColumn;
        leftColumn.insert("id", QStringLiteral("left"));
        leftColumn.insert("title", currentDisplayName);
        leftColumn.insert("stretch", 2);
        mainColumns.append(leftColumn);

        QVariantMap rightColumn;
        rightColumn.insert("id", QStringLiteral("right"));
        rightColumn.insert("title", compareDisplayName);
        rightColumn.insert("stretch", 2);
        mainColumns.append(rightColumn);

        QVariantMap categoryColumn;
        categoryColumn.insert("id", QStringLiteral("category"));
        categoryColumn.insert("title", QStringLiteral("Category"));
        categoryColumn.insert("stretch", 1);
        mainColumns.append(categoryColumn);

        QVariantList rows;
        const QJsonArray compareRows = jsonObject.value("compareRows").toArray();
        for (const QJsonValue& value : compareRows) {
            const QJsonObject rowObject = value.toObject();
            const QJsonObject leftEntry = rowObject.value("leftEntry").toObject();
            const QJsonObject rightEntry = rowObject.value("rightEntry").toObject();

            auto buildCompareCell = [&](const QJsonObject& entryObject, bool hasEntry) {
                if (!hasEntry) {
                    return QString();
                }
                return buildPreviewText(
                    entryObject.value("systemTime").toString() + QStringLiteral("\n") +
                    entryObject.value("gameTime").toString() + QStringLiteral("\n") +
                    entryObject.value("message").toString()
                );
            };

            QVariantMap row;
            row.insert("rowId", rowObject.value("normalizedKey").toString());

            QVariantMap values;
            values.insert("left", buildCompareCell(leftEntry, rowObject.value("hasLeft").toBool()));
            values.insert("right", buildCompareCell(rightEntry, rowObject.value("hasRight").toBool()));
            values.insert("category", rowObject.value("category").toString());
            row.insert("values", values);

            QVariantMap state;
            state.insert("isHighPriority", rowObject.value("isHighPriority").toBool());
            state.insert("hasLeft", rowObject.value("hasLeft").toBool());
            state.insert("hasRight", rowObject.value("hasRight").toBool());
            state.insert("leftMissing", !rowObject.value("hasLeft").toBool());
            state.insert("rightMissing", !rowObject.value("hasRight").toBool());
            state.insert("leftTooltip", formatClipboardEntry(leftEntry));
            state.insert("rightTooltip", formatClipboardEntry(rightEntry));
            row.insert("state", state);
            rows.append(row);
        }
        mainModel.insert("rows", rows);
    } else {
        packet.viewState.insert("log_entries_title", QStringLiteral("Error Log"));

        const auto addColumn = [&](const QString& id, const QString& title, int stretch) {
            QVariantMap column;
            column.insert("id", id);
            column.insert("title", title);
            column.insert("stretch", stretch);
            mainColumns.append(column);
        };

        addColumn(QStringLiteral("systemTime"), QStringLiteral("Time"), 1);
        addColumn(QStringLiteral("gameTime"), QStringLiteral("Date"), 1);
        addColumn(QStringLiteral("category"), QStringLiteral("Category"), 1);
        addColumn(QStringLiteral("message"), QStringLiteral("Message Preview"), 3);

        QVariantList rows;
        const QJsonArray entries = jsonObject.value("entries").toArray();
        for (const QJsonValue& value : entries) {
            const QJsonObject entryObject = value.toObject();
            QVariantMap row;
            row.insert("rowId", entryObject.value("normalizedKey").toString());

            QVariantMap values;
            values.insert("systemTime", entryObject.value("systemTime").toString());
            values.insert("gameTime", entryObject.value("gameTime").toString());
            values.insert("category", entryObject.value("category").toString());
            values.insert("message", buildPreviewText(entryObject.value("message").toString()));
            row.insert("values", values);

            QVariantMap state;
            state.insert("isHighPriority", entryObject.value("isHighPriority").toBool());
            state.insert("tooltip", formatClipboardEntry(entryObject));
            row.insert("state", state);
            rows.append(row);
        }
        mainModel.insert("rows", rows);
    }

    mainModel.insert("columns", mainColumns);
    packet.listModels.append(mainModel);
    return packet;
}

void ToolProxyInterface::handleDataRequest(const ToolIpc::Message& msg) {
    QJsonObject payload;
    
    switch (msg.type) {
    case ToolIpc::MessageType::GetConfig:
        payload = ConfigManager::instance().toJson();
        sendMessage(ToolIpc::MessageType::ConfigResponse, payload, msg.requestId);
        Logger::instance().logInfo("ToolProxyInterface", "Sent config data to tool process");
        break;
        
    case ToolIpc::MessageType::GetFileIndex:
        payload = FileManager::instance().toJson();
        sendMessage(ToolIpc::MessageType::FileIndexResponse, payload, msg.requestId);
        Logger::instance().logInfo("ToolProxyInterface", "Sent file index data to tool process");
        break;

    case ToolIpc::MessageType::InvokePlugin:
        {
            const QString pluginName = msg.payload.value("pluginName").toString().trimmed();
            const QString operation = msg.payload.value("operation").toString().trimmed();
            const quint32 contentType = static_cast<quint32>(msg.payload.value("contentType").toInt());
            const quint32 flags = static_cast<quint32>(msg.payload.value("flags").toInt());
            const QByteArray payloadBytes = QByteArray::fromBase64(msg.payload.value("payloadBase64").toString().toLatin1());

            payload["pluginName"] = pluginName;
            payload["operation"] = operation;

            PluginAbiBroker::Request brokerRequest;
            brokerRequest.pluginName = pluginName;
            brokerRequest.operation = operation;
            brokerRequest.contentType = static_cast<PluginAbiBroker::ContentType>(contentType);
            brokerRequest.payload = payloadBytes;
            brokerRequest.flags = flags;
            brokerRequest.authorizedDependencies = m_toolInfo.dependencies;

            const PluginAbiBroker::Response brokerResponse = PluginAbiBroker::instance().invoke(brokerRequest);
            payload["success"] = brokerResponse.success;
            payload["status"] = static_cast<int>(brokerResponse.status);
            payload["contentType"] = static_cast<int>(brokerResponse.contentType);
            payload["flags"] = static_cast<int>(brokerResponse.flags);
            payload["payloadBase64"] = QString::fromLatin1(brokerResponse.payload.toBase64());
            payload["error"] = brokerResponse.errorMessage;
            sendMessage(ToolIpc::MessageType::InvokePluginResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::ReadMatchingTextFiles:
        {
            const ToolRuntimeContext::FileRoot root = parseFileRootFromPayload(msg.payload);
            const QString relativePath = msg.payload.value("relativePath").toString();
            const QString regexPattern = msg.payload.value("regexPattern").toString();
            const bool recursive = msg.payload.value("recursive").toBool(false);
            payload["root"] = ToolRuntimeContext::fileRootToString(root);
            payload["relativePath"] = relativePath;
            payload["regexPattern"] = regexPattern;
            payload["recursive"] = recursive;

            const ToolRuntimeContext::MatchingTextFilesResult result =
                ToolRuntimeContext::instance().readMatchingTextFiles(root, relativePath, regexPattern, recursive);
            payload["success"] = result.success;
            if (result.success) {
                payload["entries"] = makeMatchingTextFileEntriesJson(result.entries);
            } else {
                payload["error"] = result.errorMessage;
            }

            sendMessage(ToolIpc::MessageType::ReadMatchingTextFilesResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::ReadBinaryFile:
        {
            const ToolRuntimeContext::FileRoot root = parseFileRootFromPayload(msg.payload);
            const QString relativePath = msg.payload.value("relativePath").toString();
            payload["root"] = ToolRuntimeContext::fileRootToString(root);
            payload["relativePath"] = relativePath;

            const ToolRuntimeContext::FileReadResult result =
                ToolRuntimeContext::instance().readFile(root, relativePath);
            const QJsonObject resultPayload = makeFileReadResponsePayload(result);
            for (auto it = resultPayload.begin(); it != resultPayload.end(); ++it) {
                payload[it.key()] = it.value();
            }

            sendMessage(ToolIpc::MessageType::ReadBinaryFileResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::ReadTextFile:
        {
            const ToolRuntimeContext::FileRoot root = parseFileRootFromPayload(msg.payload);
            const QString relativePath = msg.payload.value("relativePath").toString();
            payload["root"] = ToolRuntimeContext::fileRootToString(root);
            payload["relativePath"] = relativePath;

            const ToolRuntimeContext::TextReadResult result =
                ToolRuntimeContext::instance().readTextFile(root, relativePath);
            const QJsonObject resultPayload = makeTextReadResponsePayload(result);
            for (auto it = resultPayload.begin(); it != resultPayload.end(); ++it) {
                payload[it.key()] = it.value();
            }

            sendMessage(ToolIpc::MessageType::ReadTextFileResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::ReadEffectiveBinaryFile:
        {
            const QString relativePath = msg.payload.value("relativePath").toString();
            payload["relativePath"] = relativePath;

            const ToolRuntimeContext::FileReadResult result =
                ToolRuntimeContext::instance().readEffectiveFile(relativePath);
            const QJsonObject resultPayload = makeFileReadResponsePayload(result);
            for (auto it = resultPayload.begin(); it != resultPayload.end(); ++it) {
                payload[it.key()] = it.value();
            }

            sendMessage(ToolIpc::MessageType::ReadEffectiveBinaryFileResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::ReadEffectiveTextFile:
        {
            const QString relativePath = msg.payload.value("relativePath").toString();
            payload["relativePath"] = relativePath;

            const ToolRuntimeContext::TextReadResult result =
                ToolRuntimeContext::instance().readEffectiveTextFile(relativePath);
            const QJsonObject resultPayload = makeTextReadResponsePayload(result);
            for (auto it = resultPayload.begin(); it != resultPayload.end(); ++it) {
                payload[it.key()] = it.value();
            }

            sendMessage(ToolIpc::MessageType::ReadEffectiveTextFileResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::ReadEffectiveTextFiles:
        {
            const QString relativeRoot = msg.payload.value(QStringLiteral("relativeRoot")).toString();
            const QString suffixFilter = msg.payload.value(QStringLiteral("suffixFilter")).toString();
            payload["relativeRoot"] = relativeRoot;
            payload["suffixFilter"] = suffixFilter;

            const ToolRuntimeContext::MatchingTextFilesResult result =
                ToolRuntimeContext::instance().readEffectiveTextFiles(relativeRoot, suffixFilter);
            payload["success"] = result.success;
            if (result.success) {
                payload["entries"] = makeMatchingTextFileEntriesJson(result.entries);
            } else {
                payload["error"] = result.errorMessage;
            }

            sendMessage(ToolIpc::MessageType::ReadEffectiveTextFilesResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::WriteBinaryFile:
        {
            const ToolRuntimeContext::FileRoot root = parseFileRootFromPayload(msg.payload);
            const QString relativePath = msg.payload.value("relativePath").toString();
            const QByteArray content =
                QByteArray::fromBase64(msg.payload.value("contentBase64").toString().toLatin1());

            payload["root"] = ToolRuntimeContext::fileRootToString(root);
            payload["relativePath"] = relativePath;

            const ToolRuntimeContext::FileWriteResult result =
                ToolRuntimeContext::instance().writeFile(root, relativePath, content);
            const QJsonObject resultPayload = makeWriteResponsePayload(result);
            for (auto it = resultPayload.begin(); it != resultPayload.end(); ++it) {
                payload[it.key()] = it.value();
            }

            sendMessage(ToolIpc::MessageType::WriteBinaryFileResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::WriteTextFile:
        {
            const ToolRuntimeContext::FileRoot root = parseFileRootFromPayload(msg.payload);
            const QString relativePath = msg.payload.value("relativePath").toString();
            const QString content = msg.payload.value("content").toString();

            payload["root"] = ToolRuntimeContext::fileRootToString(root);
            payload["relativePath"] = relativePath;

            const ToolRuntimeContext::FileWriteResult result =
                ToolRuntimeContext::instance().writeTextFile(root, relativePath, content);
            const QJsonObject resultPayload = makeWriteResponsePayload(result);
            for (auto it = resultPayload.begin(); it != resultPayload.end(); ++it) {
                payload[it.key()] = it.value();
            }

            sendMessage(ToolIpc::MessageType::WriteTextFileResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::RemovePath:
        {
            const ToolRuntimeContext::FileRoot root = parseFileRootFromPayload(msg.payload);
            const QString relativePath = msg.payload.value("relativePath").toString();

            payload["root"] = ToolRuntimeContext::fileRootToString(root);
            payload["relativePath"] = relativePath;

            const ToolRuntimeContext::FileWriteResult result =
                ToolRuntimeContext::instance().removePath(root, relativePath);
            const QJsonObject resultPayload = makeWriteResponsePayload(result);
            for (auto it = resultPayload.begin(); it != resultPayload.end(); ++it) {
                payload[it.key()] = it.value();
            }

            sendMessage(ToolIpc::MessageType::RemovePathResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::EnsureDirectory:
        {
            const ToolRuntimeContext::FileRoot root = parseFileRootFromPayload(msg.payload);
            const QString relativePath = msg.payload.value("relativePath").toString();

            payload["root"] = ToolRuntimeContext::fileRootToString(root);
            payload["relativePath"] = relativePath;

            const ToolRuntimeContext::FileWriteResult result =
                ToolRuntimeContext::instance().ensureDirectory(root, relativePath);
            const QJsonObject resultPayload = makeWriteResponsePayload(result);
            for (auto it = resultPayload.begin(); it != resultPayload.end(); ++it) {
                payload[it.key()] = it.value();
            }

            sendMessage(ToolIpc::MessageType::EnsureDirectoryResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::ListDirectory:
        {
            const ToolRuntimeContext::FileRoot root = parseFileRootFromPayload(msg.payload);
            const QString relativePath = msg.payload.value("relativePath").toString();
            const bool recursive = msg.payload.value("recursive").toBool(false);

            payload["root"] = ToolRuntimeContext::fileRootToString(root);
            payload["relativePath"] = relativePath;
            payload["recursive"] = recursive;

            const ToolRuntimeContext::DirectoryListResult result =
                ToolRuntimeContext::instance().listDirectory(root, relativePath, recursive);
            payload["success"] = result.success;
            if (result.success) {
                payload["entries"] = makeDirectoryEntriesJson(result.entries);
            } else {
                payload["error"] = result.errorMessage;
            }

            sendMessage(ToolIpc::MessageType::ListDirectoryResponse, payload, msg.requestId);
        }
        break;

    case ToolIpc::MessageType::ListEffectiveFiles:
        {
            const QString relativeRoot = msg.payload.value(QStringLiteral("relativeRoot")).toString();
            const QString suffixFilter = msg.payload.value(QStringLiteral("suffixFilter")).toString();
            const ToolRuntimeContext::EffectiveFileListResult result =
                ToolRuntimeContext::instance().listEffectiveFiles(relativeRoot, suffixFilter);
            payload["success"] = result.success;
            if (result.success) {
                payload["entries"] = makeEffectiveFileEntriesJson(result.entries);
            } else {
                payload["error"] = result.errorMessage;
            }

            sendMessage(ToolIpc::MessageType::ListEffectiveFilesResponse, payload, msg.requestId);
        }
        break;
        
    default:
        break;
    }
}

// ============================================================================
// Scripted UI Resources (New Architecture)
// ============================================================================

ToolGuiResourceDescriptor ToolProxyInterface::guiResourceDescriptor() const {
    ToolGuiResourceDescriptor descriptor;
    descriptor.presetFile = m_metaData.value("guiPreset").toString();
    return descriptor;
}

ToolWorkerDescriptor ToolProxyInterface::workerDescriptor() const {
    ToolWorkerDescriptor descriptor;
    descriptor.workerId = m_metaData.value("workerId").toString();
    if (descriptor.workerId.isEmpty()) {
        descriptor.workerId = m_toolInfo.id;
    }
    return descriptor;
}

// ============================================================================
// Worker Session Lifecycle (New Architecture)
// ============================================================================

void ToolProxyInterface::initializeWorkerSession() {
    if (!m_infoLoaded) {
        preloadInfo();
    }

    Logger::instance().logInfo(
        "ToolProxyInterface",
        QString("Initializing worker session for %1").arg(m_toolInfo.id)
    );

    setCachedLifecycleState(
        QStringLiteral("starting"),
        QStringLiteral("Starting worker process..."),
        true
    );

    if (!isProcessRunning() && !startProcess()) {
        handleSessionUnavailable(QStringLiteral("Failed to start worker process."));
        return;
    }

    m_initialStateQueryPending = true;
    if (isWorkerSessionReady()) {
        requestInitialStateAsync();
    }
}

ToolUiStatePacket ToolProxyInterface::initialUiState() const {
    return m_cachedStatePacket;
}

ToolUiStatePacket ToolProxyInterface::handleUiAction(const ToolUiActionRequest& request) {
    QJsonObject payload;
    payload["actionType"] = request.actionType;
    payload["targetId"] = request.targetId;
    payload["arguments"] = QJsonObject::fromVariantMap(request.arguments);

    if (!sendStateRequest(
        ToolIpc::MessageType::UiAction,
        payload,
        QStringLiteral("ui_action")
    )) {
        Logger::instance().logWarning("ToolProxyInterface", "Worker UI action was not sent because the session is unavailable.");
    }

    return m_cachedStatePacket;
}
