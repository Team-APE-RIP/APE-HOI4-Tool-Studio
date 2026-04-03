//-------------------------------------------------------------------------------------
// ConnectionWarningOverlay.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ConnectionWarningOverlay.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

ConnectionWarningOverlay::ConnectionWarningOverlay(QWidget *parent)
    : QWidget(parent)
    , m_hostWidget(parent)
    , m_warningBar(new QWidget(this))
    , m_reconnectingPanel(new QWidget(this))
    , m_titleLabel(new QLabel(m_warningBar))
    , m_messageLabel(new QLabel(m_warningBar))
    , m_countdownLabel(new QLabel(m_warningBar))
    , m_reconnectTitleLabel(new QLabel(m_reconnectingPanel))
    , m_countdownTimer(new QTimer(this))
    , m_spinnerTimer(new QTimer(this))
    , m_remainingSeconds(30)
    , m_actionRemainingSeconds(0)
    , m_spinnerAngle(0)
    , m_mode(OverlayMode::Countdown)
    , m_scenario(WarningScenario::Network)
    , m_reconnectRemainingOffset(0.0)
    , m_reconnectStatusOffset(0.0)
    , m_reconnectReasonOffset(0.0)
    , m_reconnectRemainingTextWidth(0.0)
    , m_reconnectStatusTextWidth(0.0)
    , m_reconnectReasonTextWidth(0.0)
    , m_marqueeTickCount(0)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    m_warningBar->setObjectName("ConnectionWarningBar");
    m_reconnectingPanel->setObjectName("ConnectionReconnectingPanel");

    QVBoxLayout *barLayout = new QVBoxLayout(m_warningBar);
    barLayout->setContentsMargins(20, 10, 20, 10);
    barLayout->setSpacing(4);

    m_titleLabel->setObjectName("ConnectionWarningTitle");
    m_messageLabel->setObjectName("ConnectionWarningMessage");
    m_countdownLabel->setObjectName("ConnectionWarningCountdown");

    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_countdownLabel->setAlignment(Qt::AlignCenter);

    barLayout->addWidget(m_titleLabel);
    barLayout->addWidget(m_messageLabel);
    barLayout->addWidget(m_countdownLabel);

    QVBoxLayout *reconnectingLayout = new QVBoxLayout(m_reconnectingPanel);
    reconnectingLayout->setContentsMargins(32, 96, 32, 32);
    reconnectingLayout->setSpacing(12);
    reconnectingLayout->addStretch();

    m_reconnectTitleLabel->setObjectName("ConnectionReconnectTitle");
    m_reconnectTitleLabel->setAlignment(Qt::AlignCenter);
    m_reconnectTitleLabel->hide();

    reconnectingLayout->addWidget(m_reconnectTitleLabel, 0, Qt::AlignHCenter);
    reconnectingLayout->addStretch();

    m_warningBar->hide();
    m_reconnectingPanel->hide();
    hide();

    connect(m_countdownTimer, &QTimer::timeout, this, &ConnectionWarningOverlay::onCountdownTick);
    connect(m_spinnerTimer, &QTimer::timeout, this, &ConnectionWarningOverlay::onSpinnerTick);
    m_spinnerTimer->setInterval(80);

    if (m_hostWidget) {
        m_hostWidget->installEventFilter(this);
    }

    applyInteractionPolicy();
    updateTheme();
    updateTexts();

    Logger::instance().logInfo(
        "ConnectionWarningOverlay",
        QString("Constructed. Host=%1 thread=%2")
            .arg(m_hostWidget ? "valid" : "null")
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId())));
}

ConnectionWarningOverlay::~ConnectionWarningOverlay() {
    Logger::instance().logInfo(
        "ConnectionWarningOverlay",
        QString("Destroyed. thread=%1")
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId())));
}

void ConnectionWarningOverlay::updateTheme() {
    const bool isDark = ConfigManager::instance().isCurrentThemeDark();
    const QString barBg = isDark ? "rgba(120, 0, 0, 0.88)" : "rgba(200, 0, 0, 0.9)";
    const QString panelBg = isDark ? "rgba(170, 0, 0, 1.0)" : "rgba(220, 0, 0, 1.0)";
    const QString textColor = "#FFFFFF";

    m_warningBar->setStyleSheet(QString(
        "QWidget#ConnectionWarningBar {"
        " background-color: %1;"
        " border-radius: 6px;"
        "}"
        "QLabel#ConnectionWarningTitle { color: %2; font-weight: 700; font-size: 14px; }"
        "QLabel#ConnectionWarningMessage { color: %2; font-size: 12px; }"
        "QLabel#ConnectionWarningCountdown { color: %2; font-size: 12px; font-weight: 600; }")
                                    .arg(barBg, textColor));

    m_reconnectingPanel->setStyleSheet(QString(
        "QWidget#ConnectionReconnectingPanel { background-color: transparent; }"
        "QLabel#ConnectionReconnectTitle { color: %1; font-weight: 800; font-size: 22px; }")
                                            .arg(textColor));

    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, QColor(panelBg));
    setPalette(palette);

    update();
}

void ConnectionWarningOverlay::updateTexts() {
    updateCountdownText();
    updateReconnectTexts();
    updateReconnectCountdownText();
}

void ConnectionWarningOverlay::showWarning() {
    Logger::instance().logInfo(
        "ConnectionWarningOverlay",
        QString("showWarning called. visible=%1 mode=%2 barVisible=%3 thread=%4")
            .arg(isVisible())
            .arg(m_mode == OverlayMode::Countdown ? "countdown" : "reconnecting")
            .arg(m_warningBar->isVisible())
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId())));

    m_scenario = WarningScenario::Network;
    m_accountActionInfo = AccountActionInfo();
    m_mode = OverlayMode::Countdown;
    m_remainingSeconds = 30;
    m_actionRemainingSeconds = 0;
    m_spinnerAngle = 0;

    m_spinnerTimer->stop();
    m_countdownTimer->stop();
    resetMarqueeAnimation();
    updateTexts();
    updateTheme();
    applyInteractionPolicy();
    updatePosition();

    m_warningBar->setVisible(true);
    m_reconnectingPanel->setVisible(false);
    setVisible(true);
    raise();
    show();

    if (!m_countdownTimer->isActive()) {
        m_countdownTimer->start(1000);
    }

    Logger::instance().logInfo(
        "ConnectionWarningOverlay",
        QString("showWarning done. geo=%1,%2,%3,%4 timerActive=%5")
            .arg(geometry().x())
            .arg(geometry().y())
            .arg(geometry().width())
            .arg(geometry().height())
            .arg(m_countdownTimer->isActive()));
}

void ConnectionWarningOverlay::showAccountActionWarning(const AccountActionInfo& info) {
    Logger::instance().logInfo(
        "ConnectionWarningOverlay",
        QString("showAccountActionWarning called. type=%1 blocking=%2 remaining=%3 permanent=%4")
            .arg(info.type)
            .arg(info.blocking)
            .arg(info.remainingSeconds)
            .arg(info.permanent));

    m_accountActionInfo = info;
    m_scenario = resolveScenarioFromAccountAction(info);
    m_mode = OverlayMode::Reconnecting;
    m_remainingSeconds = 30;
    m_actionRemainingSeconds = qMax<qint64>(0, info.remainingSeconds);
    m_spinnerAngle = 0;

    m_warningBar->hide();
    m_reconnectingPanel->show();
    setVisible(true);
    raise();
    show();

    resetMarqueeAnimation();
    updateTexts();
    updateTheme();
    applyInteractionPolicy();
    updatePosition();

    if (!m_spinnerTimer->isActive()) {
        m_spinnerTimer->start();
    }

    if (!info.permanent && info.remainingSeconds > 0) {
        if (!m_countdownTimer->isActive()) {
            m_countdownTimer->start(1000);
        }
    } else {
        m_countdownTimer->stop();
    }

    update();
}

void ConnectionWarningOverlay::hideWarning() {
    Logger::instance().logInfo(
        "ConnectionWarningOverlay",
        QString("hideWarning called. timerActive=%1 spinnerActive=%2")
            .arg(m_countdownTimer->isActive())
            .arg(m_spinnerTimer->isActive()));

    m_countdownTimer->stop();
    m_spinnerTimer->stop();
    m_mode = OverlayMode::Countdown;
    m_scenario = WarningScenario::Network;
    m_remainingSeconds = 30;
    m_actionRemainingSeconds = 0;
    m_spinnerAngle = 0;
    m_accountActionInfo = AccountActionInfo();
    resetMarqueeAnimation();
    applyInteractionPolicy();
    m_warningBar->hide();
    m_reconnectingPanel->hide();
    hide();
}

void ConnectionWarningOverlay::onCountdownTick() {
    if (m_scenario == WarningScenario::Network && m_mode == OverlayMode::Countdown) {
        m_remainingSeconds -= 1;
        Logger::instance().logInfo(
            "ConnectionWarningOverlay",
            QString("Network countdown tick: %1").arg(m_remainingSeconds));

        if (m_remainingSeconds <= 0) {
            Logger::instance().logInfo(
                "ConnectionWarningOverlay",
                "Countdown reached 0, switching to reconnecting mode.");
            enterReconnectingMode();
            return;
        }

        updateCountdownText();
        return;
    }

    if (m_scenario != WarningScenario::Network && m_mode == OverlayMode::Reconnecting && !m_accountActionInfo.permanent) {
        m_actionRemainingSeconds = qMax<qint64>(0, m_actionRemainingSeconds - 1);
        updateReconnectCountdownText();
    }
}

void ConnectionWarningOverlay::onSpinnerTick() {
    m_spinnerAngle = (m_spinnerAngle + 30) % 360;
    if (m_scenario != WarningScenario::Network && m_mode == OverlayMode::Reconnecting) {
        updateMarqueeState();
    }
    update();
}

QString ConnectionWarningOverlay::formatRemainingTime(qint64 remainingSeconds) const {
    LocalizationManager &loc = LocalizationManager::instance();
    const qint64 normalizedSeconds = qMax<qint64>(0, remainingSeconds);
    const qint64 days = normalizedSeconds / 86400;
    const qint64 hours = (normalizedSeconds % 86400) / 3600;
    const qint64 minutes = (normalizedSeconds % 3600) / 60;
    const qint64 seconds = normalizedSeconds % 60;

    QStringList parts;
    if (days > 0) {
        parts << loc.getString("ConnectionWarning", "DurationDaysUnit").arg(days);
    }
    if (hours > 0) {
        parts << loc.getString("ConnectionWarning", "DurationHoursUnit").arg(hours);
    }
    if (minutes > 0) {
        parts << loc.getString("ConnectionWarning", "DurationMinutesUnit").arg(minutes);
    }
    if (seconds > 0 || parts.isEmpty()) {
        parts << loc.getString("ConnectionWarning", "DurationSecondsUnit").arg(seconds);
    }

    return parts.join(' ');
}

QString ConnectionWarningOverlay::buildAccountActionReasonText() const {
    LocalizationManager &loc = LocalizationManager::instance();
    const QString reasonText = m_accountActionInfo.reason.isEmpty()
        ? loc.getString("ConnectionWarning", "AccountActionNoReason")
        : m_accountActionInfo.reason;
    return loc.getString("ConnectionWarning", "AccountActionReasonPrefix").arg(reasonText);
}

QString ConnectionWarningOverlay::buildAccountActionReleaseText() const {
    LocalizationManager &loc = LocalizationManager::instance();
    const QString releasePrefix = loc.getString("ConnectionWarning", "AccountActionReleasePrefix");

    if (m_accountActionInfo.permanent || m_accountActionInfo.type == "terminated") {
        return releasePrefix.arg(loc.getString("ConnectionWarning", "AccountActionPermanent"));
    }

    if (m_actionRemainingSeconds > 0) {
        return releasePrefix.arg(formatRemainingTime(m_actionRemainingSeconds));
    }

    return releasePrefix.arg(loc.getString("ConnectionWarning", "AccountActionPendingRefresh"));
}

QString ConnectionWarningOverlay::buildAccountActionStatusText() const {
    LocalizationManager &loc = LocalizationManager::instance();

    switch (m_scenario) {
    case WarningScenario::Restricted:
        return loc.getString("ConnectionWarning", "RestrictedReconnectMessage");
    case WarningScenario::Paused:
        return loc.getString("ConnectionWarning", "PausedReconnectMessage");
    case WarningScenario::Banned:
        return loc.getString("ConnectionWarning", "BannedReconnectMessage");
    case WarningScenario::Terminated:
        return loc.getString("ConnectionWarning", "TerminatedReconnectMessage");
    default:
        return loc.getString("ConnectionWarning", "ReconnectMessage");
    }
}

ConnectionWarningOverlay::WarningScenario ConnectionWarningOverlay::resolveScenarioFromAccountAction(const AccountActionInfo& info) const {
    if (info.type == "restricted") return WarningScenario::Restricted;
    if (info.type == "paused") return WarningScenario::Paused;
    if (info.type == "banned") return WarningScenario::Banned;
    if (info.type == "terminated") return WarningScenario::Terminated;
    return WarningScenario::Network;
}

void ConnectionWarningOverlay::updateCountdownText() {
    LocalizationManager &loc = LocalizationManager::instance();

    if (m_scenario == WarningScenario::Network) {
        m_titleLabel->setText(loc.getString("ConnectionWarning", "Title"));
        m_messageLabel->setText(loc.getString("ConnectionWarning", "Message"));
        m_countdownLabel->setText(loc.getString("ConnectionWarning", "Countdown").arg(m_remainingSeconds));
        return;
    }

    m_countdownLabel->clear();
}

void ConnectionWarningOverlay::updateReconnectTexts() {
    LocalizationManager &loc = LocalizationManager::instance();

    if (m_scenario == WarningScenario::Network) {
        m_reconnectTitleLabel->setText(loc.getString("ConnectionWarning", "ReconnectTitle"));
        m_reconnectRemainingText.clear();
        m_reconnectStatusText.clear();
        m_reconnectReasonText.clear();
        resetMarqueeAnimation();
        return;
    }

    QString titleKey;
    switch (m_scenario) {
    case WarningScenario::Restricted:
        titleKey = "RestrictedReconnectTitle";
        break;
    case WarningScenario::Paused:
        titleKey = "PausedReconnectTitle";
        break;
    case WarningScenario::Banned:
        titleKey = "BannedReconnectTitle";
        break;
    case WarningScenario::Terminated:
        titleKey = "TerminatedReconnectTitle";
        break;
    default:
        titleKey = "ReconnectTitle";
        break;
    }

    m_reconnectTitleLabel->setText(loc.getString("ConnectionWarning", titleKey));
    m_reconnectStatusText = buildAccountActionStatusText();
    m_reconnectReasonText = buildAccountActionReasonText();
    updateMarqueeState();
}

void ConnectionWarningOverlay::updateReconnectCountdownText() {
    LocalizationManager &loc = LocalizationManager::instance();

    if (m_scenario == WarningScenario::Network) {
        m_reconnectRemainingText.clear();
        m_reconnectStatusText.clear();
        m_reconnectReasonText.clear();
        resetMarqueeAnimation();
        return;
    }

    m_reconnectRemainingText = buildAccountActionReleaseText();

    if (!m_accountActionInfo.permanent && m_actionRemainingSeconds <= 0) {
        m_reconnectRemainingText = loc.getString("ConnectionWarning", "AccountActionPendingRefreshHint");
    }

    updateMarqueeState();
}

void ConnectionWarningOverlay::enterReconnectingMode() {
    m_countdownTimer->stop();
    m_mode = OverlayMode::Reconnecting;
    applyInteractionPolicy();
    updateTexts();
    updateTheme();
    updatePosition();
    m_warningBar->hide();
    m_reconnectingPanel->show();
    raise();
    show();

    if (!m_spinnerTimer->isActive()) {
        m_spinnerTimer->start();
    }

    update();
}

void ConnectionWarningOverlay::applyInteractionPolicy() {
    setAttribute(Qt::WA_TransparentForMouseEvents, !isBlockingMode());
}

bool ConnectionWarningOverlay::isBlockingMode() const {
    return m_mode == OverlayMode::Reconnecting;
}

void ConnectionWarningOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal overlayRadius = 9.0;
    const qreal borderRadius = 6.0;
    const QRectF overlayRect = rect();

    if (m_mode == OverlayMode::Countdown) {
        const qreal penWidth = 4.0;
        const QRectF borderRect = overlayRect.adjusted(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);

        QPen pen(QColor(220, 0, 0, 220), penWidth);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        QPainterPath path;
        path.addRoundedRect(borderRect, borderRadius, borderRadius);
        painter.drawPath(path);
        return;
    }

    QPainterPath overlayPath;
    overlayPath.addRoundedRect(overlayRect, overlayRadius, overlayRadius);
    painter.fillPath(overlayPath, QColor(220, 0, 0, 255));

    const int spinnerSize = 48;
    const int spinnerTop = qMax(48, height() / 2 - 88);
    const QRect spinnerRect((width() - spinnerSize) / 2, spinnerTop, spinnerSize, spinnerSize);

    painter.save();
    painter.translate(spinnerRect.center());
    painter.rotate(m_spinnerAngle);

    QPen spinnerPen(QColor(255, 255, 255, 235), 5.0, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(spinnerPen);

    for (int i = 0; i < 12; ++i) {
        QColor tickColor(255, 255, 255);
        tickColor.setAlpha(50 + i * 16);
        spinnerPen.setColor(tickColor);
        painter.setPen(spinnerPen);
        painter.drawLine(0, -spinnerSize / 2 + 6, 0, -spinnerSize / 2 + 18);
        painter.rotate(30.0);
    }

    painter.restore();

    if (m_scenario == WarningScenario::Network) {
        return;
    }

    const QRect panelRect = m_reconnectingPanel->geometry();
    const int horizontalPadding = 32;
    const int lineSpacing = 12;
    const int lineHeight = 28;
    const int textLeft = panelRect.left() + horizontalPadding;
    const int textWidth = qMax(80, panelRect.width() - horizontalPadding * 2);

    QFont titleFont = painter.font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    const QFontMetrics titleMetrics(titleFont);
    const int titleHeight = titleMetrics.height();
    const int titleTop = panelRect.top() + 96;
    const QRect titleRect(textLeft, titleTop, textWidth, titleHeight);

    const int baseTop = titleRect.bottom() + lineSpacing + 6;
    const QRect remainingRect(textLeft, baseTop, textWidth, lineHeight);
    const QRect statusRect(textLeft, baseTop + lineHeight + lineSpacing, textWidth, lineHeight);
    const QRect reasonRect(textLeft, baseTop + (lineHeight + lineSpacing) * 2, textWidth, lineHeight);

    painter.save();
    painter.setFont(titleFont);
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(titleRect, Qt::AlignCenter, m_reconnectTitleLabel->text());
    painter.restore();

    auto drawMarqueeLine = [&painter](const QRect& rect, const QString& text, qreal textWidth, qreal offset, bool emphasize) {
        painter.save();
        painter.setClipRect(rect);

        QFont font = painter.font();
        font.setPointSize(14);
        font.setBold(emphasize);
        painter.setFont(font);

        const QFontMetrics metrics(font);
        const int baselineY = rect.y() + (rect.height() + metrics.ascent() - metrics.descent()) / 2;
        const qreal fadeWidth = qMin<qreal>(42.0, rect.width() * 0.18);

        auto buildGradientBrush = [&](qreal left, qreal width) {
            QLinearGradient gradient(left, 0.0, left + width, 0.0);
            const qreal fadeRatio = width > 0.0 ? qBound(0.0, fadeWidth / width, 0.45) : 0.0;
            gradient.setColorAt(0.0, QColor(255, 255, 255, 0));
            gradient.setColorAt(fadeRatio, QColor(255, 255, 255, 255));
            gradient.setColorAt(1.0 - fadeRatio, QColor(255, 255, 255, 255));
            gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
            return QBrush(gradient);
        };

        auto drawTextPath = [&](qreal x) {
            QPainterPath path;
            path.addText(QPointF(x, baselineY), font, text);
            painter.fillPath(path, buildGradientBrush(x, textWidth));
        };

        if (text.isEmpty()) {
            painter.restore();
            return;
        }

        if (textWidth <= rect.width()) {
            painter.setPen(QColor(255, 255, 255));
            painter.drawText(rect, Qt::AlignCenter, text);
            painter.restore();
            return;
        }

        const qreal gap = 80.0;
        const qreal startX = rect.x() + (rect.width() - textWidth) / 2.0 - offset;
        drawTextPath(startX);
        drawTextPath(startX + textWidth + gap);
        painter.restore();
    };

    drawMarqueeLine(remainingRect, m_reconnectRemainingText, m_reconnectRemainingTextWidth, m_reconnectRemainingOffset, true);
    drawMarqueeLine(statusRect, m_reconnectStatusText, m_reconnectStatusTextWidth, m_reconnectStatusOffset, false);
    drawMarqueeLine(reasonRect, m_reconnectReasonText, m_reconnectReasonTextWidth, m_reconnectReasonOffset, false);
}

bool ConnectionWarningOverlay::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_hostWidget && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updatePosition();
    }
    return QWidget::eventFilter(obj, event);
}

void ConnectionWarningOverlay::mousePressEvent(QMouseEvent *event) {
    if (isBlockingMode()) {
        if (forwardMouseEventToButton(event) || tryStartWindowDrag(event)) {
            event->accept();
            return;
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ConnectionWarningOverlay::mouseReleaseEvent(QMouseEvent *event) {
    if (isBlockingMode()) {
        if (forwardMouseEventToButton(event)) {
            event->accept();
            return;
        }

        if (QWidget *windowWidget = window()) {
            const QPoint globalPos = event->globalPosition().toPoint();
            QMouseEvent forwardedEvent(
                event->type(),
                windowWidget->mapFromGlobal(globalPos),
                globalPos,
                event->button(),
                event->buttons(),
                event->modifiers());
            QCoreApplication::sendEvent(windowWidget, &forwardedEvent);
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ConnectionWarningOverlay::mouseDoubleClickEvent(QMouseEvent *event) {
    if (isBlockingMode()) {
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ConnectionWarningOverlay::mouseMoveEvent(QMouseEvent *event) {
    if (isBlockingMode()) {
        if (QWidget *windowWidget = window()) {
            const QPoint globalPos = event->globalPosition().toPoint();
            QMouseEvent forwardedEvent(
                event->type(),
                windowWidget->mapFromGlobal(globalPos),
                globalPos,
                event->button(),
                event->buttons(),
                event->modifiers());
            QCoreApplication::sendEvent(windowWidget, &forwardedEvent);
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

bool ConnectionWarningOverlay::shouldAllowPassThrough(const QPoint &pos) const {
    if (!m_hostWidget) {
        return false;
    }

    const int sidebarWidth = 250;
    return pos.x() <= sidebarWidth;
}

bool ConnectionWarningOverlay::forwardMouseEventToButton(QMouseEvent *event) {
    if (!m_hostWidget) {
        return false;
    }

    const QPoint hostPos = event->pos();
    QWidget *targetWidget = m_hostWidget->childAt(hostPos);
    QPushButton *button = qobject_cast<QPushButton *>(targetWidget);
    if (!button) {
        return false;
    }

    if (!button->isVisible() || !button->isEnabled()) {
        return false;
    }

    const QPoint buttonLocalPos = button->mapFrom(m_hostWidget, hostPos);
    QMouseEvent forwardedEvent(
        event->type(),
        buttonLocalPos,
        event->globalPosition(),
        event->button(),
        event->buttons(),
        event->modifiers());
    QCoreApplication::sendEvent(button, &forwardedEvent);
    return true;
}

bool ConnectionWarningOverlay::tryStartWindowDrag(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        return false;
    }

    if (QWidget *windowWidget = window()) {
        const QPoint globalPos = event->globalPosition().toPoint();
        QMouseEvent forwardedEvent(
            event->type(),
            windowWidget->mapFromGlobal(globalPos),
            globalPos,
            event->button(),
            event->buttons(),
            event->modifiers());
        QCoreApplication::sendEvent(windowWidget, &forwardedEvent);
        return true;
    }

    return false;
}

void ConnectionWarningOverlay::updatePosition() {
    if (!m_hostWidget) {
        Logger::instance().logInfo("ConnectionWarningOverlay", "updatePosition skipped: host is null.");
        return;
    }

    setGeometry(m_hostWidget->rect());

    const int barHeight = 90;
    m_warningBar->setGeometry(20, height() - barHeight - 20, width() - 40, barHeight);

    const int reconnectHorizontalMargin = 32;
    const int reconnectTopMargin = 96;
    const int reconnectBottomMargin = 32;
    const int reconnectSpacing = 12;
    const int maxPanelWidth = 1280;
    const int availablePanelWidth = qMax(420, width() - 80);

    QFont titleFont = font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    const int titleWidth = QFontMetrics(titleFont).horizontalAdvance(m_reconnectTitleLabel->text());
    const int contentWidth = qMax(titleWidth, availablePanelWidth - reconnectHorizontalMargin * 2);
    const int desiredPanelWidth = contentWidth + reconnectHorizontalMargin * 2;
    const int panelWidth = qMin(availablePanelWidth, qMin(maxPanelWidth, desiredPanelWidth));

    const int titleHeight = QFontMetrics(titleFont).height();
    const int lineHeight = 28;
    const int desiredPanelHeight = reconnectTopMargin + titleHeight + reconnectSpacing
        + lineHeight + reconnectSpacing
        + lineHeight + reconnectSpacing
        + lineHeight + reconnectBottomMargin;
    const int panelHeight = qMin(height() - 80, qMax(220, desiredPanelHeight));

    m_reconnectingPanel->setGeometry((width() - panelWidth) / 2, (height() - panelHeight) / 2 + 18, panelWidth, panelHeight);
    updateMarqueeState();

    Logger::instance().logInfo(
        "ConnectionWarningOverlay",
        QString("updatePosition hostRect=%1,%2,%3,%4 barGeo=%5,%6,%7,%8 reconnectGeo=%9,%10,%11,%12 desiredReconnectWidth=%13 availableReconnectWidth=%14")
            .arg(m_hostWidget->rect().x())
            .arg(m_hostWidget->rect().y())
            .arg(m_hostWidget->rect().width())
            .arg(m_hostWidget->rect().height())
            .arg(m_warningBar->geometry().x())
            .arg(m_warningBar->geometry().y())
            .arg(m_warningBar->geometry().width())
            .arg(m_warningBar->geometry().height())
            .arg(m_reconnectingPanel->geometry().x())
            .arg(m_reconnectingPanel->geometry().y())
            .arg(m_reconnectingPanel->geometry().width())
            .arg(m_reconnectingPanel->geometry().height())
            .arg(desiredPanelWidth)
            .arg(availablePanelWidth));
}

void ConnectionWarningOverlay::updateMarqueeState() {
    const int availableWidth = qMax(80, m_reconnectingPanel->width() - 64);

    QFont remainingFont = font();
    remainingFont.setPointSize(14);
    remainingFont.setBold(true);

    QFont messageFont = font();
    messageFont.setPointSize(14);
    messageFont.setBold(false);

    m_reconnectRemainingTextWidth = QFontMetrics(remainingFont).horizontalAdvance(m_reconnectRemainingText);
    m_reconnectStatusTextWidth = QFontMetrics(messageFont).horizontalAdvance(m_reconnectStatusText);
    m_reconnectReasonTextWidth = QFontMetrics(messageFont).horizontalAdvance(m_reconnectReasonText);

    const qreal gap = 80.0;
    const qreal speed = 1.5;
    const qint64 holdTicks = 18;

    auto advanceOffset = [&](qreal& offset, qreal width) {
        if (width <= availableWidth) {
            offset = 0.0;
            return;
        }

        if (m_marqueeTickCount < holdTicks) {
            return;
        }

        offset += speed;
        if (offset > width + gap) {
            offset = 0.0;
        }
    };

    advanceOffset(m_reconnectRemainingOffset, m_reconnectRemainingTextWidth);
    advanceOffset(m_reconnectStatusOffset, m_reconnectStatusTextWidth);
    advanceOffset(m_reconnectReasonOffset, m_reconnectReasonTextWidth);

    m_marqueeTickCount += 1;
}

void ConnectionWarningOverlay::resetMarqueeAnimation() {
    m_reconnectRemainingOffset = 0.0;
    m_reconnectStatusOffset = 0.0;
    m_reconnectReasonOffset = 0.0;
    m_reconnectRemainingTextWidth = 0.0;
    m_reconnectStatusTextWidth = 0.0;
    m_reconnectReasonTextWidth = 0.0;
    m_marqueeTickCount = 0;
}