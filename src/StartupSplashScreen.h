//-------------------------------------------------------------------------------------
// StartupSplashScreen.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef STARTUPSPLASHSCREEN_H
#define STARTUPSPLASHSCREEN_H

#include <QWidget>

enum class StartupSplashStage {
    CheckingInstance,
    PreparingRuntime,
    LoadingLanguage,
    ConfiguringRuntime,
    OrganizingPackages,
    RefreshingSecurityBundle,
    PreparingAccountSession,
    RegisteringFileAssociations,
    CleaningCaches,
    BuildingMainWindow,
    OpeningMainWindow
};

class StartupSplashScreen : public QWidget {
public:
    explicit StartupSplashScreen(QWidget* parent = nullptr);

    void showSplash();
    void applyLocalizedStrings();
    void setProgress(qreal progress, StartupSplashStage stage);
    void setProgress(qreal progress, const QString& stageText);
    void finishWithMainWindow(QWidget* mainWindow);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void setLocalizedStrings(const QString& productName,
                             const QString& copyrightText,
                             const QString& artistCredit,
                             const QString& footerProductText);
    void centerOnPrimaryScreen();
    void processPaintEvents();

    QString m_productName;
    QString m_copyrightText;
    QString m_artistCredit;
    QString m_footerProductText;
    QString m_contributorsText;
    QString m_stageText;
    qreal m_progress = 0.0;
};

#endif // STARTUPSPLASHSCREEN_H
