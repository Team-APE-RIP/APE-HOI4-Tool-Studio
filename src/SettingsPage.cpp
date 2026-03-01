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
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QApplication>

static QPixmap loadSvgIcon(const QString &path, bool isDark) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QPixmap();

    QString svgContent = QTextStream(&file).readAll();
    file.close();

    QString color = isDark ? "#E0E0E0" : "#333333";
    svgContent.replace("currentColor", color);

    QPixmap pixmap;
    pixmap.loadFromData(svgContent.toUtf8(), "SVG");
    return pixmap;
}

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
    interfaceLayout->addWidget(createSettingRow("Theme", ":/icons/palette.svg", "Theme Mode", "Select application appearance", m_themeCombo));

    m_sidebarCompactCheck = new QCheckBox();
    m_sidebarCompactCheck->setChecked(ConfigManager::instance().getSidebarCompactMode());
    connect(m_sidebarCompactCheck, &QCheckBox::toggled, [this](bool checked){
        ConfigManager::instance().setSidebarCompactMode(checked);
        emit sidebarCompactChanged(checked);
    });
    // Restore the correct icon for the sidebar setting
    interfaceLayout->addWidget(createSettingRow("Sidebar", ":/icons/sidebar.svg", "Compact Sidebar", "Auto-collapse sidebar", m_sidebarCompactCheck));

    contentLayout->addWidget(createGroup("Interface", interfaceLayout));

    // 1.5 Accessibility Group
    QVBoxLayout *accessibilityLayout = new QVBoxLayout();
    accessibilityLayout->setSpacing(0);

    m_languageCombo = new QComboBox();
    m_languageCombo->addItems({"English", "简体中文", "繁體中文"});
    m_languageCombo->setCurrentText(ConfigManager::instance().getLanguage());
    connect(m_languageCombo, &QComboBox::currentTextChanged, [this](const QString &lang){
        if (lang != ConfigManager::instance().getLanguage()) {
            ConfigManager::instance().setLanguage(lang);
            emit languageChanged();
        }
    });
    accessibilityLayout->addWidget(createSettingRow("Lang", ":/icons/globe.svg", "Language", "Restart required to apply changes", m_languageCombo));

    m_pinToStartBtn = new QPushButton("Pin");
    m_pinToStartBtn->setObjectName("PinToStartBtn");
    m_pinToStartBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pinToStartBtn, &QPushButton::clicked, this, &SettingsPage::createStartMenuShortcut);
    accessibilityLayout->addWidget(createSettingRow("PinToStart", ":/icons/pin.svg", "Pin to Start", "Create a shortcut in the Start menu", m_pinToStartBtn));

    m_clearCacheBtn = new QPushButton("Clear");
    m_clearCacheBtn->setObjectName("ClearCacheBtn");
    m_clearCacheBtn->setCursor(Qt::PointingHandCursor);
    connect(m_clearCacheBtn, &QPushButton::clicked, this, &SettingsPage::clearAppCache);
    accessibilityLayout->addWidget(createSettingRow("ClearCache", ":/icons/trash.svg", "Clear App Cache", "App will close automatically after clearing", m_clearCacheBtn));

    contentLayout->addWidget(createGroup("Accessibility", accessibilityLayout));

    // 2. Debug Group
    QVBoxLayout *debugLayout = new QVBoxLayout();
    debugLayout->setSpacing(0);

    m_debugCheck = new QCheckBox();
    m_debugCheck->setChecked(ConfigManager::instance().getDebugMode());
    connect(m_debugCheck, &QCheckBox::toggled, [this](bool checked){
        ConfigManager::instance().setDebugMode(checked);
        emit debugModeChanged(checked);
    });
    debugLayout->addWidget(createSettingRow("Debug", ":/icons/bug.svg", "Show Usage Overlay", "Show memory usage overlay", m_debugCheck));

    m_maxLogFilesSpin = new QSpinBox();
    m_maxLogFilesSpin->setRange(1, 100);
    m_maxLogFilesSpin->setValue(ConfigManager::instance().getMaxLogFiles());
    connect(m_maxLogFilesSpin, QOverload<int>::of(&QSpinBox::valueChanged), [](int value){
        ConfigManager::instance().setMaxLogFiles(value);
    });
    debugLayout->addWidget(createSettingRow("MaxLogs", ":/icons/broom.svg", "Max Log Files", "Number of log files to keep", m_maxLogFilesSpin));

    m_openLogBtn = new QPushButton("Open Logs");
    m_openLogBtn->setObjectName("OpenLogBtn");
    m_openLogBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openLogBtn, &QPushButton::clicked, this, &SettingsPage::openLogDir);
    debugLayout->addWidget(createSettingRow("Log", ":/icons/folder.svg", "Log Directory", "Open application logs", m_openLogBtn));

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
    m_versionLabel = new QLabel(QString("v%1").arg(APP_VERSION));
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
    QGridLayout *osLayout = new QGridLayout(m_openSourceArea);
    osLayout->setContentsMargins(10, 10, 10, 10);
    osLayout->setSpacing(10);
    
    QFile osFile(":/openSource.json");
    if (osFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = osFile.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            int row = 0;
            int col = 0;
            for (int i = 0; i < arr.size(); ++i) {
                QJsonValue val = arr.at(i);
                if (val.isObject()) {
                    QJsonObject obj = val.toObject();
                    QString name = obj["name"].toString();
                    QString license = obj["license"].toString();
                    QString url = obj["url"].toString();
                    
                    QPushButton *cardBtn = new QPushButton();
                    cardBtn->setCursor(Qt::PointingHandCursor);
                    cardBtn->setFixedSize(120, 60);
                    
                    // Use HTML to format the text
                    QString btnText = QString("<div style='text-align:center;'><b>%1</b><br><span style='font-size:10px; color:#888;'>%2</span></div>").arg(name, license);
                    
                    // We need a QLabel inside the button to render HTML properly, or just use plain text if HTML is not supported well in QPushButton.
                    // Actually, QPushButton supports basic HTML via its text property in some styles, but a better way is to use a layout inside the button or just plain text with newlines.
                    // Let's use a layout inside the button for better control.
                    QVBoxLayout *cardLayout = new QVBoxLayout(cardBtn);
                    cardLayout->setContentsMargins(5, 5, 5, 5);
                    cardLayout->setSpacing(2);
                    
                    QLabel *nameLbl = new QLabel(name);
                    nameLbl->setAlignment(Qt::AlignCenter);
                    nameLbl->setStyleSheet("font-weight: bold; font-size: 12px; border: none; background: transparent;");
                    
                    QLabel *licLbl = new QLabel(license);
                    licLbl->setAlignment(Qt::AlignCenter);
                    licLbl->setStyleSheet("color: #888; font-size: 10px; border: none; background: transparent;");
                    
                    cardLayout->addWidget(nameLbl);
                    cardLayout->addWidget(licLbl);
                    
                    cardBtn->setStyleSheet(
                        "QPushButton {"
                        "   background-color: rgba(128, 128, 128, 0.1);"
                        "   border: 1px solid rgba(128, 128, 128, 0.2);"
                        "   border-radius: 8px;"
                        "}"
                        "QPushButton:hover {"
                        "   background-color: rgba(128, 128, 128, 0.2);"
                        "   border: 1px solid rgba(128, 128, 128, 0.4);"
                        "}"
                    );
                    
                    if (!url.isEmpty()) {
                        connect(cardBtn, &QPushButton::clicked, [this, url]() {
                            openUrl(url);
                        });
                    }
                    
                    osLayout->addWidget(cardBtn, row, col);
                    
                    col++;
                    if (col >= 7) {
                        col = 0;
                        row++;
                    }
                }
            }
        }
        osFile.close();
    }

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
    layout->setContentsMargins(15, 10, 20, 10);
    layout->setSpacing(15);
    
    QLabel *iconLbl = new QLabel();
    iconLbl->setObjectName("SettingIcon");
    iconLbl->setFixedSize(34, 34);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setProperty("svgIcon", icon); // Store path for theme updates
    
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    iconLbl->setPixmap(loadSvgIcon(icon, isDark));
    
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
    QLabel *accessibilityGroup = findChild<QLabel*>("Accessibility_GroupTitle");
    if(accessibilityGroup) accessibilityGroup->setText(loc.getString("SettingsPage", "Group_Accessibility"));
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

    QLabel *pinTitle = findChild<QLabel*>("PinToStart_Title");
    if(pinTitle) pinTitle->setText(loc.getString("SettingsPage", "PinToStart_Title"));
    QLabel *pinDesc = findChild<QLabel*>("PinToStart_Desc");
    if(pinDesc) pinDesc->setText(loc.getString("SettingsPage", "PinToStart_Desc"));
    if(m_pinToStartBtn) m_pinToStartBtn->setText(loc.getString("SettingsPage", "PinToStart_Title")); // Or a specific button text if needed

    QLabel *clearTitle = findChild<QLabel*>("ClearCache_Title");
    if(clearTitle) clearTitle->setText(loc.getString("SettingsPage", "ClearCache_Title"));
    QLabel *clearDesc = findChild<QLabel*>("ClearCache_Desc");
    if(clearDesc) clearDesc->setText(loc.getString("SettingsPage", "ClearCache_Desc"));
    if(m_clearCacheBtn) m_clearCacheBtn->setText(loc.getString("SettingsPage", "ClearCache_Title")); // Or a specific button text if needed
    
    QPushButton *githubLink = findChild<QPushButton*>("GithubLink");
    if(githubLink) githubLink->setText(loc.getString("SettingsPage", "GithubLink"));

    if(m_openSourceToggleBtn) m_openSourceToggleBtn->setText(loc.getString("SettingsPage", "OpenSourceBtn"));
}

void SettingsPage::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
                  
    QList<QLabel*> iconLabels = findChildren<QLabel*>("SettingIcon");
    for (QLabel* lbl : iconLabels) {
        QString iconPath = lbl->property("svgIcon").toString();
        if (!iconPath.isEmpty()) {
            lbl->setPixmap(loadSvgIcon(iconPath, isDark));
        }
    }
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

void SettingsPage::createStartMenuShortcut() {
    // For Windows Start Menu, the typical path for current user is:
    // %APPDATA%\Microsoft\Windows\Start Menu\Programs
    // QStandardPaths::ApplicationsLocation usually points to this on Windows.
    QString startMenuPath = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (startMenuPath.isEmpty()) {
        Logger::instance().logError("Settings", "Could not find Start Menu path");
        return;
    }

    QString shortcutPath = QDir(startMenuPath).filePath("APE HOI4 Tool Studio.lnk");
    QString targetPath = QCoreApplication::applicationFilePath();

    // In Qt, QFile::link creates a shortcut on Windows
    QFile::remove(shortcutPath); // Remove if it already exists
    if (QFile::link(targetPath, shortcutPath)) {
        Logger::instance().logInfo("Settings", "Successfully created Start Menu shortcut at: " + shortcutPath);
    } else {
        Logger::instance().logError("Settings", "Failed to create Start Menu shortcut at: " + shortcutPath);
    }
    Logger::instance().logClick("CreateStartMenuShortcut");
}

void SettingsPage::clearAppCache() {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
    QDir dir(tempDir);
    if (dir.exists()) {
        dir.removeRecursively();
        Logger::instance().logInfo("Settings", "Cleared app cache at: " + tempDir);
    }
    Logger::instance().logClick("ClearAppCache");
    QApplication::quit();
}
