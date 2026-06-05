//-------------------------------------------------------------------------------------
// ConfigManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ConfigManager.h"
#include "GameLanguageCatalog.h"
#include "Logger.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>

namespace {
const char* kRegistryOrganization = "Team-APE-RIP";
const char* kRegistryApplication = "APE-HOI4-Tool-Studio";

const char* kConfigGroup = "Config";
const char* kPathGroup = "Path";

const char* kConfigGamePath = "Config/GamePath";
const char* kConfigModPath = "Config/ModPath";
const char* kConfigDocPath = "Config/DocPath";
const char* kConfigLanguage = "Config/Language";
const char* kConfigGameLanguage = "Config/GameLanguage";
const char* kConfigTheme = "Config/Theme";
const char* kConfigDebugMode = "Config/DebugMode";
const char* kConfigSidebarCompact = "Config/SidebarCompact";
const char* kConfigDisplayMode = "Config/DisplayMode";
const char* kConfigDisplayScreen = "Config/DisplayScreen";
const char* kConfigDisplayResolutionWidth = "Config/DisplayResolutionWidth";
const char* kConfigDisplayResolutionHeight = "Config/DisplayResolutionHeight";
const char* kConfigMaxLogFiles = "Config/MaxLogFiles";

const char* kPathInstallPath = "Path/InstallPath";
const char* kPathAutoSetup = "Path/AutoSetup";

QString readJsonString(const QJsonObject& object, const QString& key) {
    return object.value(key).toString().trimmed();
}

QJsonObject buildGameLanguageNamesObject(const QString& gamePath, const QString& selectedGameLanguage) {
    QJsonObject object;
    QFile file(QDir(gamePath).filePath(QStringLiteral("localisation/languages.yml")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        object[QStringLiteral("l_english")] = QStringLiteral("English");
        return object;
    }

    const GameLanguageCatalog::LanguageSections sections =
        GameLanguageCatalog::parseLanguageSections(QString::fromUtf8(file.readAll()));
    const QList<GameLanguageCatalog::LanguageOption> options =
        GameLanguageCatalog::optionsFromEnglishSection(sections);
    for (const GameLanguageCatalog::LanguageOption& option : options) {
        object[option.code] = GameLanguageCatalog::localizedLanguageName(
            sections,
            option.code,
            selectedGameLanguage);
    }
    return object;
}
}

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    loadDefaults();
    migrateLegacyConfigFiles();
    loadConfig();
    Logger::instance().setMaxLogFiles(m_maxLogFiles);
}

QSettings ConfigManager::createSettings() const {
    return QSettings(kRegistryOrganization, kRegistryApplication);
}

void ConfigManager::loadDefaults() {
    m_gamePath.clear();
    m_modPath.clear();
    m_language = "en_US";
    m_gameLanguage = "l_english";
    m_theme = Theme::System;
    m_debugMode = false;
    m_sidebarCompactMode = false;
    m_displayMode = DisplayMode::Window;
    m_displayScreenName.clear();
    m_displayResolution = QSize(1280, 720);
    m_maxLogFiles = 10;
}

QString ConfigManager::normalizeLanguageCode(const QString& value) const {
    const QString normalized = value.trimmed();
    if (normalized == "简体中文") {
        return "zh_CN";
    }
    if (normalized == "繁體中文") {
        return "zh_TW";
    }
    if (normalized == "English") {
        return "en_US";
    }
    if (normalized.isEmpty()) {
        return "en_US";
    }

    return normalized;
}

QString ConfigManager::normalizeGameLanguageCode(const QString& value) const {
    const QString normalized = value.trimmed();
    if (normalized.isEmpty()) {
        return QStringLiteral("l_english");
    }
    return normalized;
}

void ConfigManager::removeLegacyFile(const QString& filePath) const {
    if (filePath.isEmpty()) {
        return;
    }

    if (QFile::exists(filePath) && !QFile::remove(filePath)) {
        Logger::instance().logWarning("ConfigManager", "Failed to remove legacy file: " + filePath);
    }
}

void ConfigManager::migrateLegacyConfigFiles() {
    QSettings settings = createSettings();

    const QString tempRoot =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
    const QString setupCacheRoot = tempRoot + "/setup_cache";
    const QString configPath = tempRoot + "/config.json";
    const QString modConfigPath = tempRoot + "/mod_config.json";
    const QString pathPath = tempRoot + "/path.json";
    const QString tempLanguagePath = setupCacheRoot + "/temp_lang.json";

    bool shouldRemoveConfig = false;
    bool shouldRemoveModConfig = false;
    bool shouldRemovePath = false;
    bool shouldRemoveTempLanguage = false;

    QFile configFile(configPath);
    if (configFile.exists() && configFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(configFile.readAll());
        const QJsonObject object = document.object();

        if (!settings.contains(kConfigGamePath) && object.contains("gamePath")) {
            settings.setValue(kConfigGamePath, readJsonString(object, "gamePath"));
        }
        if (!settings.contains(kConfigLanguage) && object.contains("language")) {
            settings.setValue(kConfigLanguage, normalizeLanguageCode(readJsonString(object, "language")));
        }
        if (!settings.contains(kConfigTheme) && object.contains("theme")) {
            settings.setValue(kConfigTheme, object.value("theme").toInt(static_cast<int>(Theme::System)));
        }
        if (!settings.contains(kConfigDebugMode) && object.contains("debugMode")) {
            settings.setValue(kConfigDebugMode, object.value("debugMode").toBool(false));
        }
        if (!settings.contains(kConfigSidebarCompact) && object.contains("sidebarCompact")) {
            settings.setValue(kConfigSidebarCompact, object.value("sidebarCompact").toBool(false));
        }
        if (!settings.contains(kConfigMaxLogFiles) && object.contains("maxLogFiles")) {
            settings.setValue(kConfigMaxLogFiles, object.value("maxLogFiles").toInt(10));
        }
        shouldRemoveConfig = true;
        configFile.close();
    }

    QFile modConfigFile(modConfigPath);
    if (modConfigFile.exists() && modConfigFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(modConfigFile.readAll());
        const QJsonObject object = document.object();

        if (!settings.contains(kConfigModPath) && object.contains("modPath")) {
            settings.setValue(kConfigModPath, readJsonString(object, "modPath"));
        }

        shouldRemoveModConfig = true;
        modConfigFile.close();
    }

    QFile pathFile(pathPath);
    if (pathFile.exists() && pathFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(pathFile.readAll());
        const QJsonObject object = document.object();

        if (!settings.contains(kPathInstallPath) && object.contains("path")) {
            settings.setValue(kPathInstallPath, readJsonString(object, "path"));
        }
        if (!settings.contains(kPathAutoSetup) && object.contains("auto")) {
            const QString autoValue = readJsonString(object, "auto");
            settings.setValue(kPathAutoSetup, autoValue == "1");
        }

        shouldRemovePath = true;
        pathFile.close();
    }

    QFile tempLanguageFile(tempLanguagePath);
    if (tempLanguageFile.exists() && tempLanguageFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(tempLanguageFile.readAll());
        const QJsonObject object = document.object();

        if (!settings.contains(kConfigLanguage) && object.contains("language")) {
            settings.setValue(kConfigLanguage, normalizeLanguageCode(readJsonString(object, "language")));
        }

        shouldRemoveTempLanguage = true;
        tempLanguageFile.close();
    }

    if (!settings.contains(kConfigLanguage)) {
        settings.setValue(kConfigLanguage, QString("en_US"));
    }
    if (!settings.contains(kConfigGameLanguage)) {
        settings.setValue(kConfigGameLanguage, QStringLiteral("l_english"));
    }
    if (settings.contains(kConfigDocPath)) {
        settings.remove(kConfigDocPath);
    }

    if (shouldRemoveConfig) {
        removeLegacyFile(configPath);
    }
    if (shouldRemoveModConfig) {
        removeLegacyFile(modConfigPath);
    }
    if (shouldRemovePath) {
        removeLegacyFile(pathPath);
    }
    if (shouldRemoveTempLanguage) {
        removeLegacyFile(tempLanguagePath);
    }
}

void ConfigManager::loadConfig() {
    loadDefaults();

    QSettings settings = createSettings();
    m_gamePath = settings.value(kConfigGamePath, "").toString().trimmed();
    m_modPath = settings.value(kConfigModPath, "").toString().trimmed();
    if (settings.contains(kConfigDocPath)) {
        settings.remove(kConfigDocPath);
    }
    m_language = normalizeLanguageCode(settings.value(kConfigLanguage, "en_US").toString());
    m_gameLanguage = normalizeGameLanguageCode(settings.value(kConfigGameLanguage, "l_english").toString());
    m_theme = static_cast<Theme>(settings.value(kConfigTheme, static_cast<int>(Theme::System)).toInt());
    m_debugMode = settings.value(kConfigDebugMode, false).toBool();
    m_sidebarCompactMode = settings.value(kConfigSidebarCompact, false).toBool();
    m_displayMode = static_cast<DisplayMode>(
        settings.value(kConfigDisplayMode, static_cast<int>(DisplayMode::Window)).toInt());
    if (m_displayMode != DisplayMode::Window && m_displayMode != DisplayMode::Fullscreen) {
        m_displayMode = DisplayMode::Window;
    }
    m_displayScreenName = settings.value(kConfigDisplayScreen, "").toString().trimmed();
    const int displayWidth = settings.value(kConfigDisplayResolutionWidth, 1280).toInt();
    const int displayHeight = settings.value(kConfigDisplayResolutionHeight, 720).toInt();
    m_displayResolution = QSize(displayWidth, displayHeight);
    if (!m_displayResolution.isValid() || m_displayResolution.width() < 1280 || m_displayResolution.height() < 720) {
        m_displayResolution = QSize(1280, 720);
    }
    m_maxLogFiles = settings.value(kConfigMaxLogFiles, 10).toInt();

    Logger::instance().setMaxLogFiles(m_maxLogFiles);
}

void ConfigManager::saveConfig() {
    QSettings settings = createSettings();
    settings.setValue(kConfigGamePath, m_gamePath);
    settings.setValue(kConfigModPath, m_modPath);
    settings.remove(kConfigDocPath);
    settings.setValue(kConfigLanguage, normalizeLanguageCode(m_language));
    settings.setValue(kConfigGameLanguage, normalizeGameLanguageCode(m_gameLanguage));
    settings.setValue(kConfigTheme, static_cast<int>(m_theme));
    settings.setValue(kConfigDebugMode, m_debugMode);
    settings.setValue(kConfigSidebarCompact, m_sidebarCompactMode);
    settings.setValue(kConfigDisplayMode, static_cast<int>(m_displayMode));
    settings.setValue(kConfigDisplayScreen, m_displayScreenName);
    settings.setValue(kConfigDisplayResolutionWidth, m_displayResolution.width());
    settings.setValue(kConfigDisplayResolutionHeight, m_displayResolution.height());
    settings.setValue(kConfigMaxLogFiles, m_maxLogFiles);
}

void ConfigManager::saveModConfig() {
    saveConfig();
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
    const QString normalized = normalizeLanguageCode(lang);
    if (m_language != normalized) {
        Logger::instance().logInfo("Config", "Language changed to: " + normalized);
        m_language = normalized;
        saveConfig();
        emit languageChanged(normalized);
    }
}

QString ConfigManager::getGameLanguage() const { return m_gameLanguage; }

void ConfigManager::setGameLanguage(const QString& lang) {
    const QString normalized = normalizeGameLanguageCode(lang);
    if (m_gameLanguage != normalized) {
        Logger::instance().logInfo("Config", "Game language changed to: " + normalized);
        m_gameLanguage = normalized;
        saveConfig();
        emit gameLanguageChanged(normalized);
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

void ConfigManager::setDebugMode(bool enabled) {
    m_debugMode = enabled;
    saveConfig();
}

bool ConfigManager::getSidebarCompactMode() const { return m_sidebarCompactMode; }

void ConfigManager::setSidebarCompactMode(bool enabled) {
    m_sidebarCompactMode = enabled;
    saveConfig();
}

ConfigManager::DisplayMode ConfigManager::getDisplayMode() const { return m_displayMode; }

void ConfigManager::setDisplayMode(DisplayMode mode) {
    if (mode != DisplayMode::Window && mode != DisplayMode::Fullscreen) {
        mode = DisplayMode::Window;
    }

    if (m_displayMode != mode) {
        m_displayMode = mode;
        saveConfig();
    }
}

QString ConfigManager::getDisplayScreenName() const { return m_displayScreenName; }

void ConfigManager::setDisplayScreenName(const QString& screenName) {
    const QString normalized = screenName.trimmed();
    if (m_displayScreenName != normalized) {
        m_displayScreenName = normalized;
        saveConfig();
    }
}

QSize ConfigManager::getDisplayResolution() const { return m_displayResolution; }

void ConfigManager::setDisplayResolution(const QSize& resolution) {
    QSize normalized = resolution;
    if (!normalized.isValid() || normalized.width() < 1280 || normalized.height() < 720) {
        normalized = QSize(1280, 720);
    }

    if (m_displayResolution != normalized) {
        m_displayResolution = normalized;
        saveConfig();
    }
}

int ConfigManager::getMaxLogFiles() const { return m_maxLogFiles; }

void ConfigManager::setMaxLogFiles(int count) {
    m_maxLogFiles = count;
    Logger::instance().setMaxLogFiles(count);
    saveConfig();
}

QString ConfigManager::getModPath() const { return m_modPath; }

void ConfigManager::setModPath(const QString& path) {
    if (m_modPath != path) {
        Logger::instance().logInfo("Config", "Mod path changed to: " + path);
        m_modPath = path;
        saveModConfig();
    }
}

void ConfigManager::clearModPath() {
    m_modPath.clear();
    saveModConfig();
}

void ConfigManager::clearGamePath() {
    m_gamePath.clear();
    saveConfig();
}

QString ConfigManager::getDocPath() const {
    const QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).trimmed();
    if (documentsPath.isEmpty()) {
        return QString();
    }

    return QDir(QDir(documentsPath).filePath(QStringLiteral("Paradox Interactive")))
        .filePath(QStringLiteral("Hearts of Iron IV"));
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
    }
    if (m_theme == Theme::System) {
        return isSystemDarkTheme();
    }
    return false;
}

QJsonObject ConfigManager::toJson() const {
    QJsonObject obj;
    obj["language"] = normalizeLanguageCode(m_language);
    obj["gameLanguage"] = normalizeGameLanguageCode(m_gameLanguage);
    obj["gameLanguageNames"] = buildGameLanguageNamesObject(m_gamePath, normalizeGameLanguageCode(m_gameLanguage));
    obj["theme"] = static_cast<int>(m_theme);
    obj["debugMode"] = m_debugMode;
    obj["maxLogFiles"] = m_maxLogFiles;
    return obj;
}

void ConfigManager::setFromJson(const QJsonObject& obj) {
    if (obj.contains("language")) m_language = normalizeLanguageCode(obj["language"].toString());
    if (obj.contains("gameLanguage")) m_gameLanguage = normalizeGameLanguageCode(obj["gameLanguage"].toString());
    if (obj.contains("theme")) m_theme = static_cast<Theme>(obj["theme"].toInt());
    if (obj.contains("debugMode")) m_debugMode = obj["debugMode"].toBool();
    if (obj.contains("maxLogFiles")) m_maxLogFiles = obj["maxLogFiles"].toInt();

    Logger::instance().setMaxLogFiles(m_maxLogFiles);
    Logger::instance().logInfo("ConfigManager", "Loaded sanitized config from IPC data");
}

QString ConfigManager::getComboBoxItemStyle(bool isDark) {
    QString text = isDark ? "#FFFFFF" : "#1D1D1F";
    QString itemHover = isDark ? "#3A3A3C" : "rgba(0, 0, 0, 0.05)";

    return QString(R"(
        QComboBox QAbstractItemView::item {
            padding: 6px 0px;
            color: %1;
            text-align: center;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: %2;
            color: %1;
        }
    )").arg(text, itemHover);
}
