//-------------------------------------------------------------------------------------
// MainWindow.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QTimer>
#include <QMap>
#include "ToolInterface.h"
#include "SettingsPage.h"
#include "ConfigPage.h"
#include "ToolsPage.h"
#include "AccountPage.h"
#include "LoadingOverlay.h"
#include "Update.h"
#include "UserAgreementOverlay.h"
#include "Advertisement.h"
#include "LoginDialog.h"
#include "SetupDialog.h"
#include "ConnectionWarningOverlay.h"
#include "ExternalPackageManager.h"

class QCloseEvent;
class QComboBox;
class QLineEdit;
class QResizeEvent;
class QShowEvent;
class QTreeWidget;
class ToolInterface;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const ExternalPackageManager::PendingRequest& pendingRequest = ExternalPackageManager::PendingRequest(),
                        QWidget *parent = nullptr);
    ~MainWindow();

    void handleExternalRequest(const ExternalPackageManager::PendingRequest& request);
    bool ensureReadyForIncomingRequest(ExternalPackageManager::RequestType requestType);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSettingsClicked();
    void onConfigClicked();
    void onToolsClicked();
    void onAccountClicked();
    void closeOverlay();
    void onToolSelected(const QString &toolId);

    // Actions from pages
    void onLanguageChanged();
    void onThemeChanged();
    void onDebugModeChanged(bool enabled);
    void onSidebarCompactChanged(bool enabled);
    void updateMemoryUsage();
    void onModClosed();
    void onGamePathChanged();
    void onModPathChanged();
    void onPathInvalid(const QString& titleKey, const QString& msgKey);
    void onToolProcessCrashed(const QString& toolId, const QString& error);
    void onLoginSuccessful();
    void onLogoutRequested();
    void onSetupCompleted();
    void onConnectionLost();
    void onConnectionRestored();
    void onAccountActionBlocked();
    void onAccountActionCleared();
    void requestUpdateShutdown();
    void onRightSidebarSearchTextChanged(const QString& text);
    void onRightSidebarSearchColumnChanged(int index);
    void onRightSidebarSelectAllClicked();
    void refreshActiveToolRightSidebarUi();

    // Window Controls
    void minimizeWindow();
    void maximizeWindow();
    void closeWindow();

    // Sidebar Animation
    void expandSidebar();
    void collapseSidebar();
    void toggleSidebarLock();

    // Advertisement
    void checkAndShowAdvertisement(bool loadFilesAfterAd = false);

private:
    void setupUi();
    void setupSidebar();
    void setupDebugOverlay();
    void loadStyle();
    void updateTexts();
    void applyTheme();
    void performShutdown();
    void processPendingExternalRequest();
    void processPendingToolRequest();
    void processPendingPluginRequest();
    void finishPendingRequest();
    bool restartApplicationForPendingRequest(const QString& restartMessage);
    void bringToFront();
    void resetActiveToolUi();
    void setupRightSidebarUi();
    void clearRightSidebarContent();
    void rebuildRightSidebarRail(const QList<ToolRightSidebarButtonDefinition>& definitions,
                                 const ToolRightSidebarState& state);
    void updateRightSidebarState(const ToolRightSidebarState& state);
    void applyRightSidebarSearchFilter();
    void setRightSidebarListVisible(bool visible);
    void setRightSidebarSearchVisible(bool visible);
    void setRightSidebarExtraActionsVisible(bool visible);
    void updateRightSidebarSelectAllButtonText();
    void updateRightSidebarGeometries();
    void updateRightSidebarRailGeometry();
    void updateRightSidebarListGeometry();
    int currentRightSidebarListWidth() const;
    int normalizedRightSidebarListWidth(int width) const;
    QTreeWidget* currentRightSidebarListWidget() const;
    static QIcon loadThemedSvgIcon(const QString& resourcePath, bool isDark, const QString& colorOverride = QString());

    // UI Components
    QWidget *m_centralWidget;
    QWidget *m_sidebar;
    QStackedWidget *m_mainStack;
    QWidget *m_dashboard;
    QWidget *m_dashboardContent;
    QLabel *m_dashboardTitleLabel;
    QWidget *m_rightSidebarRail;
    QVBoxLayout *m_rightSidebarRailLayout;
    QWidget *m_rightSidebarPanel;
    QWidget *m_rightSidebarResizeHandle;
    QWidget *m_rightSidebarHeaderArea;
    QLabel *m_rightSidebarTitleLabel;
    QWidget *m_rightSidebarSearchContainer;
    QComboBox *m_rightSidebarSearchColumnCombo;
    QLineEdit *m_rightSidebarSearchEdit;
    QWidget *m_rightSidebarExtraActionsContainer;
    QPushButton *m_rightSidebarSelectAllBtn;
    QWidget *m_rightSidebarContentContainer;
    QVBoxLayout *m_rightSidebarContentLayout;
    SettingsPage *m_settingsPage;
    ConfigPage *m_configPage;
    ToolsPage *m_toolsPage;
    AccountPage *m_accountPage;

    // Sidebar Widgets
    QWidget *m_windowControls;
    QLabel *m_appIcon;
    QLabel *m_bottomAppIcon;
    QLabel *m_appTitle;
    QHBoxLayout *m_titleLayout;
    QToolButton *m_toolsBtn;
    QToolButton *m_settingsBtn;
    QToolButton *m_accountBtn;
    QToolButton *m_configBtn;
    QVBoxLayout *m_sidebarLayout;
    QWidget *m_sidebarControlsContainer;
    QWidget *m_controlsHorizontal;
    QWidget *m_controlsVertical;

    // Debug Overlay
    QLabel *m_memUsageLabel;
    QTimer *m_memTimer;

    // Loading Overlay
    LoadingOverlay *m_loadingOverlay;
    QTimer *m_scanCheckTimer;

    // Update Overlay
    Update *m_updateOverlay;

    // User Agreement Overlay
    UserAgreementOverlay *m_userAgreementOverlay;

    // Login Overlay
    LoginDialog *m_loginOverlay;

    // Advertisement Overlay
    Advertisement *m_advertisementOverlay;

    // Setup Overlay
    SetupDialog *m_setupOverlay;

    // Connection Warning Overlay
    ConnectionWarningOverlay *m_connectionWarningOverlay;

    // Startup sequence state
    bool m_startupSequenceStarted = false;
    bool m_setupSkipped = false;
    bool m_loginCompleted = false;
    bool m_pendingConnectionWarning = false;
    ExternalPackageManager::PendingRequest m_pendingExternalRequest;
    bool m_pendingExternalRequestHandled = false;

    // Sidebar collapse delay timer
    QTimer *m_sidebarCollapseTimer;

    // Dragging state
    bool m_dragging = false;
    QPoint m_dragPosition;

    QString m_currentLang;
    bool m_sidebarExpanded = true;
    bool m_sidebarLocked = false;

    // First load tracking
    bool m_firstLoadCompleted = false;

    // Right sidebar state
    int m_rightSidebarListWidth = 190;
    int m_rightSidebarRailWidth = 60;
    int m_rightSidebarDefaultListWidth = 190;
    int m_rightSidebarMinimumListWidth = 0;
    int m_rightSidebarMaximumListWidth = 440;
    bool m_rightSidebarListVisible = false;
    int m_rightSidebarLastExpandedWidth = 190;
    bool m_rightSidebarRestoreRequested = false;
    bool m_rightSidebarResizeDragging = false;
    int m_rightSidebarResizeStartGlobalX = 0;
    int m_rightSidebarResizeStartWidth = 0;
    ToolInterface* m_activeTool = nullptr;
    QTreeWidget* m_activeRightSidebarListWidget = nullptr;
    QString m_rightSidebarSearchText;
    QMap<int, int> m_rightSidebarSearchComboToColumn;
    QMap<QString, QToolButton*> m_rightSidebarRailButtons;

    // Shutdown state
    bool m_shutdownInProgress = false;
    bool m_updateShutdownRequested = false;
    bool m_restartRequested = false;
};

#endif // MAINWINDOW_H