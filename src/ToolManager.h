#ifndef TOOLMANAGER_H
#define TOOLMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QPluginLoader>
#include "ToolInterface.h"

class ToolManager : public QObject {
    Q_OBJECT

public:
    static ToolManager& instance();

    void loadTools();
    QList<ToolInterface*> getTools() const;
    ToolInterface* getTool(const QString& id) const;
    
    // Check if any tool widget is currently active/visible
    bool isToolActive() const;
    void setToolActive(bool active);

signals:
    void toolsLoaded();

private:
    ToolManager();
    
    QList<ToolInterface*> m_tools;
    QMap<QString, ToolInterface*> m_toolMap;
    bool m_isToolActive = false;
};

#endif // TOOLMANAGER_H
