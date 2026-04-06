//-------------------------------------------------------------------------------------
// Setup.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMouseEvent>
#include <QPoint>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
#include <QStringList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>

class Setup : public QDialog {
    Q_OBJECT

public:
    explicit Setup(QWidget *parent = nullptr);
    ~Setup() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void browseDirectory();
    void startInstall();
    void changeLanguage(int index);
    void closeWindow();

private:
    void setupUi();
    void loadLanguage(const QString& langCode);
    bool extractPayload(const QString& targetDir);
    void saveTempLanguage();
    void applyTheme();
    bool detectSystemDarkMode();

    void migrateLegacySettings();
    void populateLanguageCombo();
    QString normalizeLanguageCode(const QString& value) const;
    QString displayTextForLanguage(const QString& langCode) const;
    QString currentInstallPath() const;
    bool currentAutoSetupFlag() const;

    QMap<QString, QString> parseMetaFile(const QString& path) const;
    QMap<QString, QString> parseSimpleYamlFile(const QString& path) const;
    QString localizedValue(const QString& key, const QString& fallback = QString()) const;

    QMap<QString, QString> currentLoc;
    QMap<QString, QString> m_languageTextByCode;

    QWidget* m_centralWidget;
    QLabel* titleLabel;
    QLabel* pathLabel;
    QLineEdit* pathEdit;
    QPushButton* browseBtn;
    QComboBox* langCombo;
    QPushButton* installBtn;
    QProgressBar* progressBar;

    QString currentLang;
    bool m_isDarkMode;
    bool m_dragging;
    QPoint m_dragPosition;
    bool m_isAutoSetup;
};