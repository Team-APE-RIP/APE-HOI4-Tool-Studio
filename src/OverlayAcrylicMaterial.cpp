//-------------------------------------------------------------------------------------
// OverlayAcrylicMaterial.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "OverlayAcrylicMaterial.h"

#include "ConfigManager.h"

#include <QApplication>
#include <QFontMetrics>
#include <QMetaObject>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QStyleOptionButton>
#include <QTimer>

#include <algorithm>

namespace {
int clampByte(int value) {
    return std::clamp(value, 0, 255);
}

quint32 noiseForPoint(int x, int y, int salt) {
    quint32 value = static_cast<quint32>(x) * 374761393u
        + static_cast<quint32>(y) * 668265263u
        + static_cast<quint32>(salt) * 2246822519u;
    value = (value ^ (value >> 13)) * 1274126177u;
    return value ^ (value >> 16);
}

QWidget* centralBackdropForWindow(QWidget* window) {
    if (!window) {
        return nullptr;
    }
    if (window->objectName() == QStringLiteral("CentralWidget")
        || QByteArray(window->metaObject()->className()) == QByteArray("ScreenAcrylicBackdrop")) {
        return window;
    }
    QWidget* central = window->findChild<QWidget*>(QStringLiteral("CentralWidget"), Qt::FindChildrenRecursively);
    if (central) {
        return central;
    }
    const auto children = window->findChildren<QWidget*>(QString(), Qt::FindChildrenRecursively);
    for (QWidget* child : children) {
        if (child && QByteArray(child->metaObject()->className()) == QByteArray("ScreenAcrylicBackdrop")) {
            return child;
        }
    }
    return nullptr;
}

QWidget* findBackdrop(QWidget* target) {
    for (QWidget* cursor = target; cursor; cursor = cursor->parentWidget()) {
        if (QWidget* window = cursor->window()) {
            if (QWidget* backdrop = centralBackdropForWindow(window)) {
                return backdrop;
            }
        }
        if (QWidget* backdrop = centralBackdropForWindow(cursor)) {
            return backdrop;
        }
    }

    if (qApp) {
        const auto windows = QApplication::topLevelWidgets();
        for (QWidget* window : windows) {
            if (QWidget* backdrop = centralBackdropForWindow(window)) {
                return backdrop;
            }
        }
    }
    return nullptr;
}

QImage backdropImage(QWidget* target, const QRect& globalRect, const QSize& targetSize) {
    QWidget* backdrop = findBackdrop(target);
    if (!backdrop) {
        return QImage();
    }

    QImage image;
    const bool invoked = QMetaObject::invokeMethod(
        backdrop,
        "acrylicImageForGlobalRect",
        Qt::DirectConnection,
        Q_RETURN_ARG(QImage, image),
        Q_ARG(QRect, globalRect),
        Q_ARG(QSize, targetSize));
    return invoked ? image : QImage();
}

QImage syntheticMaterial(const QSize& size, const QPoint& globalTopLeft, bool isDark) {
    if (size.width() <= 0 || size.height() <= 0) {
        return QImage();
    }

    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    const int baseR = isDark ? 42 : 238;
    const int baseG = isDark ? 42 : 240;
    const int baseB = isDark ? 45 : 244;
    const int tintR = isDark ? 30 : 255;
    const int tintG = isDark ? 34 : 255;
    const int tintB = isDark ? 42 : 255;

    for (int y = 0; y < size.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            const int gx = globalTopLeft.x() + x;
            const int gy = globalTopLeft.y() + y;
            const int coarse = static_cast<int>(noiseForPoint(gx / 18, gy / 18, 19) & 0xff) - 128;
            const int fine = static_cast<int>(noiseForPoint(gx / 5, gy / 5, 43) & 0xff) - 128;
            const int diagonal = ((gx + gy) % 96) - 48;
            const int vertical = size.height() > 1 ? (y * 22 / size.height()) - 11 : 0;
            const int weight = isDark ? 34 : 38;

            int r = baseR + coarse / 12 + fine / 32 + diagonal / 22 + vertical;
            int g = baseG + coarse / 14 + fine / 36 + diagonal / 26 + vertical;
            int b = baseB + coarse / 10 + fine / 28 - diagonal / 24 + vertical;

            r = (r * (100 - weight) + tintR * weight) / 100;
            g = (g * (100 - weight) + tintG * weight) / 100;
            b = (b * (100 - weight) + tintB * weight) / 100;

            line[x] = qRgba(clampByte(r), clampByte(g), clampByte(b), 255);
        }
    }
    return image;
}

QColor textColor(bool isDark, bool enabled, OverlayAcrylicButton::Role role) {
    if (!enabled) {
        return isDark ? QColor(235, 235, 245, 92) : QColor(60, 60, 67, 88);
    }
    if (role == OverlayAcrylicButton::Role::Accent || role == OverlayAcrylicButton::Role::Danger) {
        return QColor(255, 255, 255);
    }
    return isDark ? QColor(245, 245, 247) : QColor(29, 29, 31);
}

QColor roleTint(OverlayAcrylicButton::Role role, bool isDark, bool enabled, bool hovered, bool pressed, bool checked) {
    if (!enabled) {
        return isDark ? QColor(84, 84, 88, 96) : QColor(210, 210, 216, 118);
    }

    int alpha = pressed || checked ? 214 : (hovered ? 184 : 158);
    if (role == OverlayAcrylicButton::Role::Accent) {
        return isDark ? QColor(10, 132, 255, alpha) : QColor(0, 122, 255, alpha);
    }
    if (role == OverlayAcrylicButton::Role::Danger) {
        return QColor(255, 59, 48, pressed ? 204 : (hovered ? 174 : 146));
    }
    alpha = pressed ? 128 : (hovered ? 102 : 76);
    return isDark ? QColor(86, 86, 92, alpha) : QColor(255, 255, 255, alpha);
}
}

QImage OverlayAcrylicMaterial::materialImage(QWidget* target, const QRect& localRect, bool isDark, const QSize& requestedSize) {
    if (!target || localRect.width() <= 0 || localRect.height() <= 0) {
        return QImage();
    }

    const QSize targetSize = requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0
        ? requestedSize
        : localRect.size();
    const QRect globalRect(target->mapToGlobal(localRect.topLeft()), localRect.size());

    QImage image = backdropImage(target, globalRect, targetSize);
    if (image.isNull()) {
        image = syntheticMaterial(targetSize, globalRect.topLeft(), isDark);
    }
    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

void OverlayAcrylicMaterial::paintOverlayBackdrop(QPainter& painter, QWidget* target, const QRectF& rect, qreal radius, bool isDark, int dimAlpha) {
    if (rect.isEmpty()) {
        return;
    }

    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    painter.save();
    painter.setClipPath(path);
    const QImage material = materialImage(target, rect.toAlignedRect(), isDark);
    if (!material.isNull()) {
        painter.drawImage(rect.topLeft(), material);
    }

    const QColor wash = isDark
        ? QColor(0, 0, 0, std::clamp(dimAlpha, 0, 255))
        : QColor(28, 28, 30, std::clamp(dimAlpha, 0, 255));
    painter.fillRect(rect, wash);

    QLinearGradient highlight(rect.topLeft(), rect.bottomLeft());
    highlight.setColorAt(0.0, QColor(255, 255, 255, isDark ? 18 : 28));
    highlight.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.fillRect(rect, highlight);
    painter.restore();
}

void OverlayAcrylicMaterial::paintPanel(QPainter& painter, QWidget* target, const QRectF& rect, qreal radius, bool isDark, bool elevated) {
    if (rect.isEmpty()) {
        return;
    }

    QPainterPath path;
    path.addRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    painter.save();
    painter.setClipPath(path);
    const QImage material = materialImage(target, rect.toAlignedRect(), isDark);
    if (!material.isNull()) {
        painter.drawImage(rect.topLeft(), material);
    }

    const QColor panelWash = isDark
        ? QColor(34, 34, 38, elevated ? 174 : 146)
        : QColor(255, 255, 255, elevated ? 168 : 138);
    painter.fillRect(rect, panelWash);

    QLinearGradient sheen(rect.topLeft(), rect.bottomLeft());
    sheen.setColorAt(0.0, QColor(255, 255, 255, isDark ? 38 : 110));
    sheen.setColorAt(0.34, QColor(255, 255, 255, isDark ? 12 : 46));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.fillRect(rect, sheen);
    painter.restore();

    const QColor border = isDark ? QColor(255, 255, 255, 38) : QColor(60, 60, 67, 42);
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
}

QString OverlayAcrylicMaterial::accentGlassBrush(bool isDark) {
    return isDark
        ? QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(108, 188, 255, 240), stop:0.42 rgba(10, 132, 255, 210), stop:1 rgba(0, 74, 190, 235))")
        : QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(98, 180, 255, 240), stop:0.45 rgba(0, 122, 255, 204), stop:1 rgba(0, 84, 210, 230))");
}

QString OverlayAcrylicMaterial::accentGlassHoverBrush(bool isDark) {
    return isDark
        ? QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(130, 203, 255, 250), stop:0.44 rgba(26, 148, 255, 230), stop:1 rgba(0, 86, 208, 245))")
        : QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 rgba(120, 194, 255, 250), stop:0.46 rgba(12, 134, 255, 224), stop:1 rgba(0, 94, 224, 245))");
}

void OverlayAcrylicMaterial::installLiveRefresh(QWidget* target, int intervalMs) {
    if (!target || intervalMs <= 0 || target->findChild<QTimer*>(QStringLiteral("__OverlayAcrylicLiveRefresh"), Qt::FindDirectChildrenOnly)) {
        return;
    }

    QTimer* timer = new QTimer(target);
    timer->setObjectName(QStringLiteral("__OverlayAcrylicLiveRefresh"));
    timer->setInterval(intervalMs);
    timer->setTimerType(Qt::CoarseTimer);
    QObject::connect(timer, &QTimer::timeout, target, [target]() {
        if (target->isVisible() && target->updatesEnabled()) {
            target->update();
        }
    });
    timer->start();
}

OverlayAcrylicPanel::OverlayAcrylicPanel(QWidget* parent)
    : QWidget(parent) {
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_StyledBackground, false);
    OverlayAcrylicMaterial::installLiveRefresh(this);
}

void OverlayAcrylicPanel::setAcrylicRadius(qreal radius) {
    m_radius = radius;
    update();
}

void OverlayAcrylicPanel::setElevatedAcrylic(bool elevated) {
    m_elevated = elevated;
    update();
}

void OverlayAcrylicPanel::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    OverlayAcrylicMaterial::paintPanel(
        painter,
        this,
        QRectF(rect()),
        m_radius,
        ConfigManager::instance().isCurrentThemeDark(),
        m_elevated);
}

OverlayAcrylicButton::OverlayAcrylicButton(Role role, QWidget* parent)
    : QPushButton(parent)
    , m_role(role) {
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_StyledBackground, false);
    OverlayAcrylicMaterial::installLiveRefresh(this);
}

void OverlayAcrylicButton::setAcrylicRole(Role role) {
    m_role = role;
    update();
}

void OverlayAcrylicButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    const bool isDark = ConfigManager::instance().isCurrentThemeDark();
    const bool hovered = underMouse();
    const bool pressed = isDown();
    const bool checked = isCheckable() && isChecked();
    const bool enabled = isEnabled();
    const QRectF buttonRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = 7.0;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addRoundedRect(buttonRect, radius, radius);

    painter.save();
    painter.setClipPath(path);
    const QImage material = OverlayAcrylicMaterial::materialImage(this, rect(), isDark);
    if (!material.isNull()) {
        painter.drawImage(QPointF(0.0, 0.0), material);
    }
    painter.fillRect(buttonRect, roleTint(m_role, isDark, enabled, hovered, pressed, checked));

    QLinearGradient sheen(buttonRect.topLeft(), buttonRect.bottomLeft());
    sheen.setColorAt(0.0, QColor(255, 255, 255, enabled ? 68 : 24));
    sheen.setColorAt(0.45, QColor(255, 255, 255, enabled ? 18 : 6));
    sheen.setColorAt(1.0, QColor(0, 0, 0, m_role == Role::Secondary ? 18 : 38));
    painter.fillRect(buttonRect, sheen);
    painter.restore();

    QColor border = isDark ? QColor(255, 255, 255, 42) : QColor(60, 60, 67, 40);
    if (m_role == Role::Accent && enabled) {
        border = QColor(160, 212, 255, hovered || checked ? 150 : 104);
    }
    painter.setPen(QPen(border, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    QStyleOptionButton option;
    option.initFrom(this);
    option.text = text();

    QFont buttonFont = font();
    buttonFont.setWeight(QFont::DemiBold);
    painter.setFont(buttonFont);
    painter.setPen(textColor(isDark, enabled, m_role));
    const int textMargin = 16;
    const QRect textRect = rect().adjusted(textMargin, 0, -textMargin, 0);
    const QString elided = QFontMetrics(buttonFont).elidedText(text(), Qt::ElideRight, textRect.width());
    painter.drawText(textRect, Qt::AlignCenter, elided);
}
