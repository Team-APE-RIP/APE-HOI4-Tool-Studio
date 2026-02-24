#ifndef TOOLIPCPROTOCOL_H
#define TOOLIPCPROTOCOL_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>

namespace ToolIpc {

// Message types for IPC communication
enum class MessageType : quint32 {
    // Lifecycle
    Init = 1,
    Shutdown = 2,
    Heartbeat = 3,
    HeartbeatAck = 4,
    
    // Widget management
    CreateWidget = 10,
    CreateWidgetResponse = 11,
    CreateSidebarWidget = 12,
    CreateSidebarWidgetResponse = 13,
    DestroyWidget = 14,
    ShowWidget = 15,
    ShowWidgetResponse = 16,
    ResizeWidget = 17,
    
    // Tool info
    GetToolInfo = 20,
    ToolInfoResponse = 21,
    
    // Language & Theme
    LoadLanguage = 30,
    ApplyTheme = 31,
    
    // Data requests (Tool -> Host)
    GetConfig = 40,
    ConfigResponse = 41,
    GetFileIndex = 42,
    FileIndexResponse = 43,
    GetTags = 44,
    TagsResponse = 45,
    
    // Data notifications (Host -> Tool)
    ConfigChanged = 50,
    FileIndexChanged = 51,
    ThemeChanged = 52,
    
    // Error
    Error = 100,
    
    // Ready signal
    Ready = 200
};

// IPC Message structure
struct Message {
    MessageType type;
    quint32 requestId;
    QJsonObject payload;
    
    QByteArray serialize() const {
        QJsonObject obj;
        obj["type"] = static_cast<int>(type);
        obj["requestId"] = static_cast<int>(requestId);
        obj["payload"] = payload;
        
        QJsonDocument doc(obj);
        QByteArray data = doc.toJson(QJsonDocument::Compact);
        
        // Prepend length (4 bytes)
        quint32 len = data.size();
        QByteArray result;
        result.append(reinterpret_cast<const char*>(&len), sizeof(len));
        result.append(data);
        return result;
    }
    
    static Message deserialize(const QByteArray& data) {
        Message msg;
        msg.type = MessageType::Error;
        msg.requestId = 0;
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            msg.type = static_cast<MessageType>(obj["type"].toInt());
            msg.requestId = obj["requestId"].toInt();
            msg.payload = obj["payload"].toObject();
        }
        return msg;
    }
};

// Helper to create messages
inline Message createMessage(MessageType type, quint32 requestId = 0, const QJsonObject& payload = QJsonObject()) {
    Message msg;
    msg.type = type;
    msg.requestId = requestId;
    msg.payload = payload;
    return msg;
}

// Tool info structure (serializable)
struct ToolInfo {
    QString id;
    QString name;
    QString description;
    QString version;
    QString compatibleVersion;
    QString author;
    QString iconPath;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["description"] = description;
        obj["version"] = version;
        obj["compatibleVersion"] = compatibleVersion;
        obj["author"] = author;
        obj["iconPath"] = iconPath;
        return obj;
    }
    
    static ToolInfo fromJson(const QJsonObject& obj) {
        ToolInfo info;
        info.id = obj["id"].toString();
        info.name = obj["name"].toString();
        info.description = obj["description"].toString();
        info.version = obj["version"].toString();
        info.compatibleVersion = obj["compatibleVersion"].toString();
        info.author = obj["author"].toString();
        info.iconPath = obj["iconPath"].toString();
        return info;
    }
};

// Window handle wrapper (platform-specific)
struct WindowHandle {
    qint64 handle;
    int width;
    int height;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["handle"] = handle;
        obj["width"] = width;
        obj["height"] = height;
        return obj;
    }
    
    static WindowHandle fromJson(const QJsonObject& obj) {
        WindowHandle wh;
        wh.handle = obj["handle"].toInteger();
        wh.width = obj["width"].toInt();
        wh.height = obj["height"].toInt();
        return wh;
    }
};

// Constants
const QString IPC_SERVER_PREFIX = "APEHOI4ToolStudio_";
const int HEARTBEAT_INTERVAL_MS = 5000;
const int HEARTBEAT_TIMEOUT_MS = 15000;
const int PROCESS_START_TIMEOUT_MS = 10000;

} // namespace ToolIpc

#endif // TOOLIPCPROTOCOL_H
