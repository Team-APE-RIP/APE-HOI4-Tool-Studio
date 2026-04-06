//-------------------------------------------------------------------------------------
// LocalizationManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "LocalizationManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace {
const QString kDefaultLanguageCode = QStringLiteral("en_US");

QString getLocalisationRootPath() {
    QDir appDir(QCoreApplication::applicationDirPath());
    if (appDir.exists("localisation")) {
        return appDir.filePath("localisation");
    }
    if (QDir(appDir.filePath("../localisation")).exists()) {
        return appDir.filePath("../localisation");
    }
    return appDir.filePath("localisation");
}

QString unquoteValue(QString value) {
    value = value.trimmed();
    if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
        value = value.mid(1, value.size() - 2);
    }
    value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    return value;
}
}

LocalizationManager& LocalizationManager::instance() {
    static LocalizationManager instance;
    return instance;
}

LocalizationManager::LocalizationManager()
    : m_currentLang(kDefaultLanguageCode) {
}

QString LocalizationManager::normalizeLanguageCode(const QString& value) const {
    const QString trimmed = value.trimmed();
    if (m_languageCodeByText.contains(trimmed)) {
        return m_languageCodeByText.value(trimmed);
    }
    if (m_languageTextByCode.contains(trimmed)) {
        return trimmed;
    }
    
    return kDefaultLanguageCode;
}

void LocalizationManager::ensureLanguageMetadataLoaded() {
    if (!m_languageMetadataLoaded) {
        reloadLanguageMetadata();
    }
}

void LocalizationManager::reloadLanguageMetadata() {
    m_languageTextByCode.clear();
    m_languageCodeByText.clear();

    QDir rootDir(getLocalisationRootPath());
    const QStringList languageDirectories =
        rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString& directoryName : languageDirectories) {
        const QString metaPath = rootDir.filePath(directoryName + "/meta.htsl");
        const QMap<QString, QString> meta = parseMetaFile(metaPath);

        const QString langCode = meta.value("lang").trimmed().isEmpty()
            ? directoryName
            : meta.value("lang").trimmed();
        const QString displayText = meta.value("text").trimmed().isEmpty()
            ? langCode
            : meta.value("text").trimmed();

        m_languageTextByCode.insert(langCode, displayText);
        m_languageCodeByText.insert(displayText, langCode);
    }

    if (!m_languageTextByCode.contains(kDefaultLanguageCode)) {
        m_languageTextByCode.insert(kDefaultLanguageCode, QStringLiteral("English"));
        m_languageCodeByText.insert(QStringLiteral("English"), kDefaultLanguageCode);
    }

    m_languageMetadataLoaded = true;
}

QStringList LocalizationManager::availableLanguageCodes() const {
    const_cast<LocalizationManager*>(this)->ensureLanguageMetadataLoaded();
    return m_languageTextByCode.keys();
}

QStringList LocalizationManager::availableLanguageDisplayNames() const {
    const_cast<LocalizationManager*>(this)->ensureLanguageMetadataLoaded();

    QStringList displayNames;
    for (auto it = m_languageTextByCode.begin(); it != m_languageTextByCode.end(); ++it) {
        displayNames.append(it.value());
    }
    return displayNames;
}

QString LocalizationManager::displayNameForLanguage(const QString& langCode) const {
    const_cast<LocalizationManager*>(this)->ensureLanguageMetadataLoaded();
    const QString normalized = normalizeLanguageCode(langCode);
    return m_languageTextByCode.value(normalized, m_languageTextByCode.value(kDefaultLanguageCode, QStringLiteral("English")));
}

QMap<QString, QString> LocalizationManager::parseMetaFile(const QString& path) const {
    QMap<QString, QString> result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        const int separatorIndex = line.indexOf('=');
        if (separatorIndex <= 0) {
            continue;
        }

        const QString key = line.left(separatorIndex).trimmed();
        const QString value = unquoteValue(line.mid(separatorIndex + 1));
        result.insert(key, value);
    }

    return result;
}

QMap<QString, QString> LocalizationManager::parseSimpleYamlFile(const QString& path) const {
    QMap<QString, QString> result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream stream(&file);
    bool insideLanguageRoot = false;

    while (!stream.atEnd()) {
        const QString rawLine = stream.readLine();
        const QString trimmed = rawLine.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            continue;
        }

        if (!insideLanguageRoot) {
            if (trimmed.startsWith("l_") && trimmed.endsWith(':')) {
                insideLanguageRoot = true;
            }
            continue;
        }

        if (!rawLine.startsWith(' ') && !rawLine.startsWith('\t')) {
            continue;
        }

        const int separatorIndex = trimmed.indexOf(':');
        if (separatorIndex <= 0) {
            continue;
        }

        const QString key = trimmed.left(separatorIndex).trimmed();
        const QString value = unquoteValue(trimmed.mid(separatorIndex + 1));
        result.insert(key, value);
    }

    return result;
}

void LocalizationManager::loadLanguageCategoryFile(const QString& langCode,
                                                   const QString& fileName,
                                                   QMap<QString, QMap<QString, QString>>& targetMap) const {
    const QString category = QFileInfo(fileName).completeBaseName();
    const QString filePath = QString("%1/%2/%3").arg(getLocalisationRootPath(), langCode, fileName);
    const QMap<QString, QString> values = parseSimpleYamlFile(filePath);
    if (values.isEmpty()) {
        return;
    }

    QMap<QString, QString>& categoryMap = targetMap[category];
    for (auto it = values.begin(); it != values.end(); ++it) {
        categoryMap[it.key()] = it.value();
    }
}

void LocalizationManager::loadLanguageIntoMap(const QString& langCode,
                                              QMap<QString, QMap<QString, QString>>& targetMap) const {
    QDir directory(QString("%1/%2").arg(getLocalisationRootPath(), langCode));
    const QStringList files = directory.entryList(QStringList() << "*.yml", QDir::Files, QDir::Name);

    for (const QString& fileName : files) {
        loadLanguageCategoryFile(langCode, fileName, targetMap);
    }
}

void LocalizationManager::loadLanguage(const QString& langCode) {
    ensureLanguageMetadataLoaded();

    const QString normalized = normalizeLanguageCode(langCode);
    const QString selectedCode = m_languageTextByCode.contains(normalized) ? normalized : kDefaultLanguageCode;

    m_currentLang = selectedCode;
    m_translations.clear();

    loadLanguageIntoMap(kDefaultLanguageCode, m_translations);
    if (selectedCode != kDefaultLanguageCode) {
        loadLanguageIntoMap(selectedCode, m_translations);
    }
}

QString LocalizationManager::currentLang() const {
    return normalizeLanguageCode(m_currentLang);
}

QString LocalizationManager::getString(const QString& category, const QString& key) const {
    if (m_translations.contains(category) && m_translations.value(category).contains(key)) {
        return m_translations.value(category).value(key);
    }
    return key;
}