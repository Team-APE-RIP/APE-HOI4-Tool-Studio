//-------------------------------------------------------------------------------------
// ToolQmlBridge.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLQMLBRIDGE_H
#define TOOLQMLBRIDGE_H

#include "ToolGuiRuntime.h"

#include <QMap>
#include <QObject>
#include <QPoint>
#include <QVariantMap>

class ToolQmlBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(QVariantMap values READ values NOTIFY valuesChanged)
    Q_PROPERTY(QVariantMap localizedStrings READ localizedStrings NOTIFY localizedStringsChanged)
    Q_PROPERTY(int localizationRevision READ localizationRevision NOTIFY localizationRevisionChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    Q_PROPERTY(QString acrylicSource READ acrylicSource NOTIFY acrylicChanged)

public:
    explicit ToolQmlBridge(QObject* parent = nullptr);
    ~ToolQmlBridge() override = default;

    QString currentPage() const;
    QVariantMap values() const;
    QVariantMap localizedStrings() const;
    int localizationRevision() const;
    QString theme() const;
    QString acrylicSource() const;

    void setStateSnapshot(const ToolGuiStateSnapshot& snapshot);
    void setLocalizedStrings(const QMap<QString, QString>& strings);
    void setTheme(const QString& themeName);
    void setAcrylicRevision(int revision);

    Q_INVOKABLE QVariantMap model(const QString& modelId) const;
    Q_INVOKABLE QVariantList modelRows(const QString& modelId) const;
    Q_INVOKABLE int modelRowCount(const QString& modelId) const;
    Q_INVOKABLE QVariantList modelColumns(const QString& modelId) const;
    Q_INVOKABLE QVariantMap row(const QString& modelId, int rowIndex) const;
    Q_INVOKABLE QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;
    Q_INVOKABLE QString text(const QString& key, const QString& fallback = QString()) const;
    Q_INVOKABLE QString imageSource(const QString& pngBase64, const QString& cacheHint = QString()) const;
    Q_INVOKABLE void dispatchAction(const QString& actionType,
                                    const QString& targetId = QString(),
                                    const QVariantMap& arguments = QVariantMap());
    Q_INVOKABLE void startWindowDrag(qreal globalX, qreal globalY);
    Q_INVOKABLE void updateWindowDrag(qreal globalX, qreal globalY);
    Q_INVOKABLE void endWindowDrag();

signals:
    void currentPageChanged();
    void valuesChanged();
    void localizedStringsChanged();
    void localizationRevisionChanged();
    void themeChanged();
    void acrylicChanged();
    void stateChanged();
    void actionRequested(const QString& actionType,
                         const QString& targetId,
                         const QVariantMap& arguments);
    void windowDragStarted(const QPoint& globalPos);
    void windowDragMoved(const QPoint& globalPos);
    void windowDragEnded();

private:
    QVariantMap convertModel(const ToolGuiCollectionModel& model) const;
    QVariantMap convertRow(const ToolGuiListRow& row, const ToolGuiCollectionModel& model) const;
    QVariantList convertColumns(const ToolGuiCollectionModel& model) const;
    QVariantList convertRows(const ToolGuiCollectionModel& model) const;

    ToolGuiStateSnapshot m_stateSnapshot;
    QMap<QString, QString> m_localizedStrings;
    int m_localizationRevision = 0;
    int m_acrylicRevision = 0;
    QString m_theme;
};

#endif // TOOLQMLBRIDGE_H
