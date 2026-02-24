#include "ToolHostMode.h"
#include "ToolInterface.h"
#include "ToolIpcProtocol.h"
#include "FileManager.h"
#include "ConfigManager.h"
#include "TagManager.h"
#include "Logger.h"

#include <QApplication>
#include <QLocalSocket>
#include <QPluginLoader>
#include <QDir>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QEventLoop>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class ToolHostApp : public QObject {
    Q_OBJECT
public:
    ToolHostApp(const QString& serverName, const QString& toolPath, QObject* parent = nullptr)
        : QObject(parent)
        , m_serverName(serverName)
        , m_toolPath(toolPath)
        , m_tool(nullptr)
        , m_mainWidget(nullptr)
        , m_sidebarWidget(nullptr)
        , m_requestId(0)
        , m_dataReady(false)
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
        QPluginLoader loader(m_toolPath);
        QObject* plugin = loader.instance();
        
        if (!plugin) {
            qCritical() << "Failed to load plugin:" << loader.errorString();
            return false;
        }
        
        m_tool = qobject_cast<ToolInterface*>(plugin);
        if (!m_tool) {
            qCritical() << "Plugin does not implement ToolInterface";
            delete plugin;
            return false;
        }
        
        // Load metadata
        QFileInfo fi(m_toolPath);
        QString metadataPath = fi.absolutePath() + "/metadata.json";
        QFile metaFile(metadataPath);
        if (metaFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
            m_tool->setMetaData(doc.object());
        }
        
        m_tool->initialize();
        qDebug() << "Tool loaded:" << m_tool->id();
        return true;
    }
    
    void connectToServer() {
        qDebug() << "Connecting to server:" << m_serverName;
        m_socket->connectToServer(m_serverName);
    }

private slots:
    void onConnected() {
        qDebug() << "Connected to main process";
        m_heartbeatTimer->start(ToolIpc::HEARTBEAT_INTERVAL_MS);
        
        // Send ready signal with tool info FIRST (don't block)
        ToolIpc::ToolInfo info;
        info.id = m_tool->id();
        info.name = m_tool->name();
        info.description = m_tool->description();
        info.version = m_tool->version();
        info.compatibleVersion = m_tool->compatibleVersion();
        info.author = m_tool->author();
        
        QJsonObject payload;
        payload["toolInfo"] = info.toJson();
        
        sendMessage(ToolIpc::MessageType::Ready, payload);
        
        // Request data asynchronously (non-blocking)
        requestInitialDataAsync();
    }
    
    void onDisconnected() {
        qDebug() << "Disconnected from main process, exiting...";
        m_heartbeatTimer->stop();
        QApplication::quit();
    }
    
    void onError(QLocalSocket::LocalSocketError error) {
        qCritical() << "Socket error:" << error << m_socket->errorString();
        if (error == QLocalSocket::ServerNotFoundError || 
            error == QLocalSocket::ConnectionRefusedError) {
            QTimer::singleShot(1000, this, &ToolHostApp::connectToServer);
        }
    }
    
    void onReadyRead() {
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
            
        case ToolIpc::MessageType::CreateWidget:
            handleCreateWidget(msg);
            break;
            
        case ToolIpc::MessageType::CreateSidebarWidget:
            handleCreateSidebarWidget(msg);
            break;
            
        case ToolIpc::MessageType::DestroyWidget:
            handleDestroyWidget(msg);
            break;
            
        case ToolIpc::MessageType::ShowWidget:
            handleShowWidget(msg);
            break;
            
        case ToolIpc::MessageType::ResizeWidget:
            handleResizeWidget(msg);
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
            
        case ToolIpc::MessageType::Shutdown:
            qDebug() << "Shutdown requested";
            handleShutdown();
            break;
            
        case ToolIpc::MessageType::ConfigResponse:
        case ToolIpc::MessageType::FileIndexResponse:
        case ToolIpc::MessageType::TagsResponse:
            handleDataResponse(msg);
            break;
            
        default:
            qWarning() << "Unknown message type:" << static_cast<int>(msg.type);
            break;
        }
    }
    
    void handleCreateWidget(const ToolIpc::Message& msg) {
        Logger::instance().logInfo("ToolHost", "handleCreateWidget called");
        
        if (m_mainWidget) {
            Logger::instance().logInfo("ToolHost", "Deleting existing widget");
            delete m_mainWidget;
            m_mainWidget = nullptr;
        }
        
        // Wait for data to be ready before creating widget
        if (!m_dataReady) {
            Logger::instance().logInfo("ToolHost", "Waiting for data before creating widget...");
            int waitCount = 0;
            const int maxWait = 100;
            while (!m_dataReady && waitCount < maxWait) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(40);
                waitCount++;
            }
            if (m_dataReady) {
                Logger::instance().logInfo("ToolHost", "Data ready, creating widget");
            } else {
                Logger::instance().logWarning("ToolHost", "Data not ready after timeout, creating widget anyway");
            }
        }
        
        Logger::instance().logInfo("ToolHost", "Calling m_tool->createWidget(nullptr)");
        m_mainWidget = m_tool->createWidget(nullptr);
        
        if (m_mainWidget) {
            Logger::instance().logInfo("ToolHost", "Widget created successfully, setting attributes");
            m_mainWidget->setAttribute(Qt::WA_NativeWindow);
            m_mainWidget->setAttribute(Qt::WA_DontShowOnScreen, true); // Prevent showing on screen initially
            
            if (m_mainWidget->width() == 0 || m_mainWidget->height() == 0) {
                Logger::instance().logInfo("ToolHost", "Widget has zero size, resizing to 800x600");
                m_mainWidget->resize(800, 600);
            }
            
            // Get winId first to create the native window
            WId wid = m_mainWidget->winId();
            Logger::instance().logInfo("ToolHost", QString("Got WinId: %1").arg(wid));
            
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(wid);
            Logger::instance().logInfo("ToolHost", QString("HWND: %1, IsWindow: %2").arg((quintptr)hwnd).arg(IsWindow(hwnd)));
            
            // Get current styles
            LONG style = GetWindowLong(hwnd, GWL_STYLE);
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            Logger::instance().logInfo("ToolHost", QString("Before - Style: 0x%1, ExStyle: 0x%2").arg(style, 0, 16).arg(exStyle, 0, 16));
            
            // Set window styles BEFORE showing - prevent taskbar appearance and make it invisible initially
            exStyle |= WS_EX_TOOLWINDOW;
            exStyle &= ~WS_EX_APPWINDOW;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
            
            // Hide the window initially - it will be shown after embedding
            ShowWindow(hwnd, SW_HIDE);
            
            LONG newExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            Logger::instance().logInfo("ToolHost", QString("After - ExStyle: 0x%1").arg(newExStyle, 0, 16));
#endif
            
            // DO NOT call show() here - the window will be shown after embedding by the main process
            // Just create the native window handle
            Logger::instance().logInfo("ToolHost", "Widget prepared (not shown yet, waiting for embedding)");
            
            Logger::instance().logInfo("ToolHost", QString("Qt Size: %1x%2")
                .arg(m_mainWidget->width()).arg(m_mainWidget->height()));
            
            ToolIpc::WindowHandle wh;
            wh.handle = static_cast<qint64>(wid);
            wh.width = m_mainWidget->width();
            wh.height = m_mainWidget->height();
            
            QJsonObject payload;
            payload["window"] = wh.toJson();
            payload["success"] = true;
            
            Logger::instance().logInfo("ToolHost", QString("Sending CreateWidgetResponse with handle: %1").arg(wid));
            sendMessage(ToolIpc::MessageType::CreateWidgetResponse, payload, msg.requestId);
        } else {
            Logger::instance().logError("ToolHost", "Widget creation FAILED");
            QJsonObject payload;
            payload["success"] = false;
            payload["error"] = "Failed to create widget";
            sendMessage(ToolIpc::MessageType::CreateWidgetResponse, payload, msg.requestId);
        }
    }
    
    void handleCreateSidebarWidget(const ToolIpc::Message& msg) {
        if (m_sidebarWidget) {
            delete m_sidebarWidget;
        }
        
        m_sidebarWidget = m_tool->createSidebarWidget(nullptr);
        
        if (m_sidebarWidget) {
            m_sidebarWidget->setAttribute(Qt::WA_NativeWindow);
            
            if (m_sidebarWidget->width() == 0 || m_sidebarWidget->height() == 0) {
                m_sidebarWidget->resize(300, 600);
            }
            
            // CRITICAL: Show the widget BEFORE getting winId and returning handle
            m_sidebarWidget->show();
            QCoreApplication::processEvents();
            
            WId wid = m_sidebarWidget->winId();
            
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(wid);
            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
            exStyle |= WS_EX_TOOLWINDOW;
            exStyle &= ~WS_EX_APPWINDOW;
            SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
#endif
            
            ToolIpc::WindowHandle wh;
            wh.handle = static_cast<qint64>(wid);
            wh.width = m_sidebarWidget->width();
            wh.height = m_sidebarWidget->height();
            
            QJsonObject payload;
            payload["window"] = wh.toJson();
            payload["success"] = true;
            
            qDebug() << "Created sidebar widget with WinId:" << wid;
            
            sendMessage(ToolIpc::MessageType::CreateSidebarWidgetResponse, payload, msg.requestId);
        } else {
            QJsonObject payload;
            payload["success"] = false;
            payload["hasSidebar"] = false;
            sendMessage(ToolIpc::MessageType::CreateSidebarWidgetResponse, payload, msg.requestId);
        }
    }
    
    void handleDestroyWidget(const ToolIpc::Message& msg) {
        Q_UNUSED(msg);
        if (m_mainWidget) {
            delete m_mainWidget;
            m_mainWidget = nullptr;
        }
        if (m_sidebarWidget) {
            delete m_sidebarWidget;
            m_sidebarWidget = nullptr;
        }
    }
    
    void handleShowWidget(const ToolIpc::Message& msg) {
        bool showMain = msg.payload.contains("main") ? msg.payload["main"].toBool() : true;
        bool showSidebar = msg.payload.contains("sidebar") ? msg.payload["sidebar"].toBool() : false;
        
        Logger::instance().logInfo("ToolHost", QString("handleShowWidget - main: %1, sidebar: %2").arg(showMain).arg(showSidebar));
        
        if (showMain && m_mainWidget) {
            // Remove the DontShowOnScreen attribute now that we're ready to show
            m_mainWidget->setAttribute(Qt::WA_DontShowOnScreen, false);
            
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
            if (IsWindow(hwnd)) {
                // Check if the window has been embedded (has a parent)
                HWND parent = GetParent(hwnd);
                Logger::instance().logInfo("ToolHost", QString("Main widget parent hwnd: %1").arg((quintptr)parent));
                
                if (parent != NULL) {
                    // Window is embedded - need to show it AND invalidate
                    // The parent process has already set WS_VISIBLE style, but we need to ensure it's shown
                    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                    InvalidateRect(hwnd, NULL, TRUE);
                    UpdateWindow(hwnd);
                    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                } else {
                    // Window not yet embedded - show it
                    ShowWindow(hwnd, SW_SHOW);
                    InvalidateRect(hwnd, NULL, TRUE);
                    UpdateWindow(hwnd);
                }
                
                BOOL isVisible = IsWindowVisible(hwnd);
                RECT rect;
                GetWindowRect(hwnd, &rect);
                Logger::instance().logInfo("ToolHost", QString("Main widget state - hwnd: %1, visible: %2, rect: %3,%4,%5,%6")
                    .arg((quintptr)hwnd).arg(isVisible)
                    .arg(rect.left).arg(rect.top).arg(rect.right).arg(rect.bottom));
            }
#endif
            // CRITICAL: Must call Qt show() to update Qt's internal state
            // Even though the window is embedded, Qt needs to know it's visible
            m_mainWidget->show();
            m_mainWidget->update();
            m_mainWidget->repaint();
            QCoreApplication::processEvents();
            
            Logger::instance().logInfo("ToolHost", QString("Main widget Qt state - visible: %1, size: %2x%3")
                .arg(m_mainWidget->isVisible()).arg(m_mainWidget->width()).arg(m_mainWidget->height()));
        }
        if (showSidebar && m_sidebarWidget) {
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(m_sidebarWidget->winId());
            if (IsWindow(hwnd)) {
                HWND parent = GetParent(hwnd);
                if (parent != NULL) {
                    InvalidateRect(hwnd, NULL, TRUE);
                    UpdateWindow(hwnd);
                } else {
                    ShowWindow(hwnd, SW_SHOW);
                    InvalidateRect(hwnd, NULL, TRUE);
                    UpdateWindow(hwnd);
                }
            }
#endif
            m_sidebarWidget->update();
            Logger::instance().logInfo("ToolHost", "Sidebar widget updated");
        }
        
        QJsonObject payload;
        payload["success"] = true;
        sendMessage(ToolIpc::MessageType::ShowWidgetResponse, payload, msg.requestId);
    }
    
    void handleResizeWidget(const ToolIpc::Message& msg) {
        int width = msg.payload["width"].toInt();
        int height = msg.payload["height"].toInt();
        bool isMain = msg.payload.contains("main") ? msg.payload["main"].toBool() : true;
        
        Logger::instance().logInfo("ToolHost", QString("handleResizeWidget - %1x%2, main: %3").arg(width).arg(height).arg(isMain));
        
        QWidget* widget = isMain ? m_mainWidget : m_sidebarWidget;
        
        if (widget && width > 0 && height > 0) {
            // Resize the Qt widget
            widget->resize(width, height);
            
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(widget->winId());
            if (IsWindow(hwnd)) {
                // Also resize via Windows API to ensure consistency
                SetWindowPos(hwnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                InvalidateRect(hwnd, NULL, TRUE);
                UpdateWindow(hwnd);
            }
#endif
            
            widget->update();
            widget->repaint();
            QCoreApplication::processEvents();
            
            Logger::instance().logInfo("ToolHost", QString("Widget resized to %1x%2").arg(widget->width()).arg(widget->height()));
        }
    }
    
    void handleLoadLanguage(const ToolIpc::Message& msg) {
        QString lang = msg.payload["language"].toString();
        m_tool->loadLanguage(lang);
    }
    
    void handleApplyTheme(const ToolIpc::Message& msg) {
        Q_UNUSED(msg);
        m_tool->applyTheme();
    }
    
    void handleGetToolInfo(const ToolIpc::Message& msg) {
        ToolIpc::ToolInfo info;
        info.id = m_tool->id();
        info.name = m_tool->name();
        info.description = m_tool->description();
        info.version = m_tool->version();
        info.compatibleVersion = m_tool->compatibleVersion();
        info.author = m_tool->author();
        
        QJsonObject payload;
        payload["toolInfo"] = info.toJson();
        
        sendMessage(ToolIpc::MessageType::ToolInfoResponse, payload, msg.requestId);
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
            
        case ToolIpc::MessageType::TagsResponse:
            if (msg.payload.contains("tags")) {
                TagManager::instance().setFromJson(msg.payload["tags"].toObject());
            }
            qDebug() << "Received tags data from main process";
            m_tagsReceived = true;
            break;
            
        default:
            break;
        }
        
        if (m_configReceived && m_fileIndexReceived && m_tagsReceived) {
            m_dataReady = true;
            if (m_dataWaitLoop) {
                m_dataWaitLoop->quit();
            }
        }
    }
    
    void handleShutdown() {
        qDebug() << "Handling shutdown - hiding widgets first";
        
        if (m_mainWidget) {
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
            if (IsWindow(hwnd)) {
                ShowWindow(hwnd, SW_HIDE);
            }
#endif
            m_mainWidget->hide();
            delete m_mainWidget;
            m_mainWidget = nullptr;
        }
        
        if (m_sidebarWidget) {
#ifdef Q_OS_WIN
            HWND hwnd = reinterpret_cast<HWND>(m_sidebarWidget->winId());
            if (IsWindow(hwnd)) {
                ShowWindow(hwnd, SW_HIDE);
            }
#endif
            m_sidebarWidget->hide();
            delete m_sidebarWidget;
            m_sidebarWidget = nullptr;
        }
        
        m_heartbeatTimer->stop();
        
        if (m_socket->state() == QLocalSocket::ConnectedState) {
            m_socket->disconnectFromServer();
        }
        
        QApplication::quit();
    }
    
    void requestInitialDataAsync() {
        m_configReceived = false;
        m_fileIndexReceived = false;
        m_tagsReceived = false;
        m_dataReady = false;
        
        sendMessage(ToolIpc::MessageType::GetConfig);
        sendMessage(ToolIpc::MessageType::GetFileIndex);
        sendMessage(ToolIpc::MessageType::GetTags);
        
        qDebug() << "Requested initial data from main process (async)";
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
    
    ToolInterface* m_tool;
    QWidget* m_mainWidget;
    QWidget* m_sidebarWidget;
    quint32 m_requestId;
    
    bool m_configReceived = false;
    bool m_fileIndexReceived = false;
    bool m_tagsReceived = false;
    bool m_dataReady = false;
    QEventLoop* m_dataWaitLoop = nullptr;
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
