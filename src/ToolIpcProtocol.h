//-------------------------------------------------------------------------------------
// ToolIpcProtocol.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLIPCPROTOCOL_H
#define TOOLIPCPROTOCOL_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QByteArray>
#include <QStringList>

namespace ToolIpc {

// Message types for IPC communication
enum class MessageType : quint32 {
    // Lifecycle
    Init = 1,
    Shutdown = 2,
    Heartbeat = 3,
    HeartbeatAck = 4,
    
    // Legacy widget hosting messages were removed in the QML host architecture.

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
    InvokePlugin = 44,
    InvokePluginResponse = 45,
    ReadMatchingTextFiles = 46,
    ReadMatchingTextFilesResponse = 47,
    ReadBinaryFile = 48,
    ReadBinaryFileResponse = 49,
    ReadTextFile = 50,
    ReadTextFileResponse = 51,
    ReadEffectiveBinaryFile = 52,
    ReadEffectiveBinaryFileResponse = 53,
    ReadEffectiveTextFile = 54,
    ReadEffectiveTextFileResponse = 55,
    WriteBinaryFile = 56,
    WriteBinaryFileResponse = 57,
    WriteTextFile = 58,
    WriteTextFileResponse = 59,
    RemovePath = 60,
    RemovePathResponse = 61,
    EnsureDirectory = 62,
    EnsureDirectoryResponse = 63,
    ListDirectory = 64,
    ListDirectoryResponse = 65,
    ListEffectiveFiles = 66,
    ListEffectiveFilesResponse = 67,
    ReadEffectiveTextFiles = 68,
    ReadEffectiveTextFilesResponse = 69,
    
    // Data notifications (Host -> Tool)
    ConfigChanged = 70,
    FileIndexChanged = 71,
    ThemeChanged = 72,
    
    // UI state synchronization (QML host <-> Worker)
    UiAction = 80,
    UiActionResponse = 81,
    StateUpdate = 82,
    StateQuery = 83,
    StateQueryResponse = 84,
    
    // Worker management
    WorkerHeartbeat = 90,
    WorkerShutdown = 91,
    WorkerReady = 92,
    
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
    QStringList dependencies;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["description"] = description;
        obj["version"] = version;
        obj["compatibleVersion"] = compatibleVersion;
        obj["author"] = author;
        obj["iconPath"] = iconPath;

        QJsonArray dependencyArray;
        for (const QString& dependency : dependencies) {
            dependencyArray.append(dependency);
        }
        obj["dependencies"] = dependencyArray;
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

        const QJsonArray dependencyArray = obj["dependencies"].toArray();
        for (const QJsonValue& value : dependencyArray) {
            const QString dependency = value.toString().trimmed();
            if (!dependency.isEmpty()) {
                info.dependencies.append(dependency);
            }
        }
        return info;
    }
};

// Constants
const QString IPC_SERVER_PREFIX = "APEHOI4ToolStudio_";
const int HEARTBEAT_INTERVAL_MS = 5000;
const int HEARTBEAT_TIMEOUT_MS = 15000;
const int PROCESS_START_TIMEOUT_MS = 10000;

} // namespace ToolIpc

#endif // TOOLIPCPROTOCOL_H
