//-------------------------------------------------------------------------------------
// SetupDialog.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef SETUPDIALOG_H
#define SETUPDIALOG_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

class SetupDialog : public QWidget {
    Q_OBJECT

public:
    explicit SetupDialog(QWidget *parent = nullptr);

    QString getGamePath() const;
    QString getModPath() const;

    void showOverlay();
    void hideOverlay();
    void updateTheme();
    void updateTexts();

    // Static method to check if config is valid
    static bool isConfigValid();

signals:
    void setupCompleted();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void browseGamePath();
    void browseModPath();
    void browseDocPath();
    void onGamePathChanged(const QString &path);
    void onModPathChanged(const QString &path);
    void onDocPathChanged(const QString &path);
    void validateAndAccept();

private:
    void setupUi();
    void updatePosition();

    QWidget *m_container;
    QLabel *m_titleLabel;
    QLabel *m_iconLabel;
    QLabel *m_gameLabel;
    QLabel *m_docLabel;
    QLabel *m_modLabel;
    QLineEdit *m_gamePathEdit;
    QLineEdit *m_docPathEdit;
    QLineEdit *m_modPathEdit;
    QPushButton *m_browseGameBtn;
    QPushButton *m_browseDocBtn;
    QPushButton *m_browseModBtn;
    QPushButton *m_confirmBtn;
    
    bool m_isDarkMode;
};

#endif // SETUPDIALOG_H
