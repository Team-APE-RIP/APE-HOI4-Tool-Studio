#include "MainWindow.h"
#include "ConfigManager.h"
#include "CustomMessageBox.h"
#include "LocalizationManager.h"
#include "PathValidator.h"
#include "SetupDialog.h"
#include "FileManager.h"
#include "TagManager.h"
#include "ToolManager.h"
#include "ToolProxyInterface.h"
#include "Logger.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFrame>
#include <QDebug>
#include <QApplication>
#include <QStyle>
#include <QPropertyAnimation>
#include <QCloseEvent>
#include <windows.h>
#include <psapi.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_currentLang = ConfigManager::instance().getLanguage();
    LocalizationManager::instance().loadLanguage(m_currentLang);
    
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setWindowIcon(QIcon(":/app.ico"));
    
    setupUi();
    setupDebugOverlay();
    applyTheme();
    resize(1280, 720);
    setMinimumSize(1280, 720);

    m_sidebar->installEventFilter(this);

    // Setup sidebar collapse delay timer (1.5 seconds)
    m_sidebarCollapseTimer = new QTimer(this);
    m_sidebarCollapseTimer->setSingleShot(true);
    m_sidebarCollapseTimer->setInterval(500); // Change from 1500 to 500
    connect(m_sidebarCollapseTimer, &QTimer::timeout, this, &MainWindow::collapseSidebar);

    // Start Path Monitoring
    PathValidator::instance().startMonitoring();
    connect(&PathValidator::instance(), &PathValidator::pathInvalid, this, &MainWindow::onPathInvalid);

    // Setup Loading Overlay
    m_loadingOverlay = new LoadingOverlay(m_centralWidget);
    LocalizationManager& loc = LocalizationManager::instance();
    m_loadingOverlay->setMessage(loc.getString("MainWindow", "LoadingFiles"));
    
    // Setup scan check timer - poll every 500ms to check if scanning is complete
    m_scanCheckTimer = new QTimer(this);
    m_scanCheckTimer->setInterval(500);
    connect(m_scanCheckTimer, &QTimer::timeout, this, [this]() {
        if (!FileManager::instance().isScanning()) {
            Logger::instance().logInfo("MainWindow", "Scan complete detected via polling - hiding overlay");
            m_scanCheckTimer->stop();
            m_loadingOverlay->hideOverlay();
        }
    });
    
    // Show overlay and start scanning (delayed to ensure UI is ready)
    QTimer::singleShot(100, this, [this]() {
        m_loadingOverlay->showOverlay();
        m_scanCheckTimer->start();
        FileManager::instance().startScanning();
    });

    // Initialize TagManager (it will listen to FileManager scan events)
    TagManager::instance();

    // Load Tools
    ToolManager::instance().loadTools();
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
    // Force refresh tools page to ensure UI is updated after loading
    m_toolsPage->refreshTools();
    
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
        m_settingsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_configBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
        m_titleLayout->setAlignment(Qt::AlignCenter);
        m_toolsBtn->setText("");
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
    
    // Dashboard Splitter (Content | Right Sidebar)
    QHBoxLayout *dashLayout = new QHBoxLayout(m_dashboard);
    dashLayout->setContentsMargins(0, 0, 0, 0);
    dashLayout->setSpacing(0);

    m_dashboardSplitter = new QSplitter(Qt::Horizontal, m_dashboard);
    m_dashboardSplitter->setHandleWidth(1);
    
    m_dashboardContent = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(m_dashboardContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *dashLabel = new QLabel("Dashboard Area", m_dashboardContent);
    dashLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(dashLabel);

    m_rightSidebar = new QWidget();
    m_rightSidebar->setFixedWidth(250); // Default width
    m_rightSidebar->hide(); // Hidden by default
    QVBoxLayout *rightLayout = new QVBoxLayout(m_rightSidebar);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_dashboardSplitter->addWidget(m_dashboardContent);
    m_dashboardSplitter->addWidget(m_rightSidebar);
    m_dashboardSplitter->setStretchFactor(0, 1);
    m_dashboardSplitter->setStretchFactor(1, 0);

    dashLayout->addWidget(m_dashboardSplitter);
    m_mainStack->addWidget(m_dashboard);

    m_settingsPage = new SettingsPage();
    connect(m_settingsPage, &SettingsPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_settingsPage, &SettingsPage::languageChanged, this, &MainWindow::onLanguageChanged);
    connect(m_settingsPage, &SettingsPage::themeChanged, this, &MainWindow::onThemeChanged);
    connect(m_settingsPage, &SettingsPage::debugModeChanged, this, &MainWindow::onDebugModeChanged);
    connect(m_settingsPage, &SettingsPage::sidebarCompactChanged, this, &MainWindow::onSidebarCompactChanged);
    m_mainStack->addWidget(m_settingsPage);

    m_configPage = new ConfigPage();
    connect(m_configPage, &ConfigPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_configPage, &ConfigPage::modClosed, this, &MainWindow::onModClosed);
    connect(m_configPage, &ConfigPage::gamePathChanged, this, &MainWindow::onGamePathChanged);
    m_mainStack->addWidget(m_configPage);

    m_toolsPage = new ToolsPage();
    connect(m_toolsPage, &ToolsPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_toolsPage, &ToolsPage::toolSelected, this, &MainWindow::onToolSelected);
    m_mainStack->addWidget(m_toolsPage);

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
    m_toolsBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_toolsBtn, &QPushButton::clicked, this, &MainWindow::onToolsClicked);
    m_sidebarLayout->addWidget(m_toolsBtn);

    m_settingsBtn = new QToolButton(this);
    m_settingsBtn->setObjectName("SidebarButton");
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    // No icon set here, handled by text/style
    m_settingsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly); 
    m_settingsBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    m_sidebarLayout->addWidget(m_settingsBtn);

    m_configBtn = new QToolButton(this);
    m_configBtn->setObjectName("SidebarButton");
    m_configBtn->setCursor(Qt::PointingHandCursor);
    // No icon set here
    m_configBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
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
    ConfigManager::Theme theme = ConfigManager::instance().getTheme();
    bool isDark = (theme == ConfigManager::Theme::Dark);
    
    QString bg = isDark ? "#2C2C2E" : "#F5F5F7";
    QString sidebarBg = isDark ? "#2C2C2E" : "#F5F5F7";
    QString text = isDark ? "#FFFFFF" : "#1D1D1F";
    QString border = isDark ? "#3A3A3C" : "#D2D2D7";
    QString rowBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString rowHover = isDark ? "#3A3A3C" : "#F5F5F7";
    QString iconBg = isDark ? "#3A3A3C" : "#EEEEEE";
    QString treeItemHover = isDark ? "#3A3A3C" : "#E8E8E8";
    QString treeItemSelected = isDark ? "#0A84FF" : "#007AFF";
    
    QString style = QString(R"(
        QWidget#CentralWidget { background-color: %1; border: 1px solid %4; border-radius: 10px; }
        QWidget#Sidebar { background-color: %2; border-right: 1px solid %4; border-top-left-radius: 10px; border-bottom-left-radius: 10px; }
        QWidget#OverlayContainer { background-color: %2; border-top-right-radius: 10px; border-bottom-right-radius: 10px; }
        QWidget#SettingsContent, QWidget#ToolsContent { background-color: %2; }
        QWidget#Dashboard { background-color: %1; }
        
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
            background-color: %7; border-radius: 8px; color: %3;
        }
        
        QComboBox {
            border: 1px solid %4; border-radius: 6px; padding: 4px; background-color: %5; color: %3;
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
            background-color: %1; border: none;
        }
        
        QToolTip {
            background-color: %2; color: %3; border: 1px solid %4; padding: 5px; border-radius: 4px;
        }
        
        QSplitter::handle {
            background-color: %4;
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
    )").arg(bg, sidebarBg, text, border, rowBg, rowHover, iconBg, treeItemHover, treeItemSelected);
    
    setStyleSheet(style);
    
    // Apply theme to right sidebar
    m_rightSidebar->setStyleSheet(QString("background-color: %1; border-left: 1px solid %2;").arg(sidebarBg, border));
    m_dashboardContent->setStyleSheet(QString("background-color: %1;").arg(bg));
    
    // Apply theme to splitter handle
    m_dashboardSplitter->setStyleSheet(QString("QSplitter::handle { background-color: %1; }").arg(border));
}

void MainWindow::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    // Sidebar
    if (m_sidebarExpanded) {
        m_toolsBtn->setText(loc.getString("MainWindow", "Tools"));
        m_settingsBtn->setText(loc.getString("MainWindow", "Settings"));
        m_configBtn->setText(loc.getString("MainWindow", "Config"));
    } else {
        m_toolsBtn->setText("");
        m_settingsBtn->setText("");
        m_configBtn->setText("");
    }
    
    m_appTitle->setText(loc.getString("MainWindow", "Title"));

    m_settingsPage->updateTexts();
    m_configPage->updateTexts();
    m_toolsPage->updateTexts();
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

    Logger::instance().logInfo("MainWindow", "Launching tool: " + tool->name());

    // Clear current dashboard content
    QLayoutItem *child;
    while ((child = m_dashboardContent->layout()->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->hide();
            child->widget()->deleteLater();
        }
        delete child;
    }
    
    // Clear right sidebar
    while ((child = m_rightSidebar->layout()->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->hide();
            child->widget()->deleteLater();
        }
        delete child;
    }
    m_rightSidebar->hide();

    // Create and add tool widget
    QWidget *toolWidget = tool->createWidget(m_dashboardContent);
    if (toolWidget) {
        m_dashboardContent->layout()->addWidget(toolWidget);
        ToolManager::instance().setToolActive(true);
        
        // Handle Sidebar
        QWidget *sidebarWidget = tool->createSidebarWidget(m_rightSidebar);
        if (sidebarWidget) {
            m_rightSidebar->layout()->addWidget(sidebarWidget);
            m_rightSidebar->show();
        }
        
        // Load current language for the tool (after widgets are created)
        QString currentLang = ConfigManager::instance().getLanguage();
        tool->loadLanguage(currentLang);
    } else {
        Logger::instance().logError("MainWindow", "Failed to create widget for tool: " + toolId);
        // Restore default dashboard?
        QLabel *dashLabel = new QLabel("Dashboard Area", m_dashboardContent);
        dashLabel->setAlignment(Qt::AlignCenter);
        m_dashboardContent->layout()->addWidget(dashLabel);
        ToolManager::instance().setToolActive(false);
    }

    closeOverlay(); 
}

void MainWindow::closeOverlay() {
    m_mainStack->setCurrentIndex(0);
}

void MainWindow::onLanguageChanged() {
    QString lang = ConfigManager::instance().getLanguage();
    if (lang != m_currentLang) {
        m_currentLang = lang;
        LocalizationManager::instance().loadLanguage(lang);
        updateTexts();
        
        LocalizationManager& loc = LocalizationManager::instance();
        CustomMessageBox::information(this, 
            loc.getString("MainWindow", "RestartTitle"), 
            loc.getString("MainWindow", "RestartMsg"));
    }
}

void MainWindow::onThemeChanged() {
    applyTheme();
    
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
    if (enabled) collapseSidebar();
    else expandSidebar();
}

void MainWindow::onModClosed() {
    Logger::instance().logInfo("MainWindow", "Mod closed, showing setup dialog");
    
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
    
    // Hide main window and show setup dialog
    hide();
    
    SetupDialog setup;
    if (setup.exec() == QDialog::Accepted) {
        ConfigManager& config = ConfigManager::instance();
        config.setGamePath(setup.getGamePath());
        config.setModPath(setup.getModPath());
        config.setLanguage(setup.getLanguage());
        
        // Update UI
        m_configPage->updateTexts();
        
        // Restart path monitoring
        PathValidator::instance().startMonitoring();
        
        // Restart file scanning for new mod
        m_loadingOverlay->showOverlay();
        m_scanCheckTimer->start();
        FileManager::instance().startScanning();
        
        // Show main window again
        show();
        Logger::instance().logInfo("MainWindow", "Setup completed, showing main window");
    } else {
        // User cancelled setup, close the application
        Logger::instance().logInfo("MainWindow", "Setup cancelled, closing application");
        close();
    }
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
    
    // Show setup dialog if paths become invalid
    SetupDialog setup(this);
    if (setup.exec() == QDialog::Accepted) {
        config.setGamePath(setup.getGamePath());
        config.setModPath(setup.getModPath());
        config.setLanguage(setup.getLanguage());
        m_configPage->updateTexts(); // Refresh config page if open
        
        // Restart monitoring
        PathValidator::instance().startMonitoring();
    }
}

void MainWindow::minimizeWindow() { showMinimized(); }
void MainWindow::maximizeWindow() { 
    if (isMaximized()) showNormal(); 
    else showMaximized(); 
}
void MainWindow::closeWindow() { 
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
    anim->start(QAbstractAnimation::DeleteWhenStopped);
    m_sidebar->setMaximumWidth(250);
    m_sidebarLayout->setContentsMargins(20, 20, 20, 20); // Restore margins
    m_appTitle->show();
    m_appIcon->show();
    m_bottomAppIcon->hide();
    m_controlsVertical->hide();
    m_controlsHorizontal->show();
    m_titleLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_toolsBtn->show();
    m_settingsBtn->show();
    m_configBtn->show();
    
    LocalizationManager& loc = LocalizationManager::instance();
    m_toolsBtn->setText(loc.getString("MainWindow", "Tools"));
    m_settingsBtn->setText(loc.getString("MainWindow", "Settings"));
    m_configBtn->setText(loc.getString("MainWindow", "Config"));
    
    m_sidebarExpanded = true;
}

void MainWindow::collapseSidebar() {
    if (!m_sidebarExpanded) return;
    QPropertyAnimation *anim = new QPropertyAnimation(m_sidebar, "maximumWidth");
    anim->setDuration(500); // Slower animation
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    anim->setStartValue(250);
    anim->setEndValue(60);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
    m_sidebar->setMinimumWidth(60);
    m_sidebarLayout->setContentsMargins(0, 20, 0, 20); // Remove side margins for centering
    m_appTitle->hide();
    m_appIcon->hide();
    m_bottomAppIcon->show();
    m_controlsHorizontal->hide();
    m_controlsVertical->show();
    m_titleLayout->setAlignment(Qt::AlignCenter);
    m_toolsBtn->hide();
    m_settingsBtn->hide();
    m_configBtn->hide();
    m_sidebarExpanded = false;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_sidebar && ConfigManager::instance().getSidebarCompactMode()) {
        if (event->type() == QEvent::Enter) {
            // Stop collapse timer if running, then expand
            m_sidebarCollapseTimer->stop();
            expandSidebar();
        } else if (event->type() == QEvent::Leave) {
            // Start 1.5 second delay timer before collapsing
            m_sidebarCollapseTimer->start();
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
    if (event->buttons() & Qt::LeftButton && m_dragging) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}
void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Stop all tool processes before closing
    ToolManager::instance().unloadTools();
    event->accept();
}

void MainWindow::onGamePathChanged() {
    Logger::instance().logInfo("MainWindow", "Game path changed, reloading files");
    
    // Show loading overlay and restart file scanning
    m_loadingOverlay->showOverlay();
    m_scanCheckTimer->start();
    FileManager::instance().startScanning();
}

void MainWindow::onToolProcessCrashed(const QString& toolId, const QString& error) {
    Logger::instance().logError("MainWindow", QString("Tool %1 crashed: %2").arg(toolId, error));
    
    // Clear dashboard content
    QLayoutItem *child;
    while ((child = m_dashboardContent->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    
    // Clear right sidebar
    while ((child = m_rightSidebar->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    m_rightSidebar->hide();
    
    // Restore default dashboard
    QLabel *dashLabel = new QLabel("Dashboard Area", m_dashboardContent);
    dashLabel->setAlignment(Qt::AlignCenter);
    m_dashboardContent->layout()->addWidget(dashLabel);
    
    ToolManager::instance().setToolActive(false);
    
    // Show error message to user
    LocalizationManager& loc = LocalizationManager::instance();
    CustomMessageBox::information(this, 
        loc.getString("MainWindow", "ToolCrashedTitle"),
        loc.getString("MainWindow", "ToolCrashedMsg").arg(toolId).arg(error));
}
