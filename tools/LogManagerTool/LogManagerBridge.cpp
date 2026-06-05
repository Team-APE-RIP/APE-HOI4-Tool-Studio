//-------------------------------------------------------------------------------------
// LogManagerBridge.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "LogManagerBridge.h"

#include "../../src/ToolRuntimeContext.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QStringConverter>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <utility>

namespace LogManagerBridge {

std::unique_ptr<WorkerSession> g_legacySession;
std::string g_legacyLastError;
std::string g_legacySerializedState;

namespace {
using LogManager::CompareRow;
using LogManager::DirectoryEntry;
using LogManager::DirectoryListResult;
using LogManager::IFileSystem;
using LogManager::LoadingStateSnapshot;
using LogManager::LogEntry;
using LogManager::LogFileRecord;
using LogManager::SortMode;
using LogManager::StateSnapshot;
using LogManager::StatisticsSnapshot;
using LogManager::TextReadResult;

QString normalizeRuntimePath(QString value) {
    value.replace('\\', '/');
    value = QDir::cleanPath(value.trimmed());
    return value == QStringLiteral(".") ? QString() : value;
}

TextReadResult readSingleMatchingTextFile(const QString& relativePath) {
    TextReadResult result{};

    const QString normalizedPath = normalizeRuntimePath(relativePath);
    if (normalizedPath.isEmpty()) {
        return result;
    }

    const int separatorIndex = normalizedPath.lastIndexOf('/');
    const QString parentPath = separatorIndex >= 0
        ? normalizedPath.left(separatorIndex)
        : QString();
    const QString fileName = separatorIndex >= 0
        ? normalizedPath.mid(separatorIndex + 1)
        : normalizedPath;
    if (fileName.isEmpty()) {
        return result;
    }

    const ToolRuntimeContext::MatchingTextFilesResult matchingResult =
        ToolRuntimeContext::instance().readMatchingTextFiles(
            ToolRuntimeContext::FileRoot::Doc,
            parentPath,
            QStringLiteral("^%1$").arg(QRegularExpression::escape(fileName)),
            false
        );
    if (!matchingResult.success) {
        return result;
    }

    for (const ToolRuntimeContext::TextFileMatchEntry& entry : matchingResult.entries) {
        if (normalizeRuntimePath(entry.relativePath) != normalizedPath) {
            continue;
        }
        result.success = true;
        result.content = entry.content.toUtf8().toStdString();
        return result;
    }

    if (matchingResult.entries.size() == 1) {
        const ToolRuntimeContext::TextFileMatchEntry& entry = matchingResult.entries.first();
        if (entry.name == fileName) {
            result.success = true;
            result.content = entry.content.toUtf8().toStdString();
        }
    }

    return result;
}

class ToolRuntimeFileSystem final : public IFileSystem {
public:
    DirectoryListResult listDirectory(const std::string& relativePath, bool recursive) const override {
        const ToolRuntimeContext::DirectoryListResult runtimeResult =
            ToolRuntimeContext::instance().listDirectory(
                ToolRuntimeContext::FileRoot::Doc,
                QString::fromUtf8(relativePath.c_str()),
                recursive
            );

        DirectoryListResult result{};
        result.success = runtimeResult.success;
        if (!runtimeResult.success) {
            return result;
        }

        result.entries.reserve(static_cast<std::size_t>(runtimeResult.entries.size()));
        for (const ToolRuntimeContext::DirectoryEntry& runtimeEntry : runtimeResult.entries) {
            DirectoryEntry entry;
            entry.relativePath = runtimeEntry.relativePath.toUtf8().toStdString();
            entry.name = runtimeEntry.name.toUtf8().toStdString();
            entry.isDirectory = runtimeEntry.isDirectory;
            result.entries.push_back(std::move(entry));
        }
        return result;
    }

    TextReadResult readTextFile(const std::string& relativePath) const override {
        const QString requestedPath = QString::fromUtf8(relativePath.c_str());
        const ToolRuntimeContext::TextReadResult runtimeResult =
            ToolRuntimeContext::instance().readTextFile(
                ToolRuntimeContext::FileRoot::Doc,
                requestedPath
            );

        TextReadResult result{};
        result.success = runtimeResult.success;
        if (runtimeResult.success) {
            result.content = runtimeResult.content.toUtf8().toStdString();
            return result;
        }

        return readSingleMatchingTextFile(requestedPath);
    }
};

QMap<QString, QString> parseMetaFile(const QString& filePath) {
    QMap<QString, QString> parsed;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return parsed;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#endif

    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#') || line.startsWith(QStringLiteral("//"))) {
            continue;
        }

        const int equalsIndex = line.indexOf('=');
        if (equalsIndex <= 0) {
            continue;
        }

        const QString key = line.left(equalsIndex).trimmed();
        QString value = line.mid(equalsIndex + 1).trimmed();
        if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\''))) {
            value = value.mid(1, value.length() - 2);
        }
        parsed.insert(key, value);
    }

    return parsed;
}

QString resolveLanguageCode(const QString& localisationRoot, const QString& requestedValue) {
    const QString normalized = requestedValue.trimmed();
    QDir root(localisationRoot);

    if (!normalized.isEmpty() && root.exists(normalized)) {
        return normalized;
    }

    const QStringList languageDirectories = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& dirName : languageDirectories) {
        const QMap<QString, QString> meta = parseMetaFile(root.filePath(dirName + QStringLiteral("/meta.htsl")));
        if (normalized == meta.value(QStringLiteral("lang"))
            || normalized == meta.value(QStringLiteral("text"))) {
            return dirName;
        }
    }

    if (root.exists(QStringLiteral("en_US"))) {
        return QStringLiteral("en_US");
    }
    if (!languageDirectories.isEmpty()) {
        return languageDirectories.first();
    }
    return QStringLiteral("en_US");
}

QString localizedFallback(const QString& key) {
    static QMap<QString, QString> fallbacks;
    if (fallbacks.isEmpty()) {
        fallbacks.insert(QStringLiteral("Name"), QStringLiteral("Log Manager"));
        fallbacks.insert(QStringLiteral("Description"), QStringLiteral("Browse and compare HOI4 error logs."));
        fallbacks.insert(QStringLiteral("Refresh"), QStringLiteral("Refresh"));
        fallbacks.insert(QStringLiteral("SortByTime"), QStringLiteral("Sort by Time"));
        fallbacks.insert(QStringLiteral("SortByCategory"), QStringLiteral("Sort by Category"));
        fallbacks.insert(QStringLiteral("SearchPlaceholder"), QStringLiteral("Search logs"));
        fallbacks.insert(QStringLiteral("Latest"), QStringLiteral("Latest"));
        fallbacks.insert(QStringLiteral("LogFiles"), QStringLiteral("Log Files"));
        fallbacks.insert(QStringLiteral("ErrorLog"), QStringLiteral("Error Log"));
        fallbacks.insert(QStringLiteral("CompareMode"), QStringLiteral("Compare Mode"));
        fallbacks.insert(QStringLiteral("ColSystemTime"), QStringLiteral("Time"));
        fallbacks.insert(QStringLiteral("ColGameTime"), QStringLiteral("Date"));
        fallbacks.insert(QStringLiteral("ColCategory"), QStringLiteral("Category"));
        fallbacks.insert(QStringLiteral("ColMessage"), QStringLiteral("Message Preview"));
        fallbacks.insert(QStringLiteral("Compare"), QStringLiteral("Compare"));
        fallbacks.insert(QStringLiteral("StopCompare"), QStringLiteral("Stop Compare"));
        fallbacks.insert(QStringLiteral("Total"), QStringLiteral("Total"));
        fallbacks.insert(QStringLiteral("Filtered"), QStringLiteral("Filtered"));
        fallbacks.insert(QStringLiteral("NoFiles"), QStringLiteral("No log files found"));
        fallbacks.insert(QStringLiteral("NoEntries"), QStringLiteral("No entries to display"));
        fallbacks.insert(QStringLiteral("Loading"), QStringLiteral("Loading"));
        fallbacks.insert(QStringLiteral("NormalView"), QStringLiteral("Normal View"));
        fallbacks.insert(QStringLiteral("CurrentFile"), QStringLiteral("Current File"));
        fallbacks.insert(QStringLiteral("CompareFile"), QStringLiteral("Compare File"));
        fallbacks.insert(QStringLiteral("Ready"), QStringLiteral("Ready"));
    }
    return fallbacks.value(key, key);
}

bool loadLocalizedStringsFromJson(WorkerSession* session, const QJsonObject& localizedStringsObject) {
    if (!session || localizedStringsObject.isEmpty()) {
        return false;
    }

    session->localizedStrings.clear();
    for (auto iterator = localizedStringsObject.begin(); iterator != localizedStringsObject.end(); ++iterator) {
        if (!iterator->isString()) {
            continue;
        }
        session->localizedStrings.insert(iterator.key(), iterator->toString());
    }

    return !session->localizedStrings.isEmpty();
}

QString localizedString(const WorkerSession* session, const QString& key) {
    if (!session) {
        return localizedFallback(key);
    }
    return session->localizedStrings.value(key, localizedFallback(key));
}

QString buildPreviewText(QString text) {
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace('\r', '\n');
    text = text.simplified();
    if (text.length() > 180) {
        text = text.left(180) + QStringLiteral("...");
    }
    return text;
}

QString buildCompareCellText(const LogEntry& entry) {
    QStringList lines;
    lines << QString::fromUtf8(entry.systemTime.c_str())
          << QString::fromUtf8(entry.gameTime.c_str())
          << QString::fromUtf8(entry.message.c_str()).trimmed();
    return lines.join(QStringLiteral("\n"));
}

QString formatEntryForClipboard(const QString& systemTime,
                                const QString& gameTime,
                                const QString& category,
                                const QString& message) {
    return QStringLiteral("[%1][%2][%3]: %4")
        .arg(systemTime, gameTime, category, message);
}

QString formatEntryForClipboard(const LogEntry& entry) {
    return formatEntryForClipboard(
        QString::fromUtf8(entry.systemTime.c_str()),
        QString::fromUtf8(entry.gameTime.c_str()),
        QString::fromUtf8(entry.category.c_str()),
        QString::fromUtf8(entry.message.c_str())
    );
}

QString displayNameForUi(const WorkerSession* session, const LogFileRecord& fileRecord) {
    return fileRecord.isLatest
        ? localizedString(session, QStringLiteral("Latest"))
        : QString::fromUtf8(fileRecord.displayName.c_str());
}

QString displayNameForFile(const WorkerSession* session,
                           const std::vector<LogFileRecord>& files,
                           const std::string& displayName) {
    for (const LogFileRecord& file : files) {
        if (file.displayName == displayName) {
            return displayNameForUi(session, file);
        }
    }
    return QString::fromUtf8(displayName.c_str());
}

QString sortModeKey(SortMode mode) {
    return mode == SortMode::ByCategory
        ? QStringLiteral("category")
        : QStringLiteral("time");
}

QString sortActionId(SortMode mode) {
    return mode == SortMode::ByCategory
        ? QStringLiteral("error_log::sort_category")
        : QStringLiteral("error_log::sort_time");
}

QString viewModeKey(const StateSnapshot& state) {
    return state.isCompareMode
        ? QStringLiteral("compare")
        : QStringLiteral("normal");
}

QJsonObject buildColumn(const QString& key, const QString& title, int width = -1, bool stretch = false, bool hidden = false) {
    QJsonObject column;
    column[QStringLiteral("key")] = key;
    column[QStringLiteral("id")] = key;
    column[QStringLiteral("title")] = title;
    column[QStringLiteral("text")] = title;
    if (width > 0) {
        column[QStringLiteral("width")] = width;
    }
    if (stretch) {
        column[QStringLiteral("stretch")] = true;
    }
    if (hidden) {
        column[QStringLiteral("hidden")] = true;
    }
    return column;
}

QJsonObject buildContextAction(const QString& actionId, const QString& text, const QString& visibleWhen) {
    QJsonObject action;
    action[QStringLiteral("actionId")] = actionId;
    action[QStringLiteral("text")] = text;
    if (!visibleWhen.isEmpty()) {
        action[QStringLiteral("visibleWhen")] = visibleWhen;
    }
    return action;
}

QJsonObject serializeLoadingState(const LoadingStateSnapshot& loading) {
    QJsonObject object;
    object[QStringLiteral("active")] = loading.active;
    object[QStringLiteral("progress")] = loading.progress;
    object[QStringLiteral("text")] = QString::fromUtf8(loading.text.c_str());
    return object;
}

QJsonObject serializeStatisticsState(const StatisticsSnapshot& statistics) {
    QJsonObject object;
    object[QStringLiteral("fileCount")] = statistics.fileCount;
    object[QStringLiteral("totalCount")] = statistics.totalCount;
    object[QStringLiteral("filteredCount")] = statistics.filteredCount;
    return object;
}

QJsonObject serializeEntry(const LogEntry& entry) {
    QJsonObject object;
    object[QStringLiteral("systemTime")] = QString::fromUtf8(entry.systemTime.c_str());
    object[QStringLiteral("gameTime")] = QString::fromUtf8(entry.gameTime.c_str());
    object[QStringLiteral("category")] = QString::fromUtf8(entry.category.c_str());
    object[QStringLiteral("message")] = QString::fromUtf8(entry.message.c_str());
    object[QStringLiteral("normalizedKey")] = QString::fromUtf8(entry.normalizedKey.c_str());
    object[QStringLiteral("isHighPriority")] = entry.isHighPriority;
    object[QStringLiteral("originalIndex")] = entry.originalIndex;
    return object;
}

QJsonObject serializeFile(const WorkerSession* session,
                          const LogFileRecord& file,
                          bool isCurrent,
                          bool isCompare) {
    QJsonObject object;
    object[QStringLiteral("id")] = QString::fromUtf8(file.displayName.c_str());
    object[QStringLiteral("displayName")] = QString::fromUtf8(file.displayName.c_str());
    object[QStringLiteral("displayNameUi")] = displayNameForUi(session, file);
    object[QStringLiteral("sourcePath")] = QString::fromUtf8(file.sourcePath.c_str());
    object[QStringLiteral("isLatest")] = file.isLatest;
    object[QStringLiteral("isLoaded")] = file.isLoaded;
    object[QStringLiteral("isCurrent")] = isCurrent;
    object[QStringLiteral("isCompare")] = isCompare;
    object[QStringLiteral("canCompare")] = !isCurrent && !isCompare;
    object[QStringLiteral("canStopCompare")] = isCompare;
    object[QStringLiteral("entryCount")] = static_cast<int>(file.entries.size());
    return object;
}

QJsonObject serializeCompareRow(const CompareRow& row) {
    QJsonObject object;
    object[QStringLiteral("normalizedKey")] = QString::fromUtf8(row.normalizedKey.c_str());
    object[QStringLiteral("category")] = QString::fromUtf8(row.category.c_str());
    object[QStringLiteral("isHighPriority")] = row.isHighPriority;
    object[QStringLiteral("hasLeft")] = row.hasLeft;
    object[QStringLiteral("hasRight")] = row.hasRight;
    object[QStringLiteral("leftEntry")] = row.hasLeft ? serializeEntry(row.leftEntry) : QJsonObject();
    object[QStringLiteral("rightEntry")] = row.hasRight ? serializeEntry(row.rightEntry) : QJsonObject();
    object[QStringLiteral("leftPreview")] = row.hasLeft
        ? buildPreviewText(buildCompareCellText(row.leftEntry))
        : QString();
    object[QStringLiteral("rightPreview")] = row.hasRight
        ? buildPreviewText(buildCompareCellText(row.rightEntry))
        : QString();
    return object;
}

QJsonObject buildFileListModel(const WorkerSession* session, const StateSnapshot& state) {
    QJsonObject model;
    model[QStringLiteral("id")] = QStringLiteral("file_list");
    model[QStringLiteral("title")] = localizedString(session, QStringLiteral("LogFiles"));

    QJsonArray contextActions;
    contextActions.append(buildContextAction(
        QStringLiteral("compare"),
        localizedString(session, QStringLiteral("Compare")),
        QStringLiteral("can_compare")
    ));
    contextActions.append(buildContextAction(
        QStringLiteral("stop_compare"),
        localizedString(session, QStringLiteral("StopCompare")),
        QStringLiteral("can_stop_compare")
    ));
    model[QStringLiteral("contextActions")] = contextActions;

    QJsonArray columns;
    columns.append(buildColumn(QStringLiteral("name"), localizedString(session, QStringLiteral("LogFiles")), -1, true));
    model[QStringLiteral("columns")] = columns;

    QJsonArray rows;
    for (const LogFileRecord& file : state.files) {
        const bool isCurrent = file.displayName == state.currentFileName;
        const bool isCompare = file.displayName == state.compareFileName;
        const QString uiDisplayName = displayNameForUi(session, file);

        QJsonObject row;
        row[QStringLiteral("id")] = QString::fromUtf8(file.displayName.c_str());
        row[QStringLiteral("rowId")] = QString::fromUtf8(file.displayName.c_str());
        row[QStringLiteral("role")] = file.isLatest ? QStringLiteral("latest") : QString();

        QJsonObject values;
        values[QStringLiteral("name")] = uiDisplayName;
        values[QStringLiteral("displayName")] = QString::fromUtf8(file.displayName.c_str());
        values[QStringLiteral("sourcePath")] = QString::fromUtf8(file.sourcePath.c_str());
        values[QStringLiteral("entryCount")] = static_cast<int>(file.entries.size());
        row[QStringLiteral("values")] = values;

        QJsonArray cells;
        QJsonObject nameCell;
        nameCell[QStringLiteral("value")] = uiDisplayName;
        cells.append(nameCell);
        row[QStringLiteral("cells")] = cells;

        QJsonObject stateObject;
        stateObject[QStringLiteral("is_latest")] = file.isLatest;
        stateObject[QStringLiteral("is_loaded")] = file.isLoaded;
        stateObject[QStringLiteral("is_current")] = isCurrent;
        stateObject[QStringLiteral("is_compare")] = isCompare;
        stateObject[QStringLiteral("can_compare")] = !isCurrent && !isCompare;
        stateObject[QStringLiteral("can_stop_compare")] = isCompare;
        if (isCompare) {
            stateObject[QStringLiteral("backgroundColor")] = QStringLiteral("#FFEBD2");
        }
        row[QStringLiteral("state")] = stateObject;

        rows.append(row);
    }
    model[QStringLiteral("rows")] = rows;

    QJsonArray selection;
    if (!state.currentFileName.empty()) {
        selection.append(QString::fromUtf8(state.currentFileName.c_str()));
    }
    model[QStringLiteral("selection")] = selection;
    return model;
}

QJsonObject buildEntriesModel(const WorkerSession* session, const StateSnapshot& state) {
    QJsonObject model;
    model[QStringLiteral("id")] = QStringLiteral("log_entries");
    model[QStringLiteral("title")] = localizedString(session, QStringLiteral("ErrorLog"));

    QJsonArray columns;
    columns.append(buildColumn(QStringLiteral("timestamp"), localizedString(session, QStringLiteral("ColSystemTime")), 90));
    columns.append(buildColumn(QStringLiteral("game_date"), localizedString(session, QStringLiteral("ColGameTime")), 130));
    columns.append(buildColumn(QStringLiteral("category"), localizedString(session, QStringLiteral("ColCategory")), 220));
    columns.append(buildColumn(QStringLiteral("message"), localizedString(session, QStringLiteral("ColMessage")), -1, true));
    model[QStringLiteral("columns")] = columns;

    QJsonArray rows;
    for (const LogEntry& entry : state.entries) {
        const QString categoryText = QString::fromUtf8(entry.category.c_str());
        const QString messagePreview = buildPreviewText(QString::fromUtf8(entry.message.c_str()));
        const QString normalizedCategory = categoryText.trimmed().toLower();
        const QString compactRowId = QStringLiteral("entry_%1").arg(entry.originalIndex);
        QString messageRole;
        if (normalizedCategory.contains(QStringLiteral("error"))) {
            messageRole = QStringLiteral("error");
        } else if (normalizedCategory.contains(QStringLiteral("warning"))) {
            messageRole = QStringLiteral("warning");
        }

        QJsonObject row;
        row[QStringLiteral("id")] = compactRowId;
        row[QStringLiteral("rowId")] = compactRowId;
        row[QStringLiteral("role")] = entry.isHighPriority ? QStringLiteral("priority") : QString();

        QJsonObject values;
        values[QStringLiteral("timestamp")] = QString::fromUtf8(entry.systemTime.c_str());
        values[QStringLiteral("game_date")] = QString::fromUtf8(entry.gameTime.c_str());
        values[QStringLiteral("category")] = categoryText;
        values[QStringLiteral("message")] = messagePreview;
        row[QStringLiteral("values")] = values;

        QJsonObject stateObject;
        stateObject[QStringLiteral("is_high_priority")] = entry.isHighPriority;
        row[QStringLiteral("state")] = stateObject;

        rows.append(row);
    }
    model[QStringLiteral("rows")] = rows;
    return model;
}

QJsonObject buildCompareModel(const WorkerSession* session, const StateSnapshot& state) {
    QJsonObject model;
    model[QStringLiteral("id")] = QStringLiteral("compare_entries");
    model[QStringLiteral("title")] = localizedString(session, QStringLiteral("CompareMode"));

    const QString leftTitle = displayNameForFile(session, state.files, state.currentFileName);
    const QString rightTitle = displayNameForFile(session, state.files, state.compareFileName);

    QJsonArray columns;
    columns.append(buildColumn(QStringLiteral("left"), leftTitle, -1, true));
    columns.append(buildColumn(QStringLiteral("right"), rightTitle, -1, true));
    columns.append(buildColumn(QStringLiteral("category"), localizedString(session, QStringLiteral("ColCategory")), 220));
    model[QStringLiteral("columns")] = columns;

    QJsonArray rows;
    int compareRowIndex = 0;
    for (const CompareRow& compareRow : state.compareRows) {
        const QString leftValue = compareRow.hasLeft
            ? buildPreviewText(buildCompareCellText(compareRow.leftEntry))
            : QString();
        const QString rightValue = compareRow.hasRight
            ? buildPreviewText(buildCompareCellText(compareRow.rightEntry))
            : QString();
        const QString categoryValue = QString::fromUtf8(compareRow.category.c_str());
        const QString compactRowId = QStringLiteral("compare_%1").arg(compareRowIndex++);

        QJsonObject row;
        row[QStringLiteral("id")] = compactRowId;
        row[QStringLiteral("rowId")] = compactRowId;
        row[QStringLiteral("role")] = compareRow.isHighPriority ? QStringLiteral("priority") : QString();

        QJsonObject values;
        values[QStringLiteral("left")] = leftValue;
        values[QStringLiteral("right")] = rightValue;
        values[QStringLiteral("category")] = categoryValue;
        row[QStringLiteral("values")] = values;

        QJsonObject stateObject;
        stateObject[QStringLiteral("is_high_priority")] = compareRow.isHighPriority;
        stateObject[QStringLiteral("has_left")] = compareRow.hasLeft;
        stateObject[QStringLiteral("has_right")] = compareRow.hasRight;
        stateObject[QStringLiteral("left_missing")] = !compareRow.hasLeft;
        stateObject[QStringLiteral("right_missing")] = !compareRow.hasRight;
        row[QStringLiteral("state")] = stateObject;

        rows.append(row);
    }
    model[QStringLiteral("rows")] = rows;
    return model;
}

QJsonArray buildListModels(const WorkerSession* session, const StateSnapshot& state) {
    QJsonArray models;
    models.append(buildFileListModel(session, state));
    models.append(buildEntriesModel(session, state));
    models.append(buildCompareModel(session, state));
    return models;
}

QJsonObject buildLegacyModelsObject(const QJsonArray& listModels) {
    QJsonObject modelsObject;
    for (const QJsonValue& value : listModels) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject modelObject = value.toObject();
        const QString modelId = modelObject.value(QStringLiteral("id")).toString().trimmed();
        if (!modelId.isEmpty()) {
            modelsObject[modelId] = modelObject;
        }
    }
    return modelsObject;
}

QJsonObject buildSidebarState(const WorkerSession* session, const StateSnapshot& state) {
    QJsonObject sidebarState;
    sidebarState[QStringLiteral("visible")] = true;
    sidebarState[QStringLiteral("title")] = localizedString(session, QStringLiteral("LogFiles"));
    sidebarState[QStringLiteral("activeMode")] = QStringLiteral("file_list");

    QJsonArray modeOrder;
    modeOrder.append(QStringLiteral("file_list"));
    sidebarState[QStringLiteral("modeOrder")] = modeOrder;

    sidebarState[QStringLiteral("searchEnabled")] = false;
    sidebarState[QStringLiteral("selectAllEnabled")] = false;
    sidebarState[QStringLiteral("modelId")] = QStringLiteral("file_list");
    sidebarState[QStringLiteral("selectedFileName")] = QString::fromUtf8(state.currentFileName.c_str());
    sidebarState[QStringLiteral("compareFileName")] = QString::fromUtf8(state.compareFileName.c_str());
    return sidebarState;
}

QJsonObject buildTopbarState(const WorkerSession* session, const StateSnapshot& state) {
    QJsonObject topbarState;
    topbarState[QStringLiteral("visible")] = true;
    topbarState[QStringLiteral("currentPageId")] = QStringLiteral("error_log");

    QJsonArray pageOrder;
    pageOrder.append(QStringLiteral("error_log"));
    topbarState[QStringLiteral("pageOrder")] = pageOrder;

    QJsonArray functionOrder;
    functionOrder.append(QStringLiteral("error_log::refresh"));
    functionOrder.append(QStringLiteral("error_log::sort_time"));
    functionOrder.append(QStringLiteral("error_log::sort_category"));
    functionOrder.append(QStringLiteral("error_log::toggle_compare_mode"));
    topbarState[QStringLiteral("functionOrder")] = functionOrder;

    topbarState[QStringLiteral("activeFunction")] = sortActionId(state.sortMode);
    topbarState[QStringLiteral("activeViewMode")] = viewModeKey(state);
    topbarState[QStringLiteral("searchText")] = QString::fromUtf8(state.searchText.c_str());
    topbarState[QStringLiteral("searchPlaceholder")] = localizedString(session, QStringLiteral("SearchPlaceholder"));
    topbarState[QStringLiteral("compareMode")] = state.isCompareMode;

    QJsonArray rightButtons;
    rightButtons.append(QJsonObject{
        {QStringLiteral("actionId"), QStringLiteral("error_log::sort_time")},
        {QStringLiteral("text"), localizedString(session, QStringLiteral("SortByTime"))},
        {QStringLiteral("shortcut"), QStringLiteral("T")},
        {QStringLiteral("checked"), state.sortMode != SortMode::ByCategory},
        {QStringLiteral("variant"), QStringLiteral("toolbar")}
    });
    rightButtons.append(QJsonObject{
        {QStringLiteral("actionId"), QStringLiteral("error_log::sort_category")},
        {QStringLiteral("text"), localizedString(session, QStringLiteral("SortByCategory"))},
        {QStringLiteral("shortcut"), QStringLiteral("C")},
        {QStringLiteral("checked"), state.sortMode == SortMode::ByCategory},
        {QStringLiteral("variant"), QStringLiteral("toolbar")}
    });
    topbarState[QStringLiteral("rightButtons")] = rightButtons;
    return topbarState;
}

QJsonObject buildViewState(const WorkerSession* session, const StateSnapshot& state) {
    QJsonObject viewState;
    viewState[QStringLiteral("title")] = localizedString(session, QStringLiteral("ErrorLog"));
    viewState[QStringLiteral("pageTitle")] = localizedString(session, QStringLiteral("ErrorLog"));
    viewState[QStringLiteral("resultTitle")] = state.isCompareMode
        ? localizedString(session, QStringLiteral("CompareMode"))
        : localizedString(session, QStringLiteral("ErrorLog"));
    viewState[QStringLiteral("viewMode")] = viewModeKey(state);
    viewState[QStringLiteral("activeModelId")] = QString::fromUtf8(state.activeModelId.c_str());
    viewState[QStringLiteral("currentFileName")] = QString::fromUtf8(state.currentFileName.c_str());
    viewState[QStringLiteral("currentFileDisplayName")] = displayNameForFile(session, state.files, state.currentFileName);
    viewState[QStringLiteral("compareFileName")] = QString::fromUtf8(state.compareFileName.c_str());
    viewState[QStringLiteral("compareFileDisplayName")] = displayNameForFile(session, state.files, state.compareFileName);
    viewState[QStringLiteral("selectedFileName")] = QString::fromUtf8(state.currentFileName.c_str());
    viewState[QStringLiteral("compareTargetName")] = QString::fromUtf8(state.compareFileName.c_str());
    viewState[QStringLiteral("searchText")] = QString::fromUtf8(state.searchText.c_str());
    viewState[QStringLiteral("searchFilterActive")] = !state.searchText.empty();
    viewState[QStringLiteral("sortMode")] = sortModeKey(state.sortMode);
    viewState[QStringLiteral("sortModeIndex")] = state.sortMode == SortMode::ByCategory ? 1 : 0;
    viewState[QStringLiteral("sortActionId")] = sortActionId(state.sortMode);
    viewState[QStringLiteral("isCompareMode")] = state.isCompareMode;
    viewState[QStringLiteral("hasCurrentFile")] = state.hasCurrentFile;
    viewState[QStringLiteral("hasCompareFile")] = state.hasCompareFile;
    viewState[QStringLiteral("hasFiles")] = state.hasFiles;
    viewState[QStringLiteral("canStopCompare")] = state.isCompareMode;
    viewState[QStringLiteral("emptyStateText")] = state.hasFiles
        ? localizedString(session, QStringLiteral("NoEntries"))
        : localizedString(session, QStringLiteral("NoFiles"));
    viewState[QStringLiteral("loading")] = serializeLoadingState(state.loading);
    viewState[QStringLiteral("statistics")] = serializeStatisticsState(state.statistics);

    return viewState;
}

QJsonObject buildRuntimeVariables(const WorkerSession* session, const StateSnapshot& state) {
    QJsonObject runtimeVariables;
    runtimeVariables[QStringLiteral("pageTitle")] = localizedString(session, QStringLiteral("ErrorLog"));
    runtimeVariables[QStringLiteral("currentPage")] = QStringLiteral("error_log");
    runtimeVariables[QStringLiteral("viewMode")] = viewModeKey(state);
    runtimeVariables[QStringLiteral("activeModelId")] = QString::fromUtf8(state.activeModelId.c_str());
    runtimeVariables[QStringLiteral("fileListModelId")] = QStringLiteral("file_list");
    runtimeVariables[QStringLiteral("entryModelId")] = QStringLiteral("log_entries");
    runtimeVariables[QStringLiteral("compareModelId")] = QStringLiteral("compare_entries");
    runtimeVariables[QStringLiteral("resultModelId")] = state.isCompareMode
        ? QStringLiteral("compare_entries")
        : QStringLiteral("log_entries");
    runtimeVariables[QStringLiteral("currentFileName")] = QString::fromUtf8(state.currentFileName.c_str());
    runtimeVariables[QStringLiteral("currentFileDisplayName")] = displayNameForFile(session, state.files, state.currentFileName);
    runtimeVariables[QStringLiteral("compareFileName")] = QString::fromUtf8(state.compareFileName.c_str());
    runtimeVariables[QStringLiteral("compareFileDisplayName")] = displayNameForFile(session, state.files, state.compareFileName);
    runtimeVariables[QStringLiteral("searchText")] = QString::fromUtf8(state.searchText.c_str());
    runtimeVariables[QStringLiteral("searchPlaceholder")] = localizedString(session, QStringLiteral("SearchPlaceholder"));
    runtimeVariables[QStringLiteral("sortMode")] = sortModeKey(state.sortMode);
    runtimeVariables[QStringLiteral("sortModeIndex")] = state.sortMode == SortMode::ByCategory ? 1 : 0;
    runtimeVariables[QStringLiteral("sortActionId")] = sortActionId(state.sortMode);
    runtimeVariables[QStringLiteral("isCompareMode")] = state.isCompareMode;
    runtimeVariables[QStringLiteral("hasCurrentFile")] = state.hasCurrentFile;
    runtimeVariables[QStringLiteral("hasCompareFile")] = state.hasCompareFile;
    runtimeVariables[QStringLiteral("hasFiles")] = state.hasFiles;
    runtimeVariables[QStringLiteral("loadingActive")] = state.loading.active;
    runtimeVariables[QStringLiteral("loadingProgress")] = state.loading.progress;
    runtimeVariables[QStringLiteral("loadingText")] = QString::fromUtf8(state.loading.text.c_str());
    runtimeVariables[QStringLiteral("fileCount")] = state.statistics.fileCount;
    runtimeVariables[QStringLiteral("totalCount")] = state.statistics.totalCount;
    runtimeVariables[QStringLiteral("filteredCount")] = state.statistics.filteredCount;
    runtimeVariables[QStringLiteral("statusMessage")] = state.isCompareMode
        ? localizedString(session, QStringLiteral("CompareMode"))
        : localizedString(session, QStringLiteral("Ready"));
    runtimeVariables[QStringLiteral("emptyStateText")] = state.hasFiles
        ? localizedString(session, QStringLiteral("NoEntries"))
        : localizedString(session, QStringLiteral("NoFiles"));
    return runtimeVariables;
}

QJsonObject buildLegacyValues(const QJsonObject& viewState, const QJsonObject& runtimeVariables) {
    QJsonObject values = viewState;
    for (auto iterator = runtimeVariables.begin(); iterator != runtimeVariables.end(); ++iterator) {
        values.insert(iterator.key(), iterator.value());
        values.insert(QStringLiteral("ui.%1").arg(iterator.key()), iterator.value());
    }
    return values;
}

ToolWorkerResult refreshSessionState(WorkerSession* session) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }

    session->core.setFileSystem(session->fileSystem.get());

    // Populate the file list without parsing log contents; the selected file is
    // loaded after the QML view has had a chance to render the sidebar.
    if (!session->core.refreshFiles(false)) {
        setSessionError(session, QString::fromUtf8(session->core.lastError().c_str()));
        return TOOL_WORKER_ERROR_ACTION_FAILED;
    }

    clearSessionError(session);
    return TOOL_WORKER_SUCCESS;
}

std::string toLowerStd(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string trimStd(const std::string& value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character) != 0;
    });
    if (begin == value.end()) {
        return std::string();
    }

    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character) != 0;
    }).base();
    return std::string(begin, end);
}

std::string normalizedActionName(const QString& action) {
    return toLowerStd(trimStd(action.toUtf8().toStdString()));
}

std::string firstNonEmptyParam(const std::map<std::string, std::string>& params,
                               std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const auto iterator = params.find(key);
        if (iterator != params.end() && !trimStd(iterator->second).empty()) {
            return iterator->second;
        }
    }
    return std::string();
}

bool parseSortMode(const std::string& value, SortMode* outMode) {
    if (!outMode) {
        return false;
    }

    const std::string normalized = toLowerStd(trimStd(value));
    if (normalized.empty()
        || normalized == "0"
        || normalized == "time"
        || normalized == "by_time"
        || normalized == "sort_time") {
        *outMode = SortMode::ByTime;
        return true;
    }
    if (normalized == "1"
        || normalized == "category"
        || normalized == "by_category"
        || normalized == "sort_category") {
        *outMode = SortMode::ByCategory;
        return true;
    }
    return false;
}

ToolWorkerResult applyCoreActionResult(WorkerSession* session, bool success) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }

    if (!success) {
        setSessionError(session, QString::fromUtf8(session->core.lastError().c_str()));
        return TOOL_WORKER_ERROR_ACTION_FAILED;
    }

    clearSessionError(session);
    return TOOL_WORKER_SUCCESS;
}

} // namespace

ToolWorkerHandle createWorkerHandle(const char* toolId) {
    (void)toolId;
    WorkerSession* session = new (std::nothrow) WorkerSession();
    return reinterpret_cast<ToolWorkerHandle>(session);
}

void destroyWorkerHandle(ToolWorkerHandle handle) {
    delete sessionFromHandle(handle);
}

WorkerSession* sessionFromHandle(ToolWorkerHandle handle) {
    return reinterpret_cast<WorkerSession*>(handle);
}

void setSessionError(WorkerSession* session, const QString& message) {
    if (!session) {
        return;
    }
    session->lastError = message.toUtf8().toStdString();
}

void clearSessionError(WorkerSession* session) {
    if (!session) {
        return;
    }
    session->lastError.clear();
}

char* allocateCString(const QByteArray& utf8) {
    char* result = static_cast<char*>(std::malloc(static_cast<std::size_t>(utf8.size()) + 1U));
    if (!result) {
        return nullptr;
    }

    std::memcpy(result, utf8.constData(), static_cast<std::size_t>(utf8.size()));
    result[utf8.size()] = '\0';
    return result;
}

QJsonObject parseJsonObject(const char* jsonText) {
    if (!jsonText || *jsonText == '\0') {
        return QJsonObject();
    }

    const QJsonDocument document = QJsonDocument::fromJson(QByteArray(jsonText));
    if (!document.isObject()) {
        return QJsonObject();
    }
    return document.object();
}

std::map<std::string, std::string> toStringMap(const QJsonObject& object) {
    std::map<std::string, std::string> values;
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (iterator->isString()) {
            values.emplace(iterator.key().toStdString(), iterator->toString().toUtf8().toStdString());
        } else if (iterator->isBool()) {
            values.emplace(iterator.key().toStdString(), iterator->toBool() ? "true" : "false");
        } else if (iterator->isDouble()) {
            values.emplace(iterator.key().toStdString(), QByteArray::number(iterator->toDouble()).toStdString());
        } else if (iterator->isNull()) {
            values.emplace(iterator.key().toStdString(), std::string());
        } else if (iterator->isArray()) {
            values.emplace(
                iterator.key().toStdString(),
                QJsonDocument(iterator->toArray()).toJson(QJsonDocument::Compact).toStdString()
            );
        } else {
            values.emplace(
                iterator.key().toStdString(),
                QJsonDocument(iterator->toObject()).toJson(QJsonDocument::Compact).toStdString()
            );
        }
    }
    return values;
}

QJsonObject buildStatePacket(WorkerSession* session) {
    QJsonObject packet;
    packet[QStringLiteral("pageId")] = QStringLiteral("error_log");
    packet[QStringLiteral("currentPage")] = QStringLiteral("error_log");

    if (!session) {
        packet[QStringLiteral("modeId")] = QStringLiteral("default");
        packet[QStringLiteral("viewState")] = QJsonObject();
        packet[QStringLiteral("sidebarState")] = QJsonObject();
        packet[QStringLiteral("topbarState")] = QJsonObject();
        packet[QStringLiteral("runtimeVariables")] = QJsonObject();
        packet[QStringLiteral("listModels")] = QJsonArray();
        packet[QStringLiteral("patches")] = QJsonArray();
        packet[QStringLiteral("values")] = QJsonObject();
        packet[QStringLiteral("models")] = QJsonObject();
        return packet;
    }

    const StateSnapshot state = session->core.buildState();
    const QJsonObject viewState = buildViewState(session, state);
    const QJsonObject sidebarState = buildSidebarState(session, state);
    const QJsonObject topbarState = buildTopbarState(session, state);
    const QJsonObject runtimeVariables = buildRuntimeVariables(session, state);
    const QJsonArray listModels = buildListModels(session, state);

    packet[QStringLiteral("modeId")] = state.isCompareMode
        ? QStringLiteral("compare")
        : QStringLiteral("default");
    packet[QStringLiteral("viewState")] = viewState;
    packet[QStringLiteral("sidebarState")] = sidebarState;
    packet[QStringLiteral("topbarState")] = topbarState;
    packet[QStringLiteral("runtimeVariables")] = runtimeVariables;
    packet[QStringLiteral("listModels")] = listModels;
    packet[QStringLiteral("patches")] = QJsonArray();

    packet[QStringLiteral("values")] = QJsonObject();
    packet[QStringLiteral("models")] = QJsonObject();

    return packet;
}

char* serializeStatePacket(WorkerSession* session) {
    if (!session) {
        const QByteArray emptyState = QJsonDocument(buildStatePacket(nullptr)).toJson(QJsonDocument::Compact);
        return allocateCString(emptyState);
    }

    const QByteArray stateJson = QJsonDocument(buildStatePacket(session)).toJson(QJsonDocument::Compact);
    session->lastSerializedState = stateJson.toStdString();
    return allocateCString(stateJson);
}

ToolWorkerResult initializeSession(WorkerSession* session, const char* configJson) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }

    const QJsonObject config = parseJsonObject(configJson);
    session->toolDirectoryPath = config.value(QStringLiteral("toolDirectory")).toString();
    if (session->toolDirectoryPath.trimmed().isEmpty()) {
        session->toolDirectoryPath = QDir::currentPath();
    }

    session->currentLanguageCode = resolveLanguageCode(
        QDir(session->toolDirectoryPath).filePath(QStringLiteral("localisation")),
        config.value(QStringLiteral("language")).toString()
    );
    if (!loadLocalizedStringsFromJson(session, config.value(QStringLiteral("localizedStrings")).toObject())) {
        session->localizedStrings.clear();
    }

    session->fileSystem = std::make_unique<ToolRuntimeFileSystem>();
    session->core.setFileSystem(session->fileSystem.get());
    session->lastError.clear();
    session->lastSerializedState.clear();

    if (!session->core.initialize()) {
        setSessionError(session, QString::fromUtf8(session->core.lastError().c_str()));
        return TOOL_WORKER_ERROR_INITIALIZATION_FAILED;
    }

    clearSessionError(session);
    return TOOL_WORKER_SUCCESS;
}

ToolWorkerResult applyActionInternal(WorkerSession* session,
                                     const char* actionType,
                                     const char* targetId,
                                     const char* argumentsJson) {
    if (!session) {
        return TOOL_WORKER_ERROR_INVALID_HANDLE;
    }

    const QString actionText = QString::fromUtf8(actionType ? actionType : "").trimmed();
    const std::string action = normalizedActionName(actionText);
    const QJsonObject argumentsObject = parseJsonObject(argumentsJson);

    if (action.empty()) {
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "load_language") {
        const QString language = argumentsObject.value(QStringLiteral("language")).toString();
        session->currentLanguageCode = resolveLanguageCode(
            QDir(session->toolDirectoryPath).filePath(QStringLiteral("localisation")),
            language
        );
        if (argumentsObject.value(QStringLiteral("localizedStrings")).isObject()) {
            loadLocalizedStringsFromJson(session, argumentsObject.value(QStringLiteral("localizedStrings")).toObject());
        }
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "on_file_context_menu"
        || action == "on_entry_context_menu"
        || action == "on_compare_context_menu") {
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    std::map<std::string, std::string> params = toStringMap(argumentsObject);
    if (targetId && *targetId != '\0') {
        params["targetId"] = QString::fromUtf8(targetId).toUtf8().toStdString();
    }

    if (action == "refresh_files" || action == "refresh_logs" || action == "refresh") {
        return refreshSessionState(session);
    }

    if (action == "select_file" || action == "on_file_selected" || action == "file_selected") {
        const std::string fileName = firstNonEmptyParam(
            params,
            {"rowId", "targetId", "displayName", "file", "selectedFile", "fileName", "value"}
        );
        return applyCoreActionResult(session, session->core.selectFile(fileName));
    }

    if (action == "start_compare" || action == "compare" || action == "compare_selected" || action == "compare_file") {
        const std::string fileName = firstNonEmptyParam(
            params,
            {"compareFile", "compareTarget", "targetFile", "rowId", "targetId", "displayName", "file", "value"}
        );
        return applyCoreActionResult(session, session->core.startCompare(fileName));
    }

    if (action == "stop_compare" || action == "clear_compare") {
        session->core.stopCompare();
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "switch_view_mode" || action == "set_view_mode") {
        const std::string requestedMode = firstNonEmptyParam(params, {"viewMode", "mode", "value", "targetId"});
        const std::string compareFile = firstNonEmptyParam(
            params,
            {"compareFile", "compareTarget", "targetFile", "displayName", "file", "rowId"}
        );
        return applyCoreActionResult(session, session->core.switchViewMode(requestedMode, compareFile));
    }

    if (action == "toggle_view_mode" || action == "toggle_compare_mode") {
        if (session->core.isCompareMode()) {
            session->core.stopCompare();
            clearSessionError(session);
            return TOOL_WORKER_SUCCESS;
        }

        const std::string compareFile = firstNonEmptyParam(
            params,
            {"compareFile", "compareTarget", "targetFile", "displayName", "file", "rowId", "targetId"}
        );
        return applyCoreActionResult(session, session->core.switchViewMode("compare", compareFile));
    }

    if (action == "search" || action == "search_changed" || action == "on_search_changed" || action == "set_search_text") {
        session->core.setSearchText(firstNonEmptyParam(params, {"text", "value", "searchText", "search", "query"}));
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "sort_time") {
        session->core.setSortMode(SortMode::ByTime);
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "sort_category") {
        session->core.setSortMode(SortMode::ByCategory);
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "toggle_sort" || action == "sort_toggle") {
        const SortMode nextMode = session->core.sortMode() == SortMode::ByTime
            ? SortMode::ByCategory
            : SortMode::ByTime;
        session->core.setSortMode(nextMode);
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    if (action == "set_sort_mode") {
        SortMode mode = SortMode::ByTime;
        if (!parseSortMode(firstNonEmptyParam(params, {"sortMode", "mode", "value", "targetId"}), &mode)) {
            setSessionError(session, QStringLiteral("Unsupported sort mode."));
            return TOOL_WORKER_ERROR_ACTION_FAILED;
        }
        session->core.setSortMode(mode);
        clearSessionError(session);
        return TOOL_WORKER_SUCCESS;
    }

    const bool success = session->core.handleAction(actionText.toUtf8().toStdString(), params);
    return applyCoreActionResult(session, success);
}

ToolWorkerResult initializeWorkerHandle(ToolWorkerHandle handle, const char* configJson) {
    return initializeSession(sessionFromHandle(handle), configJson);
}

const char* handleWorkerAction(ToolWorkerHandle handle,
                               const char* actionType,
                               const char* targetId,
                               const char* argumentsJson,
                               ToolWorkerResult* outResult) {
    WorkerSession* session = sessionFromHandle(handle);
    const ToolWorkerResult result = applyActionInternal(session, actionType, targetId, argumentsJson);
    if (outResult) {
        *outResult = result;
    }
    return serializeStatePacket(session);
}

const char* getWorkerCurrentState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    WorkerSession* session = sessionFromHandle(handle);
    if (!session) {
        if (outResult) {
            *outResult = TOOL_WORKER_ERROR_INVALID_HANDLE;
        }
        return nullptr;
    }

    if (outResult) {
        *outResult = TOOL_WORKER_SUCCESS;
    }
    return serializeStatePacket(session);
}

const char* getWorkerInitialState(ToolWorkerHandle handle, ToolWorkerResult* outResult) {
    WorkerSession* session = sessionFromHandle(handle);
    if (!session) {
        if (outResult) {
            *outResult = TOOL_WORKER_ERROR_INVALID_HANDLE;
        }
        return nullptr;
    }

    const ToolWorkerResult result = refreshSessionState(session);
    if (outResult) {
        *outResult = result;
    }

    return serializeStatePacket(session);
}

const char* getWorkerLastError(ToolWorkerHandle handle) {
    WorkerSession* session = sessionFromHandle(handle);
    return session ? session->lastError.c_str() : "Invalid worker handle.";
}

char* legacyCurrentStateJson(bool* ok) {
    if (!g_legacySession) {
        if (ok) {
            *ok = false;
        }
        QJsonObject errorObject;
        errorObject[QStringLiteral("error")] = QStringLiteral("Legacy worker session is not initialized.");
        const QByteArray payload = QJsonDocument(errorObject).toJson(QJsonDocument::Compact);
        g_legacySerializedState = payload.toStdString();
        return allocateCString(payload);
    }

    const ToolWorkerResult result = refreshSessionState(g_legacySession.get());
    if (ok) {
        *ok = (result == TOOL_WORKER_SUCCESS);
    }
    const QByteArray payload = QJsonDocument(buildStatePacket(g_legacySession.get())).toJson(QJsonDocument::Compact);
    g_legacySerializedState = payload.toStdString();
    return allocateCString(payload);
}

int initializeLegacyWorker(void* runtimeContext) {
    (void)runtimeContext;

    g_legacySession.reset(new WorkerSession());

    QJsonObject initConfig;
    initConfig[QStringLiteral("toolDirectory")] = QDir::currentPath();
    initConfig[QStringLiteral("language")] = QStringLiteral("en_US");

    const QByteArray initJson = QJsonDocument(initConfig).toJson(QJsonDocument::Compact);
    const ToolWorkerResult result = initializeSession(g_legacySession.get(), initJson.constData());
    if (result != TOOL_WORKER_SUCCESS) {
        g_legacyLastError = g_legacySession ? g_legacySession->lastError : std::string("Legacy worker initialization failed.");
        return 1;
    }

    g_legacyLastError.clear();
    return 0;
}

void shutdownLegacyWorker() {
    g_legacySession.reset();
    g_legacyLastError.clear();
    g_legacySerializedState.clear();
}

const char* getLegacyInitialState() {
    bool ok = false;
    char* payload = legacyCurrentStateJson(&ok);
    if (!ok && !g_legacySession) {
        g_legacyLastError = "Legacy worker session is not initialized.";
    }
    return payload;
}

const char* handleLegacyAction(const char* actionJson) {
    if (!g_legacySession) {
        g_legacyLastError = "Legacy worker session is not initialized.";
        return legacyCurrentStateJson();
    }

    const QJsonObject object = parseJsonObject(actionJson);
    const QString actionType = object.value(QStringLiteral("actionType")).toString();
    const QString targetIdValue = object.value(QStringLiteral("targetId")).toString();
    const QJsonObject arguments = object.value(QStringLiteral("arguments")).toObject();

    const QByteArray actionTypeUtf8 = actionType.toUtf8();
    const QByteArray targetIdUtf8 = targetIdValue.toUtf8();
    const QByteArray argumentsUtf8 = QJsonDocument(arguments).toJson(QJsonDocument::Compact);

    const ToolWorkerResult result = applyActionInternal(
        g_legacySession.get(),
        actionTypeUtf8.constData(),
        targetIdUtf8.constData(),
        argumentsUtf8.constData()
    );
    if (result != TOOL_WORKER_SUCCESS) {
        g_legacyLastError = g_legacySession->lastError;
    } else {
        g_legacyLastError.clear();
    }

    return legacyCurrentStateJson();
}

} // namespace LogManagerBridge

extern "C" {

TOOL_WORKER_API ToolWorkerHandle ToolWorker_Create(const char* toolId) {
    return LogManagerBridge::createWorkerHandle(toolId);
}

TOOL_WORKER_API void ToolWorker_Destroy(ToolWorkerHandle handle) {
    LogManagerBridge::destroyWorkerHandle(handle);
}

TOOL_WORKER_API ToolWorkerResult ToolWorker_Initialize(ToolWorkerHandle handle, const char* configJson) {
    return LogManagerBridge::initializeWorkerHandle(handle, configJson);
}

TOOL_WORKER_API const char* ToolWorker_HandleAction(
    ToolWorkerHandle handle,
    const char* actionType,
    const char* targetId,
    const char* argumentsJson,
    ToolWorkerResult* outResult
) {
    return LogManagerBridge::handleWorkerAction(handle, actionType, targetId, argumentsJson, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetCurrentState(
    ToolWorkerHandle handle,
    ToolWorkerResult* outResult
) {
    return LogManagerBridge::getWorkerCurrentState(handle, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetInitialState(
    ToolWorkerHandle handle,
    ToolWorkerResult* outResult
) {
    return LogManagerBridge::getWorkerInitialState(handle, outResult);
}

TOOL_WORKER_API const char* ToolWorker_GetLastError(ToolWorkerHandle handle) {
    return LogManagerBridge::getWorkerLastError(handle);
}

TOOL_WORKER_API void ToolWorker_FreeString(const char* value) {
    std::free(const_cast<char*>(value));
}

TOOL_WORKER_API const char* ToolWorker_GetVersion() {
    return "2.2.0";
}

TOOL_WORKER_API const char* tool_worker_get_version() {
    return ToolWorker_GetVersion();
}

TOOL_WORKER_API int tool_worker_initialize(void* runtimeContext) {
    return LogManagerBridge::initializeLegacyWorker(runtimeContext);
}

TOOL_WORKER_API void tool_worker_shutdown() {
    LogManagerBridge::shutdownLegacyWorker();
}

TOOL_WORKER_API const char* tool_worker_get_initial_state() {
    return LogManagerBridge::getLegacyInitialState();
}

TOOL_WORKER_API const char* tool_worker_handle_action(const char* actionJson) {
    return LogManagerBridge::handleLegacyAction(actionJson);
}

TOOL_WORKER_API void tool_worker_free_string(char* value) {
    std::free(value);
}

} // extern "C"
