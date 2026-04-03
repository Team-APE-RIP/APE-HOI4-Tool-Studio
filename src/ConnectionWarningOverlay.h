//-------------------------------------------------------------------------------------
// ConnectionWarningOverlay.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef CONNECTIONWARNINGOVERLAY_H
#define CONNECTIONWARNINGOVERLAY_H

#include <QObject>
#include <QEvent>
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPoint>

#include "AuthManager.h"

class ConnectionWarningOverlay : public QWidget {
    Q_OBJECT

public:
    explicit ConnectionWarningOverlay(QWidget *parent = nullptr);
    ~ConnectionWarningOverlay();

    void updateTheme();
    void updateTexts();

public slots:
    void showWarning();
    void showAccountActionWarning(const AccountActionInfo& info);
    void hideWarning();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onCountdownTick();
    void onSpinnerTick();

private:
    enum class OverlayMode {
        Countdown,
        Reconnecting
    };

    enum class WarningScenario {
        Network,
        Restricted,
        Paused,
        Banned,
        Terminated
    };

    void updatePosition();
    void updateCountdownText();
    void updateReconnectTexts();
    void updateReconnectCountdownText();
    void enterReconnectingMode();
    QString formatRemainingTime(qint64 remainingSeconds) const;
    QString buildAccountActionReasonText() const;
    QString buildAccountActionReleaseText() const;
    QString buildAccountActionStatusText() const;
    WarningScenario resolveScenarioFromAccountAction(const AccountActionInfo& info) const;
    void applyInteractionPolicy();
    void updateMarqueeState();
    void resetMarqueeAnimation();
    bool isBlockingMode() const;
    bool shouldAllowPassThrough(const QPoint &pos) const;
    bool forwardMouseEventToButton(QMouseEvent *event);
    bool tryStartWindowDrag(QMouseEvent *event);

    QWidget *m_hostWidget;
    QWidget *m_warningBar;
    QWidget *m_reconnectingPanel;
    QLabel *m_titleLabel;
    QLabel *m_messageLabel;
    QLabel *m_countdownLabel;
    QLabel *m_reconnectTitleLabel;
    QTimer *m_countdownTimer;
    QTimer *m_spinnerTimer;
    int m_remainingSeconds;
    qint64 m_actionRemainingSeconds;
    int m_spinnerAngle;
    OverlayMode m_mode;
    WarningScenario m_scenario;
    AccountActionInfo m_accountActionInfo;
    QString m_reconnectRemainingText;
    QString m_reconnectStatusText;
    QString m_reconnectReasonText;
    qreal m_reconnectRemainingOffset;
    qreal m_reconnectStatusOffset;
    qreal m_reconnectReasonOffset;
    qreal m_reconnectRemainingTextWidth;
    qreal m_reconnectStatusTextWidth;
    qreal m_reconnectReasonTextWidth;
    qint64 m_marqueeTickCount;
};

#endif // CONNECTIONWARNINGOVERLAY_H