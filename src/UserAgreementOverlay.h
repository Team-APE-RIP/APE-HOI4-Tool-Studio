//-------------------------------------------------------------------------------------
// UserAgreementOverlay.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef USERAGREEMENTOVERLAY_H
#define USERAGREEMENTOVERLAY_H

#include <QWidget>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

class UserAgreementOverlay : public QWidget {
    Q_OBJECT

public:
    explicit UserAgreementOverlay(QWidget *parent = nullptr);
    ~UserAgreementOverlay();

    void checkAgreement();
    void showAgreement(bool isSettingsMode = false);
    void updateTheme();
    void updateTexts();

signals:
    void agreementAccepted();
    void agreementRejected();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onAcceptClicked();
    void onRejectClicked();

private:
    void updatePosition();
    void setupUi();
    void loadAgreementContent();
    void resetAcceptanceRequirements();
    void updateAcceptButtonState();
    void updateStatusText();
    void startAcceptanceCountdown();
    void onTextScrollChanged();
    QString getUAVVersion();
    QString getUAVCheckVersion();
    void saveUAVCheckVersion(const QString& version);

    QWidget *m_container;
    QLabel *m_titleLabel;
    QTextBrowser *m_textBrowser;
    QWidget *m_statusContainer;
    QLabel *m_statusLabel;
    QPushButton *m_acceptBtn;
    QPushButton *m_rejectBtn;
    QWidget *m_buttonContainer;
    QTimer *m_acceptCountdownTimer;

    QString m_currentUAV;
    bool m_isSettingsMode;
    bool m_hasScrolledToBottom;
    bool m_acceptCountdownStarted;
    bool m_acceptCountdownFinished;
    int m_acceptSecondsRemaining;
};

#endif // USERAGREEMENTOVERLAY_H