//-------------------------------------------------------------------------------------
// MainWindow.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "MainWindow.h"
#include "AgreementEvidenceManager.h"
#include "ApiRequests.h"
#include "ConfigManager.h"
#include "CustomMessageBox.h"
#include "ExternalPackageManager.h"
#include "LocalizationManager.h"
#include "PackageRegistry.h"
#include "PathValidator.h"
#include "OverlayAcrylicMaterial.h"
#include "ScreenAcrylicBackdrop.h"
#include "SetupDialog.h"
#include "FileManager.h"
#include "ToolManager.h"
#include "ToolProxyInterface.h"
#include "PluginManager.h"
#include "PluginAbiBroker.h"
#include "Logger.h"
#include "AuthManager.h"
#include "HttpClient.h"
#include "ToolRuntimeContext.h"
#include "ToolScriptedHostController.h"
#include "ToolUiContainer.h"
#include "WindowAviRecorder.h"
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFrame>
#include <QDebug>
#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QEventLoop>
#include <QStyle>
#include <QStyleOption>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QCloseEvent>
#include <QShortcut>
#include <QKeySequence>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFile>
#include <QTextStream>
#include <QPixmap>
#include <QThread>
#include <QProcess>
#include <QLineEdit>
#include <QComboBox>
#include <QTreeWidget>
#include <QHeaderView>
#include <QToolTip>
#include <QSignalBlocker>
#include <QAbstractItemView>
#include <QMenu>
#include <QByteArray>
#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QScreen>
#include <QQuickItem>
#include <QSizePolicy>
#include <QUrl>
#include <QWindow>
#include <QScopedValueRollback>
#include <QMimeData>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <string>
#include <windows.h>
#include <mmsystem.h>
#include <psapi.h>
#include <shellapi.h>

namespace {
constexpr bool kDebugForceConnectionWarning = false;
constexpr bool kVerboseToolUiLogging = false;
constexpr bool kTempWindowIdTrace = false;
constexpr int kRightSidebarRailWidth = 60;
constexpr int kRightSidebarDefaultListWidth = 190;
constexpr int kRightSidebarMaximumWidth = 440;
constexpr int kRightSidebarMinimumListWidth = 0;
static_assert(kRightSidebarDefaultListWidth > kRightSidebarMinimumListWidth, "Right sidebar default width must display the managed list.");
constexpr int kRightSidebarResizeHandleWidth = 6;
constexpr int kRightSidebarTitleBarHeight = 40;
constexpr const char* kRightSidebarDefaultButtonKey = "__default__";
constexpr const char* kRightSidebarSearchButtonKey = "__search__";
constexpr UINT_PTR kScreenshotNotificationIconId = 1001;
constexpr UINT_PTR kRecordingNotificationIconId = 1002;
constexpr qreal kWindowScreenshotCornerRadius = 10.0;
constexpr int kWindowScreenshotMaskSamples = 8;
constexpr int kWindowRecordingFrameRate = 10;

std::wstring toNullTerminatedWideText(const QString& text, int maxCharacters) {
    const QString trimmed = text.left(qMax(0, maxCharacters - 1));
    return trimmed.toStdWString();
}

void copyNotifyText(wchar_t* destination, size_t destinationCount, const QString& text) {
    if (!destination || destinationCount == 0) {
        return;
    }

    const std::wstring wideText = toNullTerminatedWideText(text, static_cast<int>(destinationCount));
    wcsncpy_s(destination, destinationCount, wideText.c_str(), _TRUNCATE);
}

void appendLe16(QByteArray& data, quint16 value) {
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
}

void appendLe32(QByteArray& data, quint32 value) {
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>((value >> 16) & 0xFF));
    data.append(static_cast<char>((value >> 24) & 0xFF));
}

QByteArray createScreenshotShutterWave() {
    constexpr int sampleRate = 22050;
    constexpr int durationMs = 170;
    constexpr int channelCount = 1;
    constexpr int bitsPerSample = 16;
    constexpr int bytesPerSample = bitsPerSample / 8;
    const int sampleCount = sampleRate * durationMs / 1000;

    QByteArray pcm;
    pcm.reserve(sampleCount * bytesPerSample);

    auto burst = [](double t, double start, double duration, double primaryHz, double secondaryHz) {
        if (t < start || t > start + duration) {
            return 0.0;
        }

        constexpr double piValue = 3.14159265358979323846;
        const double x = (t - start) / duration;
        const double envelope = std::exp(-7.0 * x) * (1.0 - x);
        return envelope * (
            std::sin(2.0 * piValue * primaryHz * (t - start))
            + 0.42 * std::sin(2.0 * piValue * secondaryHz * (t - start)));
    };

    for (int i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        const double value =
            0.78 * burst(t, 0.000, 0.032, 3800.0, 5300.0)
            + 0.52 * burst(t, 0.045, 0.052, 1500.0, 2600.0);
        const int boundedSample = qBound(-32767, static_cast<int>(std::lround(value * 32767.0)), 32767);
        appendLe16(pcm, static_cast<quint16>(static_cast<qint16>(boundedSample)));
    }

    QByteArray wav;
    wav.reserve(44 + pcm.size());
    wav.append("RIFF", 4);
    appendLe32(wav, static_cast<quint32>(36 + pcm.size()));
    wav.append("WAVE", 4);
    wav.append("fmt ", 4);
    appendLe32(wav, 16);
    appendLe16(wav, 1);
    appendLe16(wav, channelCount);
    appendLe32(wav, sampleRate);
    appendLe32(wav, sampleRate * channelCount * bytesPerSample);
    appendLe16(wav, channelCount * bytesPerSample);
    appendLe16(wav, bitsPerSample);
    wav.append("data", 4);
    appendLe32(wav, static_cast<quint32>(pcm.size()));
    wav.append(pcm);
    return wav;
}

bool isEditableQuickItem(QQuickItem* item) {
    QQuickItem* current = item;
    while (current) {
        const QMetaObject* metaObject = current->metaObject();
        const QString className = QString::fromLatin1(metaObject->className());
        if (className.contains(QStringLiteral("TextInput"), Qt::CaseInsensitive)
            || className.contains(QStringLiteral("TextArea"), Qt::CaseInsensitive)
            || className.contains(QStringLiteral("TextField"), Qt::CaseInsensitive)) {
            return true;
        }

        const int textPropertyIndex = metaObject->indexOfProperty("text");
        const int readOnlyPropertyIndex = metaObject->indexOfProperty("readOnly");
        if (textPropertyIndex >= 0 && readOnlyPropertyIndex >= 0) {
            const QVariant readOnlyValue = current->property("readOnly");
            if (!readOnlyValue.isValid() || !readOnlyValue.toBool()) {
                return true;
            }
        }

        current = current->parentItem();
    }
    return false;
}
}

class FullscreenIslandButton : public QPushButton {
public:
    explicit FullscreenIslandButton(QWidget* parent = nullptr)
        : QPushButton(parent) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAutoFillBackground(false);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setFixedSize(kCanvasSize, kCanvasSize);
        setIconPixmap(QIcon(QStringLiteral(":/app.ico")).pixmap(kIconSize, kIconSize));
    }

    void setIconPixmap(const QPixmap& pixmap) {
        m_iconPixmap = pixmap;
        update();
    }

    void setRotationAngle(qreal angle) {
        m_rotationAngle = angle;
        update();
    }

    void updateTheme(bool isDark) {
        Q_UNUSED(isDark);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

        if (m_iconPixmap.isNull())
            return;

        const QSize pixmapSize(kIconSize, kIconSize);
        const QRectF target(
            (width() - pixmapSize.width()) / 2.0,
            (height() - pixmapSize.height()) / 2.0,
            pixmapSize.width(),
            pixmapSize.height());

        painter.save();
        painter.translate(rect().center());
        painter.rotate(m_rotationAngle);
        painter.translate(-rect().center());
        painter.drawPixmap(target.toRect(), m_iconPixmap);
        painter.restore();
    }

private:
    static constexpr int kCanvasSize = 60;
    static constexpr int kIconSize = 40;
    QPixmap m_iconPixmap;
    qreal m_rotationAngle = 0.0;
};

class FullscreenRadialMenu : public QWidget {
public:
    explicit FullscreenRadialMenu(QWidget* parent = nullptr)
        : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowDoesNotAcceptFocus) {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setFixedSize(180, 180);
        hide();

        m_closeAppButton = createActionButton();
        m_closeToolButton = createActionButton();
        m_exitFullscreenButton = createActionButton();

        connect(m_closeAppButton, &QToolButton::clicked, this, [this]() {
            hideMenu();
            if (m_closeAppCallback)
                m_closeAppCallback();
        });
        connect(m_closeToolButton, &QToolButton::clicked, this, [this]() {
            hideMenu();
            if (m_closeToolCallback)
                m_closeToolCallback();
        });
        connect(m_exitFullscreenButton, &QToolButton::clicked, this, [this]() {
            hideMenu();
            if (m_exitFullscreenCallback)
                m_exitFullscreenCallback();
        });
    }

    QPoint originPoint() const {
        return QPoint(30, height() - 30);
    }

    bool isOpen() const {
        return m_open;
    }

    void setCallbacks(std::function<void()> closeAppCallback,
                      std::function<void()> closeToolCallback,
                      std::function<void()> exitFullscreenCallback) {
        m_closeAppCallback = std::move(closeAppCallback);
        m_closeToolCallback = std::move(closeToolCallback);
        m_exitFullscreenCallback = std::move(exitFullscreenCallback);
    }

    void setIslandToggleCallback(std::function<void()> callback) {
        m_islandToggleCallback = std::move(callback);
    }

    void setActionTexts(const QString& closeAppText,
                        const QString& closeToolText,
                        const QString& exitFullscreenText) {
        applyActionText(m_closeAppButton, closeAppText);
        applyActionText(m_closeToolButton, closeToolText);
        applyActionText(m_exitFullscreenButton, exitFullscreenText);
    }

    void setActionIcons(const QIcon& closeAppIcon,
                        const QIcon& closeToolIcon,
                        const QIcon& exitFullscreenIcon) {
        m_closeAppButton->setIcon(closeAppIcon);
        m_closeToolButton->setIcon(closeToolIcon);
        m_exitFullscreenButton->setIcon(exitFullscreenIcon);
    }

    void setCloseToolEnabled(bool enabled) {
        m_closeToolButton->setEnabled(enabled);
    }

    void updateTheme(bool isDark) {
        m_ringColor = isDark ? QColor(36, 36, 39) : QColor(246, 246, 248);
        m_ringBorder = isDark ? QColor(78, 78, 84) : QColor(218, 218, 224);
        const QString text = isDark ? QStringLiteral("#F5F5F7") : QStringLiteral("#1D1D1F");
        const QString buttonBg = isDark ? QStringLiteral("#343438") : QStringLiteral("#F6F6F8");
        const QString buttonHover = isDark ? QStringLiteral("#45454A") : QStringLiteral("#FFFFFF");
        const QString border = isDark ? QStringLiteral("#54545A") : QStringLiteral("#D9D9DF");
        const QString disabled = isDark ? QStringLiteral("#6B6B70") : QStringLiteral("#9A9AA0");
        setStyleSheet(QStringLiteral(
            "QToolButton#FullscreenRadialAction {"
            "  color: %1;"
            "  background-color: %2;"
            "  border: 1px solid %4;"
            "  border-radius: 19px;"
            "  padding: 0px;"
            "}"
            "QToolButton#FullscreenRadialAction:hover { background-color: %3; }"
            "QToolButton#FullscreenRadialAction:disabled { color: %5; background-color: transparent; border-color: %5; }"
        ).arg(text, buttonBg, buttonHover, border, disabled));
        update();
    }

    void showMenu() {
        setOpen(true);
    }

    void hideMenu() {
        setOpen(false);
    }

    void toggleMenu() {
        setOpen(!m_open);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (!event) {
            return;
        }

        const QPointF origin = originPoint();
        const QPointF delta = event->position() - origin;
        if (std::hypot(delta.x(), delta.y()) <= kFullscreenIslandHitRadius && m_islandToggleCallback) {
            event->accept();
            m_islandToggleCallback();
            return;
        }

        QWidget::mousePressEvent(event);
    }

    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        if (m_progress <= 0.0)
            return;

        painter.setOpacity(qBound(0.0, m_progress * 1.25, 1.0));

        const QPointF origin = originPoint();
        const QRectF arcRect = radialArcRect(origin);
        QPen ringPen(m_ringColor, kFullscreenRadialRingWidth, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(ringPen);
        painter.drawArc(arcRect, 0, qRound(90.0 * 16.0 * m_progress));

        QPen borderPen(m_ringBorder, 1.0, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(borderPen);
        painter.drawArc(arcRect.adjusted(kFullscreenRadialRingInset, kFullscreenRadialRingInset, -kFullscreenRadialRingInset, -kFullscreenRadialRingInset), 0, qRound(90.0 * 16.0 * m_progress));
        painter.drawArc(arcRect.adjusted(
            -kFullscreenRadialRingInset,
            -kFullscreenRadialRingInset,
            kFullscreenRadialRingInset,
            kFullscreenRadialRingInset), 0, qRound(90.0 * 16.0 * m_progress));
    }

private:
    QToolButton* createActionButton() {
        auto* button = new QToolButton(this);
        button->setObjectName("FullscreenRadialAction");
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setAutoRaise(false);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(20, 20));
        button->setFixedSize(38, 38);
        button->hide();
        return button;
    }

    void applyActionText(QToolButton* button, const QString& text) {
        if (!button)
            return;
        button->setToolTip(text);
        button->setAccessibleName(text);
    }

    void setOpen(bool open) {
        if (m_open == open && ((open && qFuzzyCompare(m_progress, 1.0)) || (!open && qFuzzyIsNull(m_progress))))
            return;

        m_open = open;
        if (open) {
            show();
            raise();
            repaint();
        }

        if (m_animation) {
            QVariantAnimation* oldAnimation = m_animation;
            m_animation = nullptr;
            oldAnimation->stop();
            oldAnimation->deleteLater();
        }

        auto* animation = new QVariantAnimation(this);
        m_animation = animation;
        animation->setStartValue(m_progress);
        animation->setEndValue(open ? 1.0 : 0.0);
        animation->setDuration(open ? 230 : 150);
        animation->setEasingCurve(open ? QEasingCurve::OutBack : QEasingCurve::InCubic);
        connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_progress = qBound(0.0, value.toDouble(), 1.0);
            updateButtonGeometries();
            update();
        });
        connect(animation, &QVariantAnimation::finished, this, [this, animation]() {
            if (m_animation == animation) {
                m_animation = nullptr;
            }
            if (!m_open && qFuzzyIsNull(m_progress)) {
                hide();
            }
            animation->deleteLater();
        });
        animation->start();
    }

    void updateButtonGeometries() {
        const QPointF origin = originPoint();
        const qreal radius = kFullscreenRadialActionRadius * qBound(0.0, m_progress, 1.0);
        const QList<qreal> angles = {-18.0, -45.0, -72.0};
        const QList<QToolButton*> buttons = {m_closeAppButton, m_closeToolButton, m_exitFullscreenButton};

        for (int i = 0; i < buttons.size(); ++i) {
            QToolButton* button = buttons.at(i);
            if (!button)
                continue;
            const qreal radians = qDegreesToRadians(angles.at(i));
            const int x = qRound(origin.x() + std::cos(radians) * radius - button->width() / 2.0);
            const int y = qRound(origin.y() + std::sin(radians) * radius - button->height() / 2.0);
            button->setGeometry(x, y, button->width(), button->height());
            button->setVisible(m_progress > 0.03);
            button->raise();
        }
    }

    QRectF radialArcRect(const QPointF& origin) const {
        return QRectF(
            origin.x() - kFullscreenRadialActionRadius,
            origin.y() - kFullscreenRadialActionRadius,
            kFullscreenRadialActionRadius * 2.0,
            kFullscreenRadialActionRadius * 2.0);
    }

    QToolButton* m_closeAppButton = nullptr;
    QToolButton* m_closeToolButton = nullptr;
    QToolButton* m_exitFullscreenButton = nullptr;
    std::function<void()> m_closeAppCallback;
    std::function<void()> m_closeToolCallback;
    std::function<void()> m_exitFullscreenCallback;
    std::function<void()> m_islandToggleCallback;
    QVariantAnimation* m_animation = nullptr;
    static constexpr qreal kFullscreenRadialActionRadius = 82.0;
    static constexpr qreal kFullscreenRadialRingWidth = 44.0;
    static constexpr qreal kFullscreenRadialRingInset = kFullscreenRadialRingWidth / 2.0;
    static constexpr qreal kFullscreenIslandHitRadius = 28.0;
    QColor m_ringColor = QColor(36, 36, 39);
    QColor m_ringBorder = QColor(78, 78, 84);
    qreal m_progress = 0.0;
    bool m_open = false;
};

class RightSidebarPanelWidget : public QWidget {
public:
    explicit RightSidebarPanelWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_StyledBackground, true);
    }

    void setSeparatorColor(const QColor& separatorColor) {
        m_separatorColor = separatorColor;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        QStyleOption option;
        option.initFrom(this);
        style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
        painter.fillRect(QRect(0, 0, 1, height()), m_separatorColor);
    }

private:
    QColor m_separatorColor = QColor(78, 78, 84);
};

class RightSidebarResizeHandleWidget : public QWidget {
public:
    explicit RightSidebarResizeHandleWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }

    void setDragFillColor(const QColor& color) {
        m_dragFillColor = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);

        if (!property("dragging").toBool()) {
            return;
        }

        QPainter painter(this);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), m_dragFillColor);
    }

private:
    QColor m_dragFillColor = QColor(76, 76, 80);
};

namespace {

void applyTransparentTreePalette(QTreeWidget* treeWidget) {
    if (!treeWidget) {
        return;
    }

    QPalette palette = treeWidget->palette();
    palette.setColor(QPalette::Base, Qt::transparent);
    palette.setColor(QPalette::Window, Qt::transparent);
    treeWidget->setPalette(palette);
    treeWidget->setAutoFillBackground(false);

    if (QWidget* viewport = treeWidget->viewport()) {
        viewport->setPalette(palette);
        viewport->setAutoFillBackground(false);
    }
    if (QHeaderView* header = treeWidget->header()) {
        header->setPalette(palette);
        header->setAutoFillBackground(false);
    }
}

QString rightSidebarTitleFromSession(const ToolGuiSessionState& session,
                                     const QString& modelId,
                                     const QString& fallback) {
    QString title;
    QString effectiveModelId = modelId.trimmed();
    if (effectiveModelId.isEmpty()) {
        effectiveModelId = session.sidebar.activeMode.trimmed();
    }
    if (!effectiveModelId.isEmpty() && session.lists.contains(effectiveModelId)) {
        title = session.lists.value(effectiveModelId).title.trimmed();
    }
    if (title.isEmpty()) {
        title = session.sidebar.title.trimmed();
    }
    return title.isEmpty() ? fallback : title;
}

QStringList rightSidebarHeaderLabelsForModel(const ToolGuiListModel& model) {
    QStringList labels;
    for (const ToolGuiListColumnDefinition& column : model.columns) {
        labels.append(column.title.rawText.trimmed().isEmpty() ? column.id : column.title.rawText);
    }
    return labels;
}

void applyRightSidebarHeaderLabels(QTreeWidget* listWidget, const ToolGuiListModel& model) {
    if (!listWidget) {
        return;
    }

    listWidget->setHeaderHidden(model.headerHidden || model.columns.size() <= 1);
    if (listWidget->isHeaderHidden()) {
        return;
    }

    listWidget->setHeaderLabels(rightSidebarHeaderLabelsForModel(model));
    if (QHeaderView* header = listWidget->header()) {
        header->setStretchLastSection(true);
    }
}

QQuickWidget* findQuickWidgetInToolHost(QWidget* widget) {
    if (!widget) {
        return nullptr;
    }
    if (QQuickWidget* quickWidget = qobject_cast<QQuickWidget*>(widget)) {
        return quickWidget;
    }
    return widget->findChild<QQuickWidget*>();
}
QString boolText(bool value) {
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString widgetDebugName(const QWidget* widget) {
    if (!widget) {
        return QStringLiteral("null");
    }
    return QStringLiteral("%1[%2]")
        .arg(widget->objectName().isEmpty() ? QString::fromLatin1(widget->metaObject()->className()) : widget->objectName())
        .arg(reinterpret_cast<quintptr>(widget), 0, 16);
}

QString windowSnapshot(const QWidget* widget) {
    if (!widget) {
        return QStringLiteral("window=<null>");
    }

    const quintptr winId = reinterpret_cast<quintptr>(const_cast<QWidget*>(widget)->winId());
    const quintptr effectiveWinId = reinterpret_cast<quintptr>(const_cast<QWidget*>(widget)->effectiveWinId());
    return QStringLiteral("winId=0x%1 effectiveWinId=0x%2 pid=%3 visible=%4 active=%5 minimized=%6 maximized=%7 state=0x%8 flags=0x%9")
        .arg(winId, 0, 16)
        .arg(effectiveWinId, 0, 16)
        .arg(static_cast<qulonglong>(QCoreApplication::applicationPid()))
        .arg(boolText(widget->isVisible()))
        .arg(boolText(widget->isActiveWindow()))
        .arg(boolText(widget->isMinimized()))
        .arg(boolText(widget->isMaximized()))
        .arg(static_cast<qulonglong>(widget->windowState()), 0, 16)
        .arg(static_cast<qulonglong>(widget->windowFlags()), 0, 16);
}

QList<int> intListFromVariant(const QVariant& value) {
    QList<int> result;
    const QVariantList list = value.toList();
    for (const QVariant& item : list) {
        bool ok = false;
        const int number = item.toInt(&ok);
        if (ok) {
            result.append(number);
        }
    }
    return result;
}

QIcon iconFromBase64Png(const QString& base64Data) {
    const QString trimmed = base64Data.trimmed();
    if (trimmed.isEmpty()) {
        return QIcon();
    }

    QPixmap pixmap;
    if (!pixmap.loadFromData(QByteArray::fromBase64(trimmed.toLatin1()), "PNG")) {
        return QIcon();
    }
    return QIcon(pixmap);
}

QImage tgaImageFromData(const QByteArray& data) {
    if (data.size() < 18) {
        return {};
    }

    const auto* bytes = reinterpret_cast<const uchar*>(data.constData());
    const int idLength = bytes[0];
    const int colorMapType = bytes[1];
    const int imageType = bytes[2];
    const int width = bytes[12] | (bytes[13] << 8);
    const int height = bytes[14] | (bytes[15] << 8);
    const int bpp = bytes[16];
    const int descriptor = bytes[17];

    if (colorMapType != 0 || (imageType != 2 && imageType != 10)) {
        return QImage::fromData(data);
    }
    if ((bpp != 24 && bpp != 32) || width <= 0 || height <= 0) {
        return {};
    }

    const int bytesPerPixel = bpp / 8;
    const int pixelDataOffset = 18 + idLength;
    if (pixelDataOffset < 0 || pixelDataOffset >= data.size()) {
        return {};
    }

    QImage image(width, height, QImage::Format_RGBA8888);
    const uchar* pixelData = bytes + pixelDataOffset;
    const int pixelCount = width * height;
    const int maxDataSize = data.size() - pixelDataOffset;

    auto writePixel = [&](int currentPixel, uchar blue, uchar green, uchar red, uchar alpha) {
        const int x = currentPixel % width;
        const int sourceY = currentPixel / width;
        const int destY = (descriptor & 0x20) ? sourceY : (height - 1 - sourceY);
        uchar* line = image.scanLine(destY);
        const int offset = x * 4;
        line[offset] = red;
        line[offset + 1] = green;
        line[offset + 2] = blue;
        line[offset + 3] = alpha;
    };

    if (imageType == 2) {
        for (int currentPixel = 0; currentPixel < pixelCount; ++currentPixel) {
            const int srcIndex = currentPixel * bytesPerPixel;
            if (srcIndex + bytesPerPixel > maxDataSize) {
                break;
            }
            writePixel(
                currentPixel,
                pixelData[srcIndex],
                pixelData[srcIndex + 1],
                pixelData[srcIndex + 2],
                bytesPerPixel == 4 ? pixelData[srcIndex + 3] : 255
            );
        }
        return image;
    }

    int currentPixel = 0;
    int dataIndex = 0;
    while (currentPixel < pixelCount && dataIndex < maxDataSize) {
        const uchar header = pixelData[dataIndex++];
        const int count = (header & 0x7F) + 1;

        if ((header & 0x80) != 0) {
            if (dataIndex + bytesPerPixel > maxDataSize) {
                break;
            }
            const uchar blue = pixelData[dataIndex];
            const uchar green = pixelData[dataIndex + 1];
            const uchar red = pixelData[dataIndex + 2];
            const uchar alpha = bytesPerPixel == 4 ? pixelData[dataIndex + 3] : 255;
            dataIndex += bytesPerPixel;
            for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                writePixel(currentPixel, blue, green, red, alpha);
            }
        } else {
            for (int i = 0; i < count && currentPixel < pixelCount; ++i, ++currentPixel) {
                if (dataIndex + bytesPerPixel > maxDataSize) {
                    break;
                }
                writePixel(
                    currentPixel,
                    pixelData[dataIndex],
                    pixelData[dataIndex + 1],
                    pixelData[dataIndex + 2],
                    bytesPerPixel == 4 ? pixelData[dataIndex + 3] : 255
                );
                dataIndex += bytesPerPixel;
            }
        }
    }

    return image;
}

bool truthyVariant(const QVariantMap& values, const QString& key, bool fallback = false) {
    if (!values.contains(key)) {
        return fallback;
    }
    return values.value(key).toBool();
}

QString firstNonEmptyString(const QVariantMap& values, std::initializer_list<QString> keys) {
    for (const QString& key : keys) {
        const QString value = values.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return QString();
}

QString sidebarListIds(const QMap<QString, ToolGuiListModel>& lists) {
    QStringList ids;
    for (auto it = lists.constBegin(); it != lists.constEnd(); ++it) {
        if (!it.key().trimmed().isEmpty()) {
            ids.append(it.key());
        }
    }
    return ids.join(QLatin1Char(','));
}

QString eventTypeName(QEvent::Type type) {
    switch (type) {
    case QEvent::MouseButtonPress:
        return QStringLiteral("MouseButtonPress");
    case QEvent::MouseButtonRelease:
        return QStringLiteral("MouseButtonRelease");
    case QEvent::MouseButtonDblClick:
        return QStringLiteral("MouseButtonDblClick");
    case QEvent::MouseMove:
        return QStringLiteral("MouseMove");
    case QEvent::Enter:
        return QStringLiteral("Enter");
    case QEvent::Leave:
        return QStringLiteral("Leave");
    case QEvent::Resize:
        return QStringLiteral("Resize");
    case QEvent::Show:
        return QStringLiteral("Show");
    case QEvent::Hide:
        return QStringLiteral("Hide");
    case QEvent::WinIdChange:
        return QStringLiteral("WinIdChange");
    case QEvent::PlatformSurface:
        return QStringLiteral("PlatformSurface");
    default:
        return QString::number(static_cast<int>(type));
    }
}

qreal boundedScreenshotCornerRadius(int width, int height, qreal radius) {
    const qreal halfShortestSide = std::min(width, height) / 2.0;
    return std::clamp(radius, 0.0, halfShortestSide);
}

bool pointInsideRoundedScreenshotRect(qreal x, qreal y, int width, int height, qreal radius) {
    if (width <= 0 || height <= 0 || x < 0.0 || y < 0.0 || x >= width || y >= height) {
        return false;
    }

    const qreal boundedRadius = boundedScreenshotCornerRadius(width, height, radius);
    if (boundedRadius <= 0.0) {
        return true;
    }

    const qreal centerX = std::clamp(x, boundedRadius, width - boundedRadius);
    const qreal centerY = std::clamp(y, boundedRadius, height - boundedRadius);
    const qreal dx = x - centerX;
    const qreal dy = y - centerY;
    return dx * dx + dy * dy <= boundedRadius * boundedRadius;
}

int roundedScreenshotCoverageAlpha(int pixelX, int pixelY, int width, int height, qreal radius) {
    const qreal boundedRadius = boundedScreenshotCornerRadius(width, height, radius);
    if (boundedRadius <= 0.0) {
        return 255;
    }
    if ((pixelX >= boundedRadius && pixelX < width - boundedRadius)
        || (pixelY >= boundedRadius && pixelY < height - boundedRadius)) {
        return 255;
    }

    constexpr int totalSamples = kWindowScreenshotMaskSamples * kWindowScreenshotMaskSamples;
    int coveredSamples = 0;
    for (int sampleY = 0; sampleY < kWindowScreenshotMaskSamples; ++sampleY) {
        const qreal y = pixelY + (sampleY + 0.5) / kWindowScreenshotMaskSamples;
        for (int sampleX = 0; sampleX < kWindowScreenshotMaskSamples; ++sampleX) {
            const qreal x = pixelX + (sampleX + 0.5) / kWindowScreenshotMaskSamples;
            if (pointInsideRoundedScreenshotRect(x, y, width, height, boundedRadius)) {
                ++coveredSamples;
            }
        }
    }
    return (coveredSamples * 255 + totalSamples / 2) / totalSamples;
}
#ifdef Q_OS_WIN
using DwmSetWindowAttributeFn = HRESULT (WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
constexpr DWORD kDwmWindowCornerPreferenceAttribute = 33;
constexpr int kDwmWindowCornerPreferenceDoNotRound = 1;
constexpr int kDwmWindowCornerPreferenceRound = 2;

bool setDwmWindowCornerPreference(HWND hwnd, int preference) {
    if (!hwnd) {
        return false;
    }

    static HMODULE dwmModule = LoadLibraryW(L"dwmapi.dll");
    if (!dwmModule) {
        return false;
    }

    const auto setWindowAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(
        GetProcAddress(dwmModule, "DwmSetWindowAttribute")
    );
    if (!setWindowAttribute) {
        return false;
    }

    return SUCCEEDED(setWindowAttribute(
        hwnd,
        kDwmWindowCornerPreferenceAttribute,
        &preference,
        sizeof(preference)
    ));
}

bool setDwmRoundedWindowCorners(HWND hwnd) {
    return setDwmWindowCornerPreference(hwnd, kDwmWindowCornerPreferenceRound);
}

bool setDwmSquareWindowCorners(HWND hwnd) {
    return setDwmWindowCornerPreference(hwnd, kDwmWindowCornerPreferenceDoNotRound);
}

#endif
}

MainWindow::MainWindow(const ExternalPackageManager::PendingRequest& pendingRequest, QWidget *parent)
    : QMainWindow(parent)
    , m_pendingExternalRequest(pendingRequest)
{
    m_currentLang = ConfigManager::instance().getLanguage();
    LocalizationManager::instance().loadLanguage(m_currentLang);
    
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint);
    
    // Never enable WA_TranslucentBackground on the main window, now or in any
    // future acrylic work. It turns the HWND into a layered window and can
    // break hit testing, tool input routing, and native child-window composition.
    // ScreenAcrylicBackdrop renders the material inside this opaque window.
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    ensureNativeWindowAcceptsInput(QStringLiteral("constructor_window_setup"));
    applyNativeRoundedCorners(QStringLiteral("constructor_window_setup"));
    
    setWindowIcon(QIcon(":/app.ico"));
    
    setupUi();
    setupDebugOverlay();
    setupRightSidebarUi();
    applyTheme();
    resize(1280, 720);
    setMinimumSize(1280, 720);
    QTimer::singleShot(0, this, &MainWindow::applyDisplaySettings);

    m_sidebar->installEventFilter(this);
    m_rightSidebarResizeHandle->installEventFilter(this);
    m_mainStack->installEventFilter(this);
    m_dashboard->installEventFilter(this);
    qApp->installEventFilter(this);

    // Setup sidebar collapse delay timer (1.5 seconds)
    m_sidebarCollapseTimer = new QTimer(this);
    m_sidebarCollapseTimer->setSingleShot(true);
    m_sidebarCollapseTimer->setInterval(500); // Change from 1500 to 500
    connect(m_sidebarCollapseTimer, &QTimer::timeout, this, &MainWindow::collapseSidebar);

    // Setup geometry update throttle timer for smooth animations
    m_geometryUpdateThrottleTimer = new QTimer(this);
    m_geometryUpdateThrottleTimer->setSingleShot(true);
    m_geometryUpdateThrottleTimer->setInterval(16); // ~60fps

    m_windowRecorder = std::make_unique<WindowAviRecorder>();
    m_recordingFrameTimer = new QTimer(this);
    m_recordingFrameTimer->setInterval(qMax(1, 1000 / kWindowRecordingFrameRate));
    connect(m_recordingFrameTimer, &QTimer::timeout, this, &MainWindow::captureWindowRecordingFrame);

    // Setup shortcut to toggle sidebar lock
    QShortcut *lockShortcut = new QShortcut(QKeySequence("Alt+B"), this);
    connect(lockShortcut, &QShortcut::activated, this, &MainWindow::toggleSidebarLock);

    // Connect path validation notifications. Monitoring is started later in the startup flow.
    connect(&PathValidator::instance(), &PathValidator::pathInvalid, this, &MainWindow::onPathInvalid);

    // Setup Loading Overlay
    m_loadingOverlay = new LoadingOverlay(m_centralWidget);
    LocalizationManager& loc = LocalizationManager::instance();
    m_loadingOverlay->setMessage(loc.getString("MainWindow", "LoadingFiles"));
    
    // Setup Update Overlay
    m_updateOverlay = new Update(m_centralWidget);
    connect(m_updateOverlay, &Update::updateShutdownRequested, this, &MainWindow::requestUpdateShutdown);
    
    // Setup Advertisement Overlay
    m_advertisementOverlay = new Advertisement(m_centralWidget);
    
    // Setup Login Overlay
    m_loginOverlay = new LoginDialog(m_centralWidget);
    connect(m_loginOverlay, &LoginDialog::loginSuccessful, this, &MainWindow::onLoginSuccessful);
    
    // Setup SetupDialog Overlay
    m_setupOverlay = new SetupDialog(m_centralWidget);
    connect(m_setupOverlay, &SetupDialog::setupCompleted, this, &MainWindow::onSetupCompleted);

    // Setup Connection Warning Overlay
    m_connectionWarningOverlay = new ConnectionWarningOverlay(m_centralWidget);
    connect(&AuthManager::instance(), SIGNAL(connectionLost()), this, SLOT(onConnectionLost()), Qt::QueuedConnection);
    connect(&AuthManager::instance(), SIGNAL(connectionRestored()), this, SLOT(onConnectionRestored()), Qt::QueuedConnection);
    connect(&AuthManager::instance(), SIGNAL(accountActionBlocked()), this, SLOT(onAccountActionBlocked()), Qt::QueuedConnection);
    connect(&AuthManager::instance(), SIGNAL(accountActionCleared()), this, SLOT(onAccountActionCleared()), Qt::QueuedConnection);

    // Setup User Agreement Overlay
    m_userAgreementOverlay = new UserAgreementOverlay(m_centralWidget);
    connect(m_userAgreementOverlay, &UserAgreementOverlay::agreementAccepted, this, [this]() {
        // Continue startup on the next event loop turn so the agreement overlay can hide first.
        QTimer::singleShot(0, this, [this]() {
            if (AuthManager::instance().isAuthenticated()) {
                onLoginSuccessful();
            } else if (AuthManager::instance().hasSavedCredentials()) {
                // Have saved credentials, trigger auto login
                m_loginOverlay->showAutoLoggingIn();
                AuthManager::instance().autoLogin();

                // Wait for login result
                connect(&AuthManager::instance(), &AuthManager::loginSuccess, this, [this]() {
                    onLoginSuccessful();
                }, Qt::SingleShotConnection);

                // No need to handle loginFailed here, LoginDialog will automatically
                // switch to the manual login form when login fails
            } else {
                m_loginOverlay->showLogin();
            }
            raiseWindowControlsOverlay();
        });
    });

    for (QWidget* overlay : {static_cast<QWidget*>(m_loadingOverlay),
                             static_cast<QWidget*>(m_updateOverlay),
                             static_cast<QWidget*>(m_advertisementOverlay),
                             static_cast<QWidget*>(m_loginOverlay),
                             static_cast<QWidget*>(m_setupOverlay),
                             static_cast<QWidget*>(m_connectionWarningOverlay),
                             static_cast<QWidget*>(m_userAgreementOverlay)}) {
        if (overlay) {
            overlay->installEventFilter(this);
        }
    }
    
    // Setup scan check timer - poll every 500ms to check if scanning is complete
    m_scanCheckTimer = new QTimer(this);
    m_scanCheckTimer->setInterval(500);
    connect(m_scanCheckTimer, &QTimer::timeout, this, [this]() {
        if (!FileManager::instance().isScanning()) {
            Logger::instance().logInfo("MainWindow", "Scan complete detected via polling - hiding overlay");
            m_scanCheckTimer->stop();
            m_loadingOverlay->hideOverlay();

            Logger::instance().logInfo("MainWindow", "File loading completed, scanning plugins");
            PluginManager::instance().loadPlugins();

            if (ToolManager::instance().getTools().isEmpty()) {
                Logger::instance().logInfo("MainWindow", "File loading completed, scanning tools");
                ToolManager::instance().loadTools();
            }

            Logger::instance().logInfo("MainWindow", "File loading completed, refreshing tools page");
            m_toolsPage->refreshTools();

            processPendingExternalRequest();

            // Show advertisement only on first startup load
            if (!m_firstLoadCompleted) {
                m_firstLoadCompleted = true;
                Logger::instance().logInfo("MainWindow", "First load completed, showing advertisement");
                QTimer::singleShot(500, this, [this]() {
                    m_advertisementOverlay->showAd();
                    raiseWindowControlsOverlay();
                });
            } else {
                Logger::instance().logInfo("MainWindow", "Subsequent load - skipping advertisement");
            }
        }
    });
    
    // Connect AuthManager ad signal
    connect(&AuthManager::instance(), &AuthManager::adReceived, this, [this](const QString& text, const QString& imageUrl, const QString& targetUrl) {
        m_advertisementOverlay->showAdWithData(text, imageUrl, targetUrl);
        raiseWindowControlsOverlay();
    });
    
    // Connect ad fetch failed signal - files are already loaded at this point,
    // so just log the failure without re-triggering file loading
    connect(m_advertisementOverlay, &Advertisement::adFetchFailed, this, [this]() {
        Logger::instance().logInfo("MainWindow", "Ad fetch failed, files already loaded - no action needed");
    }, Qt::SingleShotConnection);
    
    // Check User Agreement on startup as soon as the first event loop turn starts
    QTimer::singleShot(0, this, [this]() {
        m_userAgreementOverlay->checkAgreement();
        raiseWindowControlsOverlay();
    });

    // Connect tool crash signal
    connect(&ToolManager::instance(), &ToolManager::toolProcessCrashed,
            this, &MainWindow::onToolProcessCrashed);
    ToolManager::instance().setQuestionDialogHandler(
        [this](const QString& title, const QString& message, std::function<void(bool)> callback) {
            const auto result = CustomMessageBox::question(this, title, message);
            if (callback) {
                callback(result == QMessageBox::Yes);
            }
        });
    
    if (ConfigManager::instance().getSidebarCompactMode()) {
        m_sidebar->setFixedWidth(60);
        setSidebarExpandedPresentation(false);
    }
    updateWindowControlsOverlayGeometry();
}

MainWindow::~MainWindow() {
    if (m_windowRecordingActive) {
        stopWindowRecording();
    }
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void MainWindow::setupUi() {
    m_acrylicBackdrop = new ScreenAcrylicBackdrop(this);
    m_centralWidget = m_acrylicBackdrop;
    m_centralWidget->setObjectName("CentralWidget");
    setCentralWidget(m_centralWidget);
    
    // CentralWidget must NOT be translucent to prevent inheritance to children.
    m_centralWidget->setAttribute(Qt::WA_TranslucentBackground, false);

    QHBoxLayout *mainLayout = new QHBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    setupSidebar();
    mainLayout->addWidget(m_sidebar);
    m_sidebarControlsContainer->setParent(m_centralWidget);
    m_sidebarControlsContainer->raise();

    m_fullscreenRadialMenu = new FullscreenRadialMenu(m_centralWidget);
    m_fullscreenRadialMenu->setCallbacks(
        [this]() { closeWindow(); },
        [this]() { closeActiveToolWithConfirmation(); },
        [this]() { exitFullscreenFromMenu(); });
    m_fullscreenRadialMenu->setIslandToggleCallback([this]() { toggleFullscreenIslandMenu(); });

    m_mainStack = new QStackedWidget(m_centralWidget);
    
    m_dashboard = new QWidget();
    m_dashboard->setObjectName("Dashboard");
    // Dashboard must NOT be translucent
    m_dashboard->setAttribute(Qt::WA_TranslucentBackground, false);

    QHBoxLayout *dashLayout = new QHBoxLayout(m_dashboard);
    dashLayout->setContentsMargins(0, 0, 0, 0);
    dashLayout->setSpacing(0);

    m_dashboardContent = new QWidget(m_dashboard);
    m_dashboardContent->setObjectName("DashboardContent");
    
    // CRITICAL: DashboardContent is the direct parent of tool widgets (via ToolUiContainer).
    // It paints the shared content material while the top-level MainWindow stays opaque.
    m_dashboardContent->setAttribute(Qt::WA_TranslucentBackground, false);
    
    QVBoxLayout *contentLayout = new QVBoxLayout(m_dashboardContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    LocalizationManager& loc = LocalizationManager::instance();
    m_dashboardTitleLabel = new QLabel(loc.getString("MainWindow", "DashboardArea"), m_dashboardContent);
    m_dashboardTitleLabel->setAlignment(Qt::AlignCenter);
    contentLayout->addWidget(m_dashboardTitleLabel);
    dashLayout->addWidget(m_dashboardContent);

    m_rightSidebarPanel = new RightSidebarPanelWidget(m_dashboard);
    m_rightSidebarPanel->setObjectName("RightSidebarPanel");
    m_rightSidebarPanel->hide();
    QVBoxLayout *panelLayout = new QVBoxLayout(m_rightSidebarPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    m_rightSidebarHeaderArea = new QWidget(m_rightSidebarPanel);
    m_rightSidebarHeaderArea->setObjectName("RightSidebarHeaderArea");
    m_rightSidebarHeaderArea->setMinimumHeight(kRightSidebarTitleBarHeight);
    m_rightSidebarHeaderArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    QVBoxLayout *headerLayout = new QVBoxLayout(m_rightSidebarHeaderArea);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    m_rightSidebarTitleLabel = new QLabel(m_rightSidebarHeaderArea);
    m_rightSidebarTitleLabel->setObjectName("RightSidebarTitle");
    m_rightSidebarTitleLabel->setFixedHeight(kRightSidebarTitleBarHeight);
    m_rightSidebarTitleLabel->setMinimumHeight(kRightSidebarTitleBarHeight);
    m_rightSidebarTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_rightSidebarTitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    headerLayout->addWidget(m_rightSidebarTitleLabel);

    QWidget *rightSidebarOperationsArea = new QWidget(m_rightSidebarHeaderArea);
    rightSidebarOperationsArea->setObjectName("RightSidebarOperationsArea");
    QVBoxLayout *operationsLayout = new QVBoxLayout(rightSidebarOperationsArea);
    operationsLayout->setContentsMargins(0, 0, 0, 0);
    operationsLayout->setSpacing(0);

    m_rightSidebarSearchContainer = new QWidget(rightSidebarOperationsArea);
    m_rightSidebarSearchContainer->setObjectName("RightSidebarSearchContainer");
    QVBoxLayout *searchLayout = new QVBoxLayout(m_rightSidebarSearchContainer);
    searchLayout->setContentsMargins(16, 10, 16, 0);
    searchLayout->setSpacing(8);
    searchLayout->setAlignment(Qt::AlignTop);

    m_rightSidebarSearchEdit = new QLineEdit(m_rightSidebarSearchContainer);
    connect(m_rightSidebarSearchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onRightSidebarSearchTextChanged);
    searchLayout->addWidget(m_rightSidebarSearchEdit);

    m_rightSidebarSearchColumnCombo = new QComboBox(m_rightSidebarSearchContainer);
    connect(m_rightSidebarSearchColumnCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onRightSidebarSearchColumnChanged);
    searchLayout->addWidget(m_rightSidebarSearchColumnCombo);

    operationsLayout->addWidget(m_rightSidebarSearchContainer);

    m_rightSidebarExtraActionsContainer = new QWidget(rightSidebarOperationsArea);
    m_rightSidebarExtraActionsContainer->setObjectName("RightSidebarExtraActionsContainer");
    QVBoxLayout *extraActionsLayout = new QVBoxLayout(m_rightSidebarExtraActionsContainer);
    extraActionsLayout->setContentsMargins(16, 8, 16, 10);
    extraActionsLayout->setSpacing(0);
    extraActionsLayout->setAlignment(Qt::AlignTop);

    m_rightSidebarSelectAllBtn = new QPushButton(m_rightSidebarExtraActionsContainer);
    m_rightSidebarSelectAllBtn->setObjectName("RightSidebarSelectAllButton");
    connect(m_rightSidebarSelectAllBtn, &QPushButton::clicked,
            this, &MainWindow::onRightSidebarSelectAllClicked);
    extraActionsLayout->addWidget(m_rightSidebarSelectAllBtn);

    operationsLayout->addWidget(m_rightSidebarExtraActionsContainer);
    headerLayout->addWidget(rightSidebarOperationsArea);
    panelLayout->addWidget(m_rightSidebarHeaderArea);

    m_rightSidebarContentContainer = new QWidget(m_rightSidebarPanel);
    m_rightSidebarContentContainer->setObjectName("RightSidebarContentContainer");
    m_rightSidebarContentLayout = new QVBoxLayout(m_rightSidebarContentContainer);
    m_rightSidebarContentLayout->setContentsMargins(0, 0, 0, 0);
    m_rightSidebarContentLayout->setSpacing(0);
    panelLayout->addWidget(m_rightSidebarContentContainer, 1);

    m_rightSidebarResizeHandle = new RightSidebarResizeHandleWidget(m_rightSidebarPanel);
    m_rightSidebarResizeHandle->setObjectName("RightSidebarResizeHandle");
    m_rightSidebarResizeHandle->setCursor(Qt::SizeHorCursor);
    m_rightSidebarResizeHandle->setProperty("dragging", false);
    m_rightSidebarResizeHandle->hide();

    m_rightSidebarRail = new QWidget(m_dashboard);
    m_rightSidebarRail->setObjectName("RightSidebarRail");
    m_rightSidebarRail->setFixedWidth(m_rightSidebarRailWidth);
    m_rightSidebarRailLayout = new QVBoxLayout(m_rightSidebarRail);
    m_rightSidebarRailLayout->setContentsMargins(8, 8, 8, 8);
    m_rightSidebarRailLayout->setSpacing(8);

    m_mainStack->addWidget(m_dashboard);

    m_settingsPage = new SettingsPage();
    m_settingsPage->setObjectName("OverlayContainer");
    connect(m_settingsPage, &SettingsPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_settingsPage, &SettingsPage::languageChanged, this, &MainWindow::onLanguageChanged);
    connect(m_settingsPage, &SettingsPage::gameLanguageChanged, this, [this]() {
        if (ToolManager::instance().isToolActive() && m_activeTool) {
            const QString currentLang = ConfigManager::instance().getLanguage();
            m_activeTool->loadLanguage(currentLang);
            if (m_activeScriptedHostController) {
                m_activeScriptedHostController->setLocalizedStrings(m_activeTool->localizedStrings());
            }
        }
    });
    connect(m_settingsPage, &SettingsPage::themeChanged, this, &MainWindow::onThemeChanged);
    connect(m_settingsPage, &SettingsPage::debugModeChanged, this, &MainWindow::onDebugModeChanged);
    connect(m_settingsPage, &SettingsPage::sidebarCompactChanged, this, &MainWindow::onSidebarCompactChanged);
    connect(m_settingsPage, &SettingsPage::displaySettingsChanged, this, &MainWindow::applyDisplaySettings);
    connect(m_settingsPage, &SettingsPage::showUserAgreement, this, [this]() {
        m_userAgreementOverlay->showAgreement(true);
        raiseWindowControlsOverlay();
    });
    m_mainStack->addWidget(m_settingsPage);

    m_configPage = new ConfigPage();
    m_configPage->setObjectName("OverlayContainer");
    connect(m_configPage, &ConfigPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_configPage, &ConfigPage::modClosed, this, &MainWindow::onModClosed);
    connect(m_configPage, &ConfigPage::modPathChanged, this, &MainWindow::onModPathChanged);
    m_mainStack->addWidget(m_configPage);

    m_toolsPage = new ToolsPage();
    m_toolsPage->setObjectName("OverlayContainer");
    connect(m_toolsPage, &ToolsPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_toolsPage, &ToolsPage::toolSelected, this, &MainWindow::onToolSelected);
    m_mainStack->addWidget(m_toolsPage);

    m_accountPage = new AccountPage();
    m_accountPage->setObjectName("OverlayContainer");
    connect(m_accountPage, &AccountPage::closeClicked, this, &MainWindow::closeOverlay);
    connect(m_accountPage, &AccountPage::logoutRequested, this, &MainWindow::onLogoutRequested);
    m_mainStack->addWidget(m_accountPage);

    mainLayout->addWidget(m_mainStack, 1);  // MainStack takes all available space

    updateTexts();
}

void MainWindow::setupSidebar() {
    m_sidebar = new QWidget(this);
    m_sidebar->setObjectName("Sidebar");
    m_sidebar->setFixedWidth(250);
    
    // Sidebar must NOT be translucent
    m_sidebar->setAttribute(Qt::WA_TranslucentBackground, false);

    m_sidebarLayout = new QVBoxLayout(m_sidebar);
    m_sidebarLayout->setContentsMargins(20, 20, 20, 20);
    m_sidebarLayout->setSpacing(10);

    // Window Controls
    m_sidebarControlsContainer = new QWidget(m_sidebar);
    QVBoxLayout *controlsContainerLayout = new QVBoxLayout(m_sidebarControlsContainer);
    controlsContainerLayout->setContentsMargins(0, 0, 0, 0);
    controlsContainerLayout->setSpacing(0);

    auto createControlBtn = [](const QString &color, const QString &hoverColor) -> QPushButton* {
        QPushButton *btn = new QPushButton();
        btn->setFixedSize(12, 12);
        btn->setStyleSheet(QString("QPushButton { background-color: %1; border-radius: 6px; border: none; } QPushButton:hover { background-color: %2; }").arg(color, hoverColor));
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    // Horizontal
    m_controlsHorizontal = new QWidget();
    QHBoxLayout *hLayout = new QHBoxLayout(m_controlsHorizontal);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(8);
    QPushButton *closeBtnH = createControlBtn("#FF5F57", "#FF3B30");
    QPushButton *minBtnH = createControlBtn("#5AC8FA", "#32ADE6");
    connect(closeBtnH, &QPushButton::clicked, this, &MainWindow::closeWindow);
    connect(minBtnH, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    hLayout->addWidget(closeBtnH);
    hLayout->addWidget(minBtnH);
    hLayout->addStretch();

    // Vertical
    m_controlsVertical = new QWidget();
    QVBoxLayout *vLayout = new QVBoxLayout(m_controlsVertical);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(8);
    vLayout->setAlignment(Qt::AlignHCenter);
    QPushButton *closeBtnV = createControlBtn("#FF5F57", "#FF3B30");
    QPushButton *minBtnV = createControlBtn("#5AC8FA", "#32ADE6");
    connect(closeBtnV, &QPushButton::clicked, this, &MainWindow::closeWindow);
    connect(minBtnV, &QPushButton::clicked, this, &MainWindow::minimizeWindow);
    vLayout->addWidget(closeBtnV);
    vLayout->addWidget(minBtnV);
    m_controlsVertical->hide();

    controlsContainerLayout->addWidget(m_controlsHorizontal);
    controlsContainerLayout->addWidget(m_controlsVertical);
    
    m_sidebarTopSpacerSmall = new QWidget(m_sidebar);
    m_sidebarTopSpacerSmall->setFixedHeight(12);
    m_sidebarTopSpacerLarge = new QWidget(m_sidebar);
    m_sidebarTopSpacerLarge->setFixedHeight(20);
    m_sidebarLayout->addWidget(m_sidebarTopSpacerSmall);
    m_sidebarLayout->addWidget(m_sidebarTopSpacerLarge);

    // App Icon & Title (Expanded)
    m_titleLayout = new QHBoxLayout();
    m_appIcon = new QLabel();
    m_appIcon->setPixmap(QIcon(":/app.ico").pixmap(40, 40));
    m_appIcon->setFixedSize(40, 40);
    m_appIcon->setAlignment(Qt::AlignCenter);
    
    m_appTitle = new QLabel("APE HOI4\nTool Studio");
    m_appTitle->setObjectName("SidebarTitle");
    m_appTitle->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_fullscreenIslandButton = new FullscreenIslandButton(m_sidebar);
    m_fullscreenIslandButton->hide();
    connect(m_fullscreenIslandButton, &QPushButton::clicked, this, &MainWindow::toggleFullscreenIslandMenu);
    
    m_titleLayout->addWidget(m_appIcon);
    m_titleLayout->addWidget(m_appTitle);
    m_titleLayout->addStretch(); 
    m_sidebarLayout->addLayout(m_titleLayout);

    m_sidebarLayout->addStretch();

    // Navigation Buttons (QToolButton)
    m_sidebarNavigationContainer = new QWidget(m_sidebar);
    m_sidebarNavigationLayout = new QVBoxLayout(m_sidebarNavigationContainer);
    m_sidebarNavigationLayout->setContentsMargins(0, 0, 0, 0);
    m_sidebarNavigationLayout->setSpacing(10);

    m_toolsBtn = new QToolButton(this);
    m_toolsBtn->setObjectName("SidebarButton");
    m_toolsBtn->setCursor(Qt::PointingHandCursor);
    m_toolsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_toolsBtn->setIcon(loadThemedSvgIcon(":/icons/toolbox.svg", ConfigManager::instance().isCurrentThemeDark()));
    m_toolsBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_toolsBtn, &QPushButton::clicked, this, &MainWindow::onToolsClicked);
    m_sidebarNavigationLayout->addWidget(m_toolsBtn);

    m_accountBtn = new QToolButton(this);
    m_accountBtn->setObjectName("SidebarButton");
    m_accountBtn->setCursor(Qt::PointingHandCursor);
    m_accountBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_accountBtn->setIcon(loadThemedSvgIcon(":/icons/user.svg", ConfigManager::instance().isCurrentThemeDark()));
    m_accountBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_accountBtn, &QPushButton::clicked, this, &MainWindow::onAccountClicked);
    m_sidebarNavigationLayout->addWidget(m_accountBtn);

    m_settingsBtn = new QToolButton(this);
    m_settingsBtn->setObjectName("SidebarButton");
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolButtonStyle(Qt::ToolButtonTextOnly); 
    m_settingsBtn->setIcon(loadThemedSvgIcon(":/icons/settings.svg", ConfigManager::instance().isCurrentThemeDark()));
    m_settingsBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    m_sidebarNavigationLayout->addWidget(m_settingsBtn);

    m_configBtn = new QToolButton(this);
    m_configBtn->setObjectName("SidebarButton");
    m_configBtn->setCursor(Qt::PointingHandCursor);
    m_configBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_configBtn->setIcon(loadThemedSvgIcon(":/icons/folder.svg", ConfigManager::instance().isCurrentThemeDark()));
    m_configBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_configBtn, &QPushButton::clicked, this, &MainWindow::onConfigClicked);
    m_sidebarNavigationLayout->addWidget(m_configBtn);
    m_sidebarLayout->addWidget(m_sidebarNavigationContainer);

    // Bottom App Icon (Collapsed)
    m_bottomAppIcon = new QLabel();
    m_bottomAppIcon->setPixmap(QIcon(":/app.ico").pixmap(40, 40));
    m_bottomAppIcon->setFixedSize(40, 40);
    m_bottomAppIcon->setAlignment(Qt::AlignCenter);
    m_bottomAppIcon->hide(); // Initially hidden
    
    m_bottomAppIconContainer = new QWidget(m_sidebar);
    QHBoxLayout *bottomIconLayout = new QHBoxLayout(m_bottomAppIconContainer);
    bottomIconLayout->setContentsMargins(0, 0, 0, 0);
    bottomIconLayout->setSpacing(0);
    bottomIconLayout->addStretch();
    bottomIconLayout->addWidget(m_bottomAppIcon);
    bottomIconLayout->addWidget(m_fullscreenIslandButton);
    bottomIconLayout->addStretch();
    m_sidebarLayout->addWidget(m_bottomAppIconContainer);
}

void MainWindow::updateWindowControlsOverlayGeometry() {
    if (!m_sidebarControlsContainer) {
        return;
    }

    if (m_fullscreenPresentationActive) {
        m_sidebarControlsContainer->hide();
        return;
    }

    const bool compact = !m_sidebarExpanded;
    const int containerWidth = compact ? 12 : 56;
    const int containerHeight = compact ? 52 : 12;
    const int x = compact ? qMax(0, (m_sidebar->width() - containerWidth) / 2) : 20;

    m_sidebarControlsContainer->setFixedSize(containerWidth, containerHeight);
    m_sidebarControlsContainer->move(x, 20);
    raiseWindowControlsOverlay();
}

void MainWindow::raiseWindowControlsOverlay() {
    if (!m_sidebarControlsContainer) {
        return;
    }

    if (m_fullscreenPresentationActive) {
        m_sidebarControlsContainer->hide();
        return;
    }

    m_sidebarControlsContainer->show();
    m_sidebarControlsContainer->raise();
}

void MainWindow::updateFullscreenChromeGeometry() {
    if (!m_fullscreenRadialMenu || !m_fullscreenIslandButton) {
        return;
    }

    const QPoint islandCenter = m_fullscreenIslandButton->mapToGlobal(
        QPoint(m_fullscreenIslandButton->width() / 2, m_fullscreenIslandButton->height() / 2));
    m_fullscreenRadialMenu->move(islandCenter - m_fullscreenRadialMenu->originPoint());
    if (m_fullscreenRadialMenu->isVisible()) {
        m_fullscreenRadialMenu->raise();
    }
}

void MainWindow::toggleFullscreenIslandMenu() {
    if (!m_fullscreenPresentationActive || !m_fullscreenIslandButton || !m_fullscreenRadialMenu) {
        return;
    }

    if (m_fullscreenIslandSpinAnimation) {
        QVariantAnimation* oldAnimation = m_fullscreenIslandSpinAnimation;
        m_fullscreenIslandSpinAnimation = nullptr;
        oldAnimation->stop();
        oldAnimation->deleteLater();
    }

    const bool closingMenu = m_fullscreenRadialMenu->isOpen();
    auto* animation = new QVariantAnimation(this);
    m_fullscreenIslandSpinAnimation = animation;
    animation->setStartValue(0.0);
    animation->setEndValue(closingMenu ? -360.0 : 360.0);
    animation->setDuration(360);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        if (m_fullscreenIslandButton)
            m_fullscreenIslandButton->setRotationAngle(value.toDouble());
    });
    connect(animation, &QVariantAnimation::finished, this, [this, animation]() {
        if (m_fullscreenIslandSpinAnimation == animation) {
            m_fullscreenIslandSpinAnimation = nullptr;
        }
        if (m_fullscreenIslandButton)
            m_fullscreenIslandButton->setRotationAngle(0.0);
        animation->deleteLater();
    });
    animation->start();

    m_fullscreenRadialMenu->setCloseToolEnabled(ToolManager::instance().isToolActive());
    updateFullscreenChromeGeometry();
    m_fullscreenRadialMenu->toggleMenu();
}

void MainWindow::setupDebugOverlay() {
    m_memUsageLabel = new QLabel(this);
    m_memUsageLabel->setObjectName("DebugOverlay");
    m_memUsageLabel->setStyleSheet("background-color: rgba(0, 0, 0, 150); color: #00FF00; padding: 5px; border-radius: 5px; font-family: Consolas; font-weight: bold;");
    m_memUsageLabel->hide();
    
    m_memTimer = new QTimer(this);
    connect(m_memTimer, &QTimer::timeout, this, &MainWindow::updateMemoryUsage);
    
    if (ConfigManager::instance().getDebugMode()) {
        m_memUsageLabel->show();
        m_memTimer->start(1000);
    }
}

void MainWindow::updateMemoryUsage() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        double memMB = pmc.WorkingSetSize / (1024.0 * 1024.0);
        int fileCount = FileManager::instance().getFileCount();
        m_memUsageLabel->setText(QString("RAM: %1 MB | Files: %2").arg(memMB, 0, 'f', 1).arg(fileCount));
        m_memUsageLabel->adjustSize();
        m_memUsageLabel->move(width() - m_memUsageLabel->width() - 20, height() - m_memUsageLabel->height() - 20);
    }
}

void MainWindow::applyTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    
    const QString contentBg = isDark ? "rgba(30, 30, 32, 0.46)" : "rgba(246, 246, 248, 0.42)";
    const QString chromeBg = isDark ? "rgba(34, 34, 36, 0.54)" : "rgba(246, 246, 248, 0.52)";
    const QString overlayChromeBg = isDark ? "rgba(34, 34, 36, 0.15)" : "rgba(246, 246, 248, 0.17)";
    QString text = isDark ? "#FFFFFF" : "#1D1D1F";
    QString border = isDark ? "rgba(255, 255, 255, 0.12)" : "rgba(60, 60, 67, 0.18)";
    QString rowBg = isDark ? "rgba(44, 44, 46, 0.52)" : "rgba(255, 255, 255, 0.58)";
    QString rowHover = isDark ? "rgba(58, 58, 60, 0.62)" : "rgba(255, 255, 255, 0.70)";
    QString iconBg = isDark ? "rgba(255, 255, 255, 0.08)" : "rgba(118, 118, 128, 0.12)";
    QString treeItemHover = isDark ? "rgba(58, 58, 60, 0.56)" : "rgba(232, 232, 237, 0.58)";
    QString sidebarButtonHover = isDark ? "rgba(255, 255, 255, 0.105)" : "rgba(255, 255, 255, 0.42)";
    QString sidebarButtonHoverBorder = isDark ? "rgba(255, 255, 255, 0.20)" : "rgba(255, 255, 255, 0.72)";
    QString sidebarButtonPressed = isDark ? "rgba(255, 255, 255, 0.155)" : "rgba(255, 255, 255, 0.56)";
    QString accentSolid = isDark ? "#0A84FF" : "#007AFF";
    QString accentGlass = OverlayAcrylicMaterial::accentGlassBrush(isDark);
    QString treeItemSelected = accentGlass;
    QString comboIndicator = isDark ? "#FFFFFF" : "#1D1D1F";
    QString tooltipBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString tooltipBorder = isDark ? "#5C5C5F" : "#D2D2D7";
    QString tooltipText = isDark ? "#F5F5F7" : "#1D1D1F";
    const QString chromeCornerRadius = m_fullscreenPresentationActive ? QStringLiteral("0") : QStringLiteral("10");
    
    QString style = QString(R"(
        QWidget#CentralWidget { background-color: transparent; border: none; }
        QWidget#Sidebar { background-color: %2; border-right: 1px solid %4; border-top-left-radius: @CHROME_CORNER_RADIUS@px; border-bottom-left-radius: @CHROME_CORNER_RADIUS@px; }
        QWidget#OverlayContainer { background-color: %1; border-top-right-radius: @CHROME_CORNER_RADIUS@px; border-bottom-right-radius: @CHROME_CORNER_RADIUS@px; }
        QWidget#Dashboard { background-color: transparent; border-top-right-radius: @CHROME_CORNER_RADIUS@px; border-bottom-right-radius: @CHROME_CORNER_RADIUS@px; }
        QWidget#DashboardContent { background-color: %1; border-top-right-radius: @CHROME_CORNER_RADIUS@px; border-bottom-right-radius: @CHROME_CORNER_RADIUS@px; }

        QLabel { color: %3; }
        QLabel#SidebarTitle { font-size: 16px; font-weight: 800; }
        QLabel#SettingsTitle, QLabel#ConfigTitle, QLabel#ToolsTitle { font-size: 18px; font-weight: bold; }
        
        QToolButton#SidebarButton {
            color: %3;
            background-color: transparent;
            text-align: center;
            padding: 10px;
            border-radius: 8px;
            border: 1px solid transparent;
        }
        QToolButton#SidebarButton:hover {
            background-color: @SIDEBAR_BUTTON_HOVER@;
            border: 1px solid @SIDEBAR_BUTTON_HOVER_BORDER@;
        }
        QToolButton#SidebarButton:pressed {
            background-color: @SIDEBAR_BUTTON_PRESSED@;
            border: 1px solid @SIDEBAR_BUTTON_HOVER_BORDER@;
        }
        
        QWidget#SettingRow {
            background-color: %5; border: 1px solid %4; border-radius: 8px;
        }
        
        QLabel#SettingIcon {
            background-color: %7; border-radius: 10px; color: %3;
        }
        
        QComboBox {
            border: 1px solid %4; 
            border-radius: 6px; 
            padding: 5px 24px 5px 12px; 
            background-color: %5; 
            color: %3;
        }
        QComboBox:hover {
            background-color: %6;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 24px;
            border-left: none;
        }
        QComboBox QAbstractItemView {
            border: 1px solid %4;
            border-radius: 6px;
            background-color: %5;
            color: %3;
            selection-background-color: @ACCENT_GLASS@;
            selection-color: white;
            padding: 4px;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            padding: 6px 0px;
            color: %3;
            text-align: center;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: %6;
            color: %3;
        }

        QPushButton#GithubLink, QPushButton#OpenSourceBtn, QPushButton#LicenseLink {
            color: @ACCENT_SOLID@; text-align: left; background-color: transparent; border: none; font-weight: bold;
        }
        QPushButton#GithubLink:hover, QPushButton#OpenSourceBtn:hover, QPushButton#LicenseLink:hover {
            color: #0051A8;
        }

        QCheckBox::indicator {
            width: 18px; height: 18px; border-radius: 4px; border: 1px solid %4; background-color: %5;
        }
        QCheckBox::indicator:checked {
            background-color: @ACCENT_GLASS@; border: 1px solid @ACCENT_SOLID@;
            image: url(:/checkmark.svg); /* Ideally need a checkmark icon, or just color */
        }
        
        QTreeWidget {
            background-color: transparent; border: none; color: %3;
        }
        QTreeWidget#RightSidebarManagedList {
            background-color: transparent;
            alternate-background-color: transparent;
            icon-size: 10px 7px;
        }
        QTreeWidget#RightSidebarManagedList::viewport {
            background-color: transparent;
        }
        QTreeWidget::item {
            padding: 5px;
        }
        QTreeWidget::item:hover {
            background-color: %8;
        }
        QTreeWidget::item:selected {
            background-color: %9; color: white;
        }
        QHeaderView::section {
            background-color: %6; color: %3; border: none; border-bottom: 1px solid %4; padding: 5px;
        }
        
        QScrollArea {
            background-color: transparent; border: none;
        }
        QScrollArea > QWidget > QWidget {
            background-color: transparent;
        }
        
        QToolTip {
            background-color: %2; color: %3; border: 1px solid %4; padding: 5px; border-radius: 4px;
        }
        
        /* Mac-style context menu */
        QMenu {
            background-color: %5;
            border: 1px solid %4;
            border-radius: 6px;
            padding: 4px 0px;
        }
        QMenu::item {
            padding: 6px 20px;
            color: %3;
            background-color: transparent;
        }
        QMenu::item:selected {
            background-color: @ACCENT_GLASS@;
            color: white;
            border-radius: 4px;
            margin: 2px 4px;
        }
        QMenu::item:disabled {
            color: #888888;
        }
        QMenu::separator {
            height: 1px;
            background-color: %4;
            margin: 4px 8px;
        }
        
        /* Mac-style scrollbar */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 4px 2px 4px 2px;
        }
        QScrollBar::handle:vertical {
            background: rgba(128, 128, 128, 0.4);
            min-height: 30px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: rgba(128, 128, 128, 0.6);
        }
        QScrollBar::handle:vertical:pressed {
            background: rgba(128, 128, 128, 0.8);
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
        }
        
        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 2px 4px 2px 4px;
        }
        QScrollBar::handle:horizontal {
            background: rgba(128, 128, 128, 0.4);
            min-width: 30px;
            border-radius: 3px;
        }
        QScrollBar::handle:horizontal:hover {
            background: rgba(128, 128, 128, 0.6);
        }
        QScrollBar::handle:horizontal:pressed {
            background: rgba(128, 128, 128, 0.8);
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )").arg(contentBg, chromeBg, text, border, rowBg, rowHover, iconBg, treeItemHover, treeItemSelected)
        .replace("@SIDEBAR_BUTTON_HOVER@", sidebarButtonHover)
        .replace("@SIDEBAR_BUTTON_HOVER_BORDER@", sidebarButtonHoverBorder)
        .replace("@SIDEBAR_BUTTON_PRESSED@", sidebarButtonPressed)
        .replace("@COMBO_INDICATOR@", comboIndicator)
        .replace("@ACCENT_GLASS@", accentGlass)
        .replace("@ACCENT_SOLID@", accentSolid)
        .replace("@CHROME_CORNER_RADIUS@", chromeCornerRadius);
    
    setStyleSheet(style);

    QPalette tooltipPalette = QToolTip::palette();
    tooltipPalette.setColor(QPalette::ToolTipBase, QColor(tooltipBg));
    tooltipPalette.setColor(QPalette::ToolTipText, QColor(tooltipText));
    QToolTip::setPalette(tooltipPalette);
    if (qApp) {
        qApp->setStyleSheet(QStringLiteral(
            "QToolTip {"
            "background-color: %1;"
            "color: %2;"
            "border: 1px solid %3;"
            "border-radius: 6px;"
            "padding: 6px 8px;"
            "}"
        ).arg(tooltipBg, tooltipText, tooltipBorder));
    }

    if (m_acrylicBackdrop) {
        m_acrylicBackdrop->setDarkMode(isDark);
        m_acrylicBackdrop->setChromeColors(
            isDark ? QColor(32, 32, 34) : QColor(245, 245, 247),
            isDark ? QColor(255, 255, 255, 31) : QColor(60, 60, 67, 46));
        m_acrylicBackdrop->setChromeCornerRadius(m_fullscreenPresentationActive ? 0.0 : 10.0);
    }
    
    m_toolsBtn->setIcon(loadThemedSvgIcon(":/icons/toolbox.svg", isDark));
    m_accountBtn->setIcon(loadThemedSvgIcon(":/icons/user.svg", isDark));
    m_settingsBtn->setIcon(loadThemedSvgIcon(":/icons/settings.svg", isDark));
    m_configBtn->setIcon(loadThemedSvgIcon(":/icons/folder.svg", isDark));
    if (m_fullscreenIslandButton) {
        m_fullscreenIslandButton->updateTheme(isDark);
    }
    if (m_fullscreenRadialMenu) {
        m_fullscreenRadialMenu->updateTheme(isDark);
        m_fullscreenRadialMenu->setActionIcons(
            loadThemedSvgIcon(":/icons/close-app.svg", isDark),
            loadThemedSvgIcon(":/icons/close-tool.svg", isDark),
            loadThemedSvgIcon(":/icons/exit-fullscreen.svg", isDark));
    }

    m_dashboardContent->setStyleSheet(QString("QWidget#DashboardContent { background-color: %1; border-top-right-radius: %2px; border-bottom-right-radius: %2px; }").arg(contentBg, chromeCornerRadius));

    const QString railButtonColor = isDark ? "#F2F2F7" : "#1D1D1F";
    const QString railButtonHover = isDark ? "rgba(255,255,255,0.08)" : "rgba(0,0,0,0.05)";
    const QString railButtonActive = accentGlass;
    const QString railBackground = overlayChromeBg;
    const QString sidebarPanelBackground = overlayChromeBg;
    const QString inputBackground = isDark ? "rgba(58,58,60,0.52)" : "rgba(255,255,255,0.56)";
    const QColor resizeHandleActive = isDark ? QColor(76, 76, 80) : QColor(210, 210, 216);

    if (auto* panel = dynamic_cast<RightSidebarPanelWidget*>(m_rightSidebarPanel)) {
        panel->setSeparatorColor(isDark ? QColor(255, 255, 255, 31) : QColor(60, 60, 67, 46));
    }

    m_rightSidebarPanel->setStyleSheet(QString(
        "QWidget#RightSidebarPanel { background-color: %1; border-left: none; }"
        "QWidget#RightSidebarHeaderArea { background-color: transparent; border-bottom: none; }"
        "QLabel#RightSidebarTitle { background-color: transparent; color: %3; font-size: 13px; font-weight: 600; min-height: %9px; max-height: %9px; padding: 0 16px; border-bottom: 1px solid %2; }"
        "QWidget#RightSidebarOperationsArea { background-color: transparent; border-bottom: 1px solid %2; }"
        "QWidget#RightSidebarContentContainer { background-color: transparent; }"
        "QWidget#RightSidebarSearchContainer { background-color: transparent; border-bottom: none; }"
        "QWidget#RightSidebarExtraActionsContainer { background-color: transparent; }"
        "QPushButton#RightSidebarSelectAllButton { color: %3; background-color: transparent; border: 1px solid %2; border-radius: 6px; padding: 6px 10px; text-align: left; }"
        "QPushButton#RightSidebarSelectAllButton:hover { background-color: %4; }"
        "QWidget#RightSidebarSearchContainer QLineEdit {"
        " background-color: %7; color: %3; border: 1px solid %2; border-radius: 6px; padding: 6px 10px;"
        "}"
        "QWidget#RightSidebarSearchContainer QLineEdit::placeholder { color: %8; }"
        "QWidget#RightSidebarSearchContainer QComboBox {"
        " background-color: %7; color: %3; border: 1px solid %2; border-radius: 6px; padding: 6px 24px 6px 10px;"
        "}"
    ).arg(sidebarPanelBackground, border, text, railButtonHover, railBackground, railButtonActive, inputBackground, isDark ? "#98989D" : "#6E6E73").arg(kRightSidebarTitleBarHeight));

    m_rightSidebarRail->setStyleSheet(QString(
        "QWidget#RightSidebarRail { background-color: %1; border-left: 1px solid %2; border-top-right-radius: %5px; border-bottom-right-radius: %5px; }"
        "QToolButton#RightSidebarRailButton { border: none; border-radius: 8px; padding: 0px; background-color: transparent; }"
        "QToolButton#RightSidebarRailButton:hover { background-color: %3; }"
        "QToolButton#RightSidebarRailButton:checked { background-color: %4; }"
    ).arg(railBackground, border, railButtonHover, railButtonActive, chromeCornerRadius));

    if (auto* handle = dynamic_cast<RightSidebarResizeHandleWidget*>(m_rightSidebarResizeHandle)) {
        handle->setDragFillColor(resizeHandleActive);
    }
    m_rightSidebarResizeHandle->setStyleSheet(QStringLiteral(
        "QWidget#RightSidebarResizeHandle { background-color: transparent; border: none; }"
        "QWidget#RightSidebarResizeHandle[dragging=\"true\"] { background-color: transparent; border: none; }"
    ));

    for (auto it = m_rightSidebarRailButtons.begin(); it != m_rightSidebarRailButtons.end(); ++it) {
        QToolButton* button = it.value();
        if (!button) {
            continue;
        }

        const QString color = button->isChecked() ? "#FFFFFF" : railButtonColor;
        button->setIcon(loadThemedSvgIcon(button->property("iconResource").toString(), isDark, color));
        button->setIconSize(QSize(20, 20));
    }
}

QIcon MainWindow::loadThemedSvgIcon(const QString& resourcePath, bool isDark, const QString& colorOverride) {
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QIcon(resourcePath);
    }

    QString svgContent = QTextStream(&file).readAll();
    file.close();

    const QString color = colorOverride.isEmpty() ? (isDark ? "#FFFFFF" : "#1D1D1F") : colorOverride;
    svgContent.replace("currentColor", color);

    QPixmap pixmap;
    pixmap.loadFromData(svgContent.toUtf8(), "SVG");
    return QIcon(pixmap);
}

void MainWindow::setupRightSidebarUi() {
    if (!m_rightSidebarTitleLabel || !m_rightSidebarSearchEdit || !m_rightSidebarSelectAllBtn) {
        return;
    }

    LocalizationManager& loc = LocalizationManager::instance();
    m_rightSidebarTitleLabel->setText(loc.getString("MainWindow", "RightSidebarDefaultTitle"));
    m_rightSidebarSearchEdit->setPlaceholderText(loc.getString("MainWindow", "RightSidebarSearchPlaceholder"));
    m_rightSidebarSearchColumnCombo->setPlaceholderText(loc.getString("MainWindow", "RightSidebarSearchAllColumns"));
    m_rightSidebarSelectAllBtn->setText(loc.getString("MainWindow", "RightSidebarSelectAll"));
    m_rightSidebarSearchContainer->hide();
    m_rightSidebarExtraActionsContainer->hide();
    m_rightSidebarListWidth = kRightSidebarDefaultListWidth;
    m_rightSidebarListVisible = false;
    m_rightSidebarActiveButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    m_rightSidebarLastModeId.clear();
    m_rightSidebarPanel->hide();
    m_rightSidebarResizeHandle->hide();
    m_rightSidebarRail->show();

    ToolRightSidebarState emptyState;
    emptyState.listVisible = false;
    emptyState.searchModeAvailable = false;
    emptyState.searchModeActive = false;
    emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    rebuildRightSidebarRail({}, emptyState);
    updateRightSidebarGeometries();
}

void MainWindow::clearRightSidebarContent() {
    if (!m_rightSidebarContentLayout || !m_rightSidebarContentContainer) {
        m_activeScriptedSidebarWidget = nullptr;
        m_activeRightSidebarListWidget = nullptr;
        m_rightSidebarContentToolId.clear();
        return;
    }

    QLayoutItem *child = nullptr;
    while ((child = m_rightSidebarContentLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            QWidget* widget = child->widget();
            widget->hide();
            if (widget == m_activeScriptedSidebarWidget) {
                m_activeScriptedSidebarWidget = nullptr;
            }
            delete widget;  // Use immediate delete instead of deleteLater to prevent memory buildup
        }
        delete child;
    }
    const QList<QWidget*> remainingContentWidgets =
        m_rightSidebarContentContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* widget : remainingContentWidgets) {
        if (!widget) {
            continue;
        }
        widget->hide();
        if (widget == m_activeScriptedSidebarWidget) {
            m_activeScriptedSidebarWidget = nullptr;
        }
        delete widget;
    }
    m_activeRightSidebarListWidget = nullptr;
    m_rightSidebarContentToolId.clear();
}

void MainWindow::resetRightSidebarForToolTransition() {
    if (!m_rightSidebarContentLayout) {
        return;
    }

    clearRightSidebarContent();
    m_rightSidebarEffectiveIconCache.clear();
    m_rightSidebarSearchText.clear();
    m_rightSidebarLastModeId.clear();
    m_rightSidebarRestoreRequested = false;
    m_rightSidebarActiveButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);

    if (m_rightSidebarSearchEdit) {
        m_rightSidebarSearchEdit->clear();
    }
    if (m_rightSidebarSearchColumnCombo) {
        const QSignalBlocker blocker(m_rightSidebarSearchColumnCombo);
        m_rightSidebarSearchColumnCombo->clear();
    }
    m_rightSidebarSearchComboToColumn.clear();

    ToolRightSidebarState emptyState;
    emptyState.listVisible = false;
    emptyState.searchModeAvailable = false;
    emptyState.searchModeActive = false;
    emptyState.showSelectAllButton = false;
    emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    rebuildRightSidebarRail({}, emptyState);

    if (m_rightSidebarPanel) {
        m_rightSidebarPanel->hide();
    }
    if (m_rightSidebarResizeHandle) {
        m_rightSidebarResizeHandle->hide();
    }
    if (m_rightSidebarRail) {
        m_rightSidebarRail->show();
    }
    m_rightSidebarListVisible = false;
    updateRightSidebarGeometries();
}

void MainWindow::rebuildRightSidebarRail(const QList<ToolRightSidebarButtonDefinition>& definitions,
                                         const ToolRightSidebarState& state) {
    while (QLayoutItem* item = m_rightSidebarRailLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_rightSidebarRailButtons.clear();

    const bool isDark = ConfigManager::instance().isCurrentThemeDark();
    QMap<QString, ToolRightSidebarButtonDefinition> definitionMap;
    QStringList orderedKeys;

    const bool hasManagedList = (m_activeRightSidebarListWidget != nullptr)
        || state.listVisible
        || state.searchModeAvailable
        || state.showSelectAllButton
        || !state.searchableColumns.isEmpty();

    if (hasManagedList) {
        ToolRightSidebarButtonDefinition defaultButton;
        defaultButton.key = QString::fromUtf8(kRightSidebarDefaultButtonKey);
        defaultButton.iconResource = ":/icons/sidebar.svg";
        defaultButton.tooltip = LocalizationManager::instance().getString("MainWindow", "RightSidebarDefaultButtonTooltip");
        definitionMap.insert(defaultButton.key, defaultButton);
        orderedKeys.append(defaultButton.key);
    }

    if (state.searchModeAvailable) {
        ToolRightSidebarButtonDefinition searchButton;
        searchButton.key = QString::fromUtf8(kRightSidebarSearchButtonKey);
        searchButton.iconResource = ":/icons/search.svg";
        searchButton.tooltip = LocalizationManager::instance().getString("MainWindow", "RightSidebarSearchButtonTooltip");
        definitionMap.insert(searchButton.key, searchButton);
        orderedKeys.append(searchButton.key);
    }

    for (const ToolRightSidebarButtonDefinition& definition : definitions) {
        if (definition.key.isEmpty()) {
            continue;
        }
        definitionMap.insert(definition.key, definition);
        if (!orderedKeys.contains(definition.key)) {
            orderedKeys.append(definition.key);
        }
    }

    if (!state.orderedButtonKeys.isEmpty()) {
        QStringList mergedKeys;
        for (const QString& key : state.orderedButtonKeys) {
            if (definitionMap.contains(key) && !mergedKeys.contains(key)) {
                mergedKeys.append(key);
            }
        }
        for (const QString& key : orderedKeys) {
            if (!mergedKeys.contains(key)) {
                mergedKeys.append(key);
            }
        }
        orderedKeys = mergedKeys;
    }

    for (const QString& key : orderedKeys) {
        const ToolRightSidebarButtonDefinition definition = definitionMap.value(key);
        QToolButton* button = new QToolButton(m_rightSidebarRail);
        button->setObjectName("RightSidebarRailButton");
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(definition.tooltip);
        button->setFixedSize(44, 44);
        button->setProperty("iconResource", definition.iconResource);
        m_rightSidebarRailLayout->addWidget(button, 0, Qt::AlignHCenter);
        m_rightSidebarRailButtons.insert(definition.key, button);

        connect(button, &QToolButton::clicked, this, [this, key = definition.key]() {
            if (!m_activeScriptedHostController) {
                return;
            }
            if (key == QString::fromUtf8(kRightSidebarDefaultButtonKey)) {
                activateDefaultRightSidebarList(true, true);
                return;
            }
            if (currentRightSidebarListWidth() <= 0) {
                m_rightSidebarRestoreRequested = true;
            }
            m_rightSidebarActiveButtonKey = key;
            // Handle sidebar button click through scripted controller
            m_activeScriptedHostController->applyAction("sidebar_button_click", key);
            refreshActiveToolRightSidebarUi();
        });
    }

    m_rightSidebarRailLayout->addStretch(1);
    updateRightSidebarState(state);

    for (auto it = m_rightSidebarRailButtons.begin(); it != m_rightSidebarRailButtons.end(); ++it) {
        QToolButton* button = it.value();
        if (!button) {
            continue;
        }
        const QString color = button->isChecked() ? "#FFFFFF" : (isDark ? "#F2F2F7" : "#1D1D1F");
        button->setIcon(loadThemedSvgIcon(button->property("iconResource").toString(), isDark, color));
        button->setIconSize(QSize(20, 20));
    }
}

void MainWindow::activateDefaultRightSidebarList(bool notifyTool, bool refreshUi) {
    if (currentRightSidebarListWidth() <= 0) {
        m_rightSidebarRestoreRequested = true;
    }

    const QString defaultButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    m_rightSidebarActiveButtonKey = defaultButtonKey;
    m_rightSidebarSearchText.clear();
    if (m_rightSidebarSearchEdit) {
        m_rightSidebarSearchEdit->clear();
    }
    if (m_rightSidebarSearchColumnCombo) {
        m_rightSidebarSearchColumnCombo->setCurrentIndex(0);
    }

    if (notifyTool && m_activeScriptedHostController) {
        m_activeScriptedHostController->applyAction("sidebar_button_click", defaultButtonKey);
    }
    if (refreshUi) {
        refreshActiveToolRightSidebarUi();
        applyRightSidebarSearchFilter();
    }
}

void MainWindow::setRightSidebarListVisible(bool visible) {
    m_rightSidebarListVisible = visible;
    m_rightSidebarPanel->setVisible(visible);
    m_rightSidebarResizeHandle->setVisible(visible);
    m_rightSidebarRail->setVisible(true);
    updateRightSidebarGeometries();
}

void MainWindow::setRightSidebarSearchVisible(bool visible) {
    m_rightSidebarSearchContainer->setVisible(visible);
    if (!visible) {
        m_rightSidebarSearchText.clear();
        if (m_rightSidebarSearchEdit) {
            m_rightSidebarSearchEdit->clear();
        }
        if (m_rightSidebarSearchColumnCombo && m_rightSidebarSearchColumnCombo->count() > 0) {
            m_rightSidebarSearchColumnCombo->setCurrentIndex(0);
        }
    }
}

void MainWindow::setRightSidebarExtraActionsVisible(bool visible) {
    m_rightSidebarExtraActionsContainer->setVisible(visible);
}

void MainWindow::updateRightSidebarSelectAllButtonText() {
    LocalizationManager& loc = LocalizationManager::instance();
    if (!m_activeRightSidebarListWidget) {
        m_rightSidebarSelectAllBtn->setText(loc.getString("MainWindow", "RightSidebarSelectAll"));
        return;
    }

    const bool hasItems = m_activeRightSidebarListWidget->topLevelItemCount() > 0;
    bool allSelected = hasItems;
    for (int i = 0; i < m_activeRightSidebarListWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_activeRightSidebarListWidget->topLevelItem(i);
        if (!item || item->isHidden()) {
            continue;
        }
        if (!item->isSelected()) {
            allSelected = false;
            break;
        }
    }

    m_rightSidebarSelectAllBtn->setText(
        allSelected ? loc.getString("MainWindow", "RightSidebarDeselectAll")
                    : loc.getString("MainWindow", "RightSidebarSelectAll")
    );
}

void MainWindow::updateRightSidebarState(const ToolRightSidebarState& state) {
    LocalizationManager& loc = LocalizationManager::instance();
    const QString titleText = state.title.isEmpty()
        ? loc.getString("MainWindow", "RightSidebarDefaultTitle")
        : state.title;
    m_rightSidebarTitleLabel->setText(titleText);
    m_rightSidebarTitleLabel->setToolTip(titleText);
    m_rightSidebarTitleLabel->setVisible(state.listVisible);
    m_rightSidebarTitleLabel->setMinimumHeight(kRightSidebarTitleBarHeight);
    m_rightSidebarTitleLabel->setFixedHeight(kRightSidebarTitleBarHeight);
    if (state.listVisible) {
        m_rightSidebarTitleLabel->raise();
    }
    if (m_rightSidebarHeaderArea) {
        m_rightSidebarHeaderArea->setVisible(state.listVisible);
        m_rightSidebarHeaderArea->setMinimumHeight(kRightSidebarTitleBarHeight);
    }

    const bool showSearch = state.searchModeAvailable && state.searchModeActive;
    setRightSidebarSearchVisible(showSearch);
    setRightSidebarExtraActionsVisible(showSearch && state.showSelectAllButton);
    setRightSidebarListVisible(state.listVisible);

    {
        const QSignalBlocker blocker(m_rightSidebarSearchColumnCombo);
        m_rightSidebarSearchComboToColumn.clear();
        m_rightSidebarSearchColumnCombo->clear();

        if (state.searchableColumns.size() > 1) {
            m_rightSidebarSearchColumnCombo->addItem(loc.getString("MainWindow", "RightSidebarSearchAllColumns"));
            m_rightSidebarSearchComboToColumn.insert(0, -1);
            for (int i = 0; i < state.searchableColumns.size(); ++i) {
                const QString label = i < state.searchableColumnLabels.size()
                    ? state.searchableColumnLabels[i]
                    : QString::number(state.searchableColumns[i]);
                const int comboIndex = m_rightSidebarSearchColumnCombo->count();
                m_rightSidebarSearchColumnCombo->addItem(label);
                m_rightSidebarSearchComboToColumn.insert(comboIndex, state.searchableColumns[i]);
            }
            m_rightSidebarSearchColumnCombo->show();
        } else {
            m_rightSidebarSearchColumnCombo->hide();
        }
    }

    const QString effectiveActiveButtonKey = state.activeButtonKey.isEmpty()
        ? (m_rightSidebarActiveButtonKey.isEmpty()
            ? QString::fromUtf8(kRightSidebarDefaultButtonKey)
            : m_rightSidebarActiveButtonKey)
        : state.activeButtonKey;

    for (auto it = m_rightSidebarRailButtons.begin(); it != m_rightSidebarRailButtons.end(); ++it) {
        QToolButton* button = it.value();
        if (!button) {
            continue;
        }

        button->setChecked(it.key() == effectiveActiveButtonKey);
    }

    applyRightSidebarSearchFilter();
    updateRightSidebarSelectAllButtonText();
}

QTreeWidget* MainWindow::currentRightSidebarListWidget() const {
    return m_activeRightSidebarListWidget;
}

void MainWindow::onRightSidebarSearchTextChanged(const QString& text) {
    m_rightSidebarSearchText = text;
    applyRightSidebarSearchFilter();
}

void MainWindow::onRightSidebarSearchColumnChanged(int) {
    applyRightSidebarSearchFilter();
}

void MainWindow::applyRightSidebarSearchFilter() {
    if (!m_activeRightSidebarListWidget) {
        return;
    }

    const bool searchVisible = m_rightSidebarSearchContainer && m_rightSidebarSearchContainer->isVisible();
    const QString needle = searchVisible ? m_rightSidebarSearchText.trimmed() : QString();
    const int selectedColumn = m_rightSidebarSearchComboToColumn.value(m_rightSidebarSearchColumnCombo->currentIndex(), -1);

    for (int i = 0; i < m_activeRightSidebarListWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_activeRightSidebarListWidget->topLevelItem(i);
        if (!item) {
            continue;
        }

        bool matches = needle.isEmpty();
        if (!needle.isEmpty()) {
            const int columnCount = qMax(1, m_activeRightSidebarListWidget->columnCount());
            if (selectedColumn >= 0 && selectedColumn < columnCount) {
                matches = item->text(selectedColumn).contains(needle, Qt::CaseInsensitive);
            } else {
                for (int column = 0; column < columnCount; ++column) {
                    if (item->text(column).contains(needle, Qt::CaseInsensitive)) {
                        matches = true;
                        break;
                    }
                }
            }
        }

        item->setHidden(!matches);
    }

    updateRightSidebarSelectAllButtonText();
}

void MainWindow::onRightSidebarSelectAllClicked() {
    if (!m_activeRightSidebarListWidget) {
        return;
    }

    bool shouldSelectAll = false;
    for (int i = 0; i < m_activeRightSidebarListWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_activeRightSidebarListWidget->topLevelItem(i);
        if (item && !item->isHidden() && !item->isSelected()) {
            shouldSelectAll = true;
            break;
        }
    }

    for (int i = 0; i < m_activeRightSidebarListWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_activeRightSidebarListWidget->topLevelItem(i);
        if (item && !item->isHidden()) {
            item->setSelected(shouldSelectAll);
        }
    }

    updateRightSidebarSelectAllButtonText();
}

QString MainWindow::activeToolLocalizedString(const QString& key, const QString& fallback) const {
    const QString effectiveFallback = fallback.isEmpty() ? key : fallback;
    if (!m_activeTool) {
        return effectiveFallback;
    }

    return m_activeTool->localizedStrings().value(key, effectiveFallback);
}

void MainWindow::handleV2RightSidebarItemActivated(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);

    if (!item || !m_activeScriptedHostController) {
        return;
    }

    const QString targetId = item->data(0, Qt::UserRole).toString().trimmed();
    if (targetId.isEmpty()) {
        return;
    }

    const QVariantMap rowValues = item->data(0, Qt::UserRole + 2).toMap();
    QVariantMap arguments;
    arguments.insert(QStringLiteral("rowId"), targetId);
    arguments.insert(
        QStringLiteral("displayName"),
        rowValues.value(QStringLiteral("displayName"), targetId)
    );
    const QVariantMap rowState = item->data(0, Qt::UserRole + 1).toMap();
    const QString actionType = firstNonEmptyString(
        rowState,
        {QStringLiteral("selectAction"), QStringLiteral("activationAction"), QStringLiteral("action")}
    );
    m_activeScriptedHostController->applyAction(
        actionType.isEmpty() ? QStringLiteral("select_file") : actionType,
        targetId,
        arguments
    );
}

void MainWindow::handleV2RightSidebarContextMenuRequested(const QPoint& pos) {
    if (!m_activeRightSidebarListWidget || !m_activeScriptedHostController) {
        return;
    }

    QTreeWidgetItem* item = m_activeRightSidebarListWidget->itemAt(pos);
    if (!item) {
        return;
    }

    const QString targetId = item->data(0, Qt::UserRole).toString().trimmed();
    if (targetId.isEmpty()) {
        return;
    }

    const QVariantMap rowState = item->data(0, Qt::UserRole + 1).toMap();
    const QVariantMap rowValues = item->data(0, Qt::UserRole + 2).toMap();
    QMenu menu(this);
    const ToolGuiSessionState& session = m_activeScriptedHostController->sessionState();
    QString modelId = m_activeRightSidebarListWidget
        ? m_activeRightSidebarListWidget->property("toolModelId").toString().trimmed()
        : QString();
    if (modelId.isEmpty()) {
        modelId = session.sidebar.activeMode.trimmed();
    }
    QVariantList contextActions;
    if (!modelId.isEmpty() && session.lists.contains(modelId)) {
        contextActions = session.lists.value(modelId).contextActions;
    }

    QList<QAction*> customActions;
    QList<QVariantMap> customActionMaps;
    for (const QVariant& actionValue : contextActions) {
        const QVariantMap actionMap = actionValue.toMap();
        const QString actionId = firstNonEmptyString(
            actionMap,
            {QStringLiteral("actionId"), QStringLiteral("id"), QStringLiteral("action")}
        );
        if (actionId.isEmpty()) {
            continue;
        }

        const QString visibleWhen = actionMap.value(QStringLiteral("visibleWhen")).toString().trimmed();
        if (!visibleWhen.isEmpty() && !truthyVariant(rowState, visibleWhen)) {
            continue;
        }

        if (actionMap.value(QStringLiteral("separator")).toBool()) {
            menu.addSeparator();
            continue;
        }

        const QString text = actionMap.value(QStringLiteral("text")).toString().trimmed();
        QAction* action = menu.addAction(
            text.isEmpty()
                ? activeToolLocalizedString(actionId, actionId)
                : text
        );
        action->setEnabled(actionMap.value(QStringLiteral("enabled"), true).toBool());
        customActions.append(action);
        customActionMaps.append(actionMap);
    }

    if (!customActions.isEmpty()) {
        QAction* selectedAction = menu.exec(m_activeRightSidebarListWidget->viewport()->mapToGlobal(pos));
        const int selectedIndex = customActions.indexOf(selectedAction);
        if (selectedIndex >= 0 && selectedIndex < customActionMaps.size()) {
            const QVariantMap actionMap = customActionMaps.at(selectedIndex);
            QVariantMap arguments;
            arguments.insert(QStringLiteral("rowId"), targetId);
            arguments.insert(QStringLiteral("displayName"), rowValues.value(QStringLiteral("displayName"), targetId));
            arguments.insert(QStringLiteral("values"), rowValues);
            arguments.insert(QStringLiteral("state"), rowState);
            m_activeScriptedHostController->applyAction(
                firstNonEmptyString(actionMap, {QStringLiteral("actionId"), QStringLiteral("id"), QStringLiteral("action")}),
                targetId,
                arguments
            );
        }
        return;
    }

    Q_UNUSED(rowState);
    Q_UNUSED(rowValues);
}

void MainWindow::syncV2RightSidebarContent() {
    if (!m_activeScriptedHostController || !m_rightSidebarContentLayout) {
        clearRightSidebarContent();
        return;
    }

    const QString activeToolId = m_activeTool ? m_activeTool->id() : QString();
    if (m_activeRightSidebarListWidget) {
        const QString ownerToolId = m_activeRightSidebarListWidget->property("toolOwnerId").toString();
        if (!activeToolId.isEmpty() && ownerToolId != activeToolId) {
            clearRightSidebarContent();
        }
    }

    const ToolGuiSessionState& session = m_activeScriptedHostController->sessionState();

    QString modelId = session.sidebar.activeMode.trimmed();
    if (modelId.isEmpty() || !session.lists.contains(modelId)) {
        if (session.lists.contains(QStringLiteral("file_list"))) {
            modelId = QStringLiteral("file_list");
        } else if (!session.lists.isEmpty()) {
            modelId = session.lists.firstKey();
        }
    }

    if (modelId.isEmpty() || !session.lists.contains(modelId)) {
        clearRightSidebarContent();
        return;
    }

    const ToolGuiListModel model = session.lists.value(modelId);
    if (modelId != QStringLiteral("tag_list")) {
        m_rightSidebarEffectiveIconCache.clear();
    }
    const bool extendedSelection =
        model.selectionMode.compare(QStringLiteral("extended"), Qt::CaseInsensitive) == 0
        || model.selectionMode.compare(QStringLiteral("multi"), Qt::CaseInsensitive) == 0
        || model.selectionMode.compare(QStringLiteral("multiple"), Qt::CaseInsensitive) == 0;

    QTreeWidget* listWidget = qobject_cast<QTreeWidget*>(m_activeRightSidebarListWidget);
    const QString existingModelId = listWidget ? listWidget->property("toolModelId").toString().trimmed() : QString();
    const QString existingOwnerToolId = listWidget ? listWidget->property("toolOwnerId").toString() : QString();
    bool needsRecreate = false;
    const bool traceFlagSidebar = m_activeTool && m_activeTool->name() == QStringLiteral("FlagManagerTool");

    if (!listWidget) {
        needsRecreate = true;
    } else {
        const int requiredColumns = qMax(1, model.columns.size());
        if (!activeToolId.isEmpty() && existingOwnerToolId != activeToolId) {
            needsRecreate = true;
        }
        if (existingModelId != modelId) {
            needsRecreate = true;
        }
        if (listWidget->columnCount() != requiredColumns ||
            listWidget->isHeaderHidden() != (model.headerHidden || model.columns.size() <= 1)) {
            needsRecreate = true;
        }
    }
    if (traceFlagSidebar) {
        Logger::instance().logInfo(
            "MainWindow",
            QStringLiteral("[FLAG_SIDEBAR_RENDER] sidebarModel=%1 existingModel=%2 lists=%3 rows=%4 recreate=%5")
                .arg(modelId)
                .arg(existingModelId)
                .arg(sidebarListIds(session.lists))
                .arg(model.rows.size())
                .arg(needsRecreate ? QStringLiteral("true") : QStringLiteral("false"))
        );
    }

    if (needsRecreate) {
        clearRightSidebarContent();
        listWidget = new QTreeWidget(m_rightSidebarContentContainer);
        listWidget->setObjectName(QStringLiteral("RightSidebarManagedList"));
        listWidget->setColumnCount(qMax(1, model.columns.size()));
        listWidget->setRootIsDecorated(false);
        listWidget->setIndentation(0);
        listWidget->setFrameShape(QFrame::NoFrame);
        listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        listWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        listWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        listWidget->setSelectionMode(extendedSelection ? QAbstractItemView::ExtendedSelection
                                                       : QAbstractItemView::SingleSelection);
        listWidget->setAllColumnsShowFocus(true);
        listWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listWidget->setProperty("toolModelId", modelId);
        listWidget->setProperty("toolOwnerId", activeToolId);
        listWidget->setUniformRowHeights(true);
        applyTransparentTreePalette(listWidget);
        applyRightSidebarHeaderLabels(listWidget, model);

        for (int column = 0; column < model.columns.size(); ++column) {
            const ToolGuiListColumnDefinition& definition = model.columns.at(column);
            if (definition.width > 0) {
                listWidget->setColumnWidth(column, definition.width);
            }
            listWidget->setColumnHidden(column, definition.hidden);
        }

        connect(listWidget, &QTreeWidget::itemClicked,
                this, &MainWindow::handleV2RightSidebarItemActivated);
        connect(listWidget, &QTreeWidget::itemSelectionChanged,
                this, &MainWindow::updateRightSidebarSelectAllButtonText);
        connect(listWidget, &QTreeWidget::customContextMenuRequested,
                this, &MainWindow::handleV2RightSidebarContextMenuRequested);

        m_rightSidebarContentLayout->addWidget(listWidget);
        m_activeRightSidebarListWidget = listWidget;
    } else {
        listWidget->setProperty("toolOwnerId", activeToolId);
        listWidget->clear();
        listWidget->setSelectionMode(extendedSelection ? QAbstractItemView::ExtendedSelection
                                                       : QAbstractItemView::SingleSelection);
        applyTransparentTreePalette(listWidget);
        applyRightSidebarHeaderLabels(listWidget, model);
        for (int column = 0; column < model.columns.size(); ++column) {
            const ToolGuiListColumnDefinition& definition = model.columns.at(column);
            if (definition.width > 0) {
                listWidget->setColumnWidth(column, definition.width);
            }
            listWidget->setColumnHidden(column, definition.hidden);
        }
    }
    m_rightSidebarContentToolId = activeToolId;

    const int columnCount = qMax(1, listWidget->columnCount());
    bool hasRowIcons = false;
    QSize rowIconSize;

    QSignalBlocker blocker(listWidget);

    QTreeWidgetItem* itemToSelect = nullptr;
    for (const ToolGuiListRow& row : model.rows) {
        QTreeWidgetItem* item = new QTreeWidgetItem(listWidget);

        for (int column = 0; column < columnCount; ++column) {
            QString text;
            if (column < row.cells.size()) {
                text = row.cells.at(column).value.toString();
            } else if (column < model.columns.size()) {
                text = row.values.value(model.columns.at(column).id).toString();
            } else if (column == 0) {
                text = row.values.value(QStringLiteral("name")).toString();
                if (text.trimmed().isEmpty()) {
                    text = row.values.value(QStringLiteral("displayNameUi")).toString();
                }
            }
            item->setText(column, text);
        }

        const QString targetId = row.rowId.trimmed().isEmpty() ? row.id : row.rowId;
        item->setData(0, Qt::UserRole, targetId);
        item->setData(0, Qt::UserRole + 1, row.state);
        item->setData(0, Qt::UserRole + 2, row.values);

        const bool isLatest = row.role == QStringLiteral("latest")
            || row.state.value(QStringLiteral("is_latest")).toBool();
        const bool isCurrent = row.state.value(QStringLiteral("is_current")).toBool();
        const bool isSelected = isCurrent || row.state.value(QStringLiteral("selected")).toBool();
        const QString textColor = row.state.value(QStringLiteral("textColor")).toString().trimmed();
        const QString foregroundColor = textColor.isEmpty()
            ? row.state.value(QStringLiteral("foreground")).toString().trimmed()
            : textColor;
        const QString backgroundColor = row.state.value(QStringLiteral("backgroundColor")).toString().trimmed();
        QIcon rowIcon = iconFromBase64Png(row.state.value(QStringLiteral("iconBase64")).toString());
        if (rowIcon.isNull()) {
            const QString effectiveIconPath = firstNonEmptyString(
                row.state,
                {
                    QStringLiteral("iconEffectivePath"),
                    QStringLiteral("effectiveIconPath"),
                    QStringLiteral("iconLogicalPath")
                }
            );
            if (!effectiveIconPath.isEmpty()) {
                const QString cacheKey = QStringLiteral("%1|%2").arg(modelId, effectiveIconPath);
                const auto cachedIcon = m_rightSidebarEffectiveIconCache.constFind(cacheKey);
                if (cachedIcon != m_rightSidebarEffectiveIconCache.constEnd()) {
                    rowIcon = cachedIcon.value();
                } else {
                    const ToolRuntimeContext::FileReadResult readResult =
                        ToolRuntimeContext::instance().readEffectiveFile(effectiveIconPath);
                    if (readResult.success) {
                        const QImage image = tgaImageFromData(readResult.content);
                        if (!image.isNull()) {
                            rowIcon = QIcon(QPixmap::fromImage(image));
                            if (m_rightSidebarEffectiveIconCache.size() > 512) {
                                m_rightSidebarEffectiveIconCache.clear();
                            }
                            m_rightSidebarEffectiveIconCache.insert(cacheKey, rowIcon);
                        }
                    }
                }
            }
        }
        if (!rowIcon.isNull()) {
            item->setIcon(0, rowIcon);
            hasRowIcons = true;
            int iconWidth = row.state.value(QStringLiteral("iconWidth")).toInt();
            int iconHeight = row.state.value(QStringLiteral("iconHeight")).toInt();
            if (iconWidth <= 0 || iconHeight <= 0) {
                const QList<QSize> availableSizes = rowIcon.availableSizes();
                if (!availableSizes.isEmpty()) {
                    iconWidth = availableSizes.first().width();
                    iconHeight = availableSizes.first().height();
                }
            }
            if (iconWidth > 0 && iconHeight > 0) {
                rowIconSize.setWidth(qMax(rowIconSize.width(), iconWidth));
                rowIconSize.setHeight(qMax(rowIconSize.height(), iconHeight));
            }
        }

        for (int column = 0; column < columnCount; ++column) {
            if (!foregroundColor.isEmpty()) {
                item->setForeground(column, QColor(foregroundColor));
            } else if (isLatest) {
                item->setForeground(column, QColor(QStringLiteral("#007AFF")));
            } else {
                item->setForeground(column, QBrush());
            }
            if (!backgroundColor.isEmpty()) {
                item->setBackground(column, QColor(backgroundColor));
            } else {
                item->setBackground(column, QBrush());
            }
        }

        if (isSelected) {
            itemToSelect = item;
        }
    }

    blocker.unblock();
    if (itemToSelect) {
        listWidget->setCurrentItem(itemToSelect);
        itemToSelect->setSelected(true);
    }
    if (hasRowIcons && rowIconSize.isValid()) {
        listWidget->setIconSize(rowIconSize);
    } else {
        listWidget->setIconSize(QSize(16, 16));
    }
    listWidget->setStyleSheet(QString());
    applyTransparentTreePalette(listWidget);
    listWidget->show();
}

void MainWindow::logNativeInputState(const QString& reason) const {
    if (!kVerboseToolUiLogging) {
        Q_UNUSED(reason);
        return;
    }

    const Qt::WindowFlags flags = windowFlags();
    Logger::instance().logInfo("MainWindow", QString("[INPUT_CHAIN] Native input state (%1): winId=0x%2 flags=0x%3 transparentForInputFlag=%4 waTransparentForMouse=%5 waTranslucent=%6")
        .arg(reason)
        .arg(reinterpret_cast<quintptr>(const_cast<MainWindow*>(this)->winId()), 0, 16)
        .arg(static_cast<qulonglong>(flags), 0, 16)
        .arg(boolText(flags.testFlag(Qt::WindowTransparentForInput)))
        .arg(boolText(testAttribute(Qt::WA_TransparentForMouseEvents)))
        .arg(boolText(testAttribute(Qt::WA_TranslucentBackground))));

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(const_cast<MainWindow*>(this)->winId());
    if (hwnd) {
        const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        Logger::instance().logInfo("MainWindow", QString("[INPUT_CHAIN] HWND style (%1): style=0x%2 exStyle=0x%3 layered=%4 transparent=%5")
            .arg(reason)
            .arg(static_cast<qulonglong>(style), 0, 16)
            .arg(static_cast<qulonglong>(exStyle), 0, 16)
            .arg(boolText((exStyle & WS_EX_LAYERED) != 0))
            .arg(boolText((exStyle & WS_EX_TRANSPARENT) != 0)));
    }
#endif
}

void MainWindow::ensureNativeWindowAcceptsInput(const QString& reason) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    if (windowFlags().testFlag(Qt::WindowTransparentForInput)) {
        setWindowFlag(Qt::WindowTransparentForInput, false);
    }

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        const LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        if ((exStyle & WS_EX_TRANSPARENT) != 0) {
            SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            Logger::instance().logWarning("MainWindow", QString("[INPUT_CHAIN] Removed WS_EX_TRANSPARENT from MainWindow HWND during %1").arg(reason));
        }
    }
#endif

    logNativeInputState(reason);
}

void MainWindow::applyNativeRoundedCorners(const QString& reason) {
#ifdef Q_OS_WIN
    clearMask();
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (m_fullscreenPresentationActive || isFullScreen()) {
        setDwmSquareWindowCorners(hwnd);
        return;
    }
    const bool applied = setDwmRoundedWindowCorners(hwnd);
    static bool loggedSuccess = false;
    static bool loggedFailure = false;
    if (applied) {
        if (!loggedSuccess) {
            Logger::instance().logInfo("MainWindow", QString("Native rounded corners enabled via DWM during %1").arg(reason));
            loggedSuccess = true;
        }
        return;
    }

    if (!loggedFailure) {
        Logger::instance().logWarning("MainWindow", QString("DWM rounded corners are unavailable during %1; keeping opaque rectangular HWND.").arg(reason));
        loggedFailure = true;
    }
#else
    Q_UNUSED(reason);
    clearMask();
#endif
}

void MainWindow::logToolInputChain(const QString& reason) const {
    if (!kVerboseToolUiLogging) {
        Q_UNUSED(reason);
        return;
    }

    Logger::instance().logInfo("MainWindow", QString("[INPUT_CHAIN] Tool input chain (%1)").arg(reason));
    const QWidget* widgets[] = {m_centralWidget, m_mainStack, m_dashboard, m_dashboardContent};
    for (const QWidget* widget : widgets) {
        if (!widget) {
            continue;
        }
        Logger::instance().logInfo("MainWindow", QString("[INPUT_CHAIN]   %1 enabled=%2 visible=%3 waTransparentForMouse=%4 waTranslucent=%5 geometry=%6x%7+%8+%9")
            .arg(widgetDebugName(widget))
            .arg(boolText(widget->isEnabled()))
            .arg(boolText(widget->isVisible()))
            .arg(boolText(widget->testAttribute(Qt::WA_TransparentForMouseEvents)))
            .arg(boolText(widget->testAttribute(Qt::WA_TranslucentBackground)))
            .arg(widget->width())
            .arg(widget->height())
            .arg(widget->x())
            .arg(widget->y()));
    }

    if (m_dashboardContent && m_dashboardContent->layout()) {
        for (int i = 0; i < m_dashboardContent->layout()->count(); ++i) {
            const QLayoutItem* item = m_dashboardContent->layout()->itemAt(i);
            const QWidget* widget = item ? item->widget() : nullptr;
            if (!widget) {
                continue;
            }
            Logger::instance().logInfo("MainWindow", QString("[INPUT_CHAIN]   content[%1] %2 enabled=%3 visible=%4 waTransparentForMouse=%5 waTranslucent=%6 geometry=%7x%8+%9+%10")
                .arg(i)
                .arg(widgetDebugName(widget))
                .arg(boolText(widget->isEnabled()))
                .arg(boolText(widget->isVisible()))
                .arg(boolText(widget->testAttribute(Qt::WA_TransparentForMouseEvents)))
                .arg(boolText(widget->testAttribute(Qt::WA_TranslucentBackground)))
                .arg(widget->width())
                .arg(widget->height())
                .arg(widget->x())
                .arg(widget->y()));
        }
    }

    const QPoint cursorPos = QCursor::pos();
    QWidget* widgetAtCursor = QApplication::widgetAt(cursorPos);
    Logger::instance().logInfo("MainWindow", QString("[INPUT_CHAIN]   cursor=(%1,%2) widgetAtCursor=%3")
        .arg(cursorPos.x())
        .arg(cursorPos.y())
        .arg(widgetDebugName(widgetAtCursor)));
}

void MainWindow::refreshActiveToolRightSidebarUi() {
    if (!m_activeScriptedHostController) {
        clearRightSidebarContent();
        ToolRightSidebarState emptyState;
        emptyState.listVisible = false;
        emptyState.searchModeAvailable = false;
        emptyState.searchModeActive = false;
        emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
        m_rightSidebarActiveButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
        m_rightSidebarLastModeId.clear();
        rebuildRightSidebarRail({}, emptyState);
        m_rightSidebarSearchText.clear();
        m_rightSidebarRestoreRequested = false;
        m_rightSidebarPanel->hide();
        m_rightSidebarResizeHandle->hide();
        m_rightSidebarRail->show();
        updateRightSidebarGeometries();
        return;
    }

    if (m_activeScriptedHostController->isV2()) {
        const QString activeToolId = m_activeTool ? m_activeTool->id() : QString();
        if (m_activeRightSidebarListWidget) {
            const QString ownerToolId = m_activeRightSidebarListWidget->property("toolOwnerId").toString();
            if (!activeToolId.isEmpty() && ownerToolId != activeToolId) {
                clearRightSidebarContent();
            }
        }

        const ToolGuiSessionState& session = m_activeScriptedHostController->sessionState();
        const QVariantMap currentValues = m_activeScriptedHostController->currentStateSnapshot().values;
        QString currentModeId = currentValues.value(QStringLiteral("modeId")).toString().trimmed();
        if (currentModeId.isEmpty()) {
            currentModeId = currentValues.value(QStringLiteral("mode")).toString().trimmed();
        }
        if (currentModeId.isEmpty()) {
            currentModeId = session.sidebar.activeMode.trimmed();
        }
        const QString sidebarModelId = session.sidebar.activeMode.trimmed();
        const QString rightSidebarModeKey = sidebarModelId.isEmpty()
            ? currentModeId
            : QStringLiteral("%1:%2").arg(currentModeId, sidebarModelId);
        if (!rightSidebarModeKey.isEmpty() && rightSidebarModeKey != m_rightSidebarLastModeId) {
            m_rightSidebarLastModeId = rightSidebarModeKey;
            activateDefaultRightSidebarList(false, false);
        } else {
            m_rightSidebarLastModeId = rightSidebarModeKey;
        }

        syncV2RightSidebarContent();
        const bool hasOwnedSidebarList = m_activeRightSidebarListWidget
            && (activeToolId.isEmpty()
                || m_activeRightSidebarListWidget->property("toolOwnerId").toString() == activeToolId);

        ToolRightSidebarState state;
        const QString activeSidebarModelId = m_activeRightSidebarListWidget
            ? m_activeRightSidebarListWidget->property("toolModelId").toString()
            : session.sidebar.activeMode;
        state.title = rightSidebarTitleFromSession(
            session,
            activeSidebarModelId,
            activeToolLocalizedString(QStringLiteral("LogFiles"), QStringLiteral("Log Files"))
        );
        state.listVisible = session.sidebar.visible && hasOwnedSidebarList;
        state.searchModeAvailable = hasOwnedSidebarList && session.sidebar.searchEnabled;
        if (!state.searchModeAvailable && m_rightSidebarActiveButtonKey == QString::fromUtf8(kRightSidebarSearchButtonKey)) {
            m_rightSidebarActiveButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
        }
        state.searchModeActive = session.sidebar.searchEnabled
            && hasOwnedSidebarList
            && m_rightSidebarActiveButtonKey == QString::fromUtf8(kRightSidebarSearchButtonKey);
        state.showSelectAllButton = hasOwnedSidebarList && session.sidebar.selectAllEnabled;
        state.activeButtonKey = m_rightSidebarActiveButtonKey.isEmpty()
            ? QString::fromUtf8(kRightSidebarDefaultButtonKey)
            : m_rightSidebarActiveButtonKey;
        state.orderedButtonKeys = session.sidebar.modeOrder;
        if (m_activeRightSidebarListWidget && state.searchModeActive) {
            state.searchableColumns = session.sidebar.searchableColumns;
            state.searchableColumnLabels = session.sidebar.searchableColumnLabels;
            if (state.searchableColumns.isEmpty()) {
                state.searchableColumns.append(0);
            }
            while (state.searchableColumnLabels.size() < state.searchableColumns.size()) {
                const int columnIndex = state.searchableColumnLabels.size();
                const QString fallbackLabel =
                    m_activeRightSidebarListWidget->headerItem()
                    && state.searchableColumns[columnIndex] >= 0
                    && state.searchableColumns[columnIndex] < m_activeRightSidebarListWidget->columnCount()
                        ? m_activeRightSidebarListWidget->headerItem()->text(state.searchableColumns[columnIndex])
                        : QString::number(state.searchableColumns[columnIndex]);
                state.searchableColumnLabels.append(fallbackLabel);
            }
        }

        rebuildRightSidebarRail({}, state);
        if (state.listVisible && (m_rightSidebarRestoreRequested || m_rightSidebarListWidth <= 0)) {
            const int restoredWidth = normalizedRightSidebarListWidth(
                m_rightSidebarLastExpandedWidth > 0 ? m_rightSidebarLastExpandedWidth : m_rightSidebarDefaultListWidth
            );
            if (restoredWidth > 0) {
                m_rightSidebarListWidth = restoredWidth;
            }
        }
        if (m_rightSidebarListWidth > 0) {
            m_rightSidebarLastExpandedWidth = m_rightSidebarListWidth;
        }
        m_rightSidebarRestoreRequested = false;

        updateRightSidebarGeometries();
        return;
    }

    // Build state from scripted controller session
    const ToolGuiSessionState& session = m_activeScriptedHostController->sessionState();
    ToolRightSidebarState state;
    const QVariantMap currentValues = m_activeScriptedHostController->currentStateSnapshot().values;
    QString currentModeId = currentValues.value(QStringLiteral("modeId")).toString().trimmed();
    if (currentModeId.isEmpty()) {
        currentModeId = currentValues.value(QStringLiteral("mode")).toString().trimmed();
    }
    if (currentModeId.isEmpty()) {
        currentModeId = session.sidebar.activeMode.trimmed();
    }
    if (!currentModeId.isEmpty() && currentModeId != m_rightSidebarLastModeId) {
        m_rightSidebarActiveButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    }
    m_rightSidebarLastModeId = currentModeId;
    state.title = rightSidebarTitleFromSession(
        session,
        session.sidebar.activeMode,
        activeToolLocalizedString(QStringLiteral("LogFiles"), QStringLiteral("Log Files"))
    );
    state.listVisible = (m_activeRightSidebarListWidget != nullptr);
    state.searchModeAvailable = session.sidebar.searchEnabled;
    if (!state.searchModeAvailable && m_rightSidebarActiveButtonKey == QString::fromUtf8(kRightSidebarSearchButtonKey)) {
        m_rightSidebarActiveButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    }
    state.searchModeActive = session.sidebar.searchEnabled
        && m_rightSidebarActiveButtonKey == QString::fromUtf8(kRightSidebarSearchButtonKey);
    state.showSelectAllButton = session.sidebar.selectAllEnabled;
    state.activeButtonKey = m_rightSidebarActiveButtonKey.isEmpty()
        ? QString::fromUtf8(kRightSidebarDefaultButtonKey)
        : m_rightSidebarActiveButtonKey;
    
    rebuildRightSidebarRail({}, state);
    if (state.listVisible && m_rightSidebarRestoreRequested && m_rightSidebarListWidth <= 0) {
        m_rightSidebarListWidth = normalizedRightSidebarListWidth(
            m_rightSidebarLastExpandedWidth > 0 ? m_rightSidebarLastExpandedWidth : m_rightSidebarDefaultListWidth
        );
    }
    if (m_rightSidebarListWidth > 0) {
        m_rightSidebarLastExpandedWidth = m_rightSidebarListWidth;
    }
    m_rightSidebarRestoreRequested = false;
    updateRightSidebarGeometries();
}

void MainWindow::updateRightSidebarRailGeometry() {
    if (!m_dashboard || !m_rightSidebarRail) {
        return;
    }

    const QRect dashboardRect = m_dashboard->rect();
    const int railX = qMax(0, dashboardRect.width() - m_rightSidebarRailWidth);
    m_rightSidebarRail->setGeometry(railX, 0, m_rightSidebarRailWidth, dashboardRect.height());
    m_rightSidebarRail->raise();
}

void MainWindow::updateRightSidebarListGeometry() {
    if (!m_dashboard || !m_rightSidebarPanel || !m_rightSidebarResizeHandle) {
        return;
    }

    if (!m_rightSidebarListVisible) {
        m_rightSidebarPanel->hide();
        m_rightSidebarResizeHandle->hide();
        return;
    }

    const QRect dashboardRect = m_dashboard->rect();
    const int panelWidth = currentRightSidebarListWidth();
    if (panelWidth <= 0) {
        m_rightSidebarPanel->hide();
        m_rightSidebarResizeHandle->hide();
        return;
    }

    const int panelRight = qMax(0, dashboardRect.width() - m_rightSidebarRailWidth);
    const int panelLeft = qMax(0, panelRight - panelWidth);
    m_rightSidebarPanel->setGeometry(panelLeft, 0, panelWidth, dashboardRect.height());
    m_rightSidebarResizeHandle->setGeometry(
        0,
        0,
        kRightSidebarResizeHandleWidth,
        dashboardRect.height()
    );
    m_rightSidebarPanel->show();
    m_rightSidebarResizeHandle->show();
    m_rightSidebarPanel->raise();
    m_rightSidebarResizeHandle->raise();
}

void MainWindow::updateRightSidebarGeometries() {
    if (kVerboseToolUiLogging) {
        Logger::instance().logInfo("MainWindow", "[EVENT_CHAIN] ===== updateRightSidebarGeometries called =====");
    }
    
    // Log current tool UI widget state BEFORE geometry update
    if (kVerboseToolUiLogging && m_activeScriptedHostController && m_activeScriptedHostController->isV2()) {
        QWidget* toolWidget = nullptr;
        for (int i = 0; i < m_dashboardContent->layout()->count(); ++i) {
            QLayoutItem* item = m_dashboardContent->layout()->itemAt(i);
            if (item && item->widget()) {
                toolWidget = item->widget();
                break;
            }
        }
        
        if (toolWidget) {
            Logger::instance().logInfo("MainWindow", QString("[EVENT_CHAIN] Tool widget state BEFORE geometry update: enabled=%1, visible=%2, transparentForMouse=%3")
                .arg(toolWidget->isEnabled())
                .arg(toolWidget->isVisible())
                .arg(toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents)));
        }
    }
    
    updateRightSidebarRailGeometry();
    updateRightSidebarListGeometry();

    if (m_dashboardContent && m_dashboardContent->layout()) {
        const int railWidth = (m_rightSidebarRail && m_rightSidebarRail->isVisible()) ? m_rightSidebarRailWidth : 0;
        const int reservedRightWidth = railWidth + (m_rightSidebarListVisible ? currentRightSidebarListWidth() : 0);
        m_dashboardContent->layout()->setContentsMargins(0, 0, reservedRightWidth, 0);
    }
    
    // Log current tool UI widget state AFTER geometry update
    if (m_activeScriptedHostController && m_activeScriptedHostController->isV2()) {
        QWidget* toolWidget = nullptr;
        for (int i = 0; i < m_dashboardContent->layout()->count(); ++i) {
            QLayoutItem* item = m_dashboardContent->layout()->itemAt(i);
            if (item && item->widget()) {
                toolWidget = item->widget();
                break;
            }
        }
        
        if (toolWidget) {
            if (kVerboseToolUiLogging) {
                Logger::instance().logInfo("MainWindow", QString("[EVENT_CHAIN] Tool widget state AFTER geometry update: enabled=%1, visible=%2, transparentForMouse=%3")
                    .arg(toolWidget->isEnabled())
                    .arg(toolWidget->isVisible())
                    .arg(toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents)));
            }
            
            // CRITICAL: Check if attributes were changed by geometry update
            if (toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents)) {
                Logger::instance().logError("MainWindow", "[EVENT_CHAIN] !!!!! CRITICAL: Geometry update caused WA_TransparentForMouseEvents to become TRUE !!!!!");
                Logger::instance().logError("MainWindow", "[EVENT_CHAIN] Forcing it back to false.");
                toolWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
            }
            
            if (!toolWidget->isEnabled()) {
                Logger::instance().logError("MainWindow", "[EVENT_CHAIN] !!!!! CRITICAL: Geometry update caused widget to become DISABLED !!!!!");
                Logger::instance().logError("MainWindow", "[EVENT_CHAIN] Forcing it back to enabled.");
                toolWidget->setEnabled(true);
            }
        }
    }
}

int MainWindow::currentRightSidebarListWidth() const {
    return m_rightSidebarListVisible ? normalizedRightSidebarListWidth(m_rightSidebarListWidth) : 0;
}

int MainWindow::normalizedRightSidebarListWidth(int width) const {
    if (width <= m_rightSidebarMinimumListWidth) {
        return 0;
    }
    return qBound(m_rightSidebarMinimumListWidth + 1, width, m_rightSidebarMaximumListWidth);
}

void MainWindow::setRightSidebarResizeDragging(bool dragging) {
    m_rightSidebarResizeDragging = dragging;
    if (!m_rightSidebarResizeHandle) {
        return;
    }

    if (m_rightSidebarResizeHandle->property("dragging").toBool() == dragging) {
        return;
    }

    m_rightSidebarResizeHandle->setProperty("dragging", dragging);
    m_rightSidebarResizeHandle->style()->unpolish(m_rightSidebarResizeHandle);
    m_rightSidebarResizeHandle->style()->polish(m_rightSidebarResizeHandle);
    m_rightSidebarResizeHandle->update();
}

void MainWindow::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    Logger::instance().logInfo(
        "MainWindow",
        QString("updateTexts begin lang=%1 toolActive=%2")
            .arg(ConfigManager::instance().getLanguage())
            .arg(ToolManager::instance().isToolActive() ? "true" : "false")
    );
    
    // Sidebar
    if (m_sidebarExpanded) {
        m_toolsBtn->setText(loc.getString("MainWindow", "Tools"));
        m_accountBtn->setText(loc.getString("MainWindow", "Account"));
        m_settingsBtn->setText(loc.getString("MainWindow", "Settings"));
        m_configBtn->setText(loc.getString("MainWindow", "Config"));
    } else {
        m_toolsBtn->setText("");
        m_accountBtn->setText("");
        m_settingsBtn->setText("");
        m_configBtn->setText("");
    }
    
    m_appTitle->setText(loc.getString("MainWindow", "Title"));
    if (m_fullscreenRadialMenu) {
        m_fullscreenRadialMenu->setActionTexts(
            loc.getString("MainWindow", "FullscreenCloseApp"),
            loc.getString("MainWindow", "FullscreenCloseTool"),
            loc.getString("MainWindow", "FullscreenExit"));
    }

    m_settingsPage->updateTexts();
    Logger::instance().logInfo("MainWindow", "updateTexts settings page done");
    m_configPage->updateTexts();
    Logger::instance().logInfo("MainWindow", "updateTexts config page done");
    m_toolsPage->updateTexts();
    Logger::instance().logInfo("MainWindow", "updateTexts tools page done");
    m_accountPage->updateTexts();
    Logger::instance().logInfo("MainWindow", "updateTexts account page done");

    if (m_dashboardTitleLabel) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("updateTexts dashboard title label=%1 parent=%2")
                .arg(reinterpret_cast<quintptr>(m_dashboardTitleLabel.data()), 0, 16)
                .arg(reinterpret_cast<quintptr>(m_dashboardTitleLabel->parentWidget()), 0, 16)
        );
        m_dashboardTitleLabel->setText(loc.getString("MainWindow", "DashboardArea"));
    } else {
        Logger::instance().logInfo("MainWindow", "updateTexts skipped dashboard title because label is not alive");
    }
    if (m_rightSidebarSearchEdit) {
        m_rightSidebarSearchEdit->setPlaceholderText(loc.getString("MainWindow", "RightSidebarSearchPlaceholder"));
    }
    updateRightSidebarSelectAllButtonText();
    refreshActiveToolRightSidebarUi();
    Logger::instance().logInfo("MainWindow", "updateTexts end");
}

void MainWindow::onSettingsClicked() {
    if (m_mainStack->currentIndex() == 1) {
        closeOverlay();
    } else {
        m_mainStack->setCurrentIndex(1);
    }
}

void MainWindow::onConfigClicked() {
    if (m_mainStack->currentIndex() == 2) {
        closeOverlay();
    } else {
        Logger::instance().logInfo("MainWindow", "Opening config page, refreshing plugin metadata");
        PluginManager::instance().loadPlugins();
        m_configPage->updateTexts();
        m_mainStack->setCurrentIndex(2);
    }
}

void MainWindow::onToolsClicked() {
    if (m_mainStack->currentIndex() == 3) {
        closeOverlay();
    } else {
        m_mainStack->setCurrentIndex(3);
    }
}

void MainWindow::onAccountClicked() {
    if (m_mainStack->currentIndex() == 4) {
        closeOverlay();
    } else {
        m_mainStack->setCurrentIndex(4);
        m_accountPage->updateAccountInfo();
    }
}

void MainWindow::onToolSelected(const QString &toolId) {
    if (kVerboseToolUiLogging) {
        Logger::instance().logInfo("MainWindow", QString("[TRACE] onToolSelected() called for toolId=%1, mainWindow winId=%2, isVisible=%3")
            .arg(toolId)
            .arg(QString::number(reinterpret_cast<quintptr>(winId())))
            .arg(isVisible() ? "true" : "false"));
    }
    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] onToolSelected:start toolId=%1 %2 stackIndex=%3")
                .arg(toolId)
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }

    if (m_toolSelectionInProgress) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("Ignoring tool selection while another tool launch is still in progress: %1").arg(toolId)
        );
        return;
    }

    QScopedValueRollback<bool> toolSelectionGuard(m_toolSelectionInProgress, true);
    
    ToolProxyInterface* previousActiveProxy = ToolManager::instance().getActiveToolProxy();
    ToolInterface* previousActiveTool = m_activeTool;
    ToolScriptedHostController* previousActiveController = m_activeScriptedHostController;
    const bool hadActiveTool = ToolManager::instance().isToolActive();

    if (hadActiveTool) {
        LocalizationManager& loc = LocalizationManager::instance();
        const QMessageBox::StandardButton reply = CustomMessageBox::question(
            this,
            loc.getString("MainWindow", "SwitchToolTitle"),
            loc.getString("MainWindow", "SwitchToolMsg")
        );
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    ToolInterface* tool = ToolManager::instance().getTool(toolId);
    if (!tool) {
        Logger::instance().logError("MainWindow", "Selected tool not found: " + toolId);
        return;
    }

    const QStringList missingDependencies = PluginManager::instance().getMissingDependencies(tool->dependencies());
    if (!missingDependencies.isEmpty()) {
        LocalizationManager& loc = LocalizationManager::instance();
        const QString dependencyList = missingDependencies.join(", ");
        CustomMessageBox::information(
            this,
            loc.getString("MainWindow", "MissingPluginTitle"),
            loc.getString("MainWindow", "MissingPluginMessage").arg(tool->name(), dependencyList)
        );
        Logger::instance().logWarning(
            "MainWindow",
            QString("Refused to launch tool %1 due to missing plugins: %2").arg(tool->id(), dependencyList)
        );
        return;
    }

    auto setPluginInvokerForTool = [](ToolInterface* invokerTool) {
        if (!invokerTool) {
            ToolRuntimeContext::instance().setPluginInvoker({});
            return;
        }

        ToolRuntimeContext::instance().setPluginInvoker(
            [invokerTool](const ToolRuntimeContext::PluginInvokeRequest& request) {
                PluginAbiBroker::Request brokerRequest;
                brokerRequest.pluginName = request.pluginName;
                brokerRequest.operation = request.operation;
                brokerRequest.contentType = static_cast<PluginAbiBroker::ContentType>(static_cast<quint32>(request.contentType));
                brokerRequest.payload = request.payload;
                brokerRequest.flags = request.flags;
                brokerRequest.authorizedDependencies = invokerTool->dependencies();

                const PluginAbiBroker::Response brokerResponse = PluginAbiBroker::instance().invoke(brokerRequest);
                ToolRuntimeContext::PluginInvokeResponse response;
                response.success = brokerResponse.success;
                response.contentType = static_cast<ToolRuntimeContext::PluginPayloadContentType>(static_cast<quint32>(brokerResponse.contentType));
                response.payload = brokerResponse.payload;
                response.errorMessage = brokerResponse.errorMessage;
                response.status = brokerResponse.status;
                response.flags = brokerResponse.flags;
                return response;
            }
        );
    };

    setPluginInvokerForTool(tool);

    const QString currentLang = ConfigManager::instance().getLanguage();
    tool->loadLanguage(currentLang);

    ToolProxyInterface* proxy = dynamic_cast<ToolProxyInterface*>(tool);
    if (!proxy) {
        Logger::instance().logError("MainWindow", "Selected tool does not expose ToolProxyInterface: " + toolId);
        setPluginInvokerForTool(previousActiveTool);
        return;
    }

    // Reuse the controller only when reopening the same live tool session.
    ToolScriptedHostController* controller = nullptr;
    const bool reusingController = (
        hadActiveTool
        && previousActiveTool == tool
        && previousActiveProxy == proxy
        && m_activeScriptedHostController
    );
    const bool clearedRightSidebarForTransition = !reusingController;

    if (clearedRightSidebarForTransition) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("Clearing right sidebar while activating tool: %1").arg(tool->id())
        );
        resetRightSidebarForToolTransition();
    }
    
    if (reusingController) {
        // Reuse the existing controller - avoid reinitializing to prevent QML engine rebuild
        controller = m_activeScriptedHostController;
    } else {
        controller = new ToolScriptedHostController(proxy, this);
    }

    const RegisteredPackage toolPackage = PackageRegistry::registeredPackage(PackageKind::Tool, tool->id());
    const QString toolDirectoryPath = toolPackage.directoryPath;
    if (toolDirectoryPath.trimmed().isEmpty()) {
        Logger::instance().logError("MainWindow", "Registered tool directory is missing: " + tool->id());
        delete controller;
        m_toolSelectionInProgress = false;
        return;
    }

    controller->setLocalizedStrings(tool->localizedStrings());

    QString initializeError;
    // Only call initialize() for new controllers, not when reusing
    if (!reusingController && !controller->initialize(toolDirectoryPath, &initializeError)) {
        Logger::instance().logError("MainWindow", "Failed to initialize scripted host controller: " + initializeError);
        
        if (!hadActiveTool) {
            controller->deleteLater();
        }

        if (hadActiveTool) {
            setPluginInvokerForTool(previousActiveTool);
            ToolManager::instance().setActiveToolProxy(previousActiveProxy);
            ToolManager::instance().setToolActive(true);
        } else {
            ToolManager::instance().setToolActive(false);
            ToolRuntimeContext::instance().setPluginInvoker({});
        }
        if (clearedRightSidebarForTransition && hadActiveTool) {
            rebuildActiveToolUi();
        }

        LocalizationManager& loc = LocalizationManager::instance();
        CustomMessageBox::information(
            this,
            loc.getString("MainWindow", "ToolLoadFailedTitle"),
            loc.getString("MainWindow", "ToolLoadFailedMsg").arg(tool->name(), initializeError)
        );
        closeOverlay();
        return;
    }

    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] onToolSelected:after controller initialize toolId=%1 %2 stackIndex=%3")
                .arg(toolId)
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }

    m_activeTool = tool;
    m_activeScriptedHostController = controller;

    // Rebind controller signals on every activation.
    QObject::disconnect(controller, nullptr, this, nullptr);
    if (controller->isV2()) {
        connect(controller, &ToolScriptedHostController::pageChanged,
                this, &MainWindow::rebuildActiveToolUi, Qt::QueuedConnection);
    } else {
        connect(controller, &ToolScriptedHostController::sessionStateChanged,
                this, &MainWindow::rebuildActiveToolUi);
    }

    if (controller->isV2() && !controller->buildUiV2(m_dashboardContent)) {
        const QString viewReadyError = QStringLiteral("Worker view failed to become ready.");
        Logger::instance().logError("MainWindow", "Failed to build scripted host view: " + viewReadyError);

        m_activeTool = previousActiveTool;
        m_activeScriptedHostController = previousActiveController;

        if (!hadActiveTool) {
            controller->deleteLater();
        }

        if (hadActiveTool) {
            setPluginInvokerForTool(previousActiveTool);
            ToolManager::instance().setActiveToolProxy(previousActiveProxy);
            ToolManager::instance().setToolActive(true);
        } else {
            ToolManager::instance().setToolActive(false);
            ToolRuntimeContext::instance().setPluginInvoker({});
        }
        if (clearedRightSidebarForTransition && hadActiveTool) {
            rebuildActiveToolUi();
        }

        LocalizationManager& loc = LocalizationManager::instance();
        CustomMessageBox::information(
            this,
            loc.getString("MainWindow", "ToolLoadFailedTitle"),
            loc.getString("MainWindow", "ToolLoadFailedMsg").arg(tool->name(), viewReadyError)
        );
        closeOverlay();
        return;
    }

    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] onToolSelected:before setActiveToolProxy toolId=%1 %2 stackIndex=%3")
                .arg(toolId)
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    if (kVerboseToolUiLogging) {
        Logger::instance().logInfo("MainWindow", QString("[TRACE] Before setActiveToolProxy, mainWindow winId=%1, isVisible=%2")
            .arg(QString::number(reinterpret_cast<quintptr>(winId())))
            .arg(isVisible() ? "true" : "false"));
    }
    
    ToolManager::instance().setActiveToolProxy(proxy);
    ToolManager::instance().setToolActive(true);

    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] onToolSelected:after setActiveToolProxy toolId=%1 %2 stackIndex=%3")
                .arg(toolId)
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    
    if (kVerboseToolUiLogging) {
        Logger::instance().logInfo("MainWindow", QString("[TRACE] Before rebuildActiveToolUi, mainWindow winId=%1, isVisible=%2")
            .arg(QString::number(reinterpret_cast<quintptr>(winId())))
            .arg(isVisible() ? "true" : "false"));
    }
    
    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] onToolSelected:before rebuildActiveToolUi toolId=%1 %2 stackIndex=%3")
                .arg(toolId)
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    rebuildActiveToolUi();
    
    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] onToolSelected:after rebuildActiveToolUi toolId=%1 %2 stackIndex=%3")
                .arg(toolId)
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    if (kVerboseToolUiLogging) {
        Logger::instance().logInfo("MainWindow", QString("[TRACE] After rebuildActiveToolUi, mainWindow winId=%1, isVisible=%2")
            .arg(QString::number(reinterpret_cast<quintptr>(winId())))
            .arg(isVisible() ? "true" : "false"));
    }

    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] onToolSelected:before closeOverlay toolId=%1 %2 stackIndex=%3")
                .arg(toolId)
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    closeOverlay();

    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] onToolSelected:after closeOverlay toolId=%1 %2 stackIndex=%3")
                .arg(toolId)
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }

    // Clean up previous tool worker process
    if (hadActiveTool) {
        if (previousActiveController && previousActiveController != controller) {
            previousActiveController->deleteLater();
        }
        if (previousActiveProxy && previousActiveProxy != proxy) {
            Logger::instance().logInfo(
                "MainWindow",
                QString("Stopping previous tool worker process after activating %1").arg(tool->id())
            );
            previousActiveProxy->discardProcess();
        }
    }
}

void MainWindow::closeOverlay() {
    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] closeOverlay:before %1 stackIndex=%2")
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    m_mainStack->setCurrentIndex(0);
    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] closeOverlay:after %1 stackIndex=%2")
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    updateRightSidebarGeometries();
    QTimer::singleShot(0, this, [this]() {
        updateRightSidebarGeometries();
    });
}

void MainWindow::onLanguageChanged() {
    m_pendingLang = ConfigManager::instance().getLanguage();
    if (m_pendingLang == m_currentLang && !m_languageChangeInProgress) {
        return;
    }

    if (m_languageChangeQueued) {
        return;
    }

    m_languageChangeQueued = true;
    QTimer::singleShot(0, this, &MainWindow::applyPendingLanguageChange);
}

void MainWindow::applyPendingLanguageChange() {
    m_languageChangeQueued = false;

    const QString lang = m_pendingLang.isEmpty()
        ? ConfigManager::instance().getLanguage()
        : m_pendingLang;

    Logger::instance().logInfo(
        "MainWindow",
        QString("applyPendingLanguageChange begin pending=%1 current=%2 inProgress=%3")
            .arg(lang, m_currentLang)
            .arg(m_languageChangeInProgress ? "true" : "false")
    );

    if (lang == m_currentLang || m_languageChangeInProgress) {
        Logger::instance().logInfo("MainWindow", "applyPendingLanguageChange skipped");
        return;
    }

    m_languageChangeInProgress = true;
    m_currentLang = lang;
    LocalizationManager::instance().loadLanguage(lang);

    if (ToolManager::instance().isToolActive() && m_activeTool) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("applyPendingLanguageChange notifying active tool id=%1").arg(m_activeTool->id())
        );
        m_activeTool->loadLanguage(lang);
        Logger::instance().logInfo("MainWindow", "applyPendingLanguageChange active tool language done");
        if (m_activeScriptedHostController) {
            m_activeScriptedHostController->setLocalizedStrings(m_activeTool->localizedStrings());
            Logger::instance().logInfo("MainWindow", "applyPendingLanguageChange active controller strings done");
        }
    }

    Logger::instance().logInfo("MainWindow", "applyPendingLanguageChange updating main texts");
    updateTexts();
    Logger::instance().logInfo("MainWindow", "applyPendingLanguageChange updating user agreement overlay");
    m_userAgreementOverlay->updateTexts();
    Logger::instance().logInfo("MainWindow", "applyPendingLanguageChange updating login overlay");
    m_loginOverlay->updateTexts();
    Logger::instance().logInfo("MainWindow", "applyPendingLanguageChange updating connection warning overlay");
    m_connectionWarningOverlay->updateTexts();
    m_languageChangeInProgress = false;
    Logger::instance().logInfo("MainWindow", "applyPendingLanguageChange end");

}

void MainWindow::onThemeChanged() {
    applyTheme();
    
    m_settingsPage->updateTheme();
    m_configPage->updateTheme();
    m_accountPage->updateTheme();
    m_updateOverlay->updateTheme();
    m_userAgreementOverlay->updateTheme();
    m_loginOverlay->updateTheme();
    m_advertisementOverlay->updateTheme();
    m_connectionWarningOverlay->updateTheme();
    
    // Update ToolsPage theme (must be after applyTheme to override global styles)
    m_toolsPage->updateTheme();
    
    // Notify active tool to update theme
    if (ToolManager::instance().isToolActive()) {
        QList<ToolInterface*> tools = ToolManager::instance().getTools();
        for (ToolInterface* tool : tools) {
            tool->applyTheme();
        }
    }

    if (m_activeScriptedHostController && m_activeScriptedHostController->isV2()) {
        m_activeScriptedHostController->updateThemeV2(
            ConfigManager::instance().isCurrentThemeDark() ? QStringLiteral("dark") : QStringLiteral("light")
        );
    }

    if (m_activeScriptedHostController) {
        refreshActiveToolRightSidebarUi();
    }
}

void MainWindow::onDebugModeChanged(bool enabled) {
    ConfigManager::instance().setDebugMode(enabled);
    if (enabled) {
        m_memUsageLabel->show();
        m_memTimer->start(1000);
    } else {
        m_memUsageLabel->hide();
        m_memTimer->stop();
    }
}

void MainWindow::onSidebarCompactChanged(bool enabled) {
    ConfigManager::instance().setSidebarCompactMode(enabled);
    if (m_fullscreenPresentationActive) {
        return;
    }
    // Only apply the change if not locked
    if (!m_sidebarLocked) {
        if (enabled) collapseSidebar();
        else expandSidebar();
    }
}

void MainWindow::onModClosed() {
    Logger::instance().logInfo("MainWindow", "Mod closed, showing setup overlay");
    
    // Stop any active tools
    if (ToolManager::instance().isToolActive()) {
        QList<ToolInterface*> tools = ToolManager::instance().getTools();
        for (ToolInterface* t : tools) {
            ToolProxyInterface* proxy = dynamic_cast<ToolProxyInterface*>(t);
            if (proxy && proxy->isProcessRunning()) {
                proxy->discardProcess();
            }
        }
        ToolManager::instance().setToolActive(false);
    }
    
    // Stop path monitoring
    PathValidator::instance().stopMonitoring();
    
    // Set flag to skip ad after setup
    m_setupSkipped = true;
    
    // Show setup overlay
    m_setupOverlay->showOverlay();
    raiseWindowControlsOverlay();
}

void MainWindow::onPathInvalid(const QString& titleKey, const QString& msgKey) {
    LocalizationManager& loc = LocalizationManager::instance();
    CustomMessageBox::information(this, 
        loc.getString("Error", titleKey), 
        loc.getString("Error", msgKey));
    
    // Clear only the invalid path config based on which path is invalid
    ConfigManager& config = ConfigManager::instance();
    if (titleKey == "GamePathInvalid") {
        config.clearGamePath();
        Logger::instance().logInfo("MainWindow", "Game path cleared due to validation failure");
    } else if (titleKey == "ModPathInvalid") {
        config.clearModPath();
        Logger::instance().logInfo("MainWindow", "Mod path cleared due to validation failure");
    }
    
    // Show setup overlay if paths become invalid
    m_setupOverlay->showOverlay();
    raiseWindowControlsOverlay();
}

void MainWindow::closeActiveToolFromFullscreenMenu() {
    if (!ToolManager::instance().isToolActive()) {
        return;
    }
    resetActiveToolUi();
    closeOverlay();
}

void MainWindow::exitFullscreenFromMenu() {
    ConfigManager::instance().setDisplayMode(ConfigManager::DisplayMode::Window);
    if (m_settingsPage) {
        m_settingsPage->syncDisplaySettingsControls();
    }
    applyDisplaySettings();
}

void MainWindow::applyFullscreenPresentation(bool enabled) {
    if (enabled == m_fullscreenPresentationActive) {
        if (enabled) {
            setSidebarExpandedPresentation(false);
            startSidebarWidthAnimation(60, false);
        }
        return;
    }

    if (enabled) {
        m_sidebarExpandedBeforeFullscreen = m_sidebarExpanded;
        m_sidebarLockedBeforeFullscreen = m_sidebarLocked;
        m_fullscreenPresentationActive = true;
        m_sidebarLocked = true;
        if (m_sidebarCollapseTimer)
            m_sidebarCollapseTimer->stop();
        setSidebarExpandedPresentation(false);
        startSidebarWidthAnimation(60, false);
        if (m_fullscreenRadialMenu)
            m_fullscreenRadialMenu->hideMenu();
    } else {
        if (m_fullscreenRadialMenu)
            m_fullscreenRadialMenu->hideMenu();
        m_fullscreenPresentationActive = false;
        m_sidebarLocked = m_sidebarLockedBeforeFullscreen;
        setSidebarExpandedPresentation(m_sidebarExpandedBeforeFullscreen);
        startSidebarWidthAnimation(m_sidebarExpandedBeforeFullscreen ? 250 : 60, m_sidebarExpandedBeforeFullscreen);
    }

    applyTheme();
    applyNativeRoundedCorners(QStringLiteral("applyFullscreenPresentation"));
    updateWindowControlsOverlayGeometry();
    updateFullscreenChromeGeometry();
}

void MainWindow::applyNativeFullscreenWindow(QScreen* targetScreen) {
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd || !targetScreen) {
        return;
    }

    if (!m_nativeFullscreenApplied) {
        m_windowedStyle = static_cast<qintptr>(GetWindowLongPtr(hwnd, GWL_STYLE));
        m_windowedExStyle = static_cast<qintptr>(GetWindowLongPtr(hwnd, GWL_EXSTYLE));
        m_windowedGeometry = frameGeometry();
        m_nativeFullscreenApplied = true;
    }
    m_nativeFullscreenHwnd = reinterpret_cast<qintptr>(hwnd);

    const QRect rect = targetScreen->geometry();
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    style |= WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME | WS_EX_STATICEDGE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(
        hwnd,
        HWND_NOTOPMOST,
        rect.x(),
        rect.y(),
        rect.width(),
        rect.height(),
        SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    setDwmSquareWindowCorners(hwnd);
#else
    Q_UNUSED(targetScreen);
#endif
}

void MainWindow::restoreNativeWindowLayering() {
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return;
    }
    if (m_nativeFullscreenApplied) {
        SetWindowLongPtr(hwnd, GWL_STYLE, static_cast<LONG_PTR>(m_windowedStyle));
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, static_cast<LONG_PTR>(m_windowedExStyle));
        SetWindowPos(
            hwnd,
            HWND_NOTOPMOST,
            m_windowedGeometry.x(),
            m_windowedGeometry.y(),
            m_windowedGeometry.width(),
            m_windowedGeometry.height(),
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        m_nativeFullscreenApplied = false;
        m_nativeFullscreenHwnd = 0;
        m_windowedStyle = 0;
        m_windowedExStyle = 0;
        m_windowedGeometry = QRect();
    }
    SetWindowPos(
        hwnd,
        HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
#endif
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    Q_UNUSED(eventType);
    MSG* msg = static_cast<MSG*>(message);
    HWND hwnd = reinterpret_cast<HWND>(m_nativeFullscreenHwnd);
    if (!msg || !hwnd || msg->hwnd != hwnd || !m_fullscreenPresentationActive || !m_nativeFullscreenApplied) {
        return QMainWindow::nativeEvent(eventType, message, result);
    }

    QScreen* targetScreen = configuredDisplayScreen();
    if (!targetScreen) {
        return QMainWindow::nativeEvent(eventType, message, result);
    }

    const QRect rect = targetScreen->geometry();
    switch (msg->message) {
    case WM_NCLBUTTONDOWN:
        if (msg->wParam == HTCAPTION) {
            if (result) {
                *result = 0;
            }
            return true;
        }
        break;
    case WM_SYSCOMMAND:
        if ((msg->wParam & 0xFFF0) == SC_MOVE || (msg->wParam & 0xFFF0) == SC_SIZE) {
            if (result) {
                *result = 0;
            }
            return true;
        }
        break;
    case WM_MOVING:
        if (RECT* movingRect = reinterpret_cast<RECT*>(msg->lParam)) {
            movingRect->left = rect.left();
            movingRect->top = rect.top();
            movingRect->right = rect.left() + rect.width();
            movingRect->bottom = rect.top() + rect.height();
            if (result) {
                *result = TRUE;
            }
            return true;
        }
        break;
    case WM_WINDOWPOSCHANGING:
        if (WINDOWPOS* pos = reinterpret_cast<WINDOWPOS*>(msg->lParam)) {
            pos->x = rect.x();
            pos->y = rect.y();
            pos->cx = rect.width();
            pos->cy = rect.height();
            pos->flags &= ~(SWP_NOMOVE | SWP_NOSIZE);
        }
        break;
    default:
        break;
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

QScreen* MainWindow::configuredDisplayScreen() const {
    const QString targetName = ConfigManager::instance().getDisplayScreenName();
    const QList<QScreen *> screens = QGuiApplication::screens();

    for (QScreen *candidate : screens) {
        if (candidate && candidate->name() == targetName) {
            return candidate;
        }
    }

    if (QScreen *current = screen()) {
        return current;
    }
    if (QScreen *primary = QGuiApplication::primaryScreen()) {
        return primary;
    }
    return screens.isEmpty() ? nullptr : screens.first();
}

QSize MainWindow::configuredWindowResolution(QScreen* targetScreen) const {
    const QSize requested = ConfigManager::instance().getDisplayResolution();
    const QSize screenSize = targetScreen ? targetScreen->size() : QSize(3840, 2160);
    const int maxWidth = qMax(1280, qMin(3840, screenSize.width()));
    const int maxHeight = qMax(720, qMin(2160, screenSize.height()));
    const int width = qBound(1280, requested.width(), maxWidth);
    const int height = qBound(720, requested.height(), maxHeight);
    return QSize(width, height);
}

void MainWindow::applyDisplaySettings() {
    QScreen *targetScreen = configuredDisplayScreen();
    if (!targetScreen)
        return;

    if (QWindow *nativeWindow = windowHandle()) {
        nativeWindow->setScreen(targetScreen);
    }

    if (ConfigManager::instance().getDisplayMode() == ConfigManager::DisplayMode::Fullscreen) {
        applyFullscreenPresentation(true);
        applyNativeFullscreenWindow(targetScreen);
        showFullScreen();
        applyNativeFullscreenWindow(targetScreen);
    } else {
        applyFullscreenPresentation(false);
        if (isFullScreen() || isMaximized()) {
            showNormal();
        }
        restoreNativeWindowLayering();

        const QSize resolution = configuredWindowResolution(targetScreen);
        const QRect availableGeometry = targetScreen->availableGeometry();
        const QPoint centeredTopLeft(
            availableGeometry.x() + qMax(0, (availableGeometry.width() - resolution.width()) / 2),
            availableGeometry.y() + qMax(0, (availableGeometry.height() - resolution.height()) / 2));
        resize(resolution);
        move(centeredTopLeft);
    }

    updateWindowControlsOverlayGeometry();
    raiseWindowControlsOverlay();
    updateFullscreenChromeGeometry();
}

void MainWindow::minimizeWindow() { showMinimized(); }
void MainWindow::maximizeWindow() { 
    if (isMaximized()) showNormal(); 
    else showMaximized(); 
}
void MainWindow::closeWindow() { 
    if (m_shutdownInProgress) {
        return;
    }

    if (m_updateShutdownRequested) {
        close();
        return;
    }

    if (ToolManager::instance().isToolActive()) {
        LocalizationManager& loc = LocalizationManager::instance();
        auto reply = CustomMessageBox::question(this, 
            loc.getString("MainWindow", "CloseConfirmTitle"),
            loc.getString("MainWindow", "CloseConfirmMsg"));
        if (reply != QMessageBox::Yes) return;
    }
    close(); 
}

bool MainWindow::confirmCloseActiveTool() {
    if (!ToolManager::instance().isToolActive()) {
        return false;
    }

    LocalizationManager& loc = LocalizationManager::instance();
    const QMessageBox::StandardButton reply = CustomMessageBox::question(
        this,
        loc.getString("MainWindow", "CloseToolConfirmTitle"),
        loc.getString("MainWindow", "CloseToolConfirmMsg"));
    return reply == QMessageBox::Yes;
}

void MainWindow::closeActiveToolWithConfirmation() {
    if (!confirmCloseActiveTool()) {
        return;
    }

    closeActiveToolFromFullscreenMenu();
}

QWidget* MainWindow::activeDashboardToolWidget() const {
    if (!m_dashboardContent || !m_dashboardContent->layout()) {
        return nullptr;
    }

    for (int i = 0; i < m_dashboardContent->layout()->count(); ++i) {
        QLayoutItem* item = m_dashboardContent->layout()->itemAt(i);
        if (item && item->widget()) {
            return item->widget();
        }
    }
    return nullptr;
}

bool MainWindow::isFullscreenPresentationActive() const {
    return m_fullscreenPresentationActive;
}

void MainWindow::syncSidebarNavigationPlacement() {
    if (!m_sidebarLayout || !m_sidebarNavigationContainer) {
        return;
    }

    m_sidebarLayout->removeWidget(m_sidebarNavigationContainer);
    if (m_fullscreenPresentationActive) {
        m_sidebarLayout->insertWidget(3, m_sidebarNavigationContainer);
    } else {
        const int bottomIndex = m_bottomAppIconContainer
            ? qMax(0, m_sidebarLayout->indexOf(m_bottomAppIconContainer))
            : m_sidebarLayout->count();
        m_sidebarLayout->insertWidget(bottomIndex, m_sidebarNavigationContainer);
    }
    m_sidebarNavigationContainer->show();
}

void MainWindow::setSidebarExpandedPresentation(bool expanded) {
    LocalizationManager& loc = LocalizationManager::instance();
    const bool fullscreen = m_fullscreenPresentationActive;

    m_sidebarExpanded = expanded;
    m_sidebarLayout->setContentsMargins(expanded ? 20 : 0, fullscreen ? 10 : 20, expanded ? 20 : 0, fullscreen ? 0 : 20);
    if (m_sidebarTopSpacerSmall)
        m_sidebarTopSpacerSmall->setVisible(!fullscreen);
    if (m_sidebarTopSpacerLarge)
        m_sidebarTopSpacerLarge->setVisible(!fullscreen);
    m_appTitle->setVisible(expanded && !fullscreen);
    m_appIcon->setVisible(expanded && !fullscreen);
    if (m_fullscreenIslandButton)
        m_fullscreenIslandButton->setVisible(fullscreen);
    m_bottomAppIcon->setVisible(!expanded && !fullscreen);
    if (m_bottomAppIconContainer)
        m_bottomAppIconContainer->setVisible(fullscreen || !expanded);
    m_controlsHorizontal->setVisible(expanded && !fullscreen);
    m_controlsVertical->setVisible(!expanded && !fullscreen);
    m_titleLayout->setAlignment((expanded && !fullscreen) ? (Qt::AlignLeft | Qt::AlignVCenter) : Qt::AlignCenter);
    syncSidebarNavigationPlacement();

    const Qt::ToolButtonStyle buttonStyle = expanded ? Qt::ToolButtonTextOnly : Qt::ToolButtonIconOnly;
    m_toolsBtn->setToolButtonStyle(buttonStyle);
    m_accountBtn->setToolButtonStyle(buttonStyle);
    m_settingsBtn->setToolButtonStyle(buttonStyle);
    m_configBtn->setToolButtonStyle(buttonStyle);

    m_toolsBtn->setText(expanded ? loc.getString("MainWindow", "Tools") : QString());
    m_accountBtn->setText(expanded ? loc.getString("MainWindow", "Account") : QString());
    m_settingsBtn->setText(expanded ? loc.getString("MainWindow", "Settings") : QString());
    m_configBtn->setText(expanded ? loc.getString("MainWindow", "Config") : QString());

    m_toolsBtn->show();
    m_accountBtn->show();
    m_settingsBtn->show();
    m_configBtn->show();
    updateWindowControlsOverlayGeometry();
    updateFullscreenChromeGeometry();
}

void MainWindow::startSidebarWidthAnimation(int targetWidth, bool expanded) {
    if (!m_sidebar) {
        return;
    }

    if (m_sidebarWidthAnimation) {
        QVariantAnimation* previousAnimation = m_sidebarWidthAnimation;
        m_sidebarWidthAnimation = nullptr;
        previousAnimation->stop();
        previousAnimation->deleteLater();
    }

    m_sidebarExpanded = expanded;
    const int startWidth = qBound(60, m_sidebar->width(), 250);
    if (startWidth == targetWidth) {
        m_sidebar->setFixedWidth(targetWidth);
        updateRightSidebarGeometries();
        repairActiveToolWidgetAfterSidebarAnimation();
        return;
    }

    auto* animation = new QVariantAnimation(this);
    m_sidebarWidthAnimation = animation;
    animation->setStartValue(startWidth);
    animation->setEndValue(targetWidth);
    animation->setDuration(190);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_sidebar->setFixedWidth(value.toInt());
        updateWindowControlsOverlayGeometry();
        if (!m_geometryUpdateThrottleTimer->isActive()) {
            updateRightSidebarGeometries();
            m_geometryUpdateThrottleTimer->start();
        }
    });
    connect(animation, &QVariantAnimation::finished, this, [this, animation, targetWidth]() {
        if (m_sidebarWidthAnimation == animation) {
            m_sidebarWidthAnimation = nullptr;
        }
        m_sidebar->setFixedWidth(targetWidth);
        updateWindowControlsOverlayGeometry();
        updateRightSidebarGeometries();
        repairActiveToolWidgetAfterSidebarAnimation();
        animation->deleteLater();
    });

    animation->start();
}

void MainWindow::repairActiveToolWidgetAfterSidebarAnimation() {
    QWidget* toolWidget = activeDashboardToolWidget();
    if (!toolWidget) {
        return;
    }

    if (toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents)) {
        toolWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    }
    if (!toolWidget->isEnabled()) {
        toolWidget->setEnabled(true);
    }

    toolWidget->updateGeometry();
    toolWidget->update();

    if (QQuickWidget* quickWidget = findQuickWidgetInToolHost(toolWidget)) {
        quickWidget->setEnabled(true);
        quickWidget->updateGeometry();
        quickWidget->update();
        if (QQuickWindow* quickWindow = quickWidget->quickWindow()) {
            quickWindow->update();
        }
        if (QQuickItem* rootObject = quickWidget->rootObject()) {
            if (rootObject->property("enabled").isValid() && !rootObject->property("enabled").toBool()) {
                rootObject->setProperty("enabled", true);
            }
            rootObject->update();
        }
    }
}

void MainWindow::expandSidebar() {
    if (m_fullscreenPresentationActive) {
        return;
    }
    if (m_sidebarExpanded) return;

    setSidebarExpandedPresentation(true);
    startSidebarWidthAnimation(250, true);
}

void MainWindow::toggleSidebarLock() {
    if (m_fullscreenPresentationActive) {
        return;
    }

    m_sidebarLocked = !m_sidebarLocked;
    Logger::instance().logInfo("MainWindow", QString("Sidebar lock toggled: %1").arg(m_sidebarLocked ? "Locked" : "Unlocked"));
    
    if (m_sidebarLocked && m_sidebarExpanded) {
        collapseSidebar();
    } else if (!m_sidebarLocked && !m_sidebarExpanded) {
        expandSidebar();
        if (ConfigManager::instance().getSidebarCompactMode() && !m_sidebar->underMouse()) {
            m_sidebarCollapseTimer->start();
        }
    }
}

void MainWindow::captureWindowScreenshot() {
    if (!isVisible()) {
        Logger::instance().logWarning("MainWindow", "Screenshot skipped because the main window is not visible.");
        return;
    }

    prepareWindowCaptureFrame();

    QImage image = renderWindowScreenshot();
    if (image.isNull()) {
        Logger::instance().logError("MainWindow", "Screenshot failed because the rendered image is empty.");
        return;
    }

    applyScreenshotWatermark(image);
    applyWindowScreenshotAlphaMask(image);

    const QString screenshotRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("APE-HOI4-Tool-Studio/screenshot"));
    if (!QDir().mkpath(screenshotRoot)) {
        Logger::instance().logError("MainWindow", "Screenshot failed because the screenshot directory could not be created.");
        return;
    }

    const QString fileName = QStringLiteral("APEHOI4ToolStudio_%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    const QString outputPath = QDir(screenshotRoot).filePath(fileName);

    if (!image.save(outputPath, "PNG")) {
        Logger::instance().logError("MainWindow", QString("Screenshot failed while saving: %1").arg(outputPath));
        return;
    }

    if (QClipboard* clipboard = QApplication::clipboard()) {
        auto* mimeData = new QMimeData();
        QByteArray pngData;
        QBuffer pngBuffer(&pngData);
        if (pngBuffer.open(QIODevice::WriteOnly) && image.save(&pngBuffer, "PNG")) {
            mimeData->setData("image/png", pngData);
        }
        mimeData->setImageData(image);
        clipboard->setMimeData(mimeData);
    }

    Logger::instance().logInfo("MainWindow", QString("Screenshot saved to: %1").arg(outputPath));
    showScreenshotSuccessFeedback(outputPath);
}

void MainWindow::prepareWindowCaptureFrame() {
    raiseWindowControlsOverlay();

    if (m_acrylicBackdrop) {
        m_acrylicBackdrop->update();
        m_acrylicBackdrop->repaint();
    }

    const QList<QQuickWidget*> quickWidgets = findChildren<QQuickWidget*>();
    for (QQuickWidget* quickWidget : quickWidgets) {
        if (!quickWidget || !quickWidget->isVisible())
            continue;
        quickWidget->update();
        if (QQuickWindow* quickWindow = quickWidget->quickWindow()) {
            quickWindow->update();
        }
        if (QQuickItem* rootObject = quickWidget->rootObject()) {
            rootObject->update();
        }
    }

    update();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 16);
}

QImage MainWindow::renderWindowScreenshot() {
    const QPixmap pixmap = grab();
    if (pixmap.isNull()) {
        return QImage();
    }

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    image.setDevicePixelRatio(pixmap.devicePixelRatio());
    return image;
}

void MainWindow::applyScreenshotWatermark(QImage& image) const {
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform, true);

    const QString infoText = screenshotWatermarkText();
    const QString repeatedMark = infoText;
    QFont repeatedFont(QStringLiteral("Segoe UI"), qBound(14, image.width() / 54, 28));
    repeatedFont.setWeight(QFont::DemiBold);

    painter.save();
    painter.translate(image.width() / 2.0, image.height() / 2.0);
    painter.rotate(-28.0);
    painter.translate(-image.width() / 2.0, -image.height() / 2.0);
    painter.setFont(repeatedFont);
    painter.setPen(QColor(0, 0, 0, 5));

    const QFontMetrics repeatedMetrics(repeatedFont);
    const int stepX = qMax(360, repeatedMetrics.horizontalAdvance(repeatedMark) + 220);
    const int stepY = qMax(160, repeatedMetrics.height() + 118);
    for (int y = -image.height(); y < image.height() * 2; y += stepY) {
        for (int x = -image.width(); x < image.width() * 2; x += stepX) {
            painter.drawText(QPoint(x, y), repeatedMark);
        }
    }
    painter.restore();

    QFont infoFont(QStringLiteral("Segoe UI"), qBound(8, image.width() / 150, 12));
    infoFont.setWeight(QFont::Medium);
    painter.setFont(infoFont);

    const QFontMetrics infoMetrics(infoFont);
    const int paddingX = 8;
    const int paddingY = 4;
    const int margin = 10;
    const int boxWidth = infoMetrics.horizontalAdvance(infoText) + paddingX * 2;
    const int boxHeight = infoMetrics.height() + paddingY * 2;
    const QRect infoBox(
        qMax(margin, image.width() - boxWidth - margin),
        qMax(margin, image.height() - boxHeight - margin),
        qMin(boxWidth, image.width() - margin * 2),
        boxHeight
    );

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 118));
    painter.drawRoundedRect(infoBox, 6, 6);

    painter.setPen(QColor(255, 255, 255, 214));
    painter.drawText(infoBox.adjusted(paddingX, paddingY, -paddingX, -paddingY), Qt::AlignCenter, infoText);
}

void MainWindow::applyWindowScreenshotAlphaMask(QImage& image) const {
    if (image.isNull()) {
        return;
    }
    if (m_fullscreenPresentationActive || isFullScreen()) {
        return;
    }

    const qreal deviceScale = qMax<qreal>(1.0, image.devicePixelRatio());
    const qreal imageDevicePixelRatio = image.devicePixelRatio();
    image = image.convertToFormat(QImage::Format_ARGB32);
    image.setDevicePixelRatio(imageDevicePixelRatio);

    const qreal radius = kWindowScreenshotCornerRadius * deviceScale;

    for (int y = 0; y < image.height(); ++y) {
        QRgb* imageLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb sourcePixel = imageLine[x];
            const int maskAlpha = roundedScreenshotCoverageAlpha(x, y, image.width(), image.height(), radius);
            if (maskAlpha <= 0) {
                imageLine[x] = qRgba(qRed(sourcePixel), qGreen(sourcePixel), qBlue(sourcePixel), 0);
                continue;
            }

            const int sourceAlpha = qAlpha(sourcePixel);
            const int outputAlpha = (sourceAlpha * maskAlpha + 127) / 255;
            if (outputAlpha <= 0) {
                imageLine[x] = qRgba(qRed(sourcePixel), qGreen(sourcePixel), qBlue(sourcePixel), 0);
                continue;
            }

            imageLine[x] = qRgba(qRed(sourcePixel), qGreen(sourcePixel), qBlue(sourcePixel), outputAlpha);
        }
    }
}

QString MainWindow::screenshotWatermarkText() const {
    QString username = AuthManager::instance().getCurrentUsername().trimmed();
    if (username.isEmpty()) {
        username = QStringLiteral("local");
    }

#ifdef APP_VERSION
    QString version = QStringLiteral(APP_VERSION).trimmed();
#else
    QString version = QCoreApplication::applicationVersion().trimmed();
#endif
    if (version.isEmpty()) {
        version = QStringLiteral("unknown");
    }

    return QStringLiteral("%1 | APE HOI4 Tool Studio v%2 | %3")
        .arg(username, version, QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

void MainWindow::showScreenshotSuccessFeedback(const QString& outputPath) {
    playScreenshotShutterSound();
    showScreenshotSuccessNotification(outputPath);
}

void MainWindow::playScreenshotShutterSound() {
    static const QByteArray shutterWave = createScreenshotShutterWave();
    if (!PlaySoundW(reinterpret_cast<LPCWSTR>(shutterWave.constData()), nullptr, SND_MEMORY | SND_NODEFAULT)) {
        MessageBeep(MB_ICONASTERISK);
    }
}

void MainWindow::showScreenshotSuccessNotification(const QString& outputPath) {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return;
    }

    LocalizationManager& loc = LocalizationManager::instance();
    NOTIFYICONDATAW notifyData = {};
    notifyData.cbSize = sizeof(NOTIFYICONDATAW);
    notifyData.hWnd = hwnd;
    notifyData.uID = kScreenshotNotificationIconId;
    notifyData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
    notifyData.uCallbackMessage = WM_APP + 31;
    notifyData.hIcon = reinterpret_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr),
        L"IDI_ICON1",
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR));
    if (!notifyData.hIcon) {
        notifyData.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    notifyData.dwInfoFlags = NIIF_INFO;

    copyNotifyText(notifyData.szTip, std::size(notifyData.szTip), QStringLiteral("APE HOI4 Tool Studio"));
    copyNotifyText(notifyData.szInfoTitle, std::size(notifyData.szInfoTitle),
        loc.getString("MainWindow", "ScreenshotSuccessTitle"));
    copyNotifyText(notifyData.szInfo, std::size(notifyData.szInfo),
        loc.getString("MainWindow", "ScreenshotSuccessMsg").arg(QDir::toNativeSeparators(outputPath)));

    NOTIFYICONDATAW cleanupData = {};
    cleanupData.cbSize = sizeof(NOTIFYICONDATAW);
    cleanupData.hWnd = hwnd;
    cleanupData.uID = kScreenshotNotificationIconId;
    Shell_NotifyIconW(NIM_DELETE, &cleanupData);

    if (Shell_NotifyIconW(NIM_ADD, &notifyData)) {
        notifyData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &notifyData);
        Shell_NotifyIconW(NIM_MODIFY, &notifyData);
        QTimer::singleShot(10000, this, [hwnd]() {
            NOTIFYICONDATAW delayedCleanupData = {};
            delayedCleanupData.cbSize = sizeof(NOTIFYICONDATAW);
            delayedCleanupData.hWnd = hwnd;
            delayedCleanupData.uID = kScreenshotNotificationIconId;
            Shell_NotifyIconW(NIM_DELETE, &delayedCleanupData);
        });
    } else {
        Logger::instance().logWarning("MainWindow", "Failed to send screenshot success shell notification.");
    }
}

void MainWindow::toggleWindowRecording() {
    if (m_windowRecordingActive) {
        stopWindowRecording();
        return;
    }

    startWindowRecording();
}

void MainWindow::startWindowRecording() {
    if (m_windowRecordingActive) {
        return;
    }
    if (!m_windowRecorder) {
        m_windowRecorder = std::make_unique<WindowAviRecorder>();
    }
    if (!isVisible()) {
        Logger::instance().logWarning("MainWindow", "Recording skipped because the main window is not visible.");
        return;
    }

    prepareWindowCaptureFrame();
    QImage firstFrame = renderWindowScreenshot();
    if (firstFrame.isNull()) {
        Logger::instance().logError("MainWindow", "Recording failed because the first frame is empty.");
        return;
    }
    applyScreenshotWatermark(firstFrame);

    const QString recordingRoot = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("APE-HOI4-Tool-Studio/recording"));
    if (!QDir().mkpath(recordingRoot)) {
        Logger::instance().logError("MainWindow", "Recording failed because the recording directory could not be created.");
        return;
    }

    const QString fileName = QStringLiteral("APEHOI4ToolStudio_%1.avi")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    const QString outputPath = QDir(recordingRoot).filePath(fileName);

    QString errorMessage;
    if (!m_windowRecorder->start(outputPath, firstFrame.size(), kWindowRecordingFrameRate, &errorMessage)) {
        Logger::instance().logError("MainWindow", QString("Recording failed while opening AVI: %1").arg(errorMessage));
        return;
    }
    if (!m_windowRecorder->writeFrame(firstFrame, &errorMessage)) {
        Logger::instance().logError("MainWindow", QString("Recording failed while writing the first frame: %1").arg(errorMessage));
        m_windowRecorder->stop();
        return;
    }

    m_windowRecordingActive = true;
    if (m_recordingFrameTimer) {
        m_recordingFrameTimer->start();
    }
    Logger::instance().logInfo("MainWindow", QString("Recording started: %1").arg(outputPath));
}

void MainWindow::stopWindowRecording() {
    if (!m_windowRecordingActive && (!m_windowRecorder || !m_windowRecorder->isActive())) {
        return;
    }

    if (m_recordingFrameTimer) {
        m_recordingFrameTimer->stop();
    }

    const QString outputPath = m_windowRecorder ? m_windowRecorder->outputPath() : QString();
    const int frameCount = m_windowRecorder ? m_windowRecorder->frameCount() : 0;
    if (m_windowRecorder) {
        m_windowRecorder->stop();
    }
    m_windowRecordingActive = false;

    if (!outputPath.isEmpty()) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("Recording saved to: %1 (%2 frames)").arg(outputPath).arg(frameCount));
        showRecordingSuccessFeedback(outputPath);
    }
}

void MainWindow::captureWindowRecordingFrame() {
    if (!m_windowRecordingActive || !m_windowRecorder || !m_windowRecorder->isActive()) {
        return;
    }

    prepareWindowCaptureFrame();
    QImage frame = renderWindowScreenshot();
    if (frame.isNull()) {
        Logger::instance().logWarning("MainWindow", "Recording skipped an empty frame.");
        return;
    }
    applyScreenshotWatermark(frame);

    QString errorMessage;
    if (!m_windowRecorder->writeFrame(frame, &errorMessage)) {
        Logger::instance().logError("MainWindow", QString("Recording stopped after frame write failure: %1").arg(errorMessage));
        stopWindowRecording();
    }
}

void MainWindow::showRecordingSuccessFeedback(const QString& outputPath) {
    playScreenshotShutterSound();
    showRecordingSuccessNotification(outputPath);
}

void MainWindow::showRecordingSuccessNotification(const QString& outputPath) {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return;
    }

    LocalizationManager& loc = LocalizationManager::instance();
    NOTIFYICONDATAW notifyData = {};
    notifyData.cbSize = sizeof(NOTIFYICONDATAW);
    notifyData.hWnd = hwnd;
    notifyData.uID = kRecordingNotificationIconId;
    notifyData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_INFO;
    notifyData.uCallbackMessage = WM_APP + 32;
    notifyData.hIcon = reinterpret_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr),
        L"IDI_ICON1",
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR));
    if (!notifyData.hIcon) {
        notifyData.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    notifyData.dwInfoFlags = NIIF_INFO;

    copyNotifyText(notifyData.szTip, std::size(notifyData.szTip), QStringLiteral("APE HOI4 Tool Studio"));
    copyNotifyText(notifyData.szInfoTitle, std::size(notifyData.szInfoTitle),
        loc.getString("MainWindow", "RecordingSuccessTitle"));
    copyNotifyText(notifyData.szInfo, std::size(notifyData.szInfo),
        loc.getString("MainWindow", "RecordingSuccessMsg").arg(QDir::toNativeSeparators(outputPath)));

    NOTIFYICONDATAW cleanupData = {};
    cleanupData.cbSize = sizeof(NOTIFYICONDATAW);
    cleanupData.hWnd = hwnd;
    cleanupData.uID = kRecordingNotificationIconId;
    Shell_NotifyIconW(NIM_DELETE, &cleanupData);

    if (Shell_NotifyIconW(NIM_ADD, &notifyData)) {
        notifyData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &notifyData);
        Shell_NotifyIconW(NIM_MODIFY, &notifyData);
        QTimer::singleShot(10000, this, [hwnd]() {
            NOTIFYICONDATAW delayedCleanupData = {};
            delayedCleanupData.cbSize = sizeof(NOTIFYICONDATAW);
            delayedCleanupData.hWnd = hwnd;
            delayedCleanupData.uID = kRecordingNotificationIconId;
            Shell_NotifyIconW(NIM_DELETE, &delayedCleanupData);
        });
    } else {
        Logger::instance().logWarning("MainWindow", "Failed to send recording success shell notification.");
    }
}

bool MainWindow::handleGlobalUtilityShortcut(QKeyEvent* keyEvent) {
    if (!keyEvent) {
        return false;
    }

    const Qt::KeyboardModifiers modifiers =
        keyEvent->modifiers()
        & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (modifiers != Qt::AltModifier) {
        return false;
    }

    if (keyEvent->key() == Qt::Key_E) {
        if (!ToolManager::instance().isToolActive()) {
            return false;
        }
        keyEvent->accept();
        if (keyEvent->type() == QEvent::KeyPress && !keyEvent->isAutoRepeat()) {
            closeActiveToolWithConfirmation();
        }
        return true;
    }

    if (keyEvent->key() == Qt::Key_Q) {
        keyEvent->accept();
        if (keyEvent->type() == QEvent::KeyPress && !keyEvent->isAutoRepeat()) {
            clearCurrentTypingFocus();
        }
        return true;
    }

    return false;
}

bool MainWindow::clearCurrentTypingFocus() {
    bool focusCleared = false;

    QWidget* focusWidget = QApplication::focusWidget();
    if (focusWidget) {
        focusWidget->clearFocus();
        focusCleared = true;
    }

    if (QWidget* toolWidget = activeDashboardToolWidget()) {
        if (QQuickWidget* quickWidget = findQuickWidgetInToolHost(toolWidget)) {
            if (QQuickWindow* quickWindow = quickWidget->quickWindow()) {
                if (QQuickItem* activeItem = quickWindow->activeFocusItem()) {
                    activeItem->setFocus(false);
                    focusCleared = true;
                }
            }
        }
    }

    if (m_dashboard) {
        m_dashboard->setFocus(Qt::ShortcutFocusReason);
    } else {
        setFocus(Qt::ShortcutFocusReason);
    }

    return focusCleared;
}

bool MainWindow::handleToolTopbarShortcut(QKeyEvent* keyEvent) {
    if (!keyEvent || !m_activeScriptedHostController || isToolEditableInputFocused()) {
        return false;
    }

    const QVariantMap topbarState = m_activeScriptedHostController->topbarState();
    if (!topbarState.value(QStringLiteral("visible"), true).toBool()) {
        return false;
    }

    auto shortcutFromButton = [](const QVariantMap& button, int index, bool useNumericFallback) {
        QString shortcut = button.value(QStringLiteral("shortcut")).toString().trimmed();
        if (shortcut.isEmpty() && useNumericFallback && index >= 0 && index < 9) {
            shortcut = QString::number(index + 1);
        }
        return shortcut;
    };

    auto handleButtonList = [&](const QVariant& buttonsValue, bool useNumericFallback) {
        const QVariantList buttons = buttonsValue.toList();
        for (int index = 0; index < buttons.size(); ++index) {
            const QVariantMap button = buttons.at(index).toMap();
            const QString actionId = button.value(QStringLiteral("actionId")).toString().trimmed();
            if (actionId.isEmpty()) {
                continue;
            }
            if (button.contains(QStringLiteral("enabled")) && !button.value(QStringLiteral("enabled")).toBool()) {
                continue;
            }
            if (button.contains(QStringLiteral("visible")) && !button.value(QStringLiteral("visible")).toBool()) {
                continue;
            }

            const QString shortcut = shortcutFromButton(button, index, useNumericFallback);
            if (shortcut.isEmpty() || !shortcutMatchesKeyEvent(shortcut, *keyEvent)) {
                continue;
            }

            keyEvent->accept();
            if (keyEvent->type() == QEvent::KeyPress && !keyEvent->isAutoRepeat()) {
                m_activeScriptedHostController->invokeTopbarShortcut(actionId);
            }
            return true;
        }
        return false;
    };

    return handleButtonList(topbarState.value(QStringLiteral("leftButtons")), true)
        || handleButtonList(topbarState.value(QStringLiteral("buttons")), false)
        || handleButtonList(topbarState.value(QStringLiteral("rightButtons")), false);
}

bool MainWindow::isToolEditableInputFocused() const {
    QWidget* focusWidget = QApplication::focusWidget();
    if (focusWidget
        && (focusWidget->inherits("QLineEdit")
            || focusWidget->inherits("QTextEdit")
            || focusWidget->inherits("QPlainTextEdit")
            || focusWidget->inherits("QAbstractSpinBox")
            || focusWidget->inherits("QComboBox"))) {
        return true;
    }

    QWidget* toolWidget = activeDashboardToolWidget();
    QQuickWidget* quickWidget = toolWidget ? findQuickWidgetInToolHost(toolWidget) : nullptr;
    if (!quickWidget) {
        return false;
    }

    QQuickWindow* quickWindow = quickWidget->quickWindow();
    return quickWindow && isEditableQuickItem(quickWindow->activeFocusItem());
}

bool MainWindow::shortcutMatchesKeyEvent(const QString& shortcut, const QKeyEvent& keyEvent) {
    const QString trimmedShortcut = shortcut.trimmed();
    if (trimmedShortcut.isEmpty() || keyEvent.key() == Qt::Key_unknown) {
        return false;
    }

    switch (keyEvent.key()) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
        return false;
    default:
        break;
    }

    QKeySequence configuredSequence = QKeySequence::fromString(trimmedShortcut, QKeySequence::PortableText);
    if (configuredSequence.isEmpty()) {
        configuredSequence = QKeySequence::fromString(trimmedShortcut, QKeySequence::NativeText);
    }
    if (configuredSequence.isEmpty() || configuredSequence.count() != 1) {
        return false;
    }

    const Qt::KeyboardModifiers relevantModifiers =
        keyEvent.modifiers()
        & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    const QKeySequence eventSequence(static_cast<int>(relevantModifiers) | keyEvent.key());
    return configuredSequence.matches(eventSequence) == QKeySequence::ExactMatch
        && eventSequence.matches(configuredSequence) == QKeySequence::ExactMatch;
}

void MainWindow::collapseSidebar() {
    if (m_fullscreenPresentationActive && !m_sidebarExpanded) {
        return;
    }
    if (!m_sidebarExpanded) return;

    setSidebarExpandedPresentation(false);
    startSidebarWidthAnimation(60, false);
    return;
    
    Logger::instance().logInfo("MainWindow", "[COLLAPSE_SIDEBAR] ===== collapseSidebar() START =====");
    
    // Log tool UI widget state BEFORE collapse
    if (m_activeScriptedHostController && m_activeScriptedHostController->isV2()) {
        QWidget* toolWidget = nullptr;
        for (int i = 0; i < m_dashboardContent->layout()->count(); ++i) {
            QLayoutItem* item = m_dashboardContent->layout()->itemAt(i);
            if (item && item->widget()) {
                toolWidget = item->widget();
                break;
            }
        }
        
        if (toolWidget) {
            Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR] Tool widget BEFORE collapse:"));
            Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   ptr=%1").arg(reinterpret_cast<quintptr>(toolWidget), 0, 16));
            Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   enabled=%1, visible=%2, hasFocus=%3")
                .arg(toolWidget->isEnabled())
                .arg(toolWidget->isVisible())
                .arg(toolWidget->hasFocus()));
            Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   geometry=%1x%2+%3+%4")
                .arg(toolWidget->width())
                .arg(toolWidget->height())
                .arg(toolWidget->x())
                .arg(toolWidget->y()));
            Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   transparentForMouse=%1, focusPolicy=%2")
                .arg(toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents))
                .arg(toolWidget->focusPolicy()));
            Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   windowFlags=%1")
                .arg(static_cast<int>(toolWidget->windowFlags()), 0, 16));
            
            // Check parent chain visibility
            QWidget* parent = toolWidget->parentWidget();
            int level = 0;
            while (parent && level < 5) {
                Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   parent[%1]=%2, visible=%3")
                    .arg(level)
                    .arg(reinterpret_cast<quintptr>(parent), 0, 16)
                    .arg(parent->isVisible()));
                parent = parent->parentWidget();
                level++;
            }
        } else {
            Logger::instance().logWarning("MainWindow", "[COLLAPSE_SIDEBAR] No tool widget found in layout!");
        }
    }
    
    QPropertyAnimation *anim = new QPropertyAnimation(m_sidebar, "maximumWidth");
    anim->setDuration(500); // Slower animation
    anim->setEasingCurve(QEasingCurve::InOutQuad);
    anim->setStartValue(250);
    anim->setEndValue(60);
    connect(anim, &QPropertyAnimation::valueChanged, this, [this]() {
        Logger::instance().logInfo("MainWindow", "[COLLAPSE_SIDEBAR] Animation valueChanged callback");
        
        // Use throttle to prevent excessive geometry updates
        if (!m_geometryUpdateThrottleTimer->isActive()) {
            updateRightSidebarGeometries();
            m_geometryUpdateThrottleTimer->start();
        }
        
        // Log tool widget state during animation
        if (m_activeScriptedHostController && m_activeScriptedHostController->isV2()) {
            QWidget* toolWidget = nullptr;
            for (int i = 0; i < m_dashboardContent->layout()->count(); ++i) {
                QLayoutItem* item = m_dashboardContent->layout()->itemAt(i);
                if (item && item->widget()) {
                    toolWidget = item->widget();
                    break;
                }
            }
            
            if (toolWidget) {
                Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR] Tool widget DURING animation: enabled=%1, visible=%2, transparentForMouse=%3, geometry=%4x%5")
                    .arg(toolWidget->isEnabled())
                    .arg(toolWidget->isVisible())
                    .arg(toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents))
                    .arg(toolWidget->width())
                    .arg(toolWidget->height()));
            }
        }
    });
    connect(anim, &QPropertyAnimation::finished, this, [this]() {
        Logger::instance().logInfo("MainWindow", "[COLLAPSE_SIDEBAR] Animation finished callback");
        
        updateRightSidebarGeometries();
        
        // Log tool widget state after animation
        if (m_activeScriptedHostController && m_activeScriptedHostController->isV2()) {
            QWidget* toolWidget = nullptr;
            for (int i = 0; i < m_dashboardContent->layout()->count(); ++i) {
                QLayoutItem* item = m_dashboardContent->layout()->itemAt(i);
                if (item && item->widget()) {
                    toolWidget = item->widget();
                    break;
                }
            }
            
            if (toolWidget) {
                Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR] Tool widget AFTER animation:"));
                Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   enabled=%1, visible=%2, hasFocus=%3")
                    .arg(toolWidget->isEnabled())
                    .arg(toolWidget->isVisible())
                    .arg(toolWidget->hasFocus()));
                Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   geometry=%1x%2+%3+%4")
                    .arg(toolWidget->width())
                    .arg(toolWidget->height())
                    .arg(toolWidget->x())
                    .arg(toolWidget->y()));
                Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   transparentForMouse=%1, focusPolicy=%2")
                    .arg(toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents))
                    .arg(toolWidget->focusPolicy()));
                
                // CRITICAL: Check if widget can receive mouse events
                QPoint globalCursorPos = QCursor::pos();
                QWidget* widgetAtCursor = QApplication::widgetAt(globalCursorPos);
                Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR]   widgetAtCursor=%1 (toolWidget=%2)")
                    .arg(reinterpret_cast<quintptr>(widgetAtCursor), 0, 16)
                    .arg(reinterpret_cast<quintptr>(toolWidget), 0, 16));
                
                // CRITICAL FIX: If widget has wrong attributes, fix them immediately
                if (toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents)) {
                    Logger::instance().logError("MainWindow", "[COLLAPSE_SIDEBAR] !!!!! CRITICAL: WA_TransparentForMouseEvents is TRUE after animation, forcing to FALSE !!!!!");
                    toolWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
                }
                
                if (!toolWidget->isEnabled()) {
                    Logger::instance().logError("MainWindow", "[COLLAPSE_SIDEBAR] !!!!! CRITICAL: Widget is DISABLED after animation, forcing to ENABLED !!!!!");
                    toolWidget->setEnabled(true);
                }
                
                if (toolWidget->width() == 0 || toolWidget->height() == 0) {
                    Logger::instance().logError("MainWindow", "[COLLAPSE_SIDEBAR] !!!!! CRITICAL: Widget has 0x0 geometry after animation !!!!!");
                    Logger::instance().logError("MainWindow", "[COLLAPSE_SIDEBAR] Forcing layout update...");
                    m_dashboardContent->layout()->activate();
                    m_dashboardContent->updateGeometry();
                    toolWidget->updateGeometry();
                    QCoreApplication::processEvents();
                }
                
                // CRITICAL FIX: Force QQuickWidget to complete rendering after geometry change
                // QQuickWidget uses OpenGL rendering which is asynchronous
                // After geometry change, the scene graph needs to be updated before mouse events work correctly
                if (QQuickWidget* quickWidget = findQuickWidgetInToolHost(toolWidget)) {
                    Logger::instance().logInfo("MainWindow", "[COLLAPSE_SIDEBAR] Forcing nested QQuickWidget render sync...");
                    
                    // Force immediate update of the QQuickWidget's internal state
                    quickWidget->update();
                    
                    // Process all pending events to ensure rendering is complete
                    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                    
                    // Force the QML scene graph to synchronize
                    // This ensures the scene graph is updated before any mouse events are processed
                    if (QQuickWindow* quickWindow = quickWidget->quickWindow()) {
                        quickWindow->update();
                        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                    }
                    
                    // CRITICAL FIX: Force Qt to recalculate mouse event regions after geometry changes.
                    // The main window must not use WA_TranslucentBackground; this refresh only keeps
                    // QQuickWidget input regions synchronized with the wrapper.
                    
                    // Method 1: Hide and show to force region recalculation
                    quickWidget->hide();
                    QCoreApplication::processEvents();
                    quickWidget->show();
                    QCoreApplication::processEvents();
                    
                    // Method 2: Re-enable and raise
                    quickWidget->setEnabled(true);
                    quickWidget->raise();
                    quickWidget->activateWindow();
                    quickWidget->setFocus(Qt::OtherFocusReason);
                    
                    Logger::instance().logInfo("MainWindow", "[COLLAPSE_SIDEBAR] QQuickWidget render sync complete");
                    
                    // CRITICAL DEBUG: Check QML root object enabled state
                    if (QQuickWindow* quickWindow = quickWidget->quickWindow()) {
                        if (QQuickItem* contentItem = quickWindow->contentItem()) {
                            QVariant enabledProp = contentItem->property("enabled");
                            Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR] QML root contentItem enabled=%1")
                                .arg(enabledProp.isValid() ? enabledProp.toString() : "invalid"));
                        }
                    }
                    
                    // Check the actual QML root object from QQuickWidget
                    if (QQuickItem* rootObj = quickWidget->rootObject()) {
                        QVariant enabledProp = rootObj->property("enabled");
                        Logger::instance().logInfo("MainWindow", QString("[COLLAPSE_SIDEBAR] QML rootObject enabled=%1")
                            .arg(enabledProp.isValid() ? enabledProp.toString() : "invalid"));
                        
                        // CRITICAL FIX: Force QML root object to be enabled
                        if (enabledProp.isValid() && !enabledProp.toBool()) {
                            Logger::instance().logError("MainWindow", "[COLLAPSE_SIDEBAR] !!!!! CRITICAL: QML root object is DISABLED !!!!!");
                            Logger::instance().logError("MainWindow", "[COLLAPSE_SIDEBAR] Forcing QML root object to enabled=true");
                            rootObj->setProperty("enabled", true);
                        }
                    }
                }
            }
        }
        
        Logger::instance().logInfo("MainWindow", "[COLLAPSE_SIDEBAR] ===== collapseSidebar() END =====");
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
    m_sidebar->setMinimumWidth(60);
    m_sidebarLayout->setContentsMargins(0, 20, 0, 20); // Remove side margins for centering
    m_appTitle->hide();
    m_appIcon->hide();
    m_bottomAppIcon->show();
    m_controlsHorizontal->hide();
    m_controlsVertical->show();
    m_titleLayout->setAlignment(Qt::AlignCenter);
    
    m_toolsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_accountBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_settingsBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_configBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    
    m_toolsBtn->setText("");
    m_accountBtn->setText("");
    m_settingsBtn->setText("");
    m_configBtn->setText("");
    
    m_toolsBtn->show();
    m_accountBtn->show();
    m_settingsBtn->show();
    m_configBtn->show();
    
    m_sidebarExpanded = false;
}

bool MainWindow::event(QEvent *event) {
    if (kTempWindowIdTrace && event && (event->type() == QEvent::WinIdChange || event->type() == QEvent::PlatformSurface)) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] event type=%1 %2")
                .arg(eventTypeName(event->type()))
                .arg(windowSnapshot(this))
        );
    }
    if (event && (event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonRelease
        || event->type() == QEvent::MouseButtonDblClick
        || event->type() == QEvent::MouseMove)) {
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", QString("[INPUT_CHAIN] MainWindow::event type=%1 acceptedBefore=%2")
                .arg(eventTypeName(event->type()))
                .arg(boolText(event->isAccepted())));
        }
        ensureNativeWindowAcceptsInput(QStringLiteral("MainWindow::event_%1").arg(eventTypeName(event->type())));
        logToolInputChain(QStringLiteral("MainWindow::event_%1").arg(eventTypeName(event->type())));
    }
    return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (kVerboseToolUiLogging && event && (event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonRelease
        || event->type() == QEvent::MouseMove)) {
        Logger::instance().logInfo("MainWindow", QString("[INPUT_CHAIN] eventFilter obj=%1 type=%2 acceptedBefore=%3")
            .arg(widgetDebugName(qobject_cast<QWidget*>(obj)))
            .arg(eventTypeName(event->type()))
            .arg(boolText(event->isAccepted())));
    }

    if (event && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
        const bool screenshotShortcut =
            keyEvent->key() == Qt::Key_S
            && modifiers.testFlag(Qt::AltModifier)
            && !modifiers.testFlag(Qt::ShiftModifier)
            && !modifiers.testFlag(Qt::ControlModifier)
            && !modifiers.testFlag(Qt::MetaModifier);
        const bool recordingShortcut =
            keyEvent->key() == Qt::Key_S
            && modifiers.testFlag(Qt::AltModifier)
            && modifiers.testFlag(Qt::ShiftModifier)
            && !modifiers.testFlag(Qt::ControlModifier)
            && !modifiers.testFlag(Qt::MetaModifier);

        if (screenshotShortcut || recordingShortcut) {
            if (event->type() == QEvent::ShortcutOverride) {
                keyEvent->accept();
                return false;
            }

            if (!keyEvent->isAutoRepeat()) {
                if (recordingShortcut) {
                    toggleWindowRecording();
                } else {
                    captureWindowScreenshot();
                }
            }
            keyEvent->accept();
            return true;
        }

        if (handleGlobalUtilityShortcut(keyEvent)) {
            return event->type() != QEvent::ShortcutOverride;
        }

        if (handleToolTopbarShortcut(keyEvent)) {
            return event->type() != QEvent::ShortcutOverride;
        }
    }

    if ((obj == m_dashboard || obj == m_mainStack)
        && (event->type() == QEvent::Resize
            || event->type() == QEvent::LayoutRequest
            || event->type() == QEvent::Show)) {
        updateRightSidebarGeometries();
    }

    if ((obj == m_loadingOverlay
         || obj == m_updateOverlay
         || obj == m_advertisementOverlay
         || obj == m_loginOverlay
         || obj == m_setupOverlay
         || obj == m_connectionWarningOverlay
         || obj == m_userAgreementOverlay)
        && (event->type() == QEvent::Show
            || event->type() == QEvent::ZOrderChange
            || event->type() == QEvent::Resize)) {
        QTimer::singleShot(0, this, [this]() {
            updateWindowControlsOverlayGeometry();
        });
    }

    if (obj == m_sidebar && ConfigManager::instance().getSidebarCompactMode() && !m_fullscreenPresentationActive) {
        if (event->type() == QEvent::Enter) {
            if (m_sidebarLocked) {
                return QMainWindow::eventFilter(obj, event);
            }
            // Stop collapse timer if running, then expand
            m_sidebarCollapseTimer->stop();
            expandSidebar();
        } else if (event->type() == QEvent::Leave) {
            if (m_sidebarLocked) {
                return QMainWindow::eventFilter(obj, event);
            }
            // Start 1.5 second delay timer before collapsing
            m_sidebarCollapseTimer->start();
        }
    }

    if (obj == m_rightSidebarResizeHandle && m_rightSidebarListVisible) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                setRightSidebarResizeDragging(true);
                m_rightSidebarResizeStartGlobalX = mouseEvent->globalPosition().toPoint().x();
                m_rightSidebarResizeStartWidth = currentRightSidebarListWidth();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton) && m_rightSidebarResizeDragging) {
                const int delta = m_rightSidebarResizeStartGlobalX - mouseEvent->globalPosition().toPoint().x();
                m_rightSidebarListWidth = normalizedRightSidebarListWidth(m_rightSidebarResizeStartWidth + delta);
                if (m_rightSidebarListWidth > 0) {
                    m_rightSidebarLastExpandedWidth = m_rightSidebarListWidth;
                }
                updateRightSidebarGeometries();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                setRightSidebarResizeDragging(false);
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (kVerboseToolUiLogging) {
        Logger::instance().logInfo("MainWindow", QString("[INPUT_CHAIN] MainWindow::mousePressEvent button=%1 pos=(%2,%3) acceptedBefore=%4")
            .arg(event ? static_cast<int>(event->button()) : -1)
            .arg(event ? event->pos().x() : -1)
            .arg(event ? event->pos().y() : -1)
            .arg(event ? boolText(event->isAccepted()) : QStringLiteral("false")));
    }
    ensureNativeWindowAcceptsInput(QStringLiteral("MainWindow::mousePressEvent"));
    if (isFullScreen()) {
        return;
    }
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (isFullScreen()) {
        return;
    }
    if (event->buttons() & Qt::LeftButton && m_dragging && !m_rightSidebarResizeDragging) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    m_dragging = false;
    setRightSidebarResizeDragging(false);
}

void MainWindow::processPendingExternalRequest() {
    if (m_pendingExternalRequestHandled || !m_pendingExternalRequest.isValid()) {
        return;
    }

    m_pendingExternalRequestHandled = true;

    switch (m_pendingExternalRequest.type) {
    case ExternalPackageManager::RequestType::ToolDescriptor:
        processPendingToolRequest();
        break;
    case ExternalPackageManager::RequestType::PluginDescriptor:
        processPendingPluginRequest();
        break;
    case ExternalPackageManager::RequestType::None:
    default:
        finishPendingRequest();
        break;
    }
}

void MainWindow::processPendingToolRequest() {
    LocalizationManager& loc = LocalizationManager::instance();
    ExternalPackageManager::ImportResult result =
        ExternalPackageManager::importToolPackage(
            m_pendingExternalRequest.descriptorPath,
            this,
            m_pendingExternalRequest.overwriteApproved
        );

    if (result.requiresRestart) {
        if (!restartApplicationForPendingRequest(
                loc.getString("ExternalPackage", "RestartRequiredForToolMessage").arg(result.importedName))) {
            finishPendingRequest();
        }
        return;
    }

    if (result.cancelled && !result.useInstalledCopy) {
        finishPendingRequest();
        return;
    }

    if (!result.success) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "ToolOpenFailedTitle"),
            loc.getString("ExternalPackage", "ToolOpenFailedMessage").arg(result.errorMessage)
        );
        finishPendingRequest();
        return;
    }

    ToolManager::instance().loadTools();
    m_toolsPage->refreshTools();

    ToolInterface* tool = ToolManager::instance().getTool(result.importedId);
    if (!tool) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "ToolOpenFailedTitle"),
            loc.getString("ExternalPackage", "ImportedToolNotFound").arg(result.importedName)
        );
        finishPendingRequest();
        return;
    }

    if (result.alreadyInstalled) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "InstalledToolOpenTitle"),
            loc.getString("ExternalPackage", "InstalledToolOpenMessage").arg(result.importedName)
        );
    } else if (result.useInstalledCopy) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "UseInstalledToolTitle"),
            loc.getString("ExternalPackage", "UseInstalledToolMessage").arg(result.importedName)
        );
    }

    onToolSelected(result.importedId);
    finishPendingRequest();
}

void MainWindow::processPendingPluginRequest() {
    LocalizationManager& loc = LocalizationManager::instance();
    ExternalPackageManager::ImportResult result =
        ExternalPackageManager::importPluginPackage(
            m_pendingExternalRequest.descriptorPath,
            this,
            m_pendingExternalRequest.overwriteApproved
        );

    if (result.requiresRestart) {
        if (!restartApplicationForPendingRequest(
                loc.getString("ExternalPackage", "RestartRequiredForPluginMessage").arg(result.importedName))) {
            finishPendingRequest();
        }
        return;
    }

    if (result.cancelled) {
        finishPendingRequest();
        return;
    }

    if (!result.success) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "PluginInstallFailedTitle"),
            loc.getString("ExternalPackage", "PluginInstallFailedMessage").arg(result.errorMessage)
        );
        finishPendingRequest();
        return;
    }

    PluginManager::instance().loadPlugins();

    if (result.alreadyInstalled) {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "PluginAlreadyInstalledTitle"),
            loc.getString("ExternalPackage", "PluginAlreadyInstalledMessage").arg(result.importedName)
        );
    } else {
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "PluginInstallSuccessTitle"),
            loc.getString("ExternalPackage", "PluginInstallSuccessMessage").arg(result.importedName)
        );
    }
    finishPendingRequest();
}

void MainWindow::finishPendingRequest() {
    m_pendingExternalRequest = ExternalPackageManager::PendingRequest();
}

bool MainWindow::restartApplicationForPendingRequest(const QString& restartMessage) {
    LocalizationManager& loc = LocalizationManager::instance();

    ExternalPackageManager::PendingRequest restartRequest = m_pendingExternalRequest;
    restartRequest.overwriteApproved = true;

    QString saveError;
    if (!ExternalPackageManager::savePendingRestartRequest(restartRequest, &saveError)) {
        const QString failureMessage = saveError.trimmed().isEmpty()
                                           ? loc.getString("ExternalPackage", "SavePendingRestartRequestFailed")
                                           : saveError;
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "RestartRequiredTitle"),
            failureMessage
        );
        return false;
    }

    QString applicationPath = QCoreApplication::applicationFilePath();
    applicationPath.replace('/', '\\');

    const QString currentPid = QString::number(QCoreApplication::applicationPid());
    const QString delayedLaunchCommand = QString(
        "set \"APEHTS_PID=%1\" && "
        ":waitExit && "
        "tasklist /FI \"PID eq %1\" | find \"%1\" >nul && "
        "(ping 127.0.0.1 -n 2 >nul && goto waitExit) & "
        "start \"\" \"%2\" --recover-pending-request"
    ).arg(currentPid, applicationPath);

    if (!QProcess::startDetached("cmd.exe", QStringList() << "/c" << delayedLaunchCommand)) {
        ExternalPackageManager::clearPendingRestartRequest();
        CustomMessageBox::information(
            this,
            loc.getString("ExternalPackage", "RestartRequiredTitle"),
            loc.getString("ExternalPackage", "RestartProcessSpawnFailed")
        );
        return false;
    }

    m_restartRequested = true;

    CustomMessageBox::information(
        this,
        loc.getString("ExternalPackage", "RestartRequiredTitle"),
        restartMessage
    );

    close();
    return true;
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updateWindowControlsOverlayGeometry();
    updateFullscreenChromeGeometry();
    updateRightSidebarGeometries();
    applyNativeRoundedCorners(QStringLiteral("resizeEvent"));
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] showEvent:before corners %1 stackIndex=%2")
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    applyNativeRoundedCorners(QStringLiteral("showEvent"));
    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] showEvent:after corners %1 stackIndex=%2")
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    raiseWindowControlsOverlay();
    updateFullscreenChromeGeometry();
    updateRightSidebarGeometries();
    QTimer::singleShot(0, this, [this]() {
        updateRightSidebarGeometries();
        raiseWindowControlsOverlay();
        updateFullscreenChromeGeometry();
    });
}

void MainWindow::hideEvent(QHideEvent *event) {
    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] hideEvent %1 stackIndex=%2")
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    QMainWindow::hideEvent(event);
}

void MainWindow::bringToFront() {
    if (isMinimized()) {
        showNormal();
    }

    show();
    raise();
    activateWindow();
#ifdef Q_OS_WIN
    SetForegroundWindow(reinterpret_cast<HWND>(winId()));
#endif
}

void MainWindow::rebuildActiveToolUi() {
    ensureNativeWindowAcceptsInput(QStringLiteral("rebuildActiveToolUi_start"));
    logToolInputChain(QStringLiteral("rebuildActiveToolUi_start"));
    if (kTempWindowIdTrace) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("[WIN_TRACE] rebuildActiveToolUi:start %1 stackIndex=%2")
                .arg(windowSnapshot(this))
                .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
        );
    }
    if (kVerboseToolUiLogging) {
        Logger::instance().logInfo("MainWindow", "[EVENT_CHAIN] ===== rebuildActiveToolUi called =====");
        Logger::instance().logInfo("MainWindow", QString("[EVENT_CHAIN] winId=%1, isVisible=%2")
            .arg(QString::number(reinterpret_cast<quintptr>(winId())))
            .arg(isVisible() ? "true" : "false"));
    }
    
    if (!m_activeScriptedHostController || !m_dashboardContent || !m_dashboardContent->layout()) {
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", "[EVENT_CHAIN] rebuildActiveToolUi() early return - missing controller or layout");
        }
        return;
    }

    if (m_activeScriptedTopbarWidget) {
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", "[EVENT_CHAIN] Hiding and deleting topbar widget");
        }
        m_activeScriptedTopbarWidget->hide();
        m_activeScriptedTopbarWidget->deleteLater();
        m_activeScriptedTopbarWidget = nullptr;
    }

    if (m_activeScriptedSidebarWidget) {
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", "[EVENT_CHAIN] Hiding and deleting sidebar widget");
        }
        m_activeScriptedSidebarWidget->hide();
        m_activeScriptedSidebarWidget->deleteLater();
        m_activeScriptedSidebarWidget = nullptr;
    }

    if (m_activeScriptedHostController->isV2()) {
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", "[EVENT_CHAIN] V2 tool detected, getting persistent view");
        }
        
        // CRITICAL FIX: Ensure parent widget is visible FIRST
        // Child widget cannot be visible if parent is hidden
        if (m_dashboardContent && !m_dashboardContent->isVisible()) {
            if (kVerboseToolUiLogging) {
                Logger::instance().logInfo("MainWindow", "[EVENT_CHAIN] Parent m_dashboardContent is hidden, showing it BEFORE buildUiV2");
            }
            m_dashboardContent->show();
        }
        
        // Get or create the persistent view with correct parent - no deletion/recreation
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", "[EVENT_CHAIN] Calling buildUiV2...");
        }
        QWidget* pageWidget = m_activeScriptedHostController->buildUiV2(m_dashboardContent);
        if (!pageWidget) {
            Logger::instance().logWarning(
                "MainWindow",
                QStringLiteral("[EVENT_CHAIN] Skipped V2 UI rebuild because the persistent view is not available.")
            );
            return;
        }

        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", QString("[EVENT_CHAIN] Got pageWidget=%1, parent=%2")
                .arg(QString::number(reinterpret_cast<quintptr>(pageWidget)))
                .arg(QString::number(reinterpret_cast<quintptr>(pageWidget->parentWidget()))));
            Logger::instance().logInfo("MainWindow", QString("[EVENT_CHAIN] AFTER buildUiV2 - pageWidget state: enabled=%1, visible=%2, transparentForMouse=%3, focusPolicy=%4")
                .arg(pageWidget->isEnabled())
                .arg(pageWidget->isVisible())
                .arg(pageWidget->testAttribute(Qt::WA_TransparentForMouseEvents))
                .arg(pageWidget->focusPolicy()));
            Logger::instance().logInfo("MainWindow", QString("[EVENT_CHAIN] pageWidget geometry: %1x%2+%3+%4, windowFlags=%5")
                .arg(pageWidget->width())
                .arg(pageWidget->height())
                .arg(pageWidget->x())
                .arg(pageWidget->y())
                .arg(static_cast<int>(pageWidget->windowFlags()), 0, 16));
        }

        QLayout* layout = m_dashboardContent->layout();
        
        // Check if the persistent view is already in the layout
        bool viewAlreadyInLayout = false;
        for (int i = 0; i < layout->count(); ++i) {
            if (layout->itemAt(i)->widget() == pageWidget) {
                viewAlreadyInLayout = true;
                break;
            }
        }
        
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", QString("[TRACE] viewAlreadyInLayout=%1, layout item count=%2")
                .arg(viewAlreadyInLayout ? "true" : "false")
                .arg(layout->count()));
        }
        
        // Only remove widgets that are NOT the persistent view
        QList<QWidget*> widgetsToRemove;
        for (int i = layout->count() - 1; i >= 0; --i) {
            QLayoutItem* item = layout->itemAt(i);
            if (item && item->widget() && item->widget() != pageWidget) {
                widgetsToRemove.append(item->widget());
            }
        }
        
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", QString("[TRACE] Found %1 widgets to remove").arg(widgetsToRemove.size()));
        }
        
        // Remove old widgets without clearing the persistent view
        for (QWidget* widget : widgetsToRemove) {
            layout->removeWidget(widget);
            widget->hide();
            if (widget == m_dashboardTitleLabel) {
                m_dashboardTitleLabel = nullptr;
            }
            widget->deleteLater();
        }

        // CRITICAL FIX: Do NOT call clearRightSidebarContent() here for V2 tools
        // It will be called inside syncV2RightSidebarContent() via refreshActiveToolRightSidebarUi()
        // Calling it twice causes the right sidebar list to disappear
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", "[TRACE] Skipping clearRightSidebarContent for V2 tool (will be handled by syncV2RightSidebarContent)");
        }

        // Ensure the persistent view is in the layout (add only if not already present)
        // No setParent needed - parent was set correctly at creation time
        if (!viewAlreadyInLayout) {
            if (kVerboseToolUiLogging) {
                Logger::instance().logInfo("MainWindow", "[TRACE] Adding pageWidget to layout");
            }
            layout->addWidget(pageWidget);
        } else {
            if (kVerboseToolUiLogging) {
                Logger::instance().logInfo("MainWindow", "[TRACE] pageWidget already in layout, skipping addWidget");
            }
        }
        
        // CRITICAL FIX: Ensure widget has proper geometry before showing
        // Widget with 0x0 geometry will not be visible even after show()
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG] BEFORE geometry fix:"));
            Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG]   geometry=%1x%2+%3+%4")
                .arg(pageWidget->width())
                .arg(pageWidget->height())
                .arg(pageWidget->x())
                .arg(pageWidget->y()));
        }
        
        // Force layout to calculate proper size for the widget
        if (pageWidget->width() == 0 || pageWidget->height() == 0) {
            if (kVerboseToolUiLogging) {
                Logger::instance().logInfo("MainWindow", "[UI_INTERACTION_DEBUG] Widget has 0x0 geometry, forcing layout update");
            }
            layout->activate();  // Force layout to calculate sizes
            m_dashboardContent->updateGeometry();
            pageWidget->updateGeometry();
        }
        
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG] AFTER geometry fix:"));
            Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG]   geometry=%1x%2+%3+%4")
                .arg(pageWidget->width())
                .arg(pageWidget->height())
                .arg(pageWidget->x())
                .arg(pageWidget->y()));
        }
        
        // CRITICAL FIX: Ensure entire parent chain is visible
        // Child widget cannot be visible if any ancestor is hidden
        // First, ensure m_mainStack is showing the dashboard page (index 0)
        if (m_mainStack && m_mainStack->currentIndex() != 0) {
            Logger::instance().logWarning("MainWindow", QString("[UI_INTERACTION_DEBUG] m_mainStack currentIndex=%1, switching to 0 (dashboard)")
                .arg(m_mainStack->currentIndex()));
            m_mainStack->setCurrentIndex(0);
            if (kTempWindowIdTrace) {
                Logger::instance().logInfo(
                    "MainWindow",
                    QString("[WIN_TRACE] rebuildActiveToolUi:after stack switch %1 stackIndex=%2")
                        .arg(windowSnapshot(this))
                        .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
                );
            }
        }
        
        // Then ensure m_dashboard is visible
        if (m_dashboard && !m_dashboard->isVisible()) {
            if (kVerboseToolUiLogging) {
                Logger::instance().logInfo("MainWindow", "[UI_INTERACTION_DEBUG] Parent m_dashboard is hidden, showing it");
            }
            m_dashboard->show();
        }
        
        // Finally ensure m_dashboardContent is visible
        if (m_dashboardContent && !m_dashboardContent->isVisible()) {
            if (kVerboseToolUiLogging) {
                Logger::instance().logInfo("MainWindow", "[UI_INTERACTION_DEBUG] Parent m_dashboardContent is hidden, showing it");
            }
            m_dashboardContent->show();
        }
        
        // CRITICAL FIX: Always ensure widget is visible, enabled, and raised to front
        // ALSO: Force reset WA_TranslucentBackground and WA_TransparentForMouseEvents
        // These attributes may be inherited from parent widgets and cause mouse event issues
        
        // Log current state before fixing
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", QString("[ATTRIBUTE_CHECK] pageWidget BEFORE fix: translucent=%1, transparentForMouse=%2, opaque=%3")
                .arg(pageWidget->testAttribute(Qt::WA_TranslucentBackground))
                .arg(pageWidget->testAttribute(Qt::WA_TransparentForMouseEvents))
                .arg(pageWidget->testAttribute(Qt::WA_OpaquePaintEvent)));
        }
        
        // Force correct attributes on the ToolUiContainer
        pageWidget->setAttribute(Qt::WA_TranslucentBackground, false);
        pageWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        
        // Log state after fixing
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", QString("[ATTRIBUTE_CHECK] pageWidget AFTER fix: translucent=%1, transparentForMouse=%2, opaque=%3")
                .arg(pageWidget->testAttribute(Qt::WA_TranslucentBackground))
                .arg(pageWidget->testAttribute(Qt::WA_TransparentForMouseEvents))
                .arg(pageWidget->testAttribute(Qt::WA_OpaquePaintEvent)));
        }
        
        pageWidget->setEnabled(true);  // Ensure widget accepts mouse events
        pageWidget->show();             // Ensure widget is visible
        pageWidget->raise();            // Bring widget to front
        pageWidget->setFocus(Qt::OtherFocusReason);  // Set focus to receive input events
        if (kTempWindowIdTrace) {
            Logger::instance().logInfo(
                "MainWindow",
                QString("[WIN_TRACE] rebuildActiveToolUi:after page show %1 stackIndex=%2")
                    .arg(windowSnapshot(this))
                    .arg(m_mainStack ? m_mainStack->currentIndex() : -1)
            );
        }
        
        ensureNativeWindowAcceptsInput(QStringLiteral("rebuildActiveToolUi_after_page_show"));
        logToolInputChain(QStringLiteral("rebuildActiveToolUi_after_page_show"));
        
        // CRITICAL FIX: Ensure LoadingOverlay is lowered and hidden
        // Even when hidden, overlay's z-order can block events
        if (m_loadingOverlay) {
            m_loadingOverlay->hide();
            m_loadingOverlay->lower();
            if (kVerboseToolUiLogging) {
                Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG] LoadingOverlay lowered, visible=%1")
                    .arg(m_loadingOverlay->isVisible()));
            }
        }
        
        if (kVerboseToolUiLogging) {
            Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG] AFTER setEnabled/show/raise/activateWindow/setFocus:"));
            Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG]   enabled=%1, visible=%2, geometry=%3x%4")
                .arg(pageWidget->isEnabled())
                .arg(pageWidget->isVisible())
                .arg(pageWidget->width())
                .arg(pageWidget->height()));
            Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG]   parent visible=%1, transparentForMouse=%2, hasFocus=%3")
                .arg(pageWidget->parentWidget() ? pageWidget->parentWidget()->isVisible() : false)
                .arg(pageWidget->testAttribute(Qt::WA_TransparentForMouseEvents))
                .arg(pageWidget->hasFocus()));

            QPoint globalCursorPos = QCursor::pos();
            QWidget* widgetAtCursor = QApplication::widgetAt(globalCursorPos);
            Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG] Widget at cursor: %1 (pageWidget=%2)")
                .arg(reinterpret_cast<quintptr>(widgetAtCursor), 0, 16)
                .arg(reinterpret_cast<quintptr>(pageWidget), 0, 16));
        }
        
        // Check z-order by iterating through siblings
        if (kVerboseToolUiLogging && m_dashboardContent) {
            QList<QWidget*> siblings;
            for (QObject* child : m_dashboardContent->children()) {
                if (QWidget* w = qobject_cast<QWidget*>(child)) {
                    siblings.append(w);
                }
            }
            Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG] Dashboard children count: %1").arg(siblings.size()));
            for (int i = 0; i < siblings.size(); ++i) {
                QWidget* w = siblings[i];
                Logger::instance().logInfo("MainWindow", QString("[UI_INTERACTION_DEBUG]   Child[%1]: %2, visible=%3, geometry=%4x%5+%6+%7")
                    .arg(i)
                    .arg(reinterpret_cast<quintptr>(w), 0, 16)
                    .arg(w->isVisible())
                    .arg(w->width())
                    .arg(w->height())
                    .arg(w->x())
                    .arg(w->y()));
            }
        }
        
        refreshActiveToolRightSidebarUi();

        updateRightSidebarGeometries();
        return;
    }

    QLayoutItem* child = nullptr;
    while ((child = m_dashboardContent->layout()->takeAt(0)) != nullptr) {
        if (child->widget()) {
            QWidget* widget = child->widget();
            widget->hide();
            if (widget == m_dashboardTitleLabel) {
                m_dashboardTitleLabel = nullptr;
            }
            widget->deleteLater();
        }
        delete child;
    }

    clearRightSidebarContent();

    ToolGuiRenderResult renderResult = m_activeScriptedHostController->buildUi(m_dashboardContent);
    if (renderResult.mainWidget) {
        m_dashboardContent->layout()->addWidget(renderResult.mainWidget);
    }
    m_activeRightSidebarListWidget = nullptr;

    refreshActiveToolRightSidebarUi();
    updateRightSidebarGeometries();
}

void MainWindow::resetActiveToolUi() {
    ToolProxyInterface* activeProxy = ToolManager::instance().getActiveToolProxy();

    QLayoutItem *child = nullptr;
    while ((child = m_dashboardContent->layout()->takeAt(0)) != nullptr) {
        if (child->widget()) {
            QWidget* widget = child->widget();
            widget->hide();
            if (widget == m_dashboardTitleLabel) {
                m_dashboardTitleLabel = nullptr;
            }
            if (widget == m_activeScriptedTopbarWidget) {
                m_activeScriptedTopbarWidget = nullptr;
            }
            if (widget == m_activeScriptedSidebarWidget) {
                m_activeScriptedSidebarWidget = nullptr;
            }
            if (m_shutdownInProgress) {
                delete widget;
            } else {
                widget->deleteLater();
            }
        }
        delete child;
    }

    clearRightSidebarContent();

    if (m_activeScriptedTopbarWidget) {
        m_activeScriptedTopbarWidget->hide();
        if (m_shutdownInProgress) {
            delete m_activeScriptedTopbarWidget;
        } else {
            m_activeScriptedTopbarWidget->deleteLater();
        }
        m_activeScriptedTopbarWidget = nullptr;
    }

    if (m_activeScriptedSidebarWidget) {
        m_activeScriptedSidebarWidget->hide();
        if (m_shutdownInProgress) {
            delete m_activeScriptedSidebarWidget;
        } else {
            m_activeScriptedSidebarWidget->deleteLater();
        }
        m_activeScriptedSidebarWidget = nullptr;
    }

    if (m_activeScriptedHostController) {
        if (m_shutdownInProgress) {
            delete m_activeScriptedHostController;
        } else {
            m_activeScriptedHostController->deleteLater();
        }
        m_activeScriptedHostController = nullptr;
    }

    m_rightSidebarPanel->hide();
    m_rightSidebarResizeHandle->hide();
    m_rightSidebarRail->show();
    m_activeTool = nullptr;
    ToolRightSidebarState emptyState;
    emptyState.listVisible = false;
    emptyState.searchModeAvailable = false;
    emptyState.searchModeActive = false;
    emptyState.activeButtonKey = QString::fromUtf8(kRightSidebarDefaultButtonKey);
    rebuildRightSidebarRail({}, emptyState);
    updateRightSidebarGeometries();

    LocalizationManager& loc = LocalizationManager::instance();
    m_dashboardTitleLabel = new QLabel(loc.getString("MainWindow", "DashboardArea"), m_dashboardContent);
    m_dashboardTitleLabel->setAlignment(Qt::AlignCenter);
    m_dashboardContent->layout()->addWidget(m_dashboardTitleLabel);

    ToolManager::instance().setToolActive(false);
    ToolManager::instance().setActiveToolProxy(nullptr);
    if (activeProxy && !m_shutdownInProgress) {
        Logger::instance().logInfo(
            "MainWindow",
            QString("Discarding active tool worker process while closing tool: %1").arg(activeProxy->id())
        );
        activeProxy->discardProcess();
    }
    ToolRuntimeContext::instance().setPluginInvoker({});
}

bool MainWindow::ensureReadyForIncomingRequest(ExternalPackageManager::RequestType requestType) {
    bringToFront();

    if (!ToolManager::instance().isToolActive()) {
        return true;
    }

    LocalizationManager& loc = LocalizationManager::instance();
    QString title = loc.getString("ExternalPackage", "CloseRunningToolTitle");
    QString message;
    if (requestType == ExternalPackageManager::RequestType::PluginDescriptor) {
        message = loc.getString("ExternalPackage", "CloseRunningToolForPluginMessage");
    } else {
        message = loc.getString("ExternalPackage", "CloseRunningToolForToolMessage");
    }

    const QMessageBox::StandardButton reply = CustomMessageBox::question(this, title, message);
    if (reply != QMessageBox::Yes) {
        return false;
    }

    resetActiveToolUi();
    return true;
}

bool MainWindow::ensureRequiredGameDirectory() {
    const QString gamePath = PathValidator::instance().ensureGamePathDiscovered();
    if (!gamePath.isEmpty()) {
        return true;
    }

    LocalizationManager& loc = LocalizationManager::instance();
    CustomMessageBox::information(
        this,
        loc.getString("MainWindow", "GameStartupFailedTitle"),
        loc.getString("MainWindow", "GameStartupFailedMsg")
    );
    Logger::instance().logError("MainWindow", "Startup failed because the HOI4 game directory could not be discovered.");
    return false;
}

bool MainWindow::ensureRequiredDocumentDirectory() {
    const QString docPath = ConfigManager::instance().getDocPath();
    const QString docError = PathValidator::instance().validateDocPath(docPath);
    if (docError.isEmpty()) {
        return true;
    }

    LocalizationManager& loc = LocalizationManager::instance();
    const QString displayPath = docPath.isEmpty()
        ? loc.getString("Error", docError)
        : QDir::toNativeSeparators(docPath);

    CustomMessageBox::information(
        this,
        loc.getString("MainWindow", "DocumentStartupFailedTitle"),
        loc.getString("MainWindow", "DocumentStartupFailedMsg").arg(displayPath)
    );
    Logger::instance().logError("MainWindow", "Startup failed because the derived document path is invalid: " + docPath);
    return false;
}

void MainWindow::handleExternalRequest(const ExternalPackageManager::PendingRequest& request) {
    if (!request.isValid()) {
        return;
    }

    if (!ensureReadyForIncomingRequest(request.type)) {
        return;
    }

    m_pendingExternalRequest = request;
    m_pendingExternalRequestHandled = false;
    processPendingExternalRequest();
}

void MainWindow::performShutdown() {
    if (m_shutdownInProgress) {
        return;
    }

    m_shutdownInProgress = true;
    Logger::instance().logInfo(
        "MainWindow",
        QString("Starting coordinated shutdown cleanup (update_shutdown=%1)")
            .arg(m_updateShutdownRequested ? "true" : "false")
    );

    if (m_scanCheckTimer) {
        m_scanCheckTimer->stop();
    }

    if (m_sidebarCollapseTimer) {
        m_sidebarCollapseTimer->stop();
    }

    if (m_memTimer) {
        m_memTimer->stop();
    }

    PathValidator::instance().stopMonitoring();
    FileManager::instance().stopScanning();

    AuthManager::instance().shutdown();
    HttpClient::instance().shutdown();

    resetActiveToolUi();
    const bool toolsStopped = ToolManager::instance().unloadToolsAndWait(3000);
    Logger::instance().logInfo(
        "MainWindow",
        QString("Tool shutdown result: %1").arg(toolsStopped ? "all_stopped" : "timeout_or_failed")
    );
}

void MainWindow::requestUpdateShutdown() {
    if (m_shutdownInProgress || m_updateShutdownRequested) {
        return;
    }

    m_updateShutdownRequested = true;
    Logger::instance().logInfo("MainWindow", "Update requested coordinated application shutdown");
    close();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_shutdownInProgress) {
        event->accept();
        return;
    }

    hide();
    event->ignore();

    QTimer::singleShot(0, this, [this]() {
        performShutdown();
        QCoreApplication::quit();
    });
}

void MainWindow::onModPathChanged() {
    Logger::instance().logInfo("MainWindow", "Mod path changed, reloading files");

    if (!ensureRequiredGameDirectory()) {
        return;
    }

    if (!ensureRequiredDocumentDirectory()) {
        return;
    }
    
    // Show loading overlay and restart file scanning (no ad on subsequent loads)
    m_loadingOverlay->showOverlay();
    raiseWindowControlsOverlay();
    m_scanCheckTimer->start();
    FileManager::instance().startScanning();
}

void MainWindow::checkAndShowAdvertisement(bool loadFilesAfterAd) {
    const QString acceptedVersion = AgreementEvidenceManager::instance().acceptedAgreementVersion();
    const bool shouldShowAd = !acceptedVersion.trimmed().isEmpty() && acceptedVersion != "0.0.0.0";

    Logger::instance().logInfo(
        "MainWindow",
        QString("Checking advertisement condition from AgreementEvidence. accepted_version=%1 should_show=%2")
            .arg(acceptedVersion, shouldShowAd ? "true" : "false")
    );

    if (shouldShowAd) {
        if (loadFilesAfterAd) {
            disconnect(m_advertisementOverlay, &Advertisement::adClosed, this, nullptr);
            connect(m_advertisementOverlay, &Advertisement::adClosed, this, [this]() {
                Logger::instance().logInfo("MainWindow", "Ad closed, loading files");
                if (!ensureRequiredGameDirectory()) {
                    return;
                }
                if (!ensureRequiredDocumentDirectory()) {
                    return;
                }
                m_loadingOverlay->showOverlay();
                raiseWindowControlsOverlay();
                m_scanCheckTimer->start();
                FileManager::instance().startScanning();
                show();
            }, Qt::SingleShotConnection);
        }
        m_advertisementOverlay->showAd();
        raiseWindowControlsOverlay();
    } else {
        if (loadFilesAfterAd) {
            Logger::instance().logInfo("MainWindow", "No ad needed, loading files directly");
            if (!ensureRequiredGameDirectory()) {
                return;
            }
            if (!ensureRequiredDocumentDirectory()) {
                return;
            }
            m_loadingOverlay->showOverlay();
            raiseWindowControlsOverlay();
            m_scanCheckTimer->start();
            FileManager::instance().startScanning();
            show();
        }
    }
}

void MainWindow::onLoginSuccessful() {
    m_loginCompleted = true;
    Logger::instance().logInfo(
        "MainWindow",
        QString("Login successful. pendingWarning=%1 thread=%2")
            .arg(m_pendingConnectionWarning)
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
    );
    AgreementEvidenceManager::instance().flushPendingEvents(this);

    const QUrl apiWarmupUrl(ApiRequests::apiBaseUrl());
    if (apiWarmupUrl.isValid()) {
        Logger::instance().logInfo("MainWindow", "Login successful, warming up libcurl connection pool");
        HttpClient::instance().warmUpConnection(apiWarmupUrl, this, [](const HttpResponse&) {
        });
    }

    if (m_advertisementOverlay) {
        Logger::instance().logInfo("MainWindow", "Login successful, preloading advertisement resources");
        m_advertisementOverlay->preloadAd();
    }

    // After login, check for updates FIRST before loading files
    Logger::instance().logInfo("MainWindow", "Login successful, checking for updates before loading files");

    if (AuthManager::instance().hasBlockingAccountAction()) {
        QTimer::singleShot(0, this, [this]() {
            Logger::instance().logInfo("MainWindow", "login completed while account action is active, showing account warning overlay");
            m_connectionWarningOverlay->raise();
            m_connectionWarningOverlay->showAccountActionWarning(AuthManager::instance().getAccountActionInfo());
            raiseWindowControlsOverlay();
        });
    } else if (m_pendingConnectionWarning) {
        m_pendingConnectionWarning = false;
        QTimer::singleShot(0, this, [this]() {
            Logger::instance().logInfo("MainWindow", "pending warning resolved on login, calling showWarning()");
            m_connectionWarningOverlay->raise();
            m_connectionWarningOverlay->showWarning();
            raiseWindowControlsOverlay();
        });
    }

    if (kDebugForceConnectionWarning) {
        QTimer::singleShot(2000, this, [this]() {
            Logger::instance().logInfo("MainWindow", "Debug force warning after 2s.");
            m_connectionWarningOverlay->raise();
            m_connectionWarningOverlay->showWarning();
            raiseWindowControlsOverlay();
        });
    }
    
    // Show the update checking overlay immediately (blocks user interaction)
    m_updateOverlay->showCheckingOverlay();
    raiseWindowControlsOverlay();
    m_updateOverlay->checkForUpdates();
    
    // Connect to update check result
    connect(m_updateOverlay, &Update::updateCheckCompleted, this, [this](bool hasUpdate) {
        if (hasUpdate) {
            // Update dialog is shown, user must update. App will quit after update.
            Logger::instance().logInfo("MainWindow", "Update available, waiting for user to update");
            // Do nothing - the update overlay stays visible with update button
        } else {
            // No update needed, continue with normal startup flow
            Logger::instance().logInfo("MainWindow", "No update needed, continuing startup");

            if (!ensureRequiredGameDirectory()) {
                return;
            }

            if (!ensureRequiredDocumentDirectory()) {
                return;
            }
            
            if (SetupDialog::isConfigValid()) {
                // Config is valid, start path monitoring and load files directly
                Logger::instance().logInfo("MainWindow", "Config valid, starting path monitoring and loading files");
                PathValidator::instance().startMonitoring();
                m_loadingOverlay->showOverlay();
                raiseWindowControlsOverlay();
                m_scanCheckTimer->start();
                FileManager::instance().startScanning();
                show();
            } else {
                // Config invalid or missing, show setup overlay
                Logger::instance().logInfo("MainWindow", "Config invalid, showing setup");
                m_setupOverlay->showOverlay();
                raiseWindowControlsOverlay();
            }
        }
    }, Qt::SingleShotConnection);
}

void MainWindow::onLogoutRequested() {
    m_loginCompleted = false;
    m_pendingConnectionWarning = false;
    m_connectionWarningOverlay->hideWarning();
    AuthManager::instance().logout();
    m_loginOverlay->showLogin();
    raiseWindowControlsOverlay();
    closeOverlay();
}

void MainWindow::onSetupCompleted() {
    Logger::instance().logInfo("MainWindow", "Setup completed");
    
    // Check if this is from mod close (setupSkipped flag indicates flow type)
    if (m_setupSkipped) {
        // From mod close - go directly to loading files
        m_setupSkipped = false; // Reset flag
        Logger::instance().logInfo("MainWindow", "Setup from mod close - loading files");

        if (!ensureRequiredGameDirectory()) {
            return;
        }

        if (!ensureRequiredDocumentDirectory()) {
            return;
        }
        
        // Restart path monitoring
        PathValidator::instance().startMonitoring();
        
        // Restart file scanning for new mod
        m_loadingOverlay->showOverlay();
        raiseWindowControlsOverlay();
        m_scanCheckTimer->start();
        FileManager::instance().startScanning();
        
        // Show main window
        show();
    } else {
        // From startup sequence - load files directly (ad will be shown after files are loaded)
        Logger::instance().logInfo("MainWindow", "Setup from startup - loading files");

        if (!ensureRequiredGameDirectory()) {
            return;
        }

        if (!ensureRequiredDocumentDirectory()) {
            return;
        }
        
        // Restart path monitoring
        PathValidator::instance().startMonitoring();
        
        // Start file scanning
        m_loadingOverlay->showOverlay();
        raiseWindowControlsOverlay();
        m_scanCheckTimer->start();
        FileManager::instance().startScanning();
        
        // Show main window
        show();
    }
}

void MainWindow::onConnectionLost() {
    Logger::instance().logInfo(
        "MainWindow",
        QString("connectionLost received. loginCompleted=%1 pending=%2 thread=%3")
            .arg(m_loginCompleted)
            .arg(m_pendingConnectionWarning)
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()))
    );
    if (!m_loginCompleted) {
        m_pendingConnectionWarning = true;
        Logger::instance().logInfo("MainWindow", "connectionLost deferred: login not completed.");
        return;
    }
    QTimer::singleShot(0, this, [this]() {
        Logger::instance().logInfo("MainWindow", "connectionLost handler calling showWarning()");
        m_connectionWarningOverlay->raise();
        m_connectionWarningOverlay->showWarning();
        raiseWindowControlsOverlay();
    });
}

void MainWindow::onConnectionRestored() {
    m_pendingConnectionWarning = false;
    if (!AuthManager::instance().hasBlockingAccountAction()) {
        m_connectionWarningOverlay->hideWarning();
    }
}

void MainWindow::onAccountActionBlocked() {
    Logger::instance().logInfo(
        "MainWindow",
        QString("accountActionBlocked received. loginCompleted=%1 type=%2")
            .arg(m_loginCompleted)
            .arg(AuthManager::instance().getAccountActionInfo().type)
    );

    if (!m_loginCompleted) {
        return;
    }

    QTimer::singleShot(0, this, [this]() {
        m_connectionWarningOverlay->raise();
        m_connectionWarningOverlay->showAccountActionWarning(AuthManager::instance().getAccountActionInfo());
        raiseWindowControlsOverlay();
    });
}

void MainWindow::onAccountActionCleared() {
    Logger::instance().logInfo("MainWindow", "accountActionCleared received.");
    if (AuthManager::instance().isConnected()) {
        m_connectionWarningOverlay->hideWarning();
    }
}

void MainWindow::onToolProcessCrashed(const QString& toolId, const QString& error) {
    ToolProxyInterface* activeProxy = ToolManager::instance().getActiveToolProxy();
    if (!activeProxy || activeProxy->id() != toolId) {
        Logger::instance().logWarning(
            "MainWindow",
            QString("Ignoring unavailable notification for non-active tool %1").arg(toolId)
        );
        return;
    }

    Logger::instance().logError("MainWindow", QString("Tool %1 worker became unavailable: %2").arg(toolId, error));
    
    // DO NOT destroy the host - enter crash fallback state instead
    // The persistent view remains, we just show an error overlay or message
    
    LocalizationManager& loc = LocalizationManager::instance();
    CustomMessageBox::information(
        this,
        loc.getString("MainWindow", "ToolCrashedTitle"),
        loc.getString("MainWindow", "ToolCrashedMsg").arg(toolId).arg(error)
    );
    
    closeOverlay();

    if (m_activeScriptedHostController) {
        m_activeScriptedHostController->deleteLater();
        m_activeScriptedHostController = nullptr;
    }
    m_activeTool = nullptr;
    ToolManager::instance().setToolActive(false);
    
    Logger::instance().logInfo("MainWindow",
        QString("Tool %1 worker session was discarded after becoming unavailable").arg(toolId));
}
