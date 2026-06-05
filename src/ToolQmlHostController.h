//-------------------------------------------------------------------------------------
// ToolQmlHostController.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLQMLHOSTCONTROLLER_H
#define TOOLQMLHOSTCONTROLLER_H

#include "ToolGuiRuntime.h"
#include "ToolInterface.h"

#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QTimer>
#include <QString>
#include <QQuickWidget>

class QQmlComponent;
class QQmlContext;
class QQmlEngine;
class QMouseEvent;
class QEnterEvent;
class QWheelEvent;
class ToolProxyInterface;
class ToolQmlAcrylicImageProvider;
class ToolQmlBridge;
class ToolQmlThemeProvider;
class QWidget;

// Debug QQuickWidget with mouse event tracking
class DebugQuickWidget : public QQuickWidget {
    Q_OBJECT
public:
    explicit DebugQuickWidget(QQmlEngine* engine, QWidget* parent = nullptr);
    
protected:
    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    void logWidgetState(const QString& context);
};

class ToolQmlHostController : public QObject {
    Q_OBJECT

public:
    explicit ToolQmlHostController(ToolProxyInterface* proxy, QObject* parent = nullptr);
    ~ToolQmlHostController() override;

    bool initialize(const QString& toolDirectoryPath,
                    const ToolGuiResourceDescriptor& descriptor,
                    const QMap<QString, QString>& localizedStrings,
                    QString* errorMessage = nullptr);

    QString toolDirectoryPath() const { return m_toolDirectoryPath; }
    QString guiFilePath() const { return m_guiFilePath; }
    QString presetFilePath() const { return m_presetFilePath; }
    QString currentTheme() const { return m_currentTheme; }

    // Persistent view management - returns the same widget instance
    QWidget* persistentView(QWidget* parent = nullptr);
    
    void setLocalizedStrings(const QMap<QString, QString>& strings);
    void applyStatePacket(const ToolUiStatePacket& packet, bool emitPageSignal = true);
    void applyTheme(const QString& themeName);
    void dispatchAction(const QString& actionType,
                        const QString& targetId = QString(),
                        const QVariantMap& arguments = QVariantMap());
    bool invokeTopbarShortcut(const QString& actionId);
    void mergeStatePacket(const QJsonObject& statePacket, bool emitPageSignal = true);
    void setCurrentPage(const QString& pageId, bool emitPageSignal = true);

    const ToolGuiStateSnapshot& currentStateSnapshot() const { return m_currentState; }
    const ToolUiStatePacket& lastStatePacket() const { return m_lastStatePacket; }
    ToolGuiStateSnapshot stateSnapshotFromPacket(const ToolUiStatePacket& packet) const { return buildStateSnapshot(packet); }
    ToolQmlBridge* bridge() const { return m_bridge; }
    ToolQmlThemeProvider* themeProvider() const { return m_themeProvider; }
    
    // Check if persistent view is already created
    bool hasPersistentView() const { return m_activeView != nullptr; }

signals:
    void stateChanged();
    void pageChanged(const QString& pageId);

private:
    ToolGuiStateSnapshot buildStateSnapshot(const ToolUiStatePacket& packet) const;
    ToolGuiCollectionModel buildCollectionModel(const QVariantMap& modelMap) const;
    ToolUiStatePacket statePacketFromJson(const QJsonObject& statePacket) const;
    QString normalizeThemeName(const QString& theme) const;
    QString resolveGuiFilePath(const ToolGuiResourceDescriptor& descriptor) const;
    QString resolvePresetFilePath(const ToolGuiResourceDescriptor& descriptor) const;
    DebugQuickWidget* createQuickWidget(QWidget* parent);
    void prepareQmlContext(QQmlContext* context);
    void handleBridgeAction(const QString& actionType,
                            const QString& targetId,
                            const QVariantMap& arguments);
    QWidget* topLevelHostWindow() const;
    void beginWindowDrag(const QPoint& globalPos);
    void updateWindowDragPosition(const QPoint& globalPos);
    void finishWindowDrag();
    void updateCurrentState(const ToolGuiStateSnapshot& snapshot, bool emitPageSignal);
    void queueActiveViewRefresh(const QString& reason);
    void refreshAcrylicTexture();

    ToolProxyInterface* m_proxy = nullptr;
    QQmlEngine* m_engine = nullptr;
    QQmlComponent* m_component = nullptr;
    ToolQmlAcrylicImageProvider* m_acrylicProvider = nullptr;
    ToolQmlBridge* m_bridge = nullptr;
    ToolQmlThemeProvider* m_themeProvider = nullptr;
    QString m_toolDirectoryPath;
    QString m_guiFilePath;
    QString m_presetFilePath;
    QMap<QString, QString> m_localizedStrings;
    ToolUiStatePacket m_lastStatePacket;
    ToolGuiStateSnapshot m_currentState;
    QString m_currentTheme;
    QPointer<QWidget> m_activeView;  // Now stores ToolUiContainer instead of DebugQuickWidget
    QPointer<QQuickWidget> m_activeQuickWidget;
    QTimer m_acrylicRefreshTimer;
    int m_acrylicRevision = 0;
    bool m_windowDragActive = false;
    QPoint m_windowDragOffset;
};

#endif // TOOLQMLHOSTCONTROLLER_H
