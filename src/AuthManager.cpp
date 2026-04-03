//-------------------------------------------------------------------------------------
// AuthManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "AuthManager.h"

#include "LocalizationManager.h"
#include "Logger.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>
#include <QSettings>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>
#include <QUuid>
#include <QUrl>

namespace {
const QByteArray kPingServerSecret("22105c8f255c1ebbc8cbcfd192aabb45de9beb1068b6ec1ef5017313d505223475b620899e9545659cc4c6ff88114ebabb5679c0f0e03286b58029ce40ca9cd5");

QString computeHmacHex(const QByteArray& key, const QByteArray& message) {
    return QString(QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256).toHex());
}

void applyCommonApiHeaders(HttpRequestOptions& options) {
    HttpClient::addOrReplaceHeader(options, "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36 APEHOI4ToolStudio/1.0");
    HttpClient::addOrReplaceHeader(options, "Accept", "application/json, text/plain, */*");
    HttpClient::addOrReplaceHeader(options, "Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
}

void applyStrictNoCacheHeaders(HttpRequestOptions& options) {
    HttpClient::addOrReplaceHeader(options, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    HttpClient::addOrReplaceHeader(options, "Pragma", "no-cache");
    HttpClient::addOrReplaceHeader(options, "Expires", "0");
}

QString normalizedBaseHostFromResource() {
    static const QString cachedBaseHost = []() -> QString {
        QFile file(QStringLiteral(":/baseurl.txt"));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString();
        }

        const QString rawText = QTextStream(&file).readAll().trimmed();
        file.close();

        if (rawText.isEmpty()) {
            return QString();
        }

        QString normalized = rawText;
        normalized.remove(QRegularExpression(QStringLiteral("^https?://"), QRegularExpression::CaseInsensitiveOption));
        while (normalized.endsWith('/')) {
            normalized.chop(1);
        }

        const QUrl httpsUrl(QStringLiteral("https://") + normalized);
        if (!httpsUrl.isValid() || httpsUrl.host().isEmpty()) {
            return QString();
        }

        return normalized;
    }();

    return cachedBaseHost;
}

QString sanitizeSensitiveAuthEndpointText(const QString& text) {
    if (text.isEmpty()) {
        return text;
    }

    QString sanitized = text;
    const QString baseHost = normalizedBaseHostFromResource();
    static const QString kSensitiveAuthPathPrefix = QStringLiteral("/api/v1/auth");
    static const QString kRedactedAuthEndpoint = QStringLiteral("<redacted-auth-endpoint>");
    static const QString kRedactedApiBase = QStringLiteral("<redacted-api>");

    if (!baseHost.isEmpty()) {
        const QString httpsBaseUrl = QStringLiteral("https://") + baseHost;
        const QString httpBaseUrl = QStringLiteral("http://") + baseHost;
        const QString httpsAuthEndpoint = httpsBaseUrl + QStringLiteral("/api/v1/auth");
        const QString httpAuthEndpoint = httpBaseUrl + QStringLiteral("/api/v1/auth");

        sanitized.replace(httpsAuthEndpoint, kRedactedAuthEndpoint, Qt::CaseInsensitive);
        sanitized.replace(httpAuthEndpoint, kRedactedAuthEndpoint, Qt::CaseInsensitive);
        sanitized.replace(httpsBaseUrl, kRedactedApiBase, Qt::CaseInsensitive);
        sanitized.replace(httpBaseUrl, kRedactedApiBase, Qt::CaseInsensitive);
    }

    sanitized.replace(kSensitiveAuthPathPrefix, kRedactedAuthEndpoint, Qt::CaseInsensitive);

    return sanitized;
}

QString formatReplyHeaders(const HttpResponse& response) {
    if (response.headers.isEmpty()) {
        return QString();
    }

    QStringList headerLines;
    for (const HttpHeader& header : response.headers) {
        headerLines.append(QString("%1: %2").arg(QString::fromUtf8(header.name), QString::fromUtf8(header.value)));
    }

    return headerLines.join(" | ");
}

void logNetworkFailureDetails(const QString& context, const HttpResponse& response) {
    const QString responseText = sanitizeSensitiveAuthEndpointText(QString::fromUtf8(response.body));
    const QString responseHeaders = sanitizeSensitiveAuthEndpointText(formatReplyHeaders(response));
    const QString errorDetail = sanitizeSensitiveAuthEndpointText(response.errorMessage);

    Logger::instance().logError(
        "AuthManager",
        QString("%1 failed. http=%2 reason=%3 body=%4 headers=%5 backend=%6")
            .arg(context)
            .arg(response.statusCode)
            .arg(errorDetail.isEmpty() ? QStringLiteral("<none>") : errorDetail)
            .arg(responseText.isEmpty() ? QStringLiteral("<empty>") : responseText)
            .arg(responseHeaders.isEmpty() ? QStringLiteral("<none>") : responseHeaders)
            .arg(response.backendName.isEmpty() ? QStringLiteral("<unknown>") : response.backendName)
    );
}

}

AuthManager& AuthManager::instance() {
    static AuthManager instance;
    return instance;
}

QString AuthManager::getApiBaseHost() {
    return normalizedBaseHostFromResource();
}

QString AuthManager::getApiBaseUrl(bool useHttps) {
    const QString baseHost = getApiBaseHost();
    if (baseHost.isEmpty()) {
        return QString();
    }

    return QStringLiteral("%1://%2")
        .arg(useHttps ? QStringLiteral("https") : QStringLiteral("http"))
        .arg(baseHost);
}

QString AuthManager::buildApiUrl(const QString& path, bool useHttps) {
    QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        return getApiBaseUrl(useHttps);
    }

    if (!normalizedPath.startsWith('/')) {
        normalizedPath.prepend('/');
    }

    return getApiBaseUrl(useHttps) + normalizedPath;
}

QString AuthManager::sanitizeSensitiveApiText(const QString& text) {
    return sanitizeSensitiveAuthEndpointText(text);
}

AuthManager::AuthManager(QObject* parent)
    : QObject(parent)
    , m_heartbeatTimer(new QTimer(this))
    , m_connectionCheckTimer(new QTimer(this))
    , m_isAuthenticated(false)
    , m_channel("stable")
    , m_isLoggingIn(false)
    , m_isConnected(false)
    , m_connectionWarningActive(false)
    , m_hasSuccessfulConnectionCheck(false) {
    m_hwid = generateHWID();

    connect(m_heartbeatTimer, &QTimer::timeout, this, &AuthManager::sendHeartbeat);

    m_connectionCheckTimer->setInterval(CONNECTION_CHECK_INTERVAL_MS);
    connect(m_connectionCheckTimer, &QTimer::timeout, this, &AuthManager::onConnectionCheckTimer);
}

AuthManager::~AuthManager() {
}

void AuthManager::shutdown() {
    Logger::instance().logInfo("AuthManager", "Shutdown requested, stopping timers and clearing auth state");

    if (m_heartbeatTimer) {
        m_heartbeatTimer->stop();
    }

    stopConnectionCheck();

    m_isLoggingIn = false;
    m_isAuthenticated = false;
    m_isConnected = false;
    m_connectionWarningActive = false;
    m_hasSuccessfulConnectionCheck = false;

    m_token.clear();
    m_channel = "stable";
    m_channelDisplayNameKey.clear();
    m_channelDescriptionKey.clear();
    m_userId.clear();
    clearAccountActionInfo(false);

    disconnect(this, nullptr, nullptr, nullptr);
}

void AuthManager::init() {
    loadCredentials();
}

bool AuthManager::hasSavedCredentials() const {
    return !m_username.isEmpty() && !m_password.isEmpty();
}

void AuthManager::autoLogin() {
    if (hasSavedCredentials()) {
        login(m_username, m_password);
    }
}

QString AuthManager::generateHWID() {
    QByteArray hwidData = QSysInfo::machineUniqueId();
    hwidData.append(QSysInfo::currentCpuArchitecture().toUtf8());
    return QString(QCryptographicHash::hash(hwidData, QCryptographicHash::Sha256).toHex());
}

void AuthManager::login(const QString& username, const QString& password) {
    m_username = username;
    m_password = password;
    m_isLoggingIn = true;

    Logger::instance().logInfo("AuthManager", "Attempting login for user: " + m_username);

    QJsonObject json;
    json["username"] = m_username;
    json["password"] = m_password;
    json["hwid"] = m_hwid;

    const QByteArray requestData = QJsonDocument(json).toJson(QJsonDocument::Compact);
    HttpRequestOptions options = HttpClient::createJsonPost(QUrl(buildApiUrl("/api/v1/auth/login")), requestData);
    applyCommonApiHeaders(options);
    options.category = HttpRequestCategory::Auth;
    options.timeoutMs = 20000;
    options.connectTimeoutMs = 5000;
    options.maxRetries = 1;
    options.retryOnHttp5xx = true;

    QJsonObject logJson = json;
    logJson["password"] = "***";
    Logger::instance().logInfo("AuthManager", "Login request data: " + QString(QJsonDocument(logJson).toJson(QJsonDocument::Compact)));

    HttpClient::instance().send(options, this, [this](const HttpResponse& response) {
        onLoginReply(response);
    });
}

void AuthManager::onLoginReply(const HttpResponse& response) {
    m_isLoggingIn = false;

    if (!response.success) {
        const QString errorCode = response.statusCode > 0 ? QString::number(response.statusCode) : QStringLiteral("0");
        const QString sanitizedErrorDetail = sanitizeSensitiveAuthEndpointText(response.errorMessage);
        logNetworkFailureDetails("Login request", response);
        m_isAuthenticated = false;
        emit loginFailed(QString("NETWORK_ERROR:%1:%2").arg(errorCode, sanitizedErrorDetail));
        return;
    }

    Logger::instance().logInfo("AuthManager", "Login response data: " + QString::fromUtf8(response.body));

    const QJsonDocument doc = QJsonDocument::fromJson(response.body);
    if (!doc.isObject()) {
        Logger::instance().logError("AuthManager", "Invalid login response format. Response was not a JSON object.");
        m_isAuthenticated = false;
        emit loginFailed("Invalid server response format");
        return;
    }

    const QJsonObject obj = doc.object();
    const AuthResult authResult = parseAuthResult(obj);

    if (authResult.success) {
        m_token = obj["token"].toString();
        m_channel = obj["channel"].toString();
        if (m_channel.isEmpty()) {
            m_channel = "stable";
        }

        const QJsonObject channelMeta = obj["channel_meta"].toObject();
        m_channelDisplayNameKey = channelMeta["display_name_key"].toString();
        m_channelDescriptionKey = channelMeta["description_key"].toString();

        if (m_channelDisplayNameKey.isEmpty()) {
            m_channelDisplayNameKey = QString("channel.%1.name").arg(m_channel);
        }
        if (m_channelDescriptionKey.isEmpty()) {
            m_channelDescriptionKey = QString("channel.%1.description").arg(m_channel);
        }

        m_userId = obj["userId"].toVariant().toString();
        clearAccountActionInfo(false);
        m_isAuthenticated = true;
        m_isConnected = false;
        m_connectionWarningActive = false;
        m_hasSuccessfulConnectionCheck = false;
        m_connectionLossWindowTimer.restart();
        startConnectionCheck();
        saveCredentials();

        QSettings settings("Team-APE-RIP", "APE-HOI4-Tool-Studio");
        settings.setValue("Auth/LastUserId", m_userId);
        Logger::instance().logInfo(
            "AuthManager",
            QString("Login successful. Channel: %1, display key: %2, description key: %3")
                .arg(m_channel, m_channelDisplayNameKey, m_channelDescriptionKey)
        );

        m_heartbeatTimer->stop();
        emit loginSuccess();
    } else {
        if (authResult.accountAction.blocking) {
            setAccountActionInfo(authResult.accountAction, false);
        } else {
            clearAccountActionInfo(false);
        }

        const QString errorMsg = localizeAuthResultMessage(authResult);
        Logger::instance().logError(
            "AuthManager",
            QString("Login rejected. Code: %1, key: %2, message: %3")
                .arg(authResult.code, authResult.messageKey, errorMsg)
        );
        m_isAuthenticated = false;
        emit loginFailed(errorMsg);
    }
}

void AuthManager::sendHeartbeat() {
    if (!m_isAuthenticated || m_token.isEmpty()) {
        return;
    }

    QJsonObject json;
    json["hwid"] = m_hwid;

    HttpRequestOptions options = HttpClient::createJsonPost(
        QUrl(buildApiUrl("/api/v1/auth/heartbeat")),
        QJsonDocument(json).toJson(QJsonDocument::Compact)
    );
    applyCommonApiHeaders(options);
    HttpClient::addOrReplaceHeader(options, "Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    options.category = HttpRequestCategory::Auth;
    options.timeoutMs = 15000;
    options.connectTimeoutMs = 5000;
    options.maxRetries = 1;
    options.retryOnHttp5xx = true;

    HttpClient::instance().send(options, this, [this](const HttpResponse& response) {
        onHeartbeatReply(response);
    });
}

void AuthManager::onHeartbeatReply(const HttpResponse& response) {
    if (!response.success) {
        logNetworkFailureDetails("Heartbeat request", response);

        const QJsonDocument errorDoc = QJsonDocument::fromJson(response.body);
        if (errorDoc.isObject()) {
            const AccountActionInfo accountActionInfo = parseAccountActionInfo(errorDoc.object());
            if (accountActionInfo.blocking) {
                setAccountActionInfo(accountActionInfo);
                return;
            }
        }

        if (response.statusCode == 401) {
            m_isAuthenticated = false;
            m_heartbeatTimer->stop();
            stopConnectionCheck();
            m_isConnected = false;
            m_hasSuccessfulConnectionCheck = false;
            emit authExpired();
        }
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(response.body);
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        const AccountActionInfo accountActionInfo = parseAccountActionInfo(obj);
        if (accountActionInfo.active) {
            setAccountActionInfo(accountActionInfo);
        } else {
            clearAccountActionInfo();
        }

        if (obj.contains("ad") && obj["ad"].isObject()) {
            const QJsonObject adObj = obj["ad"].toObject();
            const QString text = adObj["text"].toString();
            const QString imageUrl = adObj["image_url"].toString();
            const QString targetUrl = adObj["target_url"].toString();

            Logger::instance().logInfo("AuthManager", "Received timed advertisement from heartbeat");
            emit adReceived(text, imageUrl, targetUrl);
        }
    }
}

bool AuthManager::isAuthenticated() const {
    return m_isAuthenticated;
}

bool AuthManager::isLoggingIn() const {
    return m_isLoggingIn;
}

bool AuthManager::isConnected() const {
    return m_isConnected;
}

QString AuthManager::getToken() const {
    return m_token;
}

AuthResult AuthManager::parseAuthResult(const QJsonObject& obj) const {
    AuthResult result;
    result.success = obj["status"].toString() == "success";
    result.code = obj["code"].toString();
    result.message = obj["message"].toString();
    result.messageKey = obj["message_key"].toString();
    result.accountAction = parseAccountActionInfo(obj);
    return result;
}

AccountActionInfo AuthManager::parseAccountActionInfo(const QJsonObject& obj) const {
    AccountActionInfo info;

    QJsonObject actionObject;
    if (obj.contains("account_action") && obj["account_action"].isObject()) {
        actionObject = obj["account_action"].toObject();
    } else {
        actionObject = obj;
    }

    info.type = actionObject["type"].toString().trimmed();
    info.code = actionObject["code"].toString().trimmed();
    info.reason = actionObject["reason"].toString().trimmed();
    info.messageKey = actionObject["message_key"].toString().trimmed();
    info.untilAt = actionObject["until_at"].toString().trimmed();
    info.permanent = actionObject["is_permanent"].toBool(false);
    info.active = actionObject["is_active"].toBool(false);
    info.blocking = actionObject["is_blocking"].toBool(false);
    info.remainingSeconds = actionObject["remaining_seconds"].toVariant().toLongLong();

    if (!info.code.isEmpty() && !info.type.isEmpty()) {
        info.active = true;
    }

    if (info.active && !info.blocking && !info.type.isEmpty() && info.type != "none") {
        info.blocking = true;
    }

    return info;
}

QString AuthManager::resolveServerMessage(const QJsonObject& obj) const {
    return localizeAuthResultMessage(parseAuthResult(obj));
}

QString AuthManager::localizeAuthResultMessage(const AuthResult& result) const {
    LocalizationManager& loc = LocalizationManager::instance();

    if (result.accountAction.blocking) {
        return localizeAccountActionMessage(result.accountAction, "LoginDialog");
    }

    if (!result.messageKey.isEmpty()) {
        const QString localized = loc.getString("LoginDialog", result.messageKey);
        if (!localized.isEmpty() && localized != result.messageKey) {
            return localized;
        }
    }

    if (!result.message.isEmpty()) {
        return result.message;
    }

    return loc.getString("LoginDialog", "UnknownErrorFallback");
}

QString AuthManager::localizeAccountActionMessage(const AccountActionInfo& info, const QString& tableName) const {
    LocalizationManager& loc = LocalizationManager::instance();

    QString statusKey;
    if (info.type == "restricted") statusKey = "AccountActionRestricted";
    else if (info.type == "paused") statusKey = "AccountActionPaused";
    else if (info.type == "banned") statusKey = "AccountActionBanned";
    else if (info.type == "terminated") statusKey = "AccountActionTerminated";
    else statusKey = "AccountActionUnknown";

    const QString statusText = loc.getString(tableName, statusKey);
    const QString reasonPrefix = loc.getString(tableName, "AccountActionReasonPrefix");
    const QString noReasonText = loc.getString(tableName, "AccountActionNoReason");
    const QString permanentText = loc.getString(tableName, "AccountActionPermanent");
    const QString remainingPrefix = loc.getString(tableName, "AccountActionReleasePrefix");
    const QString pendingRefreshText = loc.getString(tableName, "AccountActionPendingRefresh");

    QStringList lines;
    lines << statusText;

    if (!info.reason.isEmpty()) {
        lines << reasonPrefix.arg(info.reason);
    } else {
        lines << reasonPrefix.arg(noReasonText);
    }

    if (info.permanent || info.type == "terminated") {
        lines << remainingPrefix.arg(permanentText);
    } else if (info.remainingSeconds > 0) {
        lines << remainingPrefix.arg(formatRemainingTime(info.remainingSeconds));
    } else {
        lines << remainingPrefix.arg(pendingRefreshText);
    }

    return lines.join('\n');
}

QString AuthManager::getChannel() const {
    return m_channel;
}

QString AuthManager::getChannelDisplayNameKey() const {
    return m_channelDisplayNameKey;
}

QString AuthManager::getChannelDescriptionKey() const {
    return m_channelDescriptionKey;
}

QString AuthManager::getHWID() const {
    return m_hwid;
}

QString AuthManager::getCurrentUsername() const {
    return m_username;
}

bool AuthManager::hasBlockingAccountAction() const {
    return m_accountActionInfo.blocking;
}

AccountActionInfo AuthManager::getAccountActionInfo() const {
    return m_accountActionInfo;
}

void AuthManager::logout() {
    m_isAuthenticated = false;
    m_token.clear();
    m_username.clear();
    m_password.clear();
    m_channel = "stable";
    m_channelDisplayNameKey.clear();
    m_channelDescriptionKey.clear();
    m_userId.clear();
    clearAccountActionInfo(false);
    m_heartbeatTimer->stop();
    stopConnectionCheck();
    m_isConnected = false;
    m_connectionWarningActive = false;
    m_hasSuccessfulConnectionCheck = false;

    QSettings settings("Team-APE-RIP", "APE-HOI4-Tool-Studio");
    settings.remove("Auth/Username");
    settings.remove("Auth/Password");
    settings.remove("Auth/LastUserId");

    Logger::instance().logInfo("AuthManager", "User logged out");
}

void AuthManager::saveCredentials() {
    QSettings settings("Team-APE-RIP", "APE-HOI4-Tool-Studio");
    settings.setValue("Auth/Username", m_username);
    settings.setValue("Auth/Password", m_password);
}

void AuthManager::loadCredentials() {
    QSettings settings("Team-APE-RIP", "APE-HOI4-Tool-Studio");
    m_username = settings.value("Auth/Username", "").toString();
    m_password = settings.value("Auth/Password", "").toString();
}

void AuthManager::startConnectionCheck() {
    m_hasSuccessfulConnectionCheck = false;
    m_connectionLossWindowTimer.restart();

    if (!m_connectionCheckTimer->isActive()) {
        m_connectionCheckTimer->start();
    }
    onConnectionCheckTimer();
}

void AuthManager::stopConnectionCheck() {
    if (m_connectionCheckTimer->isActive()) {
        m_connectionCheckTimer->stop();
    }
    m_hasSuccessfulConnectionCheck = false;
}

void AuthManager::onConnectionCheckTimer() {
    if (!m_isAuthenticated || m_token.isEmpty()) {
        return;
    }

    const qint64 elapsedMs = m_connectionLossWindowTimer.isValid()
        ? m_connectionLossWindowTimer.elapsed()
        : CONNECTION_LOSS_THRESHOLD_MS;

    if (elapsedMs >= CONNECTION_LOSS_THRESHOLD_MS && !m_connectionWarningActive) {
        Logger::instance().logInfo(
            "AuthManager",
            QString("Connection lost detected after %1 ms without a successful verify, emitting connectionLost()")
                .arg(elapsedMs));
        emit connectionLost();
        m_connectionWarningActive = true;
        m_isConnected = false;
    }

    sendPingChallenge();
}

void AuthManager::sendPingChallenge() {
    QUrl challengeUrl(buildApiUrl("/api/v1/ping/challenge"));
    challengeUrl.setQuery(QString("t=%1&n=%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));

    HttpRequestOptions options = HttpClient::createGet(challengeUrl);
    applyCommonApiHeaders(options);
    applyStrictNoCacheHeaders(options);
    HttpClient::addOrReplaceHeader(options, "Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    options.category = HttpRequestCategory::Ping;
    options.timeoutMs = 12000;
    options.connectTimeoutMs = 3000;
    options.maxRetries = 2;
    options.retryOnHttp5xx = true;
    options.retryOnTimeout = true;
    options.retryBackoffMs = 300;
    options.httpVersionPolicy = HttpVersionPolicy::ForceHttp11;
    options.ipResolvePolicy = HttpIpResolvePolicy::PreferIpv4;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;

    HttpClient::instance().send(options, this, [this](const HttpResponse& response) {
        if (!response.success) {
            logNetworkFailureDetails("Ping challenge request", response);
            Logger::instance().logError(
                "AuthManager",
                QString("Ping challenge failed: network_error_%1")
                    .arg(response.statusCode > 0 ? response.statusCode : 0));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(response.body);
        if (!doc.isObject()) {
            Logger::instance().logError("AuthManager", "Ping challenge failed: invalid_json");
            return;
        }

        const QJsonObject obj = doc.object();
        const QString nonce = obj["nonce"].toString();
        const QString timestamp = obj["timestamp"].toString();
        const QString serverSig = obj["server_sig"].toString();

        if (nonce.isEmpty() || timestamp.isEmpty() || serverSig.isEmpty()) {
            Logger::instance().logError("AuthManager", "Ping challenge failed: missing_fields");
            return;
        }

        const QString expectedSig = computeHmacHex(kPingServerSecret, (nonce + timestamp).toUtf8());
        if (serverSig.compare(expectedSig, Qt::CaseInsensitive) != 0) {
            Logger::instance().logError("AuthManager", "Ping challenge failed: invalid_server_sig");
            return;
        }

        sendPingVerify(nonce, timestamp, serverSig);
    });
}

void AuthManager::onChallengeReply(const HttpResponse& response) {
    Q_UNUSED(response);
}

void AuthManager::sendPingVerify(const QString& nonce, const QString& timestamp, const QString& serverSig) {
    Q_UNUSED(serverSig);

    if (m_token.isEmpty() || m_userId.isEmpty()) {
        return;
    }

    const QByteArray sharedSecret = getDeobfuscatedSharedSecret();
    const QString clientResponse = computeHmacHex(sharedSecret, (nonce + timestamp + m_userId).toUtf8());

    QJsonObject json;
    json["nonce"] = nonce;
    json["timestamp"] = timestamp;
    json["client_response"] = clientResponse;

    HttpRequestOptions options = HttpClient::createJsonPost(
        QUrl(buildApiUrl("/api/v1/ping/verify")),
        QJsonDocument(json).toJson(QJsonDocument::Compact)
    );
    applyCommonApiHeaders(options);
    applyStrictNoCacheHeaders(options);
    HttpClient::addOrReplaceHeader(options, "Authorization", QString("Bearer %1").arg(m_token).toUtf8());
    options.category = HttpRequestCategory::Ping;
    options.timeoutMs = 12000;
    options.connectTimeoutMs = 3000;
    options.maxRetries = 2;
    options.retryOnHttp5xx = true;
    options.retryOnTimeout = true;
    options.retryBackoffMs = 300;
    options.httpVersionPolicy = HttpVersionPolicy::ForceHttp11;
    options.ipResolvePolicy = HttpIpResolvePolicy::PreferIpv4;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;

    HttpClient::instance().send(options, this, [this](const HttpResponse& response) {
        if (!response.success) {
            logNetworkFailureDetails("Ping verify request", response);
            Logger::instance().logError(
                "AuthManager",
                QString("Ping verify failed: network_error_%1")
                    .arg(response.statusCode > 0 ? response.statusCode : 0));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(response.body);
        if (!doc.isObject()) {
            Logger::instance().logError("AuthManager", "Ping verify failed: invalid_json");
            return;
        }

        const QJsonObject obj = doc.object();
        if (!obj.contains("verified") || !obj["verified"].isBool()) {
            Logger::instance().logError("AuthManager", "Ping verify failed: missing_verified");
            return;
        }

        if (!obj["verified"].toBool()) {
            const QString reason = obj["reason"].toString();
            if (reason == "account_action_blocked") {
                const AccountActionInfo accountActionInfo = parseAccountActionInfo(obj);
                if (accountActionInfo.blocking) {
                    setAccountActionInfo(accountActionInfo);
                    m_hasSuccessfulConnectionCheck = true;
                    m_connectionLossWindowTimer.restart();

                    if (!m_isConnected || m_connectionWarningActive) {
                        m_isConnected = true;
                        if (m_connectionWarningActive) {
                            m_connectionWarningActive = false;
                            emit connectionRestored();
                        }
                    }
                    return;
                }
            }

            Logger::instance().logError(
                "AuthManager",
                "Ping verify failed: " + (reason.isEmpty() ? QString("verify_failed") : reason));
            return;
        }

        const AccountActionInfo accountActionInfo = parseAccountActionInfo(obj);
        if (accountActionInfo.active) {
            setAccountActionInfo(accountActionInfo);
        } else {
            clearAccountActionInfo();
        }

        m_hasSuccessfulConnectionCheck = true;
        m_connectionLossWindowTimer.restart();

        if (!m_isConnected || m_connectionWarningActive) {
            Logger::instance().logInfo("AuthManager", "Connection restored");
            m_isConnected = true;
            if (m_connectionWarningActive) {
                m_connectionWarningActive = false;
            }
            emit connectionRestored();
        }
    });
}

void AuthManager::onVerifyReply(const HttpResponse& response) {
    Q_UNUSED(response);
}

void AuthManager::clearAccountActionInfo(bool emitSignal) {
    const bool hadBlockingState = m_accountActionInfo.blocking;
    m_accountActionInfo = AccountActionInfo();

    if (emitSignal && hadBlockingState) {
        emit accountActionCleared();
    }
}

void AuthManager::setAccountActionInfo(const AccountActionInfo& info, bool emitSignal) {
    const bool hadBlockingState = m_accountActionInfo.blocking;
    const bool stateChanged =
        m_accountActionInfo.type != info.type
        || m_accountActionInfo.reason != info.reason
        || m_accountActionInfo.remainingSeconds != info.remainingSeconds
        || m_accountActionInfo.untilAt != info.untilAt
        || m_accountActionInfo.permanent != info.permanent
        || m_accountActionInfo.blocking != info.blocking
        || m_accountActionInfo.active != info.active;

    m_accountActionInfo = info;

    if (emitSignal && info.blocking && (!hadBlockingState || stateChanged)) {
        emit accountActionBlocked();
    } else if (emitSignal && hadBlockingState && !info.blocking) {
        emit accountActionCleared();
    }
}

QString AuthManager::formatRemainingTime(qint64 remainingSeconds) const {
    LocalizationManager& loc = LocalizationManager::instance();
    const qint64 normalizedSeconds = qMax<qint64>(0, remainingSeconds);
    const qint64 days = normalizedSeconds / 86400;
    const qint64 hours = (normalizedSeconds % 86400) / 3600;
    const qint64 minutes = (normalizedSeconds % 3600) / 60;
    const qint64 seconds = normalizedSeconds % 60;

    QStringList parts;
    if (days > 0) {
        parts << loc.getString("LoginDialog", "DurationDaysUnit").arg(days);
    }
    if (hours > 0) {
        parts << loc.getString("LoginDialog", "DurationHoursUnit").arg(hours);
    }
    if (minutes > 0) {
        parts << loc.getString("LoginDialog", "DurationMinutesUnit").arg(minutes);
    }
    if (seconds > 0 || parts.isEmpty()) {
        parts << loc.getString("LoginDialog", "DurationSecondsUnit").arg(seconds);
    }

    return parts.join(' ');
}

QByteArray AuthManager::getDeobfuscatedSharedSecret() const {
    const unsigned char key = 0x5A;
    static const int part1[] = {104, 109, 109, 109, 98, 56, 57, 62, 62, 107, 57, 62, 106, 99, 111, 57, 63, 111, 63, 106, 109, 98, 104, 57, 106, 108, 99, 106, 104, 110, 105, 105, 106, 57, 56, 56, 63, 56, 108, 109};
    static const int part2[] = {99, 105, 106, 104, 106, 57, 109, 57, 59, 99, 108, 98, 104, 109, 106, 59, 57, 111, 105, 98, 59, 56, 106, 111, 107, 106, 104, 60, 60, 63, 60, 109, 108, 59, 56, 57, 109, 56, 99, 105};
    static const int part3[] = {110, 63, 63, 108, 60, 62, 63, 98, 104, 105, 57, 60, 105, 98, 110, 63, 99, 99, 56, 62, 60, 59, 63, 98, 111, 56, 99, 63, 63, 109, 106, 105, 104, 111, 104, 63, 59, 107, 105, 98, 106, 56, 57, 107, 105, 60, 106, 105};

    QByteArray secret;
    secret.reserve(128);

    auto appendPart = [&secret, key](const int* data, int length) {
        for (int i = 0; i < length; ++i) {
            secret.append(static_cast<char>(data[i] ^ key));
        }
    };

    appendPart(part1, static_cast<int>(sizeof(part1) / sizeof(int)));
    appendPart(part2, static_cast<int>(sizeof(part2) / sizeof(int)));
    appendPart(part3, static_cast<int>(sizeof(part3) / sizeof(int)));

    return secret;
}