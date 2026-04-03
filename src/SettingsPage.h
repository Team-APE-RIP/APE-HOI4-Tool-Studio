//-------------------------------------------------------------------------------------
// SettingsPage.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

class QPropertyAnimation;
class QGraphicsOpacityEffect;

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    void updateTexts();
    void updateTheme();

signals:
    void closeClicked();
    void showUserAgreement();
    void themeChanged();
    void languageChanged();
    void debugModeChanged(bool enabled);
    void sidebarCompactChanged(bool enabled);

private slots:
    void openUrl(const QString &url);
    void toggleOpenSource();
    void openLogDir();
    void createStartMenuShortcut();
    void clearAppCache();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void setupUi();
    void updateOpenSourceToggleText();
    void refreshOpenSourceLayout();
    QWidget* createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *control);
    QWidget* createGroup(const QString &title, QLayout *contentLayout);

    QComboBox *m_themeCombo;
    QComboBox *m_languageCombo;
    QCheckBox *m_debugCheck;
    QCheckBox *m_sidebarCompactCheck;
    QSpinBox *m_maxLogFilesSpin;
    QLabel *m_versionLabel;
    QWidget *m_openSourceArea;
    QWidget *m_openSourceViewport;
    QWidget *m_openSourceCardsWidget;
    QPushButton *m_openSourceToggleBtn;
    QPushButton *m_userAgreementBtn;
    QPushButton *m_openLogBtn;
    QPushButton *m_pinToStartBtn;
    QPushButton *m_clearCacheBtn;
    QPropertyAnimation *m_openSourceExpandAnimation;
    QPropertyAnimation *m_openSourceOpacityAnimation;
    QGraphicsOpacityEffect *m_openSourceOpacityEffect;
    bool m_isOpenSourceExpanded;
    QString m_openSourceToggleBaseText;
};

#endif // SETTINGSPAGE_H