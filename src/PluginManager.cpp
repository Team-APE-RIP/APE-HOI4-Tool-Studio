#include "PluginManager.h"

#include "Logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>

PluginManager::PluginManager() {}

PluginManager& PluginManager::instance() {
    static PluginManager instance;
    return instance;
}

void PluginManager::loadPlugins() {
    m_plugins.clear();
    m_pluginMap.clear();

    QDir appDir(QCoreApplication::applicationDirPath());

    if (appDir.exists("plugins")) {
        appDir.cd("plugins");
    } else {
        QDir parentDir = appDir;
        if (parentDir.cdUp() && parentDir.exists("plugins")) {
            appDir = parentDir;
            appDir.cd("plugins");
        } else {
            Logger::instance().logInfo("PluginManager", "Plugins directory not found.");
            emit pluginsLoaded();
            return;
        }
    }

    Logger::instance().logInfo("PluginManager", "Scanning plugins in: " + appDir.absolutePath());

    const QStringList subDirs = appDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& dirName : subDirs) {
        QDir pluginDir(appDir.filePath(dirName));
        const QString descriptorPath = pluginDir.filePath("descriptor.htsplugin");
        if (!QFile::exists(descriptorPath)) {
            Logger::instance().logWarning("PluginManager", "No descriptor.htsplugin found in: " + pluginDir.absolutePath());
            continue;
        }

        PluginInfo info;
        QString errorMessage;
        if (!PluginDescriptorParser::parseDescriptorFile(descriptorPath, info, &errorMessage)) {
            Logger::instance().logWarning("PluginManager", errorMessage);
            continue;
        }

        const QStringList files = pluginDir.entryList(QStringList() << "*.dll" << "*.so" << "*.dylib", QDir::Files);
        if (!files.isEmpty()) {
            info.libraryPath = pluginDir.filePath(files.first());
        }

        if (m_pluginMap.contains(info.name)) {
            Logger::instance().logWarning("PluginManager", "Duplicate plugin name found: " + info.name);
            continue;
        }

        m_plugins.append(info);
        m_pluginMap.insert(info.name, info);
        Logger::instance().logInfo("PluginManager", QString("Loaded plugin metadata: %1 (%2)").arg(info.name, info.id));
    }

    emit pluginsLoaded();
}

QList<PluginInfo> PluginManager::getPlugins() const {
    return m_plugins;
}

bool PluginManager::isPluginLoaded(const QString& name) const {
    const auto it = m_pluginMap.constFind(name);
    return it != m_pluginMap.constEnd() && it.value().isValid();
}

PluginInfo PluginManager::getPlugin(const QString& name) const {
    return m_pluginMap.value(name);
}

bool PluginManager::getPluginBinaryPath(const QString& name, QString* outPath, QString* errorMessage) const {
    const auto it = m_pluginMap.constFind(name);
    if (it == m_pluginMap.constEnd()) {
        if (errorMessage) {
            *errorMessage = QString("Plugin %1 is not loaded.").arg(name);
        }
        return false;
    }

    const PluginInfo& info = it.value();
    if (info.libraryPath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("Plugin %1 does not provide a runtime library.").arg(name);
        }
        return false;
    }

    if (outPath) {
        *outPath = info.libraryPath;
    }
    return true;
}

QStringList PluginManager::getMissingDependencies(const QStringList& dependencies) const {
    QStringList missing;
    for (const QString& dependency : dependencies) {
        if (!isPluginLoaded(dependency)) {
            missing.append(dependency);
        }
    }
    return missing;
}