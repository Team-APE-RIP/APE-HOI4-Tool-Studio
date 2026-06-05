//-------------------------------------------------------------------------------------
// ToolInterface.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLINTERFACE_H
#define TOOLINTERFACE_H

#include <QtPlugin>
#include <QString>
#include <QIcon>
#include <QJsonObject>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QList>
#include <QMap>

class QWidget;

struct ToolRightSidebarButtonDefinition {
    QString key;
    QString iconResource;
    QString tooltip;
};

struct ToolRightSidebarState {
    QString title;
    bool listVisible = false;
    bool searchModeAvailable = false;
    bool searchModeActive = false;
    bool showSelectAllButton = false;
    QString activeButtonKey;
    QStringList orderedButtonKeys;
    QList<int> searchableColumns;
    QStringList searchableColumnLabels;
};

struct ToolGuiResourceDescriptor {
    QString entryFile;              // Optional override; tools normally use <descriptor name>.qml.
    QString presetFile;
    QStringList localisationNamespaces;
};

struct ToolWorkerDescriptor {
    QString workerId;               // Unique worker identifier
};

struct ToolUiActionRequest {
    QString actionType;             // e.g., "button_click", "list_select"
    QString targetId;               // e.g., "0::large", "import_button"
    QVariantMap arguments;          // Additional action parameters
};

struct ToolUiStatePacket {
    QString pageId;                 // Current active page
    QString modeId;                 // Current active mode
    QVariantMap viewState;          // Page-specific view state
    QVariantMap sidebarState;       // Sidebar configuration
    QVariantMap topbarState;        // Topbar configuration
    QVariantMap runtimeVariables;   // Runtime variables for UI
    QVariantList listModels;        // List data models
    QVariantList patches;           // Incremental state patches
};

class ToolInterface {
public:
    virtual ~ToolInterface() = default;

    // ========================================================================
    // Basic Metadata
    // ========================================================================
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QString version() const = 0;
    virtual QString compatibleVersion() const = 0;
    virtual QString author() const = 0;
    virtual QStringList dependencies() const { return {}; }

    // Metadata injection from descriptor.apehts
    virtual void setMetaData(const QJsonObject& metaData) = 0;

    // Tool icon resource
    virtual QIcon icon() const = 0;

    // ========================================================================
    // Initialization
    // ========================================================================
    virtual void initialize() = 0;

    // ========================================================================
    // Scripted UI Resources (REQUIRED)
    // ========================================================================
    
    // Provide GUI resource descriptor
    virtual ToolGuiResourceDescriptor guiResourceDescriptor() const { return {}; }

    // Provide Worker descriptor
    virtual ToolWorkerDescriptor workerDescriptor() const { return {}; }

    // ========================================================================
    // Worker Session Lifecycle
    // ========================================================================
    
    // Initialize worker session (called once when tool is loaded)
    virtual void initializeWorkerSession() {}

    // Get initial UI state packet
    // Called when tool is first opened to establish initial UI state
    virtual ToolUiStatePacket initialUiState() const { return {}; }

    // Handle UI action from user interaction
    // Returns state update packet to refresh UI
    virtual ToolUiStatePacket handleUiAction(const ToolUiActionRequest& request) {
        Q_UNUSED(request);
        return {};
    }

    // ========================================================================
    // Localization & Theme
    // ========================================================================
    virtual void loadLanguage(const QString& lang) = 0;
    virtual QMap<QString, QString> localizedStrings() const { return {}; }
    virtual void applyTheme() {}
};

#define ToolInterface_iid "com.ape.hoi4toolstudio.ToolInterface/2.0"
Q_DECLARE_INTERFACE(ToolInterface, ToolInterface_iid)

#endif // TOOLINTERFACE_H
