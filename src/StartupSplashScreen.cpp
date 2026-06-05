//-------------------------------------------------------------------------------------
// StartupSplashScreen.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "StartupSplashScreen.h"
#include "LocalizationManager.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPixmap>
#include <QRectF>
#include <QRegularExpression>
#include <QScreen>
#include <QSize>
#include <QStringList>

namespace {
constexpr int kWindowWidth = 780;
constexpr int kWindowHeight = 520;
constexpr int kShadowMargin = 12;
constexpr qreal kCornerRadius = 14.0;
constexpr qreal kImageCornerRadius = 10.0;
constexpr qreal kBodyGap = 20.0;

QString elidedText(QPainter& painter, const QString& text, int width) {
    return QFontMetrics(painter.font()).elidedText(text, Qt::ElideRight, width);
}

QRectF coverSourceRect(const QSize& sourceSize, const QRectF& targetRect) {
    if (sourceSize.isEmpty() || targetRect.isEmpty()) {
        return QRectF();
    }

    const qreal sourceRatio = static_cast<qreal>(sourceSize.width()) / static_cast<qreal>(sourceSize.height());
    const qreal targetRatio = targetRect.width() / targetRect.height();
    if (sourceRatio > targetRatio) {
        const qreal width = sourceSize.height() * targetRatio;
        return QRectF((sourceSize.width() - width) / 2.0, 0.0, width, sourceSize.height());
    }

    const qreal height = sourceSize.width() / targetRatio;
    return QRectF(0.0, (sourceSize.height() - height) / 2.0, sourceSize.width(), height);
}

QString productSubtitle(const QString& productName) {
    const QString trimmed = productName.trimmed();
    const QString prefix = QStringLiteral("APE ");
    if (trimmed.startsWith(prefix)) {
        return trimmed.mid(prefix.size()).trimmed();
    }
    return trimmed;
}

QString startupSplashLocalizationText(const QString& key, const QString& fallback) {
    const QString value = LocalizationManager::instance().getString(QStringLiteral("StartupSplash"), key);
    return value == key ? fallback : value;
}

QString startupSplashStageText(StartupSplashStage stage) {
    switch (stage) {
    case StartupSplashStage::CheckingInstance:
        return startupSplashLocalizationText(QStringLiteral("StageCheckingInstance"), QStringLiteral("Checking running instance..."));
    case StartupSplashStage::PreparingRuntime:
        return startupSplashLocalizationText(QStringLiteral("StagePreparingRuntime"), QStringLiteral("Preparing application runtime..."));
    case StartupSplashStage::LoadingLanguage:
        return startupSplashLocalizationText(QStringLiteral("StageLoadingLanguage"), QStringLiteral("Loading interface language..."));
    case StartupSplashStage::ConfiguringRuntime:
        return startupSplashLocalizationText(QStringLiteral("StageConfiguringRuntime"), QStringLiteral("Configuring tool runtime..."));
    case StartupSplashStage::OrganizingPackages:
        return startupSplashLocalizationText(QStringLiteral("StageOrganizingPackages"), QStringLiteral("Organizing tools and plugins..."));
    case StartupSplashStage::RefreshingSecurityBundle:
        return startupSplashLocalizationText(QStringLiteral("StageRefreshingSecurityBundle"), QStringLiteral("Refreshing security bundle..."));
    case StartupSplashStage::PreparingAccountSession:
        return startupSplashLocalizationText(QStringLiteral("StagePreparingAccountSession"), QStringLiteral("Preparing account session..."));
    case StartupSplashStage::RegisteringFileAssociations:
        return startupSplashLocalizationText(QStringLiteral("StageRegisteringFileAssociations"), QStringLiteral("Registering file associations..."));
    case StartupSplashStage::CleaningCaches:
        return startupSplashLocalizationText(QStringLiteral("StageCleaningCaches"), QStringLiteral("Cleaning startup caches..."));
    case StartupSplashStage::BuildingMainWindow:
        return startupSplashLocalizationText(QStringLiteral("StageBuildingMainWindow"), QStringLiteral("Building main window..."));
    case StartupSplashStage::OpeningMainWindow:
        return startupSplashLocalizationText(QStringLiteral("StageOpeningMainWindow"), QStringLiteral("Opening main window..."));
    }

    return QStringLiteral("Preparing application runtime...");
}

QString loadStartupContributorsText() {
    QFile file(QStringLiteral(":/splash/startup_contributors.proto"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    const QString content = QString::fromUtf8(file.readAll());
    const QRegularExpression contributorPattern(QStringLiteral("^\\s*contributor\\s*:\\s*\"([^\"]+)\"\\s*$"));
    QStringList contributors;
    const QStringList lines = content.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QRegularExpressionMatch match = contributorPattern.match(line);
        if (match.hasMatch()) {
            const QString contributor = match.captured(1).trimmed();
            if (!contributor.isEmpty()) {
                contributors.append(contributor);
            }
        }
    }
    return contributors.join(QStringLiteral(", "));
}

qreal textBlockHeight(const QFont& font, const QString& text, qreal width, int flags) {
    const QFontMetricsF metrics(font);
    const QRectF bounds = metrics.boundingRect(QRectF(0.0, 0.0, width, 1000.0), flags, text);
    return qMax(metrics.height(), bounds.height());
}
}

StartupSplashScreen::StartupSplashScreen(QWidget* parent)
    : QWidget(parent,
              Qt::SplashScreen
              | Qt::FramelessWindowHint
              | Qt::WindowStaysOnTopHint
              | Qt::NoDropShadowWindowHint
              | Qt::WindowDoesNotAcceptFocus)
    , m_productName(QStringLiteral("APE HOI4 Tool Studio"))
    , m_copyrightText(QStringLiteral("© 2026 Team APE:RIP. All rights reserved.\nFor more details and legal notices, go to\nthe About APE HOI4 Tool Studio screen."))
    , m_artistCredit(QStringLiteral("Aperip Work"))
    , m_footerProductText(QStringLiteral("Aperip Daedalus Foundation"))
    , m_stageText(QStringLiteral("Preparing application runtime..."))
{
    setObjectName(QStringLiteral("StartupSplashScreen"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAutoFillBackground(false);
    setCursor(Qt::ArrowCursor);
    setFixedSize(kWindowWidth, kWindowHeight);
}

void StartupSplashScreen::showSplash() {
    centerOnPrimaryScreen();
    show();
    raise();
    processPaintEvents();
    QApplication::processEvents();
}

void StartupSplashScreen::applyLocalizedStrings() {
    setLocalizedStrings(
        startupSplashLocalizationText(QStringLiteral("ProductName"), QStringLiteral("APE HOI4 Tool Studio")),
        startupSplashLocalizationText(QStringLiteral("Copyright"), QStringLiteral("© 2026 Team APE:RIP. All rights reserved.\nFor more details and legal notices, go to\nthe About APE HOI4 Tool Studio screen.")),
        startupSplashLocalizationText(QStringLiteral("ArtistCredit"), QStringLiteral("Aperip Work")),
        startupSplashLocalizationText(QStringLiteral("FooterProduct"), QStringLiteral("Aperip Daedalus Foundation")));

    m_contributorsText = loadStartupContributorsText();
    update();
    processPaintEvents();
}

void StartupSplashScreen::setLocalizedStrings(const QString& productName,
                                              const QString& copyrightText,
                                              const QString& artistCredit,
                                              const QString& footerProductText) {
    if (!productName.trimmed().isEmpty()) {
        m_productName = productName.trimmed();
    }
    if (!copyrightText.trimmed().isEmpty()) {
        m_copyrightText = copyrightText.trimmed();
    }
    if (!artistCredit.trimmed().isEmpty()) {
        m_artistCredit = artistCredit.trimmed();
    }
    if (!footerProductText.trimmed().isEmpty()) {
        m_footerProductText = footerProductText.trimmed();
    }
    update();
    processPaintEvents();
}

void StartupSplashScreen::setProgress(qreal progress, StartupSplashStage stage) {
    setProgress(progress, startupSplashStageText(stage));
}

void StartupSplashScreen::setProgress(qreal progress, const QString& stageText) {
    m_progress = qBound(0.0, progress, 1.0);
    if (!stageText.trimmed().isEmpty()) {
        m_stageText = stageText.trimmed();
    }
    update();
    processPaintEvents();
}

void StartupSplashScreen::finishWithMainWindow(QWidget* mainWindow) {
    setProgress(1.0, m_stageText);
    if (mainWindow) {
        mainWindow->raise();
        mainWindow->activateWindow();
    }
    hide();
}

void StartupSplashScreen::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing, true);

    const QRectF panelRect = rect().adjusted(kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);

    QPainterPath panelPath;
    panelPath.addRoundedRect(panelRect, kCornerRadius, kCornerRadius);
    painter.fillPath(panelPath, QColor(255, 255, 255));

    painter.setPen(QPen(QColor(220, 222, 226), 1.0));
    painter.drawRoundedRect(panelRect, kCornerRadius, kCornerRadius);

    const QRectF imageTargetRect(panelRect.left() + 320.0,
                                 panelRect.top() + 20.0,
                                 panelRect.width() - 340.0,
                                 panelRect.height() - 40.0);
    QPainterPath imageClipPath;
    imageClipPath.addRoundedRect(imageTargetRect, kImageCornerRadius, kImageCornerRadius);
    painter.save();
    painter.setClipPath(imageClipPath);
    const QPixmap heroPixmap(QStringLiteral(":/splash/startup_hero_placeholder.png"));
    if (!heroPixmap.isNull()) {
        const QRectF sourceRect = coverSourceRect(heroPixmap.size(), imageTargetRect);
        painter.drawPixmap(imageTargetRect, heroPixmap, sourceRect);
    } else {
        painter.fillRect(imageTargetRect, QColor(0, 255, 0));
    }
    painter.restore();

    painter.setPen(QPen(QColor(226, 228, 232), 1.0));
    painter.drawRoundedRect(imageTargetRect, kImageCornerRadius, kImageCornerRadius);

    const qreal leftX = panelRect.left() + 34.0;
    const qreal headerTop = panelRect.top() + 34.0;
    const QRectF iconRect(leftX, headerTop, 58.0, 58.0);
    painter.setPen(Qt::NoPen);

    const QPixmap iconPixmap = QIcon(QStringLiteral(":/app.ico")).pixmap(58, 58);
    if (!iconPixmap.isNull()) {
        painter.drawPixmap(
            QPointF(iconRect.center().x() - iconPixmap.width() / 2.0,
                    iconRect.center().y() - iconPixmap.height() / 2.0),
            iconPixmap);
    }

    const qreal titleBlockHeight = 62.0;
    const qreal titleBlockTop = iconRect.center().y() - titleBlockHeight / 2.0;
    const QRectF titleLine1Rect(leftX + 76.0, titleBlockTop, 170.0, 35.0);
    const QRectF titleLine2Rect(leftX + 76.0, titleBlockTop + 37.0, 184.0, 25.0);

    const QPixmap titleMarkPixmap = QIcon(QStringLiteral(":/splash/ape-wordmark.svg")).pixmap(QSize(170, 35));
    if (!titleMarkPixmap.isNull()) {
        painter.drawPixmap(titleLine1Rect.toRect(), titleMarkPixmap);
    } else {
        QFont titleLine1Font = QApplication::font();
        titleLine1Font.setPointSize(24);
        titleLine1Font.setWeight(QFont::Bold);
        painter.setFont(titleLine1Font);
        painter.setPen(QColor(10, 14, 24));
        painter.drawText(titleLine1Rect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("APE"));
    }

    QFont titleLine2Font = QApplication::font();
    titleLine2Font.setPointSize(15);
    titleLine2Font.setWeight(QFont::DemiBold);
    painter.setFont(titleLine2Font);
    painter.setPen(QColor(10, 14, 24));
    painter.drawText(titleLine2Rect,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     elidedText(painter, productSubtitle(m_productName), 184));

    QFont metaFont = QApplication::font();
    metaFont.setPointSize(9);

    QFont stageFont = QApplication::font();
    stageFont.setPointSize(10);

    QFont contributorsFont = QApplication::font();
    contributorsFont.setPointSize(7);

    const qreal textBlockWidth = 254.0;
    const qreal progressBlockWidth = 268.0;
    const qreal artistHeight = textBlockHeight(metaFont, m_artistCredit, textBlockWidth, Qt::AlignLeft | Qt::AlignTop);
    const qreal copyrightHeight = textBlockHeight(metaFont, m_copyrightText, textBlockWidth, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap);
    const qreal progressHeight = textBlockHeight(stageFont, m_stageText, progressBlockWidth, Qt::AlignLeft | Qt::AlignTop);
    const qreal contributorsHeight = textBlockHeight(contributorsFont, m_contributorsText, textBlockWidth, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap);
    const qreal bodyBlockHeight = artistHeight + copyrightHeight + progressHeight + contributorsHeight + kBodyGap * 3.0;
    const qreal bodyBlockTop = panelRect.center().y() - bodyBlockHeight / 2.0;

    const QRectF artistRect(leftX, bodyBlockTop, textBlockWidth, artistHeight);
    painter.setFont(metaFont);
    painter.setPen(QColor(77, 82, 94));
    painter.drawText(artistRect,
                     Qt::AlignLeft | Qt::AlignTop,
                     m_artistCredit);

    const qreal copyrightTop = artistRect.bottom() + kBodyGap;
    const QRectF copyrightRect(leftX, copyrightTop, textBlockWidth, copyrightHeight);
    painter.drawText(copyrightRect,
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                     m_copyrightText);

    const qreal progressTop = copyrightRect.bottom() + kBodyGap;
    const QRectF progressRect(leftX, progressTop, progressBlockWidth, progressHeight);

    painter.setFont(stageFont);
    painter.setPen(QColor(10, 14, 24));
    const QString progressText = QStringLiteral("%1%2%").arg(m_stageText, QString::number(qRound(m_progress * 100.0)));
    painter.drawText(progressRect,
                     Qt::AlignLeft | Qt::AlignTop,
                     elidedText(painter, progressText, static_cast<int>(progressRect.width())));

    const qreal contributorsTop = progressRect.bottom() + kBodyGap;
    const QRectF contributorsRect(leftX, contributorsTop, textBlockWidth, contributorsHeight);
    painter.setFont(contributorsFont);
    painter.setPen(QColor(12, 18, 30));
    painter.drawText(contributorsRect,
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                     m_contributorsText);

    const QRectF footerIconRect(leftX, panelRect.bottom() - 47.0, 24.0, 24.0);
    const QPixmap footerIconPixmap = QIcon(QStringLiteral(":/splash/aperip-product.svg")).pixmap(24, 24);
    if (!footerIconPixmap.isNull()) {
        painter.drawPixmap(footerIconRect.toRect(), footerIconPixmap);
    }

    QFont footerFont = QApplication::font();
    footerFont.setPointSize(9);
    footerFont.setWeight(QFont::DemiBold);
    painter.setFont(footerFont);
    painter.setPen(QColor(90, 94, 104));
    painter.drawText(QRectF(footerIconRect.right() + 8.0, panelRect.bottom() - 44.0, 244.0, 20.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     m_footerProductText);
}

void StartupSplashScreen::centerOnPrimaryScreen() {
    QScreen* screen = QGuiApplication::primaryScreen();
    const QRect screenGeometry = screen ? screen->availableGeometry() : QRect(QPoint(0, 0), QSize(kWindowWidth, kWindowHeight));
    move(screenGeometry.center() - QPoint(width() / 2, height() / 2));
}

void StartupSplashScreen::processPaintEvents() {
    if (isVisible()) {
        repaint();
    }
}
