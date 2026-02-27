#include "ToolManager.h"
#include "ToolProxyInterface.h"
#include "Logger.h"
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QPluginLoader>
#include <QFile>

ToolManager::ToolManager() {}

ToolManager& ToolManager::instance() {
    static ToolManager instance;
    return instance;
}

void ToolManager::setProcessIsolationEnabled(bool enabled) {
    m_processIsolationEnabled = enabled;
}

void ToolManager::loadTools() {
    if (m_processIsolationEnabled) {
        loadToolsWithIsolation();
    } else {
        loadToolsDirectly();
    }
}

void ToolManager::loadToolsWithIsolation() {
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

    Logger::instance().logInfo("ToolManager", "Scanning tools (isolation mode) in: " + appDir.absolutePath());

    // Iterate over subdirectories (each tool in its own folder)
    const QStringList subDirs = appDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    if (subDirs.isEmpty()) {
        Logger::instance().logInfo("ToolManager", "No tool subdirectories found.");
    }

    for (const QString &dirName : subDirs) {
        QDir toolDir(appDir.filePath(dirName));
        Logger::instance().logInfo("ToolManager", "Checking directory: " + toolDir.absolutePath());
        
        // Check for metadata.json first
        QString metadataPath = toolDir.filePath("metadata.json");
        if (!QFile::exists(metadataPath)) {
            Logger::instance().logInfo("ToolManager", "No metadata.json found in: " + dirName);
            continue;
        }
        
        // Look for library files
        const QStringList files = toolDir.entryList(QStringList() << "*.dll" << "*.so" << "*.dylib", QDir::Files);
        
        if (files.isEmpty()) {
            Logger::instance().logInfo("ToolManager", "No plugin files found in: " + dirName);
            continue;
        }

        // Use the first library file found
        QString filePath = toolDir.filePath(files.first());
        Logger::instance().logInfo("ToolManager", "Creating proxy for plugin: " + filePath);
        
        // Create proxy interface
        ToolProxyInterface* proxy = new ToolProxyInterface(filePath, toolDir.absolutePath(), this);
        proxy->preloadInfo();
        
        if (!proxy->isInfoLoaded()) {
            Logger::instance().logWarning("ToolManager", "Failed to preload info for: " + filePath);
            delete proxy;
            continue;
        }
        
        // Version Check
        QString appVersion = APP_VERSION;
        QString requiredVersion = proxy->compatibleVersion();
        if (appVersion != requiredVersion) {
            Logger::instance().logWarning("ToolManager", 
                QString("Version mismatch for tool %1: Requires App v%2, Current App v%3")
                .arg(proxy->id()).arg(requiredVersion).arg(appVersion));
        } else {
            Logger::instance().logInfo("ToolManager", 
                QString("Tool %1 version check passed (v%2)").arg(proxy->id()).arg(requiredVersion));
        }

        // Check if already loaded (by ID)
        if (m_toolMap.contains(proxy->id())) {
            Logger::instance().logWarning("ToolManager", "Duplicate tool ID found: " + proxy->id() + ". Skipping " + files.first());
            delete proxy;
            continue; 
        }
        
        // Connect crash signal
        connect(proxy, &ToolProxyInterface::processCrashed, this, [this, proxy](const QString& error) {
            emit toolProcessCrashed(proxy->id(), error);
        });
        
        proxy->initialize();
        m_tools.append(proxy);
        m_toolMap.insert(proxy->id(), proxy);
        Logger::instance().logInfo("ToolManager", "Loaded tool (proxy): " + proxy->name() + " (" + proxy->id() + ")");
    }
    
    Logger::instance().logInfo("ToolManager", QString("Total tools loaded (isolation mode): %1").arg(m_tools.size()));
    emit toolsLoaded();
}

void ToolManager::loadToolsDirectly() {
    // Original direct loading logic (kept for fallback/debugging)
    QDir appDir(QCoreApplication::applicationDirPath());
    
    if (appDir.exists("tools")) {
        appDir.cd("tools");
    } else {
        QDir parentDir = appDir;
        if (parentDir.cdUp() && parentDir.exists("tools")) {
            appDir = parentDir;
            appDir.cd("tools");
        } else {
            Logger::instance().logInfo("ToolManager", "Tools directory not found.");
            return;
        }
    }

    Logger::instance().logInfo("ToolManager", "Scanning tools (direct mode) in: " + appDir.absolutePath());

    const QStringList subDirs = appDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    if (subDirs.isEmpty()) {
        Logger::instance().logInfo("ToolManager", "No tool subdirectories found.");
    }

    for (const QString &dirName : subDirs) {
        QDir toolDir(appDir.filePath(dirName));
        Logger::instance().logInfo("ToolManager", "Checking directory: " + toolDir.absolutePath());
        
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
    
    Logger::instance().logInfo("ToolManager", QString("Total tools loaded (direct mode): %1").arg(m_tools.size()));
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
    if (!active) {
        m_activeToolProxy = nullptr;
    }
}

void ToolManager::unloadTools() {
    Logger::instance().logInfo("ToolManager", "Unloading all tools...");
    
    // Force kill all tool proxy processes immediately (no waiting)
    for (ToolInterface* tool : m_tools) {
        ToolProxyInterface* proxy = dynamic_cast<ToolProxyInterface*>(tool);
        if (proxy) {
            proxy->forceKillProcess();
        }
    }
    
    m_isToolActive = false;
    m_activeToolProxy = nullptr;
    
    Logger::instance().logInfo("ToolManager", "All tools unloaded");
}

void ToolManager::requestQuestionDialog(const QString& title, const QString& message, 
                                         std::function<void(bool)> callback) {
    emit questionDialogRequested(title, message, callback);
}
