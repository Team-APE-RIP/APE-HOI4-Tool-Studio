#include "ConfigPage.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFileDialog>
#include <QGroupBox>

ConfigPage::ConfigPage(QWidget *parent) : QWidget(parent) {
    setupUi();
    updateTexts();
    updateTheme();
}

void ConfigPage::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    QWidget *header = new QWidget();
    header->setObjectName("OverlayHeader");
    header->setFixedHeight(60);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 20, 0);

    QLabel *title = new QLabel("Configuration");
    title->setObjectName("ConfigTitle");
    title->setStyleSheet("font-size: 18px; font-weight: bold;");
    
    QPushButton *closeBtn = new QPushButton("×");
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("border: none; font-size: 20px; color: #888;");
    connect(closeBtn, &QPushButton::clicked, this, &ConfigPage::closeClicked);

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

    // Directories Group
    QVBoxLayout *dirLayout = new QVBoxLayout();
    dirLayout->setSpacing(0);

    QPushButton *reselectGameBtn = new QPushButton("Reselect");
    reselectGameBtn->setObjectName("ReselectBtn");
    reselectGameBtn->setCursor(Qt::PointingHandCursor);
    connect(reselectGameBtn, &QPushButton::clicked, this, &ConfigPage::browseGamePath);
    m_gamePathValue = new QLabel(ConfigManager::instance().getGamePath());
    m_gamePathValue->setStyleSheet("color: #888; font-size: 12px; margin-right: 10px;");
    dirLayout->addWidget(createSettingRow("GameDir", "📁", "Game Directory", "Path to HOI4", m_gamePathValue, reselectGameBtn));

    QPushButton *closeModBtn = new QPushButton("Close Current Mod");
    closeModBtn->setObjectName("CloseModBtn");
    closeModBtn->setStyleSheet("background-color: #FF3B30; color: white; border: none; padding: 8px 16px; border-radius: 6px;");
    closeModBtn->setCursor(Qt::PointingHandCursor);
    connect(closeModBtn, &QPushButton::clicked, this, &ConfigPage::closeCurrentMod);
    m_modPathValue = new QLabel(ConfigManager::instance().getModPath());
    m_modPathValue->setStyleSheet("color: #888; font-size: 12px; margin-right: 10px;");
    dirLayout->addWidget(createSettingRow("ModDir", "📦", "Current Mod", ConfigManager::instance().getModPath(), m_modPathValue, closeModBtn));

    contentLayout->addWidget(createGroup("Directories", dirLayout));

    contentLayout->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll);
}

QWidget* ConfigPage::createGroup(const QString &title, QLayout *contentLayout) {
    QGroupBox *group = new QGroupBox();
    group->setObjectName("SettingsGroup");
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(0, 10, 0, 0);
    groupLayout->setSpacing(0);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName(title + "_GroupTitle");
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #888; margin-left: 10px; margin-bottom: 5px;");
    
    QWidget *container = new QWidget();
    container->setObjectName("GroupContainer");
    container->setLayout(contentLayout);

    groupLayout->addWidget(titleLabel);
    groupLayout->addWidget(container);
    
    return group;
}

QWidget* ConfigPage::createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *valueWidget, QWidget *control) {
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
    if (valueWidget) layout->addWidget(valueWidget);
    if (control) layout->addWidget(control);
    
    return row;
}

void ConfigPage::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    QLabel *configTitle = findChild<QLabel*>("ConfigTitle");
    if(configTitle) configTitle->setText(loc.getString("ConfigPage", "ConfigTitle"));
    
    QLabel *dirGroup = findChild<QLabel*>("Directories_GroupTitle");
    if(dirGroup) dirGroup->setText(loc.getString("ConfigPage", "Group_Directories"));

    QLabel *gameTitle = findChild<QLabel*>("GameDir_Title");
    if(gameTitle) gameTitle->setText(loc.getString("ConfigPage", "GameDir_Title"));
    QLabel *gameDesc = findChild<QLabel*>("GameDir_Desc");
    if(gameDesc) gameDesc->setText(loc.getString("ConfigPage", "GameDir_Desc"));
    
    QPushButton *reselectBtn = findChild<QPushButton*>("ReselectBtn");
    if(reselectBtn) reselectBtn->setText(loc.getString("ConfigPage", "ReselectBtn"));
    
    QLabel *modTitle = findChild<QLabel*>("ModDir_Title");
    if(modTitle) modTitle->setText(loc.getString("ConfigPage", "ModDir_Title"));
    QLabel *modDesc = findChild<QLabel*>("ModDir_Desc");
    if(modDesc) modDesc->setText(loc.getString("ConfigPage", "ModDir_Desc"));
    
    QPushButton *closeModBtn = findChild<QPushButton*>("CloseModBtn");
    if(closeModBtn) closeModBtn->setText(loc.getString("ConfigPage", "CloseModBtn"));
}

void ConfigPage::updateTheme() {
    // Handled by parent
}

void ConfigPage::browseGamePath() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Game Directory", ConfigManager::instance().getGamePath());
    if (!dir.isEmpty()) {
        ConfigManager::instance().setGamePath(dir);
        m_gamePathValue->setText(dir);
        emit gamePathChanged();
        Logger::instance().logClick("BrowseGamePath");
    }
}

void ConfigPage::closeCurrentMod() {
    ConfigManager::instance().clearModPath();
    emit modClosed();
    Logger::instance().logClick("CloseCurrentMod");
}
