//-------------------------------------------------------------------------------------
// ConfigManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class QSettings;

class ConfigManager : public QObject {
    Q_OBJECT

public:
    enum class Theme {
        System,
        Light,
        Dark
    };

    static ConfigManager& instance();

    void loadConfig();
    void saveConfig();
    void saveModConfig();

    // Global Settings
    QString getGamePath() const;
    void setGamePath(const QString& path);

    QString getLanguage() const;
    void setLanguage(const QString& lang);

    Theme getTheme() const;
    void setTheme(Theme theme);

    bool getDebugMode() const;
    void setDebugMode(bool enabled);

    bool getSidebarCompactMode() const;
    void setSidebarCompactMode(bool enabled);

    int getMaxLogFiles() const;
    void setMaxLogFiles(int count);

    // Mod Settings
    QString getModPath() const;
    void setModPath(const QString& path);
    void clearModPath();
    void clearGamePath();

    // Documents Settings
    QString getDocPath() const;
    void setDocPath(const QString& path);
    void clearDocPath();

    bool isFirstRun() const;
    bool hasModSelected() const;

    bool isSystemDarkTheme() const;
    bool isCurrentThemeDark() const;

    // Style helpers for tools
    static QString getComboBoxItemStyle(bool isDark);

    // Serialization for IPC
    QJsonObject toJson() const;
    void setFromJson(const QJsonObject& obj); // For ToolHost to initialize from IPC data

signals:
    void themeChanged(Theme theme);
    void languageChanged(const QString& lang);

private:
    ConfigManager();

    QSettings createSettings() const;
    void loadDefaults();
    void migrateLegacyConfigFiles();
    void removeLegacyFile(const QString& filePath) const;
    QString normalizeLanguageCode(const QString& value) const;

    QString m_gamePath;
    QString m_language;
    Theme m_theme;
    bool m_debugMode;
    bool m_sidebarCompactMode;
    int m_maxLogFiles;

    QString m_modPath;
    QString m_docPath;
};

#endif // CONFIGMANAGER_H