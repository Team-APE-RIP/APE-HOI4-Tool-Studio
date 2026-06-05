//-------------------------------------------------------------------------------------
// ApiRequests.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ApiRequests.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace {

constexpr const char* kAuthPathPrefix = "/api/v1/auth";
constexpr const char* kRedactedAuthEndpoint = "<redacted-auth-endpoint>";
constexpr const char* kRedactedApiBase = "<redacted-api>";

constexpr const char* kLoginPath = "/api/v1/auth/login";
constexpr const char* kRegisterPath = "/api/v1/auth/register";
constexpr const char* kHeartbeatPath = "/api/v1/auth/heartbeat";
constexpr const char* kPingChallengePath = "/api/v1/ping/challenge";
constexpr const char* kPingVerifyPath = "/api/v1/ping/verify";
constexpr const char* kCaBundleMetadataPath = "/api/v1/security/ca-bundle/meta";
constexpr const char* kCaBundleDownloadPath = "/api/v1/security/ca-bundle";
constexpr const char* kCurrentAdvertisementPath = "/api/v1/ads/current";
constexpr const char* kUpdateManifestPath = "/api/v1/update/manifest";
constexpr const char* kAgreementEventsPath = "/api/v1/agreement/events";

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

HttpRequestOptions createCommonJsonPost(const QUrl& url, const QByteArray& requestBody) {
    HttpRequestOptions options = HttpClient::createJsonPost(url, requestBody);
    ApiRequests::applyCommonHeaders(options);
    options.category = HttpRequestCategory::Auth;
    options.timeoutMs = 20000;
    options.connectTimeoutMs = 5000;
    options.maxRetries = 1;
    options.retryOnHttp5xx = true;
    return options;
}

}

namespace ApiRequests {

QString apiBaseHost() {
    return normalizedBaseHostFromResource();
}

QString apiBaseUrl(bool useHttps) {
    const QString baseHost = apiBaseHost();
    if (baseHost.isEmpty()) {
        return QString();
    }

    return QStringLiteral("%1://%2")
        .arg(useHttps ? QStringLiteral("https") : QStringLiteral("http"))
        .arg(baseHost);
}

QString buildApiUrl(const QString& path, bool useHttps) {
    QString normalizedPath = path.trimmed();
    if (normalizedPath.isEmpty()) {
        return apiBaseUrl(useHttps);
    }

    if (!normalizedPath.startsWith('/')) {
        normalizedPath.prepend('/');
    }

    return apiBaseUrl(useHttps) + normalizedPath;
}

QString authPathPrefix() {
    return QString::fromLatin1(kAuthPathPrefix);
}

QString redactedAuthEndpoint() {
    return QString::fromLatin1(kRedactedAuthEndpoint);
}

QString redactedApiBase() {
    return QString::fromLatin1(kRedactedApiBase);
}

QUrl caBundleMetadataUrl() {
    return QUrl(buildApiUrl(QString::fromLatin1(kCaBundleMetadataPath)));
}

QUrl caBundleDownloadUrl() {
    return QUrl(buildApiUrl(QString::fromLatin1(kCaBundleDownloadPath)));
}

QUrl advertisementImageUrl(const QString& imageUrl) {
    if (imageUrl.startsWith(QStringLiteral("/ads/"))) {
        return QUrl(apiBaseUrl() + imageUrl);
    }

    return QUrl(imageUrl);
}

QString updateDownloadUrl(const QString& candidate) {
    if (candidate.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || candidate.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        return candidate;
    }

    return apiBaseUrl() + candidate;
}

void applyCommonHeaders(HttpRequestOptions& options) {
    HttpClient::addOrReplaceHeader(options, "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36 APEHOI4ToolStudio/1.0");
    HttpClient::addOrReplaceHeader(options, "Accept", "application/json, text/plain, */*");
    HttpClient::addOrReplaceHeader(options, "Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
}

void applyStrictNoCacheHeaders(HttpRequestOptions& options) {
    HttpClient::addOrReplaceHeader(options, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    HttpClient::addOrReplaceHeader(options, "Pragma", "no-cache");
    HttpClient::addOrReplaceHeader(options, "Expires", "0");
}

void applyBearerAuthorization(HttpRequestOptions& options, const QString& token) {
    const QString trimmedToken = token.trimmed();
    if (!trimmedToken.isEmpty()) {
        HttpClient::addOrReplaceHeader(options, "Authorization", QString("Bearer %1").arg(trimmedToken).toUtf8());
    }
}

HttpRequestOptions createLoginRequest(const QByteArray& requestBody) {
    return createCommonJsonPost(QUrl(buildApiUrl(QString::fromLatin1(kLoginPath))), requestBody);
}

HttpRequestOptions createRegisterRequest(const QByteArray& requestBody) {
    HttpRequestOptions options = HttpClient::createJsonPost(
        QUrl(buildApiUrl(QString::fromLatin1(kRegisterPath))),
        requestBody
    );
    HttpClient::addOrReplaceHeader(options, "User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) APEHOI4ToolStudio/1.0");
    HttpClient::addOrReplaceHeader(options, "Accept", "application/json, text/plain, */*");
    HttpClient::addOrReplaceHeader(options, "Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    options.category = HttpRequestCategory::Auth;
    options.timeoutMs = 20000;
    options.connectTimeoutMs = 5000;
    options.maxRetries = 1;
    options.retryOnHttp5xx = true;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;
    return options;
}

HttpRequestOptions createHeartbeatRequest(const QByteArray& requestBody) {
    HttpRequestOptions options = HttpClient::createJsonPost(
        QUrl(buildApiUrl(QString::fromLatin1(kHeartbeatPath))),
        requestBody
    );
    applyCommonHeaders(options);
    options.category = HttpRequestCategory::Auth;
    options.timeoutMs = 15000;
    options.connectTimeoutMs = 5000;
    options.maxRetries = 1;
    options.retryOnHttp5xx = true;
    return options;
}

HttpRequestOptions createPingChallengeRequest(qint64 timestampMs, const QString& nonce) {
    QUrl challengeUrl(buildApiUrl(QString::fromLatin1(kPingChallengePath)));
    challengeUrl.setQuery(QStringLiteral("t=%1&n=%2").arg(timestampMs).arg(nonce));

    HttpRequestOptions options = HttpClient::createGet(challengeUrl);
    applyCommonHeaders(options);
    applyStrictNoCacheHeaders(options);
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
    return options;
}

HttpRequestOptions createPingVerifyRequest(const QByteArray& requestBody) {
    HttpRequestOptions options = HttpClient::createJsonPost(
        QUrl(buildApiUrl(QString::fromLatin1(kPingVerifyPath))),
        requestBody
    );
    applyCommonHeaders(options);
    applyStrictNoCacheHeaders(options);
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
    return options;
}

HttpRequestOptions createAgreementEventsRequest(const QByteArray& requestBody) {
    HttpRequestOptions options = HttpClient::createJsonPost(
        QUrl(buildApiUrl(QString::fromLatin1(kAgreementEventsPath))),
        requestBody
    );
    HttpClient::addOrReplaceHeader(options, "X-APE-Agreement-Client", "desktop-registry-evidence");
    options.category = HttpRequestCategory::Auth;
    options.timeoutMs = 15000;
    options.connectTimeoutMs = 5000;
    options.maxRetries = 1;
    options.retryOnHttp5xx = true;
    return options;
}

HttpRequestOptions createCurrentAdvertisementRequest() {
    HttpRequestOptions options = HttpClient::createGet(QUrl(buildApiUrl(QString::fromLatin1(kCurrentAdvertisementPath))));
    options.category = HttpRequestCategory::Manifest;
    options.timeoutMs = 20000;
    options.connectTimeoutMs = 10000;
    options.maxRetries = 2;
    options.retryOnHttp5xx = true;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;
    options.backendPreference = HttpBackendPreference::Libcurl;
    return options;
}

HttpRequestOptions createUpdateManifestRequest(const QString& channel, const QString& currentVersion, const QString& language) {
    HttpRequestOptions options = HttpClient::createGet(QUrl(buildApiUrl(QString::fromLatin1(kUpdateManifestPath))));
    HttpClient::addOrReplaceHeader(options, "User-Agent", "APE-HOI4-Tool-Studio-Updater");
    HttpClient::addOrReplaceHeader(options, "X-Update-Channel", channel.toUtf8());
    HttpClient::addOrReplaceHeader(options, "X-Current-Version", currentVersion.toUtf8());
    HttpClient::addOrReplaceHeader(options, "X-Lang", language.toUtf8());
    options.category = HttpRequestCategory::Manifest;
    options.timeoutMs = 12000;
    options.connectTimeoutMs = 3500;
    options.maxRetries = 2;
    options.retryOnHttp5xx = true;
    options.retryOnTimeout = true;
    options.retryBackoffMs = 350;
    options.httpVersionPolicy = HttpVersionPolicy::PreferHttp2;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;
    return options;
}

HttpRequestOptions createUpdateDownloadRequest(const QString& resolvedUrl) {
    HttpRequestOptions options = HttpClient::createGet(QUrl(resolvedUrl));
    HttpClient::addOrReplaceHeader(options, "User-Agent", "APE-HOI4-Tool-Studio-Updater");
    HttpClient::addOrReplaceHeader(options, "Accept", "*/*");
    HttpClient::addOrReplaceHeader(options, "Cache-Control", "no-cache");
    return options;
}

}
