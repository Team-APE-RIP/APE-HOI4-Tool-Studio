//-------------------------------------------------------------------------------------
// ToolQmlBridge.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolQmlBridge.h"

#include <QPoint>
#include <QVariantList>

namespace {
QVariantMap toVariantMap(const QMap<QString, QString>& strings) {
    QVariantMap result;
    for (auto it = strings.constBegin(); it != strings.constEnd(); ++it) {
        result.insert(it.key(), it.value());
    }
    return result;
}

}

ToolQmlBridge::ToolQmlBridge(QObject* parent)
    : QObject(parent) {
}

QString ToolQmlBridge::currentPage() const {
    return m_stateSnapshot.currentPage;
}

QVariantMap ToolQmlBridge::values() const {
    return m_stateSnapshot.values;
}

QVariantMap ToolQmlBridge::localizedStrings() const {
    return toVariantMap(m_localizedStrings);
}

int ToolQmlBridge::localizationRevision() const {
    return m_localizationRevision;
}

QString ToolQmlBridge::theme() const {
    return m_theme;
}

QString ToolQmlBridge::acrylicSource() const {
    return QStringLiteral("image://apetoolacrylic/backdrop?revision=%1").arg(m_acrylicRevision);
}

void ToolQmlBridge::setStateSnapshot(const ToolGuiStateSnapshot& snapshot) {
    const bool pageDidChange = m_stateSnapshot.currentPage != snapshot.currentPage;

    m_stateSnapshot = snapshot;

    if (pageDidChange) {
        emit currentPageChanged();
    }
    emit valuesChanged();

    emit stateChanged();
}

void ToolQmlBridge::setLocalizedStrings(const QMap<QString, QString>& strings) {
    if (m_localizedStrings == strings) {
        return;
    }

    m_localizedStrings = strings;
    ++m_localizationRevision;
    emit localizationRevisionChanged();
    emit localizedStringsChanged();
    emit stateChanged();
}

void ToolQmlBridge::setTheme(const QString& themeName) {
    if (m_theme == themeName) {
        return;
    }

    m_theme = themeName;
    emit themeChanged();
}

void ToolQmlBridge::setAcrylicRevision(int revision) {
    if (m_acrylicRevision == revision) {
        return;
    }

    m_acrylicRevision = revision;
    emit acrylicChanged();
}

QVariantMap ToolQmlBridge::model(const QString& modelId) const {
    if (!m_stateSnapshot.models.contains(modelId)) {
        return QVariantMap();
    }
    return convertModel(m_stateSnapshot.models.value(modelId));
}

QVariantList ToolQmlBridge::modelRows(const QString& modelId) const {
    if (!m_stateSnapshot.models.contains(modelId)) {
        return QVariantList();
    }
    return convertRows(m_stateSnapshot.models.value(modelId));
}

int ToolQmlBridge::modelRowCount(const QString& modelId) const {
    const auto modelIterator = m_stateSnapshot.models.constFind(modelId);
    return modelIterator == m_stateSnapshot.models.constEnd()
        ? 0
        : modelIterator.value().rows.size();
}

QVariantList ToolQmlBridge::modelColumns(const QString& modelId) const {
    if (!m_stateSnapshot.models.contains(modelId)) {
        return QVariantList();
    }
    return convertColumns(m_stateSnapshot.models.value(modelId));
}

QVariantMap ToolQmlBridge::row(const QString& modelId, int rowIndex) const {
    const auto modelIterator = m_stateSnapshot.models.constFind(modelId);
    if (modelIterator == m_stateSnapshot.models.constEnd()) {
        return QVariantMap();
    }

    const ToolGuiCollectionModel& model = modelIterator.value();
    if (rowIndex < 0 || rowIndex >= model.rows.size()) {
        return QVariantMap();
    }

    return convertRow(model.rows.at(rowIndex), model);
}

QVariant ToolQmlBridge::value(const QString& key, const QVariant& defaultValue) const {
    return m_stateSnapshot.values.value(key, defaultValue);
}

QString ToolQmlBridge::text(const QString& key, const QString& fallback) const {
    if (key.trimmed().isEmpty()) {
        return fallback;
    }
    return m_localizedStrings.value(key, fallback.isEmpty() ? key : fallback);
}

QString ToolQmlBridge::imageSource(const QString& pngBase64, const QString& cacheHint) const {
    Q_UNUSED(cacheHint);

    const QString trimmedBase64 = pngBase64.trimmed();
    if (trimmedBase64.isEmpty()) {
        return {};
    }

    if (trimmedBase64.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
        return trimmedBase64;
    }

    return QStringLiteral("data:image/png;base64,%1").arg(trimmedBase64);
}

void ToolQmlBridge::dispatchAction(const QString& actionType,
                                   const QString& targetId,
                                   const QVariantMap& arguments) {
    emit actionRequested(actionType, targetId, arguments);
}

void ToolQmlBridge::startWindowDrag(qreal globalX, qreal globalY) {
    emit windowDragStarted(QPoint(qRound(globalX), qRound(globalY)));
}

void ToolQmlBridge::updateWindowDrag(qreal globalX, qreal globalY) {
    emit windowDragMoved(QPoint(qRound(globalX), qRound(globalY)));
}

void ToolQmlBridge::endWindowDrag() {
    emit windowDragEnded();
}

QVariantMap ToolQmlBridge::convertModel(const ToolGuiCollectionModel& model) const {
    QVariantMap result;
    result.insert(QStringLiteral("id"), model.id);
    result.insert(QStringLiteral("title"), model.title);
    result.insert(QStringLiteral("columns"), convertColumns(model));
    result.insert(QStringLiteral("rows"), convertRows(model));
    result.insert(QStringLiteral("selection"), model.selection);
    result.insert(QStringLiteral("headerHidden"), model.headerHidden);
    result.insert(QStringLiteral("selectionMode"), model.selectionMode);
    result.insert(QStringLiteral("contextActions"), model.contextActions);
    return result;
}

QVariantMap ToolQmlBridge::convertRow(const ToolGuiListRow& row, const ToolGuiCollectionModel& model) const {
    QVariantMap result;
    result.insert(QStringLiteral("id"), row.id);
    result.insert(QStringLiteral("rowId"), row.rowId.isEmpty() ? row.id : row.rowId);
    result.insert(QStringLiteral("role"), row.role);
    result.insert(QStringLiteral("values"), row.values);
    result.insert(QStringLiteral("state"), row.state);
    result.insert(
        QStringLiteral("selected"),
        !row.id.isEmpty() && model.selection.contains(row.id)
    );

    QVariantList cellList;
    for (int index = 0; index < row.cells.size(); ++index) {
        QVariantMap cellMap;
        cellMap.insert(QStringLiteral("value"), row.cells.at(index).value);
        cellMap.insert(QStringLiteral("role"), row.cells.at(index).role);
        if (index >= 0 && index < model.columns.size()) {
            cellMap.insert(QStringLiteral("key"), model.columns.at(index).key);
        }
        cellList.append(cellMap);
    }
    result.insert(QStringLiteral("cells"), cellList);

    for (int columnIndex = 0; columnIndex < model.columns.size(); ++columnIndex) {
        const QString key = model.columns.at(columnIndex).key;
        if (key.trimmed().isEmpty()) {
            continue;
        }

        QVariant cellValue;
        if (columnIndex >= 0 && columnIndex < row.cells.size()) {
            cellValue = row.cells.at(columnIndex).value;
        } else {
            cellValue = row.values.value(key);
        }
        result.insert(key, cellValue);
    }

    return result;
}

QVariantList ToolQmlBridge::convertColumns(const ToolGuiCollectionModel& model) const {
    QVariantList columnList;
    for (const ToolGuiListColumn& column : model.columns) {
        QVariantMap columnMap;
        columnMap.insert(QStringLiteral("key"), column.key);
        columnMap.insert(QStringLiteral("text"), column.text);
        columnMap.insert(QStringLiteral("width"), column.width);
        columnMap.insert(QStringLiteral("stretch"), column.stretch);
        columnMap.insert(QStringLiteral("hidden"), column.hidden);
        columnList.append(columnMap);
    }
    return columnList;
}

QVariantList ToolQmlBridge::convertRows(const ToolGuiCollectionModel& model) const {
    QVariantList rowList;
    for (const ToolGuiListRow& row : model.rows) {
        rowList.append(convertRow(row, model));
    }
    return rowList;
}
