//-------------------------------------------------------------------------------------
// AccountPage.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "AccountPage.h"
#include "AuthManager.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QFile>
#include <QTextStream>

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

AccountPage::AccountPage(QWidget *parent) : QWidget(parent) {
    setupUi();
    updateTexts();
    updateTheme();
}

void AccountPage::setupUi() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    QWidget *header = new QWidget();
    header->setObjectName("OverlayHeader");
    header->setFixedHeight(60);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 20, 0);

    QLabel *title = new QLabel("Account");
    title->setObjectName("AccountTitle");
    title->setStyleSheet("font-size: 18px; font-weight: bold;");
    
    QPushButton *closeBtn = new QPushButton("×");
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("border: none; font-size: 20px; color: #888;");
    connect(closeBtn, &QPushButton::clicked, this, &AccountPage::closeClicked);

    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(closeBtn);
    layout->addWidget(header);

    // Content
    QScrollArea *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    
    QWidget *content = new QWidget();
    content->setObjectName("AccountContent");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(40, 20, 40, 40);
    contentLayout->setSpacing(30);

    // Account Info Group
    QVBoxLayout *accountLayout = new QVBoxLayout();
    accountLayout->setSpacing(0);
    
    m_usernameLabel = new QLabel();
    m_usernameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    m_logoutBtn = new QPushButton("Logout");
    m_logoutBtn->setObjectName("LogoutBtn");
    m_logoutBtn->setCursor(Qt::PointingHandCursor);
    m_logoutBtn->setStyleSheet("QPushButton { color: #FF3B30; border: 1px solid #FF3B30; border-radius: 6px; padding: 5px 15px; background-color: transparent; } QPushButton:hover { background-color: rgba(255, 59, 48, 0.1); }");
    connect(m_logoutBtn, &QPushButton::clicked, this, &AccountPage::logoutRequested);

    accountLayout->addWidget(createSettingRow("UserInfo", ":/icons/user.svg", "Current User", "Logged in as", m_logoutBtn));
    accountLayout->addWidget(createSettingRow("UpdateChannel", ":/icons/update-channel.svg", "Update Channel", "Stable", nullptr));

    contentLayout->addWidget(createGroup("AccountInfo", accountLayout));
    contentLayout->addStretch();

    scroll->setWidget(content);
    layout->addWidget(scroll);
}

QWidget* AccountPage::createGroup(const QString &title, QLayout *contentLayout) {
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

QWidget* AccountPage::createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *control) {
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
    
    if (id == "UserInfo") {
        m_usernameLabel = new QLabel(desc);
        m_usernameLabel->setObjectName(id + "_Desc");
        m_usernameLabel->setStyleSheet("color: #888; font-size: 12px;");
        textLayout->addWidget(titleLbl);
        textLayout->addWidget(m_usernameLabel);
    } else if (id == "UpdateChannel") {
        m_channelNameLabel = new QLabel(desc);
        m_channelNameLabel->setObjectName(id + "_Desc");
        m_channelNameLabel->setStyleSheet("color: #888; font-size: 12px;");
        textLayout->addWidget(titleLbl);
        textLayout->addWidget(m_channelNameLabel);
    } else {
        QLabel *descLbl = new QLabel(desc);
        descLbl->setObjectName(id + "_Desc");
        descLbl->setStyleSheet("color: #888; font-size: 12px;");
        textLayout->addWidget(titleLbl);
        textLayout->addWidget(descLbl);
    }
    
    layout->addWidget(iconLbl);
    layout->addLayout(textLayout);
    layout->addStretch();

    if (id == "UpdateChannel") {
        m_channelDescriptionLabel = new QLabel();
        m_channelDescriptionLabel->setObjectName(id + "_Value");
        m_channelDescriptionLabel->setStyleSheet("color: #888; font-size: 12px;");
        layout->addWidget(m_channelDescriptionLabel);
    } else if (control) {
        layout->addWidget(control);
    }
    
    return row;
}

void AccountPage::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();

    QLabel *accountTitle = findChild<QLabel*>("AccountTitle");
    if(accountTitle) accountTitle->setText(loc.getString("AccountPage", "AccountTitle"));
    
    QLabel *accountGroup = findChild<QLabel*>("AccountInfo_GroupTitle");
    if(accountGroup) accountGroup->setText(loc.getString("AccountPage", "Group_AccountInfo"));

    QLabel *userTitle = findChild<QLabel*>("UserInfo_Title");
    if(userTitle) userTitle->setText(loc.getString("AccountPage", "UserInfo_Title"));

    QLabel *channelTitle = findChild<QLabel*>("UpdateChannel_Title");
    if(channelTitle) channelTitle->setText(loc.getString("AccountPage", "UpdateChannel_Title"));
    
    if(m_logoutBtn) m_logoutBtn->setText(loc.getString("AccountPage", "LogoutBtn"));
    
    updateAccountInfo();
}

void AccountPage::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
                  
    QList<QLabel*> iconLabels = findChildren<QLabel*>("SettingIcon");
    for (QLabel* lbl : iconLabels) {
        QString iconPath = lbl->property("svgIcon").toString();
        if (!iconPath.isEmpty()) {
            lbl->setPixmap(loadSvgIcon(iconPath, isDark));
        }
    }
}

QString AccountPage::resolveLocalizedChannelText(const QString& key) const {
    if (key.isEmpty()) {
        return QString();
    }

    return LocalizationManager::instance().getString("AccountPage", key);
}

void AccountPage::updateAccountInfo() {
    LocalizationManager& loc = LocalizationManager::instance();

    if (m_usernameLabel) {
        QString username = AuthManager::instance().getCurrentUsername();
        if (username.isEmpty()) {
            m_usernameLabel->setText(loc.getString("AccountPage", "NotLoggedIn"));
        } else {
            m_usernameLabel->setText(username);
        }
    }

    const QString channelName = resolveLocalizedChannelText(AuthManager::instance().getChannelDisplayNameKey());
    const QString channelDescription = resolveLocalizedChannelText(AuthManager::instance().getChannelDescriptionKey());

    if (m_channelNameLabel) {
        if (channelName.isEmpty()) {
            m_channelNameLabel->setText(loc.getString("AccountPage", "NotLoggedIn"));
        } else {
            m_channelNameLabel->setText(channelName);
        }
    }

    if (m_channelDescriptionLabel) {
        m_channelDescriptionLabel->setText(channelDescription);
    }
}
