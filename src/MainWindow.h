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
#include <QRect>
#include <QSize>
#include <QTimer>
#include <QMap>
#include <QPointer>
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
#include "ToolUiContainer.h"

#include <memory>

class QCloseEvent;
class QComboBox;
class QImage;
class QKeyEvent;
class QLineEdit;
class QHideEvent;
class QResizeEvent;
class QScreen;
class QShowEvent;
class QTreeWidget;
class QTreeWidgetItem;
class QVariantAnimation;
class ScreenAcrylicBackdrop;
class FullscreenIslandButton;
class FullscreenRadialMenu;
class ToolInterface;
class ToolScriptedHostController;
class WindowAviRecorder;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const ExternalPackageManager::PendingRequest& pendingRequest = ExternalPackageManager::PendingRequest(),
                        QWidget *parent = nullptr);
    ~MainWindow();

    void handleExternalRequest(const ExternalPackageManager::PendingRequest& request);
    bool ensureReadyForIncomingRequest(ExternalPackageManager::RequestType requestType);

protected:
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private slots:
    void onSettingsClicked();
    void onConfigClicked();
    void onToolsClicked();
    void onAccountClicked();
    void closeOverlay();
    void onToolSelected(const QString &toolId);

    // Actions from pages
    void onLanguageChanged();
    void applyPendingLanguageChange();
    void onThemeChanged();
    void onDebugModeChanged(bool enabled);
    void onSidebarCompactChanged(bool enabled);
    void updateMemoryUsage();
    void onModClosed();
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
    void applyDisplaySettings();
    QScreen* configuredDisplayScreen() const;
    QSize configuredWindowResolution(QScreen* targetScreen) const;
    void performShutdown();
    void processPendingExternalRequest();
    void processPendingToolRequest();
    void processPendingPluginRequest();
    void finishPendingRequest();
    bool restartApplicationForPendingRequest(const QString& restartMessage);
    void bringToFront();
    void resetActiveToolUi();
    void rebuildActiveToolUi();
    void setupRightSidebarUi();
    void clearRightSidebarContent();
    void resetRightSidebarForToolTransition();
    void rebuildRightSidebarRail(const QList<ToolRightSidebarButtonDefinition>& definitions,
                                 const ToolRightSidebarState& state);
    void activateDefaultRightSidebarList(bool notifyTool, bool refreshUi);
    void updateRightSidebarState(const ToolRightSidebarState& state);
    void applyRightSidebarSearchFilter();
    void syncV2RightSidebarContent();
    void handleV2RightSidebarItemActivated(QTreeWidgetItem* item, int column);
    void handleV2RightSidebarContextMenuRequested(const QPoint& pos);
    QString activeToolLocalizedString(const QString& key, const QString& fallback = QString()) const;
    void setRightSidebarListVisible(bool visible);
    void setRightSidebarSearchVisible(bool visible);
    void setRightSidebarExtraActionsVisible(bool visible);
    void updateRightSidebarSelectAllButtonText();
    void updateRightSidebarGeometries();
    void updateRightSidebarRailGeometry();
    void updateRightSidebarListGeometry();
    void updateWindowControlsOverlayGeometry();
    void raiseWindowControlsOverlay();
    bool isFullscreenPresentationActive() const;
    void applyFullscreenPresentation(bool enabled);
    void updateFullscreenChromeGeometry();
    void toggleFullscreenIslandMenu();
    void closeActiveToolFromFullscreenMenu();
    bool confirmCloseActiveTool();
    void closeActiveToolWithConfirmation();
    void exitFullscreenFromMenu();
    void syncSidebarNavigationPlacement();
    void applyNativeFullscreenWindow(QScreen* targetScreen);
    void restoreNativeWindowLayering();
    int currentRightSidebarListWidth() const;
    int normalizedRightSidebarListWidth(int width) const;
    QTreeWidget* currentRightSidebarListWidget() const;
    void setRightSidebarResizeDragging(bool dragging);
    QWidget* activeDashboardToolWidget() const;
    void setSidebarExpandedPresentation(bool expanded);
    void startSidebarWidthAnimation(int targetWidth, bool expanded);
    void repairActiveToolWidgetAfterSidebarAnimation();
    bool ensureRequiredGameDirectory();
    bool ensureRequiredDocumentDirectory();
    void captureWindowScreenshot();
    void prepareWindowCaptureFrame();
    QImage renderWindowScreenshot();
    void applyScreenshotWatermark(QImage& image) const;
    void applyWindowScreenshotAlphaMask(QImage& image) const;
    QString screenshotWatermarkText() const;
    void showScreenshotSuccessFeedback(const QString& outputPath);
    void playScreenshotShutterSound();
    void showScreenshotSuccessNotification(const QString& outputPath);
    void toggleWindowRecording();
    void startWindowRecording();
    void stopWindowRecording();
    void captureWindowRecordingFrame();
    void showRecordingSuccessFeedback(const QString& outputPath);
    void showRecordingSuccessNotification(const QString& outputPath);
    bool handleGlobalUtilityShortcut(QKeyEvent* keyEvent);
    bool clearCurrentTypingFocus();
    bool handleToolTopbarShortcut(QKeyEvent* keyEvent);
    bool isToolEditableInputFocused() const;
    static bool shortcutMatchesKeyEvent(const QString& shortcut, const QKeyEvent& keyEvent);
    void ensureNativeWindowAcceptsInput(const QString& reason);
    void applyNativeRoundedCorners(const QString& reason);
    void logNativeInputState(const QString& reason) const;
    void logToolInputChain(const QString& reason) const;
    static QIcon loadThemedSvgIcon(const QString& resourcePath, bool isDark, const QString& colorOverride = QString());

    // UI Components
    QWidget *m_centralWidget;
    ScreenAcrylicBackdrop *m_acrylicBackdrop = nullptr;
    QWidget *m_sidebar;
    QStackedWidget *m_mainStack;
    QWidget *m_dashboard;
    QWidget *m_dashboardContent;
    QPointer<QLabel> m_dashboardTitleLabel;
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
    QWidget *m_bottomAppIconContainer;
    QLabel *m_appTitle;
    QHBoxLayout *m_titleLayout;
    QWidget *m_sidebarTopSpacerSmall;
    QWidget *m_sidebarTopSpacerLarge;
    QWidget *m_sidebarNavigationContainer;
    QVBoxLayout *m_sidebarNavigationLayout;
    FullscreenIslandButton *m_fullscreenIslandButton;
    FullscreenRadialMenu *m_fullscreenRadialMenu;
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
    QTimer *m_recordingFrameTimer = nullptr;

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
    
    // Geometry update throttle timer
    QTimer *m_geometryUpdateThrottleTimer;

    // Dragging state
    bool m_dragging = false;
    QPoint m_dragPosition;

    QString m_currentLang;
    QString m_pendingLang;
    bool m_languageChangeQueued = false;
    bool m_languageChangeInProgress = false;
    bool m_sidebarExpanded = true;
    bool m_sidebarLocked = false;
    bool m_fullscreenPresentationActive = false;
    bool m_sidebarExpandedBeforeFullscreen = true;
    bool m_sidebarLockedBeforeFullscreen = false;
    QVariantAnimation *m_sidebarWidthAnimation = nullptr;
    QVariantAnimation *m_fullscreenIslandSpinAnimation = nullptr;
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
    QString m_rightSidebarActiveButtonKey;
    ToolInterface* m_activeTool = nullptr;
    ToolScriptedHostController* m_activeScriptedHostController = nullptr;
    bool m_toolSelectionInProgress = false;
    QWidget* m_activeScriptedTopbarWidget = nullptr;
    QWidget* m_activeScriptedSidebarWidget = nullptr;
    QTreeWidget* m_activeRightSidebarListWidget = nullptr;
    QString m_rightSidebarContentToolId;
    QString m_rightSidebarSearchText;
    QString m_rightSidebarLastModeId;
    QMap<int, int> m_rightSidebarSearchComboToColumn;
    QMap<QString, QToolButton*> m_rightSidebarRailButtons;
    QMap<QString, QIcon> m_rightSidebarEffectiveIconCache;

    // Tool UI Container - independent top-level widget for tool UI
    ToolUiContainer* m_toolUiContainer = nullptr;

    // Shutdown state
    bool m_shutdownInProgress = false;
    bool m_updateShutdownRequested = false;
    bool m_restartRequested = false;
    bool m_windowRecordingActive = false;
    std::unique_ptr<WindowAviRecorder> m_windowRecorder;

#ifdef Q_OS_WIN
    qintptr m_windowedStyle = 0;
    qintptr m_windowedExStyle = 0;
    qintptr m_nativeFullscreenHwnd = 0;
    QRect m_windowedGeometry;
    bool m_nativeFullscreenApplied = false;
#endif
};

#endif // MAINWINDOW_H
