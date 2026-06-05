//-------------------------------------------------------------------------------------
// GameLanguageCatalog.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "GameLanguageCatalog.h"

#include <algorithm>

namespace {

QString removeComment(QString line) {
    bool insideSingleQuote = false;
    bool insideDoubleQuote = false;
    bool escaped = false;

    for (int index = 0; index < line.size(); ++index) {
        const QChar ch = line.at(index);
        if (escaped) {
            escaped = false;
            continue;
        }
        if (ch == QLatin1Char('\\') && insideDoubleQuote) {
            escaped = true;
            continue;
        }
        if (ch == QLatin1Char('\'') && !insideDoubleQuote) {
            insideSingleQuote = !insideSingleQuote;
            continue;
        }
        if (ch == QLatin1Char('"') && !insideSingleQuote) {
            insideDoubleQuote = !insideDoubleQuote;
            continue;
        }
        if (ch == QLatin1Char('#') && !insideSingleQuote && !insideDoubleQuote) {
            return line.left(index);
        }
    }

    return line;
}

QString unquoteValue(QString value) {
    value = value.trimmed();
    while (!value.isEmpty() && value.front().isDigit()) {
        value.remove(0, 1);
    }
    value = value.trimmed();

    if (value.size() >= 2) {
        const QChar first = value.front();
        const QChar last = value.back();
        if ((first == QLatin1Char('"') && last == QLatin1Char('"'))
            || (first == QLatin1Char('\'') && last == QLatin1Char('\''))) {
            value = value.mid(1, value.size() - 2);
        }
    }

    QString result;
    result.reserve(value.size());
    bool escaped = false;
    for (const QChar ch : value) {
        if (escaped) {
            if (ch == QLatin1Char('n')) {
                result.append(QLatin1Char('\n'));
            } else if (ch == QLatin1Char('t')) {
                result.append(QLatin1Char('\t'));
            } else {
                result.append(ch);
            }
            escaped = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            escaped = true;
            continue;
        }
        result.append(ch);
    }
    if (escaped) {
        result.append(QLatin1Char('\\'));
    }
    return result;
}

bool startsIndented(const QString& line) {
    return !line.isEmpty() && (line.front().isSpace());
}

bool isLanguageRoot(const QString& rawLine, const QString& trimmedLine) {
    return !startsIndented(rawLine)
        && trimmedLine.startsWith(QStringLiteral("l_"))
        && trimmedLine.endsWith(QLatin1Char(':'))
        && trimmedLine.indexOf(QLatin1Char(' ')) < 0
        && trimmedLine.indexOf(QLatin1Char('\t')) < 0;
}

QString fallbackDisplayName(const QString& code) {
    const QString normalized = GameLanguageCatalog::normalizeLanguageCode(code);
    if (normalized == QStringLiteral("l_english")) {
        return QStringLiteral("English");
    }
    return normalized;
}

} // namespace

QString GameLanguageCatalog::defaultLanguageCode() {
    return QStringLiteral("l_english");
}

QString GameLanguageCatalog::normalizeLanguageCode(const QString& value) {
    const QString normalized = value.trimmed();
    return normalized.isEmpty() ? defaultLanguageCode() : normalized;
}

GameLanguageCatalog::LanguageSections GameLanguageCatalog::parseLanguageSections(const QString& content) {
    LanguageSections sections;
    QString currentRoot;

    const QStringList lines = content.split(QLatin1Char('\n'));
    for (QString rawLine : lines) {
        if (!rawLine.isEmpty() && rawLine.back() == QLatin1Char('\r')) {
            rawLine.chop(1);
        }

        const QString uncommented = removeComment(rawLine);
        const QString trimmed = uncommented.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        if (isLanguageRoot(rawLine, trimmed)) {
            currentRoot = normalizeLanguageCode(trimmed.left(trimmed.size() - 1));
            if (!sections.contains(currentRoot)) {
                sections.insert(currentRoot, LanguageSection());
            }
            continue;
        }

        if (currentRoot.isEmpty() || !startsIndented(rawLine)) {
            continue;
        }

        const int separatorIndex = trimmed.indexOf(QLatin1Char(':'));
        if (separatorIndex <= 0) {
            continue;
        }

        const QString key = normalizeLanguageCode(trimmed.left(separatorIndex));
        const QString value = unquoteValue(trimmed.mid(separatorIndex + 1));
        if (!key.isEmpty() && !value.isEmpty()) {
            sections[currentRoot].insert(key, value);
        }
    }

    return sections;
}

QList<GameLanguageCatalog::LanguageOption> GameLanguageCatalog::optionsFromEnglishSection(const LanguageSections& sections) {
    QList<LanguageOption> options;
    const LanguageSection englishSection = sections.value(defaultLanguageCode());
    for (auto it = englishSection.constBegin(); it != englishSection.constEnd(); ++it) {
        options.append(LanguageOption{it.key(), it.value()});
    }

    std::sort(options.begin(), options.end(), [](const LanguageOption& left, const LanguageOption& right) {
        return left.displayName.localeAwareCompare(right.displayName) < 0;
    });

    if (options.isEmpty()) {
        options.append(LanguageOption{defaultLanguageCode(), fallbackDisplayName(defaultLanguageCode())});
    }
    return options;
}

QString GameLanguageCatalog::localizedLanguageName(const LanguageSections& sections,
                                                   const QString& targetLanguageCode,
                                                   const QString& selectedGameLanguageCode) {
    const QString targetCode = normalizeLanguageCode(targetLanguageCode);
    const QString selectedCode = normalizeLanguageCode(selectedGameLanguageCode);

    const LanguageSection selectedSection = sections.value(selectedCode);
    const QString selectedName = selectedSection.value(targetCode).trimmed();
    if (!selectedName.isEmpty()) {
        return selectedName;
    }

    const LanguageSection englishSection = sections.value(defaultLanguageCode());
    const QString englishName = englishSection.value(targetCode).trimmed();
    if (!englishName.isEmpty()) {
        return englishName;
    }

    return fallbackDisplayName(targetCode);
}

QStringList GameLanguageCatalog::localizedLanguageNames(const LanguageSections& sections,
                                                        const QStringList& targetLanguageCodes,
                                                        const QString& selectedGameLanguageCode) {
    QStringList names;
    for (const QString& code : targetLanguageCodes) {
        const QString normalized = normalizeLanguageCode(code);
        if (normalized.isEmpty()) {
            continue;
        }
        names.append(localizedLanguageName(sections, normalized, selectedGameLanguageCode));
    }
    names.removeDuplicates();
    return names;
}
