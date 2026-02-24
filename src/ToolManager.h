#ifndef TOOLMANAGER_H
#define TOOLMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QPluginLoader>
#include "ToolInterface.h"

class ToolProxyInterface;

class ToolManager : public QObject {
    Q_OBJECT

public:
    static ToolManager& instance();

    void loadTools();
    void unloadTools();  // Stop all tool processes
    QList<ToolInterface*> getTools() const;
    ToolInterface* getTool(const QString& id) const;
    
    // Check if any tool widget is currently active/visible
    bool isToolActive() const;
    void setToolActive(bool active);
    
    // Process isolation mode
    bool isProcessIsolationEnabled() const { return m_processIsolationEnabled; }
    void setProcessIsolationEnabled(bool enabled);
    
    // Get active tool proxy (for crash handling)
    ToolProxyInterface* getActiveToolProxy() const { return m_activeToolProxy; }
    void setActiveToolProxy(ToolProxyInterface* proxy) { m_activeToolProxy = proxy; }

signals:
    void toolsLoaded();
    void toolProcessCrashed(const QString& toolId, const QString& error);

private:
    ToolManager();
    
    void loadToolsWithIsolation();
    void loadToolsDirectly();
    
    QList<ToolInterface*> m_tools;
    QMap<QString, ToolInterface*> m_toolMap;
    bool m_isToolActive = false;
    bool m_processIsolationEnabled = false; // Process isolation disabled - tools run in main process (isolation mode has display issues)
    ToolProxyInterface* m_activeToolProxy = nullptr;
};

#endif // TOOLMANAGER_H
