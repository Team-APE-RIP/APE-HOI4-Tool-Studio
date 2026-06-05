//-------------------------------------------------------------------------------------
// ToolQmlThemeProvider.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolQmlThemeProvider.h"
#include "ConfigManager.h"

#include <QApplication>
#include <QFont>

namespace {
QString normalizedThemeName(const QString& themeName) {
    const QString normalized = themeName.trimmed().toLower();
    if (normalized == QStringLiteral("dark")) {
        return QStringLiteral("dark");
    }
    return QStringLiteral("light");
}

int fontPixelSize(const QFont& font, int fallback) {
    if (font.pixelSize() > 0) {
        return font.pixelSize();
    }

    const qreal pointSize = font.pointSizeF();
    if (pointSize > 0.0) {
        return static_cast<int>((pointSize * 96.0 / 72.0) + 0.5);
    }

    return fallback;
}
}

ToolQmlThemeProvider::ToolQmlThemeProvider(QObject* parent)
    : QObject(parent) {
    m_categories.insert(QStringLiteral("spacing"), buildSpacing());
    m_categories.insert(QStringLiteral("radius"), buildRadius());
    m_categories.insert(QStringLiteral("typography"), buildTypography());
    m_categories.insert(QStringLiteral("metrics"), buildMetrics());
    setTheme(
        ConfigManager::instance().isCurrentThemeDark()
            ? QStringLiteral("dark")
            : QStringLiteral("light")
    );
}

QString ToolQmlThemeProvider::theme() const {
    return m_theme;
}

QVariantMap ToolQmlThemeProvider::colors() const {
    return category(QStringLiteral("colors"));
}

QVariantMap ToolQmlThemeProvider::surfaces() const {
    return category(QStringLiteral("surfaces"));
}

QVariantMap ToolQmlThemeProvider::spacing() const {
    return category(QStringLiteral("spacing"));
}

QVariantMap ToolQmlThemeProvider::radius() const {
    return category(QStringLiteral("radius"));
}

QVariantMap ToolQmlThemeProvider::typography() const {
    return category(QStringLiteral("typography"));
}

QVariantMap ToolQmlThemeProvider::metrics() const {
    return category(QStringLiteral("metrics"));
}

QVariantMap ToolQmlThemeProvider::buttons() const {
    return category(QStringLiteral("buttons"));
}

QVariantMap ToolQmlThemeProvider::tables() const {
    return category(QStringLiteral("tables"));
}

QVariantMap ToolQmlThemeProvider::lists() const {
    return category(QStringLiteral("lists"));
}

QVariantMap ToolQmlThemeProvider::inputs() const {
    return category(QStringLiteral("inputs"));
}

QVariantMap ToolQmlThemeProvider::dividers() const {
    return category(QStringLiteral("dividers"));
}

QVariantMap ToolQmlThemeProvider::fonts() const {
    return category(QStringLiteral("fonts"));
}

void ToolQmlThemeProvider::setTheme(const QString& themeName) {
    const QString normalized = normalizeThemeName(themeName);
    if (normalized == m_theme && m_categories.contains(QStringLiteral("colors"))) {
        return;
    }

    m_theme = normalized;
    rebuildThemeCategories();

    emit themeChanged();
    emit tokensChanged();
}

QVariantMap ToolQmlThemeProvider::tokens(const QString& categoryName) const {
    return category(categoryName);
}

QVariant ToolQmlThemeProvider::token(const QString& categoryName,
                                     const QString& key,
                                     const QVariant& fallback) const {
    return category(categoryName).value(key, fallback);
}

QVariantMap ToolQmlThemeProvider::category(const QString& name) const {
    return m_categories.value(name.trimmed().toLower(), QVariantMap());
}

QVariantMap ToolQmlThemeProvider::buildLightColors() const {
    QVariantMap palette;
    palette.insert(QStringLiteral("window"), QStringLiteral("#6BF6F6F8"));
    palette.insert(QStringLiteral("topBar"), QStringLiteral("#40FFFFFF"));
    palette.insert(QStringLiteral("searchBar"), QStringLiteral("#52FFFFFF"));
    palette.insert(QStringLiteral("table"), QStringLiteral("transparent"));
    palette.insert(QStringLiteral("sidebar"), QStringLiteral("#85F6F6F8"));
    palette.insert(QStringLiteral("rail"), QStringLiteral("#85F6F6F8"));
    palette.insert(QStringLiteral("header"), QStringLiteral("#42FFFFFF"));
    palette.insert(QStringLiteral("surface"), QStringLiteral("#94FFFFFF"));
    palette.insert(QStringLiteral("surfaceAlt"), QStringLiteral("#66FFFFFF"));
    palette.insert(QStringLiteral("surfaceSidebar"), QStringLiteral("#85F6F6F8"));
    palette.insert(QStringLiteral("surfaceOverlay"), QStringLiteral("#78000000"));
    palette.insert(QStringLiteral("border"), QStringLiteral("#2E3C3C43"));
    palette.insert(QStringLiteral("borderStrong"), QStringLiteral("#423C3C43"));
    palette.insert(QStringLiteral("textPrimary"), QStringLiteral("#1D1D1F"));
    palette.insert(QStringLiteral("textSecondary"), QStringLiteral("#3A3A3C"));
    palette.insert(QStringLiteral("textMuted"), QStringLiteral("#6E6E73"));
    palette.insert(QStringLiteral("textInverted"), QStringLiteral("#FFFFFF"));
    palette.insert(QStringLiteral("accent"), QStringLiteral("#007AFF"));
    palette.insert(QStringLiteral("accentHover"), QStringLiteral("#E60A84FF"));
    palette.insert(QStringLiteral("accentPressed"), QStringLiteral("#F00066D6"));
    palette.insert(QStringLiteral("accentSoft"), QStringLiteral("#24007AFF"));
    palette.insert(QStringLiteral("success"), QStringLiteral("#16A34A"));
    palette.insert(QStringLiteral("warning"), QStringLiteral("#D97706"));
    palette.insert(QStringLiteral("danger"), QStringLiteral("#DC2626"));
    palette.insert(QStringLiteral("selection"), QStringLiteral("#D8007AFF"));
    palette.insert(QStringLiteral("hover"), QStringLiteral("#4DE8E8ED"));
    palette.insert(QStringLiteral("compareHighlight"), QStringLiteral("#75FFEBD2"));
    palette.insert(QStringLiteral("latest"), QStringLiteral("#007AFF"));
    palette.insert(QStringLiteral("priority"), QStringLiteral("#D9534F"));
    palette.insert(QStringLiteral("loadingCard"), QStringLiteral("#D9FFFFFF"));
    palette.insert(QStringLiteral("loadingBorder"), QStringLiteral("#4DD2D2D7"));
    palette.insert(QStringLiteral("loadingTrack"), QStringLiteral("#80E5E5EA"));
    palette.insert(QStringLiteral("loadingText"), QStringLiteral("#1D1D1F"));
    palette.insert(QStringLiteral("tooltipBackground"), QStringLiteral("#F5FFFFFF"));
    palette.insert(QStringLiteral("tooltipBorder"), QStringLiteral("#333C3C43"));
    palette.insert(QStringLiteral("tooltipText"), QStringLiteral("#1D1D1F"));
    palette.insert(QStringLiteral("shadow"), QStringLiteral("#1A000000"));
    return palette;
}

QVariantMap ToolQmlThemeProvider::buildDarkColors() const {
    QVariantMap palette;
    palette.insert(QStringLiteral("window"), QStringLiteral("#402A2A2C"));
    palette.insert(QStringLiteral("topBar"), QStringLiteral("#383A3A3C"));
    palette.insert(QStringLiteral("searchBar"), QStringLiteral("#4A3A3A3C"));
    palette.insert(QStringLiteral("table"), QStringLiteral("transparent"));
    palette.insert(QStringLiteral("sidebar"), QStringLiteral("#8A222224"));
    palette.insert(QStringLiteral("rail"), QStringLiteral("#8A222224"));
    palette.insert(QStringLiteral("header"), QStringLiteral("#403A3A3C"));
    palette.insert(QStringLiteral("surface"), QStringLiteral("#802C2C2E"));
    palette.insert(QStringLiteral("surfaceAlt"), QStringLiteral("#593A3A3C"));
    palette.insert(QStringLiteral("surfaceSidebar"), QStringLiteral("#8A222224"));
    palette.insert(QStringLiteral("surfaceOverlay"), QStringLiteral("#78000000"));
    palette.insert(QStringLiteral("border"), QStringLiteral("#33FFFFFF"));
    palette.insert(QStringLiteral("borderStrong"), QStringLiteral("#4AFFFFFF"));
    palette.insert(QStringLiteral("textPrimary"), QStringLiteral("#F5F5F7"));
    palette.insert(QStringLiteral("textSecondary"), QStringLiteral("#EBEBF5"));
    palette.insert(QStringLiteral("textMuted"), QStringLiteral("#A1A1A6"));
    palette.insert(QStringLiteral("textInverted"), QStringLiteral("#FFFFFF"));
    palette.insert(QStringLiteral("accent"), QStringLiteral("#007AFF"));
    palette.insert(QStringLiteral("accentHover"), QStringLiteral("#EA0A84FF"));
    palette.insert(QStringLiteral("accentPressed"), QStringLiteral("#F20066D6"));
    palette.insert(QStringLiteral("accentSoft"), QStringLiteral("#3A0A84FF"));
    palette.insert(QStringLiteral("success"), QStringLiteral("#22C55E"));
    palette.insert(QStringLiteral("warning"), QStringLiteral("#F59E0B"));
    palette.insert(QStringLiteral("danger"), QStringLiteral("#EF4444"));
    palette.insert(QStringLiteral("selection"), QStringLiteral("#DC0A84FF"));
    palette.insert(QStringLiteral("hover"), QStringLiteral("#3348484A"));
    palette.insert(QStringLiteral("compareHighlight"), QStringLiteral("#66B45F14"));
    palette.insert(QStringLiteral("latest"), QStringLiteral("#007AFF"));
    palette.insert(QStringLiteral("priority"), QStringLiteral("#F48771"));
    palette.insert(QStringLiteral("loadingCard"), QStringLiteral("#D92C2C2E"));
    palette.insert(QStringLiteral("loadingBorder"), QStringLiteral("#4AFFFFFF"));
    palette.insert(QStringLiteral("loadingTrack"), QStringLiteral("#663A3A3C"));
    palette.insert(QStringLiteral("loadingText"), QStringLiteral("#F2F2F2"));
    palette.insert(QStringLiteral("tooltipBackground"), QStringLiteral("#F52C2C2E"));
    palette.insert(QStringLiteral("tooltipBorder"), QStringLiteral("#33FFFFFF"));
    palette.insert(QStringLiteral("tooltipText"), QStringLiteral("#F5F5F7"));
    palette.insert(QStringLiteral("shadow"), QStringLiteral("#40000000"));
    return palette;
}

QVariantMap ToolQmlThemeProvider::buildSurfaces() const {
    const QVariantMap palette = colors();
    QVariantMap surfacesMap;
    surfacesMap.insert(QStringLiteral("window"), palette.value(QStringLiteral("window")));
    surfacesMap.insert(QStringLiteral("topBar"), palette.value(QStringLiteral("topBar")));
    surfacesMap.insert(QStringLiteral("searchBar"), palette.value(QStringLiteral("searchBar")));
    surfacesMap.insert(QStringLiteral("table"), palette.value(QStringLiteral("table")));
    surfacesMap.insert(QStringLiteral("sidebar"), palette.value(QStringLiteral("sidebar")));
    surfacesMap.insert(QStringLiteral("rail"), palette.value(QStringLiteral("rail")));
    surfacesMap.insert(QStringLiteral("header"), palette.value(QStringLiteral("header")));
    surfacesMap.insert(QStringLiteral("surface"), palette.value(QStringLiteral("surface")));
    surfacesMap.insert(QStringLiteral("surfaceAlt"), palette.value(QStringLiteral("surfaceAlt")));
    surfacesMap.insert(QStringLiteral("content"), palette.value(QStringLiteral("window")));
    surfacesMap.insert(QStringLiteral("overlayMask"), palette.value(QStringLiteral("surfaceOverlay")));
    surfacesMap.insert(QStringLiteral("loadingCard"), palette.value(QStringLiteral("loadingCard")));
    return surfacesMap;
}

QVariantMap ToolQmlThemeProvider::buildSpacing() const {
    QVariantMap values;
    values.insert(QStringLiteral("xxs"), 2);
    values.insert(QStringLiteral("xs"), 4);
    values.insert(QStringLiteral("sm"), 8);
    values.insert(QStringLiteral("md"), 10);
    values.insert(QStringLiteral("lg"), 12);
    values.insert(QStringLiteral("xl"), 16);
    values.insert(QStringLiteral("xxl"), 24);
    return values;
}

QVariantMap ToolQmlThemeProvider::buildRadius() const {
    QVariantMap values;
    values.insert(QStringLiteral("sm"), 4);
    values.insert(QStringLiteral("md"), 5);
    values.insert(QStringLiteral("lg"), 8);
    values.insert(QStringLiteral("xl"), 12);
    values.insert(QStringLiteral("pill"), 999);
    return values;
}

QVariantMap ToolQmlThemeProvider::buildTypography() const {
    const QFont applicationFont = QApplication::font();
    const QString fontFamily = applicationFont.family().trimmed().isEmpty()
        ? QStringLiteral("Segoe UI")
        : applicationFont.family();
    const int bodySize = fontPixelSize(applicationFont, 13);

    QVariantMap values;
    values.insert(QStringLiteral("fontFamily"), fontFamily);
    values.insert(QStringLiteral("fontFamilyMonospace"), QStringLiteral("Consolas"));
    values.insert(QStringLiteral("caption"), bodySize > 11 ? bodySize - 2 : bodySize);
    values.insert(QStringLiteral("body"), bodySize);
    values.insert(QStringLiteral("subheading"), bodySize + 2);
    values.insert(QStringLiteral("heading"), bodySize + 5);
    values.insert(QStringLiteral("weightNormal"), 400);
    values.insert(QStringLiteral("weightMedium"), 500);
    values.insert(QStringLiteral("weightBold"), 700);
    return values;
}

QVariantMap ToolQmlThemeProvider::buildMetrics() const {
    QVariantMap values;
    values.insert(QStringLiteral("topBarHeight"), 40);
    values.insert(QStringLiteral("searchHeight"), 38);
    values.insert(QStringLiteral("listHeaderHeight"), 38);
    values.insert(QStringLiteral("tableRowHeight"), 34);
    values.insert(QStringLiteral("compareHeaderHeight"), 40);
    values.insert(QStringLiteral("sideBarPanelWidth"), 190);
    values.insert(QStringLiteral("sideBarPanelMinimumWidth"), 0);
    values.insert(QStringLiteral("sideBarPanelMaximumWidth"), 440);
    values.insert(QStringLiteral("sideBarRailWidth"), 60);
    values.insert(QStringLiteral("sideBarResizeHandleWidth"), 6);
    values.insert(QStringLiteral("sideBarRailButtonSize"), 44);
    values.insert(QStringLiteral("sideBarHeaderHeight"), 40);
    values.insert(QStringLiteral("toolbarButtonHeight"), 28);
    values.insert(QStringLiteral("toolbarButtonWidth"), 100);
    values.insert(QStringLiteral("toolbarListButtonWidth"), 84);
    values.insert(QStringLiteral("statusBarHeight"), 28);
    values.insert(QStringLiteral("controlHeight"), 32);
    values.insert(QStringLiteral("controlHeightLarge"), 40);
    values.insert(QStringLiteral("iconSize"), 18);
    values.insert(QStringLiteral("loadingWidth"), 320);
    values.insert(QStringLiteral("loadingHeight"), 180);
    values.insert(QStringLiteral("loadingProgressHeight"), 6);
    values.insert(QStringLiteral("overlayOpacity"), 0.72);
    return values;
}

QVariantMap ToolQmlThemeProvider::buildButtons() const {
    const QVariantMap palette = colors();
    const QVariantMap textScale = typography();
    const bool dark = m_theme == QStringLiteral("dark");

    const QString checkedBase = dark ? QStringLiteral("#A80A84FF") : QStringLiteral("#960A84FF");
    const QString checkedTop = dark ? QStringLiteral("#AE65C9FF") : QStringLiteral("#A56ED1FF");
    const QString checkedMiddle = dark ? QStringLiteral("#8E0A84FF") : QStringLiteral("#760A84FF");
    const QString checkedBottom = dark ? QStringLiteral("#C40066D6") : QStringLiteral("#A80066D6");
    const QString checkedTint = dark ? QStringLiteral("#3000E5FF") : QStringLiteral("#2400E5FF");
    const QString checkedSheen = dark ? QStringLiteral("#45FFFFFF") : QStringLiteral("#38FFFFFF");
    const QString checkedHighlight = dark ? QStringLiteral("#78FFFFFF") : QStringLiteral("#66FFFFFF");
    const QString checkedBorder = dark ? QStringLiteral("#70FFFFFF") : QStringLiteral("#5AFFFFFF");
    const QString checkedShadow = dark ? QStringLiteral("#32001F5C") : QStringLiteral("#22003580");

    QVariantMap toolbarButton;
    toolbarButton.insert(QStringLiteral("height"), 28);
    toolbarButton.insert(QStringLiteral("radius"), 5);
    toolbarButton.insert(QStringLiteral("paddingHorizontal"), 15);
    toolbarButton.insert(QStringLiteral("paddingVertical"), 5);
    toolbarButton.insert(QStringLiteral("background"), QStringLiteral("transparent"));
    toolbarButton.insert(QStringLiteral("backgroundHover"), palette.value(QStringLiteral("hover")));
    toolbarButton.insert(QStringLiteral("backgroundChecked"), checkedBase);
    toolbarButton.insert(QStringLiteral("checkedGlassTop"), checkedTop);
    toolbarButton.insert(QStringLiteral("checkedGlassMiddle"), checkedMiddle);
    toolbarButton.insert(QStringLiteral("checkedGlassBottom"), checkedBottom);
    toolbarButton.insert(QStringLiteral("checkedGlassTint"), checkedTint);
    toolbarButton.insert(QStringLiteral("checkedGlassSheen"), checkedSheen);
    toolbarButton.insert(QStringLiteral("checkedGlassHighlight"), checkedHighlight);
    toolbarButton.insert(QStringLiteral("checkedGlassBorder"), checkedBorder);
    toolbarButton.insert(QStringLiteral("checkedGlassShadow"), checkedShadow);
    toolbarButton.insert(QStringLiteral("hoverBorder"), palette.value(QStringLiteral("border")));
    toolbarButton.insert(QStringLiteral("text"), palette.value(QStringLiteral("textMuted")));
    toolbarButton.insert(QStringLiteral("textChecked"), palette.value(QStringLiteral("textInverted")));
    toolbarButton.insert(QStringLiteral("fontFamily"), textScale.value(QStringLiteral("fontFamily")));
    toolbarButton.insert(QStringLiteral("fontPixelSize"), textScale.value(QStringLiteral("body")));
    toolbarButton.insert(QStringLiteral("weight"), textScale.value(QStringLiteral("weightNormal")));
    toolbarButton.insert(QStringLiteral("checkedWeight"), textScale.value(QStringLiteral("weightBold")));

    QVariantMap modeBadge = toolbarButton;
    modeBadge.insert(QStringLiteral("background"), checkedBase);
    modeBadge.insert(QStringLiteral("backgroundHover"), palette.value(QStringLiteral("accentHover")));
    modeBadge.insert(QStringLiteral("backgroundChecked"), checkedBase);
    modeBadge.insert(QStringLiteral("text"), palette.value(QStringLiteral("textInverted")));
    modeBadge.insert(QStringLiteral("textChecked"), palette.value(QStringLiteral("textInverted")));
    modeBadge.insert(QStringLiteral("weight"), textScale.value(QStringLiteral("weightBold")));
    modeBadge.insert(QStringLiteral("checkedWeight"), textScale.value(QStringLiteral("weightBold")));

    QVariantMap railToggle;
    railToggle.insert(QStringLiteral("width"), 24);
    railToggle.insert(QStringLiteral("height"), 40);
    railToggle.insert(QStringLiteral("radius"), 8);
    railToggle.insert(QStringLiteral("fontPixelSize"), 16);
    railToggle.insert(QStringLiteral("background"), QStringLiteral("transparent"));
    railToggle.insert(QStringLiteral("backgroundHover"), palette.value(QStringLiteral("hover")));
    railToggle.insert(QStringLiteral("backgroundChecked"), palette.value(QStringLiteral("selection")));
    railToggle.insert(QStringLiteral("text"), palette.value(QStringLiteral("textMuted")));
    railToggle.insert(QStringLiteral("textChecked"), palette.value(QStringLiteral("textInverted")));

    QVariantMap presets;
    presets.insert(QStringLiteral("toolbar"), toolbarButton);
    presets.insert(QStringLiteral("modeBadge"), modeBadge);
    presets.insert(QStringLiteral("railToggle"), railToggle);
    return presets;
}

QVariantMap ToolQmlThemeProvider::buildTables() const {
    const QVariantMap palette = colors();
    const QVariantMap textScale = typography();

    QVariantMap header;
    header.insert(QStringLiteral("height"), 38);
    header.insert(QStringLiteral("background"), palette.value(QStringLiteral("header")));
    header.insert(QStringLiteral("text"), palette.value(QStringLiteral("textPrimary")));
    header.insert(QStringLiteral("padding"), 8);
    header.insert(QStringLiteral("fontFamily"), textScale.value(QStringLiteral("fontFamily")));
    header.insert(QStringLiteral("fontPixelSize"), textScale.value(QStringLiteral("body")));
    header.insert(QStringLiteral("fontWeight"), textScale.value(QStringLiteral("weightBold")));

    QVariantMap row;
    row.insert(QStringLiteral("height"), 34);
    row.insert(QStringLiteral("background"), QStringLiteral("transparent"));
    row.insert(QStringLiteral("hoverBackground"), palette.value(QStringLiteral("hover")));
    row.insert(QStringLiteral("selectedBackground"), palette.value(QStringLiteral("selection")));
    row.insert(QStringLiteral("text"), palette.value(QStringLiteral("textPrimary")));
    row.insert(QStringLiteral("selectedText"), palette.value(QStringLiteral("textInverted")));
    row.insert(QStringLiteral("priorityText"), palette.value(QStringLiteral("priority")));
    row.insert(QStringLiteral("padding"), 8);
    row.insert(QStringLiteral("fontFamily"), textScale.value(QStringLiteral("fontFamily")));
    row.insert(QStringLiteral("fontPixelSize"), textScale.value(QStringLiteral("body")));

    QVariantMap compare;
    compare.insert(QStringLiteral("headerHeight"), 40);
    compare.insert(QStringLiteral("missingBackground"), palette.value(QStringLiteral("compareHighlight")));

    QVariantMap presets;
    presets.insert(QStringLiteral("header"), header);
    presets.insert(QStringLiteral("row"), row);
    presets.insert(QStringLiteral("compare"), compare);
    return presets;
}

QVariantMap ToolQmlThemeProvider::buildLists() const {
    const QVariantMap palette = colors();
    const QVariantMap textScale = typography();

    QVariantMap sidebarList;
    sidebarList.insert(QStringLiteral("headerHeight"), 40);
    sidebarList.insert(QStringLiteral("rowHeight"), 34);
    sidebarList.insert(QStringLiteral("padding"), 10);
    sidebarList.insert(QStringLiteral("headerPadding"), 16);
    sidebarList.insert(QStringLiteral("titleIconSize"), 18);
    sidebarList.insert(QStringLiteral("background"), palette.value(QStringLiteral("sidebar")));
    sidebarList.insert(QStringLiteral("railBackground"), palette.value(QStringLiteral("rail")));
    sidebarList.insert(QStringLiteral("text"), palette.value(QStringLiteral("textSecondary")));
    sidebarList.insert(QStringLiteral("mutedText"), palette.value(QStringLiteral("textMuted")));
    sidebarList.insert(QStringLiteral("railText"), isDarkTheme() ? QStringLiteral("#F2F2F7") : QStringLiteral("#1D1D1F"));
    sidebarList.insert(QStringLiteral("hoverBackground"), palette.value(QStringLiteral("hover")));
    sidebarList.insert(
        QStringLiteral("selectedBackground"),
        isDarkTheme() ? palette.value(QStringLiteral("accentHover")) : palette.value(QStringLiteral("selection"))
    );
    sidebarList.insert(QStringLiteral("selectedText"), palette.value(QStringLiteral("textInverted")));
    sidebarList.insert(QStringLiteral("compareBackground"), palette.value(QStringLiteral("compareHighlight")));
    sidebarList.insert(QStringLiteral("latestText"), palette.value(QStringLiteral("latest")));
    sidebarList.insert(QStringLiteral("railHoverBackground"), palette.value(QStringLiteral("hover")));
    sidebarList.insert(QStringLiteral("edgeRadius"), 10);
    sidebarList.insert(QStringLiteral("buttonRadius"), 8);
    sidebarList.insert(QStringLiteral("fontFamily"), textScale.value(QStringLiteral("fontFamily")));
    sidebarList.insert(QStringLiteral("fontPixelSize"), textScale.value(QStringLiteral("body")));
    sidebarList.insert(QStringLiteral("headerWeight"), textScale.value(QStringLiteral("weightBold")));

    QVariantMap scrollbar;
    scrollbar.insert(QStringLiteral("size"), 10);
    scrollbar.insert(QStringLiteral("crossSize"), 10);
    scrollbar.insert(QStringLiteral("thumbSize"), 5);
    scrollbar.insert(QStringLiteral("thumbActiveSize"), 6);
    scrollbar.insert(QStringLiteral("thumbCrossSize"), 5);
    scrollbar.insert(QStringLiteral("padding"), 2);
    scrollbar.insert(QStringLiteral("idleOpacity"), 0.42);
    scrollbar.insert(QStringLiteral("trackOpacity"), isDarkTheme() ? 0.34 : 0.44);
    scrollbar.insert(QStringLiteral("track"), isDarkTheme() ? QStringLiteral("#1F000000") : QStringLiteral("#1FFFFFFF"));
    scrollbar.insert(QStringLiteral("thumb"), isDarkTheme() ? QStringLiteral("#78FFFFFF") : QStringLiteral("#703C3C43"));
    scrollbar.insert(QStringLiteral("thumbHover"), isDarkTheme() ? QStringLiteral("#A8FFFFFF") : QStringLiteral("#963C3C43"));
    scrollbar.insert(QStringLiteral("thumbPressed"), isDarkTheme() ? QStringLiteral("#D0FFFFFF") : QStringLiteral("#B83C3C43"));
    scrollbar.insert(QStringLiteral("thumbBorder"), isDarkTheme() ? QStringLiteral("#24FFFFFF") : QStringLiteral("#2AFFFFFF"));

    QVariantMap presets;
    presets.insert(QStringLiteral("sidebar"), sidebarList);
    presets.insert(QStringLiteral("scrollbar"), scrollbar);
    return presets;
}

QVariantMap ToolQmlThemeProvider::buildInputs() const {
    const QVariantMap palette = colors();
    const QVariantMap textScale = typography();

    QVariantMap searchField;
    searchField.insert(QStringLiteral("height"), 38);
    searchField.insert(QStringLiteral("background"), palette.value(QStringLiteral("searchBar")));
    searchField.insert(QStringLiteral("text"), palette.value(QStringLiteral("textPrimary")));
    searchField.insert(QStringLiteral("placeholder"), palette.value(QStringLiteral("textMuted")));
    searchField.insert(QStringLiteral("selection"), palette.value(QStringLiteral("selection")));
    searchField.insert(QStringLiteral("selectedText"), palette.value(QStringLiteral("textInverted")));
    searchField.insert(QStringLiteral("paddingLeft"), 12);
    searchField.insert(QStringLiteral("paddingRight"), 12);
    searchField.insert(QStringLiteral("fontFamily"), textScale.value(QStringLiteral("fontFamily")));
    searchField.insert(QStringLiteral("fontPixelSize"), textScale.value(QStringLiteral("body")));

    QVariantMap presets;
    presets.insert(QStringLiteral("search"), searchField);
    return presets;
}

QVariantMap ToolQmlThemeProvider::buildDividers() const {
    const QVariantMap palette = colors();

    QVariantMap divider;
    divider.insert(QStringLiteral("color"), palette.value(QStringLiteral("border")));
    divider.insert(QStringLiteral("strongColor"), palette.value(QStringLiteral("borderStrong")));
    divider.insert(QStringLiteral("thickness"), 1);

    QVariantMap presets;
    presets.insert(QStringLiteral("default"), divider);
    return presets;
}

QVariantMap ToolQmlThemeProvider::buildFonts() const {
    const QVariantMap textScale = typography();

    QVariantMap presets;
    presets.insert(QStringLiteral("title"), buildFontPreset(textScale.value(QStringLiteral("subheading")).toInt(), textScale.value(QStringLiteral("weightBold")).toInt()));
    presets.insert(QStringLiteral("body"), buildFontPreset(textScale.value(QStringLiteral("body")).toInt(), textScale.value(QStringLiteral("weightNormal")).toInt()));
    presets.insert(QStringLiteral("bodyStrong"), buildFontPreset(textScale.value(QStringLiteral("body")).toInt(), textScale.value(QStringLiteral("weightBold")).toInt()));
    presets.insert(QStringLiteral("small"), buildFontPreset(textScale.value(QStringLiteral("caption")).toInt(), textScale.value(QStringLiteral("weightNormal")).toInt()));
    presets.insert(QStringLiteral("overlay"), buildFontPreset(14, textScale.value(QStringLiteral("weightMedium")).toInt()));
    return presets;
}

QVariantMap ToolQmlThemeProvider::buildFontPreset(int pixelSize, int weight) const {
    const QVariantMap textScale = typography();
    QVariantMap preset;
    preset.insert(QStringLiteral("family"), textScale.value(QStringLiteral("fontFamily")));
    preset.insert(QStringLiteral("pixelSize"), pixelSize);
    preset.insert(QStringLiteral("weight"), weight);
    return preset;
}

void ToolQmlThemeProvider::rebuildThemeCategories() {
    m_categories.insert(QStringLiteral("spacing"), buildSpacing());
    m_categories.insert(QStringLiteral("radius"), buildRadius());
    m_categories.insert(QStringLiteral("typography"), buildTypography());
    m_categories.insert(QStringLiteral("metrics"), buildMetrics());
    m_categories.insert(
        QStringLiteral("colors"),
        isDarkTheme() ? buildDarkColors() : buildLightColors()
    );
    m_categories.insert(QStringLiteral("surfaces"), buildSurfaces());
    m_categories.insert(QStringLiteral("buttons"), buildButtons());
    m_categories.insert(QStringLiteral("tables"), buildTables());
    m_categories.insert(QStringLiteral("lists"), buildLists());
    m_categories.insert(QStringLiteral("inputs"), buildInputs());
    m_categories.insert(QStringLiteral("dividers"), buildDividers());
    m_categories.insert(QStringLiteral("fonts"), buildFonts());
}

QString ToolQmlThemeProvider::normalizeThemeName(const QString& themeName) const {
    return normalizedThemeName(themeName);
}

bool ToolQmlThemeProvider::isDarkTheme() const {
    return m_theme == QStringLiteral("dark");
}
