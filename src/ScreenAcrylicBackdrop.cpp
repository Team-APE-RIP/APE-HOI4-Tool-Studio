//-------------------------------------------------------------------------------------
// ScreenAcrylicBackdrop.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ScreenAcrylicBackdrop.h"

#include "AcrylicCudaProcessor.h"
#include "AcrylicScreenCapture.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPixmap>
#include <QScreen>
#include <QWindow>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

namespace {
constexpr int kRefreshIntervalMs = 8;
constexpr int kBlurPasses = 6;
constexpr int kMinimumGridWidth = 76;
constexpr int kMaximumGridWidth = 132;
constexpr int kMinimumGridHeight = 44;
constexpr int kMaximumGridHeight = 78;
constexpr qreal kDefaultChromeCornerRadius = 10.0;
constexpr qreal kChromeBorderWidth = 1.0;

int clampToByte(int value) {
    return std::clamp(value, 0, 255);
}

int weightedChannel(int c0, int c1, int c2, int c3, int c4) {
    return (c0 + c1 * 4 + c2 * 6 + c3 * 4 + c4 + 8) / 16;
}

QColor fallbackTint(bool isDark) {
    return isDark ? QColor(32, 32, 34) : QColor(245, 245, 247);
}

QPainterPath roundedRectPath(const QRectF& rect, qreal radius) {
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    return path;
}

QPainterPath roundedFramePath(const QRectF& outerRect, qreal radius, qreal borderWidth) {
    const qreal maxInset = std::min(outerRect.width(), outerRect.height()) / 2.0;
    const qreal inset = std::clamp(borderWidth, 0.0, maxInset);
    const QRectF innerRect = outerRect.adjusted(inset, inset, -inset, -inset);
    const qreal innerRadius = std::max<qreal>(0.0, radius - inset);

    QPainterPath frame;
    frame.setFillRule(Qt::OddEvenFill);
    frame.addRoundedRect(outerRect, radius, radius);
    if (!innerRect.isEmpty()) {
        frame.addRoundedRect(innerRect, innerRadius, innerRadius);
    }
    return frame;
}

void paintSymmetricRoundedFrame(QPainter& painter, const QRectF& outerRect, qreal radius, qreal borderWidth, const QColor& color) {
    if (outerRect.isEmpty() || borderWidth <= 0.0 || color.alpha() <= 0) {
        return;
    }

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPath(roundedFramePath(outerRect, radius, borderWidth));
    painter.restore();
}
}

ScreenAcrylicBackdrop::ScreenAcrylicBackdrop(QWidget *parent)
    : QWidget(parent)
    , m_tintColor(fallbackTint(false))
    , m_borderColor(QColor(60, 60, 67, 46))
    , m_chromeCornerRadius(kDefaultChromeCornerRadius) {
    setObjectName(QStringLiteral("ScreenAcrylicBackdrop"));
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    m_refreshTimer.setInterval(kRefreshIntervalMs);
    m_refreshTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this, &ScreenAcrylicBackdrop::refreshBackdrop);
}

ScreenAcrylicBackdrop::~ScreenAcrylicBackdrop() {
    restoreCaptureExclusion();
}

void ScreenAcrylicBackdrop::setDarkMode(bool isDark) {
    if (m_isDark == isDark)
        return;

    m_isDark = isDark;
    if (m_tintColor == fallbackTint(!isDark))
        m_tintColor = fallbackTint(isDark);
    refreshBackdrop();
}

void ScreenAcrylicBackdrop::setChromeColors(const QColor& tint, const QColor& border) {
    m_tintColor = tint;
    m_borderColor = border;
    update();
}

void ScreenAcrylicBackdrop::setChromeCornerRadius(qreal radius) {
    const qreal boundedRadius = std::max<qreal>(0.0, radius);
    if (qFuzzyCompare(m_chromeCornerRadius, boundedRadius)) {
        return;
    }

    m_chromeCornerRadius = boundedRadius;
    update();
}

QImage ScreenAcrylicBackdrop::acrylicImageForGlobalRect(const QRect& globalRect, const QSize& targetSize) const {
    if (globalRect.width() <= 0 || globalRect.height() <= 0
        || targetSize.width() <= 0 || targetSize.height() <= 0
        || width() <= 0 || height() <= 0) {
        return QImage();
    }

    const QPoint localTopLeft = mapFromGlobal(globalRect.topLeft());
    const QRect localRect(localTopLeft, globalRect.size());
    const QRect clipped = localRect.intersected(rect());
    if (clipped.isEmpty()) {
        return QImage();
    }

    QImage image(targetSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(fallbackTint(m_isDark));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QRectF targetRect(
        (clipped.left() - localRect.left()) * targetSize.width() / static_cast<qreal>(localRect.width()),
        (clipped.top() - localRect.top()) * targetSize.height() / static_cast<qreal>(localRect.height()),
        clipped.width() * targetSize.width() / static_cast<qreal>(localRect.width()),
        clipped.height() * targetSize.height() / static_cast<qreal>(localRect.height())
    );

    if (!m_acrylicGrid.isNull()) {
        const QRectF sourceRect(
            clipped.left() * m_acrylicGrid.width() / static_cast<qreal>(width()),
            clipped.top() * m_acrylicGrid.height() / static_cast<qreal>(height()),
            clipped.width() * m_acrylicGrid.width() / static_cast<qreal>(width()),
            clipped.height() * m_acrylicGrid.height() / static_cast<qreal>(height())
        );
        painter.drawImage(targetRect, m_acrylicGrid, sourceRect);
    } else {
        painter.fillRect(targetRect, m_tintColor);
    }

    const QColor wash = m_isDark ? QColor(20, 20, 22, 88) : QColor(255, 255, 255, 112);
    painter.fillRect(image.rect(), wash);
    return image;
}

void ScreenAcrylicBackdrop::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

    const QColor fallbackBase = m_isDark ? QColor(48, 48, 50) : QColor(232, 232, 237);
    painter.fillRect(rect(), fallbackBase);

    const QRectF chromeRect(QPointF(0.0, 0.0), QSizeF(width(), height()));
    painter.save();
    painter.setClipPath(roundedRectPath(chromeRect, m_chromeCornerRadius));

    if (!m_acrylicGrid.isNull()) {
        painter.drawImage(rect(), m_acrylicGrid);
    } else {
        painter.fillRect(rect(), m_tintColor);
    }

    const QColor wash = m_isDark ? QColor(20, 20, 22, 88) : QColor(255, 255, 255, 112);
    painter.fillRect(rect(), wash);
    painter.restore();

    paintSymmetricRoundedFrame(painter, chromeRect, m_chromeCornerRadius, kChromeBorderWidth, m_borderColor);
}

void ScreenAcrylicBackdrop::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    refreshBackdrop();
}

void ScreenAcrylicBackdrop::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    applyCaptureExclusion();
    if (!m_refreshTimer.isActive())
        m_refreshTimer.start();
    refreshBackdrop();
}

void ScreenAcrylicBackdrop::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    m_refreshTimer.stop();
    restoreCaptureExclusion();
}

void ScreenAcrylicBackdrop::refreshBackdrop() {
    if (!isVisible() || width() <= 0 || height() <= 0)
        return;

#ifdef Q_OS_WIN
    if (!m_captureExclusionActive && !m_captureExclusionUnavailable)
        applyCaptureExclusion();
#endif

    const QSize gridSize = gridSizeForCurrentWindow();
    QImage captured = captureScreenGrid(gridSize.width(), gridSize.height());
    if (captured.isNull()) {
        update();
        return;
    }

    m_acrylicGrid = processAcrylicGrid(captured);
    update();
}

QSize ScreenAcrylicBackdrop::gridSizeForCurrentWindow() const {
    const int gridWidth = std::clamp(width() / 8, kMinimumGridWidth, kMaximumGridWidth);
    const int gridHeight = std::clamp(height() / 8, kMinimumGridHeight, kMaximumGridHeight);
    return QSize(gridWidth, gridHeight);
}

QImage ScreenAcrylicBackdrop::captureScreenGrid(int gridWidth, int gridHeight) const {
    if (gridWidth <= 0 || gridHeight <= 0)
        return QImage();

    const QPoint topLeft = mapToGlobal(QPoint(0, 0));
    const QRect logicalRect(topLeft, size());
    QScreen *screen = windowHandle() ? windowHandle()->screen() : QGuiApplication::screenAt(logicalRect.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return QImage();

    const qreal dpr = screen->devicePixelRatio();
    const QImage dxgiImage = AcrylicScreenCapture::instance().captureGrid(
        topLeft,
        size(),
        dpr,
        QSize(gridWidth, gridHeight)
    );
    if (!dxgiImage.isNull()) {
        return dxgiImage;
    }

#ifdef Q_OS_WIN
    const int sourceX = qRound(topLeft.x() * dpr);
    const int sourceY = qRound(topLeft.y() * dpr);
    const int sourceWidth = std::max(1, qRound(width() * dpr));
    const int sourceHeight = std::max(1, qRound(height() * dpr));

    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = screenDc ? CreateCompatibleDC(screenDc) : nullptr;
    HBITMAP bitmap = memoryDc ? CreateCompatibleBitmap(screenDc, gridWidth, gridHeight) : nullptr;
    HGDIOBJ oldBitmap = bitmap ? SelectObject(memoryDc, bitmap) : nullptr;

    QImage image;
    if (screenDc && memoryDc && bitmap) {
        SetStretchBltMode(memoryDc, HALFTONE);
        SetBrushOrgEx(memoryDc, 0, 0, nullptr);
        if (StretchBlt(memoryDc, 0, 0, gridWidth, gridHeight,
                       screenDc, sourceX, sourceY, sourceWidth, sourceHeight, SRCCOPY)) {
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = gridWidth;
            info.bmiHeader.biHeight = -gridHeight;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;

            image = QImage(gridWidth, gridHeight, QImage::Format_ARGB32_Premultiplied);
            if (!GetDIBits(memoryDc, bitmap, 0, gridHeight, image.bits(), &info, DIB_RGB_COLORS))
                image = QImage();
        }
    }

    if (oldBitmap)
        SelectObject(memoryDc, oldBitmap);
    if (bitmap)
        DeleteObject(bitmap);
    if (memoryDc)
        DeleteDC(memoryDc);
    if (screenDc)
        ReleaseDC(nullptr, screenDc);

    if (!image.isNull())
        return image;
#endif

    const QPixmap pixmap = screen->grabWindow(0, logicalRect.x(), logicalRect.y(), logicalRect.width(), logicalRect.height());
    if (pixmap.isNull())
        return QImage();

    return pixmap.toImage()
        .convertToFormat(QImage::Format_ARGB32_Premultiplied)
        .scaled(gridWidth, gridHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage ScreenAcrylicBackdrop::processAcrylicGrid(const QImage& source) const {
    const int tintR = m_tintColor.red();
    const int tintG = m_tintColor.green();
    const int tintB = m_tintColor.blue();
    const int sourceWeight = m_isDark ? 62 : 58;
    const int saturationPercent = m_isDark ? 106 : 104;

    QImage cudaProcessed;
    if (AcrylicCudaProcessor::instance().process(
            source,
            &cudaProcessed,
            m_tintColor,
            sourceWeight,
            saturationPercent)) {
        return cudaProcessed;
    }

    QImage blurred = weightedBlur(source.convertToFormat(QImage::Format_ARGB32_Premultiplied), kBlurPasses);
    if (blurred.isNull())
        return blurred;

    const int tintWeight = 100 - sourceWeight;

    for (int y = 0; y < blurred.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(blurred.scanLine(y));
        for (int x = 0; x < blurred.width(); ++x) {
            const QRgb pixel = line[x];
            const int red = qRed(pixel);
            const int green = qGreen(pixel);
            const int blue = qBlue(pixel);
            const int gray = (red * 30 + green * 59 + blue * 11) / 100;
            int r = gray + (red - gray) * saturationPercent / 100;
            int g = gray + (green - gray) * saturationPercent / 100;
            int b = gray + (blue - gray) * saturationPercent / 100;

            r = (r * sourceWeight + tintR * tintWeight) / 100;
            g = (g * sourceWeight + tintG * tintWeight) / 100;
            b = (b * sourceWeight + tintB * tintWeight) / 100;

            line[x] = qRgba(clampToByte(r), clampToByte(g), clampToByte(b), 255);
        }
    }

    return blurred;
}

QImage ScreenAcrylicBackdrop::weightedBlur(const QImage& source, int passes) const {
    if (source.isNull())
        return QImage();

    QImage current = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (passes <= 0)
        return current;

    QImage temp(current.size(), QImage::Format_ARGB32_Premultiplied);
    QImage output(current.size(), QImage::Format_ARGB32_Premultiplied);

    const auto sample = [](const QImage& image, int x, int y) -> QRgb {
        x = std::clamp(x, 0, image.width() - 1);
        y = std::clamp(y, 0, image.height() - 1);
        return reinterpret_cast<const QRgb *>(image.constScanLine(y))[x];
    };

    for (int pass = 0; pass < passes; ++pass) {
        for (int y = 0; y < current.height(); ++y) {
            QRgb *target = reinterpret_cast<QRgb *>(temp.scanLine(y));
            for (int x = 0; x < current.width(); ++x) {
                const QRgb c0 = sample(current, x - 2, y);
                const QRgb c1 = sample(current, x - 1, y);
                const QRgb c2 = sample(current, x, y);
                const QRgb c3 = sample(current, x + 1, y);
                const QRgb c4 = sample(current, x + 2, y);
                target[x] = qRgb(
                    weightedChannel(qRed(c0), qRed(c1), qRed(c2), qRed(c3), qRed(c4)),
                    weightedChannel(qGreen(c0), qGreen(c1), qGreen(c2), qGreen(c3), qGreen(c4)),
                    weightedChannel(qBlue(c0), qBlue(c1), qBlue(c2), qBlue(c3), qBlue(c4)));
            }
        }

        for (int y = 0; y < temp.height(); ++y) {
            QRgb *target = reinterpret_cast<QRgb *>(output.scanLine(y));
            for (int x = 0; x < temp.width(); ++x) {
                const QRgb c0 = sample(temp, x, y - 2);
                const QRgb c1 = sample(temp, x, y - 1);
                const QRgb c2 = sample(temp, x, y);
                const QRgb c3 = sample(temp, x, y + 1);
                const QRgb c4 = sample(temp, x, y + 2);
                target[x] = qRgb(
                    weightedChannel(qRed(c0), qRed(c1), qRed(c2), qRed(c3), qRed(c4)),
                    weightedChannel(qGreen(c0), qGreen(c1), qGreen(c2), qGreen(c3), qGreen(c4)),
                    weightedChannel(qBlue(c0), qBlue(c1), qBlue(c2), qBlue(c3), qBlue(c4)));
            }
        }

        if (pass + 1 < passes)
            current.swap(output);
    }

    return output;
}

void ScreenAcrylicBackdrop::applyCaptureExclusion() {
#ifdef Q_OS_WIN
    if (m_captureExclusionUnavailable)
        return;

    QWidget *topLevel = window();
    HWND hwnd = topLevel ? reinterpret_cast<HWND>(topLevel->winId()) : nullptr;
    if (!hwnd)
        return;

    const quintptr windowId = reinterpret_cast<quintptr>(hwnd);
    if (m_captureExclusionActive && m_captureWindowId == windowId)
        return;
    if (m_captureExclusionActive)
        restoreCaptureExclusion();

    DWORD previousAffinity = WDA_NONE;
    m_hadDisplayAffinity = GetWindowDisplayAffinity(hwnd, &previousAffinity) == TRUE;
    m_previousDisplayAffinity = previousAffinity;
    m_captureExclusionActive = SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE) == TRUE;
    m_captureWindowId = m_captureExclusionActive ? windowId : 0;
    if (!m_captureExclusionActive)
        m_captureExclusionUnavailable = true;
#endif
}

void ScreenAcrylicBackdrop::restoreCaptureExclusion() {
#ifdef Q_OS_WIN
    if (!m_captureExclusionActive)
        return;

    HWND hwnd = reinterpret_cast<HWND>(m_captureWindowId);
    if (!hwnd) {
        QWidget *topLevel = window();
        hwnd = topLevel ? reinterpret_cast<HWND>(topLevel->winId()) : nullptr;
    }
    if (hwnd)
        SetWindowDisplayAffinity(hwnd, m_hadDisplayAffinity ? m_previousDisplayAffinity : WDA_NONE);

    m_captureExclusionActive = false;
    m_captureWindowId = 0;
    m_hadDisplayAffinity = false;
    m_previousDisplayAffinity = WDA_NONE;
#endif
}
