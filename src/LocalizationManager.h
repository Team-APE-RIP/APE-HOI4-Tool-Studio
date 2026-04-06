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

#include <QMap>
#include <QString>
#include <QStringList>

class LocalizationManager {
public:
    static LocalizationManager& instance();

    void loadLanguage(const QString& langCode);
    QString getString(const QString& category, const QString& key) const;
    QString currentLang() const;

    QStringList availableLanguageCodes() const;
    QStringList availableLanguageDisplayNames() const;
    QString displayNameForLanguage(const QString& langCode) const;
    QString normalizeLanguageCode(const QString& value) const;

private:
    LocalizationManager();

    void ensureLanguageMetadataLoaded();
    void reloadLanguageMetadata();
    void loadLanguageIntoMap(const QString& langCode, QMap<QString, QMap<QString, QString>>& targetMap) const;
    void loadLanguageCategoryFile(const QString& langCode,
                                  const QString& fileName,
                                  QMap<QString, QMap<QString, QString>>& targetMap) const;

    QMap<QString, QString> parseMetaFile(const QString& path) const;
    QMap<QString, QString> parseSimpleYamlFile(const QString& path) const;

    QMap<QString, QMap<QString, QString>> m_translations;
    QString m_currentLang;
    QMap<QString, QString> m_languageTextByCode;
    QMap<QString, QString> m_languageCodeByText;
    bool m_languageMetadataLoaded = false;
};

#endif // LOCALIZATIONMANAGER_H