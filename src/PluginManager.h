//-------------------------------------------------------------------------------------
// PluginManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QStringList>
#include "PluginDescriptorParser.h"

class PluginManager : public QObject {
    Q_OBJECT

public:
    static PluginManager& instance();

    void loadPlugins();
    QList<PluginInfo> getPlugins() const;
    bool isPluginLoaded(const QString& name) const;
    PluginInfo getPlugin(const QString& name) const;
    bool getPluginBinaryPath(const QString& name, QString* outPath, QString* errorMessage = nullptr) const;
    QStringList getMissingDependencies(const QStringList& dependencies) const;

signals:
    void pluginsLoaded();

private:
    PluginManager();

    QList<PluginInfo> m_plugins;
    QMap<QString, PluginInfo> m_pluginMap;
};

#endif // PLUGINMANAGER_H