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
#include <QWidget>
#include <QString>
#include <QIcon>
#include <QJsonObject>
#include <QStringList>
#include <QList>

class QTreeWidget;

struct ToolRightSidebarButtonDefinition {
    QString key;
    QString iconResource;
    QString tooltip;
};

struct ToolRightSidebarState {
    QString title;
    QStringList orderedButtonKeys;
    QString activeButtonKey;
    bool listVisible = true;
    bool searchModeAvailable = false;
    bool searchModeActive = false;
    QList<int> searchableColumns;
    QStringList searchableColumnLabels;
    bool showSelectAllButton = false;
};

class ToolInterface {
public:
    virtual ~ToolInterface() = default;

    // Basic Info
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual QString description() const = 0;
    virtual QString version() const = 0;
    virtual QString compatibleVersion() const = 0; // New: Required main program version
    virtual QString author() const = 0;
    virtual QStringList dependencies() const { return {}; }

    // Metadata Injection
    virtual void setMetaData(const QJsonObject& metaData) = 0;

    // Resources
    virtual QIcon icon() const = 0;

    // Initialization
    // You might want to pass a context object here in the future
    virtual void initialize() = 0;

    // UI
    virtual QWidget* createWidget(QWidget* parent = nullptr) = 0;
    virtual QWidget* createSidebarWidget(QWidget* parent = nullptr) { return nullptr; }

    // Host-managed right sidebar integration
    virtual QList<ToolRightSidebarButtonDefinition> rightSidebarButtons() const { return {}; }
    virtual ToolRightSidebarState rightSidebarState() const { return {}; }
    virtual QTreeWidget* rightSidebarListWidget() const { return nullptr; }
    virtual void handleRightSidebarButton(const QString& key) { Q_UNUSED(key); }

    // Localization
    virtual void loadLanguage(const QString& lang) = 0;

    // Theme
    virtual void applyTheme() {}
};

#define ToolInterface_iid "com.ape.hoi4toolstudio.ToolInterface"
Q_DECLARE_INTERFACE(ToolInterface, ToolInterface_iid)

#endif // TOOLINTERFACE_H