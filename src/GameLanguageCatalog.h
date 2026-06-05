//-------------------------------------------------------------------------------------
// GameLanguageCatalog.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef GAMELANGUAGECATALOG_H
#define GAMELANGUAGECATALOG_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

class GameLanguageCatalog {
public:
    struct LanguageOption {
        QString code;
        QString displayName;
    };

    using LanguageSection = QMap<QString, QString>;
    using LanguageSections = QMap<QString, LanguageSection>;

    static QString defaultLanguageCode();
    static QString normalizeLanguageCode(const QString& value);
    static LanguageSections parseLanguageSections(const QString& content);
    static QList<LanguageOption> optionsFromEnglishSection(const LanguageSections& sections);
    static QString localizedLanguageName(const LanguageSections& sections,
                                         const QString& targetLanguageCode,
                                         const QString& selectedGameLanguageCode);
    static QStringList localizedLanguageNames(const LanguageSections& sections,
                                              const QStringList& targetLanguageCodes,
                                              const QString& selectedGameLanguageCode);

private:
    GameLanguageCatalog() = delete;
};

#endif // GAMELANGUAGECATALOG_H
