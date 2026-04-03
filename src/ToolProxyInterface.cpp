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
#include "PluginManager.h"
#include "ToolRuntimeContext.h"
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QUuid>
#include <QDir>
#include <QElapsedTimer>
#include <QShowEvent>

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
} // namespace

// ============================================================================
// ToolEmbedContainer Implementation
// ============================================================================

ToolEmbedContainer::ToolEmbedContainer(QWidget* parent)
    : QWidget(parent)
    , m_foreignWindow(nullptr)
    , m_container(nullptr)
#ifdef Q_OS_WIN
    , m_childHwnd(nullptr)
#endif
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Make container transparent so embedded window is visible
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAutoFillBackground(false);
}

ToolEmbedContainer::~ToolEmbedContainer() {
    releaseWindow();
}

bool ToolEmbedContainer::embedWindow(WId windowId) {
    releaseWindow();
    
#ifdef Q_OS_WIN
    // Use Windows API to embed the window
    HWND childHwnd = reinterpret_cast<HWND>(windowId);
    HWND parentHwnd = reinterpret_cast<HWND>(this->winId());
    
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("[Embed] Start - childHwnd: %1, parentHwnd: %2").arg((qulonglong)childHwnd).arg((qulonglong)parentHwnd));
    
    if (!IsWindow(childHwnd)) {
        Logger::instance().logError("ToolEmbedContainer", "[Embed] Invalid child window handle");
        return false;
    }
    
    // Check initial state of child window
    BOOL initialVisible = IsWindowVisible(childHwnd);
    RECT initialRect;
    GetWindowRect(childHwnd, &initialRect);
    LONG initialStyle = GetWindowLong(childHwnd, GWL_STYLE);
    LONG initialExStyle = GetWindowLong(childHwnd, GWL_EXSTYLE);
    
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("[Embed] Child initial state - Visible: %1, Rect: %2,%3,%4,%5, Style: %6, ExStyle: %7")
        .arg(initialVisible)
        .arg(initialRect.left).arg(initialRect.top).arg(initialRect.right).arg(initialRect.bottom)
        .arg(initialStyle, 0, 16).arg(initialExStyle, 0, 16));
    
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("[Embed] Container size: %1x%2, visible: %3").arg(width()).arg(height()).arg(isVisible()));
    
    // Store the window ID for later use FIRST
    m_foreignWindow = QWindow::fromWinId(windowId);
    m_childHwnd = childHwnd;
    
    // Get container size - use actual size or default if not yet sized
    // IMPORTANT: Container may not be sized yet when first created, use reasonable defaults
    int w = (width() > 100) ? width() : 800;
    int h = (height() > 100) ? height() : 600;
    
    // CRITICAL: First modify window styles to make it a child window BEFORE SetParent
    // This prevents the window from appearing briefly as a top-level window
    Logger::instance().logInfo("ToolEmbedContainer", "[Embed] Pre-modifying window styles...");
    LONG style = GetWindowLong(childHwnd, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_OVERLAPPEDWINDOW);
    style |= WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;  // Don't add WS_VISIBLE yet
    SetWindowLong(childHwnd, GWL_STYLE, style);
    
    // Remove extended styles that cause issues
    LONG exStyle = GetWindowLong(childHwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_APPWINDOW | WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE);
    SetWindowLong(childHwnd, GWL_EXSTYLE, exStyle);
    
    // Set parent AFTER changing styles
    Logger::instance().logInfo("ToolEmbedContainer", "[Embed] Calling SetParent...");
    HWND oldParent = SetParent(childHwnd, parentHwnd);
    DWORD setParentError = GetLastError();
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("[Embed] SetParent result - oldParent: %1, error: %2").arg((qulonglong)oldParent).arg(setParentError));
    
    if (oldParent == NULL && setParentError != 0) {
        Logger::instance().logError("ToolEmbedContainer", 
            QString("[Embed] SetParent failed with error: %1").arg(setParentError));
        return false;
    }
    
    // Now add WS_VISIBLE style after SetParent
    style = GetWindowLong(childHwnd, GWL_STYLE);
    style |= WS_VISIBLE;
    SetWindowLong(childHwnd, GWL_STYLE, style);
    
    LONG newStyle = GetWindowLong(childHwnd, GWL_STYLE);
    LONG newExStyle = GetWindowLong(childHwnd, GWL_EXSTYLE);
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("[Embed] After style change - Style: %1, ExStyle: %2").arg(newStyle, 0, 16).arg(newExStyle, 0, 16));
    
    // Position and show the window
    Logger::instance().logInfo("ToolEmbedContainer", QString("[Embed] Calling SetWindowPos with size %1x%2").arg(w).arg(h));
    BOOL posResult = SetWindowPos(childHwnd, HWND_TOP, 0, 0, w, h, 
                 SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    Logger::instance().logInfo("ToolEmbedContainer", QString("[Embed] SetWindowPos result: %1").arg(posResult));
    
    // Force multiple redraws to ensure content is visible
    ShowWindow(childHwnd, SW_SHOW);
    InvalidateRect(childHwnd, NULL, TRUE);
    UpdateWindow(childHwnd);
    RedrawWindow(childHwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    
    // Also invalidate the parent container
    InvalidateRect(parentHwnd, NULL, TRUE);
    UpdateWindow(parentHwnd);
    
    // Check final state
    BOOL finalVisible = IsWindowVisible(childHwnd);
    RECT finalRect;
    GetWindowRect(childHwnd, &finalRect);
    RECT clientRect;
    GetClientRect(childHwnd, &clientRect);
    
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("[Embed] Final state - Visible: %1, WindowRect: %2,%3,%4,%5, ClientRect: %6,%7,%8,%9")
        .arg(finalVisible)
        .arg(finalRect.left).arg(finalRect.top).arg(finalRect.right).arg(finalRect.bottom)
        .arg(clientRect.left).arg(clientRect.top).arg(clientRect.right).arg(clientRect.bottom));
    
    Logger::instance().logInfo("ToolEmbedContainer", "[Embed] Complete");
    return true;
#else
    // Fallback for non-Windows platforms
    m_foreignWindow = QWindow::fromWinId(windowId);
    if (!m_foreignWindow) {
        Logger::instance().logError("ToolEmbedContainer", "Failed to create QWindow from WinId");
        return false;
    }
    
    m_container = QWidget::createWindowContainer(m_foreignWindow, this);
    if (!m_container) {
        Logger::instance().logError("ToolEmbedContainer", "Failed to create window container");
        m_foreignWindow = nullptr;
        return false;
    }
    
    layout()->addWidget(m_container);
    m_container->show();
    
    Logger::instance().logInfo("ToolEmbedContainer", QString("Embedded window %1").arg(windowId));
    return true;
#endif
}

void ToolEmbedContainer::releaseWindow() {
#ifdef Q_OS_WIN
    if (m_foreignWindow) {
        HWND childHwnd = reinterpret_cast<HWND>(m_foreignWindow->winId());
        if (IsWindow(childHwnd)) {
            // Hide the window first to prevent it from appearing on desktop
            ShowWindow(childHwnd, SW_HIDE);
            // Don't call SetParent(NULL) - let the process termination handle cleanup
            // This prevents the window from appearing at screen corner
        }
    }
#endif
    if (m_container) {
        layout()->removeWidget(m_container);
        delete m_container;
        m_container = nullptr;
    }
    m_foreignWindow = nullptr;
}

void ToolEmbedContainer::setPendingWindowId(WId windowId) {
    m_pendingWindowId = windowId;
    m_embedded = false;
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("setPendingWindowId: %1").arg(windowId));
}

void ToolEmbedContainer::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("showEvent - firstShow: %1, pendingWindowId: %2, embedded: %3, size: %4x%5")
        .arg(m_firstShow).arg(m_pendingWindowId).arg(m_embedded).arg(width()).arg(height()));
    
    // On first show, if we have a pending window ID, embed it
    if (m_firstShow && m_pendingWindowId != 0 && !m_embedded) {
        m_firstShow = false;
        // Use a longer delay to ensure the container is fully laid out and has valid size
        // Also process events to ensure layout is complete
        QTimer::singleShot(100, this, [this]() {
            // Process events to ensure layout is complete
            QCoreApplication::processEvents();
            
            // Check if we have a valid size, if not, wait a bit more
            if (width() < 100 || height() < 100) {
                Logger::instance().logInfo("ToolEmbedContainer", 
                    QString("Container size too small (%1x%2), waiting...").arg(width()).arg(height()));
                QTimer::singleShot(100, this, &ToolEmbedContainer::doEmbed);
            } else {
                doEmbed();
            }
        });
    }
}

void ToolEmbedContainer::doEmbed() {
    if (m_pendingWindowId == 0 || m_embedded) {
        return;
    }
    
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("doEmbed - windowId: %1, container size: %2x%3")
        .arg(m_pendingWindowId).arg(width()).arg(height()));
    
    bool success = embedWindow(m_pendingWindowId);
    m_embedded = success;
    
    if (success) {
        Logger::instance().logInfo("ToolEmbedContainer", "doEmbed succeeded");
        
        // Schedule another refresh after a short delay
        QTimer::singleShot(100, [this]() {
#ifdef Q_OS_WIN
            if (m_childHwnd) {
                HWND childHwnd = reinterpret_cast<HWND>(m_childHwnd);
                if (IsWindow(childHwnd)) {
                    int w = width();
                    int h = height();
                    Logger::instance().logInfo("ToolEmbedContainer", 
                        QString("Post-embed refresh: %1x%2").arg(w).arg(h));
                    ShowWindow(childHwnd, SW_SHOW);
                    SetWindowPos(childHwnd, HWND_TOP, 0, 0, w, h, 
                                 SWP_SHOWWINDOW | SWP_FRAMECHANGED);
                    InvalidateRect(childHwnd, NULL, TRUE);
                    UpdateWindow(childHwnd);
                    RedrawWindow(childHwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                }
            }
#endif
            update();
        });
    } else {
        Logger::instance().logError("ToolEmbedContainer", "doEmbed failed");
    }
    
    emit embeddingComplete(success);
}

void ToolEmbedContainer::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    
    Logger::instance().logInfo("ToolEmbedContainer", 
        QString("resizeEvent - size: %1x%2, embedded: %3")
        .arg(width()).arg(height()).arg(m_embedded));
    
#ifdef Q_OS_WIN
    if (m_childHwnd && m_embedded) {
        HWND childHwnd = reinterpret_cast<HWND>(m_childHwnd);
        if (IsWindow(childHwnd)) {
            SetWindowPos(childHwnd, HWND_TOP, 0, 0, width(), height(), SWP_NOZORDER | SWP_SHOWWINDOW);
            // Force repaint
            InvalidateRect(childHwnd, NULL, TRUE);
            UpdateWindow(childHwnd);
        }
    }
#else
    if (m_container) {
        m_container->resize(size());
    }
#endif
    
    // Emit signal to notify parent that resize happened
    emit resized(width(), height());
}

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
    m_toolInfo.id = metaData.value("id").toString();
    m_toolInfo.version = metaData.value("version").toString();
    m_toolInfo.compatibleVersion = metaData.value("compatibleVersion").toString();
    m_toolInfo.author = metaData.value("author").toString();
    m_toolInfo.dependencies = ToolDescriptorParser::extractDependencies(metaData);
    m_infoLoaded = true;
}

// Helper function to convert display language name to language code
static QString languageNameToCode(const QString& langName) {
    if (langName == "简体中文") return "zh_CN";
    if (langName == "繁體中文") return "zh_TW";
    if (langName == "English") return "en_US";
    // If already a code, return as-is
    if (langName == "zh_CN" || langName == "zh_TW" || langName == "en_US") return langName;
    return "en_US"; // Default fallback
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

    // Try to load localized name/description - use current language
    QString currentLang = ConfigManager::instance().getLanguage();
    QString langCode = languageNameToCode(currentLang);
    QString locPath = m_toolDir + "/localization/" + langCode + ".json";
    QFile locFile(locPath);

    // Fallback to en_US if current language file doesn't exist
    if (!locFile.exists()) {
        locPath = m_toolDir + "/localization/en_US.json";
        locFile.setFileName(locPath);
    }

    if (locFile.open(QIODevice::ReadOnly)) {
        QJsonDocument locDoc = QJsonDocument::fromJson(locFile.readAll());
        QJsonObject locObj = locDoc.object();
        if (locObj.contains("Name")) {
            m_toolInfo.name = locObj["Name"].toString();
        }
        if (locObj.contains("Description")) {
            m_toolInfo.description = locObj["Description"].toString();
        }
        locFile.close();
    }

    Logger::instance().logInfo("ToolProxyInterface", "Preloaded info for tool: " + m_toolInfo.id + " with language: " + langCode);
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
    if (m_process && m_process->state() != QProcess::NotRunning) {
        Logger::instance().logWarning("ToolProxyInterface", "Process already running");
        return true;
    }

    m_stopping = false;
    m_processReady = false;
    m_pendingRequests.clear();
    m_buffer.clear();

    if (m_socket) {
        m_socket->abort();
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
    
    // Use the main application itself as the tool host (with --tool-host argument)
    QString appPath = QCoreApplication::applicationFilePath();
    
    QStringList args;
    args << "--tool-host" << m_serverName << m_toolPath << m_toolInfo.name;
    
    // Pass log file path to subprocess so logs are merged
    QString logPath = Logger::instance().logFilePath();
    if (!logPath.isEmpty()) {
        args << "--log-file" << logPath;
    }
    
    Logger::instance().logInfo("ToolProxyInterface", QString("Starting tool subprocess: %1 %2").arg(appPath, args.join(" ")));
    m_process->start(appPath, args);
    
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
    
    if (m_mainContainer) {
        Logger::instance().logInfo("ToolProxyInterface", "Releasing main container window");
        m_mainContainer->releaseWindow();
    }
    if (m_sidebarContainer) {
        Logger::instance().logInfo("ToolProxyInterface", "Releasing sidebar container window");
        m_sidebarContainer->releaseWindow();
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
    
    if (m_mainContainer) {
        m_mainContainer->releaseWindow();
    }
    if (m_sidebarContainer) {
        m_sidebarContainer->releaseWindow();
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

bool ToolProxyInterface::isProcessRunning() const {
    return m_process && m_process->state() == QProcess::Running && m_processReady && !m_stopping;
}

QWidget* ToolProxyInterface::createWidget(QWidget* parent) {
    Logger::instance().logInfo("ToolProxyInterface", "createWidget called");
    
    if (!isProcessRunning()) {
        Logger::instance().logInfo("ToolProxyInterface", "Process not running, starting...");
        if (!startProcess()) {
            Logger::instance().logError("ToolProxyInterface", "Failed to start tool process");
            return nullptr;
        }
        
        // Wait for process to be ready with shorter intervals
        int waitCount = 0;
        const int maxWait = 100; // 100 * 50ms = 5 seconds max
        while (!m_processReady && waitCount < maxWait) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(40);
            waitCount++;
        }
        
        if (!m_processReady) {
            Logger::instance().logError("ToolProxyInterface", "Tool process did not become ready in time");
            forceKillProcess();
            return nullptr;
        }
        Logger::instance().logInfo("ToolProxyInterface", "Process is ready");
    }
    
    // Create container widget with a reasonable initial size
    // DO NOT call show() here - let the layout system handle it
    m_mainContainer = new ToolEmbedContainer(parent);
    m_mainContainer->setAttribute(Qt::WA_NativeWindow);
    m_mainContainer->setMinimumSize(400, 300);  // Set minimum size
    m_mainContainer->winId(); // Force native window creation
    
    Logger::instance().logInfo("ToolProxyInterface", 
        QString("Container created, size: %1x%2")
        .arg(m_mainContainer->width()).arg(m_mainContainer->height()));
    
    // Request widget creation from subprocess
    bool success = false;
    WId windowId = 0;
    bool responseReceived = false;
    
    sendRequest(ToolIpc::MessageType::CreateWidget, QJsonObject(), 
        [&](const ToolIpc::Message& response) {
            if (response.payload["success"].toBool()) {
                ToolIpc::WindowHandle wh = ToolIpc::WindowHandle::fromJson(response.payload["window"].toObject());
                windowId = static_cast<WId>(wh.handle);
                success = true;
                Logger::instance().logInfo("ToolProxyInterface", 
                    QString("Received window handle: %1").arg(windowId));
            } else {
                Logger::instance().logError("ToolProxyInterface", 
                    QString("CreateWidget failed: %1").arg(response.payload["error"].toString()));
            }
            responseReceived = true;
        });
    
    // Wait for response with shorter intervals
    int waitCount = 0;
    const int maxWait = 200; // 200 * 25ms = 5 seconds max
    while (!responseReceived && waitCount < maxWait) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(20);
        waitCount++;
    }
    
    Logger::instance().logInfo("ToolProxyInterface", 
        QString("Wait finished: success=%1, windowId=%2").arg(success).arg(windowId));
    
    if (success && windowId != 0) {
        // Store the window ID for delayed embedding (will be embedded in showEvent)
        m_pendingWindowId = windowId;
        m_mainContainer->setPendingWindowId(windowId);
        
        // DO NOT send ShowWidget here - wait until embedding is complete
        // Connect to resize signal to notify subprocess
        connect(m_mainContainer, &ToolEmbedContainer::resized, 
                [this](int w, int h) {
            if (isProcessRunning()) {
                QJsonObject payload;
                payload["width"] = w;
                payload["height"] = h;
                payload["main"] = true;
                sendMessage(ToolIpc::MessageType::ResizeWidget, payload);
            }
        });
        
        // Connect to embedding complete signal
        connect(m_mainContainer, &ToolEmbedContainer::embeddingComplete, 
                [this, windowId](bool embedSuccess) {
            if (embedSuccess) {
                Logger::instance().logInfo("ToolProxyInterface", "Embedding complete, now sending ShowWidget");
                
                // NOW tell the tool to show its widget (after embedding is done)
                QJsonObject showPayload;
                showPayload["main"] = true;
                showPayload["sidebar"] = false;
                sendMessage(ToolIpc::MessageType::ShowWidget, showPayload);
                
                // Schedule a delayed refresh to ensure window is visible after layout is complete
                QTimer::singleShot(200, [this, windowId]() {
                    if (m_mainContainer && m_mainContainer->isEmbedded()) {
#ifdef Q_OS_WIN
                        HWND childHwnd = reinterpret_cast<HWND>(windowId);
                        if (IsWindow(childHwnd)) {
                            int w = m_mainContainer->width();
                            int h = m_mainContainer->height();
                            Logger::instance().logInfo("ToolProxyInterface", 
                                QString("Final refresh: resizing to %1x%2").arg(w).arg(h));
                            ShowWindow(childHwnd, SW_SHOW);
                            SetWindowPos(childHwnd, HWND_TOP, 0, 0, w, h, 
                                         SWP_SHOWWINDOW | SWP_FRAMECHANGED);
                            InvalidateRect(childHwnd, NULL, TRUE);
                            UpdateWindow(childHwnd);
                            RedrawWindow(childHwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                            
                            // Check visibility
                            BOOL isVisible = IsWindowVisible(childHwnd);
                            Logger::instance().logInfo("ToolProxyInterface", 
                                QString("After final refresh - IsVisible: %1").arg(isVisible));
                        }
#endif
                        m_mainContainer->update();
                    }
                });
            }
        });
        
        Logger::instance().logInfo("ToolProxyInterface", "Container ready for delayed embedding");
        return m_mainContainer;
    }
    
    Logger::instance().logError("ToolProxyInterface", "Failed to create tool widget");
    delete m_mainContainer;
    m_mainContainer = nullptr;
    return nullptr;
}

QWidget* ToolProxyInterface::createSidebarWidget(QWidget* parent) {
    if (!isProcessRunning()) {
        return nullptr;
    }
    
    // Create container widget
    m_sidebarContainer = new ToolEmbedContainer(parent);
    m_sidebarContainer->setAttribute(Qt::WA_NativeWindow);
    m_sidebarContainer->winId();
    
    // Request sidebar widget creation from subprocess
    bool success = false;
    WId windowId = 0;
    bool responseReceived = false;
    
    sendRequest(ToolIpc::MessageType::CreateSidebarWidget, QJsonObject(), 
        [&](const ToolIpc::Message& response) {
            if (response.payload["success"].toBool()) {
                ToolIpc::WindowHandle wh = ToolIpc::WindowHandle::fromJson(response.payload["window"].toObject());
                windowId = static_cast<WId>(wh.handle);
                success = true;
            }
            responseReceived = true;
        });
    
    // Wait for response with processEvents to keep UI responsive
    QElapsedTimer timer;
    timer.start();
    while (!responseReceived && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    
    if (success && windowId != 0) {
        QThread::msleep(50);
        if (m_sidebarContainer->embedWindow(windowId)) {
            // Now tell the tool to show its sidebar widget
            QJsonObject showPayload;
            showPayload["main"] = false;
            showPayload["sidebar"] = true;
            sendMessage(ToolIpc::MessageType::ShowWidget, showPayload);
            
            return m_sidebarContainer;
        }
    }
    
    // No sidebar or failed - this is okay, not all tools have sidebars
    delete m_sidebarContainer;
    m_sidebarContainer = nullptr;
    return nullptr;
}

void ToolProxyInterface::loadLanguage(const QString& lang) {
    // Convert display name to language code
    QString langCode = languageNameToCode(lang);
    
    // Always update local cached info first
    QString locPath = m_toolDir + "/localization/" + langCode + ".json";
    QFile locFile(locPath);
    
    Logger::instance().logInfo("ToolProxyInterface", 
        QString("loadLanguage called for %1, lang=%2, code=%3, path=%4").arg(m_toolInfo.id, lang, langCode, locPath));
    
    if (locFile.open(QIODevice::ReadOnly)) {
        QJsonDocument locDoc = QJsonDocument::fromJson(locFile.readAll());
        QJsonObject locObj = locDoc.object();
        if (locObj.contains("Name")) {
            m_toolInfo.name = locObj["Name"].toString();
            Logger::instance().logInfo("ToolProxyInterface", 
                QString("Updated name to: %1").arg(m_toolInfo.name));
        }
        if (locObj.contains("Description")) {
            m_toolInfo.description = locObj["Description"].toString();
            Logger::instance().logInfo("ToolProxyInterface", 
                QString("Updated description to: %1").arg(m_toolInfo.description));
        }
        locFile.close();
    } else {
        Logger::instance().logWarning("ToolProxyInterface", 
            QString("Failed to open localization file: %1").arg(locPath));
    }
    
    // Send to subprocess if running
    if (isProcessRunning()) {
        QJsonObject payload;
        payload["language"] = lang;
        sendMessage(ToolIpc::MessageType::LoadLanguage, payload);
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

    Logger::instance().logWarning("ToolProxyInterface", "Tool process disconnected");
    m_socket = nullptr;
    m_processReady = false;
    
    // Check if process crashed
    if (m_process && m_process->state() == QProcess::NotRunning) {
        emit processCrashed("Tool process disconnected unexpectedly");
    }
}

void ToolProxyInterface::onProcessStarted() {
    Logger::instance().logInfo("ToolProxyInterface", "Tool process started");
}

void ToolProxyInterface::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Logger::instance().logInfo("ToolProxyInterface", 
        QString("Tool process finished with code %1, status %2").arg(exitCode).arg(exitStatus));
    
    m_processReady = false;

    if (m_stopping) {
        Logger::instance().logInfo("ToolProxyInterface", "Process finished during shutdown");
        return;
    }
    
    if (exitStatus == QProcess::CrashExit) {
        emit processCrashed(QString("Tool process crashed with exit code %1").arg(exitCode));
    } else {
        emit processStopped();
    }
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
        Logger::instance().logInfo("ToolProxyInterface", "Ignoring process error during shutdown: " + errorStr);
        return;
    }
    
    Logger::instance().logError("ToolProxyInterface", "Process error: " + errorStr);
    emit processCrashed(errorStr);
}

void ToolProxyInterface::onHeartbeatTimeout() {
    Logger::instance().logWarning("ToolProxyInterface", "Heartbeat timeout - tool process may be unresponsive");
}

void ToolProxyInterface::handleMessage(const ToolIpc::Message& msg) {
    // Check if this is a response to a pending request
    if (m_pendingRequests.contains(msg.requestId)) {
        auto callback = m_pendingRequests.take(msg.requestId);
        callback(msg);
        return;
    }
    
    switch (msg.type) {
    case ToolIpc::MessageType::Ready:
        {
            // Tool process is ready
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
            m_heartbeatTimer = new QTimer(this);
            connect(m_heartbeatTimer, &QTimer::timeout, [this]() {
                sendMessage(ToolIpc::MessageType::HeartbeatAck);
            });
            
            m_heartbeatTimeoutTimer = new QTimer(this);
            m_heartbeatTimeoutTimer->setSingleShot(true);
            connect(m_heartbeatTimeoutTimer, &QTimer::timeout, this, &ToolProxyInterface::onHeartbeatTimeout);
            
            Logger::instance().logInfo("ToolProxyInterface", "Tool process ready: " + m_toolInfo.id);
            emit processStarted();
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
        
    case ToolIpc::MessageType::GetConfig:
    case ToolIpc::MessageType::GetFileIndex:
    case ToolIpc::MessageType::GetPluginBinaryPath:
    case ToolIpc::MessageType::ReadBinaryFile:
    case ToolIpc::MessageType::ReadTextFile:
    case ToolIpc::MessageType::ReadEffectiveBinaryFile:
    case ToolIpc::MessageType::ReadEffectiveTextFile:
    case ToolIpc::MessageType::WriteBinaryFile:
    case ToolIpc::MessageType::WriteTextFile:
    case ToolIpc::MessageType::RemovePath:
    case ToolIpc::MessageType::EnsureDirectory:
    case ToolIpc::MessageType::ListDirectory:
        // Handle data requests from tool
        handleDataRequest(msg);
        break;
        
    default:
        Logger::instance().logWarning("ToolProxyInterface", 
            QString("Unhandled message type: %1").arg(static_cast<int>(msg.type)));
        break;
    }
}

bool ToolProxyInterface::isPluginDependencyAuthorized(const QString& pluginName) const {
    return m_toolInfo.dependencies.contains(pluginName, Qt::CaseSensitive);
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

    case ToolIpc::MessageType::GetPluginBinaryPath:
        {
            const QString pluginName = msg.payload.value("pluginName").toString().trimmed();
            payload["pluginName"] = pluginName;

            if (pluginName.isEmpty()) {
                payload["success"] = false;
                payload["error"] = "Plugin name is empty.";
                sendMessage(ToolIpc::MessageType::PluginBinaryPathResponse, payload, msg.requestId);
                Logger::instance().logWarning("ToolProxyInterface", "Rejected plugin binary path request with empty plugin name");
                break;
            }

            if (!isPluginDependencyAuthorized(pluginName)) {
                payload["success"] = false;
                payload["error"] = QString("Plugin %1 is not declared in tool dependencies.").arg(pluginName);
                sendMessage(ToolIpc::MessageType::PluginBinaryPathResponse, payload, msg.requestId);
                Logger::instance().logWarning(
                    "ToolProxyInterface",
                    QString("Rejected unauthorized plugin request from tool %1 for plugin %2").arg(m_toolInfo.id, pluginName)
                );
                break;
            }

            QString libraryPath;
            QString errorMessage;
            if (!PluginManager::instance().getPluginBinaryPath(pluginName, &libraryPath, &errorMessage)) {
                payload["success"] = false;
                payload["error"] = errorMessage;
                sendMessage(ToolIpc::MessageType::PluginBinaryPathResponse, payload, msg.requestId);
                Logger::instance().logWarning(
                    "ToolProxyInterface",
                    QString("Failed authorized plugin request from tool %1 for plugin %2: %3")
                        .arg(m_toolInfo.id, pluginName, errorMessage)
                );
                break;
            }

            payload["success"] = true;
            payload["libraryPath"] = libraryPath;
            sendMessage(ToolIpc::MessageType::PluginBinaryPathResponse, payload, msg.requestId);
            Logger::instance().logInfo(
                "ToolProxyInterface",
                QString("Granted plugin binary path to tool %1 for plugin %2").arg(m_toolInfo.id, pluginName)
            );
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
        
    default:
        break;
    }
}