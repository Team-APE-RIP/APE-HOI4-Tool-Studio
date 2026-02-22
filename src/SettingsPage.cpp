#include "SettingsPage.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>
#include <QGroupBox>

SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent) {
    setupUi();
    updateTexts();
    updateTheme();
}

void SettingsPage::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    QWidget *header = new QWidget();
    header->setObjectName("OverlayHeader");
    header->setFixedHeight(60);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 20, 0);

    QLabel *title = new QLabel("Settings");
    title->setObjectName("SettingsTitle");
    title->setStyleSheet("font-size: 18px; font-weight: bold;");
    
    QPushButton *closeBtn = new QPushButton("×");
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("border: none; font-size: 20px; color: #888;");
    connect(closeBtn, &QPushButton::clicked, this, &SettingsPage::closeClicked);

    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(closeBtn);
    layout->addWidget(header);

    // Content
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    
    QWidget *content = new QWidget();
    content->setObjectName("SettingsContent");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(40, 20, 40, 40);
    contentLayout->setSpacing(30);

    // 1. Interface Group
    QVBoxLayout *interfaceLayout = new QVBoxLayout();
    interfaceLayout->setSpacing(0);
    
    m_themeCombo = new QComboBox();
    m_themeCombo->setCurrentIndex(static_cast<int>(ConfigManager::instance().getTheme()));
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index){
        ConfigManager::instance().setTheme(static_cast<ConfigManager::Theme>(index));
        emit themeChanged();
    });
    interfaceLayout->addWidget(createSettingRow("Theme", "🎨", "Theme Mode", "Select application appearance", m_themeCombo));

    m_languageCombo = new QComboBox();
    m_languageCombo->addItems({"English", "简体中文", "繁體中文"});
    m_languageCombo->setCurrentText(ConfigManager::instance().getLanguage());
    connect(m_languageCombo, &QComboBox::currentTextChanged, [this](const QString &lang){
        if (lang != ConfigManager::instance().getLanguage()) {
            ConfigManager::instance().setLanguage(lang);
            emit languageChanged();
        }
    });
    interfaceLayout->addWidget(createSettingRow("Lang", "🌐", "Language", "Restart required to apply changes", m_languageCombo));

    m_sidebarCompactCheck = new QCheckBox();
    m_sidebarCompactCheck->setChecked(ConfigManager::instance().getSidebarCompactMode());
    connect(m_sidebarCompactCheck, &QCheckBox::toggled, [this](bool checked){
        ConfigManager::instance().setSidebarCompactMode(checked);
        emit sidebarCompactChanged(checked);
    });
    interfaceLayout->addWidget(createSettingRow("Sidebar", "◀", "Compact Sidebar", "Auto-collapse sidebar", m_sidebarCompactCheck));

    contentLayout->addWidget(createGroup("Interface", interfaceLayout));

    // 2. Debug Group
    QVBoxLayout *debugLayout = new QVBoxLayout();
    debugLayout->setSpacing(0);

    m_debugCheck = new QCheckBox();
    m_debugCheck->setChecked(ConfigManager::instance().getDebugMode());
    connect(m_debugCheck, &QCheckBox::toggled, [this](bool checked){
        ConfigManager::instance().setDebugMode(checked);
        emit debugModeChanged(checked);
    });
    debugLayout->addWidget(createSettingRow("Debug", "🐞", "Show Usage Overlay", "Show memory usage overlay", m_debugCheck));

    m_maxLogFilesSpin = new QSpinBox();
    m_maxLogFilesSpin->setRange(1, 100);
    m_maxLogFilesSpin->setValue(ConfigManager::instance().getMaxLogFiles());
    connect(m_maxLogFilesSpin, QOverload<int>::of(&QSpinBox::valueChanged), [](int value){
        ConfigManager::instance().setMaxLogFiles(value);
    });
    debugLayout->addWidget(createSettingRow("MaxLogs", "🧹", "Max Log Files", "Number of log files to keep", m_maxLogFilesSpin));

    m_openLogBtn = new QPushButton("Open Logs");
    m_openLogBtn->setObjectName("OpenLogBtn");
    m_openLogBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openLogBtn, &QPushButton::clicked, this, &SettingsPage::openLogDir);
    debugLayout->addWidget(createSettingRow("Log", "📂", "Log Directory", "Open application logs", m_openLogBtn));

    contentLayout->addWidget(createGroup("Debug", debugLayout));

    // 3. About Group
    QVBoxLayout *aboutLayout = new QVBoxLayout();
    aboutLayout->setSpacing(0);

    QWidget *aboutRow = new QWidget();
    aboutRow->setObjectName("SettingRow");
    QVBoxLayout *aboutRowLayout = new QVBoxLayout(aboutRow);
    aboutRowLayout->setContentsMargins(20, 20, 20, 20);
    aboutRowLayout->setSpacing(10);
    
    QHBoxLayout *infoLayout = new QHBoxLayout();
    QLabel *appName = new QLabel("APE HOI4 Tool Studio");
    appName->setStyleSheet("font-weight: bold; font-size: 16px;");
    m_versionLabel = new QLabel("v1.0.0");
    infoLayout->addWidget(appName);
    infoLayout->addStretch();
    infoLayout->addWidget(m_versionLabel);
    
    QLabel *copyright = new QLabel("© 2026 Team APE:RIP. All rights reserved.");
    copyright->setStyleSheet("color: #888; font-size: 12px;");
    
    QPushButton *githubLink = new QPushButton("GitHub Repository");
    githubLink->setObjectName("GithubLink");
    githubLink->setFlat(true);
    githubLink->setCursor(Qt::PointingHandCursor);
    connect(githubLink, &QPushButton::clicked, [this](){ openUrl("https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio"); });

    QPushButton *licenseLink = new QPushButton("LICENSE");
    licenseLink->setObjectName("LicenseLink");
    licenseLink->setFlat(true);
    licenseLink->setCursor(Qt::PointingHandCursor);
    connect(licenseLink, &QPushButton::clicked, [this](){ openUrl("https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/blob/main/LICENSE"); });

    m_openSourceToggleBtn = new QPushButton("Open Source Libraries ▼");
    m_openSourceToggleBtn->setObjectName("OpenSourceBtn");
    m_openSourceToggleBtn->setFlat(true);
    m_openSourceToggleBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openSourceToggleBtn, &QPushButton::clicked, this, &SettingsPage::toggleOpenSource);

    m_openSourceArea = new QWidget();
    m_openSourceArea->setVisible(false);
    QVBoxLayout *osLayout = new QVBoxLayout(m_openSourceArea);
    osLayout->setContentsMargins(10, 0, 0, 0);
    QLabel *osList = new QLabel("• Qt Framework (LGPLv3)\n• ... (Other libs)");
    osList->setStyleSheet("color: #888;");
    osLayout->addWidget(osList);

    aboutRowLayout->addLayout(infoLayout);
    aboutRowLayout->addWidget(copyright);
    aboutRowLayout->addWidget(githubLink);
    aboutRowLayout->addWidget(licenseLink);
    aboutRowLayout->addWidget(m_openSourceToggleBtn);
    aboutRowLayout->addWidget(m_openSourceArea);
    
    aboutLayout->addWidget(aboutRow);
    contentLayout->addWidget(createGroup("About", aboutLayout));

    contentLayout->addStretch();

    scroll->setWidget(content);
    layout->addWidget(scroll);
}

QWidget* SettingsPage::createGroup(const QString &title, QLayout *contentLayout) {
    QGroupBox *group = new QGroupBox();
    group->setObjectName("SettingsGroup");
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(0, 10, 0, 0);
    groupLayout->setSpacing(0);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName(title + "_GroupTitle"); // For translation
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #888; margin-left: 10px; margin-bottom: 5px;");
    
    QWidget *container = new QWidget();
    container->setObjectName("GroupContainer"); // For styling rounded corners
    container->setLayout(contentLayout);

    groupLayout->addWidget(titleLabel);
    groupLayout->addWidget(container);
    
    return group;
}

QWidget* SettingsPage::createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *control) {
    QWidget *row = new QWidget();
    row->setObjectName("SettingRow");
    row->setFixedHeight(60);
    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(20, 10, 20, 10);
    
    QLabel *iconLbl = new QLabel(icon);
    iconLbl->setObjectName("SettingIcon");
    iconLbl->setFixedSize(30, 30);
    iconLbl->setAlignment(Qt::AlignCenter);
    
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    QLabel *titleLbl = new QLabel(title);
    titleLbl->setObjectName(id + "_Title");
    titleLbl->setStyleSheet("font-weight: bold; font-size: 14px;");
    QLabel *descLbl = new QLabel(desc);
    descLbl->setObjectName(id + "_Desc");
    descLbl->setStyleSheet("color: #888; font-size: 12px;");
    textLayout->addWidget(titleLbl);
    textLayout->addWidget(descLbl);
    
    layout->addWidget(iconLbl);
    layout->addLayout(textLayout);
    layout->addStretch();
    if(control) layout->addWidget(control);
    
    return row;
}

void SettingsPage::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();

    // Update Theme Combo Items
    {
        QSignalBlocker blocker(m_themeCombo);
        m_themeCombo->clear();
        m_themeCombo->addItem(loc.getString("SettingsPage", "Theme_System"));
        m_themeCombo->addItem(loc.getString("SettingsPage", "Theme_Light"));
        m_themeCombo->addItem(loc.getString("SettingsPage", "Theme_Dark"));
        m_themeCombo->setCurrentIndex(static_cast<int>(ConfigManager::instance().getTheme()));
    }

    QLabel *settingsTitle = findChild<QLabel*>("SettingsTitle");
    if(settingsTitle) settingsTitle->setText(loc.getString("SettingsPage", "SettingsTitle"));
    
    // Group Titles
    QLabel *interfaceGroup = findChild<QLabel*>("Interface_GroupTitle");
    if(interfaceGroup) interfaceGroup->setText(loc.getString("SettingsPage", "Group_Interface"));
    QLabel *debugGroup = findChild<QLabel*>("Debug_GroupTitle");
    if(debugGroup) debugGroup->setText(loc.getString("SettingsPage", "Group_Debug"));
    QLabel *aboutGroup = findChild<QLabel*>("About_GroupTitle");
    if(aboutGroup) aboutGroup->setText(loc.getString("SettingsPage", "Group_About"));

    // Rows
    QLabel *themeTitle = findChild<QLabel*>("Theme_Title");
    if(themeTitle) themeTitle->setText(loc.getString("SettingsPage", "Theme_Title"));
    QLabel *themeDesc = findChild<QLabel*>("Theme_Desc");
    if(themeDesc) themeDesc->setText(loc.getString("SettingsPage", "Theme_Desc"));
    
    QLabel *langTitle = findChild<QLabel*>("Lang_Title");
    if(langTitle) langTitle->setText(loc.getString("SettingsPage", "Lang_Title"));
    QLabel *langDesc = findChild<QLabel*>("Lang_Desc");
    if(langDesc) langDesc->setText(loc.getString("SettingsPage", "Lang_Desc"));

    QLabel *debugTitle = findChild<QLabel*>("Debug_Title");
    if(debugTitle) debugTitle->setText(loc.getString("SettingsPage", "Debug_Title"));
    QLabel *debugDesc = findChild<QLabel*>("Debug_Desc");
    if(debugDesc) debugDesc->setText(loc.getString("SettingsPage", "Debug_Desc"));

    QLabel *maxLogsTitle = findChild<QLabel*>("MaxLogs_Title");
    if(maxLogsTitle) maxLogsTitle->setText(loc.getString("SettingsPage", "MaxLogs_Title"));
    QLabel *maxLogsDesc = findChild<QLabel*>("MaxLogs_Desc");
    if(maxLogsDesc) maxLogsDesc->setText(loc.getString("SettingsPage", "MaxLogs_Desc"));

    QLabel *logTitle = findChild<QLabel*>("Log_Title");
    if(logTitle) logTitle->setText(loc.getString("SettingsPage", "Log_Title"));
    QLabel *logDesc = findChild<QLabel*>("Log_Desc");
    if(logDesc) logDesc->setText(loc.getString("SettingsPage", "Log_Desc"));
    if(m_openLogBtn) m_openLogBtn->setText(loc.getString("SettingsPage", "Log_Btn"));

    QLabel *sidebarTitle = findChild<QLabel*>("Sidebar_Title");
    if(sidebarTitle) sidebarTitle->setText(loc.getString("SettingsPage", "Sidebar_Title"));
    QLabel *sidebarDesc = findChild<QLabel*>("Sidebar_Desc");
    if(sidebarDesc) sidebarDesc->setText(loc.getString("SettingsPage", "Sidebar_Desc"));
    
    QPushButton *githubLink = findChild<QPushButton*>("GithubLink");
    if(githubLink) githubLink->setText(loc.getString("SettingsPage", "GithubLink"));

    if(m_openSourceToggleBtn) m_openSourceToggleBtn->setText(loc.getString("SettingsPage", "OpenSourceBtn"));
}

void SettingsPage::updateTheme() {
    // Handled by MainWindow QSS
}

void SettingsPage::openUrl(const QString &url) {
    QDesktopServices::openUrl(QUrl(url));
    Logger::instance().logClick("OpenUrl: " + url);
}

void SettingsPage::toggleOpenSource() {
    m_openSourceArea->setVisible(!m_openSourceArea->isVisible());
    Logger::instance().logClick("ToggleOpenSource");
}

void SettingsPage::openLogDir() {
    Logger::instance().openLogDirectory();
    Logger::instance().logClick("OpenLogDir");
}
