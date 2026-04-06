//-------------------------------------------------------------------------------------
// UserAgreementOverlay.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "UserAgreementOverlay.h"
#include "AgreementEvidenceManager.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QScrollBar>
#include <QTextCursor>
#include <QStyle>

UserAgreementOverlay::UserAgreementOverlay(QWidget *parent)
    : QWidget(parent),
      m_acceptCountdownTimer(nullptr),
      m_isSettingsMode(false),
      m_hasScrolledToBottom(false),
      m_acceptCountdownStarted(false),
      m_acceptCountdownFinished(false),
      m_acceptSecondsRemaining(5)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setupUi();
    updateTheme();
    updateTexts();
    
    hide();
    
    if (parent) {
        parent->installEventFilter(this);
    }
}

UserAgreementOverlay::~UserAgreementOverlay() {
}

void UserAgreementOverlay::setupUi() {
    m_container = new QWidget(this);
    m_container->setObjectName("UserAgreementContainer");
    m_container->setFixedSize(600, 450);
    
    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(10);
    
    // Title
    m_titleLabel = new QLabel(m_container);
    m_titleLabel->setObjectName("UserAgreementTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);
    
    // Text Browser for Markdown
    m_textBrowser = new QTextBrowser(m_container);
    m_textBrowser->setObjectName("UserAgreementText");
    m_textBrowser->setOpenExternalLinks(true);
    layout->addWidget(m_textBrowser);
    connect(m_textBrowser->verticalScrollBar(), &QScrollBar::valueChanged, this, &UserAgreementOverlay::onTextScrollChanged);

    m_statusContainer = new QWidget(m_container);
    QHBoxLayout *statusLayout = new QHBoxLayout(m_statusContainer);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->addStretch();
    m_statusLabel = new QLabel(m_statusContainer);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(false);
    m_statusLabel->setMinimumWidth(420);
    m_statusLabel->setMaximumWidth(520);
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_statusLabel->setObjectName("UserAgreementStatus");
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    layout->addWidget(m_statusContainer);
    
    // Buttons
    m_buttonContainer = new QWidget(m_container);
    QHBoxLayout *btnLayout = new QHBoxLayout(m_buttonContainer);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(15);
    
    m_rejectBtn = new QPushButton(m_buttonContainer);
    m_rejectBtn->setObjectName("RejectBtn");
    m_rejectBtn->setCursor(Qt::PointingHandCursor);
    m_rejectBtn->setFixedHeight(32);
    connect(m_rejectBtn, &QPushButton::clicked, this, &UserAgreementOverlay::onRejectClicked);
    
    m_acceptBtn = new QPushButton(m_buttonContainer);
    m_acceptBtn->setObjectName("AcceptBtn");
    m_acceptBtn->setCursor(Qt::PointingHandCursor);
    m_acceptBtn->setFixedHeight(32);
    connect(m_acceptBtn, &QPushButton::clicked, this, &UserAgreementOverlay::onAcceptClicked);

    m_acceptCountdownTimer = new QTimer(this);
    m_acceptCountdownTimer->setInterval(1000);
    connect(m_acceptCountdownTimer, &QTimer::timeout, this, [this]() {
        if (m_acceptSecondsRemaining > 0) {
            --m_acceptSecondsRemaining;
        }

        if (m_acceptSecondsRemaining <= 0) {
            m_acceptCountdownFinished = true;
            m_acceptCountdownTimer->stop();
            AgreementEvidenceManager::instance().recordCountdownCompleted(m_currentUAV);
        }

        updateAcceptButtonState();
        updateStatusText();
    });
    
    btnLayout->addWidget(m_rejectBtn);
    btnLayout->addWidget(m_acceptBtn);
    
    layout->addWidget(m_buttonContainer);
}

void UserAgreementOverlay::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    
    QString containerBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString textColor = isDark ? "#FFFFFF" : "#1D1D1F";
    QString textBg = isDark ? "#1C1C1E" : "#F5F5F7";
    QString borderColor = isDark ? "#3A3A3C" : "#D2D2D7";
    QString primaryBtnBg = "#007AFF";
    QString primaryBtnHoverBg = "#0062CC";
    QString disabledBtnBg = isDark ? "#3A3A3C" : "#D1D1D6";
    QString disabledBtnText = isDark ? "#8E8E93" : "#8E8E93";
    QString secondaryBtnBg = isDark ? "#3A3A3C" : "#E5E5EA";
    QString secondaryBtnHoverBg = isDark ? "#48484A" : "#D1D1D6";
    QString secondaryBtnText = isDark ? "#FFFFFF" : "#1D1D1F";
    QString statusTextColor = isDark ? "#A1A1AA" : "#6E6E73";
    QString readyStatusTextColor = isDark ? "#32D74B" : "#1F9D3A";
    
    m_container->setStyleSheet(QString(
        "QWidget#UserAgreementContainer {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(containerBg, borderColor));
    
    m_titleLabel->setStyleSheet(QString("color: %1;").arg(textColor));
    
    m_textBrowser->setStyleSheet(QString(
        "QTextBrowser#UserAgreementText {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "}"
    ).arg(textBg, textColor, borderColor));

    m_statusLabel->setStyleSheet(QString(
        "QLabel#UserAgreementStatus {"
        "  color: %1;"
        "  padding: 0px 4px;"
        "  font-size: 12px;"
        "}"
        "QLabel#UserAgreementStatus[ready='true'] {"
        "  color: %2;"
        "}"
    ).arg(statusTextColor, readyStatusTextColor));
    
    m_acceptBtn->setStyleSheet(QString(
        "QPushButton#AcceptBtn {"
        "  background-color: %1;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#AcceptBtn:hover {"
        "  background-color: %2;"
        "}"
        "QPushButton#AcceptBtn:disabled {"
        "  background-color: %3;"
        "  color: %4;"
        "}"
    ).arg(primaryBtnBg, primaryBtnHoverBg, disabledBtnBg, disabledBtnText));
    
    m_rejectBtn->setStyleSheet(QString(
        "QPushButton#RejectBtn {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#RejectBtn:hover {"
        "  background-color: %3;"
        "}"
    ).arg(secondaryBtnBg, secondaryBtnText, secondaryBtnHoverBg));
}

void UserAgreementOverlay::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    QString titleTemplate = loc.getString("UserAgreement", "Title");
    if (titleTemplate.isEmpty()) {
        titleTemplate = "APE HOI4 Tool Studio User Agreement (%1)";
    }
    m_titleLabel->setText(titleTemplate.arg(m_currentUAV));

    QString rejectText = loc.getString("UserAgreement", "Reject");
    if (rejectText.isEmpty()) {
        rejectText = "Reject";
    }
    m_rejectBtn->setText(rejectText);
    
    updateAcceptButtonState();
    updateStatusText();
    
    loadAgreementContent();
}

QString UserAgreementOverlay::getUAVVersion() {
    QFile file(":/UserAgreement/version.json");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            return doc.object()["UAV"].toString();
        }
    }
    return "0.0.0.0";
}

QString UserAgreementOverlay::getUAVCheckVersion() {
    return AgreementEvidenceManager::instance().acceptedAgreementVersion();
}

void UserAgreementOverlay::saveUAVCheckVersion(const QString& version) {
    AgreementEvidenceManager::instance().storeAcceptedAgreementVersion(version);
}

void UserAgreementOverlay::loadAgreementContent() {
    const QString lang = ConfigManager::instance().getLanguage();
    QString mdPath = ":/UserAgreement/en_US.md";

    if (lang == "zh_CN") {
        mdPath = ":/UserAgreement/zh_CN.md";
    } else if (lang == "zh_TW") {
        mdPath = ":/UserAgreement/zh_TW.md";
    }

    QFile file(mdPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text) && mdPath != ":/UserAgreement/en_US.md") {
        file.setFileName(":/UserAgreement/en_US.md");
        file.open(QIODevice::ReadOnly | QIODevice::Text);
    }

    if (file.isOpen()) {
        QString content = QString::fromUtf8(file.readAll());
        m_textBrowser->setMarkdown(content);
    } else {
        m_textBrowser->setPlainText("Failed to load user agreement.");
    }
}

void UserAgreementOverlay::checkAgreement() {
    m_currentUAV = getUAVVersion();
    QString checkedUAV = getUAVCheckVersion();
    
    // Simple string comparison works for "YYYY.M.D.R" if zero-padded, 
    // but for safety, we should split and compare integers.
    // For simplicity, assuming standard format or simple string compare if length is same.
    // A robust version comparison:
    auto splitVersion = [](const QString& v) -> QList<int> {
        QList<int> parts;
        for (const QString& p : v.split('.')) {
            parts.append(p.toInt());
        }
        while (parts.size() < 4) parts.append(0);
        return parts;
    };
    
    QList<int> current = splitVersion(m_currentUAV);
    QList<int> checked = splitVersion(checkedUAV);
    
    bool needsUpdate = false;
    for (int i = 0; i < 4; ++i) {
        if (current[i] > checked[i]) {
            needsUpdate = true;
            break;
        } else if (current[i] < checked[i]) {
            break;
        }
    }
    
    if (needsUpdate) {
        showAgreement(false);
    } else {
        emit agreementAccepted();
    }
}

void UserAgreementOverlay::resetAcceptanceRequirements() {
    m_hasScrolledToBottom = false;
    m_acceptCountdownStarted = false;
    m_acceptCountdownFinished = false;
    m_acceptSecondsRemaining = 5;

    if (m_acceptCountdownTimer) {
        m_acceptCountdownTimer->stop();
    }
}

void UserAgreementOverlay::updateAcceptButtonState() {
    LocalizationManager& loc = LocalizationManager::instance();

    if (m_isSettingsMode) {
        QString closeText = loc.getString("UserAgreement", "Close");
        if (closeText.isEmpty()) {
            closeText = "Close";
        }
        m_acceptBtn->setText(closeText);
        m_acceptBtn->setEnabled(true);
        return;
    }

    QString acceptText = loc.getString("UserAgreement", "Accept");
    if (acceptText.isEmpty()) {
        acceptText = "Accept";
    }

    m_acceptBtn->setText(acceptText);
    m_acceptBtn->setEnabled(m_hasScrolledToBottom && m_acceptCountdownFinished);
}

void UserAgreementOverlay::updateStatusText() {
    LocalizationManager& loc = LocalizationManager::instance();

    if (m_isSettingsMode) {
        m_statusLabel->clear();
        m_statusLabel->setProperty("ready", false);
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
        return;
    }

    QString statusText;
    bool isReady = false;

    if (!m_hasScrolledToBottom) {
        statusText = loc.getString("UserAgreement", "ScrollToBottomPrompt");
        if (statusText.isEmpty()) {
            statusText = "Please scroll to the bottom to finish reading the agreement.";
        }
    } else if (!m_acceptCountdownFinished) {
        statusText = loc.getString("UserAgreement", "WaitCountdownPrompt");
        if (statusText.isEmpty()) {
            statusText = "You have reached the end. Please wait %1 seconds before accepting.";
        }
        statusText = statusText.arg(m_acceptSecondsRemaining);
    } else {
        statusText = loc.getString("UserAgreement", "ReadyToAcceptPrompt");
        if (statusText.isEmpty()) {
            statusText = "Reading requirements satisfied. You can now accept the agreement.";
        }
        isReady = true;
    }

    m_statusLabel->setText(statusText);
    m_statusLabel->setProperty("ready", isReady);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

void UserAgreementOverlay::startAcceptanceCountdown() {
    if (m_isSettingsMode || m_acceptCountdownStarted || m_acceptCountdownFinished) {
        return;
    }

    m_acceptCountdownStarted = true;
    m_acceptSecondsRemaining = 5;
    AgreementEvidenceManager::instance().recordCountdownStarted(m_currentUAV, m_acceptSecondsRemaining);
    updateAcceptButtonState();
    updateStatusText();
    m_acceptCountdownTimer->start();
}

void UserAgreementOverlay::onTextScrollChanged() {
    if (m_isSettingsMode || m_hasScrolledToBottom) {
        return;
    }

    QScrollBar *scrollBar = m_textBrowser->verticalScrollBar();
    if (!scrollBar) {
        return;
    }

    if (scrollBar->value() >= scrollBar->maximum()) {
        m_hasScrolledToBottom = true;
        AgreementEvidenceManager::instance().recordScrolledToBottom(m_currentUAV, m_isSettingsMode);
        startAcceptanceCountdown();
        updateAcceptButtonState();
        updateStatusText();
    }
}

void UserAgreementOverlay::showAgreement(bool isSettingsMode) {
    m_isSettingsMode = isSettingsMode;
    m_currentUAV = getUAVVersion();

    resetAcceptanceRequirements();
    AgreementEvidenceManager::instance().recordAgreementShown(m_currentUAV, m_isSettingsMode);
    
    m_rejectBtn->setVisible(!isSettingsMode);
    m_statusContainer->setVisible(!isSettingsMode);
    
    updateTexts();

    m_textBrowser->moveCursor(QTextCursor::Start);
    QScrollBar *scrollBar = m_textBrowser->verticalScrollBar();
    if (scrollBar) {
        scrollBar->setValue(scrollBar->minimum());
    }
    
    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void UserAgreementOverlay::onAcceptClicked() {
    if (!m_isSettingsMode) {
        AgreementEvidenceManager::instance().recordAccepted(m_currentUAV, false);
        saveUAVCheckVersion(m_currentUAV);
        emit agreementAccepted();
    } else {
        AgreementEvidenceManager::instance().recordClosedFromSettings(m_currentUAV);
    }
    hide();
}

void UserAgreementOverlay::onRejectClicked() {
    if (!m_isSettingsMode) {
        AgreementEvidenceManager::instance().recordRejected(m_currentUAV, false);
        emit agreementRejected();
        QApplication::quit();
    }
}

void UserAgreementOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPainterPath path;
    QRectF r = rect();
    qreal radius = 9;
    
    path.addRoundedRect(r, radius, radius);
    
    painter.fillPath(path, QColor(0, 0, 0, 150));
}

bool UserAgreementOverlay::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QWidget::eventFilter(obj, event);
}

void UserAgreementOverlay::updatePosition() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_container->move(
            (width() - m_container->width()) / 2,
            (height() - m_container->height()) / 2
        );
    }
}