//-------------------------------------------------------------------------------------
// Advertisement.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef ADVERTISEMENT_H
#define ADVERTISEMENT_H

#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "HttpClient.h"

class Advertisement : public QWidget {
    Q_OBJECT

public:
    explicit Advertisement(QWidget* parent = nullptr);
    ~Advertisement();

    void showAd();
    void preloadAd();
    void showAdWithData(const QString& text, const QString& imageUrl, const QString& targetUrl);
    void hideAd();
    void updateTheme();

signals:
    void adClosed();
    void adFetchFailed();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void updateCountdown();
    void onImageClicked();
    void onAdDataReceived(const HttpResponse& response);
    void onAdImageReceived(const HttpResponse& response);
    void updatePosition();
    void fetchAdData(bool forDisplay);
    void applyPreparedAdAndShow();
    void resetPreparedAdState();

    QWidget* m_container;
    QLabel* m_titleLabel;
    QLabel* m_imageLabel;
    QPushButton* m_closeButton;

    QTimer* m_countdownTimer;
    int m_countdownSeconds;

    QString m_currentUrl;
    QString m_currentText;

    QString m_preparedText;
    QString m_preparedUrl;
    QString m_preparedImageUrl;
    QPixmap m_preparedPixmap;
    bool m_hasPreparedAd = false;
    bool m_isPreparingAd = false;
    bool m_displayWhenPrepared = false;

    QList<QJsonObject> m_adList;
};

#endif // ADVERTISEMENT_H