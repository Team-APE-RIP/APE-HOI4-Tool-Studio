#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QDir>

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

    bool isFirstRun() const;
    bool hasModSelected() const;
    
    // Serialization for IPC
    QJsonObject toJson() const;
    void setFromJson(const QJsonObject& obj); // For ToolHost to initialize from IPC data

signals:
    void themeChanged(Theme theme);
    void languageChanged(const QString& lang);

private:
    ConfigManager();
    QString getGlobalConfigPath() const;
    QString getModConfigPath() const;

    QString m_gamePath;
    QString m_language;
    Theme m_theme;
    bool m_debugMode;
    bool m_sidebarCompactMode;
    int m_maxLogFiles;

    QString m_modPath;
};

#endif // CONFIGMANAGER_H
