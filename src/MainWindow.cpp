//-------------------------------------------------------------------------------------
// MainWindow.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "MainWindow.h"
#include "AgreementEvidenceManager.h"
#include "ConfigManager.h"
#include "CustomMessageBox.h"
#include "ExternalPackageManager.h"
#include "LocalizationManager.h"
#include "PathValidator.h"
#include "SetupDialog.h"
#include "FileManager.h"
#include "ToolManager.h"
#include "ToolProxyInterface.h"
#include "PluginManager.h"
#include "Logger.h"
#include "AuthManager.h"
#include "HttpClient.h"
#include "ToolRuntimeContext.h"
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFrame>
#include <QDebug>
#include <QApplication>
#include <QCoreApplication>
#include <QStyle>
#include <QPropertyAnimation>
#include <QCloseEvent>
#include <QShortcut>
#include <QKeySequence>
#include <QFile>
#include <QTextStream>
#include <QPixmap>
#include <QThread>
#include <QProcess>
#include <QLineEdit>
#include <QComboBox>
#include <QTreeWidget>
#include <QHeaderView>
#include <QSignalBlocker>
#include <windows.h>
#include <psapi.h>

namespace {
constexpr bool kDebugForceConnectionWarning = false;
constexpr int kRightSidebarRailWidth = 60;
constexpr int kRightSidebarDefaultListWidth = 190;
constexpr int kRightSidebarMaximumWidth = 440;
constexpr int kRightSidebarMinimumListWidth = 0;
constexpr int kRightSidebarResizeHandleWidth = 6;
constexpr int kRightSidebarTitleBarHeight = 40;
constexpr const char* kRightSidebarDefaultButtonKey = "__default__";
constexpr const char* kRightSidebarSearchButtonKey = "__search__";
}

MainWindow::MainWindow(const ExternalPackageManager::PendingRequest& pendingRequest, QWidget *parent)
    : QMainWindow(parent)
    , m_pendingExternalRequest(pendingRequest)
{
    m_currentLang = ConfigManager::instance().getLanguage();
    LocalizationManager::instance().loadLanguage(m_currentLang);
    
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setWindowIcon(QIcon(":/app.ico"));
    
    setupUi();
    setupDebugOverlay();
    setupRightSidebarUi();
    applyTheme();
    resize(1280, 720);
    setMinimumSize(1280, 720);

    m_sidebar->installEventFilter(this);
    m_rightSidebarResizeHandle->installEventFilter(this);
    m_mainStack->installEventFilter(this);
    m_dashboard->installEventFilter(this);

    // Setup sidebar collapse delay timer (1.5 seconds)
    m_sidebarCollapseTimer = new QTimer(this);
    m_sidebarCollapseTimer->setSingleShot(true);
    m_sidebarCollapseTimer->setInterval(500); // Change from 1500 to 500
    connect(m_sidebarCollapseTimer, &QTimer::timeout, this, &MainWindow::collapseSidebar);

    // Setup shortcut to toggle sidebar lock
    QShortcut *lockShortcut = new QShortcut(QKeySequence("Ctrl+B"), this);
    connect(lockShortcut, &QShortcut::activated, this, &MainWindow::toggleSidebarLock);

    // Connect path validation notifications. Monitoring is started later in the startup flow.
    connect(&PathValidator::instance(), &PathValidator::pathInvalid, this, &MainWindow::onPathInvalid);

    // Setup Loading Overlay
    m_loadingOverlay = new LoadingOverlay(m_centralWidget);
    LocalizationManager& loc = LocalizationManager::instance();
    m_loadingOverlay->setMessage(loc.getString("MainWindow", "LoadingFiles"));
    
    // Setup Update Overlay
    m_updateOverlay = new Update(m_centralWidget);
    connect(m_updateOverlay, &Update::updateShutdownRequested, this, &MainWindow::requestUpdateShutdown);
    
    // Setup Advertisement Overlay
    m_advertisementOverlay = new Advertisement(m_centralWidget);
    
    // Setup Login Overlay
    m_loginOverlay = new LoginDialog(m_centralWidget);
    connect(m_loginOverlay, &LoginDialog::loginSuccessful, this, &MainWindow::onLoginSuccessful);
    
    // Setup SetupDialog Overlay
    m_setupOverlay = new SetupDialog(m_centralWidget);
    connect(m_setupOverlay, &SetupDialog::setupCompleted, this, &MainWindow::onSetupCompleted);

    // Setup Connection Warning Overlay
    m_connectionWarningOverlay = new ConnectionWarningOverlay(m_centralWidget);
    connect(&AuthManager::instance(), SIGNAL(connectionLost()), this, SLOT(onConnectionLost()), Qt::QueuedConnection);
    connect(&AuthManager::instance(), SIGNAL(connectionRestored()), this, SLOT(onConnectionRestored()), Qt::QueuedConnection);
    connect(&AuthManager::instance(), SIGNAL(accountActionBlocked()), this, SLOT(onAccountActionBlocked()), Qt::QueuedConnection);
    connect(&AuthManager::instance(), SIGNAL(accountActionCleared()), this, SLOT(onAccountActionCleared()), Qt::QueuedConnection);

    // Setup User Agreement Overlay
    m_userAgreementOverlay = new UserAgreementOverlay(m_centralWidget);
    connect(m_userAgreementOverlay, &UserAgreementOverlay::agreementAccepted, this, [this]() {
        // Continue startup on the next event loop turn so the agreement overlay can hide first.
        QTimer::singleShot(0, this, [this]() {
            if (AuthManager::instance().isAuthenticated()) {
                onLoginSuccessful();
            } else if (AuthManager::instance().hasSavedCredentials()) {
                // Have saved credentials, trigger auto login
                m_loginOverlay->showAutoLoggingIn();
                AuthManager::instance().autoLogin();

                // Wait for login result
                connect(&AuthManager::instance(), &AuthManager::loginSuccess, this, [this]() {
                    onLoginSuccessful();
                }, Qt::SingleShotConnection);

                // No need to handle loginFailed here, LoginDialog will automatically
                // switch to the manual login form when login fails
            } else {
                m_loginOverlay->showLogin();
            }
        });
    });
    
    // Setup scan check timer - poll every 500ms to check if scanning is complete
    m_scanCheckTimer = new QTimer(this);
    m_scanCheckTimer->setInterval(500);
    connect(m_scanCheckTimer, &QTimer::timeout, this, [this]() {
        if (!FileManager::instance().isScanning()) {
            Logger::instance().logInfo("MainWindow", "Scan complete detected via polling - hiding overlay");
            m_scanCheckTimer->stop();
            m_loadingOverlay->hideOverlay();

            Logger::instance().logInfo("MainWindow", "File loading completed, scanning plugins");
            PluginManager::instance().loadPlugins();

            if (ToolManager::instance().getTools().isEmpty()) {
                Logger::instance().logInfo("MainWindow", "File loading completed, scanning tools");
                ToolManager::instance().loadTools();
            }

            Logger::instance().logInfo("MainWindow", "File loading completed, refreshing tools page");
            m_toolsPage->refreshTools();

            processPendingExternalRequest();

            // Show advertisement only on first startup load
            if (!m_firstLoadCompleted) {
                m_firstLoadCompleted = true;
                Logger::instance().logInfo("MainWindow", "First load completed, showing advertisement");
                QTimer::singleShot(500, this, [this]() {
                    m_advertisementOverlay->showAd();
                });
            } else {
                Logger::instance().logInfo("MainWindow", "Subsequent load - skipping advertisement");
            }
        }
    });
    
    // Connect AuthManager ad signal
    connect(&AuthManager::instance(), &AuthManager::adReceived, this, [this](const QString& text, const QString& imageUrl, const QString& targetUrl) {
        m_advertisementOverlay->showAdWithData(text, imageUrl, targetUrl);
    });
    
    // Connect ad fetch failed signal - files are already loaded at this point,
    // so just log the failure without re-triggering file loading
    connect(m_advertisementOverlay, &Advertisement::adFetchFailed, this, [this]() {
        Logger::instance().logInfo("MainWindow", "Ad fetch failed, files already loaded - no action needed");
    }, Qt::SingleShotConnection);
    
    // Check User Agreement on startup as soon as the first event loop turn starts
    QTimer::singleShot(0, this, [this]() {
        m_userAgreementOverlay->checkAgreement();
    });

    // Connect tool crash signal
    connect(&ToolManager::instance(), &ToolManager::toolProcessCrashed,
            this, &MainWindow::onToolProcessCrashed);
    // Connect question dialog request signal (for tools to show CustomMessageBox)
    connect(&ToolManager::instance(), &ToolManager::questionDialogRequested,
            this, [this](const QString& title, const QString& message,
                         std::function<void(bool)> callback) {
        auto result = CustomMessageBox::question(this, title, message);
        callback(result == QMessageBox::Yes);
    });
    
    if (ConfigManager::instance().getSidebarCompactMode()) {
        m_sidebarExpanded = false;
        m_sidebar->setFixedWidth(60);
        m_sidebarLayout->setContentsMargins(0, 20, 0, 20); // Fix initial margin
        m_appTitle->hide();
        m_appIcon->hide();
        m_bottomAppIcon->show();
        m_controlsHorizontal->hide();
        m_controlsVertical->show();
        m_toolsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_accountBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_settingsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_configBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_titleLayout->setAlignment(Qt::AlignCenter);
        m_toolsBtn->setText("");
        m_accountBtn->setText("");
        m_settingsBtn->setText("");
        m_configBtn->setText("");
    }
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUi() {
    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("CentralWidget");
    setCentralWidget(m_centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupSidebar();
    mainLayout->addWidget(m_sidebar);

    m_mainStack = new QStackedWidget(this);
    
    m_dashboard = new QWidget();
    m_dashboard->setObjectName("Dashboard");

    QHBoxLayout *dashLayout = new QHBoxLayout(m_dashboard);
    dashLayout->setContentsMargins(0, 0, 0, 0);
    dashLayout->setSpacing(0);

    m_dashboardContent = new QWidget(m_dashboard);
    m_dashboardContent->setObjectName("DashboardContent");
    QVBoxLayout *contentLayout = new QVBoxLayout(m_dashboardContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    LocalizationManager& loc = LocalizationManager::instance();
    m_dashboardTitleLabel = new QLabel(loc.getString("MainWindow", "DashboardArea"), m_dashboardContent);
    m_dashboardTitleLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(m_dashboardTitleLabel);
    dashLayout->addWidget(m_dashboardContent);

    m_rightSidebarPanel = new QWidget(m_dashboard);
    m_rightSidebarPanel->setObjectName("RightSidebarPanel");
    m_rightSidebarPanel->hide();
    QVBoxLayout *panelLayout = new QVBoxLayout(m_rightSidebarPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    m_rightSidebarHeaderArea = new QWidget(m_rightSidebarPanel);
    m_rightSidebarHeaderArea->setObjectName("RightSidebarHeaderArea");
    QVBoxLayout *headerLayout = new QVBoxLayout(m_rightSidebarHeaderArea);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    m_rightSidebarTitleLabel = new QLabel(m_rightSidebarHeaderArea);
    m_rightSidebarTitleLabel->setObjectName("RightSidebarTitle");
    m_rightSidebarTitleLabel->setFixedHeight(kRightSidebarTitleBarHeight);
    m_rightSidebarTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    headerLayout->addWidget(m_rightSidebarTitleLabel);

    QWidget *rightSidebarOperationsArea = new QWidget(m_rightSidebarHeaderArea);
    rightSidebarOperationsArea->setObjectName("RightSidebarOperationsArea");
    QVBoxLayout *operationsLayout = new QVBoxLayout(rightSidebarOperationsArea);
    operationsLayout->setContentsMargins(0, 0, 0, 0);
    operationsLayout->setSpacing(0);

    m_rightSidebarSearchContainer = new QWidget(rightSidebarOperationsArea);
    m_rightSidebarSearchContainer->setObjectName("RightSidebarSearchContainer");
    QVBoxLayout *searchLayout = new QVBoxLayout(m_rightSidebarSearchContainer);
    searchLayout->setContentsMargins(16, 10, 16, 0);
    searchLayout->setSpacing(8);
    searchLayout->setAlignment(Qt::AlignTop);

    m_rightSidebarSearchEdit = new QLineEdit(m_rightSidebarSearchContainer);
    connect(m_rightSidebarSearchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onRightSidebarSearchTextChanged);
    searchLayout->addWidget(m_rightSidebarSearchEdit);

    m_rightSidebarSearchColumnCombo = new QComboBox(m_rightSidebarSearchContainer);
    connect(m_rightSidebarSearchColumnCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onRightSidebarSearchColumnChanged);
    searchLayout->addWidget(m_rightSidebarSearchColumnCombo);

    operationsLayout->addWidget(m_rightSidebarSearchContainer);

    m_rightSidebarExtraActionsContainer = new QWidget(rightSidebarOperationsArea);
    m_rightSidebarExtraActionsContainer->setObjectName("RightSidebarExtraActionsContainer");
    QVBoxLayout *extraActionsLayout = new QVBoxLayout(m_rightSidebarExtraActionsContainer);
    extraActionsLayout->setContentsMargins(16, 8, 16, 10);
    extraActionsLayout->setSpacing(0);
    extraActionsLayout->setAlignment(Qt::AlignTop);

    m_rightSidebarSelectAllBtn = new QPushButton(m_rightSidebarExtraActionsContainer);
    m_rightSidebarSelectAllBtn->setObjectName("RightSidebarSelectAllButton");
    connect(m_rightSidebarSelectAllBtn, &QPushButton::clicked,
            this, &MainWindow::onRightSidebarSelectAllClicked);
    extraActionsLayout->addWidget(m_rightSidebarSelectAllBtn);

    operationsLayout->addWidget(m_rightSidebarExtraActionsContainer);
    headerLayout->addWidget(rightSidebarOperationsArea);
    panelLayout->addWidget(m_rightSidebarHeaderArea);

    m_rightSidebarContentContainer = new QWidget(m_rightSidebarPanel);
    m_rightSidebarContentContainer->setObjectName("RightSidebarContentContainer");
    m_rightSidebarContentLayout = new QVBoxLayout(m_rightSidebarContentContainer);
    m_rightSidebarContentLayout->setContentsMargins(0, 0, 0, 0);
    m_rightSidebarContentLayout->setSpacing(0);
    panelLayout->addWidget(m_rightSidebarContentContainer, 1);

    m_rightSidebarResizeHandle = new QWidget(m_dashboard);
    m_rightSidebarResizeHandle->setObjectName("RightSidebarResizeHandle");
    m_rightSidebarResizeHandle->setCursor(Qt::SizeHorCursor);
    m_rightSidebarResizeHandle->hide();

    m_rightSidebarRail = new QWidget(m_dashboard);
    m_rightSidebarRail->setObjectName("RightSidebarRail");
    m_rightSidebarRail->setFixedWidth(m_rightSidebarRailWidth);
    m_rightSidebarRailLayout = new QVBoxLayout(m_rightSidebarRail);
    m_rightSidebarRailLayout->setContentsMargins(8, 8, 8, 8);
    m_rightSidebarRailLayout->setSpacing(8);

    m_mainStack->addWidget(m_dashboard);

    m_settingsPage = new SettingsPage();
    m_settingsPage->setObjectName("OverlayContainer");
    connect(m_settingsPage, &SettingsPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_settingsPage, &SettingsPage::languageChanged, this, &MainWindow::onLanguageChanged);
    connect(m_settingsPage, &SettingsPage::themeChanged, this, &MainWindow::onThemeChanged);
    connect(m_settingsPage, &SettingsPage::debugModeChanged, this, &MainWindow::onDebugModeChanged);
    connect(m_settingsPage, &SettingsPage::sidebarCompactChanged, this, &MainWindow::onSidebarCompactChanged);
    connect(m_settingsPage, &SettingsPage::showUserAgreement, this, [this]() {
        m_userAgreementOverlay->showAgreement(true);
    });
    m_mainStack->addWidget(m_settingsPage);

    m_configPage = new ConfigPage();
    m_configPage->setObjectName("OverlayContainer");
    connect(m_configPage, &ConfigPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_configPage, &ConfigPage::modClosed, this, &MainWindow::onModClosed);
    connect(m_configPage, &ConfigPage::gamePathChanged, this, &MainWindow::onGamePathChanged);
    connect(m_configPage, &ConfigPage::modPathChanged, this, &MainWindow::onModPathChanged);
    m_mainStack->addWidget(m_configPage);

    m_toolsPage = new ToolsPage();
    m_toolsPage->setObjectName("OverlayContainer");
    connect(m_toolsPage, &ToolsPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_toolsPage, &ToolsPage::toolSelected, this, &MainWindow::onToolSelected);
    m_mainStack->addWidget(m_toolsPage);

    m_accountPage = new AccountPage();
    m_accountPage->setObjectName("OverlayContainer");
    connect(m_accountPage, &AccountPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_accountPage, &AccountPage::logoutRequested, this, &MainWindow::onLogoutRequested);
    m_mainStack->addWidget(m_accountPage);

    mainLayout->addWidget(m_mainStack);

    updateTexts();
}

void MainWindow::setupSidebar() {
    m_sidebar = new QWidget(this);
    m_sidebar->setObjectName("Sidebar");
    m_sidebar->setFixedWidth(250);

    m_sidebarLayout = new QVBoxLayout(m_sidebar);
    m_sidebarLayout->setContentsMargins(20, 20, 20, 20);
    m_sidebarLayout->setSpacing(10);

    // Window Controls
    m_sidebarControlsContainer = new QWidget(m_sidebar);
    QVBoxLayout *controlsContainerLayout = new QVBoxLayout(m_sidebarControlsContainer);
    controlsContainerLayout->setContentsMargins(0, 0, 0, 0);
    controlsContainerLayout->setSpacing(0);

    auto createControlBtn = [](const QString &color, const QString &hoverColor) -> QPushButton* {
        QPushButton *btn = new QPushButton();
        btn->setFixedSize(12, 12);
        btn->setStyleSheet(QString("QPushButton { background-color: %1; border-radius: 6px; border: none; } QPushButton:hover { background-color: %2; }").arg(color, hoverColor));
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    // Horizontal
    m_controlsHorizontal = new QWidget();
    QHBoxLayout *hLayout = new QHBoxLayout(m_controlsHorizontal);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(8);
    QPushButton *closeBtnH = createControlBtn("#FF5F57", "#FF3B30");
    QPushButton *minBtnH = createControlBtn("#FFBD2E", "#FFAD1F");
    QPushButton *maxBtnH = createControlBtn("#28C940", "#24B538");
    connect(closeBtnH, &QPushButton::clicked, this, &MainWindow::closeWindow);
    connect(minBtnH, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    // connect(maxBtnH, &QPushButton::clicked, this, &MainWindow::maximizeWindow); // Disabled
    hLayout->addWidget(closeBtnH);
    hLayout->addWidget(minBtnH);
    hLayout->addWidget(maxBtnH);
    hLayout->addStretch();

    // Vertical
    m_controlsVertical = new QWidget();
    QVBoxLayout *vLayout = new QVBoxLayout(m_controlsVertical);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(8);
    vLayout->setAlignment(Qt::AlignHCenter);
    QPushButton *closeBtnV = createControlBtn("#FF5F57", "#FF3B30");
    QPushButton *minBtnV = createControlBtn("#FFBD2E", "#FFAD1F");
    QPushButton *maxBtnV = createControlBtn("#28C940", "#24B538");
    connect(closeBtnV, &QPushButton::clicked, this, &MainWindow::closeWindow);
    connect(minBtnV, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    // connect(maxBtnV, &QPushButton::clicked, this, &MainWindow::maximizeWindow); // Disabled
    vLayout->addWidget(closeBtnV);
    vLayout->addWidget(minBtnV);
    vLayout->addWidget(maxBtnV);
    m_controlsVertical->hide();

    controlsContainerLayout->addWidget(m_controlsHorizontal);
    controlsContainerLayout->addWidget(m_controlsVertical);
    
    m_sidebarLayout->addWidget(m_sidebarControlsContainer);
    m_sidebarLayout->addSpacing(20);

    // App Icon & Title (Expanded)
    m_titleLayout = new QHBoxLayout();
    m_appIcon = new QLabel();
    m_appIcon->setPixmap(QIcon(":/app.ico").pixmap(40, 40));
    m_appIcon->setFixedSize(40, 40);
    m_appIcon->setAlignment(Qt::AlignCenter);
    
    m_appTitle = new QLabel("APE HOI4\nTool Studio");
    m_appTitle->setObjectName("SidebarTitle");
    m_appTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    m_titleLayout->addWidget(m_appIcon);
    m_titleLayout->addWidget(m_appTitle);
    m_titleLayout->addStretch(); 
    m_sidebarLayout->addLayout(m_titleLayout);

    m_sidebarLayout->addStretch();

    // Navigation Buttons (QToolButton)
    m_toolsBtn = new QToolButton(this);
    m_toolsBtn->setObjectName("SidebarButton");
    m_toolsBtn->setCursor(Qt::PointingHandCursor);
    m_toolsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_toolsBtn->setIcon(loadThemedSvgIcon(":/icons/toolbox.svg", ConfigManager::instance().isCurrentThemeDark()));
    m_toolsBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_toolsBtn, &QPushButton::clicked, this, &MainWindow::onToolsClicked);
    m_sidebarLayout->addWidget(m_toolsBtn);

    m_accountBtn = new QToolButton(this);
    m_accountBtn->setObjectName("SidebarButton");
    m_accountBtn->setCursor(Qt::PointingHandCursor);
    m_accountBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_accountBtn->setIcon(loadThemedSvgIcon(":/icons/user.svg", ConfigManager::instance().isCurrentThemeDark()));
    m_accountBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_accountBtn, &QPushButton::clicked, this, &MainWindow::onAccountClicked);
    m_sidebarLayout->addWidget(m_accountBtn);

    m_settingsBtn = new QToolButton(this);
    m_settingsBtn->setObjectName("SidebarButton");
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly); 
    m_settingsBtn->setIcon(loadThemedSvgIcon(":/icons/settings.svg", ConfigManager::instance().isCurrentThemeDark()));
    m_settingsBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    m_sidebarLayout->addWidget(m_settingsBtn);

    m_configBtn = new QToolButton(this);
    m_configBtn->setObjectName("SidebarButton");
    m_configBtn->setCursor(Qt::PointingHandCursor);
    m_configBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_configBtn->setIcon(loadThemedSvgIcon(":/icons/folder.svg", ConfigManager::instance().isCurrentThemeDark()));
    m_configBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_configBtn, &QPushButton::clicked, this, &MainWindow::onConfigClicked);
    m_sidebarLayout->addWidget(m_configBtn);

    // Bottom App Icon (Collapsed)
    m_bottomAppIcon = new QLabel();
    m_bottomAppIcon->setPixmap(QIcon(":/app.ico").pixmap(40, 40));
    m_bottomAppIcon->setFixedSize(40, 40);
    m_bottomAppIcon->setAlignment(Qt::AlignCenter);
    m_bottomAppIcon->hide(); // Initially hidden
    
    QHBoxLayout *bottomIconLayout = new QHBoxLayout();
    bottomIconLayout->addStretch();
    bottomIconLayout->addWidget(m_bottomAppIcon);
    bottomIconLayout->addStretch();
    m_sidebarLayout->addLayout(bottomIconLayout);
}

void MainWindow::setupDebugOverlay() {
    m_memUsageLabel = new QLabel(this);
    m_memUsageLabel->setObjectName("DebugOverlay");
    m_memUsageLabel->setStyleSheet("background-color: rgba(0, 0, 0, 150); color: #00FF00; padding: 5px; border-radius: 5px; font-family: Consolas; font-weight: bold;");
    m_memUsageLabel->hide();
    
    m_memTimer = new QTimer(this);
    connect(m_memTimer, &QTimer::timeout, this, &MainWindow::updateMemoryUsage);
    
    if (ConfigManager::instance().getDebugMode()) {
        m_memUsageLabel->show();
        m_memTimer->start(1000);
    }
}

void MainWindow::updateMemoryUsage() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        double memMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
        int fileCount = FileManager::instance().getFileCount();
        m_memUsageLabel->setText(QString("RAM: %1 MB | Files: %2").arg(memMB, 0, 'f', 1).arg(fileCount));
        m_memUsageLabel->adjustSize();
        m_memUsageLabel->move(width() - m_memUsageLabel->width() - 20, height() - m_memUsageLabel->height() - 20);
    }
}

void MainWindow::applyTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    
    QString bg = isDark ? "#2C2C2E" : "#F5F5F7";
    QString sidebarBg = isDark ? "#2C2C2E" : "#F5F5F7";
    QString text = isDark ? "#FFFFFF" : "#1D1D1F";
    QString border = isDark ? "#3A3A3C" : "#D2D2D7";
    QString rowBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString rowHover = isDark ? "#3A3A3C" : "rgba(0, 0, 0, 0.05)";
    QString iconBg = isDark ? "#3A3A3C" : "#EEEEEE";
    QString treeItemHover = isDark ? "#3A3A3C" : "#E8E8E8";
    QString treeItemSelected = isDark ? "#0A84FF" : "#007AFF";
    QString comboIndicator = isDark ? "#FFFFFF" : "#1D1D1F";
    
    QString style = QString(R"(
        QWidget#CentralWidget { background-color: %1; border: 1px solid %4; border-radius: 10px; }
        QWidget#Sidebar { background-color: %2; border-right: 1px solid %4; border-top-left-radius: 10px; border-bottom-left-radius: 10px; }
        QWidget#OverlayContainer { background-color: %2; border-top-right-radius: 10px; border-bottom-right-radius: 10px; }
        QWidget#Dashboard { background-color: %1; border-top-right-radius: 10px; border-bottom-right-radius: 10px; }
        QWidget#DashboardContent { background-color: %1; border-top-right-radius: 10px; border-bottom-right-radius: 10px; }

        QLabel { color: %3; }
        QLabel#SidebarTitle { font-size: 16px; font-weight: 800; }
        QLabel#SettingsTitle, QLabel#ConfigTitle, QLabel#ToolsTitle { font-size: 18px; font-weight: bold; }
        
        QToolButton#SidebarButton {
            color: %3; background-color: transparent; text-align: center; padding: 10px; border-radius: 8px; border: none;
        }
        QToolButton#SidebarButton:hover { background-color: %6; }
        
        QWidget#SettingRow {
            background-color: %5; border: 1px solid %4; border-radius: 8px;
        }
        
        QLabel#SettingIcon {
            background-color: %7; border-radius: 10px; color: %3;
        }
        
        QComboBox {
            border: 1px solid %4; 
            border-radius: 6px; 
            padding: 5px 24px 5px 12px; 
            background-color: %5; 
            color: %3;
        }
        QComboBox:hover {
            background-color: %6;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: none;
        }
        QComboBox QAbstractItemView {
            border: 1px solid %4;
            border-radius: 6px;
            background-color: %5;
            color: %3;
            selection-background-color: #007AFF;
            selection-color: white;
            padding: 4px;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            padding: 6px 12px;
            border-left: 3px solid transparent;
            color: %3;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: %6;
            border-left: 3px solid %10;
            color: %3;
        }

        QPushButton#GithubLink, QPushButton#OpenSourceBtn, QPushButton#LicenseLink {
            color: #007AFF; text-align: left; background-color: transparent; border: none; font-weight: bold;
        }
        QPushButton#GithubLink:hover, QPushButton#OpenSourceBtn:hover, QPushButton#LicenseLink:hover {
            color: #0051A8;
        }

        QCheckBox::indicator {
            width: 18px; height: 18px; border-radius: 4px; border: 1px solid %4; background-color: %5;
        }
        QCheckBox::indicator:checked {
            background-color: #007AFF; border: 1px solid #007AFF;
            image: url(:/checkmark.svg); /* Ideally need a checkmark icon, or just color */
        }
        
        QTreeWidget {
            background-color: %2; border: none; color: %3;
        }
        QTreeWidget::item {
            padding: 5px;
        }
        QTreeWidget::item:hover {
            background-color: %8;
        }
        QTreeWidget::item:selected {
            background-color: %9; color: white;
        }
        QHeaderView::section {
            background-color: %2; color: %3; border: none; padding: 5px;
        }
        
        QScrollArea {
            background-color: transparent; border: none;
        }
        QScrollArea > QWidget > QWidget {
            background-color: transparent;
        }
        
        QToolTip {
            background-color: %2; color: %3; border: 1px solid %4; padding: 5px; border-radius: 4px;
        }
        
        /* Mac-style context menu */
        QMenu {
            background-color: %5;
            border: 1px solid %4;
            border-radius: 6px;
            padding: 4px 0px;
        }
        QMenu::item {
            padding: 6px 20px;
            color: %3;
            background-color: transparent;
        }
        QMenu::item:selected {
            background-color: #007AFF;
            color: white;
            border-radius: 4px;
            margin: 2px 4px;
        }
        QMenu::item:disabled {
            color: #888888;
        }
        QMenu::separator {
            height: 1px;
            background-color: %4;
            margin: 4px 8px;
        }
        
        /* Mac-style scrollbar */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 4px 2px 4px 2px;
        }
        QScrollBar::handle:vertical {
            background: rgba(128, 128, 128, 0.4);
            min-height: 30px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(128, 128, 128, 0.6);
        }
        QScrollBar::handle:vertical:pressed {
            background: rgba(128, 128, 128, 0.8);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
        
        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 2px 4px 2px 4px;
        }
        QScrollBar::handle:horizontal {
            background: rgba(128, 128, 128, 0.4);
            min-width: 30px;
            border-radius: 3px;
        }
        QScrollBar::handle:horizontal:hover {
            background: rgba(128, 128, 128, 0.6);
        }
        QScrollBar::handle:horizontal:pressed {
            background: rgba(128, 128, 128, 0.8);
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )").arg(bg, sidebarBg, text, border, rowBg, rowHover, iconBg, treeItemHover, treeItemSelected).replace("%10", comboIndicator);
    
    setStyleSheet(style);
    
    m_toolsBtn->setIcon(loadThemedSvgIcon(":/icons/toolbox.svg", isDark));
    m_accountBtn->setIcon(loadThemedSvgIcon(":/icons/user.svg", isDark));
    m_settingsBtn->setIcon(loadThemedSvgIcon(":/icons/settings.svg", isDark));
    m_configBtn->setIcon(loadThemedSvgIcon(":/icons/folder.svg", isDark));

    m_dashboardContent->setStyleSheet(QString("QWidget#DashboardContent { background-color: %1; border-top-right-radius: 10px; border-bottom-right-radius: 10px; }").arg(bg));

    const QString railButtonColor = isDark ? "#F2F2F7" : "#1D1D1F";
    const QString railButtonHover = isDark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.05)";
    const QString railButtonActive = isDark ? "#0A84FF" : "#007AFF";
    const QString railBackground = isDark ? "#1F1F21" : "#ECECEF";
    const QString sidebarPanelBackground = isDark ? "#2C2C2E" : "#F5F5F7";
    const QString inputBackground = isDark ? "#3A3A3C" : "#FFFFFF";

    m_rightSidebarPanel->setStyleSheet(QString(
        "QWidget#RightSidebarPanel { background-color: %1; border-left: none; }"
        "QWidget#RightSidebarHeaderArea { background-color: %1; border-bottom: none; }"
        "QLabel#RightSidebarTitle { color: %3; font-size: 13px; font-weight: 600; min-height: %9px; max-height: %9px; padding: 0 16px; border-bottom: 1px solid %2; }"
        "QWidget#RightSidebarOperationsArea { background-color: transparent; border-bottom: 1px solid %2; }"
        "QWidget#RightSidebarContentContainer { background-color: %1; }"
        "QWidget#RightSidebarSearchContainer { background-color: transparent; border-bottom: none; }"
        "QWidget#RightSidebarExtraActionsContainer { background-color: transparent; }"
        "QPushButton#RightSidebarSelectAllButton { color: %3; background-color: transparent; border: 1px solid %2; border-radius: 6px; padding: 6px 10px; text-align: left; }"
        "QPushButton#RightSidebarSelectAllButton:hover { background-color: %4; }"
        "QWidget#RightSidebarSearchContainer QLineEdit {"
        " background-color: %7; color: %3; border: 1px solid %2; border-radius: 6px; padding: 6px 10px;"
        "}"
        "QWidget#RightSidebarSearchContainer QLineEdit::placeholder { color: %8; }"
        "QWidget#RightSidebarSearchContainer QComboBox {"
        " background-color: %7; color: %3; border: 1px solid %2; border-radius: 6px; padding: 6px 24px 6px 10px;"
        "}"
    ).arg(sidebarPanelBackground, border, text, railButtonHover, railBackground, railButtonActive, inputBackground, isDark ? "#98989D" : "#6E6E73").arg(kRightSidebarTitleBarHeight));

    m_rightSidebarRail->setStyleSheet(QString(
        "QWidget#RightSidebarRail { background-color: %1; border-left: 1px solid %2; border-top-right-radius: 10px; border-bottom-right-radius: 10px; }"
        "QToolButton#RightSidebarRailButton { border: none; border-radius: 8px; padding: 0px; background-color: transparent; }"
        "QToolButton#RightSidebarRailButton:hover { background-color: %3; }"
        "QToolButton#RightSidebarRailButton:checked { background-color: %4; }"
    ).arg(railBackground, border, railButtonHover, railButtonActive));

    m_rightSidebarResizeHandle->setStyleSheet(QString(
        "QWidget#RightSidebarResizeHandle { background-color: transparent; border-left: 1px solid %1; }"
    ).arg(border));

    for (auto it = m_rightSidebarRailButtons.begin(); it != m_rightSidebarRailButtons.end(); ++it) {
        QToolButton* button = it.value();
        if (!button) {
            continue;
        }

        const QString color = button->isChecked() ? "#FFFFFF" : railButtonColor;
        button->setIcon(loadThemedSvgIcon(button->property("iconResource").toString(), isDark, color));
        button->setIconSize(QSize(20, 20));
    }
}

QIcon MainWindow::loadThemedSvgIcon(const QString& resourcePath, bool isDark, const QString& colorOverride) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QIcon(resourcePath);
    }

    QString svgContent = QTextStream(&file).readAll();
    file.close();

    const QString color = colorOverride.isEmpty() ? (isDark ? "#FFFFFF" : "#1D1D1F") : colorOverride;
    svgContent.replace("currentColor", color);

    QPixmap pixmap;
    pixmap.loadFromData(svgContent.toUtf8(), "SVG");
    return QIcon(pixmap);
}

void MainWindow::setupRightSidebarUi() {
    if (!m_rightSidebarTitleLabel || !m_rightSidebarSearchEdit || !m_rightSidebarSelectAllBtn) {
        return;
    }

    LocalizationManager& loc = LocalizationManager::instance();
    m_rightSidebarTitleLabel->setText(loc.getString("MainWindow", "RightSidebarDefaultTitle"));
    m_rightSidebarSearchEdit->setPlaceholderText(loc.getString("MainWindow", "RightSidebarSearchPlaceholder"));
    m_rightSidebarSearchColumnCombo->setPlaceholderText(loc.getString("MainWindow", "RightSidebarSearchAllColumns"));
    m_rightSidebarSelectAllBtn->setText(loc.getString("MainWindow", "RightSidebarSelectAll"));
    m_rightSidebarSearchContainer->hide();
    m_rightSidebarExtraActionsContainer->hide();
    m_rightSidebarListWidth = kRightSidebarDefaultListWidth;
    m_rightSidebarListVisible = false;
    m_rightSidebarPanel->hide();
    m_rightSidebarResizeHandle->hide();
    m_rightSidebarRail->show();

    ToolRightSidebarState emptyState;
    emptyState.listVisible = false;
    emptyState.searchModeAvailable = false;
    emptyState.searchModeActive = false;
    emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    rebuildRightSidebarRail({}, emptyState);
    updateRightSidebarGeometries();
}

void MainWindow::clearRightSidebarContent() {
    QLayoutItem *child = nullptr;
    while ((child = m_rightSidebarContentLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->hide();
            child->widget()->deleteLater();
        }
        delete child;
    }
    m_activeRightSidebarListWidget = nullptr;
}

void MainWindow::rebuildRightSidebarRail(const QList<ToolRightSidebarButtonDefinition>& definitions,
                                         const ToolRightSidebarState& state) {
    while (QLayoutItem* item = m_rightSidebarRailLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_rightSidebarRailButtons.clear();

    const bool isDark = ConfigManager::instance().isCurrentThemeDark();
    QMap<QString, ToolRightSidebarButtonDefinition> definitionMap;
    QStringList orderedKeys;

    const bool hasManagedList = (m_activeRightSidebarListWidget != nullptr)
        || state.listVisible
        || state.searchModeAvailable
        || state.showSelectAllButton
        || !state.searchableColumns.isEmpty();

    if (hasManagedList) {
        ToolRightSidebarButtonDefinition defaultButton;
        defaultButton.key = QString::fromUtf8(kRightSidebarDefaultButtonKey);
        defaultButton.iconResource = ":/icons/sidebar.svg";
        defaultButton.tooltip = LocalizationManager::instance().getString("MainWindow", "RightSidebarDefaultButtonTooltip");
        definitionMap.insert(defaultButton.key, defaultButton);
        orderedKeys.append(defaultButton.key);
    }

    if (state.searchModeAvailable) {
        ToolRightSidebarButtonDefinition searchButton;
        searchButton.key = QString::fromUtf8(kRightSidebarSearchButtonKey);
        searchButton.iconResource = ":/icons/search.svg";
        searchButton.tooltip = LocalizationManager::instance().getString("MainWindow", "RightSidebarSearchButtonTooltip");
        definitionMap.insert(searchButton.key, searchButton);
        orderedKeys.append(searchButton.key);
    }

    for (const ToolRightSidebarButtonDefinition& definition : definitions) {
        if (definition.key.isEmpty()) {
            continue;
        }
        definitionMap.insert(definition.key, definition);
        if (!orderedKeys.contains(definition.key)) {
            orderedKeys.append(definition.key);
        }
    }

    if (!state.orderedButtonKeys.isEmpty()) {
        QStringList mergedKeys;
        for (const QString& key : state.orderedButtonKeys) {
            if (definitionMap.contains(key) && !mergedKeys.contains(key)) {
                mergedKeys.append(key);
            }
        }
        for (const QString& key : orderedKeys) {
            if (!mergedKeys.contains(key)) {
                mergedKeys.append(key);
            }
        }
        orderedKeys = mergedKeys;
    }

    for (const QString& key : orderedKeys) {
        const ToolRightSidebarButtonDefinition definition = definitionMap.value(key);
        QToolButton* button = new QToolButton(m_rightSidebarRail);
        button->setObjectName("RightSidebarRailButton");
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(definition.tooltip);
        button->setFixedSize(44, 44);
        button->setProperty("iconResource", definition.iconResource);
        m_rightSidebarRailLayout->addWidget(button, 0, Qt::AlignHCenter);
        m_rightSidebarRailButtons.insert(definition.key, button);

        connect(button, &QToolButton::clicked, this, [this, key = definition.key]() {
            if (!m_activeTool) {
                return;
            }
            if (currentRightSidebarListWidth() <= 0) {
                m_rightSidebarRestoreRequested = true;
            }
            if (key == QString::fromUtf8(kRightSidebarDefaultButtonKey)) {
                m_rightSidebarSearchText.clear();
                if (m_rightSidebarSearchEdit) {
                    m_rightSidebarSearchEdit->clear();
                }
                if (m_rightSidebarSearchColumnCombo) {
                    m_rightSidebarSearchColumnCombo->setCurrentIndex(0);
                }
            }
            m_activeTool->handleRightSidebarButton(key);
            refreshActiveToolRightSidebarUi();
            if (key == QString::fromUtf8(kRightSidebarDefaultButtonKey)) {
                applyRightSidebarSearchFilter();
            }
        });
    }

    m_rightSidebarRailLayout->addStretch(1);
    updateRightSidebarState(state);

    for (auto it = m_rightSidebarRailButtons.begin(); it != m_rightSidebarRailButtons.end(); ++it) {
        QToolButton* button = it.value();
        if (!button) {
            continue;
        }
        const QString color = button->isChecked() ? "#FFFFFF" : (isDark ? "#F2F2F7" : "#1D1D1F");
        button->setIcon(loadThemedSvgIcon(button->property("iconResource").toString(), isDark, color));
        button->setIconSize(QSize(20, 20));
    }
}

void MainWindow::setRightSidebarListVisible(bool visible) {
    m_rightSidebarListVisible = visible;
    m_rightSidebarPanel->setVisible(visible);
    m_rightSidebarResizeHandle->setVisible(visible);
    m_rightSidebarRail->setVisible(true);
    updateRightSidebarGeometries();
}

void MainWindow::setRightSidebarSearchVisible(bool visible) {
    m_rightSidebarSearchContainer->setVisible(visible);
    if (!visible) {
        m_rightSidebarSearchText.clear();
        if (m_rightSidebarSearchEdit) {
            m_rightSidebarSearchEdit->clear();
        }
        if (m_rightSidebarSearchColumnCombo && m_rightSidebarSearchColumnCombo->count() > 0) {
            m_rightSidebarSearchColumnCombo->setCurrentIndex(0);
        }
    }
}

void MainWindow::setRightSidebarExtraActionsVisible(bool visible) {
    m_rightSidebarExtraActionsContainer->setVisible(visible);
}

void MainWindow::updateRightSidebarSelectAllButtonText() {
    LocalizationManager& loc = LocalizationManager::instance();
    if (!m_activeRightSidebarListWidget) {
        m_rightSidebarSelectAllBtn->setText(loc.getString("MainWindow", "RightSidebarSelectAll"));
        return;
    }

    const bool hasItems = m_activeRightSidebarListWidget->topLevelItemCount() > 0;
    bool allSelected = hasItems;
    for (int i = 0; i < m_activeRightSidebarListWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_activeRightSidebarListWidget->topLevelItem(i);
        if (!item || item->isHidden()) {
            continue;
        }
        if (!item->isSelected()) {
            allSelected = false;
            break;
        }
    }

    m_rightSidebarSelectAllBtn->setText(
        allSelected ? loc.getString("MainWindow", "RightSidebarDeselectAll")
                    : loc.getString("MainWindow", "RightSidebarSelectAll")
    );
}

void MainWindow::updateRightSidebarState(const ToolRightSidebarState& state) {
    LocalizationManager& loc = LocalizationManager::instance();
    m_rightSidebarTitleLabel->setText(state.title.isEmpty() ? loc.getString("MainWindow", "RightSidebarDefaultTitle") : state.title);

    const bool showSearch = state.searchModeAvailable && state.searchModeActive;
    setRightSidebarSearchVisible(showSearch);
    setRightSidebarExtraActionsVisible(showSearch && state.showSelectAllButton);
    setRightSidebarListVisible(state.listVisible);

    {
        const QSignalBlocker blocker(m_rightSidebarSearchColumnCombo);
        m_rightSidebarSearchComboToColumn.clear();
        m_rightSidebarSearchColumnCombo->clear();

        if (state.searchableColumns.size() > 1) {
            m_rightSidebarSearchColumnCombo->addItem(loc.getString("MainWindow", "RightSidebarSearchAllColumns"));
            m_rightSidebarSearchComboToColumn.insert(0, -1);
            for (int i = 0; i < state.searchableColumns.size(); ++i) {
                const QString label = i < state.searchableColumnLabels.size()
                    ? state.searchableColumnLabels[i]
                    : QString::number(state.searchableColumns[i]);
                const int comboIndex = m_rightSidebarSearchColumnCombo->count();
                m_rightSidebarSearchColumnCombo->addItem(label);
                m_rightSidebarSearchComboToColumn.insert(comboIndex, state.searchableColumns[i]);
            }
            m_rightSidebarSearchColumnCombo->show();
        } else {
            m_rightSidebarSearchColumnCombo->hide();
        }
    }

    for (auto it = m_rightSidebarRailButtons.begin(); it != m_rightSidebarRailButtons.end(); ++it) {
        QToolButton* button = it.value();
        if (!button) {
            continue;
        }

        const bool checked = (it.key() == state.activeButtonKey)
            || (state.activeButtonKey.isEmpty() && it.key() == QString::fromUtf8(kRightSidebarDefaultButtonKey) && !state.searchModeActive)
            || (state.searchModeActive && it.key() == QString::fromUtf8(kRightSidebarSearchButtonKey));
        button->setChecked(checked);
    }

    applyRightSidebarSearchFilter();
    updateRightSidebarSelectAllButtonText();
}

QTreeWidget* MainWindow::currentRightSidebarListWidget() const {
    return m_activeRightSidebarListWidget;
}

void MainWindow::onRightSidebarSearchTextChanged(const QString& text) {
    m_rightSidebarSearchText = text;
    applyRightSidebarSearchFilter();
}

void MainWindow::onRightSidebarSearchColumnChanged(int) {
    applyRightSidebarSearchFilter();
}

void MainWindow::applyRightSidebarSearchFilter() {
    if (!m_activeRightSidebarListWidget) {
        return;
    }

    const QString needle = m_rightSidebarSearchText.trimmed();
    const int selectedColumn = m_rightSidebarSearchComboToColumn.value(m_rightSidebarSearchColumnCombo->currentIndex(), -1);

    for (int i = 0; i < m_activeRightSidebarListWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_activeRightSidebarListWidget->topLevelItem(i);
        if (!item) {
            continue;
        }

        bool matches = needle.isEmpty();
        if (!matches) {
            const int columnCount = qMax(1, m_activeRightSidebarListWidget->columnCount());
            if (selectedColumn >= 0 && selectedColumn < columnCount) {
                matches = item->text(selectedColumn).contains(needle, Qt::CaseInsensitive);
            } else {
                for (int column = 0; column < columnCount; ++column) {
                    if (item->text(column).contains(needle, Qt::CaseInsensitive)) {
                        matches = true;
                        break;
                    }
                }
            }
        }

        item->setHidden(!matches);
    }

    updateRightSidebarSelectAllButtonText();
}

void MainWindow::onRightSidebarSelectAllClicked() {
    if (!m_activeRightSidebarListWidget) {
        return;
    }

    bool shouldSelectAll = false;
    for (int i = 0; i < m_activeRightSidebarListWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_activeRightSidebarListWidget->topLevelItem(i);
        if (item && !item->isHidden() && !item->isSelected()) {
            shouldSelectAll = true;
            break;
        }
    }

    for (int i = 0; i < m_activeRightSidebarListWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_activeRightSidebarListWidget->topLevelItem(i);
        if (item && !item->isHidden()) {
            item->setSelected(shouldSelectAll);
        }
    }

    updateRightSidebarSelectAllButtonText();
}

void MainWindow::refreshActiveToolRightSidebarUi() {
    if (!m_activeTool) {
        ToolRightSidebarState emptyState;
        emptyState.listVisible = false;
        emptyState.searchModeAvailable = false;
        emptyState.searchModeActive = false;
        emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
        rebuildRightSidebarRail({}, emptyState);
        m_rightSidebarSearchText.clear();
        m_rightSidebarRestoreRequested = false;
        m_rightSidebarPanel->hide();
        m_rightSidebarResizeHandle->hide();
        m_rightSidebarRail->show();
        updateRightSidebarGeometries();
        return;
    }

    const ToolRightSidebarState state = m_activeTool->rightSidebarState();
    rebuildRightSidebarRail(m_activeTool->rightSidebarButtons(), state);
    if (state.listVisible && m_rightSidebarRestoreRequested && m_rightSidebarListWidth <= 0) {
        m_rightSidebarListWidth = normalizedRightSidebarListWidth(
            m_rightSidebarLastExpandedWidth > 0 ? m_rightSidebarLastExpandedWidth : m_rightSidebarDefaultListWidth
        );
    }
    if (m_rightSidebarListWidth > 0) {
        m_rightSidebarLastExpandedWidth = m_rightSidebarListWidth;
    }
    m_rightSidebarRestoreRequested = false;
    updateRightSidebarGeometries();
}

void MainWindow::updateRightSidebarRailGeometry() {
    if (!m_dashboard || !m_rightSidebarRail) {
        return;
    }

    const QRect dashboardRect = m_dashboard->rect();
    const int railX = qMax(0, dashboardRect.width() - m_rightSidebarRailWidth);
    m_rightSidebarRail->setGeometry(railX, 0, m_rightSidebarRailWidth, dashboardRect.height());
    m_rightSidebarRail->raise();
}

void MainWindow::updateRightSidebarListGeometry() {
    if (!m_dashboard || !m_rightSidebarPanel || !m_rightSidebarResizeHandle) {
        return;
    }

    if (!m_rightSidebarListVisible) {
        m_rightSidebarPanel->hide();
        m_rightSidebarResizeHandle->hide();
        return;
    }

    const QRect dashboardRect = m_dashboard->rect();
    const int panelWidth = currentRightSidebarListWidth();
    if (panelWidth <= 0) {
        m_rightSidebarPanel->hide();
        m_rightSidebarResizeHandle->hide();
        return;
    }

    const int panelRight = qMax(0, dashboardRect.width() - m_rightSidebarRailWidth);
    const int panelLeft = qMax(0, panelRight - panelWidth);
    m_rightSidebarPanel->setGeometry(panelLeft, 0, panelWidth, dashboardRect.height());
    m_rightSidebarResizeHandle->setGeometry(
        qMax(0, panelLeft - 1),
        0,
        kRightSidebarResizeHandleWidth,
        dashboardRect.height()
    );
    m_rightSidebarPanel->show();
    m_rightSidebarResizeHandle->show();
    m_rightSidebarResizeHandle->raise();
    m_rightSidebarPanel->raise();
}

void MainWindow::updateRightSidebarGeometries() {
    updateRightSidebarRailGeometry();
    updateRightSidebarListGeometry();

    if (m_dashboardContent && m_dashboardContent->layout()) {
        const int reservedRightWidth = m_rightSidebarRailWidth + (m_rightSidebarListVisible ? currentRightSidebarListWidth() : 0);
        m_dashboardContent->layout()->setContentsMargins(0, 0, reservedRightWidth, 0);
    }
}

int MainWindow::currentRightSidebarListWidth() const {
    return m_rightSidebarListVisible ? normalizedRightSidebarListWidth(m_rightSidebarListWidth) : 0;
}

int MainWindow::normalizedRightSidebarListWidth(int width) const {
    if (width <= m_rightSidebarMinimumListWidth) {
        return 0;
    }
    return qBound(m_rightSidebarMinimumListWidth + 1, width, m_rightSidebarMaximumListWidth);
}

void MainWindow::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    // Sidebar
    if (m_sidebarExpanded) {
        m_toolsBtn->setText(loc.getString("MainWindow", "Tools"));
        m_accountBtn->setText(loc.getString("MainWindow", "Account"));
        m_settingsBtn->setText(loc.getString("MainWindow", "Settings"));
        m_configBtn->setText(loc.getString("MainWindow", "Config"));
    } else {
        m_toolsBtn->setText("");
        m_accountBtn->setText("");
        m_settingsBtn->setText("");
        m_configBtn->setText("");
    }
    
    m_appTitle->setText(loc.getString("MainWindow", "Title"));

    m_settingsPage->updateTexts();
    m_configPage->updateTexts();
    m_toolsPage->updateTexts();
    m_accountPage->updateTexts();

    if (m_dashboardTitleLabel) {
        m_dashboardTitleLabel->setText(loc.getString("MainWindow", "DashboardArea"));
    }
    if (m_rightSidebarSearchEdit) {
        m_rightSidebarSearchEdit->setPlaceholderText(loc.getString("MainWindow", "RightSidebarSearchPlaceholder"));
    }
    updateRightSidebarSelectAllButtonText();
    refreshActiveToolRightSidebarUi();
}

void MainWindow::onSettingsClicked() {
    if (m_mainStack->currentIndex() == 1) {
        closeOverlay();
    } else {
        m_mainStack->setCurrentIndex(1);
    }
}

void MainWindow::onConfigClicked() {
    if (m_mainStack->currentIndex() == 2) {
        closeOverlay();
    } else {
        Logger::instance().logInfo("MainWindow", "Opening config page, refreshing plugin metadata");
        PluginManager::instance().loadPlugins();
        m_configPage->updateTexts();
        m_mainStack->setCurrentIndex(2);
    }
}

void MainWindow::onToolsClicked() {
    if (m_mainStack->currentIndex() == 3) {
        closeOverlay();
    } else {
        m_mainStack->setCurrentIndex(3);
    }
}

void MainWindow::onAccountClicked() {
    if (m_mainStack->currentIndex() == 4) {
        closeOverlay();
    } else {
        m_mainStack->setCurrentIndex(4);
        m_accountPage->updateAccountInfo();
    }
}

void MainWindow::onToolSelected(const QString &toolId) {
    // Check if another tool is active
    if (ToolManager::instance().isToolActive()) {
        LocalizationManager& loc = LocalizationManager::instance();
        auto reply = CustomMessageBox::question(this, 
            loc.getString("MainWindow", "SwitchToolTitle"),
            loc.getString("MainWindow", "SwitchToolMsg"));
        if (reply != QMessageBox::Yes) return;
        
        // Stop the currently active tool process before switching - use async kill
        Logger::instance().logInfo("MainWindow", "Stopping current tool before switching...");
        // Just kill the process directly without waiting
        QList<ToolInterface*> tools = ToolManager::instance().getTools();
        for (ToolInterface* t : tools) {
            ToolProxyInterface* proxy = dynamic_cast<ToolProxyInterface*>(t);
            if (proxy && proxy->isProcessRunning()) {
                // Force kill without waiting
                proxy->forceKillProcess();
            }
        }
        ToolManager::instance().setToolActive(false);
    }

    ToolInterface* tool = ToolManager::instance().getTool(toolId);
    if (!tool) {
        Logger::instance().logError("MainWindow", "Selected tool not found: " + toolId);
        return;
    }

    const QStringList missingDependencies = PluginManager::instance().getMissingDependencies(tool->dependencies());
    if (!missingDependencies.isEmpty()) {
        LocalizationManager& loc = LocalizationManager::instance();
        const QString dependencyList = missingDependencies.join(", ");
        CustomMessageBox::information(
            this,
            loc.getString("MainWindow", "MissingPluginTitle"),
            loc.getString("MainWindow", "MissingPluginMessage").arg(tool->name(), dependencyList)
        );
        Logger::instance().logWarning(
            "MainWindow",
            QString("Refused to launch tool %1 due to missing plugins: %2").arg(tool->id(), dependencyList)
        );
        return;
    }

    Logger::instance().logInfo("MainWindow", "Launching tool: " + tool->name());

    ToolRuntimeContext::instance().setPluginBinaryPathResolver(
        [tool](const QString& pluginName, QString* outPath, QString* errorMessage) {
            if (!tool) {
                if (errorMessage) {
                    *errorMessage = "Current tool context is not available.";
                }
                return false;
            }

            if (!tool->dependencies().contains(pluginName, Qt::CaseSensitive)) {
                if (errorMessage) {
                    *errorMessage = QString("Plugin %1 is not declared in tool dependencies.").arg(pluginName);
                }
                return false;
            }

            return PluginManager::instance().getPluginBinaryPath(pluginName, outPath, errorMessage);
        }
    );

    // Clear current dashboard content
    QLayoutItem *child;
    while ((child = m_dashboardContent->layout()->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->hide();
            child->widget()->deleteLater();
        }
        delete child;
    }
    
    if (QObject* activeToolObject = dynamic_cast<QObject*>(m_activeTool)) {
        disconnect(activeToolObject, SIGNAL(rightSidebarStateChanged()),
                   this, SLOT(refreshActiveToolRightSidebarUi()));
    }

    clearRightSidebarContent();
    m_rightSidebarPanel->hide();
    m_rightSidebarResizeHandle->hide();
    m_rightSidebarRail->show();
    m_activeTool = nullptr;
    m_rightSidebarSearchText.clear();
    ToolRightSidebarState emptyState;
    emptyState.listVisible = false;
    emptyState.searchModeAvailable = false;
    emptyState.searchModeActive = false;
    emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    rebuildRightSidebarRail({}, emptyState);
    updateRightSidebarGeometries();

    // Create and add tool widget
    QWidget *toolWidget = tool->createWidget(m_dashboardContent);
    if (toolWidget) {
        m_dashboardContent->layout()->addWidget(toolWidget);
        ToolManager::instance().setToolActive(true);
        
        // Handle Sidebar
        QWidget *sidebarWidget = tool->createSidebarWidget(m_rightSidebarContentContainer);
        m_activeTool = tool;
        if (QObject* toolObject = dynamic_cast<QObject*>(tool)) {
            connect(toolObject, SIGNAL(rightSidebarStateChanged()),
                    this, SLOT(refreshActiveToolRightSidebarUi()), Qt::UniqueConnection);
        }
        if (sidebarWidget) {
            m_rightSidebarContentLayout->addWidget(sidebarWidget);
            m_activeRightSidebarListWidget = tool->rightSidebarListWidget();
            if (m_activeRightSidebarListWidget) {
                connect(m_activeRightSidebarListWidget, &QTreeWidget::itemSelectionChanged,
                        this, &MainWindow::updateRightSidebarSelectAllButtonText);
            }
            refreshActiveToolRightSidebarUi();
        } else {
            ToolRightSidebarState emptyState;
            emptyState.listVisible = false;
            emptyState.searchModeAvailable = false;
            emptyState.searchModeActive = false;
            emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
            rebuildRightSidebarRail({}, emptyState);
            m_rightSidebarPanel->hide();
            m_rightSidebarResizeHandle->hide();
            m_rightSidebarRail->show();
            updateRightSidebarGeometries();
        }

        // Load current language for the tool (after widgets are created)
        QString currentLang = ConfigManager::instance().getLanguage();
        tool->loadLanguage(currentLang);
    } else {
        Logger::instance().logError("MainWindow", "Failed to create widget for tool: " + toolId);
        // Restore default dashboard?
        LocalizationManager& loc = LocalizationManager::instance();
        m_dashboardTitleLabel = new QLabel(loc.getString("MainWindow", "DashboardArea"), m_dashboardContent);
        m_dashboardTitleLabel->setAlignment(Qt::AlignCenter);
        m_dashboardContent->layout()->addWidget(m_dashboardTitleLabel);
        ToolManager::instance().setToolActive(false);
        ToolRuntimeContext::instance().setPluginBinaryPathResolver({});
    }

    closeOverlay(); 
}

void MainWindow::closeOverlay() {
    m_mainStack->setCurrentIndex(0);
    updateRightSidebarGeometries();
    QTimer::singleShot(0, this, [this]() {
        updateRightSidebarGeometries();
    });
}

void MainWindow::onLanguageChanged() {
    QString lang = ConfigManager::instance().getLanguage();
    if (lang != m_currentLang) {
        m_currentLang = lang;
        LocalizationManager::instance().loadLanguage(lang);

        if (ToolManager::instance().isToolActive()) {
            QList<ToolInterface*> tools = ToolManager::instance().getTools();
            for (ToolInterface* tool : tools) {
                if (tool) {
                    tool->loadLanguage(lang);
                }
            }
        }

        updateTexts();
        m_userAgreementOverlay->updateTexts();
        m_loginOverlay->updateTexts();
        m_connectionWarningOverlay->updateTexts();
        
        LocalizationManager& loc = LocalizationManager::instance();
        CustomMessageBox::information(this, 
            loc.getString("MainWindow", "RestartTitle"), 
            loc.getString("MainWindow", "RestartMsg"));
    }
}

void MainWindow::onThemeChanged() {
    applyTheme();
    
    m_settingsPage->updateTheme();
    m_configPage->updateTheme();
    m_accountPage->updateTheme();
    m_updateOverlay->updateTheme();
    m_userAgreementOverlay->updateTheme();
    m_loginOverlay->updateTheme();
    m_advertisementOverlay->updateTheme();
    m_connectionWarningOverlay->updateTheme();
    
    // Update ToolsPage theme (must be after applyTheme to override global styles)
    m_toolsPage->updateTheme();
    
    // Notify active tool to update theme
    if (ToolManager::instance().isToolActive()) {
        QList<ToolInterface*> tools = ToolManager::instance().getTools();
        for (ToolInterface* tool : tools) {
            tool->applyTheme();
        }
    }
}

void MainWindow::onDebugModeChanged(bool enabled) {
    ConfigManager::instance().setDebugMode(enabled);
    if (enabled) {
        m_memUsageLabel->show();
        m_memTimer->start(1000);
    } else {
        m_memUsageLabel->hide();
        m_memTimer->stop();
    }
}

void MainWindow::onSidebarCompactChanged(bool enabled) {
    ConfigManager::instance().setSidebarCompactMode(enabled);
    // Only apply the change if not locked
    if (!m_sidebarLocked) {
        if (enabled) collapseSidebar();
        else expandSidebar();
    }
}

void MainWindow::onModClosed() {
    Logger::instance().logInfo("MainWindow", "Mod closed, showing setup overlay");
    
    // Stop any active tools
    if (ToolManager::instance().isToolActive()) {
        QList<ToolInterface*> tools = ToolManager::instance().getTools();
        for (ToolInterface* t : tools) {
            ToolProxyInterface* proxy = dynamic_cast<ToolProxyInterface*>(t);
            if (proxy && proxy->isProcessRunning()) {
                proxy->forceKillProcess();
            }
        }
        ToolManager::instance().setToolActive(false);
    }
    
    // Stop path monitoring
    PathValidator::instance().stopMonitoring();
    
    // Set flag to skip ad after setup
    m_setupSkipped = true;
    
    // Show setup overlay
    m_setupOverlay->showOverlay();
}

void MainWindow::onPathInvalid(const QString& titleKey, const QString& msgKey) {
    LocalizationManager& loc = LocalizationManager::instance();
    CustomMessageBox::information(this, 
        loc.getString("Error", titleKey), 
        loc.getString("Error", msgKey));
    
    // Clear only the invalid path config based on which path is invalid
    ConfigManager& config = ConfigManager::instance();
    if (titleKey == "GamePathInvalid") {
        config.clearGamePath();
        Logger::instance().logInfo("MainWindow", "Game path cleared due to validation failure");
    } else if (titleKey == "ModPathInvalid") {
        config.clearModPath();
        Logger::instance().logInfo("MainWindow", "Mod path cleared due to validation failure");
    }
    
    // Show setup overlay if paths become invalid
    m_setupOverlay->showOverlay();
}

void MainWindow::minimizeWindow() { showMinimized(); }
void MainWindow::maximizeWindow() { 
    if (isMaximized()) showNormal(); 
    else showMaximized(); 
}
void MainWindow::closeWindow() { 
    if (m_shutdownInProgress) {
        return;
    }

    if (m_updateShutdownRequested) {
        close();
        return;
    }

    if (ToolManager::instance().isToolActive()) {
        LocalizationManager& loc = LocalizationManager::instance();
        auto reply = CustomMessageBox::question(this, 
            loc.getString("MainWindow", "CloseConfirmTitle"),
            loc.getString("MainWindow", "CloseConfirmMsg"));
        if (reply != QMessageBox::Yes) return;
    }
    close(); 
}

void MainWindow::expandSidebar() {
    if (m_sidebarExpanded) return;
    QPropertyAnimation *anim = new QPropertyAnimation(m_sidebar, "minimumWidth");
    anim->setDuration(500); // Slower animation
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    anim->setStartValue(60);
    anim->setEndValue(250);
    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
        updateRightSidebarGeometries();
    });
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        updateRightSidebarGeometries();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
    m_sidebar->setMaximumWidth(250);
    m_sidebarLayout->setContentsMargins(20, 20, 20, 20); // Restore margins
    m_appTitle->show();
    m_appIcon->show();
    m_bottomAppIcon->hide();
    m_controlsVertical->hide();
    m_controlsHorizontal->show();
    m_titleLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    
    m_toolsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_accountBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_settingsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_configBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    
    m_toolsBtn->show();
    m_accountBtn->show();
    m_settingsBtn->show();
    m_configBtn->show();
    
    LocalizationManager& loc = LocalizationManager::instance();
    m_toolsBtn->setText(loc.getString("MainWindow", "Tools"));
    m_accountBtn->setText(loc.getString("MainWindow", "Account"));
    m_settingsBtn->setText(loc.getString("MainWindow", "Settings"));
    m_configBtn->setText(loc.getString("MainWindow", "Config"));
    
    m_sidebarExpanded = true;
}

void MainWindow::toggleSidebarLock() {
    m_sidebarLocked = !m_sidebarLocked;
    Logger::instance().logInfo("MainWindow", QString("Sidebar lock toggled: %1").arg(m_sidebarLocked ? "Locked" : "Unlocked"));
    
    if (m_sidebarLocked && m_sidebarExpanded) {
        collapseSidebar();
    } else if (!m_sidebarLocked && !m_sidebarExpanded) {
        expandSidebar();
        if (ConfigManager::instance().getSidebarCompactMode() && !m_sidebar->underMouse()) {
            m_sidebarCollapseTimer->start();
        }
    }
}

void MainWindow::collapseSidebar() {
    if (!m_sidebarExpanded) return;
    QPropertyAnimation *anim = new QPropertyAnimation(m_sidebar, "maximumWidth");
    anim->setDuration(500); // Slower animation
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    anim->setStartValue(250);
    anim->setEndValue(60);
    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
        updateRightSidebarGeometries();
    });
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        updateRightSidebarGeometries();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
    m_sidebar->setMinimumWidth(60);
    m_sidebarLayout->setContentsMargins(0, 20, 0, 20); // Remove side margins for centering
    m_appTitle->hide();
    m_appIcon->hide();
    m_bottomAppIcon->show();
    m_controlsHorizontal->hide();
    m_controlsVertical->show();
    m_titleLayout->setAlignment(Qt::AlignCenter);
    
    m_toolsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_accountBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_settingsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_configBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    
    m_toolsBtn->setText("");
    m_accountBtn->setText("");
    m_settingsBtn->setText("");
    m_configBtn->setText("");
    
    m_toolsBtn->show();
    m_accountBtn->show();
    m_settingsBtn->show();
    m_configBtn->show();
    
    m_sidebarExpanded = false;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if ((obj == m_dashboard || obj == m_mainStack)
        && (event->type() == QEvent::Resize
            || event->type() == QEvent::LayoutRequest
            || event->type() == QEvent::Show)) {
        updateRightSidebarGeometries();
    }

    if (obj == m_sidebar && ConfigManager::instance().getSidebarCompactMode()) {
        if (event->type() == QEvent::Enter) {
            if (m_sidebarLocked) {
                return QMainWindow::eventFilter(obj, event);
            }
            // Stop collapse timer if running, then expand
            m_sidebarCollapseTimer->stop();
            expandSidebar();
        } else if (event->type() == QEvent::Leave) {
            if (m_sidebarLocked) {
                return QMainWindow::eventFilter(obj, event);
            }
            // Start 1.5 second delay timer before collapsing
            m_sidebarCollapseTimer->start();
        }
    }

    if (obj == m_rightSidebarResizeHandle && m_rightSidebarListVisible) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_rightSidebarResizeDragging = true;
                m_rightSidebarResizeStartGlobalX = mouseEvent->globalPosition().toPoint().x();
                m_rightSidebarResizeStartWidth = currentRightSidebarListWidth();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton) && m_rightSidebarResizeDragging) {
                const int delta = m_rightSidebarResizeStartGlobalX - mouseEvent->globalPosition().toPoint().x();
                m_rightSidebarListWidth = normalizedRightSidebarListWidth(m_rightSidebarResizeStartWidth + delta);
                if (m_rightSidebarListWidth > 0) {
                    m_rightSidebarLastExpandedWidth = m_rightSidebarListWidth;
                }
                updateRightSidebarGeometries();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_rightSidebarResizeDragging = false;
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton && m_dragging && !m_rightSidebarResizeDragging) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    m_dragging = false;
    m_rightSidebarResizeDragging = false;
}

void MainWindow::processPendingExternalRequest() {
    if (m_pendingExternalRequestHandled || !m_pendingExternalRequest.isValid()) {
        return;
    }

    m_pendingExternalRequestHandled = true;

    switch (m_pendingExternalRequest.type) {
    case ExternalPackageManager::RequestType::ToolDescriptor:
        processPendingToolRequest();
        break;
    case ExternalPackageManager::RequestType::PluginDescriptor:
        processPendingPluginRequest();
        break;
    case ExternalPackageManager::RequestType::None:
    default:
        finishPendingRequest();
        break;
    }
}

void MainWindow::processPendingToolRequest() {
    LocalizationManager& loc = LocalizationManager::instance();
    ExternalPackageManager::ImportResult result =
        ExternalPackageManager::importToolPackage(
            m_pendingExternalRequest.descriptorPath,
            this,
            m_pendingExternalRequest.overwriteApproved
        );

    if (result.requiresRestart) {
        if (!restartApplicationForPendingRequest(
                loc.getString("ExternalPackage", "RestartRequiredForToolMessage").arg(result.importedName))) {
            finishPendingRequest();
        }
        return;
    }

    if (result.cancelled && !result.useInstalledCopy) {
        finishPendingRequest();
        return;
    }

    if (!result.success) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "ToolOpenFailedTitle"),
            loc.getString("ExternalPackage", "ToolOpenFailedMessage").arg(result.errorMessage)
        );
        finishPendingRequest();
        return;
    }

    ToolManager::instance().loadTools();
    m_toolsPage->refreshTools();

    ToolInterface* tool = ToolManager::instance().getTool(result.importedId);
    if (!tool) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "ToolOpenFailedTitle"),
            loc.getString("ExternalPackage", "ImportedToolNotFound").arg(result.importedName)
        );
        finishPendingRequest();
        return;
    }

    if (result.alreadyInstalled) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "InstalledToolOpenTitle"),
            loc.getString("ExternalPackage", "InstalledToolOpenMessage").arg(result.importedName)
        );
    } else if (result.useInstalledCopy) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "UseInstalledToolTitle"),
            loc.getString("ExternalPackage", "UseInstalledToolMessage").arg(result.importedName)
        );
    }

    onToolSelected(result.importedId);
    finishPendingRequest();
}

void MainWindow::processPendingPluginRequest() {
    LocalizationManager& loc = LocalizationManager::instance();
    ExternalPackageManager::ImportResult result =
        ExternalPackageManager::importPluginPackage(
            m_pendingExternalRequest.descriptorPath,
            this,
            m_pendingExternalRequest.overwriteApproved
        );

    if (result.requiresRestart) {
        if (!restartApplicationForPendingRequest(
                loc.getString("ExternalPackage", "RestartRequiredForPluginMessage").arg(result.importedName))) {
            finishPendingRequest();
        }
        return;
    }

    if (result.cancelled) {
        finishPendingRequest();
        return;
    }

    if (!result.success) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "PluginInstallFailedTitle"),
            loc.getString("ExternalPackage", "PluginInstallFailedMessage").arg(result.errorMessage)
        );
        finishPendingRequest();
        return;
    }

    PluginManager::instance().loadPlugins();

    if (result.alreadyInstalled) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "PluginAlreadyInstalledTitle"),
            loc.getString("ExternalPackage", "PluginAlreadyInstalledMessage").arg(result.importedName)
        );
    } else {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "PluginInstallSuccessTitle"),
            loc.getString("ExternalPackage", "PluginInstallSuccessMessage").arg(result.importedName)
        );
    }
    finishPendingRequest();
}

void MainWindow::finishPendingRequest() {
    m_pendingExternalRequest = ExternalPackageManager::PendingRequest();
}

bool MainWindow::restartApplicationForPendingRequest(const QString& restartMessage) {
    LocalizationManager& loc = LocalizationManager::instance();

    ExternalPackageManager::PendingRequest restartRequest = m_pendingExternalRequest;
    restartRequest.overwriteApproved = true;

    QString saveError;
    if (!ExternalPackageManager::savePendingRestartRequest(restartRequest, &saveError)) {
        const QString failureMessage = saveError.trimmed().isEmpty()
                                           ? loc.getString("ExternalPackage", "SavePendingRestartRequestFailed")
                                           : saveError;
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "RestartRequiredTitle"),
            failureMessage
        );
        return false;
    }

    QString applicationPath = QCoreApplication::applicationFilePath();
    applicationPath.replace('/', '\\');

    const QString currentPid = QString::number(QCoreApplication::applicationPid());
    const QString delayedLaunchCommand = QString(
        "set \"APEHTS_PID=%1\" && "
        ":waitExit && "
        "tasklist /FI \"PID eq %1\" | find \"%1\" >nul && "
        "(ping 127.0.0.1 -n 2 >nul && goto waitExit) & "
        "start \"\" \"%2\" --recover-pending-request"
    ).arg(currentPid, applicationPath);

    if (!QProcess::startDetached("cmd.exe", QStringList() << "/c" << delayedLaunchCommand)) {
        ExternalPackageManager::clearPendingRestartRequest();
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "RestartRequiredTitle"),
            loc.getString("ExternalPackage", "RestartProcessSpawnFailed")
        );
        return false;
    }

    m_restartRequested = true;

    CustomMessageBox::information(
        this,
        loc.getString("ExternalPackage", "RestartRequiredTitle"),
        restartMessage
    );

    close();
    return true;
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updateRightSidebarGeometries();
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    updateRightSidebarGeometries();
    QTimer::singleShot(0, this, [this]() {
        updateRightSidebarGeometries();
    });
}

void MainWindow::bringToFront() {
    if (isMinimized()) {
        showNormal();
    }

    show();
    raise();
    activateWindow();
#ifdef Q_OS_WIN
    SetForegroundWindow(reinterpret_cast<HWND>(winId()));
#endif
}

void MainWindow::resetActiveToolUi() {
    if (QObject* activeToolObject = dynamic_cast<QObject*>(m_activeTool)) {
        disconnect(activeToolObject, SIGNAL(rightSidebarStateChanged()),
                   this, SLOT(refreshActiveToolRightSidebarUi()));
    }

    QLayoutItem *child = nullptr;
    while ((child = m_dashboardContent->layout()->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->hide();
            child->widget()->deleteLater();
        }
        delete child;
    }

    clearRightSidebarContent();

    m_rightSidebarPanel->hide();
    m_rightSidebarResizeHandle->hide();
    m_rightSidebarRail->show();
    m_activeTool = nullptr;
    ToolRightSidebarState emptyState;
    emptyState.listVisible = false;
    emptyState.searchModeAvailable = false;
    emptyState.searchModeActive = false;
    emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    rebuildRightSidebarRail({}, emptyState);
    updateRightSidebarGeometries();

    LocalizationManager& loc = LocalizationManager::instance();
    m_dashboardTitleLabel = new QLabel(loc.getString("MainWindow", "DashboardArea"), m_dashboardContent);
    m_dashboardTitleLabel->setAlignment(Qt::AlignCenter);
    m_dashboardContent->layout()->addWidget(m_dashboardTitleLabel);

    ToolManager::instance().setToolActive(false);
    ToolRuntimeContext::instance().setPluginBinaryPathResolver({});
}

bool MainWindow::ensureReadyForIncomingRequest(ExternalPackageManager::RequestType requestType) {
    bringToFront();

    if (!ToolManager::instance().isToolActive()) {
        return true;
    }

    LocalizationManager& loc = LocalizationManager::instance();
    QString title = loc.getString("ExternalPackage", "CloseRunningToolTitle");
    QString message;
    if (requestType == ExternalPackageManager::RequestType::PluginDescriptor) {
        message = loc.getString("ExternalPackage", "CloseRunningToolForPluginMessage");
    } else {
        message = loc.getString("ExternalPackage", "CloseRunningToolForToolMessage");
    }

    const QMessageBox::StandardButton reply = CustomMessageBox::question(this, title, message);
    if (reply != QMessageBox::Yes) {
        return false;
    }

    resetActiveToolUi();
    return true;
}

void MainWindow::handleExternalRequest(const ExternalPackageManager::PendingRequest& request) {
    if (!request.isValid()) {
        return;
    }

    if (!ensureReadyForIncomingRequest(request.type)) {
        return;
    }

    m_pendingExternalRequest = request;
    m_pendingExternalRequestHandled = false;
    processPendingExternalRequest();
}

void MainWindow::performShutdown() {
    if (m_shutdownInProgress) {
        return;
    }

    m_shutdownInProgress = true;
    Logger::instance().logInfo(
        "MainWindow",
        QString("Starting coordinated shutdown cleanup (update_shutdown=%1)")
            .arg(m_updateShutdownRequested ? "true" : "false")
    );

    if (m_scanCheckTimer) {
        m_scanCheckTimer->stop();
    }

    if (m_sidebarCollapseTimer) {
        m_sidebarCollapseTimer->stop();
    }

    if (m_memTimer) {
        m_memTimer->stop();
    }

    PathValidator::instance().stopMonitoring();
    FileManager::instance().stopScanning();

    AuthManager::instance().shutdown();
    HttpClient::instance().shutdown();

    const bool toolsStopped = ToolManager::instance().unloadToolsAndWait(3000);
    Logger::instance().logInfo(
        "MainWindow",
        QString("Tool shutdown result: %1").arg(toolsStopped ? "all_stopped" : "timeout_or_failed")
    );
}

void MainWindow::requestUpdateShutdown() {
    if (m_shutdownInProgress || m_updateShutdownRequested) {
        return;
    }

    m_updateShutdownRequested = true;
    Logger::instance().logInfo("MainWindow", "Update requested coordinated application shutdown");
    close();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    Logger::instance().logInfo(
        "MainWindow",
        QString("Close event received (update_shutdown=%1)")
            .arg(m_updateShutdownRequested ? "true" : "false")
    );

    if (m_shutdownInProgress) {
        event->accept();
        return;
    }

    hide();
    event->ignore();

    QTimer::singleShot(0, this, [this]() {
        performShutdown();
        QCoreApplication::quit();
    });
}

void MainWindow::onGamePathChanged() {
    Logger::instance().logInfo("MainWindow", "Game path changed, reloading files");
    
    // Show loading overlay and restart file scanning (no ad on subsequent loads)
    m_loadingOverlay->showOverlay();
    m_scanCheckTimer->start();
    FileManager::instance().startScanning();
}

void MainWindow::onModPathChanged() {
    Logger::instance().logInfo("MainWindow", "Mod path changed, reloading files");
    
    // Show loading overlay and restart file scanning (no ad on subsequent loads)
    m_loadingOverlay->showOverlay();
    m_scanCheckTimer->start();
    FileManager::instance().startScanning();
}

void MainWindow::checkAndShowAdvertisement(bool loadFilesAfterAd) {
    const QString acceptedVersion = AgreementEvidenceManager::instance().acceptedAgreementVersion();
    const bool shouldShowAd = !acceptedVersion.trimmed().isEmpty() && acceptedVersion != "0.0.0.0";

    Logger::instance().logInfo(
        "MainWindow",
        QString("Checking advertisement condition from AgreementEvidence. accepted_version=%1 should_show=%2")
            .arg(acceptedVersion, shouldShowAd ? "true" : "false")
    );

    if (shouldShowAd) {
        if (loadFilesAfterAd) {
            disconnect(m_advertisementOverlay, &Advertisement::adClosed, this, nullptr);
            connect(m_advertisementOverlay, &Advertisement::adClosed, this, [this]() {
                Logger::instance().logInfo("MainWindow", "Ad closed, loading files");
                m_loadingOverlay->showOverlay();
                m_scanCheckTimer->start();
                FileManager::instance().startScanning();
                show();
            }, Qt::SingleShotConnection);
        }
        m_advertisementOverlay->showAd();
    } else {
        if (loadFilesAfterAd) {
            Logger::instance().logInfo("MainWindow", "No ad needed, loading files directly");
            m_loadingOverlay->showOverlay();
            m_scanCheckTimer->start();
            FileManager::instance().startScanning();
            show();
        }
    }
}

void MainWindow::onLoginSuccessful() {
    m_loginCompleted = true;
    Logger::instance().logInfo(
        "MainWindow",
        QString("Login successful. pendingWarning=%1 thread=%2")
            .arg(m_pendingConnectionWarning)
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
    );
    AgreementEvidenceManager::instance().flushPendingEvents(this);

    const QUrl apiWarmupUrl(AuthManager::getApiBaseUrl());
    if (apiWarmupUrl.isValid()) {
        Logger::instance().logInfo("MainWindow", "Login successful, warming up libcurl connection pool");
        HttpClient::instance().warmUpConnection(apiWarmupUrl, this, [](const HttpResponse&) {
        });
    }

    if (m_advertisementOverlay) {
        Logger::instance().logInfo("MainWindow", "Login successful, preloading advertisement resources");
        m_advertisementOverlay->preloadAd();
    }

    // After login, check for updates FIRST before loading files
    Logger::instance().logInfo("MainWindow", "Login successful, checking for updates before loading files");

    if (AuthManager::instance().hasBlockingAccountAction()) {
        QTimer::singleShot(0, this, [this]() {
            Logger::instance().logInfo("MainWindow", "login completed while account action is active, showing account warning overlay");
            m_connectionWarningOverlay->raise();
            m_connectionWarningOverlay->showAccountActionWarning(AuthManager::instance().getAccountActionInfo());
        });
    } else if (m_pendingConnectionWarning) {
        m_pendingConnectionWarning = false;
        QTimer::singleShot(0, this, [this]() {
            Logger::instance().logInfo("MainWindow", "pending warning resolved on login, calling showWarning()");
            m_connectionWarningOverlay->raise();
            m_connectionWarningOverlay->showWarning();
        });
    }

    if (kDebugForceConnectionWarning) {
        QTimer::singleShot(2000, this, [this]() {
            Logger::instance().logInfo("MainWindow", "Debug force warning after 2s.");
            m_connectionWarningOverlay->raise();
            m_connectionWarningOverlay->showWarning();
        });
    }
    
    // Show the update checking overlay immediately (blocks user interaction)
    m_updateOverlay->showCheckingOverlay();
    m_updateOverlay->checkForUpdates();
    
    // Connect to update check result
    connect(m_updateOverlay, &Update::updateCheckCompleted, this, [this](bool hasUpdate) {
        if (hasUpdate) {
            // Update dialog is shown, user must update. App will quit after update.
            Logger::instance().logInfo("MainWindow", "Update available, waiting for user to update");
            // Do nothing - the update overlay stays visible with update button
        } else {
            // No update needed, continue with normal startup flow
            Logger::instance().logInfo("MainWindow", "No update needed, continuing startup");
            
            if (SetupDialog::isConfigValid()) {
                // Config is valid, start path monitoring and load files directly
                Logger::instance().logInfo("MainWindow", "Config valid, starting path monitoring and loading files");
                PathValidator::instance().startMonitoring();
                m_loadingOverlay->showOverlay();
                m_scanCheckTimer->start();
                FileManager::instance().startScanning();
                show();
            } else {
                // Config invalid or missing, show setup overlay
                Logger::instance().logInfo("MainWindow", "Config invalid, showing setup");
                m_setupOverlay->showOverlay();
            }
        }
    }, Qt::SingleShotConnection);
}

void MainWindow::onLogoutRequested() {
    m_loginCompleted = false;
    m_pendingConnectionWarning = false;
    m_connectionWarningOverlay->hideWarning();
    AuthManager::instance().logout();
    m_loginOverlay->showLogin();
    closeOverlay();
}

void MainWindow::onSetupCompleted() {
    Logger::instance().logInfo("MainWindow", "Setup completed");
    
    // Check if this is from mod close (setupSkipped flag indicates flow type)
    if (m_setupSkipped) {
        // From mod close - go directly to loading files
        m_setupSkipped = false; // Reset flag
        Logger::instance().logInfo("MainWindow", "Setup from mod close - loading files");
        
        // Restart path monitoring
        PathValidator::instance().startMonitoring();
        
        // Restart file scanning for new mod
        m_loadingOverlay->showOverlay();
        m_scanCheckTimer->start();
        FileManager::instance().startScanning();
        
        // Show main window
        show();
    } else {
        // From startup sequence - load files directly (ad will be shown after files are loaded)
        Logger::instance().logInfo("MainWindow", "Setup from startup - loading files");
        
        // Restart path monitoring
        PathValidator::instance().startMonitoring();
        
        // Start file scanning
        m_loadingOverlay->showOverlay();
        m_scanCheckTimer->start();
        FileManager::instance().startScanning();
        
        // Show main window
        show();
    }
}

void MainWindow::onConnectionLost() {
    Logger::instance().logInfo(
        "MainWindow",
        QString("connectionLost received. loginCompleted=%1 pending=%2 thread=%3")
            .arg(m_loginCompleted)
            .arg(m_pendingConnectionWarning)
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
    );
    if (!m_loginCompleted) {
        m_pendingConnectionWarning = true;
        Logger::instance().logInfo("MainWindow", "connectionLost deferred: login not completed.");
        return;
    }
    QTimer::singleShot(0, this, [this]() {
        Logger::instance().logInfo("MainWindow", "connectionLost handler calling showWarning()");
        m_connectionWarningOverlay->raise();
        m_connectionWarningOverlay->showWarning();
    });
}

void MainWindow::onConnectionRestored() {
    m_pendingConnectionWarning = false;
    if (!AuthManager::instance().hasBlockingAccountAction()) {
        m_connectionWarningOverlay->hideWarning();
    }
}

void MainWindow::onAccountActionBlocked() {
    Logger::instance().logInfo(
        "MainWindow",
        QString("accountActionBlocked received. loginCompleted=%1 type=%2")
            .arg(m_loginCompleted)
            .arg(AuthManager::instance().getAccountActionInfo().type)
    );

    if (!m_loginCompleted) {
        return;
    }

    QTimer::singleShot(0, this, [this]() {
        m_connectionWarningOverlay->raise();
        m_connectionWarningOverlay->showAccountActionWarning(AuthManager::instance().getAccountActionInfo());
    });
}

void MainWindow::onAccountActionCleared() {
    Logger::instance().logInfo("MainWindow", "accountActionCleared received.");
    if (AuthManager::instance().isConnected()) {
        m_connectionWarningOverlay->hideWarning();
    }
}

void MainWindow::onToolProcessCrashed(const QString& toolId, const QString& error) {
    Logger::instance().logError("MainWindow", QString("Tool %1 crashed: %2").arg(toolId, error));
    
    // Clear dashboard content
    QLayoutItem *child;
    while ((child = m_dashboardContent->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    
    if (QObject* activeToolObject = dynamic_cast<QObject*>(m_activeTool)) {
        disconnect(activeToolObject, SIGNAL(rightSidebarStateChanged()),
                   this, SLOT(refreshActiveToolRightSidebarUi()));
    }

    clearRightSidebarContent();
    m_rightSidebarPanel->hide();
    m_rightSidebarResizeHandle->hide();
    m_rightSidebarRail->show();
    m_activeTool = nullptr;
    ToolRightSidebarState emptyState;
    emptyState.listVisible = false;
    emptyState.searchModeAvailable = false;
    emptyState.searchModeActive = false;
    emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    rebuildRightSidebarRail({}, emptyState);
    updateRightSidebarGeometries();
    
    // Restore default dashboard
    LocalizationManager& loc = LocalizationManager::instance();
    m_dashboardTitleLabel = new QLabel(loc.getString("MainWindow", "DashboardArea"), m_dashboardContent);
    m_dashboardTitleLabel->setAlignment(Qt::AlignCenter);
    m_dashboardContent->layout()->addWidget(m_dashboardTitleLabel);
    
    ToolManager::instance().setToolActive(false);
    ToolRuntimeContext::instance().setPluginBinaryPathResolver({});
    
    // Show error message to user
    CustomMessageBox::information(this, 
        loc.getString("MainWindow", "ToolCrashedTitle"),
        loc.getString("MainWindow", "ToolCrashedMsg").arg(toolId).arg(error));
}