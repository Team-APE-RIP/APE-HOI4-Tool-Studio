//-------------------------------------------------------------------------------------
// AgreementEvidenceManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "AgreementEvidenceManager.h"

#include "AuthManager.h"
#include "HttpClient.h"
#include "Logger.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>

namespace {
const char* kAgreementRegistryGroup = "AgreementEvidence";
const char* kPendingEventsKey = "AgreementEvidence/PendingEvents";
const char* kLastEventHashKey = "AgreementEvidence/LastEventHash";
const char* kStateSnapshotKey = "AgreementEvidence/StateSnapshot";
const int kAgreementBatchLimit = 50;

QString toCompactJson(const QJsonObject& object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString sanitizeVersionValue(const QString& versionText) {
    const QString trimmed = versionText.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("0.0.0.0") : trimmed;
}

QSettings createAgreementSettings() {
    return QSettings("Team-APE-RIP", "APE-HOI4-Tool-Studio");
}

QString getLegacyUavCheckPath() {
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/APE-HOI4-Tool-Studio/UAVCheck.json";
}
}

AgreementEvidenceManager& AgreementEvidenceManager::instance() {
    static AgreementEvidenceManager instance;
    return instance;
}

AgreementEvidenceManager::AgreementEvidenceManager(QObject* parent)
    : QObject(parent) {
    migrateLegacyAgreementAcceptance();
}

void AgreementEvidenceManager::recordAgreementShown(const QString& agreementVersion, bool isSettingsMode) {
    QJsonObject eventObject = buildBaseEvent("agreement_shown", agreementVersion, isSettingsMode);
    appendEvent(eventObject);
    updateLastViewState(agreementVersion, isSettingsMode);
}

void AgreementEvidenceManager::recordScrolledToBottom(const QString& agreementVersion, bool isSettingsMode) {
    QJsonObject eventObject = buildBaseEvent("scrolled_to_bottom", agreementVersion, isSettingsMode);
    appendEvent(eventObject);
    updateLastViewState(agreementVersion, isSettingsMode);
}

void AgreementEvidenceManager::recordCountdownStarted(const QString& agreementVersion, int secondsRemaining) {
    QJsonObject eventObject = buildBaseEvent("countdown_started", agreementVersion, false);
    eventObject["seconds_remaining"] = secondsRemaining;
    appendEvent(eventObject);
}

void AgreementEvidenceManager::recordCountdownCompleted(const QString& agreementVersion) {
    QJsonObject eventObject = buildBaseEvent("countdown_completed", agreementVersion, false);
    appendEvent(eventObject);
}

void AgreementEvidenceManager::recordAccepted(const QString& agreementVersion, bool isSettingsMode) {
    QJsonObject eventObject = buildBaseEvent("agreement_accepted", agreementVersion, isSettingsMode);
    appendEvent(eventObject);
    updateAcceptanceState(agreementVersion, "accepted");
}

void AgreementEvidenceManager::recordRejected(const QString& agreementVersion, bool isSettingsMode) {
    QJsonObject eventObject = buildBaseEvent("agreement_rejected", agreementVersion, isSettingsMode);
    appendEvent(eventObject);
    updateAcceptanceState(agreementVersion, "rejected");
}

void AgreementEvidenceManager::recordClosedFromSettings(const QString& agreementVersion) {
    QJsonObject eventObject = buildBaseEvent("settings_view_closed", agreementVersion, true);
    appendEvent(eventObject);
    updateLastViewState(agreementVersion, true);
}

void AgreementEvidenceManager::flushPendingEvents(QObject* context) {
    const QString jsonText = loadPendingEventsJson();
    if (jsonText.trimmed().isEmpty()) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(jsonText.toUtf8());
    if (!document.isArray()) {
        Logger::instance().logWarning("AgreementEvidenceManager", "Pending agreement events are invalid and cannot be flushed");
        return;
    }

    const QJsonArray sourceArray = document.array();
    if (sourceArray.isEmpty()) {
        return;
    }

    QJsonArray batchArray;
    const int batchSize = qMin(sourceArray.size(), kAgreementBatchLimit);
    for (int index = 0; index < batchSize; ++index) {
        if (sourceArray.at(index).isObject()) {
            batchArray.append(sourceArray.at(index).toObject());
        }
    }

    if (batchArray.isEmpty()) {
        return;
    }

    QJsonObject payload;
    payload["client_version"] = getClientVersion();
    payload["username"] = getCurrentUsername();
    payload["user_id"] = getCurrentUserId();
    payload["hwid"] = getCurrentHwid();
    payload["events"] = batchArray;

    postEventsBatch(QJsonDocument(payload).toJson(QJsonDocument::Compact), context);
}

QString AgreementEvidenceManager::buildAgreementHash(const QString& agreementVersion) const {
    const QByteArray source = sanitizeVersionValue(agreementVersion).toUtf8();
    return QString(QCryptographicHash::hash(source, QCryptographicHash::Sha256).toHex());
}

QString AgreementEvidenceManager::buildEventHash(const QJsonObject& eventObject, const QString& previousHash) const {
    QJsonObject normalizedObject = eventObject;
    normalizedObject.remove("event_hash");
    const QByteArray payload = previousHash.toUtf8() + "|" + QJsonDocument(normalizedObject).toJson(QJsonDocument::Compact);
    return QString(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

QString AgreementEvidenceManager::getCurrentUsername() const {
    return AuthManager::instance().getCurrentUsername().trimmed();
}

QString AgreementEvidenceManager::getCurrentUserId() const {
    QSettings settings("Team-APE-RIP", "APE-HOI4-Tool-Studio");
    return settings.value("Auth/LastUserId").toString().trimmed();
}

QString AgreementEvidenceManager::getCurrentHwid() const {
    return AuthManager::instance().getHWID().trimmed();
}

QString AgreementEvidenceManager::getClientVersion() const {
#ifdef APP_VERSION
    return QStringLiteral(APP_VERSION);
#else
    return QCoreApplication::applicationVersion().trimmed().isEmpty()
        ? QStringLiteral("unknown")
        : QCoreApplication::applicationVersion().trimmed();
#endif
}

QJsonObject AgreementEvidenceManager::buildBaseEvent(const QString& eventType,
                                                     const QString& agreementVersion,
                                                     bool isSettingsMode) const {
    QJsonObject eventObject;
    eventObject["event_type"] = eventType;
    eventObject["agreement_version"] = sanitizeVersionValue(agreementVersion);
    eventObject["agreement_hash"] = buildAgreementHash(agreementVersion);
    eventObject["is_settings_mode"] = isSettingsMode;
    eventObject["occurred_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    eventObject["client_version"] = getClientVersion();
    eventObject["username"] = getCurrentUsername();
    eventObject["user_id"] = getCurrentUserId();
    eventObject["hwid"] = getCurrentHwid();
    eventObject["device_platform"] = QStringLiteral("windows");
    return eventObject;
}

void AgreementEvidenceManager::appendEvent(QJsonObject eventObject) {
    QJsonDocument document = QJsonDocument::fromJson(loadPendingEventsJson().toUtf8());
    QJsonArray eventArray = document.isArray() ? document.array() : QJsonArray();

    const QString previousHash = loadLastEventHash();
    eventObject["previous_hash"] = previousHash;
    eventObject["event_hash"] = buildEventHash(eventObject, previousHash);

    eventArray.append(eventObject);
    savePendingEventsJson(QString::fromUtf8(QJsonDocument(eventArray).toJson(QJsonDocument::Compact)));
    saveLastEventHash(eventObject["event_hash"].toString());

    Logger::instance().logInfo(
        "AgreementEvidenceManager",
        QString("Agreement event queued: %1 version=%2 settings=%3")
            .arg(eventObject["event_type"].toString(),
                 eventObject["agreement_version"].toString(),
                 eventObject["is_settings_mode"].toBool() ? "true" : "false")
    );
}

void AgreementEvidenceManager::updateAcceptanceState(const QString& agreementVersion, const QString& decision) {
    QJsonObject stateObject = loadStateSnapshot();
    stateObject["last_decision"] = decision;
    stateObject["accepted_version"] = sanitizeVersionValue(agreementVersion);
    stateObject["accepted_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    stateObject["accepted_by_username"] = getCurrentUsername();
    stateObject["accepted_by_user_id"] = getCurrentUserId();
    stateObject["accepted_by_hwid"] = getCurrentHwid();
    saveStateSnapshot(stateObject);
}

void AgreementEvidenceManager::updateLastViewState(const QString& agreementVersion, bool isSettingsMode) {
    QJsonObject stateObject = loadStateSnapshot();
    stateObject["last_viewed_version"] = sanitizeVersionValue(agreementVersion);
    stateObject["last_viewed_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    stateObject["last_viewed_in_settings_mode"] = isSettingsMode;
    stateObject["last_viewed_by_username"] = getCurrentUsername();
    stateObject["last_viewed_by_user_id"] = getCurrentUserId();
    stateObject["last_viewed_by_hwid"] = getCurrentHwid();
    saveStateSnapshot(stateObject);
}

QJsonObject AgreementEvidenceManager::loadStateSnapshot() const {
    QSettings settings = createAgreementSettings();
    const QString jsonText = settings.value(kStateSnapshotKey).toString();
    const QJsonDocument document = QJsonDocument::fromJson(jsonText.toUtf8());
    return document.isObject() ? document.object() : QJsonObject();
}

void AgreementEvidenceManager::saveStateSnapshot(const QJsonObject& stateObject) const {
    QSettings settings = createAgreementSettings();
    settings.setValue(kStateSnapshotKey, QString::fromUtf8(QJsonDocument(stateObject).toJson(QJsonDocument::Compact)));
    settings.sync();
}

QString AgreementEvidenceManager::loadLastEventHash() const {
    QSettings settings = createAgreementSettings();
    return settings.value(kLastEventHashKey).toString().trimmed();
}

void AgreementEvidenceManager::saveLastEventHash(const QString& hashValue) const {
    QSettings settings = createAgreementSettings();
    settings.setValue(kLastEventHashKey, hashValue.trimmed());
    settings.sync();
}

QString AgreementEvidenceManager::loadPendingEventsJson() const {
    QSettings settings = createAgreementSettings();
    const QString jsonText = settings.value(kPendingEventsKey).toString();
    return jsonText.trimmed().isEmpty() ? QStringLiteral("[]") : jsonText;
}

void AgreementEvidenceManager::savePendingEventsJson(const QString& jsonText) const {
    QSettings settings = createAgreementSettings();
    settings.setValue(kPendingEventsKey, jsonText);
    settings.sync();
}

void AgreementEvidenceManager::migrateLegacyAgreementAcceptance() {
    QJsonObject stateObject = loadStateSnapshot();
    if (!stateObject.value("accepted_version").toString().trimmed().isEmpty()) {
        return;
    }

    QFile file(getLegacyUavCheckPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!document.isObject()) {
        return;
    }

    const QString acceptedVersion = document.object().value("UAVCheck").toString().trimmed();
    if (acceptedVersion.isEmpty() || acceptedVersion == "0.0.0.0") {
        return;
    }

    stateObject["accepted_version"] = acceptedVersion;
    stateObject["accepted_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    stateObject["migrated_from_legacy_uavcheck"] = true;
    saveStateSnapshot(stateObject);

    QJsonObject migratedEvent = buildBaseEvent("legacy_acceptance_migrated", acceptedVersion, false);
    migratedEvent["legacy_source"] = "UAVCheck.json";
    appendEvent(migratedEvent);
}

void AgreementEvidenceManager::postEventsBatch(const QByteArray& requestBody, QObject* context) {
    if (AuthManager::instance().getToken().trimmed().isEmpty()) {
        return;
    }

    HttpRequestOptions options = HttpClient::createJsonPost(
        QUrl(AuthManager::buildApiUrl("/api/v1/agreement/events")),
        requestBody
    );
    HttpClient::addOrReplaceHeader(options, "Authorization", QString("Bearer %1").arg(AuthManager::instance().getToken()).toUtf8());
    HttpClient::addOrReplaceHeader(options, "X-APE-Agreement-Client", "desktop-registry-evidence");
    options.category = HttpRequestCategory::Auth;
    options.timeoutMs = 15000;
    options.connectTimeoutMs = 5000;
    options.maxRetries = 1;
    options.retryOnHttp5xx = true;

    HttpClient::instance().send(options, context ? context : this, [this](const HttpResponse& response) {
        if (!response.success) {
            Logger::instance().logWarning(
                "AgreementEvidenceManager",
                QString("Agreement event flush failed: http=%1 reason=%2")
                    .arg(response.statusCode)
                    .arg(response.errorMessage.isEmpty() ? QStringLiteral("<none>") : response.errorMessage)
            );
            return;
        }

        const QJsonDocument responseDocument = QJsonDocument::fromJson(response.body);
        if (!responseDocument.isObject()) {
            Logger::instance().logWarning("AgreementEvidenceManager", "Agreement event flush response is not a JSON object");
            return;
        }

        const QJsonObject responseObject = responseDocument.object();
        const bool success = responseObject.value("success").toBool(false);
        const int acceptedCount = responseObject.value("accepted_count").toInt(0);
        if (!success || acceptedCount <= 0) {
            Logger::instance().logWarning("AgreementEvidenceManager", "Agreement event flush was not accepted by server");
            return;
        }

        QJsonDocument pendingDocument = QJsonDocument::fromJson(loadPendingEventsJson().toUtf8());
        QJsonArray pendingArray = pendingDocument.isArray() ? pendingDocument.array() : QJsonArray();
        QJsonArray remainingArray;
        for (int index = acceptedCount; index < pendingArray.size(); ++index) {
            remainingArray.append(pendingArray.at(index));
        }

        savePendingEventsJson(QString::fromUtf8(QJsonDocument(remainingArray).toJson(QJsonDocument::Compact)));

        Logger::instance().logInfo(
            "AgreementEvidenceManager",
            QString("Agreement events flushed successfully: accepted=%1 remaining=%2")
                .arg(acceptedCount)
                .arg(remainingArray.size())
        );
    });
}