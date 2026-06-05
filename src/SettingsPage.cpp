//-------------------------------------------------------------------------------------
// SettingsPage.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "SettingsPage.h"
#include "ConfigManager.h"
#include "GameLanguageCatalog.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include "OverlayAcrylicMaterial.h"
#include "OverlayControlStyle.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEasingCurve>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QGradient>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVariantAnimation>
#include <QVBoxLayout>

#include <functional>
#include <utility>

static QPixmap loadSvgIcon(const QString &path, bool isDark) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QPixmap();

    QString svgContent = QTextStream(&file).readAll();
    file.close();

    const QString color = isDark ? "#E9EEF8" : "#333333";
    svgContent.replace("currentColor", color);

    QPixmap pixmap;
    pixmap.loadFromData(svgContent.toUtf8(), "SVG");
    return pixmap;
}

static constexpr int kSettingsSwitchWidth = 90;
static constexpr int kSettingsSwitchHeight = 32;
static constexpr int kMonitorPreviewHeight = 250;
static constexpr int kMonitorPreviewSpacing = 20;
static constexpr int kMonitorChooserSidePadding = 30;
static constexpr int kMonitorChooserMinWidth = 600;
static constexpr int kMonitorChooserMaxWidth = 1200;
static constexpr int kMonitorPreviewLabelHeight = 34;
static constexpr int kMonitorPreviewVerticalPadding = 12;

static QList<QSize> commonDisplayResolutions() {
    return {
        QSize(1280, 720),
        QSize(1366, 768),
        QSize(1600, 900),
        QSize(1920, 1080),
        QSize(1920, 1200),
        QSize(2560, 1080),
        QSize(2560, 1440),
        QSize(2560, 1600),
        QSize(3440, 1440),
        QSize(3840, 1600),
        QSize(3840, 2160)
    };
}

static GameLanguageCatalog::LanguageSections readGameLanguageSectionsFromGamePath(const QString &gamePath) {
    const QString languagesPath = QDir(gamePath).filePath(QStringLiteral("localisation/languages.yml"));
    QFile file(languagesPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return GameLanguageCatalog::parseLanguageSections(QString::fromUtf8(file.readAll()));
}

static QScreen *screenForName(const QString &screenName) {
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen && screen->name() == screenName) {
            return screen;
        }
    }

    if (QScreen *primary = QGuiApplication::primaryScreen()) {
        return primary;
    }
    return screens.isEmpty() ? nullptr : screens.first();
}

static QList<QSize> resolutionOptionsForScreen(QScreen *screen) {
    const QSize screenSize = screen ? screen->size() : QSize(3840, 2160);
    QList<QSize> options;
    for (const QSize &resolution : commonDisplayResolutions()) {
        if (resolution.width() >= 1280
            && resolution.height() >= 720
            && resolution.width() <= 3840
            && resolution.height() <= 2160
            && resolution.width() <= screenSize.width()
            && resolution.height() <= screenSize.height()) {
            options.append(resolution);
        }
    }
    return options;
}

static QSize bestResolutionForScreen(QScreen *screen, const QSize &preferred) {
    const QList<QSize> options = resolutionOptionsForScreen(screen);
    if (options.contains(preferred)) {
        return preferred;
    }
    if (!options.isEmpty()) {
        return options.last();
    }
    return QSize(1280, 720);
}

static int monitorPreviewWidthForSize(const QSize &screenSize) {
    if (!screenSize.isValid() || screenSize.height() <= 0) {
        return qRound(kMonitorPreviewHeight * (16.0 / 9.0));
    }
    return qMax(1, qRound(kMonitorPreviewHeight * (static_cast<double>(screenSize.width()) / screenSize.height())));
}

static QImage captureScreenPreviewImage(QScreen *screen) {
    if (!screen)
        return QImage();

    const QRect geometry = screen->geometry();
    QPixmap pixmap = screen->grabWindow(0, geometry.x(), geometry.y(), geometry.width(), geometry.height());
    if (pixmap.isNull()) {
        pixmap = screen->grabWindow(0);
    }
    if (pixmap.isNull())
        return QImage();

    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(1.0);
    return image;
}

class DisplayScreenPreviewCard : public QWidget {
public:
    DisplayScreenPreviewCard(const QString &screenName, const QSize &screenSize, const QImage &previewImage, QWidget *parent = nullptr)
        : QWidget(parent),
          m_screenName(screenName),
          m_screenSize(screenSize),
          m_previewImage(previewImage),
          m_previewWidth(monitorPreviewWidthForSize(screenSize)) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover, true);
        setFixedSize(m_previewWidth, kMonitorPreviewHeight + kMonitorPreviewLabelHeight + kMonitorPreviewVerticalPadding);
    }

    int previewWidth() const {
        return m_previewWidth;
    }

    void setSelectedPreview(bool selected) {
        if (m_selected == selected)
            return;
        m_selected = selected;
        update();
    }

    void setClickedCallback(std::function<void()> callback) {
        m_clickedCallback = std::move(callback);
    }

protected:
    void enterEvent(QEnterEvent *event) override {
        QWidget::enterEvent(event);
        update();
    }

    void leaveEvent(QEvent *event) override {
        QWidget::leaveEvent(event);
        update();
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event && event->button() == Qt::LeftButton) {
            if (m_clickedCallback)
                m_clickedCallback();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        const bool isDark = ConfigManager::instance().isCurrentThemeDark();
        const QColor borderColor = m_selected
            ? (isDark ? QColor(10, 132, 255) : QColor(0, 122, 255))
            : (underMouse() ? (isDark ? QColor(255, 255, 255, 90) : QColor(60, 60, 67, 86))
                            : (isDark ? QColor(255, 255, 255, 36) : QColor(60, 60, 67, 34)));
        const QColor fillColor = isDark ? QColor(28, 28, 30, 118) : QColor(255, 255, 255, 140);
        const QColor labelColor = isDark ? QColor(245, 247, 252) : QColor(28, 31, 36);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF previewRect(
            (width() - m_previewWidth) / 2.0,
            0.0,
            m_previewWidth,
            kMonitorPreviewHeight);
        QPainterPath clipPath;
        clipPath.addRoundedRect(previewRect, 8.0, 8.0);

        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor);
        painter.drawRoundedRect(previewRect, 8.0, 8.0);
        painter.setClipPath(clipPath);
        if (!m_previewImage.isNull()) {
            QImage scaled = m_previewImage.scaled(
                previewRect.size().toSize(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
            scaled.setDevicePixelRatio(1.0);
            const QPointF topLeft(
                previewRect.left() + (previewRect.width() - scaled.width()) / 2.0,
                previewRect.top() + (previewRect.height() - scaled.height()) / 2.0);
            painter.drawImage(QRectF(topLeft, QSizeF(scaled.size())), scaled);
        } else {
            QLinearGradient gradient(previewRect.topLeft(), previewRect.bottomRight());
            gradient.setColorAt(0.0, isDark ? QColor(58, 65, 78) : QColor(220, 226, 235));
            gradient.setColorAt(1.0, isDark ? QColor(18, 22, 30) : QColor(246, 248, 252));
            painter.fillRect(previewRect, gradient);
        }
        painter.restore();

        QPen borderPen(borderColor, m_selected ? 3.0 : 1.0);
        painter.setPen(borderPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(previewRect.adjusted(0.75, 0.75, -0.75, -0.75), 8.0, 8.0);

        const QRect labelRect(0, kMonitorPreviewHeight + 8, width(), kMonitorPreviewLabelHeight - 8);
        QFont labelFont = font();
        labelFont.setPointSize(10);
        labelFont.setBold(m_selected);
        painter.setFont(labelFont);
        painter.setPen(labelColor);
        const QFontMetrics metrics(labelFont);
        const QString label = metrics.elidedText(m_screenName, Qt::ElideMiddle, labelRect.width() - 8);
        painter.drawText(labelRect, Qt::AlignHCenter | Qt::AlignTop, label);
    }

private:
    QString m_screenName;
    QSize m_screenSize;
    QImage m_previewImage;
    std::function<void()> m_clickedCallback;
    int m_previewWidth = 0;
    bool m_selected = false;
};

class DisplayScreenChooserOverlay : public QWidget {
public:
    explicit DisplayScreenChooserOverlay(QWidget *parent = nullptr)
        : QWidget(parent) {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setFocusPolicy(Qt::StrongFocus);
        hide();

        OverlayAcrylicMaterial::installLiveRefresh(this);

        m_panel = new OverlayAcrylicPanel(this);
        m_panel->setObjectName("DisplayScreenChooserPanel");

        auto *panelLayout = new QVBoxLayout(m_panel);
        panelLayout->setContentsMargins(kMonitorChooserSidePadding, 25, kMonitorChooserSidePadding, 25);
        panelLayout->setSpacing(18);

        m_scrollArea = new QScrollArea(m_panel);
        m_scrollArea->setObjectName("DisplayScreenChooserScroll");
        m_scrollArea->setWidgetResizable(false);
        m_scrollArea->setFrameShape(QFrame::NoFrame);
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        m_cardsWidget = new QWidget(m_scrollArea);
        m_cardsWidget->setObjectName("DisplayScreenChooserCards");
        m_cardsLayout = new QHBoxLayout(m_cardsWidget);
        m_cardsLayout->setContentsMargins(0, 0, 0, 0);
        m_cardsLayout->setSpacing(kMonitorPreviewSpacing);
        m_cardsLayout->setAlignment(Qt::AlignCenter);
        m_scrollArea->setWidget(m_cardsWidget);

        panelLayout->addWidget(m_scrollArea);

        auto *buttonRow = new QWidget(m_panel);
        auto *buttonLayout = new QHBoxLayout(buttonRow);
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        buttonLayout->setSpacing(15);

        m_cancelButton = new OverlayAcrylicButton(OverlayAcrylicButton::Role::Secondary, buttonRow);
        m_cancelButton->setCursor(Qt::PointingHandCursor);
        m_cancelButton->setFixedHeight(32);
        connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
            hide();
        });

        m_confirmButton = new OverlayAcrylicButton(OverlayAcrylicButton::Role::Accent, buttonRow);
        m_confirmButton->setCursor(Qt::PointingHandCursor);
        m_confirmButton->setFixedHeight(32);
        connect(m_confirmButton, &QPushButton::clicked, this, [this]() {
            acceptSelection();
        });

        buttonLayout->addWidget(m_cancelButton, 1);
        buttonLayout->addWidget(m_confirmButton, 1);
        panelLayout->addWidget(buttonRow);

        updateTexts();
        updateTheme();

        connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) {
            if (isVisible())
                QTimer::singleShot(0, this, [this]() { rebuildPreviews(currentSelectedScreenName()); });
        });
        connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
            if (isVisible())
                QTimer::singleShot(0, this, [this]() { rebuildPreviews(currentSelectedScreenName()); });
        });
    }

    void showChooser(const QString &selectedScreenName, std::function<void(const QString &)> acceptedCallback) {
        m_acceptedCallback = std::move(acceptedCallback);
        rebuildPreviews(selectedScreenName);

        if (parentWidget()) {
            setGeometry(parentWidget()->rect());
        }
        raise();
        show();
        setFocus(Qt::ActiveWindowFocusReason);
        positionPanel();
    }

    void updateTexts() {
        LocalizationManager &loc = LocalizationManager::instance();
        if (m_cancelButton)
            m_cancelButton->setText(loc.getString("SettingsPage", "DisplayScreenChooser_Cancel"));
        if (m_confirmButton)
            m_confirmButton->setText(loc.getString("SettingsPage", "DisplayScreenChooser_Confirm"));
    }

    void updateTheme() {
        const bool isDark = ConfigManager::instance().isCurrentThemeDark();
        const QString background = isDark ? "rgba(20, 20, 22, 84)" : "rgba(255, 255, 255, 88)";
        const QString border = isDark ? "rgba(255, 255, 255, 44)" : "rgba(60, 60, 67, 38)";
        setStyleSheet(QStringLiteral(
            "QScrollArea#DisplayScreenChooserScroll, QWidget#DisplayScreenChooserCards {"
            "  background: transparent;"
            "  border: none;"
            "}"
            "QScrollBar:horizontal {"
            "  background: transparent;"
            "  height: 10px;"
            "  margin: 2px 30px 0px 30px;"
            "}"
            "QScrollBar::handle:horizontal {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 5px;"
            "  min-width: 40px;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "  width: 0px;"
            "  border: none;"
            "  background: transparent;"
            "}").arg(background, border));

        for (DisplayScreenPreviewCard *button : m_previewButtons) {
            if (button)
                button->update();
        }
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        OverlayAcrylicMaterial::paintOverlayBackdrop(
            painter,
            this,
            QRectF(rect()),
            9.0,
            ConfigManager::instance().isCurrentThemeDark(),
            132);
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        positionPanel();
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (!event) {
            QWidget::keyPressEvent(event);
            return;
        }

        if (event->key() == Qt::Key_A) {
            setSelectedIndex(m_selectedIndex - 1);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_D) {
            setSelectedIndex(m_selectedIndex + 1);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            hide();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            acceptSelection();
            event->accept();
            return;
        }

        QWidget::keyPressEvent(event);
    }

private:
    void clearPreviews() {
        while (QLayoutItem *item = m_cardsLayout->takeAt(0)) {
            if (QWidget *widget = item->widget())
                widget->deleteLater();
            delete item;
        }
        m_previewButtons.clear();
        m_screenNames.clear();
    }

    void rebuildPreviews(const QString &selectedScreenName) {
        clearPreviews();
        m_selectedScreenName = selectedScreenName;

        const QList<QScreen *> screens = QGuiApplication::screens();
        int selectedIndex = -1;
        int totalPreviewWidth = 0;

        for (int index = 0; index < screens.size(); ++index) {
            QScreen *screen = screens.at(index);
            if (!screen)
                continue;

            const QString screenName = screen->name();
            const QSize screenSize = screen->size();
            auto *button = new DisplayScreenPreviewCard(screenName, screenSize, captureScreenPreviewImage(screen), m_cardsWidget);
            const int buttonIndex = m_previewButtons.size();
            button->setClickedCallback([this, buttonIndex]() {
                setSelectedIndex(buttonIndex);
            });

            m_previewButtons.append(button);
            m_screenNames.append(screenName);
            m_cardsLayout->addWidget(button, 0, Qt::AlignVCenter);

            totalPreviewWidth += button->previewWidth();
            if (screenName == selectedScreenName)
                selectedIndex = buttonIndex;
        }

        if (selectedIndex < 0 && !m_screenNames.isEmpty()) {
            selectedIndex = 0;
            if (QScreen *primary = QGuiApplication::primaryScreen()) {
                const int primaryIndex = m_screenNames.indexOf(primary->name());
                if (primaryIndex >= 0)
                    selectedIndex = primaryIndex;
            }
        }

        const int previewCount = m_previewButtons.size();
        const int cardsWidth = totalPreviewWidth + qMax(0, previewCount - 1) * kMonitorPreviewSpacing;
        m_contentWidth = kMonitorChooserSidePadding * 2 + cardsWidth;
        const int panelWidth = qMin(kMonitorChooserMaxWidth, qMax(kMonitorChooserMinWidth, m_contentWidth));
        const int viewportWidth = qMax(1, panelWidth - kMonitorChooserSidePadding * 2);
        const int cardAreaWidth = qMax(viewportWidth, cardsWidth);
        const bool needsHorizontalScroll = m_contentWidth > kMonitorChooserMaxWidth;
        const int cardAreaHeight = kMonitorPreviewHeight + kMonitorPreviewLabelHeight + kMonitorPreviewVerticalPadding;

        m_cardsWidget->setFixedSize(cardAreaWidth, cardAreaHeight);
        m_scrollArea->setFixedHeight(cardAreaHeight + (needsHorizontalScroll ? 14 : 0));
        m_scrollArea->setHorizontalScrollBarPolicy(needsHorizontalScroll ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);

        m_panel->setFixedWidth(panelWidth);
        m_panel->adjustSize();
        m_panel->setFixedHeight(m_panel->sizeHint().height());

        setSelectedIndex(selectedIndex);
        positionPanel();
    }

    void setSelectedIndex(int index) {
        if (m_previewButtons.isEmpty())
            return;

        if (index < 0)
            index = m_previewButtons.size() - 1;
        if (index >= m_previewButtons.size())
            index = 0;

        m_selectedIndex = index;
        m_selectedScreenName = m_screenNames.value(m_selectedIndex);
        for (int i = 0; i < m_previewButtons.size(); ++i) {
            m_previewButtons.at(i)->setSelectedPreview(i == m_selectedIndex);
        }

        if (m_scrollArea && m_previewButtons.value(m_selectedIndex)) {
            m_scrollArea->ensureWidgetVisible(m_previewButtons.at(m_selectedIndex), kMonitorChooserSidePadding, 0);
        }
    }

    void acceptSelection() {
        if (m_selectedIndex >= 0 && m_selectedIndex < m_screenNames.size() && m_acceptedCallback) {
            m_acceptedCallback(m_screenNames.at(m_selectedIndex));
        }
        hide();
    }

    void positionPanel() {
        if (!m_panel)
            return;

        m_panel->move(
            qMax(0, (width() - m_panel->width()) / 2),
            qMax(0, (height() - m_panel->height()) / 2));
    }

    QString currentSelectedScreenName() const {
        if (!m_selectedScreenName.isEmpty())
            return m_selectedScreenName;
        if (m_selectedIndex >= 0 && m_selectedIndex < m_screenNames.size())
            return m_screenNames.at(m_selectedIndex);
        return ConfigManager::instance().getDisplayScreenName();
    }

    OverlayAcrylicPanel *m_panel = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_cardsWidget = nullptr;
    QHBoxLayout *m_cardsLayout = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_confirmButton = nullptr;
    QList<DisplayScreenPreviewCard *> m_previewButtons;
    QStringList m_screenNames;
    QString m_selectedScreenName;
    std::function<void(const QString &)> m_acceptedCallback;
    int m_selectedIndex = 0;
    int m_contentWidth = kMonitorChooserMinWidth;
};

class AppleSwitch : public QCheckBox {
public:
    explicit AppleSwitch(QWidget *parent = nullptr)
        : QCheckBox(parent), m_position(0.0), m_isDarkMode(false) {
        setText(QString());
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setProperty("settingsPillSwitch", true);
        setFixedSize(kSettingsSwitchWidth, kSettingsSwitchHeight);

        m_animation.setDuration(160);
        m_animation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_position = value.toReal();
            update();
        });
        connect(this, &QCheckBox::toggled, this, [this](bool checked) {
            animateTo(checked ? 1.0 : 0.0);
        });
    }

    QSize sizeHint() const override {
        return QSize(kSettingsSwitchWidth, kSettingsSwitchHeight);
    }

    void setDarkMode(bool isDarkMode) {
        if (m_isDarkMode == isDarkMode)
            return;
        m_isDarkMode = isDarkMode;
        update();
    }

protected:
    void showEvent(QShowEvent *event) override {
        QCheckBox::showEvent(event);
        m_position = isChecked() ? 1.0 : 0.0;
        update();
    }

    bool hitButton(const QPoint &pos) const override {
        return rect().contains(pos);
    }

    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF trackRect = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
        const qreal trackRadius = trackRect.height() / 2.0;
        const QColor accent(52, 199, 89);
        const QColor offColor = m_isDarkMode ? QColor(72, 72, 74) : QColor(229, 229, 234);
        const QColor borderColor = m_isDarkMode ? QColor(255, 255, 255, 36) : QColor(60, 60, 67, 46);
        const QColor trackColor = isChecked() ? accent : offColor;

        painter.setPen(QPen(isChecked() ? accent : borderColor, 1.0));
        painter.setBrush(trackColor);
        painter.drawRoundedRect(trackRect, trackRadius, trackRadius);

        const qreal knobSize = trackRect.height() - 8.0;
        const qreal knobMargin = 4.0;
        const qreal minX = trackRect.left() + knobMargin;
        const qreal maxX = trackRect.right() - knobMargin - knobSize;
        const qreal knobX = minX + (maxX - minX) * m_position;
        const QRectF knobRect(knobX, trackRect.top() + knobMargin, knobSize, knobSize);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, m_isDarkMode ? 70 : 36));
        painter.drawEllipse(knobRect.translated(0.0, 1.4));
        painter.setBrush(m_isDarkMode ? QColor(245, 245, 247) : QColor(255, 255, 255));
        painter.drawEllipse(knobRect);
    }

private:
    void animateTo(qreal target) {
        if (qFuzzyCompare(m_position, target))
            return;

        m_animation.stop();
        m_animation.setStartValue(m_position);
        m_animation.setEndValue(target);
        m_animation.start();
    }

    QVariantAnimation m_animation;
    qreal m_position;
    bool m_isDarkMode;
};

class ThemeModeButton : public QPushButton {
    Q_OBJECT

public:
    explicit ThemeModeButton(QWidget *parent = nullptr)
        : QPushButton(parent) {
        setText(QString());
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedSize(kSettingsSwitchWidth, kSettingsSwitchHeight);
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover, true);

        m_positionAnimation.setDuration(230);
        m_positionAnimation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_positionAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_position = value.toReal();
            update();
        });

        m_hoverAnimation.setDuration(160);
        m_hoverAnimation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_hoverProgress = value.toReal();
            update();
        });
    }

    QSize sizeHint() const override {
        return QSize(kSettingsSwitchWidth, kSettingsSwitchHeight);
    }

    int themeIndex() const {
        return m_themeIndex;
    }

    void setThemeNames(const QString &systemName, const QString &lightName, const QString &darkName) {
        m_systemName = systemName;
        m_lightName = lightName;
        m_darkName = darkName;
        updateAccessibleText();
    }

    void setThemeIndex(int index, bool animated = false) {
        const int normalized = qBound(
            static_cast<int>(ConfigManager::Theme::System),
            index,
            static_cast<int>(ConfigManager::Theme::Dark));
        const qreal nextPosition = positionForThemeIndex(normalized);
        const bool targetChanged = !qFuzzyCompare(m_targetPosition + 1.0, nextPosition + 1.0);

        m_themeIndex = normalized;
        m_targetPosition = nextPosition;
        updateAccessibleText();

        if (!targetChanged && qFuzzyCompare(m_position + 1.0, nextPosition + 1.0)) {
            update();
            return;
        }

        if (animated) {
            animatePosition(nextPosition);
        } else {
            m_positionAnimation.stop();
            m_position = nextPosition;
            update();
        }
    }

    void setDarkMode(bool isDarkMode) {
        if (m_isDarkMode == isDarkMode)
            return;
        m_isDarkMode = isDarkMode;
        update();
    }

signals:
    void themeIndexChanged(int index);

protected:
    void enterEvent(QEnterEvent *event) override {
        QPushButton::enterEvent(event);
        animateHover(1.0);
    }

    void leaveEvent(QEvent *event) override {
        QPushButton::leaveEvent(event);
        animateHover(0.0);
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (!event)
            return;

        int nextIndex = -1;
        if (event->button() == Qt::LeftButton) {
            nextIndex = nextLeftClickIndex();
        } else if (event->button() == Qt::RightButton) {
            nextIndex = nextRightClickIndex();
        }

        if (nextIndex >= static_cast<int>(ConfigManager::Theme::System) && nextIndex <= static_cast<int>(ConfigManager::Theme::Dark)) {
            event->accept();
            if (nextIndex != m_themeIndex) {
                emit themeIndexChanged(nextIndex);
            }
            return;
        }

        QPushButton::mousePressEvent(event);
    }

    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF trackRect = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
        const qreal radius = trackRect.height() / 2.0;
        const qreal p = qBound<qreal>(0.0, m_position, 1.0);

        QLinearGradient sky(trackRect.topLeft(), trackRect.bottomRight());
        sky.setColorAt(0.0, mixColor(QColor(96, 205, 255), QColor(24, 32, 58), p));
        sky.setColorAt(0.58, mixColor(QColor(64, 165, 245), QColor(34, 38, 78), p));
        sky.setColorAt(1.0, mixColor(QColor(33, 139, 225), QColor(47, 35, 84), p));

        QPainterPath trackPath;
        trackPath.addRoundedRect(trackRect, radius, radius);

        painter.setPen(Qt::NoPen);
        painter.setBrush(sky);
        painter.drawPath(trackPath);

        painter.save();
        painter.setClipPath(trackPath);
        drawClouds(painter, trackRect, 1.0 - p);
        drawStars(painter, trackRect, p);
        painter.restore();

        const QColor borderColor = m_isDarkMode
            ? QColor(255, 255, 255, 32 + static_cast<int>(34 * m_hoverProgress))
            : QColor(255, 255, 255, 118 + static_cast<int>(36 * m_hoverProgress));
        painter.setPen(QPen(borderColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(trackPath);

        const qreal knobSize = trackRect.height() - 8.0;
        const qreal knobMargin = 4.0;
        const qreal minX = trackRect.left() + knobMargin;
        const qreal maxX = trackRect.right() - knobMargin - knobSize;
        const QPointF knobTopLeft(minX + (maxX - minX) * p, trackRect.top() + knobMargin);
        const QRectF knobRect(knobTopLeft, QSizeF(knobSize, knobSize));
        const QPointF knobCenter = knobRect.center();

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, m_isDarkMode ? 72 : 36));
        painter.drawEllipse(knobRect.translated(0.0, 1.4));

        QRadialGradient knobFill(knobCenter - QPointF(knobSize * 0.18, knobSize * 0.2), knobSize * 0.78);
        if (isSystemTheme()) {
            knobFill.setColorAt(0.0, m_isDarkMode ? QColor(248, 250, 255) : QColor(255, 255, 255));
            knobFill.setColorAt(1.0, m_isDarkMode ? QColor(182, 192, 214) : QColor(232, 238, 246));
        } else {
            knobFill.setColorAt(0.0, mixColor(QColor(255, 229, 113), QColor(236, 242, 255), p));
            knobFill.setColorAt(1.0, mixColor(QColor(255, 181, 65), QColor(176, 192, 226), p));
        }

        painter.setBrush(knobFill);
        painter.drawEllipse(knobRect);

        if (isSystemTheme()) {
            drawGear(painter, knobCenter, knobSize * 0.25, m_isDarkMode ? QColor(45, 56, 82) : QColor(70, 88, 116));
        } else if (p < 0.48) {
            drawSun(painter, knobCenter, knobSize * 0.25);
        } else {
            drawMoon(painter, knobRect, p);
        }
    }

private:
    static QColor mixColor(const QColor &from, const QColor &to, qreal progress) {
        const qreal p = qBound<qreal>(0.0, progress, 1.0);
        return QColor(
            static_cast<int>(from.red() + (to.red() - from.red()) * p),
            static_cast<int>(from.green() + (to.green() - from.green()) * p),
            static_cast<int>(from.blue() + (to.blue() - from.blue()) * p),
            static_cast<int>(from.alpha() + (to.alpha() - from.alpha()) * p));
    }

    bool isSystemTheme() const {
        return m_themeIndex == static_cast<int>(ConfigManager::Theme::System);
    }

    qreal positionForThemeIndex(int index) const {
        if (index == static_cast<int>(ConfigManager::Theme::Dark))
            return 1.0;
        if (index == static_cast<int>(ConfigManager::Theme::System))
            return ConfigManager::instance().isSystemDarkTheme() ? 1.0 : 0.0;
        return 0.0;
    }

    int nextLeftClickIndex() const {
        if (isSystemTheme())
            return -1;
        return m_themeIndex == static_cast<int>(ConfigManager::Theme::Dark)
            ? static_cast<int>(ConfigManager::Theme::Light)
            : static_cast<int>(ConfigManager::Theme::Dark);
    }

    int nextRightClickIndex() const {
        if (isSystemTheme()) {
            return ConfigManager::instance().isSystemDarkTheme()
                ? static_cast<int>(ConfigManager::Theme::Dark)
                : static_cast<int>(ConfigManager::Theme::Light);
        }
        return static_cast<int>(ConfigManager::Theme::System);
    }

    void animatePosition(qreal target) {
        m_positionAnimation.stop();
        m_positionAnimation.setStartValue(m_position);
        m_positionAnimation.setEndValue(target);
        m_positionAnimation.start();
    }

    void animateHover(qreal target) {
        m_hoverAnimation.stop();
        m_hoverAnimation.setStartValue(m_hoverProgress);
        m_hoverAnimation.setEndValue(target);
        m_hoverAnimation.start();
    }

    void updateAccessibleText() {
        QString currentName;
        if (m_themeIndex == static_cast<int>(ConfigManager::Theme::System)) {
            currentName = m_systemName;
        } else if (m_themeIndex == static_cast<int>(ConfigManager::Theme::Dark)) {
            currentName = m_darkName;
        } else {
            currentName = m_lightName;
        }

        if (!currentName.isEmpty()) {
            setAccessibleName(currentName);
            setToolTip(currentName);
        }
    }

    void drawClouds(QPainter &painter, const QRectF &trackRect, qreal opacity) const {
        if (opacity <= 0.01)
            return;

        painter.save();
        painter.setOpacity(qBound<qreal>(0.0, opacity, 1.0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 88));

        const qreal lowY = trackRect.bottom() - 4.8;
        painter.drawEllipse(QRectF(trackRect.left() + 1.0, lowY - 5.2, 10.0, 10.0));
        painter.drawEllipse(QRectF(trackRect.left() + 8.0, lowY - 8.4, 15.0, 15.0));
        painter.drawEllipse(QRectF(trackRect.left() + 22.0, lowY - 4.8, 11.0, 11.0));
        painter.drawRoundedRect(QRectF(trackRect.left() - 1.5, lowY + 0.7, 37.0, 6.2), 3.1, 3.1);

        painter.setBrush(QColor(255, 255, 255, 216));

        const qreal y = trackRect.bottom() - 7.0;
        painter.drawEllipse(QRectF(trackRect.left() + 10.0, y - 7.0, 13.0, 13.0));
        painter.drawEllipse(QRectF(trackRect.left() + 20.0, y - 10.0, 18.0, 18.0));
        painter.drawEllipse(QRectF(trackRect.left() + 34.0, y - 6.5, 14.0, 14.0));
        painter.drawRoundedRect(QRectF(trackRect.left() + 9.0, y, 43.0, 8.5), 4.2, 4.2);

        painter.setBrush(QColor(255, 255, 255, 168));
        const qreal midY = trackRect.top() + 17.0;
        painter.drawEllipse(QRectF(trackRect.left() + 47.5, midY - 5.5, 10.5, 10.5));
        painter.drawEllipse(QRectF(trackRect.left() + 55.5, midY - 8.0, 14.0, 14.0));
        painter.drawEllipse(QRectF(trackRect.left() + 67.0, midY - 4.5, 9.5, 9.5));
        painter.drawRoundedRect(QRectF(trackRect.left() + 47.0, midY + 1.0, 30.5, 5.6), 2.8, 2.8);

        painter.setOpacity(qBound<qreal>(0.0, opacity * 0.58, 1.0));
        painter.setBrush(QColor(255, 255, 255, 190));
        painter.drawEllipse(QRectF(trackRect.right() - 24.0, trackRect.top() + 7.0, 10.0, 10.0));
        painter.drawEllipse(QRectF(trackRect.right() - 18.0, trackRect.top() + 5.5, 13.0, 13.0));
        painter.drawEllipse(QRectF(trackRect.right() - 7.0, trackRect.top() + 9.0, 7.5, 7.5));
        painter.drawRoundedRect(QRectF(trackRect.right() - 25.0, trackRect.top() + 12.5, 25.0, 6.5), 3.2, 3.2);
        painter.restore();
    }

    void drawStars(QPainter &painter, const QRectF &trackRect, qreal opacity) const {
        if (opacity <= 0.01)
            return;

        painter.save();
        painter.setOpacity(qBound<qreal>(0.0, opacity, 1.0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 228));

        const QPointF points[] = {
            QPointF(trackRect.left() + 17.0, trackRect.top() + 8.0),
            QPointF(trackRect.left() + 30.0, trackRect.top() + 19.0),
            QPointF(trackRect.left() + 45.0, trackRect.top() + 10.0),
            QPointF(trackRect.left() + 57.0, trackRect.top() + 22.0)
        };

        for (const QPointF &point : points) {
            painter.drawEllipse(point, 1.35, 1.35);
        }

        painter.setOpacity(qBound<qreal>(0.0, opacity * 0.72, 1.0));
        painter.drawEllipse(QPointF(trackRect.left() + 23.0, trackRect.top() + 24.0), 0.9, 0.9);
        painter.drawEllipse(QPointF(trackRect.left() + 51.0, trackRect.top() + 6.0), 0.9, 0.9);
        painter.restore();
    }

    void drawSun(QPainter &painter, const QPointF &center, qreal radius) const {
        painter.save();
        painter.translate(center);
        painter.setPen(QPen(QColor(255, 248, 178, 214), 1.2, Qt::SolidLine, Qt::RoundCap));
        for (int i = 0; i < 8; ++i) {
            painter.save();
            painter.rotate(45.0 * i);
            painter.drawLine(QPointF(0.0, -radius - 3.0), QPointF(0.0, -radius - 0.8));
            painter.restore();
        }
        painter.restore();
    }

    void drawMoon(QPainter &painter, const QRectF &knobRect, qreal progress) const {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(mixColor(QColor(255, 220, 106), QColor(117, 132, 168), qBound<qreal>(0.0, progress, 1.0)));
        painter.drawEllipse(QRectF(knobRect.left() + knobRect.width() * 0.42, knobRect.top() + knobRect.height() * 0.25, 3.2, 3.2));
        painter.drawEllipse(QRectF(knobRect.left() + knobRect.width() * 0.28, knobRect.top() + knobRect.height() * 0.56, 2.4, 2.4));
        painter.restore();
    }

    void drawGear(QPainter &painter, const QPointF &center, qreal radius, const QColor &color) const {
        painter.save();
        painter.translate(center);
        painter.setPen(QPen(color, 1.35, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        for (int i = 0; i < 8; ++i) {
            painter.save();
            painter.rotate(45.0 * i);
            painter.drawLine(QPointF(0.0, -radius - 2.6), QPointF(0.0, -radius - 0.4));
            painter.restore();
        }
        painter.drawEllipse(QPointF(0.0, 0.0), radius, radius);
        painter.drawEllipse(QPointF(0.0, 0.0), radius * 0.38, radius * 0.38);
        painter.restore();
    }

    QVariantAnimation m_positionAnimation;
    QVariantAnimation m_hoverAnimation;
    qreal m_position = 0.0;
    qreal m_targetPosition = 0.0;
    qreal m_hoverProgress = 0.0;
    int m_themeIndex = static_cast<int>(ConfigManager::Theme::System);
    bool m_isDarkMode = false;
    QString m_systemName;
    QString m_lightName;
    QString m_darkName;
};

static bool isSettingsCustomPillControl(QWidget *control) {
    return dynamic_cast<AppleSwitch *>(control) != nullptr
        || dynamic_cast<ThemeModeButton *>(control) != nullptr;
}

static void polishSettingRowControl(QWidget *control) {
    if (!control)
        return;

    if (isSettingsCustomPillControl(control)) {
        control->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        control->setFixedSize(kSettingsSwitchWidth, kSettingsSwitchHeight);
        return;
    }

    OverlayControlStyle::polishFormControl(control);
}

class MarqueeLabel : public QWidget {
public:
    explicit MarqueeLabel(const QString &text = QString(), QWidget *parent = nullptr)
        : QWidget(parent), m_text(text), m_offset(0.0), m_paused(false), m_overflowing(false) {
        setAttribute(Qt::WA_TranslucentBackground);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(22);

        m_timer.setInterval(24);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            if (!m_overflowing || m_paused)
                return;

            const QFontMetrics fm(font());
            const qreal textWidth = fm.horizontalAdvance(m_text);
            const qreal gap = 34.0;
            const qreal cycleWidth = textWidth + gap;
            if (cycleWidth <= 0.0)
                return;

            m_offset += 0.65;
            if (m_offset >= cycleWidth)
                m_offset = 0.0;

            update();
        });
    }

    void setText(const QString &text) {
        if (m_text == text)
            return;
        m_text = text;
        m_offset = 0.0;
        updateAnimationState();
    }

    void setPaused(bool paused) {
        m_paused = paused;
    }

protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        updateAnimationState();
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        updateAnimationState();
    }

    void changeEvent(QEvent *event) override {
        QWidget::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange)
            updateAnimationState();
    }

    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        const QRect drawRect = rect().adjusted(0, 0, -1, -1);
        const QFontMetrics fm(font());
        const qreal textWidth = fm.horizontalAdvance(m_text);
        const int baseline = (height() + fm.ascent() - fm.descent()) / 2;

        QPixmap textLayer(size());
        textLayer.fill(Qt::transparent);

        {
            QPainter textPainter(&textLayer);
            textPainter.setRenderHint(QPainter::TextAntialiasing, true);
            textPainter.setPen(palette().color(QPalette::ButtonText));

            if (!m_overflowing || textWidth <= drawRect.width()) {
                textPainter.drawText(drawRect, Qt::AlignCenter, m_text);
            } else {
                const qreal gap = 34.0;
                const qreal startX = 12.0 - m_offset;
                textPainter.drawText(QPointF(startX, baseline), m_text);
                textPainter.drawText(QPointF(startX + textWidth + gap, baseline), m_text);
            }
        }

        if (m_overflowing) {
            QPainter maskPainter(&textLayer);
            maskPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);

            QLinearGradient fade(0, 0, width(), 0);
            fade.setColorAt(0.0, QColor(0, 0, 0, 0));
            fade.setColorAt(0.06, QColor(0, 0, 0, 185));
            fade.setColorAt(0.14, QColor(0, 0, 0, 255));
            fade.setColorAt(0.86, QColor(0, 0, 0, 255));
            fade.setColorAt(0.94, QColor(0, 0, 0, 185));
            fade.setColorAt(1.0, QColor(0, 0, 0, 0));
            maskPainter.fillRect(rect(), fade);
        }

        painter.drawPixmap(0, 0, textLayer);
    }

private:
    void updateAnimationState() {
        const QFontMetrics fm(font());
        const int textWidth = fm.horizontalAdvance(m_text);
        m_overflowing = isVisible() && textWidth > qMax(0, width() - 24);

        if (m_overflowing) {
            if (!m_timer.isActive())
                m_timer.start();
        } else {
            m_timer.stop();
            m_offset = 0.0;
        }

        update();
    }

    QString m_text;
    QTimer m_timer;
    qreal m_offset;
    bool m_paused;
    bool m_overflowing;
};

class OpenSourceCardButton : public QPushButton {
public:
    explicit OpenSourceCardButton(const QString &name, const QString &license, QWidget *parent = nullptr)
        : QPushButton(parent), m_hoverProgress(0.0), m_isDarkMode(false) {
        setCursor(Qt::PointingHandCursor);
        setFlat(true);
        setCheckable(false);
        setMinimumSize(124, 56);
        setMaximumWidth(152);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 6, 10, 6);
        layout->setSpacing(0);

        m_nameLabel = new MarqueeLabel(name, this);
        QFont titleFont = font();
        titleFont.setBold(true);
        titleFont.setPointSizeF(titleFont.pointSizeF() - 0.1);
        m_nameLabel->setFont(titleFont);

        m_licenseLabel = new QLabel(license, this);
        m_licenseLabel->setAlignment(Qt::AlignCenter);

        layout->addWidget(m_nameLabel);
        layout->addWidget(m_licenseLabel);

        m_hoverAnimation = new QVariantAnimation(this);
        m_hoverAnimation->setDuration(170);
        m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_hoverProgress = value.toDouble();
            update();
        });

        setDarkMode(false);
    }

    void setDarkMode(bool isDarkMode) {
        m_isDarkMode = isDarkMode;
        m_nameLabel->setStyleSheet(QString("background: transparent; color: %1;").arg(isDarkMode ? "#F7FAFF" : "#13161B"));
        m_licenseLabel->setStyleSheet(QString("background: transparent; font-size: 8px; color: %1;")
                                          .arg(isDarkMode ? "rgba(224, 232, 245, 150)" : "rgba(59, 72, 89, 116)"));
        update();
    }

protected:
    void enterEvent(QEnterEvent *event) override {
        QPushButton::enterEvent(event);
        m_nameLabel->setPaused(true);
        animateHover(1.0);
    }

    void leaveEvent(QEvent *event) override {
        QPushButton::leaveEvent(event);
        m_nameLabel->setPaused(false);
        animateHover(0.0);
    }

    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF cardRect = rect().adjusted(1, 1, -1, -1).translated(0, -1.0 * m_hoverProgress);
        const qreal radius = 13.0;

        const QColor shadowColor = m_isDarkMode
            ? QColor(0, 0, 0, 18 + static_cast<int>(10 * m_hoverProgress))
            : QColor(84, 106, 136, 4 + static_cast<int>(7 * m_hoverProgress));
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadowColor);
        painter.drawRoundedRect(cardRect.translated(0, 4), radius, radius);

        QLinearGradient fill(cardRect.topLeft(), cardRect.bottomLeft());
        if (m_isDarkMode) {
            fill.setColorAt(0.0, QColor(255, 255, 255, 8 + static_cast<int>(6 * m_hoverProgress)));
            fill.setColorAt(0.22, QColor(52, 60, 73, 118 + static_cast<int>(8 * m_hoverProgress)));
            fill.setColorAt(1.0, QColor(37, 43, 54, 126 + static_cast<int>(10 * m_hoverProgress)));
        } else {
            fill.setColorAt(0.0, QColor(255, 255, 255, 144 + static_cast<int>(14 * m_hoverProgress)));
            fill.setColorAt(0.28, QColor(255, 255, 255, 104 + static_cast<int>(14 * m_hoverProgress)));
            fill.setColorAt(1.0, QColor(239, 244, 250, 82 + static_cast<int>(12 * m_hoverProgress)));
        }

        const QColor borderColor = m_isDarkMode
            ? QColor(255, 255, 255, 18 + static_cast<int>(9 * m_hoverProgress))
            : QColor(255, 255, 255, 112 + static_cast<int>(18 * m_hoverProgress));

        painter.setBrush(fill);
        painter.setPen(QPen(borderColor, 1.0));
        painter.drawRoundedRect(cardRect, radius, radius);

        QRectF highlightRect = cardRect.adjusted(1.0, 1.0, -1.0, -cardRect.height() * 0.52);
        QLinearGradient highlight(highlightRect.topLeft(), highlightRect.bottomLeft());
        highlight.setColorAt(0.0, m_isDarkMode ? QColor(255, 255, 255, 15 + static_cast<int>(8 * m_hoverProgress))
                                               : QColor(255, 255, 255, 72 + static_cast<int>(18 * m_hoverProgress)));
        highlight.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(highlight);
        painter.drawRoundedRect(highlightRect, radius - 2.0, radius - 2.0);
    }

private:
    void animateHover(qreal endValue) {
        m_hoverAnimation->stop();
        m_hoverAnimation->setStartValue(m_hoverProgress);
        m_hoverAnimation->setEndValue(endValue);
        m_hoverAnimation->start();
    }

    MarqueeLabel *m_nameLabel;
    QLabel *m_licenseLabel;
    QVariantAnimation *m_hoverAnimation;
    qreal m_hoverProgress;
    bool m_isDarkMode;
};

class OpenSourceCardsWidget : public QWidget {
public:
    explicit OpenSourceCardsWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }

    void addCard(QWidget *card) {
        if (!card)
            return;
        card->setParent(this);
        m_cards.append(card);
        updateGeometry();
    }

    const QList<QWidget *> &cards() const {
        return m_cards;
    }

    QSize sizeHint() const override {
        const int width = QWidget::width() > 0 ? QWidget::width() : 720;
        return calculateLayout(width, false);
    }

    QSize minimumSizeHint() const override {
        return QSize(0, calculateLayout(qMax(320, QWidget::width()), false).height());
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        calculateLayout(event->size().width(), true);
    }

private:
    QSize calculateLayout(int availableWidth, bool apply) const {
        const int outerMargin = 2;
        const int hSpacing = 8;
        const int vSpacing = 8;
        const int cardMinWidth = 124;
        const int cardMaxWidth = 152;
        const int cardHeight = 56;

        const int safeWidth = qMax(availableWidth, cardMinWidth + outerMargin * 2);
        const int effectiveWidth = qMax(safeWidth - outerMargin * 2, cardMinWidth);

        int columns = qMax(1, (effectiveWidth + hSpacing) / (cardMinWidth + hSpacing));
        int cardWidth = (effectiveWidth - (columns - 1) * hSpacing) / columns;
        cardWidth = qBound(cardMinWidth, cardWidth, cardMaxWidth);

        while (columns > 1) {
            const int requiredWidth = columns * cardWidth + (columns - 1) * hSpacing;
            if (requiredWidth <= effectiveWidth)
                break;
            --columns;
            cardWidth = (effectiveWidth - (columns - 1) * hSpacing) / columns;
            cardWidth = qBound(cardMinWidth, cardWidth, cardMaxWidth);
        }

        int y = outerMargin;
        int index = 0;

        while (index < m_cards.size()) {
            const int itemsThisRow = qMin(columns, m_cards.size() - index);
            const int rowWidth = itemsThisRow * cardWidth + (itemsThisRow - 1) * hSpacing;
            const int startX = outerMargin + qMax(0, (effectiveWidth - rowWidth) / 2);

            for (int i = 0; i < itemsThisRow; ++i) {
                QWidget *card = m_cards.at(index + i);
                if (apply)
                    card->setGeometry(startX + i * (cardWidth + hSpacing), y, cardWidth, cardHeight);
            }

            index += itemsThisRow;
            y += cardHeight + vSpacing;
        }

        const int totalHeight = qMax(cardHeight + outerMargin * 2, y - vSpacing + outerMargin);
        return QSize(safeWidth, totalHeight);
    }

    QList<QWidget *> m_cards;
};

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent),
      m_themeButton(nullptr),
      m_languageCombo(nullptr),
      m_gameLanguageCombo(nullptr),
      m_displayModeCombo(nullptr),
      m_displayScreenButton(nullptr),
      m_resolutionCombo(nullptr),
      m_displayScreenChooser(nullptr),
      m_debugCheck(nullptr),
      m_sidebarCompactCheck(nullptr),
      m_maxLogFilesSpin(nullptr),
      m_versionLabel(nullptr),
      m_openSourceArea(nullptr),
      m_openSourceViewport(nullptr),
      m_openSourceCardsWidget(nullptr),
      m_openSourceToggleBtn(nullptr),
      m_userAgreementBtn(nullptr),
      m_openLogBtn(nullptr),
      m_pinToStartBtn(nullptr),
      m_clearCacheBtn(nullptr),
      m_openSourceExpandAnimation(nullptr),
      m_openSourceOpacityAnimation(nullptr),
      m_openSourceOpacityEffect(nullptr),
      m_isOpenSourceExpanded(false) {
    setupUi();
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) {
        refreshDisplayControls();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
        refreshDisplayControls();
    });
    updateTexts();
    updateTheme();
    updateOpenSourceToggleText();
}

void SettingsPage::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QWidget *header = new QWidget();
    header->setObjectName("OverlayHeader");
    header->setFixedHeight(60);

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(20, 0, 20, 0);

    QLabel *title = new QLabel("Settings");
    title->setObjectName("SettingsTitle");
    title->setStyleSheet("font-size: 18px; font-weight: bold;");

    QPushButton *closeBtn = new QPushButton("×");
    closeBtn->setObjectName("OverlayCloseButton");
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &SettingsPage::closeClicked);

    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(closeBtn);
    layout->addWidget(header);

    auto *scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget *content = new QWidget();
    content->setObjectName("SettingsContent");

    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(40, 20, 40, 40);
    contentLayout->setSpacing(30);

    auto *interfaceLayout = new QVBoxLayout();
    interfaceLayout->setSpacing(0);

    m_themeButton = new ThemeModeButton();
    m_themeButton->setThemeIndex(static_cast<int>(ConfigManager::instance().getTheme()), false);
    connect(m_themeButton, &ThemeModeButton::themeIndexChanged, [this](int index) {
        ConfigManager::instance().setTheme(static_cast<ConfigManager::Theme>(index));
        if (m_themeButton) {
            m_themeButton->setThemeIndex(index, true);
        }
        emit themeChanged();
    });
    interfaceLayout->addWidget(createSettingRow("Theme", ":/icons/palette.svg", "Theme Mode", "Select application appearance", m_themeButton));

    m_sidebarCompactCheck = new AppleSwitch();
    m_sidebarCompactCheck->setObjectName("OverlaySwitch");
    m_sidebarCompactCheck->setChecked(ConfigManager::instance().getSidebarCompactMode());
    connect(m_sidebarCompactCheck, &QCheckBox::toggled, [this](bool checked) {
        ConfigManager::instance().setSidebarCompactMode(checked);
        emit sidebarCompactChanged(checked);
    });
    interfaceLayout->addWidget(createSettingRow("Sidebar", ":/icons/sidebar.svg", "Compact Sidebar", "Auto-collapse sidebar", m_sidebarCompactCheck));

    m_displayModeCombo = new QComboBox();
    connect(m_displayModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (index < 0 || !m_displayModeCombo)
            return;

        const QVariant data = m_displayModeCombo->itemData(index);
        if (!data.isValid())
            return;

        ConfigManager::instance().setDisplayMode(static_cast<ConfigManager::DisplayMode>(data.toInt()));
        emit displaySettingsChanged();
    });
    interfaceLayout->addWidget(createSettingRow("DisplayMode", ":/icons/eye.svg", "Display Mode", "Choose window or fullscreen mode", m_displayModeCombo));

    m_displayScreenButton = new QPushButton();
    m_displayScreenButton->setObjectName("DisplayScreenButton");
    m_displayScreenButton->setCursor(Qt::PointingHandCursor);
    connect(m_displayScreenButton, &QPushButton::clicked, this, &SettingsPage::showDisplayScreenChooser);
    interfaceLayout->addWidget(createSettingRow("DisplayScreen", ":/icons/monitor.svg", "Monitor", "Choose the target display", m_displayScreenButton));

    m_resolutionCombo = new QComboBox();
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (index < 0 || !m_resolutionCombo)
            return;

        const QSize resolution = m_resolutionCombo->itemData(index).toSize();
        if (!resolution.isValid())
            return;

        ConfigManager::instance().setDisplayResolution(resolution);
        emit displaySettingsChanged();
    });
    interfaceLayout->addWidget(createSettingRow("Resolution", ":/icons/resolution.svg", "Resolution", "Window size for the selected display", m_resolutionCombo));
    refreshDisplayModeOptions();
    refreshDisplayControls();

    contentLayout->addWidget(createGroup("Interface", interfaceLayout));

    auto *accessibilityLayout = new QVBoxLayout();
    accessibilityLayout->setSpacing(0);

    m_languageCombo = new QComboBox();
    m_languageCombo->addItems(LocalizationManager::instance().availableLanguageDisplayNames());
    m_languageCombo->setCurrentText(
        LocalizationManager::instance().displayNameForLanguage(ConfigManager::instance().getLanguage())
    );
    connect(m_languageCombo, &QComboBox::currentTextChanged, [this](const QString &langText) {
        const QString langCode = LocalizationManager::instance().normalizeLanguageCode(langText);
        if (langCode != ConfigManager::instance().getLanguage()) {
            ConfigManager::instance().setLanguage(langCode);
            QTimer::singleShot(0, this, [this]() {
                emit languageChanged();
            });
        }
    });
    accessibilityLayout->addWidget(createSettingRow("InterfaceLanguage", ":/icons/interface-language.svg", "Interface Language", "Used to display in-app interface text", m_languageCombo));

    m_gameLanguageCombo = new QComboBox();
    connect(m_gameLanguageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (index < 0 || !m_gameLanguageCombo)
            return;

        const QString langCode = m_gameLanguageCombo->itemData(index).toString().trimmed();
        if (!langCode.isEmpty() && langCode != ConfigManager::instance().getGameLanguage()) {
            ConfigManager::instance().setGameLanguage(langCode);
            emit gameLanguageChanged();
        }
    });
    accessibilityLayout->addWidget(createSettingRow("GameLanguage", ":/icons/globe.svg", "Game Language", "Used for game localisation names", m_gameLanguageCombo));
    refreshGameLanguageOptions();

    m_pinToStartBtn = new QPushButton("Pin");
    m_pinToStartBtn->setObjectName("PinToStartBtn");
    m_pinToStartBtn->setCursor(Qt::PointingHandCursor);
    connect(m_pinToStartBtn, &QPushButton::clicked, this, &SettingsPage::createStartMenuShortcut);
    accessibilityLayout->addWidget(createSettingRow("PinToStart", ":/icons/pin.svg", "Pin to Start", "Create a shortcut in the Start menu", m_pinToStartBtn));

    m_clearCacheBtn = new QPushButton("Clear");
    m_clearCacheBtn->setObjectName("ClearCacheBtn");
    m_clearCacheBtn->setCursor(Qt::PointingHandCursor);
    connect(m_clearCacheBtn, &QPushButton::clicked, this, &SettingsPage::clearAppCache);
    accessibilityLayout->addWidget(createSettingRow("ClearCache", ":/icons/trash.svg", "Clear App Cache", "App will close automatically after clearing", m_clearCacheBtn));

    contentLayout->addWidget(createGroup("Accessibility", accessibilityLayout));

    auto *debugLayout = new QVBoxLayout();
    debugLayout->setSpacing(0);

    m_debugCheck = new AppleSwitch();
    m_debugCheck->setObjectName("OverlaySwitch");
    m_debugCheck->setChecked(ConfigManager::instance().getDebugMode());
    connect(m_debugCheck, &QCheckBox::toggled, [this](bool checked) {
        ConfigManager::instance().setDebugMode(checked);
        emit debugModeChanged(checked);
    });
    debugLayout->addWidget(createSettingRow("Debug", ":/icons/bug.svg", "Show Usage Overlay", "Show memory usage overlay", m_debugCheck));

    m_maxLogFilesSpin = new QSpinBox();
    m_maxLogFilesSpin->setRange(1, 100);
    m_maxLogFilesSpin->setValue(ConfigManager::instance().getMaxLogFiles());
    connect(m_maxLogFilesSpin, QOverload<int>::of(&QSpinBox::valueChanged), [](int value) {
        ConfigManager::instance().setMaxLogFiles(value);
    });
    debugLayout->addWidget(createSettingRow("MaxLogs", ":/icons/broom.svg", "Max Log Files", "Number of log files to keep", m_maxLogFilesSpin));

    m_openLogBtn = new QPushButton("View");
    m_openLogBtn->setObjectName("OpenLogBtn");
    m_openLogBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openLogBtn, &QPushButton::clicked, this, &SettingsPage::openLogDir);
    debugLayout->addWidget(createSettingRow("Log", ":/icons/folder.svg", "Log Directory", "Open application logs", m_openLogBtn));

    contentLayout->addWidget(createGroup("Debug", debugLayout));

    auto *aboutLayout = new QVBoxLayout();
    aboutLayout->setSpacing(0);

    QWidget *aboutRow = new QWidget();
    aboutRow->setObjectName("SettingRow");

    auto *aboutRowLayout = new QVBoxLayout(aboutRow);
    aboutRowLayout->setContentsMargins(20, 20, 20, 18);
    aboutRowLayout->setSpacing(6);

    auto *infoLayout = new QHBoxLayout();
    QLabel *appName = new QLabel("APE HOI4 Tool Studio");
    appName->setStyleSheet("font-weight: bold; font-size: 16px;");
    m_versionLabel = new QLabel(QString("v%1").arg(APP_VERSION));
    infoLayout->addWidget(appName);
    infoLayout->addStretch();
    infoLayout->addWidget(m_versionLabel);

    QLabel *copyright = new QLabel("© 2026 Team APE:RIP. All rights reserved.");
    copyright->setStyleSheet("color: #888; font-size: 12px;");

    QPushButton *githubLink = new QPushButton("Official Website");
    githubLink->setObjectName("GithubLink");
    githubLink->setFlat(true);
    githubLink->setCursor(Qt::PointingHandCursor);
    connect(githubLink, &QPushButton::clicked, [this]() {
        openUrl("https://www.aperip.com/");
    });

    m_userAgreementBtn = new QPushButton("User Agreement");
    m_userAgreementBtn->setObjectName("UserAgreementBtn");
    m_userAgreementBtn->setFlat(true);
    m_userAgreementBtn->setCursor(Qt::PointingHandCursor);
    connect(m_userAgreementBtn, &QPushButton::clicked, this, &SettingsPage::showUserAgreement);

    m_openSourceToggleBtn = new QPushButton("Open Source Libraries");
    m_openSourceToggleBtn->setObjectName("OpenSourceBtn");
    m_openSourceToggleBtn->setFlat(true);
    m_openSourceToggleBtn->setCursor(Qt::PointingHandCursor);
    connect(m_openSourceToggleBtn, &QPushButton::clicked, this, &SettingsPage::toggleOpenSource);

    m_openSourceArea = new QWidget();
    m_openSourceArea->setObjectName("OpenSourceGlassPanel");
    m_openSourceArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_openSourceArea->setMinimumHeight(0);
    m_openSourceArea->setMaximumHeight(0);

    auto *openSourceAreaLayout = new QVBoxLayout(m_openSourceArea);
    openSourceAreaLayout->setContentsMargins(0, 0, 0, 0);
    openSourceAreaLayout->setSpacing(0);

    m_openSourceViewport = new QWidget();
    m_openSourceViewport->setObjectName("OpenSourceViewport");
    m_openSourceViewport->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    auto *viewportLayout = new QVBoxLayout(m_openSourceViewport);
    viewportLayout->setContentsMargins(0, 4, 0, 0);
    viewportLayout->setSpacing(0);

    m_openSourceCardsWidget = new OpenSourceCardsWidget(m_openSourceViewport);
    viewportLayout->addWidget(m_openSourceCardsWidget, 0, Qt::AlignHCenter);

    openSourceAreaLayout->addWidget(m_openSourceViewport);

    m_openSourceExpandAnimation = new QPropertyAnimation(m_openSourceArea, "maximumHeight", this);
    m_openSourceExpandAnimation->setDuration(260);
    m_openSourceExpandAnimation->setEasingCurve(QEasingCurve::OutCubic);

    m_openSourceArea->installEventFilter(this);
    m_openSourceViewport->installEventFilter(this);
    m_openSourceCardsWidget->installEventFilter(this);

    QFile osFile(":/openSource.json");
    if (osFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QByteArray data = osFile.readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (int i = 0; i < arr.size(); ++i) {
                const QJsonValue val = arr.at(i);
                if (!val.isObject())
                    continue;

                const QJsonObject obj = val.toObject();
                const QString name = obj["name"].toString();
                const QString license = obj["license"].toString();
                const QString url = obj["url"].toString();

                auto *cardBtn = new OpenSourceCardButton(name, license, m_openSourceCardsWidget);
                if (!url.isEmpty()) {
                    connect(cardBtn, &QPushButton::clicked, [this, url]() {
                        openUrl(url);
                    });
                }

                static_cast<OpenSourceCardsWidget *>(m_openSourceCardsWidget)->addCard(cardBtn);
            }
        }
        osFile.close();
    }

    aboutRowLayout->addLayout(infoLayout);
    aboutRowLayout->addWidget(copyright);
    aboutRowLayout->addWidget(githubLink);
    aboutRowLayout->addWidget(m_userAgreementBtn);
    aboutRowLayout->addWidget(m_openSourceToggleBtn);
    aboutRowLayout->addWidget(m_openSourceArea);

    aboutLayout->addWidget(aboutRow);
    contentLayout->addWidget(createGroup("About", aboutLayout));
    contentLayout->addStretch();

    scroll->setWidget(content);
    layout->addWidget(scroll);

    m_displayScreenChooser = new DisplayScreenChooserOverlay(this);
    m_displayScreenChooser->setGeometry(rect());

    QTimer::singleShot(0, this, [this]() {
        refreshOpenSourceLayout();
    });
}

QWidget *SettingsPage::createGroup(const QString &title, QLayout *contentLayout) {
    auto *group = new QGroupBox();
    group->setObjectName("SettingsGroup");

    auto *groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(0, 10, 0, 0);
    groupLayout->setSpacing(0);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setObjectName(title + "_GroupTitle");
    titleLabel->setProperty("overlayRole", "GroupTitle");

    QWidget *container = new QWidget();
    container->setObjectName("GroupContainer");
    container->setLayout(contentLayout);

    groupLayout->addWidget(titleLabel, 0, Qt::AlignLeft);
    groupLayout->addWidget(container);

    return group;
}

QWidget *SettingsPage::createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *control) {
    auto *row = new QWidget();
    row->setObjectName("SettingRow");
    row->setFixedHeight(56);

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(14, 8, 18, 8);
    layout->setSpacing(13);

    QLabel *iconLbl = new QLabel();
    iconLbl->setObjectName("SettingIcon");
    iconLbl->setFixedSize(32, 32);
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setProperty("svgIcon", icon);

    const bool isDark = ConfigManager::instance().isCurrentThemeDark();
    iconLbl->setPixmap(loadSvgIcon(icon, isDark));

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);

    QLabel *titleLbl = new QLabel(title);
    titleLbl->setObjectName(id + "_Title");
    titleLbl->setStyleSheet("font-weight: bold; font-size: 14px;");

    QLabel *descLbl = new QLabel(desc);
    descLbl->setObjectName(id + "_Desc");
    descLbl->setStyleSheet("color: #888; font-size: 12px;");

    textLayout->addWidget(titleLbl);
    textLayout->addWidget(descLbl);

    layout->addWidget(iconLbl);
    layout->addLayout(textLayout);
    layout->addStretch();
    if (control) {
        polishSettingRowControl(control);
        layout->addWidget(control);
    }

    return row;
}

bool SettingsPage::eventFilter(QObject *watched, QEvent *event) {
    if ((watched == m_openSourceViewport || watched == m_openSourceArea || watched == m_openSourceCardsWidget)
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest)) {
        QTimer::singleShot(0, this, [this]() {
            refreshOpenSourceLayout();
        });
    }

    return QWidget::eventFilter(watched, event);
}

void SettingsPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_displayScreenChooser)
        m_displayScreenChooser->setGeometry(rect());
    QTimer::singleShot(0, this, [this]() {
        refreshOpenSourceLayout();
    });
}

void SettingsPage::updateOpenSourceToggleText() {
    if (!m_openSourceToggleBtn)
        return;

    QString baseText = m_openSourceToggleBaseText.isEmpty() ? QStringLiteral("Open Source Libraries") : m_openSourceToggleBaseText;
    while (!baseText.isEmpty()) {
        const QChar lastChar = baseText.back();
        if (lastChar.isSpace() || lastChar == QChar(0x25BE) || lastChar == QChar(0x25B8) || lastChar == QChar(0x25BC)
            || lastChar == QChar(0x25B6)) {
            baseText.chop(1);
        } else {
            break;
        }
    }

    m_openSourceToggleBtn->setText(QString("%1 %2").arg(baseText, m_isOpenSourceExpanded ? QStringLiteral("▾") : QStringLiteral("▸")));
}

void SettingsPage::refreshOpenSourceLayout() {
    if (!m_openSourceCardsWidget || !m_openSourceViewport || !m_openSourceArea)
        return;

    auto *cardsWidget = static_cast<OpenSourceCardsWidget *>(m_openSourceCardsWidget);

    int availableWidth = m_openSourceArea->width();
    if (availableWidth <= 0)
        availableWidth = m_openSourceViewport->width();
    if (availableWidth <= 0)
        return;

    const int cardsWidth = qMax(120, availableWidth);
    cardsWidget->setFixedWidth(cardsWidth);
    cardsWidget->updateGeometry();

    const int cardsHeight = cardsWidget->sizeHint().height();
    cardsWidget->setFixedHeight(cardsHeight);

    const int targetHeight = cardsHeight + 4;

    if (!m_isOpenSourceExpanded) {
        if (m_openSourceExpandAnimation)
            m_openSourceExpandAnimation->stop();
        m_openSourceArea->setMinimumHeight(0);
        m_openSourceArea->setMaximumHeight(0);
        m_openSourceArea->updateGeometry();
        return;
    }

    if (m_openSourceExpandAnimation && m_openSourceExpandAnimation->state() == QAbstractAnimation::Running)
        m_openSourceExpandAnimation->stop();

    m_openSourceArea->setMinimumHeight(0);
    m_openSourceArea->setMaximumHeight(targetHeight);
    m_openSourceArea->updateGeometry();
}

void SettingsPage::refreshDisplayModeOptions() {
    if (!m_displayModeCombo)
        return;

    LocalizationManager &loc = LocalizationManager::instance();
    const int selectedMode = static_cast<int>(ConfigManager::instance().getDisplayMode());
    QSignalBlocker blocker(m_displayModeCombo);

    m_displayModeCombo->clear();
    m_displayModeCombo->addItem(
        loc.getString("SettingsPage", "DisplayMode_Window"),
        static_cast<int>(ConfigManager::DisplayMode::Window));
    m_displayModeCombo->addItem(
        loc.getString("SettingsPage", "DisplayMode_Fullscreen"),
        static_cast<int>(ConfigManager::DisplayMode::Fullscreen));

    const int index = m_displayModeCombo->findData(selectedMode);
    m_displayModeCombo->setCurrentIndex(index >= 0 ? index : 0);
}

void SettingsPage::refreshGameLanguageOptions() {
    if (!m_gameLanguageCombo)
        return;

    const GameLanguageCatalog::LanguageSections sections =
        readGameLanguageSectionsFromGamePath(ConfigManager::instance().getGamePath());
    const QList<GameLanguageCatalog::LanguageOption> options =
        GameLanguageCatalog::optionsFromEnglishSection(sections);

    QString selectedLanguage = ConfigManager::instance().getGameLanguage();
    bool selectedLanguageAvailable = false;

    QSignalBlocker blocker(m_gameLanguageCombo);
    m_gameLanguageCombo->clear();
    for (const GameLanguageCatalog::LanguageOption &option : options) {
        if (option.code == selectedLanguage) {
            selectedLanguageAvailable = true;
        }
        m_gameLanguageCombo->addItem(option.displayName, option.code);
    }

    if (!selectedLanguageAvailable && !options.isEmpty()) {
        selectedLanguage = options.first().code;
        ConfigManager::instance().setGameLanguage(selectedLanguage);
    }

    const int index = m_gameLanguageCombo->findData(selectedLanguage);
    m_gameLanguageCombo->setCurrentIndex(index >= 0 ? index : 0);
}

void SettingsPage::refreshDisplayControls() {
    const QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *selectedScreen = screenForName(ConfigManager::instance().getDisplayScreenName());
    const QString selectedName = selectedScreen ? selectedScreen->name() : QString();

    if (!selectedName.isEmpty() && ConfigManager::instance().getDisplayScreenName() != selectedName) {
        ConfigManager::instance().setDisplayScreenName(selectedName);
    } else if (selectedName.isEmpty() && !screens.isEmpty() && screens.first()) {
        ConfigManager::instance().setDisplayScreenName(screens.first()->name());
    }

    updateDisplayScreenButtonText();
    refreshResolutionOptions();
}

void SettingsPage::refreshResolutionOptions() {
    if (!m_resolutionCombo)
        return;

    QScreen *selectedScreen = screenForName(ConfigManager::instance().getDisplayScreenName());
    const QList<QSize> options = resolutionOptionsForScreen(selectedScreen);
    QSize selectedResolution = bestResolutionForScreen(selectedScreen, ConfigManager::instance().getDisplayResolution());

    if (ConfigManager::instance().getDisplayResolution() != selectedResolution) {
        ConfigManager::instance().setDisplayResolution(selectedResolution);
    }

    QSignalBlocker blocker(m_resolutionCombo);
    m_resolutionCombo->clear();
    for (const QSize &resolution : options) {
        m_resolutionCombo->addItem(
            QStringLiteral("%1 x %2").arg(resolution.width()).arg(resolution.height()),
            resolution);
    }

    int selectedIndex = m_resolutionCombo->findData(selectedResolution);
    if (selectedIndex < 0 && m_resolutionCombo->count() > 0) {
        selectedIndex = m_resolutionCombo->count() - 1;
    }
    m_resolutionCombo->setCurrentIndex(selectedIndex);
}

void SettingsPage::updateDisplayScreenButtonText() {
    if (!m_displayScreenButton)
        return;

    LocalizationManager &loc = LocalizationManager::instance();
    const QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *selectedScreen = screenForName(ConfigManager::instance().getDisplayScreenName());
    int selectedIndex = -1;

    for (int index = 0; index < screens.size(); ++index) {
        if (screens.at(index) == selectedScreen) {
            selectedIndex = index;
            break;
        }
    }

    if (!selectedScreen) {
        m_displayScreenButton->setText(loc.getString("SettingsPage", "DisplayScreen_ButtonNone"));
        m_displayScreenButton->setToolTip(QString());
        return;
    }

    const QSize size = selectedScreen->size();
    m_displayScreenButton->setText(
        loc.getString("SettingsPage", "DisplayScreen_Button").arg(selectedIndex >= 0 ? selectedIndex + 1 : 1));
    m_displayScreenButton->setToolTip(
        loc.getString("SettingsPage", "DisplayScreen_Item")
            .arg(selectedIndex >= 0 ? selectedIndex + 1 : 1)
            .arg(selectedScreen->name())
            .arg(size.width())
            .arg(size.height()));
}

void SettingsPage::showDisplayScreenChooser() {
    if (!m_displayScreenChooser)
        return;

    m_displayScreenChooser->showChooser(
        ConfigManager::instance().getDisplayScreenName(),
        [this](const QString &screenName) {
            if (screenName.isEmpty())
                return;

            ConfigManager::instance().setDisplayScreenName(screenName);
            refreshDisplayControls();
            emit displaySettingsChanged();
        });
}

void SettingsPage::syncDisplaySettingsControls() {
    refreshDisplayModeOptions();
    refreshDisplayControls();
}

void SettingsPage::updateTexts() {
    LocalizationManager &loc = LocalizationManager::instance();

    if (m_themeButton) {
        m_themeButton->setThemeNames(
            loc.getString("SettingsPage", "Theme_System"),
            loc.getString("SettingsPage", "Theme_Light"),
            loc.getString("SettingsPage", "Theme_Dark"));
        m_themeButton->setThemeIndex(static_cast<int>(ConfigManager::instance().getTheme()), false);
    }

    if (QLabel *settingsTitle = findChild<QLabel *>("SettingsTitle"))
        settingsTitle->setText(loc.getString("SettingsPage", "SettingsTitle"));
    if (QLabel *interfaceGroup = findChild<QLabel *>("Interface_GroupTitle"))
        interfaceGroup->setText(loc.getString("SettingsPage", "Group_Interface"));
    if (QLabel *accessibilityGroup = findChild<QLabel *>("Accessibility_GroupTitle"))
        accessibilityGroup->setText(loc.getString("SettingsPage", "Group_Accessibility"));
    if (QLabel *debugGroup = findChild<QLabel *>("Debug_GroupTitle"))
        debugGroup->setText(loc.getString("SettingsPage", "Group_Debug"));
    if (QLabel *aboutGroup = findChild<QLabel *>("About_GroupTitle"))
        aboutGroup->setText(loc.getString("SettingsPage", "Group_About"));

    if (QLabel *themeTitle = findChild<QLabel *>("Theme_Title"))
        themeTitle->setText(loc.getString("SettingsPage", "Theme_Title"));
    if (QLabel *themeDesc = findChild<QLabel *>("Theme_Desc"))
        themeDesc->setText(loc.getString("SettingsPage", "Theme_Desc"));

    if (QLabel *interfaceLanguageTitle = findChild<QLabel *>("InterfaceLanguage_Title"))
        interfaceLanguageTitle->setText(loc.getString("SettingsPage", "InterfaceLanguage_Title"));
    if (QLabel *interfaceLanguageDesc = findChild<QLabel *>("InterfaceLanguage_Desc"))
        interfaceLanguageDesc->setText(loc.getString("SettingsPage", "InterfaceLanguage_Desc"));
    if (m_languageCombo) {
        QSignalBlocker blocker(m_languageCombo);
        const QString currentDisplayName =
            loc.displayNameForLanguage(ConfigManager::instance().getLanguage());
        const QStringList languageDisplayNames = loc.availableLanguageDisplayNames();
        bool languageItemsChanged = m_languageCombo->count() != languageDisplayNames.size();
        if (!languageItemsChanged) {
            for (int i = 0; i < languageDisplayNames.size(); ++i) {
                if (m_languageCombo->itemText(i) != languageDisplayNames.at(i)) {
                    languageItemsChanged = true;
                    break;
                }
            }
        }
        if (languageItemsChanged) {
            m_languageCombo->clear();
            m_languageCombo->addItems(languageDisplayNames);
        }
        if (m_languageCombo->currentText() != currentDisplayName) {
            m_languageCombo->setCurrentText(currentDisplayName);
        }
    }
    if (QLabel *gameLanguageTitle = findChild<QLabel *>("GameLanguage_Title"))
        gameLanguageTitle->setText(loc.getString("SettingsPage", "GameLanguage_Title"));
    if (QLabel *gameLanguageDesc = findChild<QLabel *>("GameLanguage_Desc"))
        gameLanguageDesc->setText(loc.getString("SettingsPage", "GameLanguage_Desc"));
    refreshGameLanguageOptions();

    if (QLabel *debugTitle = findChild<QLabel *>("Debug_Title"))
        debugTitle->setText(loc.getString("SettingsPage", "Debug_Title"));
    if (QLabel *debugDesc = findChild<QLabel *>("Debug_Desc"))
        debugDesc->setText(loc.getString("SettingsPage", "Debug_Desc"));

    if (QLabel *maxLogsTitle = findChild<QLabel *>("MaxLogs_Title"))
        maxLogsTitle->setText(loc.getString("SettingsPage", "MaxLogs_Title"));
    if (QLabel *maxLogsDesc = findChild<QLabel *>("MaxLogs_Desc"))
        maxLogsDesc->setText(loc.getString("SettingsPage", "MaxLogs_Desc"));

    if (QLabel *logTitle = findChild<QLabel *>("Log_Title"))
        logTitle->setText(loc.getString("SettingsPage", "Log_Title"));
    if (QLabel *logDesc = findChild<QLabel *>("Log_Desc"))
        logDesc->setText(loc.getString("SettingsPage", "Log_Desc"));
    if (m_openLogBtn)
        m_openLogBtn->setText(loc.getString("SettingsPage", "Log_Btn"));

    if (QLabel *sidebarTitle = findChild<QLabel *>("Sidebar_Title"))
        sidebarTitle->setText(loc.getString("SettingsPage", "Sidebar_Title"));
    if (QLabel *sidebarDesc = findChild<QLabel *>("Sidebar_Desc"))
        sidebarDesc->setText(loc.getString("SettingsPage", "Sidebar_Desc"));

    if (QLabel *displayModeTitle = findChild<QLabel *>("DisplayMode_Title"))
        displayModeTitle->setText(loc.getString("SettingsPage", "DisplayMode_Title"));
    if (QLabel *displayModeDesc = findChild<QLabel *>("DisplayMode_Desc"))
        displayModeDesc->setText(loc.getString("SettingsPage", "DisplayMode_Desc"));

    if (QLabel *displayScreenTitle = findChild<QLabel *>("DisplayScreen_Title"))
        displayScreenTitle->setText(loc.getString("SettingsPage", "DisplayScreen_Title"));
    if (QLabel *displayScreenDesc = findChild<QLabel *>("DisplayScreen_Desc"))
        displayScreenDesc->setText(loc.getString("SettingsPage", "DisplayScreen_Desc"));
    if (m_displayScreenChooser)
        m_displayScreenChooser->updateTexts();

    if (QLabel *resolutionTitle = findChild<QLabel *>("Resolution_Title"))
        resolutionTitle->setText(loc.getString("SettingsPage", "Resolution_Title"));
    if (QLabel *resolutionDesc = findChild<QLabel *>("Resolution_Desc"))
        resolutionDesc->setText(loc.getString("SettingsPage", "Resolution_Desc"));

    refreshDisplayModeOptions();
    refreshDisplayControls();

    if (QLabel *pinTitle = findChild<QLabel *>("PinToStart_Title"))
        pinTitle->setText(loc.getString("SettingsPage", "PinToStart_Title"));
    if (QLabel *pinDesc = findChild<QLabel *>("PinToStart_Desc"))
        pinDesc->setText(loc.getString("SettingsPage", "PinToStart_Desc"));
    if (m_pinToStartBtn)
        m_pinToStartBtn->setText(loc.getString("SettingsPage", "PinToStart_Btn"));

    if (QLabel *clearTitle = findChild<QLabel *>("ClearCache_Title"))
        clearTitle->setText(loc.getString("SettingsPage", "ClearCache_Title"));
    if (QLabel *clearDesc = findChild<QLabel *>("ClearCache_Desc"))
        clearDesc->setText(loc.getString("SettingsPage", "ClearCache_Desc"));
    if (m_clearCacheBtn)
        m_clearCacheBtn->setText(loc.getString("SettingsPage", "ClearCache_Btn"));

    if (QPushButton *githubLink = findChild<QPushButton *>("GithubLink"))
        githubLink->setText(loc.getString("SettingsPage", "GithubLink"));
    if (m_userAgreementBtn)
        m_userAgreementBtn->setText(loc.getString("SettingsPage", "UserAgreementBtn"));

    m_openSourceToggleBaseText = loc.getString("SettingsPage", "OpenSourceBtn");
    updateOpenSourceToggleText();
}

void SettingsPage::updateTheme() {
    const bool isDark = ConfigManager::instance().isCurrentThemeDark();
    setStyleSheet(OverlayControlStyle::pageStyleSheet(isDark));

    const QList<QLabel *> iconLabels = findChildren<QLabel *>("SettingIcon");
    for (QLabel *lbl : iconLabels) {
        const QString iconPath = lbl->property("svgIcon").toString();
        if (!iconPath.isEmpty())
            lbl->setPixmap(loadSvgIcon(iconPath, isDark));
    }

    const QString panelStyle = QStringLiteral(
        "QWidget#OpenSourceGlassPanel {"
        "  background: transparent;"
        "  border: none;"
        "}"
        "QWidget#OpenSourceViewport {"
        "  background: transparent;"
        "}");

    if (auto *switchControl = dynamic_cast<AppleSwitch *>(m_sidebarCompactCheck))
        switchControl->setDarkMode(isDark);
    if (auto *switchControl = dynamic_cast<AppleSwitch *>(m_debugCheck))
        switchControl->setDarkMode(isDark);
    if (m_themeButton) {
        m_themeButton->setDarkMode(isDark);
        if (m_themeButton->themeIndex() != static_cast<int>(ConfigManager::instance().getTheme())) {
            m_themeButton->setThemeIndex(static_cast<int>(ConfigManager::instance().getTheme()), false);
        }
    }
    if (m_displayScreenChooser)
        m_displayScreenChooser->updateTheme();

    const QString linkButtonStyle = OverlayControlStyle::linkButtonStyle(isDark);

    if (m_openSourceArea)
        m_openSourceArea->setStyleSheet(panelStyle);

    if (QPushButton *githubLink = findChild<QPushButton *>("GithubLink"))
        githubLink->setStyleSheet(linkButtonStyle);
    if (m_userAgreementBtn)
        m_userAgreementBtn->setStyleSheet(linkButtonStyle);
    if (m_openSourceToggleBtn)
        m_openSourceToggleBtn->setStyleSheet(linkButtonStyle);

    if (auto *cardsWidget = static_cast<OpenSourceCardsWidget *>(m_openSourceCardsWidget)) {
        for (QWidget *cardWidget : cardsWidget->cards()) {
            if (auto *card = dynamic_cast<OpenSourceCardButton *>(cardWidget))
                card->setDarkMode(isDark);
        }
    }
}

void SettingsPage::openUrl(const QString &url) {
    QDesktopServices::openUrl(QUrl(url));
    Logger::instance().logClick("OpenUrl: " + url);
}

void SettingsPage::toggleOpenSource() {
    if (!m_openSourceArea || !m_openSourceViewport || !m_openSourceExpandAnimation)
        return;

    refreshOpenSourceLayout();

    const int contentHeight = m_openSourceCardsWidget ? m_openSourceCardsWidget->height() + 20 : 0;
    m_isOpenSourceExpanded = !m_isOpenSourceExpanded;
    updateOpenSourceToggleText();

    m_openSourceExpandAnimation->stop();
    m_openSourceArea->setMinimumHeight(0);
    m_openSourceExpandAnimation->setStartValue(m_openSourceArea->maximumHeight());
    m_openSourceExpandAnimation->setEndValue(m_isOpenSourceExpanded ? contentHeight : 0);
    m_openSourceExpandAnimation->start();

    Logger::instance().logClick("ToggleOpenSource");
}

void SettingsPage::openLogDir() {
    Logger::instance().openLogDirectory();
    Logger::instance().logClick("OpenLogDir");
}

void SettingsPage::createStartMenuShortcut() {
    const QString startMenuPath = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (startMenuPath.isEmpty()) {
        Logger::instance().logError("Settings", "Could not find Start Menu path");
        return;
    }

    const QString shortcutPath = QDir(startMenuPath).filePath("APE HOI4 Tool Studio.lnk");
    const QString targetPath = QCoreApplication::applicationFilePath();

    QFile::remove(shortcutPath);
    if (QFile::link(targetPath, shortcutPath)) {
        Logger::instance().logInfo("Settings", "Successfully created Start Menu shortcut at: " + shortcutPath);
    } else {
        Logger::instance().logError("Settings", "Failed to create Start Menu shortcut at: " + shortcutPath);
    }

    Logger::instance().logClick("CreateStartMenuShortcut");
}

void SettingsPage::clearAppCache() {
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
    QDir dir(tempDir);
    if (dir.exists()) {
        dir.removeRecursively();
        Logger::instance().logInfo("Settings", "Cleared app cache at: " + tempDir);
    }

    Logger::instance().logClick("ClearAppCache");
    QApplication::quit();
}

#include "SettingsPage.moc"
