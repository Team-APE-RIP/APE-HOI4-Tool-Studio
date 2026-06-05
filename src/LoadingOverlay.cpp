//-------------------------------------------------------------------------------------
// LoadingOverlay.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "LoadingOverlay.h"
#include "ConfigManager.h"
#include "OverlayAcrylicMaterial.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QScreen>
#include <QDebug>

LoadingOverlay::LoadingOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    OverlayAcrylicMaterial::installLiveRefresh(this);
    // Don't set FramelessWindowHint - it makes this a separate window instead of a child widget
    
    // Container for content
    m_container = new OverlayAcrylicPanel(this);
    m_container->setObjectName("LoadingContainer");
    m_container->setFixedSize(350, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(15);
    layout->setAlignment(Qt::AlignCenter);
    
    // Icon
    m_iconLabel = new QLabel(m_container);
    m_iconLabel->setPixmap(QPixmap(":/app.ico"));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_iconLabel);
    
    // Message
    m_messageLabel = new QLabel(m_container);
    m_messageLabel->setObjectName("LoadingMessage");
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setWordWrap(true);
    layout->addWidget(m_messageLabel);
    
    // Progress bar
    m_progressBar = new QProgressBar(m_container);
    m_progressBar->setObjectName("LoadingProgressBar");
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setRange(0, 0); // Indeterminate by default
    layout->addWidget(m_progressBar);
    
    updateTheme();
    
    hide();
    
    if (parent) {
        parent->installEventFilter(this);
    }
}

void LoadingOverlay::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    
    QString textColor = isDark ? "#FFFFFF" : "#1D1D1F";
    QString progressBg = isDark ? "rgba(58, 58, 60, 0.58)" : "rgba(229, 229, 234, 0.68)";
    QString progressChunk = OverlayAcrylicMaterial::accentGlassBrush(isDark);
    
    m_container->setStyleSheet(QString(
        "QWidget#LoadingContainer {"
        "  background-color: transparent;"
        "  border: none;"
        "}"
    ));
    
    m_messageLabel->setStyleSheet(QString(
        "QLabel#LoadingMessage {"
        "  color: %1;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
    ).arg(textColor));
    
    m_progressBar->setStyleSheet(QString(
        "QProgressBar#LoadingProgressBar {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        "QProgressBar#LoadingProgressBar::chunk {"
        "  background-color: %2;"
        "  border-radius: 3px;"
        "}"
    ).arg(progressBg, progressChunk));
}

void LoadingOverlay::setMessage(const QString& message) {
    m_messageLabel->setText(message);
}

void LoadingOverlay::setProgress(int value) {
    if (value < 0) {
        m_progressBar->setRange(0, 0); // Indeterminate
    } else {
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(value);
    }
}

void LoadingOverlay::showOverlay() {
    updateTheme();
    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void LoadingOverlay::hideOverlay() {
    qDebug() << "LoadingOverlay::hideOverlay() called, isVisible:" << isVisible();
    hide();
    // CRITICAL FIX: Lower the overlay to prevent it from blocking tool UI
    // Even when hidden, the widget's z-order can still affect event routing
    if (parentWidget()) {
        lower();
    }
    qDebug() << "LoadingOverlay::hideOverlay() after hide() and lower(), isVisible:" << isVisible();
}

void LoadingOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    OverlayAcrylicMaterial::paintOverlayBackdrop(
        painter,
        this,
        QRectF(rect()),
        9.0,
        ConfigManager::instance().isCurrentThemeDark(),
        116);
}

bool LoadingOverlay::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QWidget::eventFilter(obj, event);
}

void LoadingOverlay::updatePosition() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_container->move(
            (width() - m_container->width()) / 2,
            (height() - m_container->height()) / 2
        );
    }
}
