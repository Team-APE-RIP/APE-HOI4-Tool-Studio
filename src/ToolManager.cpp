#include "ToolManager.h"
#include "Logger.h"
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>

ToolManager::ToolManager() {}

ToolManager& ToolManager::instance() {
    static ToolManager instance;
    return instance;
}

void ToolManager::loadTools() {
    // Determine the path to the 'tools' directory
    QDir appDir(QCoreApplication::applicationDirPath());
    
    // Check for 'tools' directory in the same directory as the executable
    if (appDir.exists("tools")) {
        appDir.cd("tools");
    } else {
        // Fallback: Check if we are in bin/debug/release and tools is one level up
        QDir parentDir = appDir;
        if (parentDir.cdUp() && parentDir.exists("tools")) {
            appDir = parentDir;
            appDir.cd("tools");
        } else {
            Logger::instance().logInfo("ToolManager", "Tools directory not found.");
            return;
        }
    }

    Logger::instance().logInfo("ToolManager", "Scanning tools in: " + appDir.absolutePath());

    // Iterate over subdirectories (each tool in its own folder)
    const QStringList subDirs = appDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    if (subDirs.isEmpty()) {
        Logger::instance().logInfo("ToolManager", "No tool subdirectories found.");
    }

    for (const QString &dirName : subDirs) {
        QDir toolDir(appDir.filePath(dirName));
        Logger::instance().logInfo("ToolManager", "Checking directory: " + toolDir.absolutePath());
        
        // Look for library files
        const QStringList files = toolDir.entryList(QStringList() << "*.dll" << "*.so" << "*.dylib", QDir::Files);
        
        if (files.isEmpty()) {
            Logger::instance().logInfo("ToolManager", "No plugin files found in: " + dirName);
        }

        for (const QString &fileName : files) {
            QString filePath = toolDir.filePath(fileName);
            Logger::instance().logInfo("ToolManager", "Attempting to load plugin: " + filePath);
            
            QPluginLoader loader(filePath);
            QObject *plugin = loader.instance();
            
            if (plugin) {
                ToolInterface *tool = qobject_cast<ToolInterface *>(plugin);
                if (tool) {
                    // Inject metadata
                    QJsonObject metaData = loader.metaData().value("MetaData").toObject();
                    tool->setMetaData(metaData);

                    // Version Check
                    QString appVersion = APP_VERSION;
                    QString requiredVersion = tool->compatibleVersion();
                    if (appVersion != requiredVersion) {
                        Logger::instance().logWarning("ToolManager", 
                            QString("Version mismatch for tool %1: Requires App v%2, Current App v%3")
                            .arg(tool->id()).arg(requiredVersion).arg(appVersion));
                    } else {
                        Logger::instance().logInfo("ToolManager", 
                            QString("Tool %1 version check passed (v%2)").arg(tool->id()).arg(requiredVersion));
                    }

                    // Check if already loaded (by ID)
                    if (m_toolMap.contains(tool->id())) {
                        Logger::instance().logWarning("ToolManager", "Duplicate tool ID found: " + tool->id() + ". Skipping " + fileName);
                        continue; 
                    }
                    
                    tool->initialize();
                    m_tools.append(tool);
                    m_toolMap.insert(tool->id(), tool);
                    Logger::instance().logInfo("ToolManager", "Loaded tool: " + tool->name() + " (" + tool->id() + ")");
                } else {
                    Logger::instance().logError("ToolManager", "Plugin is not a ToolInterface: " + fileName);
                    delete plugin;
                }
            } else {
                Logger::instance().logError("ToolManager", "Failed to load plugin: " + fileName + ". Error: " + loader.errorString());
            }
        }
    }
    
    Logger::instance().logInfo("ToolManager", QString("Total tools loaded: %1").arg(m_tools.size()));
    emit toolsLoaded();
}

QList<ToolInterface*> ToolManager::getTools() const {
    return m_tools;
}

ToolInterface* ToolManager::getTool(const QString& id) const {
    return m_toolMap.value(id, nullptr);
}

bool ToolManager::isToolActive() const {
    return m_isToolActive;
}

void ToolManager::setToolActive(bool active) {
    m_isToolActive = active;
}
