//-------------------------------------------------------------------------------------
// AgreementEvidenceManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef AGREEMENTEVIDENCEMANAGER_H
#define AGREEMENTEVIDENCEMANAGER_H

#include <QObject>
#include <QJsonObject>
#include <QString>

class AgreementEvidenceManager : public QObject {
    Q_OBJECT

public:
    static AgreementEvidenceManager& instance();

    void recordAgreementShown(const QString& agreementVersion, bool isSettingsMode);
    void recordScrolledToBottom(const QString& agreementVersion, bool isSettingsMode);
    void recordCountdownStarted(const QString& agreementVersion, int secondsRemaining);
    void recordCountdownCompleted(const QString& agreementVersion);
    void recordAccepted(const QString& agreementVersion, bool isSettingsMode);
    void recordRejected(const QString& agreementVersion, bool isSettingsMode);
    void recordClosedFromSettings(const QString& agreementVersion);

    QString acceptedAgreementVersion() const;
    void storeAcceptedAgreementVersion(const QString& agreementVersion, bool migratedFromLegacy = false);

    void flushPendingEvents(QObject* context = nullptr);

private:
    explicit AgreementEvidenceManager(QObject* parent = nullptr);
    ~AgreementEvidenceManager() override = default;

    AgreementEvidenceManager(const AgreementEvidenceManager&) = delete;
    AgreementEvidenceManager& operator=(const AgreementEvidenceManager&) = delete;

    QString buildAgreementHash(const QString& agreementVersion) const;
    QString buildEventHash(const QJsonObject& eventObject, const QString& previousHash) const;
    QString getCurrentUsername() const;
    QString getCurrentUserId() const;
    QString getCurrentHwid() const;
    QString getClientVersion() const;

    QJsonObject buildBaseEvent(const QString& eventType,
                               const QString& agreementVersion,
                               bool isSettingsMode) const;
    void appendEvent(QJsonObject eventObject);
    void updateAcceptanceState(const QString& agreementVersion, const QString& decision);
    void updateLastViewState(const QString& agreementVersion, bool isSettingsMode);

    QJsonObject loadStateSnapshot() const;
    void saveStateSnapshot(const QJsonObject& stateObject) const;

    QString loadLastEventHash() const;
    void saveLastEventHash(const QString& hashValue) const;

    QString loadPendingEventsJson() const;
    void savePendingEventsJson(const QString& jsonText) const;

    void migrateLegacyAgreementAcceptance();

    void postEventsBatch(const QByteArray& requestBody, QObject* context);
};

#endif // AGREEMENTEVIDENCEMANAGER_H