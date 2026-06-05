//-------------------------------------------------------------------------------------
// ToolManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLMANAGER_H
#define TOOLMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <functional>
#include "ToolInterface.h"

class ToolProxyInterface;

class ToolManager : public QObject {
    Q_OBJECT

public:
    static ToolManager& instance();

    void loadTools();
    void unloadTools();  // Stop all tool processes
    bool unloadToolsAndWait(int timeoutMsPerTool = 3000);
    QList<ToolInterface*> getTools() const;
    ToolInterface* getTool(const QString& id) const;
    
    // Check if any tool widget is currently active/visible
    bool isToolActive() const;
    void setToolActive(bool active);
    
    // Get active tool proxy (for crash handling)
    ToolProxyInterface* getActiveToolProxy() const;
    void setActiveToolProxy(ToolProxyInterface* proxy);

signals:
    void toolsLoaded();
    void toolProcessCrashed(const QString& toolId, const QString& error);

public:
    // Register main window dialog handler for tool callbacks.
    void setQuestionDialogHandler(std::function<void(const QString&, const QString&, std::function<void(bool)>)> handler);
    // Request main window to show a question dialog (for tools to use)
    void requestQuestionDialog(const QString& title, const QString& message,
                               std::function<void(bool)> callback);

private:
    ToolManager();
    
    void loadToolProxies();
    
    QList<ToolInterface*> m_tools;
    QMap<QString, ToolInterface*> m_toolMap;
    bool m_isToolActive = false;
    ToolProxyInterface* m_activeToolProxy = nullptr;
    std::function<void(const QString&, const QString&, std::function<void(bool)>)> m_questionDialogHandler;
};

#endif // TOOLMANAGER_H
