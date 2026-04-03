//-------------------------------------------------------------------------------------
// LocalizationManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LOCALIZATIONMANAGER_H
#define LOCALIZATIONMANAGER_H

#include <QString>
#include <QJsonObject>
#include <QMap>

class LocalizationManager {
public:
    static LocalizationManager& instance();

    void loadLanguage(const QString& langCode);
    QString getString(const QString& category, const QString& key) const;
    // Returns the current language locale code (e.g. "zh_CN", "en_US")
    QString currentLang() const;

private:
    LocalizationManager();
    void loadFile(const QString& category, const QString& path);

    // Map<Category, Map<Key, Value>>
    QMap<QString, QMap<QString, QString>> m_translations;
    QString m_currentLang;
};

#endif // LOCALIZATIONMANAGER_H
