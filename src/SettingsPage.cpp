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
#include "LocalizationManager.h"
#include "Logger.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QEasingCurve>
#include <QEvent>
#include <QFile>
#include <QGradient>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVariantAnimation>
#include <QVBoxLayout>

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
      m_themeCombo(nullptr),
      m_languageCombo(nullptr),
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
    closeBtn->setFixedSize(30, 30);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet("border: none; font-size: 20px; color: #888;");
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

    m_themeCombo = new QComboBox();
    m_themeCombo->setCurrentIndex(static_cast<int>(ConfigManager::instance().getTheme()));
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        ConfigManager::instance().setTheme(static_cast<ConfigManager::Theme>(index));
        emit themeChanged();
    });
    interfaceLayout->addWidget(createSettingRow("Theme", ":/icons/palette.svg", "Theme Mode", "Select application appearance", m_themeCombo));

    m_sidebarCompactCheck = new QCheckBox();
    m_sidebarCompactCheck->setChecked(ConfigManager::instance().getSidebarCompactMode());
    connect(m_sidebarCompactCheck, &QCheckBox::toggled, [this](bool checked) {
        ConfigManager::instance().setSidebarCompactMode(checked);
        emit sidebarCompactChanged(checked);
    });
    interfaceLayout->addWidget(createSettingRow("Sidebar", ":/icons/sidebar.svg", "Compact Sidebar", "Auto-collapse sidebar", m_sidebarCompactCheck));

    contentLayout->addWidget(createGroup("Interface", interfaceLayout));

    auto *accessibilityLayout = new QVBoxLayout();
    accessibilityLayout->setSpacing(0);

    m_languageCombo = new QComboBox();
    m_languageCombo->addItems({"English", "简体中文", "繁體中文"});
    m_languageCombo->setCurrentText(ConfigManager::instance().getLanguage());
    connect(m_languageCombo, &QComboBox::currentTextChanged, [this](const QString &lang) {
        if (lang != ConfigManager::instance().getLanguage()) {
            ConfigManager::instance().setLanguage(lang);
            emit languageChanged();
        }
    });
    accessibilityLayout->addWidget(createSettingRow("Lang", ":/icons/globe.svg", "Language", "Restart required to apply changes", m_languageCombo));

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

    m_debugCheck = new QCheckBox();
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

    m_openLogBtn = new QPushButton("Open Logs");
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

    QPushButton *githubLink = new QPushButton("GitHub Repository");
    githubLink->setObjectName("GithubLink");
    githubLink->setFlat(true);
    githubLink->setCursor(Qt::PointingHandCursor);
    connect(githubLink, &QPushButton::clicked, [this]() {
        openUrl("https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio");
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
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #888; margin-left: 10px; margin-bottom: 5px;");

    QWidget *container = new QWidget();
    container->setObjectName("GroupContainer");
    container->setLayout(contentLayout);

    groupLayout->addWidget(titleLabel);
    groupLayout->addWidget(container);

    return group;
}

QWidget *SettingsPage::createSettingRow(const QString &id, const QString &icon, const QString &title, const QString &desc, QWidget *control) {
    auto *row = new QWidget();
    row->setObjectName("SettingRow");
    row->setFixedHeight(60);

    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(15, 10, 20, 10);
    layout->setSpacing(15);

    QLabel *iconLbl = new QLabel();
    iconLbl->setObjectName("SettingIcon");
    iconLbl->setFixedSize(34, 34);
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
    if (control)
        layout->addWidget(control);

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

void SettingsPage::updateTexts() {
    LocalizationManager &loc = LocalizationManager::instance();

    {
        QSignalBlocker blocker(m_themeCombo);
        m_themeCombo->clear();
        m_themeCombo->addItem(loc.getString("SettingsPage", "Theme_System"));
        m_themeCombo->addItem(loc.getString("SettingsPage", "Theme_Light"));
        m_themeCombo->addItem(loc.getString("SettingsPage", "Theme_Dark"));
        m_themeCombo->setCurrentIndex(static_cast<int>(ConfigManager::instance().getTheme()));
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

    if (QLabel *langTitle = findChild<QLabel *>("Lang_Title"))
        langTitle->setText(loc.getString("SettingsPage", "Lang_Title"));
    if (QLabel *langDesc = findChild<QLabel *>("Lang_Desc"))
        langDesc->setText(loc.getString("SettingsPage", "Lang_Desc"));

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

    if (QLabel *pinTitle = findChild<QLabel *>("PinToStart_Title"))
        pinTitle->setText(loc.getString("SettingsPage", "PinToStart_Title"));
    if (QLabel *pinDesc = findChild<QLabel *>("PinToStart_Desc"))
        pinDesc->setText(loc.getString("SettingsPage", "PinToStart_Desc"));
    if (m_pinToStartBtn)
        m_pinToStartBtn->setText(loc.getString("SettingsPage", "PinToStart_Title"));

    if (QLabel *clearTitle = findChild<QLabel *>("ClearCache_Title"))
        clearTitle->setText(loc.getString("SettingsPage", "ClearCache_Title"));
    if (QLabel *clearDesc = findChild<QLabel *>("ClearCache_Desc"))
        clearDesc->setText(loc.getString("SettingsPage", "ClearCache_Desc"));
    if (m_clearCacheBtn)
        m_clearCacheBtn->setText(loc.getString("SettingsPage", "ClearCache_Title"));

    if (QPushButton *githubLink = findChild<QPushButton *>("GithubLink"))
        githubLink->setText(loc.getString("SettingsPage", "GithubLink"));
    if (m_userAgreementBtn)
        m_userAgreementBtn->setText(loc.getString("SettingsPage", "UserAgreementBtn"));

    m_openSourceToggleBaseText = loc.getString("SettingsPage", "OpenSourceBtn");
    updateOpenSourceToggleText();
}

void SettingsPage::updateTheme() {
    const bool isDark = ConfigManager::instance().isCurrentThemeDark();

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

    const QString linkButtonStyle = isDark
        ? QStringLiteral(
              "QPushButton {"
              "  color: rgba(225, 234, 248, 0.92);"
              "  text-align: left;"
              "  background: transparent;"
              "  border: none;"
              "  padding: 4px 0px;"
              "  font-weight: 600;"
              "}"
              "QPushButton:hover {"
              "  color: rgba(248, 251, 255, 1.0);"
              "}"
              "QPushButton:pressed {"
              "  color: rgba(200, 215, 239, 0.92);"
              "}")
        : QStringLiteral(
              "QPushButton {"
              "  color: rgba(35, 82, 170, 0.96);"
              "  text-align: left;"
              "  background: transparent;"
              "  border: none;"
              "  padding: 4px 0px;"
              "  font-weight: 600;"
              "}"
              "QPushButton:hover {"
              "  color: rgba(18, 58, 132, 1.0);"
              "}"
              "QPushButton:pressed {"
              "  color: rgba(52, 94, 177, 0.92);"
              "}");

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