//-------------------------------------------------------------------------------------
// LoginDialog.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QProgressBar>
#include <QTimer>
#include <QDateTime>

#include "AuthManager.h"
#include "HttpClient.h"

class QToolButton;

class LoginDialog : public QWidget {
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    void updateTheme();
    void updateTexts();
    void showLogin();
    void hideLogin();
    void showAutoLoggingIn();

signals:
    void loginSuccessful();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onToggleModeClicked();
    void onLoginSuccess();
    void onLoginFailed(const QString& errorMsg);
    void onRegisterReply(const HttpResponse& response);
    void onTogglePasswordVisibility();
    void onAccountActionCountdownTick();

private:
    void setupUi();
    void setMode(bool isLogin);
    void updatePosition();
    void startAccountActionCountdown(const AccountActionInfo& info);
    void stopAccountActionCountdown();
    void refreshAccountActionStatusText();
    void setPlainStatusMessage(const QString& message, const QString& color);
    void showAccountActionStatusWidget(bool show);

    bool m_isLoginMode;
    QWidget *m_container;
    
    QStackedWidget *m_stackedWidget;
    QWidget *m_formPage;
    QWidget *m_loadingPage;

    QLineEdit *m_usernameInput;
    QLineEdit *m_passwordInput;
    QPushButton *m_actionBtn;
    QPushButton *m_toggleModeBtn;
    QWidget *m_statusContainer;
    QLabel *m_statusLabel;
    QWidget *m_accountActionStatusWidget;
    QWidget *m_accountActionLine1;
    QWidget *m_accountActionLine2;
    QWidget *m_accountActionLine3;
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QToolButton *m_togglePasswordBtn;
    
    QLabel *m_loadingIconLabel;
    QLabel *m_loadingMessageLabel;
    QProgressBar *m_loadingProgressBar;
    QTimer *m_accountActionCountdownTimer;
    AccountActionInfo m_pendingAccountActionInfo;
    QDateTime m_accountActionExpireAtUtc;
};

#endif // LOGINDIALOG_H