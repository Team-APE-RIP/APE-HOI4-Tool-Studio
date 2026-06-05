//-------------------------------------------------------------------------------------
// PluginManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "PluginManager.h"

#include "PackageRegistry.h"
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

    const QList<RegisteredPackage> registeredPlugins = PackageRegistry::registeredPackages(PackageKind::Plugin);
    Logger::instance().logInfo(
        "PluginManager",
        QString("Scanning registered plugins: %1").arg(registeredPlugins.size())
    );

    for (const RegisteredPackage& registeredPlugin : registeredPlugins) {
        QDir pluginDir(registeredPlugin.directoryPath);
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

        if (info.id != registeredPlugin.id) {
            Logger::instance().logWarning(
                "PluginManager",
                QString("Plugin registry ID does not match descriptor ID: %1 != %2")
                    .arg(registeredPlugin.id, info.id)
            );
            continue;
        }
        info.official = registeredPlugin.official;

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
