//-------------------------------------------------------------------------------------
// AuthManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef AUTHMANAGER_H
#define AUTHMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QJsonObject>
#include <QByteArray>
#include <QElapsedTimer>

#include "HttpClient.h"

struct AccountActionInfo {
    bool active = false;
    bool blocking = false;
    bool permanent = false;
    QString type;
    QString code;
    QString reason;
    QString messageKey;
    QString untilAt;
    qint64 remainingSeconds = 0;
};

struct AuthResult {
    bool success = false;
    QString code;
    QString message;
    QString messageKey;
    AccountActionInfo accountAction;
};

class AuthManager : public QObject {
    Q_OBJECT

public:
    static AuthManager& instance();

    void init();
    void login(const QString& username, const QString& password);
    bool hasSavedCredentials() const;
    void autoLogin();
    bool isAuthenticated() const;
    bool isLoggingIn() const;
    bool isConnected() const;
    QString getToken() const;
    QString getChannel() const;
    QString getChannelDisplayNameKey() const;
    QString getChannelDescriptionKey() const;
    QString getHWID() const;
    QString getCurrentUsername() const;
    bool hasBlockingAccountAction() const;
    AccountActionInfo getAccountActionInfo() const;
    static QString getApiBaseHost();
    static QString getApiBaseUrl(bool useHttps = true);
    static QString buildApiUrl(const QString& path, bool useHttps = true);
    static QString sanitizeSensitiveApiText(const QString& text);
    void logout();
    void shutdown();

    AuthResult parseAuthResult(const QJsonObject& obj) const;
    AccountActionInfo parseAccountActionInfo(const QJsonObject& obj) const;
    QString resolveServerMessage(const QJsonObject& obj) const;
    QString localizeAuthResultMessage(const AuthResult& result) const;
    QString localizeAccountActionMessage(const AccountActionInfo& info, const QString& tableName) const;

signals:
    void loginSuccess();
    void loginFailed(const QString& errorMsg);
    void authExpired();
    void adReceived(const QString& text, const QString& imageUrl, const QString& targetUrl);
    void connectionLost();
    void connectionRestored();
    void accountActionBlocked();
    void accountActionCleared();

private:
    AuthManager(QObject* parent = nullptr);
    ~AuthManager();

    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    QString generateHWID();
    void saveCredentials();
    void loadCredentials();

    void sendHeartbeat();
    void startConnectionCheck();
    void stopConnectionCheck();
    void onConnectionCheckTimer();
    void sendPingChallenge();
    void sendPingVerify(const QString& nonce, const QString& timestamp, const QString& serverSig);

    void onLoginReply(const HttpResponse& response);
    void onHeartbeatReply(const HttpResponse& response);
    void onChallengeReply(const HttpResponse& response);
    void onVerifyReply(const HttpResponse& response);
    void clearAccountActionInfo(bool emitSignal = true);
    void setAccountActionInfo(const AccountActionInfo& info, bool emitSignal = true);
    QString formatRemainingTime(qint64 remainingSeconds) const;

    QByteArray getDeobfuscatedSharedSecret() const;

    QTimer* m_heartbeatTimer;
    QTimer* m_connectionCheckTimer;

    QString m_hwid;
    QString m_username;
    QString m_password;
    QString m_token;
    QString m_channel;
    QString m_channelDisplayNameKey;
    QString m_channelDescriptionKey;
    QString m_userId;
    AccountActionInfo m_accountActionInfo;
    bool m_isAuthenticated;
    bool m_isLoggingIn;

    bool m_isConnected;
    bool m_connectionWarningActive;
    QElapsedTimer m_connectionLossWindowTimer;
    bool m_hasSuccessfulConnectionCheck;

    static constexpr int CONNECTION_CHECK_INTERVAL_MS = 5000;
    static constexpr int CONNECTION_LOSS_THRESHOLD_MS = 32000;
};

#endif // AUTHMANAGER_H