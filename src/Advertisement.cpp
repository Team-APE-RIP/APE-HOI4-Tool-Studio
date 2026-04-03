//-------------------------------------------------------------------------------------
// Advertisement.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "Advertisement.h"

#include "AuthManager.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "Logger.h"

#include <QDesktopServices>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QUrl>

Advertisement::Advertisement(QWidget* parent)
    : QWidget(parent)
    , m_countdownSeconds(5) {
    setAttribute(Qt::WA_TranslucentBackground);
    hide();

    m_container = new QWidget(this);
    m_container->setObjectName("adContainer");

    QVBoxLayout* layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    m_titleLabel = new QLabel(m_container);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName("adTitle");

    m_imageLabel = new QLabel(m_container);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setCursor(Qt::PointingHandCursor);
    m_imageLabel->setScaledContents(true);
    m_imageLabel->setMinimumSize(400, 300);
    m_imageLabel->installEventFilter(this);

    m_closeButton = new QPushButton(m_container);
    m_closeButton->setObjectName("adCloseButton");
    m_closeButton->setMinimumHeight(40);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    connect(m_closeButton, &QPushButton::clicked, this, &Advertisement::hideAd);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_imageLabel, 1);
    layout->addWidget(m_closeButton);

    m_countdownTimer = new QTimer(this);
    connect(m_countdownTimer, &QTimer::timeout, this, &Advertisement::updateCountdown);

    updateTheme();
}

Advertisement::~Advertisement() {
}

void Advertisement::showAd() {
    Logger::instance().logInfo("Advertisement", "showAd() called");

    if (m_hasPreparedAd && !m_preparedPixmap.isNull()) {
        Logger::instance().logInfo("Advertisement", "Using prepared advertisement cache for display");
        applyPreparedAdAndShow();
        return;
    }

    m_displayWhenPrepared = true;
    fetchAdData(true);
}

void Advertisement::preloadAd() {
    if (m_isPreparingAd || m_hasPreparedAd) {
        return;
    }

    Logger::instance().logInfo("Advertisement", "preloadAd() called, preparing advertisement resources");
    m_displayWhenPrepared = false;
    fetchAdData(false);
}

void Advertisement::showAdWithData(const QString& text, const QString& imageUrl, const QString& targetUrl) {
    Logger::instance().logInfo("Advertisement", "showAdWithData() called");

    m_currentText = text;
    m_currentUrl = targetUrl;

    if (imageUrl.isEmpty()) {
        Logger::instance().logError("Advertisement", "Ad image URL is empty");
        emit adFetchFailed();
        return;
    }

    QString fullImageUrl = imageUrl;
    if (imageUrl.startsWith("/ads/")) {
        fullImageUrl = AuthManager::getApiBaseUrl() + imageUrl;
    }

    HttpRequestOptions options = HttpClient::createGet(QUrl(fullImageUrl));
    options.category = HttpRequestCategory::AdvertisementImage;
    options.timeoutMs = 90000;
    options.connectTimeoutMs = 8000;
    options.lowSpeedLimitBytesPerSecond = 512;
    options.lowSpeedTimeSeconds = 20;
    options.maxRetries = 2;
    options.retryOnHttp5xx = true;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;
    options.backendPreference = HttpBackendPreference::Libcurl;

    HttpClient::instance().send(options, this, [this, imageUrl, text, targetUrl](const HttpResponse& response) {
        if (!response.success) {
            Logger::instance().logError("Advertisement", "Failed to fetch ad image: " + response.errorMessage);
            m_isPreparingAd = false;
            if (m_displayWhenPrepared) {
                emit adFetchFailed();
            }
            return;
        }

        QPixmap pixmap;
        if (!pixmap.loadFromData(response.body)) {
            Logger::instance().logError("Advertisement", "Failed to decode ad image data");
            m_isPreparingAd = false;
            if (m_displayWhenPrepared) {
                emit adFetchFailed();
            }
            return;
        }

        m_preparedText = text;
        m_preparedUrl = targetUrl;
        m_preparedImageUrl = imageUrl;
        m_preparedPixmap = pixmap;
        m_hasPreparedAd = true;
        m_isPreparingAd = false;

        if (m_displayWhenPrepared) {
            applyPreparedAdAndShow();
        }
    });
}

void Advertisement::fetchAdData(bool forDisplay) {
    if (m_isPreparingAd) {
        if (forDisplay) {
            m_displayWhenPrepared = true;
        }
        return;
    }

    m_isPreparingAd = true;
    if (forDisplay) {
        m_displayWhenPrepared = true;
    }

    HttpRequestOptions options = HttpClient::createGet(QUrl(AuthManager::getApiBaseUrl() + "/api/v1/ads/current"));
    options.category = HttpRequestCategory::Manifest;
    options.timeoutMs = 20000;
    options.connectTimeoutMs = 10000;
    options.maxRetries = 2;
    options.retryOnHttp5xx = true;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;
    options.backendPreference = HttpBackendPreference::Libcurl;

    const QString token = AuthManager::instance().getToken();
    if (!token.isEmpty()) {
        HttpClient::addOrReplaceHeader(options, "Authorization", QString("Bearer %1").arg(token).toUtf8());
    }

    HttpClient::instance().send(options, this, [this](const HttpResponse& response) {
        onAdDataReceived(response);
    });
}

void Advertisement::onAdDataReceived(const HttpResponse& response) {
    if (!response.success) {
        Logger::instance().logError("Advertisement", "Failed to fetch ad data: " + response.errorMessage);
        m_isPreparingAd = false;
        if (m_displayWhenPrepared) {
            emit adFetchFailed();
        }
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(response.body, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        Logger::instance().logError("Advertisement", "Invalid ad JSON response");
        m_isPreparingAd = false;
        if (m_displayWhenPrepared) {
            emit adFetchFailed();
        }
        return;
    }

    const QJsonObject adObj = doc.object();
    if (!adObj["is_active"].toBool()) {
        Logger::instance().logInfo("Advertisement", "No active advertisement currently.");
        m_isPreparingAd = false;
        if (m_displayWhenPrepared) {
            emit adFetchFailed();
        }
        return;
    }

    showAdWithData(adObj["text"].toString(), adObj["image_url"].toString(), adObj["target_url"].toString());
}

void Advertisement::onAdImageReceived(const HttpResponse& response) {
    Q_UNUSED(response);
}

void Advertisement::applyPreparedAdAndShow() {
    if (!m_hasPreparedAd || m_preparedPixmap.isNull()) {
        emit adFetchFailed();
        return;
    }

    m_currentText = m_preparedText;
    m_currentUrl = m_preparedUrl;
    m_imageLabel->setPixmap(m_preparedPixmap);

    QString titleFormat = LocalizationManager::instance().getString("Advertisement", "Advertisement.Title");
    if (titleFormat.isEmpty() || titleFormat == "Advertisement.Title") {
        titleFormat = "Advertisement - %1";
    }
    m_titleLabel->setText(titleFormat.arg(m_currentText));

    m_countdownSeconds = 5;
    m_closeButton->setText(QString::number(m_countdownSeconds));
    m_closeButton->setEnabled(false);

    if (parentWidget()) {
        resize(parentWidget()->size());
    }

    updatePosition();
    show();
    raise();

    m_displayWhenPrepared = false;
    m_countdownTimer->start(1000);
}

void Advertisement::resetPreparedAdState() {
    m_preparedText.clear();
    m_preparedUrl.clear();
    m_preparedImageUrl.clear();
    m_preparedPixmap = QPixmap();
    m_hasPreparedAd = false;
}

void Advertisement::hideAd() {
    m_countdownTimer->stop();
    emit adClosed();
    hide();

    resetPreparedAdState();
    preloadAd();
}

void Advertisement::updateCountdown() {
    m_countdownSeconds--;
    if (m_countdownSeconds > 0) {
        m_closeButton->setText(QString::number(m_countdownSeconds));
    } else {
        m_countdownTimer->stop();
        m_closeButton->setText(LocalizationManager::instance().getString("Common", "Close"));
        m_closeButton->setEnabled(true);
    }
}

void Advertisement::onImageClicked() {
    if (!m_currentUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_currentUrl));
    }
}

void Advertisement::updateTheme() {
    const bool isDark = ConfigManager::instance().isCurrentThemeDark();

    const QString containerBg = isDark ? "#2d2d2d" : "#ffffff";
    const QString textColor = isDark ? "#ffffff" : "#333333";
    const QString borderColor = isDark ? "#3d3d3d" : "#e0e0e0";
    const QString btnBg = isDark ? "#007acc" : "#0078d7";
    const QString btnBgHover = isDark ? "#0098ff" : "#1084d8";
    const QString btnDisabledBg = isDark ? "#555555" : "#cccccc";

    setStyleSheet(QString(
        "#adContainer {"
        "   background-color: %1;"
        "   border: 1px solid %2;"
        "   border-radius: 10px;"
        "}"
        "#adTitle {"
        "   color: %3;"
        "   font-size: 18px;"
        "   font-weight: bold;"
        "}"
        "#adCloseButton {"
        "   background-color: %4;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 5px;"
        "   font-size: 14px;"
        "   font-weight: bold;"
        "}"
        "#adCloseButton:hover:!disabled {"
        "   background-color: %5;"
        "}"
        "#adCloseButton:disabled {"
        "   background-color: %6;"
        "   color: #888888;"
        "}"
    ).arg(containerBg, borderColor, textColor, btnBg, btnBgHover, btnDisabledBg));
}

void Advertisement::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    const QRectF r = rect();
    const qreal radius = 9;

    path.addRoundedRect(r, radius, radius);
    painter.fillPath(path, QColor(0, 0, 0, 120));
}

bool Advertisement::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_imageLabel && event->type() == QEvent::MouseButtonRelease) {
        onImageClicked();
        return true;
    }

    if (event->type() == QEvent::Resize && parentWidget()) {
        resize(parentWidget()->size());
        updatePosition();
    }

    return QWidget::eventFilter(obj, event);
}

void Advertisement::updatePosition() {
    if (m_container) {
        const int x = (width() - m_container->width()) / 2;
        const int y = (height() - m_container->height()) / 2;
        m_container->move(x, y);
    }
}