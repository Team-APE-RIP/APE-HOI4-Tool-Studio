//-------------------------------------------------------------------------------------
// ConfigPage.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ConfigPage.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "PathValidator.h"
#include "CustomMessageBox.h"
#include "PluginManager.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFileDialog>
#include <QGroupBox>
#include <QFile>
#include <QTextStream>
#include <algorithm>

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

ConfigPage::ConfigPage(QWidget *parent) : QWidget(parent) {
    setupUi();
    connect(&PluginManager::instance(), &PluginManager::pluginsLoaded, this, &ConfigPage::refreshPluginList);
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
    content->setObjectName("ConfigContent");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(40, 20, 40, 40);
    contentLayout->setSpacing(30);

    // Directories Group
    QVBoxLayout *dirLayout = new QVBoxLayout();
    dirLayout->setSpacing(0);

    QPushButton *reselectGameBtn = new QPushButton("Reselect");
    reselectGameBtn->setObjectName("GameDir_ReselectBtn");
    reselectGameBtn->setCursor(Qt::PointingHandCursor);
    connect(reselectGameBtn, &QPushButton::clicked, this, &ConfigPage::browseGamePath);
    m_gamePathValue = new QLabel(ConfigManager::instance().getGamePath());
    m_gamePathValue->setStyleSheet("color: #888; font-size: 12px; margin-right: 10px;");
    dirLayout->addWidget(createSettingRow("GameDir", ":/icons/folder-game.svg", "Game Directory", "Path to HOI4", m_gamePathValue, reselectGameBtn));

    QPushButton *reselectDocBtn = new QPushButton("Reselect");
    reselectDocBtn->setObjectName("DocDir_ReselectBtn");
    reselectDocBtn->setCursor(Qt::PointingHandCursor);
    connect(reselectDocBtn, &QPushButton::clicked, this, &ConfigPage::browseDocPath);
    m_docPathValue = new QLabel(ConfigManager::instance().getDocPath());
    m_docPathValue->setStyleSheet("color: #888; font-size: 12px; margin-right: 10px;");
    dirLayout->addWidget(createSettingRow("DocDir", ":/icons/folder-doc.svg", "Documents Directory", "Path to HOI4 Documents", m_docPathValue, reselectDocBtn));

    QPushButton *reselectModBtn = new QPushButton("Reselect");
    reselectModBtn->setObjectName("ModDir_ReselectBtn");
    reselectModBtn->setCursor(Qt::PointingHandCursor);
    connect(reselectModBtn, &QPushButton::clicked, this, &ConfigPage::browseModPath);
    m_modPathValue = new QLabel(ConfigManager::instance().getModPath());
    m_modPathValue->setStyleSheet("color: #888; font-size: 12px; margin-right: 10px;");
    dirLayout->addWidget(createSettingRow("ModDir", ":/icons/package.svg", "Current Mod", ConfigManager::instance().getModPath(), m_modPathValue, reselectModBtn));

    contentLayout->addWidget(createGroup("Directories", dirLayout));

    // Plugins Group
    m_pluginListLayout = new QVBoxLayout();
    m_pluginListLayout->setSpacing(0);
    refreshPluginList();
    contentLayout->addWidget(createGroup("Plugins", m_pluginListLayout));

    contentLayout->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll);
}

void ConfigPage::refreshPlugins() {
    PluginManager::instance().loadPlugins();
    refreshPluginList();
}

void ConfigPage::refreshPluginList() {
    if (!m_pluginListLayout) {
        return;
    }

    QLayoutItem *item;
    while ((item = m_pluginListLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const QList<PluginInfo> plugins = PluginManager::instance().getPlugins();
    LocalizationManager& loc = LocalizationManager::instance();

    if (plugins.isEmpty()) {
        QLabel *emptyLabel = new QLabel(loc.getString("ConfigPage", "Plugins_Empty"));
        emptyLabel->setObjectName("Plugins_Empty");
        emptyLabel->setStyleSheet("color: #888; font-size: 12px; padding: 12px 4px;");
        m_pluginListLayout->addWidget(emptyLabel);
        return;
    }

    QList<PluginInfo> sortedPlugins = plugins;
    std::sort(sortedPlugins.begin(), sortedPlugins.end(), [](const PluginInfo& left, const PluginInfo& right) {
        bool leftOk = false;
        bool rightOk = false;
        const int leftId = left.id.toInt(&leftOk);
        const int rightId = right.id.toInt(&rightOk);

        if (leftOk && rightOk) {
            return leftId < rightId;
        }

        return left.id < right.id;
    });

    for (const PluginInfo& plugin : sortedPlugins) {
        const QString title = plugin.name;
        const QString desc = plugin.author.isEmpty() ? "-" : plugin.author;

        QString valueText;
        if (plugin.libraryPath.isEmpty()) {
            valueText = QString("ID: %1\n%2").arg(plugin.id, loc.getString("ConfigPage", "Plugins_MetadataOnly"));
        } else {
            valueText = QString("ID: %1\nv%2").arg(plugin.id, plugin.version.isEmpty() ? "-" : plugin.version);
        }

        QLabel *valueLabel = new QLabel(valueText);
        valueLabel->setStyleSheet("color: #888; font-size: 12px; margin-right: 10px;");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_pluginListLayout->addWidget(createSettingRow("Plugin_" + plugin.name, ":/icons/package.svg", title, desc, valueLabel, nullptr));
    }
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
    layout->setContentsMargins(15, 10, 20, 10);
    layout->setSpacing(15);

    QLabel *iconLbl = new QLabel();
    iconLbl->setObjectName("SettingIcon");
    iconLbl->setFixedSize(34, 34);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setProperty("svgIcon", icon);

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

    QLabel *pluginGroup = findChild<QLabel*>("Plugins_GroupTitle");
    if(pluginGroup) pluginGroup->setText(loc.getString("ConfigPage", "Group_Plugins"));

    QLabel *gameTitle = findChild<QLabel*>("GameDir_Title");
    if(gameTitle) gameTitle->setText(loc.getString("ConfigPage", "GameDir_Title"));
    QLabel *gameDesc = findChild<QLabel*>("GameDir_Desc");
    if(gameDesc) gameDesc->setText(loc.getString("ConfigPage", "GameDir_Desc"));

    QLabel *docTitle = findChild<QLabel*>("DocDir_Title");
    if(docTitle) docTitle->setText(loc.getString("ConfigPage", "DocDir_Title"));
    QLabel *docDesc = findChild<QLabel*>("DocDir_Desc");
    if(docDesc) docDesc->setText(loc.getString("ConfigPage", "DocDir_Desc"));

    QPushButton *reselectGameBtn = findChild<QPushButton*>("GameDir_ReselectBtn");
    if(reselectGameBtn) reselectGameBtn->setText(loc.getString("ConfigPage", "ReselectBtn"));

    QPushButton *reselectDocBtn = findChild<QPushButton*>("DocDir_ReselectBtn");
    if(reselectDocBtn) reselectDocBtn->setText(loc.getString("ConfigPage", "ReselectBtn"));

    QLabel *modTitle = findChild<QLabel*>("ModDir_Title");
    if(modTitle) modTitle->setText(loc.getString("ConfigPage", "ModDir_Title"));
    QLabel *modDesc = findChild<QLabel*>("ModDir_Desc");
    if(modDesc) modDesc->setText(loc.getString("ConfigPage", "ModDir_Desc"));

    QPushButton *reselectModBtn = findChild<QPushButton*>("ModDir_ReselectBtn");
    if(reselectModBtn) reselectModBtn->setText(loc.getString("ConfigPage", "ReselectBtn"));

    // Refresh path values from ConfigManager (real-time read)
    if(m_gamePathValue) m_gamePathValue->setText(ConfigManager::instance().getGamePath());
    if(m_docPathValue) m_docPathValue->setText(ConfigManager::instance().getDocPath());
    if(m_modPathValue) m_modPathValue->setText(ConfigManager::instance().getModPath());

    refreshPluginList();
}

void ConfigPage::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();

    QList<QLabel*> iconLabels = findChildren<QLabel*>("SettingIcon");
    for (QLabel* lbl : iconLabels) {
        QString iconPath = lbl->property("svgIcon").toString();
        if (!iconPath.isEmpty()) {
            lbl->setPixmap(loadSvgIcon(iconPath, isDark));
        }
    }
}

void ConfigPage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    Logger::instance().logInfo("ConfigPage", "Config page shown, forcing plugin refresh");
    refreshPlugins();
}

void ConfigPage::browseGamePath() {
    LocalizationManager& loc = LocalizationManager::instance();
    QString dir = QFileDialog::getExistingDirectory(this,
        loc.getString("SetupDialog", "SelectGameDir"),
        ConfigManager::instance().getGamePath());
    if (!dir.isEmpty()) {
        // Validate game path before saving
        QString gameError = PathValidator::instance().validateGamePath(dir);
        if (!gameError.isEmpty()) {
            CustomMessageBox::information(this,
                loc.getString("Error", "GamePathInvalid"),
                loc.getString("Error", gameError));
            Logger::instance().logError("ConfigPage", "Game path validation failed: " + gameError);
            return;
        }

        ConfigManager::instance().setGamePath(dir);
        m_gamePathValue->setText(dir);
        emit gamePathChanged();
        Logger::instance().logClick("BrowseGamePath");
    }
}

void ConfigPage::browseDocPath() {
    LocalizationManager& loc = LocalizationManager::instance();
    QString dir = QFileDialog::getExistingDirectory(this,
        loc.getString("SetupDialog", "SelectDocDir"),
        ConfigManager::instance().getDocPath());
    if (!dir.isEmpty()) {
        // Validate doc path before saving
        QString docError = PathValidator::instance().validateDocPath(dir);
        if (!docError.isEmpty()) {
            CustomMessageBox::information(this,
                loc.getString("Error", "DocPathInvalid"),
                loc.getString("Error", docError));
            Logger::instance().logError("ConfigPage", "Doc path validation failed: " + docError);
            return;
        }

        ConfigManager::instance().setDocPath(dir);
        m_docPathValue->setText(dir);
        Logger::instance().logClick("BrowseDocPath");
    }
}

void ConfigPage::browseModPath() {
    LocalizationManager& loc = LocalizationManager::instance();
    QString dir = QFileDialog::getExistingDirectory(this,
        loc.getString("SetupDialog", "SelectModDir"),
        ConfigManager::instance().getModPath());
    if (!dir.isEmpty()) {
        // Validate mod path before saving
        QString modError = PathValidator::instance().validateModPath(dir);
        if (!modError.isEmpty()) {
            CustomMessageBox::information(this,
                loc.getString("Error", "ModPathInvalid"),
                loc.getString("Error", modError));
            Logger::instance().logError("ConfigPage", "Mod path validation failed: " + modError);
            return;
        }

        ConfigManager::instance().setModPath(dir);
        m_modPathValue->setText(dir);
        emit modPathChanged();
        Logger::instance().logClick("BrowseModPath");
    }
}