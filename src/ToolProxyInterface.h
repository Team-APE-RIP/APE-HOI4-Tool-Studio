#ifndef TOOLPROXYINTERFACE_H
#define TOOLPROXYINTERFACE_H

#include <QObject>
#include <QProcess>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <QPointer>
#include <QMap>
#include <functional>
#include "ToolInterface.h"
#include "ToolIpcProtocol.h"

class QVBoxLayout;

// Container widget that embeds the tool's window from subprocess
class ToolEmbedContainer : public QWidget {
    Q_OBJECT
public:
    explicit ToolEmbedContainer(QWidget* parent = nullptr);
    ~ToolEmbedContainer();
    
    // Set pending window ID for delayed embedding
    void setPendingWindowId(WId windowId);
    bool embedWindow(WId windowId);
    void releaseWindow();
    
    // Check if window is embedded
    bool isEmbedded() const { return m_embedded; }
    
signals:
    void embeddingComplete(bool success);
    void resized(int width, int height);
    
protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
private:
    void doEmbed();
    
    QWindow* m_foreignWindow;
    QWidget* m_container;
    WId m_pendingWindowId = 0;
    bool m_embedded = false;
    bool m_firstShow = true;
#ifdef Q_OS_WIN
    void* m_childHwnd;  // HWND stored as void* to avoid including windows.h
#endif
};

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
    
    void setMetaData(const QJsonObject& metaData) override;
    QIcon icon() const override;
    void initialize() override;
    QWidget* createWidget(QWidget* parent = nullptr) override;
    QWidget* createSidebarWidget(QWidget* parent = nullptr) override;
    void loadLanguage(const QString& lang) override;
    void applyTheme() override;
    
    // Process management
    bool startProcess();
    void stopProcess();
    void forceKillProcess();
    bool isProcessRunning() const;
    
    // Check if tool info is loaded (from metadata.json pre-scan)
    bool isInfoLoaded() const { return m_infoLoaded; }
    void preloadInfo(); // Load basic info from metadata.json without starting process

signals:
    void processStarted();
    void processStopped();
    void processCrashed(const QString& error);
    void widgetReady(QWidget* widget);
    void sidebarWidgetReady(QWidget* widget);

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
    void sendMessage(ToolIpc::MessageType type, const QJsonObject& payload = QJsonObject(), quint32 requestId = 0);
    quint32 nextRequestId() { return ++m_requestIdCounter; }
    
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
    
    ToolIpc::ToolInfo m_toolInfo;
    bool m_infoLoaded;
    bool m_processReady;
    
    quint32 m_requestIdCounter;
    QMap<quint32, ResponseCallback> m_pendingRequests;
    
    QPointer<ToolEmbedContainer> m_mainContainer;
    QPointer<ToolEmbedContainer> m_sidebarContainer;
    WId m_pendingWindowId = 0;
};

#endif // TOOLPROXYINTERFACE_H
