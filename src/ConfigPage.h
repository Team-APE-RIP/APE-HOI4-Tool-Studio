//-------------------------------------------------------------------------------------
// ConfigPage.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QShowEvent>

class ConfigPage : public QWidget {
    Q_OBJECT

public:
    explicit ConfigPage(QWidget *parent = nullptr);
    void updateTexts();
    void updateTheme();
    void refreshPlugins();

signals:
    void closeClicked();
    void gamePathChanged();
    void modClosed();
    void modPathChanged();

private slots:
    void browseGamePath();
    void browseDocPath();
    void browseModPath();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setupUi();
    void refreshPluginList();
    QWidget* createGroup(const QString &title, QLayout *contentLayout);
    QWidget* createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *valueWidget, QWidget *control);

    QLabel *m_gamePathValue;
    QLabel *m_docPathValue;
    QLabel *m_modPathValue;
    QVBoxLayout *m_pluginListLayout;
};

#endif // CONFIGPAGE_H