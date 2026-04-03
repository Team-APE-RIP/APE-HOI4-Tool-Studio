//-------------------------------------------------------------------------------------
// PathValidator.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "PathValidator.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

PathValidator& PathValidator::instance() {
    static PathValidator instance;
    return instance;
}

PathValidator::PathValidator() {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &PathValidator::checkPaths);
}

void PathValidator::startMonitoring() {
    if (!m_timer->isActive()) {
        m_timer->start(5000); // Check every 5 seconds
    }
}

void PathValidator::stopMonitoring() {
    m_timer->stop();
}

QString PathValidator::validateGamePath(const QString& path) {
    if (path.isEmpty()) return ""; // Empty is handled by SetupDialog validation
    
    QDir dir(path);
    if (!dir.exists()) {
        Logger::instance().logError("PathValidator", "Game directory does not exist: " + path);
        return "PathNotExist";
    }
    
    if (!dir.exists("hoi4.exe")) {
        Logger::instance().logError("PathValidator", "hoi4.exe not found in: " + path);
        return "Hoi4NotFound";
    }
    
    return "";
}

QString PathValidator::validateModPath(const QString& path) {
    if (path.isEmpty()) return "";
    
    QDir dir(path);
    if (!dir.exists()) {
        Logger::instance().logError("PathValidator", "Mod directory does not exist: " + path);
        return "PathNotExist";
    }
    
    if (dir.dirName() == "mod") {
        Logger::instance().logError("PathValidator", "Mod directory name cannot be 'mod': " + path);
        return "ModNameInvalid";
    }
    
    QStringList filters;
    filters << "*.mod";
    QStringList modFiles = dir.entryList(filters, QDir::Files);
    
    if (modFiles.isEmpty()) {
        Logger::instance().logError("PathValidator", "No .mod file found in: " + path);
        return "NoModFile";
    }
    
    return "";
}

QString PathValidator::validateDocPath(const QString& path) {
    if (path.isEmpty()) return "";
    
    QDir dir(path);
    if (!dir.exists()) {
        Logger::instance().logError("PathValidator", "Documents directory does not exist: " + path);
        return "PathNotExist";
    }
    
    // Check if the selected folder is named "Hearts of Iron IV"
    if (dir.dirName() != "Hearts of Iron IV") {
        Logger::instance().logError("PathValidator", "Selected folder must be named 'Hearts of Iron IV': " + path);
        return "DocFolderNameInvalid";
    }
    
    // Check if parent directory contains "Paradox Interactive"
    QDir parentDir = dir;
    parentDir.cdUp();  // Navigate to parent directory
    if (!parentDir.dirName().contains("Paradox Interactive", Qt::CaseInsensitive)) {
        Logger::instance().logError("PathValidator", "Parent directory must contain 'Paradox Interactive': " + path);
        return "DocParentNotParadoxInteractive";
    }
    
    return "";
}

void PathValidator::checkPaths() {
    ConfigManager& config = ConfigManager::instance();
    
    QString gamePath = config.getGamePath();
    if (!gamePath.isEmpty()) {
        QString error = validateGamePath(gamePath);
        if (!error.isEmpty()) {
            emit pathInvalid("GamePathInvalid", error);
            // Stop monitoring to avoid spamming dialogs
            stopMonitoring();
            return;
        }
    }
    
    QString modPath = config.getModPath();
    if (!modPath.isEmpty()) {
        QString error = validateModPath(modPath);
        if (!error.isEmpty()) {
            emit pathInvalid("ModPathInvalid", error);
            stopMonitoring();
            return;
        }
    }
}
