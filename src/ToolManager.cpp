//-------------------------------------------------------------------------------------
// ToolManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolManager.h"
#include "ToolProxyInterface.h"
#include "ToolDescriptorParser.h"
#include "PackageRegistry.h"
#include "Logger.h"
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace {
QStringList splitCompatibleVersions(const QString& versions) {
    return versions.split(";", Qt::SkipEmptyParts);
}

bool matchVersionPattern(const QString& appVersion, const QString& pattern) {
    QString trimmed = pattern.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    QStringList appParts = appVersion.split(".", Qt::SkipEmptyParts);
    QStringList patternParts = trimmed.split(".", Qt::SkipEmptyParts);

    if (appParts.size() < patternParts.size()) {
        return false;
    }

    for (int i = 0; i < patternParts.size(); ++i) {
        const QString& part = patternParts[i];
        if (part == "*") {
            continue;
        }
        if (appParts[i] != part) {
            return false;
        }
    }

    return true;
}

bool isVersionCompatible(const QString& appVersion, const QString& requirement) {
    const QStringList patterns = splitCompatibleVersions(requirement);
    if (patterns.isEmpty()) {
        return false;
    }

    for (const QString& pattern : patterns) {
        if (matchVersionPattern(appVersion, pattern)) {
            return true;
        }
    }
    return false;
}

QString toolBinaryExtension() {
#if defined(Q_OS_WIN)
    return QStringLiteral(".dll");
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    return QStringLiteral(".dylib");
#else
    return QStringLiteral(".so");
#endif
}

QString resolveToolBinaryPath(const QDir& toolDir, const QString& toolName) {
    const QString trimmedToolName = toolName.trimmed();
    if (trimmedToolName.isEmpty()) {
        return QString();
    }

    const QString candidatePath = toolDir.filePath(trimmedToolName + toolBinaryExtension());
    const QFileInfo candidateInfo(candidatePath);
    if (!candidateInfo.exists() || !candidateInfo.isFile()) {
        return QString();
    }

    return candidateInfo.absoluteFilePath();
}
}

ToolManager::ToolManager() {}

ToolManager& ToolManager::instance() {
    static ToolManager instance;
    return instance;
}

void ToolManager::setQuestionDialogHandler(
    std::function<void(const QString&, const QString&, std::function<void(bool)>)> handler) {
    m_questionDialogHandler = std::move(handler);
}

ToolProxyInterface* ToolManager::getActiveToolProxy() const {
    return m_activeToolProxy;
}

void ToolManager::setActiveToolProxy(ToolProxyInterface* proxy) {
    if (m_activeToolProxy == proxy) {
        return;
    }

    const QString previousId = m_activeToolProxy ? m_activeToolProxy->id() : QStringLiteral("<none>");
    const QString nextId = proxy ? proxy->id() : QStringLiteral("<none>");
    Logger::instance().logInfo(
        "ToolManager",
        QString("Active tool proxy changed from %1 to %2").arg(previousId, nextId)
    );
    m_activeToolProxy = proxy;
}

void ToolManager::loadTools() {
    m_tools.clear();
    m_toolMap.clear();
    m_isToolActive = false;
    setActiveToolProxy(nullptr);

    loadToolProxies();
}

void ToolManager::loadToolProxies() {
    const QList<RegisteredPackage> registeredTools = PackageRegistry::registeredPackages(PackageKind::Tool);
    if (registeredTools.isEmpty()) {
        Logger::instance().logInfo("ToolManager", "No registered tools found.");
    }

    for (const RegisteredPackage& registeredTool : registeredTools) {
        QDir toolDir(registeredTool.directoryPath);
        Logger::instance().logInfo("ToolManager", "Checking directory: " + toolDir.absolutePath());
        
        const QString descriptorPath = toolDir.filePath("descriptor.apehts");
        QJsonObject metaData;
        QString errorMessage;
        if (!ToolDescriptorParser::parseDescriptorFile(descriptorPath, metaData, &errorMessage)) {
            Logger::instance().logWarning("ToolManager", errorMessage);
            continue;
        }

        const QString toolId = metaData.value("id").toString().trimmed();
        const QString toolName = metaData.value("name").toString().trimmed();
        if (toolId != registeredTool.id) {
            Logger::instance().logWarning(
                "ToolManager",
                QString("Tool registry ID does not match descriptor ID: %1 != %2")
                    .arg(registeredTool.id, toolId)
            );
            continue;
        }

        const QString filePath = resolveToolBinaryPath(toolDir, toolName);
        if (filePath.trimmed().isEmpty()) {
            Logger::instance().logWarning(
                "ToolManager",
                QString("Expected tool worker binary not found for %1: %2")
                    .arg(toolName, toolDir.filePath(toolName + toolBinaryExtension()))
            );
            continue;
        }

        Logger::instance().logInfo(
            "ToolManager",
            QString("Creating tool proxy for plugin: %1")
                .arg(filePath)
        );

        ToolProxyInterface* proxy = new ToolProxyInterface(filePath, toolDir.absolutePath(), this);
        proxy->setMetaData(metaData);
        proxy->preloadInfo();
        
        if (!proxy->isInfoLoaded()) {
            Logger::instance().logWarning("ToolManager", "Failed to preload info for: " + filePath);
            delete proxy;
            continue;
        }
        
        // Version Check
        QString appVersion = APP_VERSION;
        QString requiredVersion = proxy->compatibleVersion();
        if (!isVersionCompatible(appVersion, requiredVersion)) {
            Logger::instance().logWarning("ToolManager", 
                QString("Version mismatch for tool %1: Requires App v%2, Current App v%3")
                .arg(proxy->id()).arg(requiredVersion).arg(appVersion));
        } else {
            Logger::instance().logInfo("ToolManager", 
                QString("Tool %1 version check passed (v%2)").arg(proxy->id()).arg(requiredVersion));
        }

        // Check if already loaded (by ID)
        if (m_toolMap.contains(proxy->id())) {
            Logger::instance().logWarning("ToolManager", "Duplicate tool ID found: " + proxy->id() + ". Skipping " + filePath);
            delete proxy;
            continue; 
        }
        
        // Connect crash signal
        connect(proxy, &ToolProxyInterface::processCrashed, this, [this, proxy](const QString& error) {
            if (proxy != m_activeToolProxy) {
                Logger::instance().logWarning(
                    "ToolManager",
                    QString("Ignoring crash from non-active proxy %1").arg(proxy->id())
                );
                return;
            }

            Logger::instance().logError(
                "ToolManager",
                QString("Active tool proxy crashed: %1 (%2)").arg(proxy->id(), error)
            );
            emit toolProcessCrashed(proxy->id(), error);
        });
        
        proxy->initialize();
        m_tools.append(proxy);
        m_toolMap.insert(proxy->id(), proxy);
        Logger::instance().logInfo("ToolManager", "Loaded tool (proxy): " + proxy->name() + " (" + proxy->id() + ")");
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
    if (!active) {
        setActiveToolProxy(nullptr);
    }
}

void ToolManager::unloadTools() {
    Logger::instance().logInfo("ToolManager", "Unloading all tools...");
    
    for (ToolInterface* tool : m_tools) {
        ToolProxyInterface* proxy = dynamic_cast<ToolProxyInterface*>(tool);
        if (proxy) {
            proxy->forceKillProcess();
        }
    }
    
    m_isToolActive = false;
    setActiveToolProxy(nullptr);
    
    Logger::instance().logInfo("ToolManager", "All tools unloaded");
}

bool ToolManager::unloadToolsAndWait(int timeoutMsPerTool) {
    Logger::instance().logInfo(
        "ToolManager",
        QString("Unloading all tools with wait, timeout per tool: %1 ms").arg(timeoutMsPerTool)
    );

    bool allStopped = true;

    for (ToolInterface* tool : m_tools) {
        ToolProxyInterface* proxy = dynamic_cast<ToolProxyInterface*>(tool);
        if (!proxy) {
            continue;
        }

        if (!proxy->isProcessRunning()) {
            continue;
        }

        proxy->stopProcess();
        if (!proxy->waitForProcessStopped(timeoutMsPerTool)) {
            Logger::instance().logWarning(
                "ToolManager",
                QString("Tool process did not confirm stop in time: %1").arg(proxy->id())
            );
            proxy->forceKillProcess();
            if (!proxy->waitForProcessStopped(timeoutMsPerTool)) {
                Logger::instance().logError(
                    "ToolManager",
                    QString("Tool process still running after force kill: %1").arg(proxy->id())
                );
                allStopped = false;
            }
        }
    }

    m_isToolActive = false;
    setActiveToolProxy(nullptr);

    Logger::instance().logInfo(
        "ToolManager",
        QString("All tools unload with wait completed: %1").arg(allStopped ? "success" : "partial_failure")
    );
    return allStopped;
}

void ToolManager::requestQuestionDialog(const QString& title, const QString& message,
                                         std::function<void(bool)> callback) {
    if (m_questionDialogHandler) {
        m_questionDialogHandler(title, message, std::move(callback));
        return;
    }

    if (callback) {
        callback(false);
    }
}
