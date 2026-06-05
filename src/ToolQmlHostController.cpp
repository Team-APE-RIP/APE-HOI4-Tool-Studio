//-------------------------------------------------------------------------------------
// ToolQmlHostController.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolQmlHostController.h"

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QMouseEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QEnterEvent>
#include <QWheelEvent>

#include "ConfigManager.h"
#include "AcrylicCudaProcessor.h"
#include "AcrylicScreenCapture.h"
#include "Logger.h"
#include "ToolProxyInterface.h"
#include "ToolQmlBridge.h"
#include "ToolQmlHostComponents.h"
#include "ToolQmlThemeProvider.h"
#include "ToolUiContainer.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QPalette>
#include <QPixmap>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickWidget>
#include <QScreen>
#include <QSizePolicy>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QWindow>

#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class ToolQmlAcrylicImageProvider : public QQuickImageProvider {
public:
    ToolQmlAcrylicImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image) {
    }

    QImage requestImage(const QString&, QSize* size, const QSize& requestedSize) override {
        QMutexLocker locker(&m_mutex);
        QImage image = m_image;
        locker.unlock();

        if (image.isNull()) {
            image = QImage(8, 8, QImage::Format_ARGB32_Premultiplied);
            image.fill(QColor(42, 42, 44, 255));
        }
        if (size) {
            *size = image.size();
        }
        if (requestedSize.isValid() && requestedSize.width() > 0 && requestedSize.height() > 0) {
            return image.scaled(requestedSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        return image;
    }

    void setImage(const QImage& image) {
        QMutexLocker locker(&m_mutex);
        m_image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

private:
    QMutex m_mutex;
    QImage m_image;
};

//-------------------------------------------------------------------------------------
// DebugQuickWidget Implementation - Mouse Event Tracking
//-------------------------------------------------------------------------------------

DebugQuickWidget::DebugQuickWidget(QQmlEngine* engine, QWidget* parent)
    : QQuickWidget(engine, parent)
{
}

void DebugQuickWidget::logWidgetState(const QString& context) {
    Q_UNUSED(context);
}

bool DebugQuickWidget::event(QEvent* event) {
    return QQuickWidget::event(event);
}

void DebugQuickWidget::mousePressEvent(QMouseEvent* event) {
    QQuickWidget::mousePressEvent(event);
    event->accept();
}

void DebugQuickWidget::mouseReleaseEvent(QMouseEvent* event) {
    QQuickWidget::mouseReleaseEvent(event);
    event->accept();
}

void DebugQuickWidget::mouseMoveEvent(QMouseEvent* event) {
    QQuickWidget::mouseMoveEvent(event);
    event->accept();
}

void DebugQuickWidget::wheelEvent(QWheelEvent* event) {
    QQuickWidget::wheelEvent(event);
    event->accept();
}

void DebugQuickWidget::enterEvent(QEnterEvent* event) {
    QQuickWidget::enterEvent(event);
}

void DebugQuickWidget::leaveEvent(QEvent* event) {
    QQuickWidget::leaveEvent(event);
}

//-------------------------------------------------------------------------------------
// ToolQmlHostController Implementation
//-------------------------------------------------------------------------------------

namespace {
const char* kLogContext = "ToolQmlHostController";
constexpr bool kVerboseToolQmlHostLogging = false;
constexpr int kToolAcrylicRefreshIntervalMs = 8;
constexpr int kToolAcrylicBlurPasses = 5;
constexpr int kToolAcrylicMinimumGridWidth = 72;
constexpr int kToolAcrylicMaximumGridWidth = 144;
constexpr int kToolAcrylicMinimumGridHeight = 44;
constexpr int kToolAcrylicMaximumGridHeight = 96;

QString detectThemeFromApplicationPalette() {
    return ConfigManager::instance().isCurrentThemeDark()
        ? QStringLiteral("dark")
        : QStringLiteral("light");
}

QColor qmlSceneClearColor() {
    return ConfigManager::instance().isCurrentThemeDark()
        ? QColor(42, 42, 44, 255)
        : QColor(246, 246, 248, 255);
}

int clampToByte(int value) {
    return std::clamp(value, 0, 255);
}

int weightedChannel(int c0, int c1, int c2, int c3, int c4) {
    return (c0 + c1 * 4 + c2 * 6 + c3 * 4 + c4 + 8) / 16;
}

QSize acrylicGridSizeForWidget(const QWidget* widget) {
    if (!widget) {
        return QSize();
    }

    return QSize(
        std::clamp(widget->width() / 8, kToolAcrylicMinimumGridWidth, kToolAcrylicMaximumGridWidth),
        std::clamp(widget->height() / 8, kToolAcrylicMinimumGridHeight, kToolAcrylicMaximumGridHeight)
    );
}

QImage captureWidgetScreenGrid(const QWidget* widget, const QSize& gridSize) {
    if (!widget || gridSize.width() <= 0 || gridSize.height() <= 0 || widget->width() <= 0 || widget->height() <= 0) {
        return QImage();
    }

    const QPoint topLeft = widget->mapToGlobal(QPoint(0, 0));
    const QRect logicalRect(topLeft, widget->size());
    QScreen* screen = widget->windowHandle() ? widget->windowHandle()->screen() : QGuiApplication::screenAt(logicalRect.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return QImage();
    }

    const qreal dpr = screen->devicePixelRatio();
    const QImage dxgiImage = AcrylicScreenCapture::instance().captureGrid(
        topLeft,
        widget->size(),
        dpr,
        gridSize
    );
    if (!dxgiImage.isNull()) {
        return dxgiImage;
    }

#ifdef Q_OS_WIN
    const int sourceX = qRound(topLeft.x() * dpr);
    const int sourceY = qRound(topLeft.y() * dpr);
    const int sourceWidth = std::max(1, qRound(widget->width() * dpr));
    const int sourceHeight = std::max(1, qRound(widget->height() * dpr));

    HDC screenDc = GetDC(nullptr);
    HDC memoryDc = screenDc ? CreateCompatibleDC(screenDc) : nullptr;
    HBITMAP bitmap = memoryDc ? CreateCompatibleBitmap(screenDc, gridSize.width(), gridSize.height()) : nullptr;
    HGDIOBJ oldBitmap = bitmap ? SelectObject(memoryDc, bitmap) : nullptr;

    QImage image;
    if (screenDc && memoryDc && bitmap) {
        SetStretchBltMode(memoryDc, HALFTONE);
        SetBrushOrgEx(memoryDc, 0, 0, nullptr);
        if (StretchBlt(
                memoryDc,
                0,
                0,
                gridSize.width(),
                gridSize.height(),
                screenDc,
                sourceX,
                sourceY,
                sourceWidth,
                sourceHeight,
                SRCCOPY)) {
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = gridSize.width();
            info.bmiHeader.biHeight = -gridSize.height();
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;

            image = QImage(gridSize, QImage::Format_ARGB32_Premultiplied);
            if (!GetDIBits(memoryDc, bitmap, 0, gridSize.height(), image.bits(), &info, DIB_RGB_COLORS)) {
                image = QImage();
            }
        }
    }

    if (oldBitmap) {
        SelectObject(memoryDc, oldBitmap);
    }
    if (bitmap) {
        DeleteObject(bitmap);
    }
    if (memoryDc) {
        DeleteDC(memoryDc);
    }
    if (screenDc) {
        ReleaseDC(nullptr, screenDc);
    }

    if (!image.isNull()) {
        return image;
    }
#endif

    const QPixmap pixmap = screen->grabWindow(
        0,
        logicalRect.x(),
        logicalRect.y(),
        logicalRect.width(),
        logicalRect.height()
    );
    if (pixmap.isNull()) {
        return QImage();
    }

    return pixmap.toImage()
        .convertToFormat(QImage::Format_ARGB32_Premultiplied)
        .scaled(gridSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage blurAcrylicGrid(const QImage& source, int passes) {
    if (source.isNull()) {
        return QImage();
    }

    QImage current = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (passes <= 0) {
        return current;
    }

    QImage temp(current.size(), QImage::Format_ARGB32_Premultiplied);
    QImage output(current.size(), QImage::Format_ARGB32_Premultiplied);

    const auto sample = [](const QImage& image, int x, int y) -> QRgb {
        x = std::clamp(x, 0, image.width() - 1);
        y = std::clamp(y, 0, image.height() - 1);
        return reinterpret_cast<const QRgb*>(image.constScanLine(y))[x];
    };

    for (int pass = 0; pass < passes; ++pass) {
        for (int y = 0; y < current.height(); ++y) {
            QRgb* target = reinterpret_cast<QRgb*>(temp.scanLine(y));
            for (int x = 0; x < current.width(); ++x) {
                const QRgb c0 = sample(current, x - 2, y);
                const QRgb c1 = sample(current, x - 1, y);
                const QRgb c2 = sample(current, x, y);
                const QRgb c3 = sample(current, x + 1, y);
                const QRgb c4 = sample(current, x + 2, y);
                target[x] = qRgb(
                    weightedChannel(qRed(c0), qRed(c1), qRed(c2), qRed(c3), qRed(c4)),
                    weightedChannel(qGreen(c0), qGreen(c1), qGreen(c2), qGreen(c3), qGreen(c4)),
                    weightedChannel(qBlue(c0), qBlue(c1), qBlue(c2), qBlue(c3), qBlue(c4))
                );
            }
        }

        for (int y = 0; y < temp.height(); ++y) {
            QRgb* target = reinterpret_cast<QRgb*>(output.scanLine(y));
            for (int x = 0; x < temp.width(); ++x) {
                const QRgb c0 = sample(temp, x, y - 2);
                const QRgb c1 = sample(temp, x, y - 1);
                const QRgb c2 = sample(temp, x, y);
                const QRgb c3 = sample(temp, x, y + 1);
                const QRgb c4 = sample(temp, x, y + 2);
                target[x] = qRgb(
                    weightedChannel(qRed(c0), qRed(c1), qRed(c2), qRed(c3), qRed(c4)),
                    weightedChannel(qGreen(c0), qGreen(c1), qGreen(c2), qGreen(c3), qGreen(c4)),
                    weightedChannel(qBlue(c0), qBlue(c1), qBlue(c2), qBlue(c3), qBlue(c4))
                );
            }
        }

        if (pass + 1 < passes) {
            current.swap(output);
        }
    }

    return output;
}

QImage processToolAcrylicGrid(const QImage& source, bool isDark) {
    const QColor tint = isDark ? QColor(38, 38, 40) : QColor(246, 246, 248);
    const int sourceWeight = isDark ? 66 : 62;
    const int saturationPercent = isDark ? 106 : 104;

    QImage cudaProcessed;
    if (AcrylicCudaProcessor::instance().process(
            source,
            &cudaProcessed,
            tint,
            sourceWeight,
            saturationPercent)) {
        return cudaProcessed;
    }

    QImage blurred = blurAcrylicGrid(source, kToolAcrylicBlurPasses);
    if (blurred.isNull()) {
        return blurred;
    }

    const int tintWeight = 100 - sourceWeight;

    for (int y = 0; y < blurred.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(blurred.scanLine(y));
        for (int x = 0; x < blurred.width(); ++x) {
            const QRgb pixel = line[x];
            const int red = qRed(pixel);
            const int green = qGreen(pixel);
            const int blue = qBlue(pixel);
            const int gray = (red * 30 + green * 59 + blue * 11) / 100;

            int r = gray + (red - gray) * saturationPercent / 100;
            int g = gray + (green - gray) * saturationPercent / 100;
            int b = gray + (blue - gray) * saturationPercent / 100;

            r = (r * sourceWeight + tint.red() * tintWeight) / 100;
            g = (g * sourceWeight + tint.green() * tintWeight) / 100;
            b = (b * sourceWeight + tint.blue() * tintWeight) / 100;

            line[x] = qRgba(clampToByte(r), clampToByte(g), clampToByte(b), 255);
        }
    }

    return blurred;
}

QString effectivePageIdFromPacket(const ToolUiStatePacket& packet) {
    QString pageId = packet.pageId.trimmed();
    if (pageId.isEmpty()) {
        pageId = packet.topbarState.value(QStringLiteral("currentPageId")).toString().trimmed();
    }
    return pageId;
}

void insertStateAlias(QVariantMap* values, const QString& key, const QVariant& value) {
    if (!values || key.trimmed().isEmpty()) {
        return;
    }

    values->insert(key, value);
    values->insert(QStringLiteral("ui.%1").arg(key), value);
}

QString columnTextFromVariantMap(const QVariantMap& columnMap) {
    const QString text = columnMap.value(QStringLiteral("text")).toString();
    if (!text.trimmed().isEmpty()) {
        return text;
    }

    const QVariant titleValue = columnMap.value(QStringLiteral("title"));
    if (titleValue.metaType().id() == QMetaType::QString) {
        return titleValue.toString();
    }

    const QVariantMap titleMap = titleValue.toMap();
    const QString rawText = titleMap.value(QStringLiteral("rawText")).toString();
    if (!rawText.trimmed().isEmpty()) {
        return rawText;
    }

    return titleMap.value(QStringLiteral("text")).toString();
}

QString titleTextFromVariantMap(const QVariantMap& map) {
    const QVariant titleValue = map.value(QStringLiteral("title"));
    if (titleValue.metaType().id() == QMetaType::QString) {
        return titleValue.toString();
    }

    const QVariantMap titleMap = titleValue.toMap();
    const QString rawText = titleMap.value(QStringLiteral("rawText")).toString();
    if (!rawText.trimmed().isEmpty()) {
        return rawText;
    }

    const QString text = titleMap.value(QStringLiteral("text")).toString();
    if (!text.trimmed().isEmpty()) {
        return text;
    }

    return map.value(QStringLiteral("text")).toString();
}

QUrl toResolvedQmlUrl(const QString& path) {
    if (path.trimmed().isEmpty()) {
        return QUrl();
    }
    if (path.startsWith(QLatin1Char(':'))) {
        return QUrl(QStringLiteral("qrc%1").arg(path));
    }
    return QUrl::fromLocalFile(path);
}

QByteArray placeholderQmlSource() {
    return QByteArrayLiteral(
        "import QtQuick 2.15\n"
        "import QtQuick.Controls 2.15\n"
        "import APE.ToolHost 1.0\n"
        "Rectangle {\n"
        "    color: HostTheme.surfaces.window\n"
        "    anchors.fill: parent\n"
        "    Column {\n"
        "        anchors.centerIn: parent\n"
        "        spacing: HostTheme.spacing.lg\n"
        "        width: Math.min(parent.width * 0.72, 680)\n"
        "        Text {\n"
        "            anchors.horizontalCenter: parent.horizontalCenter\n"
        "            text: qsTr(\"QML tool host is active\")\n"
        "            color: HostTheme.colors.textPrimary\n"
        "            font.pixelSize: HostTheme.typography.heading\n"
        "            font.weight: HostTheme.typography.weightBold\n"
        "        }\n"
        "        Text {\n"
        "            width: parent.width\n"
        "            wrapMode: Text.WordWrap\n"
        "            horizontalAlignment: Text.AlignHCenter\n"
        "            text: qsTr(\"This tool has not provided a QML entry file yet. The host bridge, theme tokens, and worker communication channel are ready.\")\n"
        "            color: HostTheme.colors.textSecondary\n"
        "            font.pixelSize: HostTheme.typography.body\n"
        "        }\n"
        "        Rectangle {\n"
        "            anchors.horizontalCenter: parent.horizontalCenter\n"
        "            width: parent.width\n"
        "            radius: HostTheme.radius.lg\n"
        "            color: HostTheme.surfaces.surface\n"
        "            border.color: HostTheme.colors.border\n"
        "            border.width: 1\n"
        "            implicitHeight: stateColumn.implicitHeight + HostTheme.spacing.xl\n"
        "            Column {\n"
        "                id: stateColumn\n"
        "                anchors.fill: parent\n"
        "                anchors.margins: HostTheme.spacing.lg\n"
        "                spacing: HostTheme.spacing.sm\n"
        "                Text {\n"
        "                    text: qsTr(\"Current Page: %1\").arg(toolBridge.currentPage.length > 0 ? toolBridge.currentPage : qsTr(\"(none)\"))\n"
        "                    color: HostTheme.colors.textPrimary\n"
        "                    font.pixelSize: HostTheme.typography.subheading\n"
        "                }\n"
        "                Text {\n"
        "                    width: parent.width\n"
        "                    wrapMode: Text.WordWrap\n"
        "                    text: qsTr(\"Available state keys: %1\").arg(Object.keys(toolBridge.values).join(\", \"))\n"
        "                    color: HostTheme.colors.textMuted\n"
        "                    font.pixelSize: HostTheme.typography.caption\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "}\n"
    );
}
}

ToolQmlHostController::ToolQmlHostController(ToolProxyInterface* proxy, QObject* parent)
    : QObject(parent)
    , m_proxy(proxy) {
    m_acrylicRefreshTimer.setInterval(kToolAcrylicRefreshIntervalMs);
    m_acrylicRefreshTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_acrylicRefreshTimer, &QTimer::timeout, this, &ToolQmlHostController::refreshAcrylicTexture);
}

ToolQmlHostController::~ToolQmlHostController() {
    m_acrylicRefreshTimer.stop();
    m_activeQuickWidget = nullptr;
    if (m_proxy) {
        disconnect(m_proxy, nullptr, this, nullptr);
    }
    if (m_bridge) {
        disconnect(m_bridge, nullptr, this, nullptr);
    }
    if (m_activeView) {
        QWidget* view = m_activeView.data();
        m_activeView = nullptr;
        view->hide();
        view->setParent(nullptr);
        delete view;
    }
    if (m_component) {
        delete m_component;
        m_component = nullptr;
    }
}

bool ToolQmlHostController::initialize(const QString& toolDirectoryPath,
                                       const ToolGuiResourceDescriptor& descriptor,
                                       const QMap<QString, QString>& localizedStrings,
                                       QString* errorMessage) {
    if (!m_proxy) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("ToolQmlHostController has no bound tool proxy instance.");
        }
        return false;
    }

    // Check if already initialized with valid engine
    const bool alreadyInitialized = (m_engine != nullptr && m_bridge != nullptr && m_themeProvider != nullptr);
    
    m_toolDirectoryPath = toolDirectoryPath;
    m_guiFilePath = resolveGuiFilePath(descriptor);
    m_presetFilePath = resolvePresetFilePath(descriptor);
    m_localizedStrings = localizedStrings;

    // Only create new QML engine and components if not already initialized
    if (!alreadyInitialized) {
        if (m_engine) {
            delete m_engine;
            m_engine = nullptr;
            m_acrylicProvider = nullptr;
        }
        if (m_component) {
            delete m_component;
            m_component = nullptr;
        }
        if (m_bridge) {
            delete m_bridge;
            m_bridge = nullptr;
        }
        if (m_themeProvider) {
            delete m_themeProvider;
            m_themeProvider = nullptr;
        }

        ToolQmlHostComponents::registerTypes();

        m_engine = new QQmlEngine(this);
        m_acrylicProvider = new ToolQmlAcrylicImageProvider();
        m_engine->addImageProvider(QStringLiteral("apetoolacrylic"), m_acrylicProvider);
        m_bridge = new ToolQmlBridge(this);
        m_themeProvider = new ToolQmlThemeProvider(this);
        ToolQmlHostComponents::setSharedThemeProvider(m_themeProvider);

        connect(
            m_bridge,
            &ToolQmlBridge::actionRequested,
            this,
            [this](const QString& actionType, const QString& targetId, const QVariantMap& arguments) {
                handleBridgeAction(actionType, targetId, arguments);
            }
        );
        connect(m_bridge, &ToolQmlBridge::windowDragStarted, this, [this](const QPoint& globalPos) {
            beginWindowDrag(globalPos);
        });
        connect(m_bridge, &ToolQmlBridge::windowDragMoved, this, [this](const QPoint& globalPos) {
            updateWindowDragPosition(globalPos);
        });
        connect(m_bridge, &ToolQmlBridge::windowDragEnded, this, [this]() {
            finishWindowDrag();
        });
    } else {
        // Already initialized, just update localized strings and theme
        Logger::instance().logInfo("ToolQmlHostController", "Reusing existing QML engine and components");
    }

    // Always update localized strings and theme (may have changed)
    m_bridge->setLocalizedStrings(m_localizedStrings);
    applyTheme(QString());

    m_proxy->initializeWorkerSession();
    if (!m_proxy->isProcessRunning()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Worker session failed to become ready.");
        }
        return false;
    }
    m_lastStatePacket = m_proxy->initialUiState();
    applyStatePacket(m_lastStatePacket, false);

    if (!m_guiFilePath.trimmed().isEmpty()) {
        const QUrl qmlUrl = toResolvedQmlUrl(m_guiFilePath);
        if (!qmlUrl.isValid()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Invalid QML entry path: %1").arg(m_guiFilePath);
            }
            return false;
        }
    }

    return true;
}

QWidget* ToolQmlHostController::persistentView(QWidget* parent) {
    if (kVerboseToolQmlHostLogging) {
        Logger::instance().logInfo(kLogContext, QString("[EVENT_CHAIN] ===== persistentView called ====="));
        Logger::instance().logInfo(kLogContext, QString("[EVENT_CHAIN] parent=%1, parent->winId=%2, m_activeView=%3")
            .arg(reinterpret_cast<quintptr>(parent), 0, 16)
            .arg(parent ? parent->winId() : 0)
            .arg(m_activeView ? reinterpret_cast<quintptr>(m_activeView.data()) : 0, 0, 16));
    }
    
    if (kVerboseToolQmlHostLogging && parent) {
        Logger::instance().logInfo(kLogContext, QString("[EVENT_CHAIN] Parent widget state: enabled=%1, visible=%2, transparentForMouse=%3, focusPolicy=%4, windowFlags=%5")
            .arg(parent->isEnabled())
            .arg(parent->isVisible())
            .arg(parent->testAttribute(Qt::WA_TransparentForMouseEvents))
            .arg(parent->focusPolicy())
            .arg(static_cast<int>(parent->windowFlags()), 0, 16));
    }
    
    // If view already exists, return it directly without reparenting
    // Parent must be set correctly at creation time to avoid native handle rebuild
    if (m_activeView) {
        if (kVerboseToolQmlHostLogging) {
            Logger::instance().logInfo(kLogContext, QString("[EVENT_CHAIN] Returning existing m_activeView"));
            Logger::instance().logInfo(kLogContext, QString("[EVENT_CHAIN] BEFORE return - m_activeView state: enabled=%1, visible=%2, transparentForMouse=%3, focusPolicy=%4, windowFlags=%5, geometry=%6x%7+%8+%9")
                .arg(m_activeView->isEnabled())
                .arg(m_activeView->isVisible())
                .arg(m_activeView->testAttribute(Qt::WA_TransparentForMouseEvents))
                .arg(m_activeView->focusPolicy())
                .arg(static_cast<int>(m_activeView->windowFlags()), 0, 16)
                .arg(m_activeView->width())
                .arg(m_activeView->height())
                .arg(m_activeView->x())
                .arg(m_activeView->y()));
        }
        
        // CRITICAL: Check if attributes were unexpectedly changed
        if (m_activeView->testAttribute(Qt::WA_TransparentForMouseEvents)) {
            Logger::instance().logError(kLogContext, "[EVENT_CHAIN] !!!!! CRITICAL: WA_TransparentForMouseEvents is TRUE when it should be FALSE !!!!!");
            Logger::instance().logError(kLogContext, "[EVENT_CHAIN] This means the attribute was reset somewhere. Forcing it back to false.");
            m_activeView->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        }
        
        if (!m_activeView->isEnabled()) {
            Logger::instance().logError(kLogContext, "[EVENT_CHAIN] !!!!! CRITICAL: Widget is DISABLED when it should be ENABLED !!!!!");
            Logger::instance().logError(kLogContext, "[EVENT_CHAIN] Forcing it back to enabled.");
            m_activeView->setEnabled(true);
        }

        if (!m_activeQuickWidget) {
            m_activeQuickWidget = m_activeView->findChild<QQuickWidget*>();
        }
        if (m_activeQuickWidget && !m_acrylicRefreshTimer.isActive()) {
            m_acrylicRefreshTimer.start();
            QTimer::singleShot(0, this, &ToolQmlHostController::refreshAcrylicTexture);
        }
        
        return m_activeView;
    }

    if (kVerboseToolQmlHostLogging) {
        Logger::instance().logInfo(kLogContext, QString("[WINDOW_TRACE] Creating new QQuickWidget with ToolUiContainer wrapper, parent->winId=%1")
            .arg(parent ? parent->winId() : 0));
    }
    if (parent) {
        // Keep the native parent chain stable before the quick host is created.
        // The previous verbose trace did this implicitly through parent->winId().
        static_cast<void>(parent->winId());
    }
    
    // ARCHITECTURE FIX: Create ToolUiContainer first to isolate attributes.
    // MainWindow stays opaque; the wrapper only prevents accidental child
    // attributes from breaking input routing.
    ToolUiContainer* container = new ToolUiContainer(parent);
    // Create the native child handle before QQuickWidget setup. Older verbose logging
    // did this implicitly through parent->winId(); keeping it explicit prevents Qt
    // from rebuilding the top-level window while preparing the quick scene.
    static_cast<void>(container->winId());
    if (kVerboseToolQmlHostLogging) {
        Logger::instance().logInfo(kLogContext, QString("[ARCHITECTURE_FIX] Created ToolUiContainer: ptr=%1")
            .arg(reinterpret_cast<quintptr>(container), 0, 16));
    }
    
    // Create the persistent QQuickWidget with container as parent (not MainWindow's content area).
    // This keeps the native child chain stable while the parent renders acrylic.
    DebugQuickWidget* quickWidget = createQuickWidget(container);
    if (!quickWidget) {
        container->deleteLater();
        return nullptr;
    }
    
    // Set the QQuickWidget as the container's tool widget
    container->setToolWidget(quickWidget);
    if (kVerboseToolQmlHostLogging) {
        Logger::instance().logInfo(kLogContext, QString("[ARCHITECTURE_FIX] QQuickWidget wrapped in container"));
    }
    
    if (kVerboseToolQmlHostLogging) {
        Logger::instance().logInfo(kLogContext, QString("[WINDOW_TRACE] QQuickWidget created, parent->winId=%1")
            .arg(container ? container->winId() : 0));
    }

    if (m_component) {
        delete m_component;
        m_component = nullptr;
    }

    QUrl sourceUrl = toResolvedQmlUrl(m_guiFilePath);
    bool usePlaceholderSource = sourceUrl.isEmpty();
    if (!usePlaceholderSource && sourceUrl.isLocalFile()) {
        const QFileInfo fileInfo(sourceUrl.toLocalFile());
        if (!fileInfo.exists() || fileInfo.suffix().compare(QStringLiteral("qml"), Qt::CaseInsensitive) != 0) {
            usePlaceholderSource = true;
        }
    }

    QObject* rootObject = nullptr;
    if (usePlaceholderSource) {
        sourceUrl = QUrl(QStringLiteral("inline:/APE/ToolPlaceholder.qml"));
        m_component = new QQmlComponent(m_engine, this);
        m_component->setData(placeholderQmlSource(), sourceUrl);
        rootObject = m_component->create(quickWidget->rootContext());
    } else {
        m_component = new QQmlComponent(m_engine, sourceUrl, this);
        rootObject = m_component->create(quickWidget->rootContext());
    }

    if (!rootObject) {
        QStringList errors;
        for (const QQmlError& error : m_component->errors()) {
            errors.append(error.toString());
        }
        Logger::instance().logError(
            kLogContext,
            QStringLiteral("Failed to instantiate QML view for %1: %2")
                .arg(m_proxy->id(), errors.join(QStringLiteral(" | ")))
        );
        quickWidget->deleteLater();
        container->deleteLater();
        return nullptr;
    }

    quickWidget->setContent(sourceUrl, m_component, rootObject);
    
    // CRITICAL FIX: Ensure QML root object is enabled for mouse events
    // QML items can have enabled property that blocks mouse events
    if (rootObject) {
        if (kVerboseToolQmlHostLogging) {
            QVariant enabledProp = rootObject->property("enabled");
            Logger::instance().logInfo(kLogContext, QString("[UI_INTERACTION_DEBUG] QML rootObject enabled property: %1")
                .arg(enabledProp.isValid() ? enabledProp.toString() : "not set"));
        }
        
        // Force enable the root object
        rootObject->setProperty("enabled", true);
        
        if (kVerboseToolQmlHostLogging) {
            Logger::instance().logInfo(kLogContext, QString("[UI_INTERACTION_DEBUG] QML rootObject enabled property after force set: %1")
                .arg(rootObject->property("enabled").toString()));
        }
    }
    
    // ARCHITECTURE FIX: Store the container (not the QQuickWidget) as m_activeView
    // This ensures the attribute-isolating container is what gets returned
    m_activeView = container;
    m_activeQuickWidget = quickWidget;
    connect(quickWidget, &QObject::destroyed, this, [this, quickWidget]() {
        if (m_activeQuickWidget == quickWidget) {
            m_activeQuickWidget = nullptr;
            m_acrylicRefreshTimer.stop();
        }
    });
    if (!m_acrylicRefreshTimer.isActive()) {
        m_acrylicRefreshTimer.start();
    }
    QTimer::singleShot(0, this, &ToolQmlHostController::refreshAcrylicTexture);
    
    if (kVerboseToolQmlHostLogging) {
        Logger::instance().logInfo(kLogContext,
            QStringLiteral("[ARCHITECTURE_FIX] Created persistent QML view with ToolUiContainer wrapper for %1").arg(m_proxy->id()));
        Logger::instance().logInfo(kLogContext, QString("[ARCHITECTURE_FIX] Returning container: ptr=%1, transparentForMouse=%2")
            .arg(reinterpret_cast<quintptr>(container), 0, 16)
            .arg(container->testAttribute(Qt::WA_TransparentForMouseEvents)));
    }
    
    return container;
}

void ToolQmlHostController::setLocalizedStrings(const QMap<QString, QString>& strings) {
    const bool stringsChanged = m_localizedStrings != strings;
    m_localizedStrings = strings;
    if (m_bridge) {
        m_bridge->setLocalizedStrings(m_localizedStrings);
    }
    if (stringsChanged) {
        queueActiveViewRefresh(QStringLiteral("localized strings changed"));
    }
}

void ToolQmlHostController::applyStatePacket(const ToolUiStatePacket& packet, bool emitPageSignal) {
    m_lastStatePacket = packet;
    if (kVerboseToolQmlHostLogging) {
        Logger::instance().logInfo(
            kLogContext,
            QStringLiteral("[STATE_CHAIN] Applying state packet: page=%1 mode=%2 listModels=%3 sidebarVisible=%4 activeMode=%5")
                .arg(packet.pageId)
                .arg(packet.modeId)
                .arg(packet.listModels.size())
                .arg(packet.sidebarState.value(QStringLiteral("visible")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
                .arg(packet.sidebarState.value(QStringLiteral("activeMode")).toString())
        );
    }
    updateCurrentState(buildStateSnapshot(packet), emitPageSignal);
}

void ToolQmlHostController::applyTheme(const QString& themeName) {
    m_currentTheme = normalizeThemeName(themeName);
    if (m_themeProvider) {
        m_themeProvider->setTheme(m_currentTheme);
    }
    if (m_bridge) {
        m_bridge->setTheme(m_currentTheme);
    }
    if (m_activeView) {
        QQuickWidget* quickWidget = qobject_cast<QQuickWidget*>(m_activeView.data());
        if (!quickWidget) {
            quickWidget = m_activeView->findChild<QQuickWidget*>();
        }
        if (quickWidget) {
            quickWidget->setClearColor(qmlSceneClearColor());
            if (QQuickWindow* quickWindow = quickWidget->quickWindow()) {
                quickWindow->setColor(qmlSceneClearColor());
            }
            quickWidget->update();
        }
    }
}

void ToolQmlHostController::dispatchAction(const QString& actionType,
                                           const QString& targetId,
                                           const QVariantMap& arguments) {
    handleBridgeAction(actionType, targetId, arguments);
}

bool ToolQmlHostController::invokeTopbarShortcut(const QString& actionId) {
    const QString trimmedActionId = actionId.trimmed();
    if (trimmedActionId.isEmpty()) {
        return false;
    }

    QObject* rootObject = m_activeQuickWidget ? m_activeQuickWidget->rootObject() : nullptr;
    if (!rootObject && m_activeView) {
        if (QQuickWidget* quickWidget = m_activeView->findChild<QQuickWidget*>()) {
            rootObject = quickWidget->rootObject();
        }
    }

    if (rootObject) {
        const QVariant actionValue(trimmedActionId);
        if (QMetaObject::invokeMethod(rootObject, "applyTopbarAction", Q_ARG(QVariant, actionValue))) {
            return true;
        }
    }

    QVariantMap arguments;
    arguments.insert(QStringLiteral("actionId"), trimmedActionId);
    dispatchAction(QStringLiteral("button_click"), trimmedActionId, arguments);
    return true;
}

void ToolQmlHostController::mergeStatePacket(const QJsonObject& statePacket, bool emitPageSignal) {
    applyStatePacket(statePacketFromJson(statePacket), emitPageSignal);
}

void ToolQmlHostController::setCurrentPage(const QString& pageId, bool emitPageSignal) {
    const QString trimmedPageId = pageId.trimmed();
    if (trimmedPageId.isEmpty()) {
        return;
    }

    ToolGuiStateSnapshot snapshot = m_currentState;
    snapshot.currentPage = trimmedPageId;
    insertStateAlias(&snapshot.values, QStringLiteral("currentPage"), trimmedPageId);
    updateCurrentState(snapshot, emitPageSignal);
}

ToolGuiStateSnapshot ToolQmlHostController::buildStateSnapshot(const ToolUiStatePacket& packet) const {
    ToolGuiStateSnapshot snapshot;
    snapshot.currentPage = effectivePageIdFromPacket(packet);
    snapshot.values = packet.viewState;

    for (auto it = packet.runtimeVariables.constBegin(); it != packet.runtimeVariables.constEnd(); ++it) {
        insertStateAlias(&snapshot.values, it.key(), it.value());
    }

    if (!snapshot.currentPage.isEmpty()) {
        insertStateAlias(&snapshot.values, QStringLiteral("currentPage"), snapshot.currentPage);
    }
    if (!packet.modeId.trimmed().isEmpty()) {
        insertStateAlias(&snapshot.values, QStringLiteral("modeId"), packet.modeId);
    }
    if (packet.sidebarState.contains(QStringLiteral("title"))) {
        insertStateAlias(&snapshot.values, QStringLiteral("sidebarTitle"), packet.sidebarState.value(QStringLiteral("title")));
    }
    if (packet.sidebarState.contains(QStringLiteral("activeMode"))) {
        insertStateAlias(&snapshot.values, QStringLiteral("sidebarMode"), packet.sidebarState.value(QStringLiteral("activeMode")));
    }
    if (packet.topbarState.contains(QStringLiteral("activeFunction"))) {
        insertStateAlias(&snapshot.values, QStringLiteral("activeFunction"), packet.topbarState.value(QStringLiteral("activeFunction")));
    }

    for (const QVariant& modelValue : packet.listModels) {
        const QVariantMap modelMap = modelValue.toMap();
        const ToolGuiCollectionModel model = buildCollectionModel(modelMap);
        if (!model.id.trimmed().isEmpty()) {
            snapshot.models.insert(model.id, model);
        }
    }

    return snapshot;
}

ToolGuiCollectionModel ToolQmlHostController::buildCollectionModel(const QVariantMap& modelMap) const {
    ToolGuiCollectionModel model;
    model.id = modelMap.value(QStringLiteral("id")).toString();
    model.title = titleTextFromVariantMap(modelMap).trimmed();
    model.headerHidden = modelMap.value(QStringLiteral("headerHidden"), false).toBool();
    model.selectionMode = modelMap.value(QStringLiteral("selectionMode")).toString();
    model.contextActions = modelMap.value(QStringLiteral("contextActions")).toList();

    const QVariantList columns = modelMap.value(QStringLiteral("columns")).toList();
    for (const QVariant& columnValue : columns) {
        const QVariantMap columnMap = columnValue.toMap();
        ToolGuiListColumn column;
        column.key = columnMap.value(QStringLiteral("key")).toString();
        if (column.key.trimmed().isEmpty()) {
            column.key = columnMap.value(QStringLiteral("id")).toString();
        }
        column.text = columnTextFromVariantMap(columnMap);
        column.width = columnMap.value(QStringLiteral("width"), -1).toInt();
        column.stretch = columnMap.value(QStringLiteral("stretch")).toBool();
        column.hidden = columnMap.value(QStringLiteral("hidden")).toBool();
        model.columns.append(column);
    }

    const QVariantList rows = modelMap.value(QStringLiteral("rows")).toList();
    for (const QVariant& rowValue : rows) {
        const QVariantMap rowMap = rowValue.toMap();
        ToolGuiListRow row;
        row.id = rowMap.value(QStringLiteral("id")).toString();
        if (row.id.trimmed().isEmpty()) {
            row.id = rowMap.value(QStringLiteral("rowId")).toString();
        }
        row.rowId = row.id;
        row.role = rowMap.value(QStringLiteral("role")).toString();
        row.values = rowMap.value(QStringLiteral("values")).toMap();
        row.state = rowMap.value(QStringLiteral("state")).toMap();

        const QVariantList cells = rowMap.value(QStringLiteral("cells")).toList();
        if (!cells.isEmpty()) {
            for (const QVariant& cellValue : cells) {
                const QVariantMap cellMap = cellValue.toMap();
                ToolGuiCellValue cell;
                cell.value = cellMap.value(QStringLiteral("value"));
                cell.role = cellMap.value(QStringLiteral("role")).toString();
                row.cells.append(cell);
            }
        } else {
            for (const ToolGuiListColumn& column : model.columns) {
                ToolGuiCellValue cell;
                cell.value = row.values.value(column.key);
                row.cells.append(cell);
            }
        }

        model.rows.append(row);
    }

    const QVariant selectionVariant = modelMap.value(QStringLiteral("selection"));
    if (selectionVariant.metaType().id() == QMetaType::QStringList) {
        model.selection = selectionVariant.toStringList();
    } else {
        const QVariantList selectionList = selectionVariant.toList();
        for (const QVariant& value : selectionList) {
            const QString selectedId = value.toString().trimmed();
            if (!selectedId.isEmpty()) {
                model.selection.append(selectedId);
            }
        }
    }

    return model;
}

ToolUiStatePacket ToolQmlHostController::statePacketFromJson(const QJsonObject& statePacket) const {
    ToolUiStatePacket packet;
    packet.pageId = statePacket.value(QStringLiteral("pageId")).toString().trimmed();
    if (packet.pageId.isEmpty()) {
        packet.pageId = statePacket.value(QStringLiteral("currentPage")).toString().trimmed();
    }
    packet.modeId = statePacket.value(QStringLiteral("modeId")).toString().trimmed();
    packet.viewState = statePacket.value(QStringLiteral("viewState")).toObject().toVariantMap();
    if (packet.viewState.isEmpty()) {
        packet.viewState = statePacket.value(QStringLiteral("values")).toObject().toVariantMap();
    }
    packet.sidebarState = statePacket.value(QStringLiteral("sidebarState")).toObject().toVariantMap();
    packet.topbarState = statePacket.value(QStringLiteral("topbarState")).toObject().toVariantMap();
    packet.runtimeVariables = statePacket.value(QStringLiteral("runtimeVariables")).toObject().toVariantMap();
    if (statePacket.value(QStringLiteral("listModels")).isArray()) {
        packet.listModels = statePacket.value(QStringLiteral("listModels")).toArray().toVariantList();
    } else {
        const QJsonObject modelsObject = statePacket.value(QStringLiteral("models")).toObject();
        for (auto it = modelsObject.begin(); it != modelsObject.end(); ++it) {
            QVariantMap modelMap = it.value().toObject().toVariantMap();
            if (!modelMap.contains(QStringLiteral("id"))) {
                modelMap.insert(QStringLiteral("id"), it.key());
            }
            packet.listModels.append(modelMap);
        }
    }
    if (statePacket.value(QStringLiteral("patches")).isArray()) {
        packet.patches = statePacket.value(QStringLiteral("patches")).toArray().toVariantList();
    }
    return packet;
}

QString ToolQmlHostController::normalizeThemeName(const QString& theme) const {
    const QString normalized = theme.trimmed().toLower();
    if (normalized == QStringLiteral("light") || normalized == QStringLiteral("dark")) {
        return normalized;
    }
    return detectThemeFromApplicationPalette();
}

QString ToolQmlHostController::resolveGuiFilePath(const ToolGuiResourceDescriptor& descriptor) const {
    const QString entryFile = descriptor.entryFile.trimmed();
    if (!entryFile.isEmpty()) {
        if (entryFile.startsWith(QLatin1Char(':'))) {
            return entryFile;
        }

        const QFileInfo fileInfo(entryFile);
        if (fileInfo.isAbsolute()) {
            return fileInfo.absoluteFilePath();
        }

        return QDir(m_toolDirectoryPath).filePath(entryFile);
    }

    const QJsonObject metaData = m_proxy ? m_proxy->metaData() : QJsonObject();
    const QString toolName = metaData.value("name").toString(m_proxy ? m_proxy->name() : QString()).trimmed();
    if (toolName.isEmpty()) {
        return QString();
    }

    const QString candidatePath = QDir(m_toolDirectoryPath).filePath(toolName + QStringLiteral(".qml"));
    const QFileInfo candidateInfo(candidatePath);
    if (!candidateInfo.exists() || !candidateInfo.isFile()) {
        return QString();
    }

    return candidateInfo.absoluteFilePath();
}

QString ToolQmlHostController::resolvePresetFilePath(const ToolGuiResourceDescriptor& descriptor) const {
    const QString presetFile = descriptor.presetFile.trimmed();
    if (presetFile.isEmpty()) {
        return QString();
    }
    if (presetFile.startsWith(QLatin1Char(':'))) {
        return presetFile;
    }

    const QFileInfo fileInfo(presetFile);
    if (fileInfo.isAbsolute()) {
        return fileInfo.absoluteFilePath();
    }

    return QDir(m_toolDirectoryPath).filePath(presetFile);
}

DebugQuickWidget* ToolQmlHostController::createQuickWidget(QWidget* parent) {
    if (!m_engine) {
        m_engine = new QQmlEngine(this);
        m_acrylicProvider = new ToolQmlAcrylicImageProvider();
        m_engine->addImageProvider(QStringLiteral("apetoolacrylic"), m_acrylicProvider);
    }

    if (kVerboseToolQmlHostLogging) {
        Logger::instance().logInfo(kLogContext, QString("[UI_INTERACTION_DEBUG] createQuickWidget called, parent=%1, parent->winId=%2")
            .arg(reinterpret_cast<quintptr>(parent), 0, 16)
            .arg(parent ? parent->winId() : 0));
    }

    DebugQuickWidget* quickWidget = new DebugQuickWidget(m_engine, parent);
    quickWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // CRITICAL FIX: QQuickWidget must never use WA_TranslucentBackground.
    // A transparent FBO clears to black in this host chain, so the scene clears
    // to the shared content material color and QML surfaces add local glass.
    quickWidget->setAttribute(Qt::WA_TranslucentBackground, false);
    quickWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    quickWidget->setAttribute(Qt::WA_OpaquePaintEvent, false);
    
    quickWidget->setAutoFillBackground(true);
    
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setClearColor(qmlSceneClearColor());
    if (QQuickWindow* quickWindow = quickWidget->quickWindow()) {
        quickWindow->setColor(qmlSceneClearColor());
    }
    quickWidget->setFocusPolicy(Qt::StrongFocus);
    quickWidget->setMouseTracking(true);
    
    if (kVerboseToolQmlHostLogging) {
        Logger::instance().logInfo(kLogContext, QString("[UI_INTERACTION_DEBUG] DebugQuickWidget created with initial state:"));
        Logger::instance().logInfo(kLogContext, QString("[UI_INTERACTION_DEBUG]   enabled=%1, visible=%2, transparentForMouse=%3, focusPolicy=%4")
            .arg(quickWidget->isEnabled())
            .arg(quickWidget->isVisible())
            .arg(quickWidget->testAttribute(Qt::WA_TransparentForMouseEvents))
            .arg(quickWidget->focusPolicy()));
        Logger::instance().logInfo(kLogContext, QString("[UI_INTERACTION_DEBUG]   windowFlags=%1, geometry=%2x%3+%4+%5")
            .arg(static_cast<int>(quickWidget->windowFlags()), 0, 16)
            .arg(quickWidget->width())
            .arg(quickWidget->height())
            .arg(quickWidget->x())
            .arg(quickWidget->y()));
    }
    
    prepareQmlContext(quickWidget->rootContext());

    // CRITICAL: Ensure widget is visible to receive mouse events
    // Invisible widgets are skipped by Qt's mouse event dispatch system
    quickWidget->setVisible(true);
    quickWidget->show();

    return quickWidget;
}

void ToolQmlHostController::prepareQmlContext(QQmlContext* context) {
    if (!context) {
        return;
    }

    context->setContextProperty(QStringLiteral("toolBridge"), m_bridge);
    context->setContextProperty(QStringLiteral("toolTheme"), m_themeProvider);
    context->setContextProperty(QStringLiteral("toolId"), m_proxy ? m_proxy->id() : QString());
}

void ToolQmlHostController::handleBridgeAction(const QString& actionType,
                                               const QString& targetId,
                                               const QVariantMap& arguments) {
    if (!m_proxy || actionType.trimmed().isEmpty()) {
        return;
    }

    ToolUiActionRequest request;
    request.actionType = actionType;
    request.targetId = targetId;
    request.arguments = arguments;
    if (!request.targetId.trimmed().isEmpty() && !request.arguments.contains(QStringLiteral("targetId"))) {
        request.arguments.insert(QStringLiteral("targetId"), request.targetId);
    }

    applyStatePacket(m_proxy->handleUiAction(request), true);
}

QWidget* ToolQmlHostController::topLevelHostWindow() const {
    QWidget* hostWidget = m_activeView.data();
    return hostWidget ? hostWidget->window() : nullptr;
}

void ToolQmlHostController::beginWindowDrag(const QPoint& globalPos) {
    QWidget* window = topLevelHostWindow();
    if (!window) {
        return;
    }

    m_windowDragActive = true;
    m_windowDragOffset = globalPos - window->frameGeometry().topLeft();

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (hwnd) {
        ReleaseCapture();
        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return;
    }
#endif

    if (QWindow* windowHandle = window->windowHandle()) {
        windowHandle->startSystemMove();
    }
}

void ToolQmlHostController::updateWindowDragPosition(const QPoint& globalPos) {
    if (!m_windowDragActive) {
        return;
    }

    QWidget* window = topLevelHostWindow();
    if (!window) {
        m_windowDragActive = false;
        return;
    }

    window->move(globalPos - m_windowDragOffset);
}

void ToolQmlHostController::finishWindowDrag() {
    m_windowDragActive = false;
}

void ToolQmlHostController::queueActiveViewRefresh(const QString& reason) {
    if (!m_activeView) {
        return;
    }

    Logger::instance().logInfo(
        kLogContext,
        QStringLiteral("Queueing active QML view refresh: %1").arg(reason)
    );

    QPointer<QWidget> view = m_activeView;
    QTimer::singleShot(0, this, [view]() {
        QWidget* activeWidget = view.data();
        if (!activeWidget) {
            return;
        }

        QWidget* toolWidget = activeWidget;
        if (ToolUiContainer* container = qobject_cast<ToolUiContainer*>(activeWidget)) {
            toolWidget = container->toolWidget();
        }

        QQuickWidget* quickWidget = qobject_cast<QQuickWidget*>(toolWidget);
        if (!quickWidget && activeWidget) {
            quickWidget = activeWidget->findChild<QQuickWidget*>();
        }

        if (quickWidget) {
            if (QQuickItem* rootItem = quickWidget->rootObject()) {
                rootItem->update();
            }
            if (QQuickWindow* quickWindow = quickWidget->quickWindow()) {
                quickWindow->update();
            }
            quickWidget->update();
        }

        if (toolWidget && toolWidget != activeWidget) {
            toolWidget->update();
        }
        activeWidget->update();
    });
}

void ToolQmlHostController::refreshAcrylicTexture() {
    if (!m_acrylicProvider || !m_bridge) {
        return;
    }

    QQuickWidget* quickWidget = m_activeQuickWidget.data();
    if (!quickWidget && m_activeView) {
        quickWidget = m_activeView->findChild<QQuickWidget*>();
        m_activeQuickWidget = quickWidget;
    }
    if (!quickWidget || !quickWidget->isVisible() || quickWidget->width() <= 1 || quickWidget->height() <= 1) {
        return;
    }

    const QSize gridSize = acrylicGridSizeForWidget(quickWidget);
    const QImage captured = captureWidgetScreenGrid(quickWidget, gridSize);
    if (captured.isNull()) {
        return;
    }

    const QImage acrylic = processToolAcrylicGrid(captured, ConfigManager::instance().isCurrentThemeDark());
    if (acrylic.isNull()) {
        return;
    }

    m_acrylicProvider->setImage(acrylic);
    m_bridge->setAcrylicRevision(++m_acrylicRevision);

    if (QQuickItem* rootItem = quickWidget->rootObject()) {
        rootItem->update();
    }
    if (QQuickWindow* quickWindow = quickWidget->quickWindow()) {
        quickWindow->update();
    }
    quickWidget->update();
}

void ToolQmlHostController::updateCurrentState(const ToolGuiStateSnapshot& snapshot, bool emitPageSignal) {
    const QString previousPage = m_currentState.currentPage;
    m_currentState = snapshot;

    if (m_bridge) {
        m_bridge->setStateSnapshot(m_currentState);
    }

    emit stateChanged();

    if (emitPageSignal && !m_currentState.currentPage.trimmed().isEmpty() && m_currentState.currentPage != previousPage) {
        emit pageChanged(m_currentState.currentPage);
    }
}
