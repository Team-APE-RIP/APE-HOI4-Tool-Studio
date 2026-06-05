//-------------------------------------------------------------------------------------
// ToolScriptedHostController.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLSCRIPTEDHOSTCONTROLLER_H
#define TOOLSCRIPTEDHOSTCONTROLLER_H

#include "ToolGuiRuntime.h"
#include "ToolInterface.h"

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QScopedPointer>

class ToolProxyInterface;
class ToolQmlHostController;
class QWidget;

class ToolScriptedHostController : public QObject {
    Q_OBJECT

public:
    explicit ToolScriptedHostController(ToolProxyInterface* proxy, QObject* parent = nullptr);
    ~ToolScriptedHostController() override;

    bool initialize(const QString& toolDirectoryPath, QString* errorMessage = nullptr);

    bool isV2() const { return true; }
    QString guiFilePath() const { return m_guiFilePath; }

    QWidget* buildUiV2(QWidget* parent = nullptr, const QString& guiFilePath = QString(), const QString& pageId = QString());
    void updateStateV2(const QJsonObject& statePacket);
    void updateThemeV2(const QString& theme);

    ToolGuiRenderResult buildUi(QWidget* parent = nullptr);
    void applyAction(const QString& actionType,
                     const QString& targetId,
                     const QVariantMap& arguments = QVariantMap());
    void mergeStatePacket(const QJsonObject& statePacket);
    void setLocalizedStrings(const QMap<QString, QString>& strings);

    const ToolGuiSessionState& sessionState() const { return m_sessionState; }
    const ToolGuiStateSnapshot& currentStateSnapshot() const { return m_currentState; }
    QVariantMap topbarState() const { return m_lastStatePacket.topbarState; }
    bool invokeTopbarShortcut(const QString& actionId);

signals:
    void sessionStateChanged();
    void pageChanged(const QString& pageId);

private:
    void syncLegacySessionState();
    void notifySessionStateChanged();

    ToolProxyInterface* m_proxy = nullptr;
    QScopedPointer<ToolQmlHostController> m_qmlHostController;
    QString m_toolDirectoryPath;
    QString m_guiFilePath;
    ToolUiStatePacket m_lastStatePacket;
    ToolGuiStateSnapshot m_currentState;
    ToolGuiSessionState m_sessionState;
    QMap<QString, QString> m_localizedStrings;
    QString m_currentTheme;
};

#endif // TOOLSCRIPTEDHOSTCONTROLLER_H
