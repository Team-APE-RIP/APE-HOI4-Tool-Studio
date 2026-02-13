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
