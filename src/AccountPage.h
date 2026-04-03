//-------------------------------------------------------------------------------------
// AccountPage.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef ACCOUNTPAGE_H
#define ACCOUNTPAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class AccountPage : public QWidget {
    Q_OBJECT

public:
    explicit AccountPage(QWidget *parent = nullptr);
    void updateTexts();
    void updateTheme();
    void updateAccountInfo();

signals:
    void closeClicked();
    void logoutRequested();

private:
    void setupUi();
    QWidget* createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *control);
    QWidget* createGroup(const QString &title, QLayout *contentLayout);
    QString resolveLocalizedChannelText(const QString& key) const;

    QLabel *m_usernameLabel;
    QLabel *m_channelNameLabel;
    QLabel *m_channelDescriptionLabel;
    QPushButton *m_logoutBtn;
};

#endif // ACCOUNTPAGE_H
