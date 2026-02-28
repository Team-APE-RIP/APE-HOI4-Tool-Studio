#include "ConfigManager.h"
#include "Logger.h"
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <QApplication>
#include <QStyleHints>

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    m_language = "English";
    m_theme = Theme::System;
    m_debugMode = false;
    m_sidebarCompactMode = false;
    m_maxLogFiles = 10; // Default to 10
    loadConfig();
    
    // Ensure config file exists on first run (create with defaults)
    QFile configFile(getGlobalConfigPath());
    if (!configFile.exists()) {
        // Check for setup language cache
        QString tempLangPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/setup_cache/temp_lang.json";
        QFile tempLangFile(tempLangPath);
        if (tempLangFile.exists() && tempLangFile.open(QIODevice::ReadOnly)) {
            QByteArray data = tempLangFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            if (obj.contains("language")) {
                m_language = obj["language"].toString();
            }
            tempLangFile.close();
            tempLangFile.remove(); // Clean up
        }
        
        saveConfig();
    }
}

QString ConfigManager::getGlobalConfigPath() const {
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir dir(tempPath);
    if (!dir.exists("APE-HOI4-Tool-Studio")) {
        dir.mkdir("APE-HOI4-Tool-Studio");
    }
    return dir.filePath("APE-HOI4-Tool-Studio/config.json");
}

QString ConfigManager::getModConfigPath() const {
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir dir(tempPath);
    if (!dir.exists("APE-HOI4-Tool-Studio")) {
        dir.mkdir("APE-HOI4-Tool-Studio");
    }
    return dir.filePath("APE-HOI4-Tool-Studio/mod_config.json");
}

void ConfigManager::loadConfig() {
    QFile file(getGlobalConfigPath());
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        if (obj.contains("gamePath")) m_gamePath = obj["gamePath"].toString();
        if (obj.contains("language")) m_language = obj["language"].toString();
        if (obj.contains("theme")) m_theme = static_cast<Theme>(obj["theme"].toInt());
        if (obj.contains("debugMode")) m_debugMode = obj["debugMode"].toBool();
        if (obj.contains("sidebarCompact")) m_sidebarCompactMode = obj["sidebarCompact"].toBool();
        if (obj.contains("maxLogFiles")) m_maxLogFiles = obj["maxLogFiles"].toInt();
        file.close();
    }

    QFile modFile(getModConfigPath());
    if (modFile.open(QIODevice::ReadOnly)) {
        QByteArray data = modFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        if (obj.contains("modPath")) m_modPath = obj["modPath"].toString();
        modFile.close();
    }
}

void ConfigManager::saveConfig() {
    QJsonObject obj;
    obj["gamePath"] = m_gamePath;
    obj["language"] = m_language;
    obj["theme"] = static_cast<int>(m_theme);
    obj["debugMode"] = m_debugMode;
    obj["sidebarCompact"] = m_sidebarCompactMode;
    obj["maxLogFiles"] = m_maxLogFiles;

    QJsonDocument doc(obj);
    QFile file(getGlobalConfigPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
    }
}

void ConfigManager::saveModConfig() {
    QJsonObject obj;
    obj["modPath"] = m_modPath;

    QJsonDocument doc(obj);
    QFile file(getModConfigPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
    }
}

QString ConfigManager::getGamePath() const { return m_gamePath; }
void ConfigManager::setGamePath(const QString& path) { 
    if (m_gamePath != path) {
        Logger::instance().logInfo("Config", "Game path changed to: " + path);
        m_gamePath = path; 
        saveConfig(); 
    }
}

QString ConfigManager::getLanguage() const { return m_language; }
void ConfigManager::setLanguage(const QString& lang) { 
    if (m_language != lang) {
        Logger::instance().logInfo("Config", "Language changed to: " + lang);
        m_language = lang; 
        saveConfig(); 
        emit languageChanged(lang);
    }
}

ConfigManager::Theme ConfigManager::getTheme() const { return m_theme; }
void ConfigManager::setTheme(Theme theme) { 
    if (m_theme != theme) {
        m_theme = theme; 
        saveConfig(); 
        emit themeChanged(theme);
    }
}

bool ConfigManager::getDebugMode() const { return m_debugMode; }
void ConfigManager::setDebugMode(bool enabled) { m_debugMode = enabled; saveConfig(); }

bool ConfigManager::getSidebarCompactMode() const { return m_sidebarCompactMode; }
void ConfigManager::setSidebarCompactMode(bool enabled) { m_sidebarCompactMode = enabled; saveConfig(); }

int ConfigManager::getMaxLogFiles() const { return m_maxLogFiles; }
void ConfigManager::setMaxLogFiles(int count) { m_maxLogFiles = count; saveConfig(); }

QString ConfigManager::getModPath() const { return m_modPath; }
void ConfigManager::setModPath(const QString& path) { 
    if (m_modPath != path) {
        Logger::instance().logInfo("Config", "Mod path changed to: " + path);
        m_modPath = path; 
        saveModConfig(); 
    }
}

void ConfigManager::clearModPath() {
    m_modPath = "";
    saveModConfig();
}

void ConfigManager::clearGamePath() {
    m_gamePath = "";
    saveConfig();
}

bool ConfigManager::isFirstRun() const {
    return m_gamePath.isEmpty();
}

bool ConfigManager::hasModSelected() const {
    return !m_modPath.isEmpty();
}

bool ConfigManager::isSystemDarkTheme() const {
    if (qApp) {
        return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }
    return false;
}

bool ConfigManager::isCurrentThemeDark() const {
    if (m_theme == Theme::Dark) {
        return true;
    } else if (m_theme == Theme::System) {
        return isSystemDarkTheme();
    }
    return false;
}

QJsonObject ConfigManager::toJson() const {
    QJsonObject obj;
    obj["gamePath"] = m_gamePath;
    obj["modPath"] = m_modPath;
    obj["language"] = m_language;
    obj["theme"] = static_cast<int>(m_theme);
    obj["debugMode"] = m_debugMode;
    return obj;
}

void ConfigManager::setFromJson(const QJsonObject& obj) {
    if (obj.contains("gamePath")) m_gamePath = obj["gamePath"].toString();
    if (obj.contains("modPath")) m_modPath = obj["modPath"].toString();
    if (obj.contains("language")) m_language = obj["language"].toString();
    if (obj.contains("theme")) m_theme = static_cast<Theme>(obj["theme"].toInt());
    if (obj.contains("debugMode")) m_debugMode = obj["debugMode"].toBool();
    
    Logger::instance().logInfo("ConfigManager", "Loaded config from IPC data");
}

QString ConfigManager::getComboBoxItemStyle(bool isDark) {
    QString text = isDark ? "#FFFFFF" : "#1D1D1F";
    QString itemHover = isDark ? "#3A3A3C" : "rgba(0, 0, 0, 0.05)";
    QString comboIndicator = isDark ? "#FFFFFF" : "#1D1D1F";
    
    return QString(R"(
        QComboBox QAbstractItemView::item {
            padding: 6px 12px;
            border-left: 3px solid transparent;
            color: %1;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: %2;
            border-left: 3px solid %3;
            color: %1;
        }
    )").arg(text, itemHover, comboIndicator);
}
