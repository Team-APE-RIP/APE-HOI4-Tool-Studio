#include "MainWindow.h"
#include "ConfigManager.h"
#include "CustomMessageBox.h"
#include "LocalizationManager.h"
#include "PathValidator.h"
#include "SetupDialog.h"
#include "FileManager.h"
#include "ToolManager.h"
#include "Logger.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFrame>
#include <QDebug>
#include <QApplication>
#include <QStyle>
#include <QPropertyAnimation>
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

    // Start Path Monitoring
    PathValidator::instance().startMonitoring();
    connect(&PathValidator::instance(), &PathValidator::pathInvalid, this, &MainWindow::onPathInvalid);

    // Start File Scanning
    FileManager::instance().startScanning();

    // Load Tools
    ToolManager::instance().loadTools();
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
    QLabel *dashLabel = new QLabel("Dashboard Area", m_dashboard);
    dashLabel->setAlignment(Qt::AlignCenter);
    QVBoxLayout *dashLayout = new QVBoxLayout(m_dashboard);
    dashLayout->addWidget(dashLabel);
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
    connect(maxBtnH, &QPushButton::clicked, this, &MainWindow::maximizeWindow);
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
    connect(maxBtnV, &QPushButton::clicked, this, &MainWindow::maximizeWindow);
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
    m_appIcon->setPixmap(QIcon(":/app.ico").pixmap(32, 32));
    m_appIcon->setFixedSize(32, 32);
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
    m_bottomAppIcon->setPixmap(QIcon(":/app.ico").pixmap(32, 32));
    m_bottomAppIcon->setFixedSize(32, 32);
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
    
    QString bg = isDark ? "#1C1C1E" : "#FFFFFF";
    QString sidebarBg = isDark ? "#2C2C2E" : "#F5F5F7";
    QString text = isDark ? "#FFFFFF" : "#1D1D1F";
    QString border = isDark ? "#3A3A3C" : "#D2D2D7";
    QString rowBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString rowHover = isDark ? "#3A3A3C" : "#F5F5F7";
    QString iconBg = isDark ? "#3A3A3C" : "#EEEEEE";
    
    QString style = QString(R"(
        QWidget#CentralWidget { background-color: %1; border: 1px solid %4; border-radius: 10px; }
        QWidget#Sidebar { background-color: %2; border-right: 1px solid %4; border-top-left-radius: 10px; border-bottom-left-radius: 10px; }
        QWidget#OverlayContainer { background-color: %2; border-top-right-radius: 10px; border-bottom-right-radius: 10px; }
        QWidget#SettingsContent, QWidget#ToolsContent { background-color: %2; }
        
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
    )").arg(bg, sidebarBg, text, border, rowBg, rowHover, iconBg);
    
    setStyleSheet(style);
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
    }

    ToolInterface* tool = ToolManager::instance().getTool(toolId);
    if (!tool) {
        Logger::instance().logError("MainWindow", "Selected tool not found: " + toolId);
        return;
    }

    Logger::instance().logInfo("MainWindow", "Launching tool: " + tool->name());

    // Clear current dashboard
    QLayoutItem *child;
    while ((child = m_dashboard->layout()->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // Create and add tool widget
    QWidget *toolWidget = tool->createWidget(m_dashboard);
    if (toolWidget) {
        m_dashboard->layout()->addWidget(toolWidget);
        ToolManager::instance().setToolActive(true);
    } else {
        Logger::instance().logError("MainWindow", "Failed to create widget for tool: " + toolId);
        // Restore default dashboard?
        QLabel *dashLabel = new QLabel("Dashboard Area", m_dashboard);
        dashLabel->setAlignment(Qt::AlignCenter);
        m_dashboard->layout()->addWidget(dashLabel);
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
    LocalizationManager& loc = LocalizationManager::instance();
    CustomMessageBox::information(this, 
        loc.getString("MainWindow", "ModClosedTitle"), 
        loc.getString("MainWindow", "ModClosedMsg"));
    close();
}

void MainWindow::onPathInvalid(const QString& titleKey, const QString& msgKey) {
    LocalizationManager& loc = LocalizationManager::instance();
    CustomMessageBox::information(this, 
        loc.getString("Error", titleKey), 
        loc.getString("Error", msgKey));
    
    // Show setup dialog if paths become invalid
    SetupDialog setup(this);
    if (setup.exec() == QDialog::Accepted) {
        ConfigManager& config = ConfigManager::instance();
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
            expandSidebar();
        } else if (event->type() == QEvent::Leave) {
            collapseSidebar();
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
