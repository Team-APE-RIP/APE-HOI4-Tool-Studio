#include "LocalizationManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>

LocalizationManager& LocalizationManager::instance() {
    static LocalizationManager instance;
    return instance;
}

LocalizationManager::LocalizationManager() {
}

void LocalizationManager::loadLanguage(const QString& langCode) {
    m_currentLang = langCode;
    m_translations.clear();

    // Map langCode to folder name
    QString folderName;
    if (langCode == "English") folderName = "en_US";
    else if (langCode == "简体中文") folderName = "zh_CN";
    else if (langCode == "繁體中文") folderName = "zh_TW";
    else folderName = "en_US"; // Default

    // Load all json files in the folder
    // Since we are using resources, we can't easily iterate unless we know filenames or use QDir iterator on resource system
    // But QDir iterator works on resources too.
    
    QDir dir(":/localization/" + folderName);
    QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files);
    
    for (const QString& file : files) {
        QString category = file.section('.', 0, 0); // e.g. "MainWindow.json" -> "MainWindow"
        loadFile(category, dir.filePath(file));
    }
}

void LocalizationManager::loadFile(const QString& category, const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open localization file:" << path;
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();

    QMap<QString, QString> categoryMap;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        categoryMap.insert(it.key(), it.value().toString());
    }
    m_translations.insert(category, categoryMap);
}

QString LocalizationManager::getString(const QString& category, const QString& key) const {
    if (m_translations.contains(category) && m_translations[category].contains(key)) {
        return m_translations[category][key];
    }
    return key; // Fallback to key if not found
}
