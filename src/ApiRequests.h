//-------------------------------------------------------------------------------------
// ApiRequests.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef APIREQUESTS_H
#define APIREQUESTS_H

#include "HttpClient.h"

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace ApiRequests {

QString apiBaseHost();
QString apiBaseUrl(bool useHttps = true);
QString buildApiUrl(const QString& path, bool useHttps = true);

QString authPathPrefix();
QString redactedAuthEndpoint();
QString redactedApiBase();

QUrl caBundleMetadataUrl();
QUrl caBundleDownloadUrl();
QUrl advertisementImageUrl(const QString& imageUrl);
QString updateDownloadUrl(const QString& candidate);

void applyCommonHeaders(HttpRequestOptions& options);
void applyStrictNoCacheHeaders(HttpRequestOptions& options);
void applyBearerAuthorization(HttpRequestOptions& options, const QString& token);

HttpRequestOptions createLoginRequest(const QByteArray& requestBody);
HttpRequestOptions createRegisterRequest(const QByteArray& requestBody);
HttpRequestOptions createHeartbeatRequest(const QByteArray& requestBody);
HttpRequestOptions createPingChallengeRequest(qint64 timestampMs, const QString& nonce);
HttpRequestOptions createPingVerifyRequest(const QByteArray& requestBody);
HttpRequestOptions createAgreementEventsRequest(const QByteArray& requestBody);
HttpRequestOptions createCurrentAdvertisementRequest();
HttpRequestOptions createUpdateManifestRequest(const QString& channel, const QString& currentVersion, const QString& language);
HttpRequestOptions createUpdateDownloadRequest(const QString& resolvedUrl);

}

#endif // APIREQUESTS_H
