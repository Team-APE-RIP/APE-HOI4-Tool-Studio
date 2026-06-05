//-------------------------------------------------------------------------------------
// ToolQmlThemeProvider.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLQMLTHEMEPROVIDER_H
#define TOOLQMLTHEMEPROVIDER_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantMap>

class ToolQmlThemeProvider : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap surfaces READ surfaces NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap spacing READ spacing NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap radius READ radius NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap typography READ typography NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap buttons READ buttons NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap tables READ tables NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap lists READ lists NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap inputs READ inputs NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap dividers READ dividers NOTIFY tokensChanged)
    Q_PROPERTY(QVariantMap fonts READ fonts NOTIFY tokensChanged)

public:
    explicit ToolQmlThemeProvider(QObject* parent = nullptr);
    ~ToolQmlThemeProvider() override = default;

    QString theme() const;
    QVariantMap colors() const;
    QVariantMap surfaces() const;
    QVariantMap spacing() const;
    QVariantMap radius() const;
    QVariantMap typography() const;
    QVariantMap metrics() const;
    QVariantMap buttons() const;
    QVariantMap tables() const;
    QVariantMap lists() const;
    QVariantMap inputs() const;
    QVariantMap dividers() const;
    QVariantMap fonts() const;

    void setTheme(const QString& themeName);

    Q_INVOKABLE QVariantMap tokens(const QString& category) const;
    Q_INVOKABLE QVariant token(const QString& category,
                               const QString& key,
                               const QVariant& fallback = QVariant()) const;

signals:
    void themeChanged();
    void tokensChanged();

private:
    QVariantMap category(const QString& name) const;
    QVariantMap buildLightColors() const;
    QVariantMap buildDarkColors() const;
    QVariantMap buildSurfaces() const;
    QVariantMap buildSpacing() const;
    QVariantMap buildRadius() const;
    QVariantMap buildTypography() const;
    QVariantMap buildMetrics() const;
    QVariantMap buildButtons() const;
    QVariantMap buildTables() const;
    QVariantMap buildLists() const;
    QVariantMap buildInputs() const;
    QVariantMap buildDividers() const;
    QVariantMap buildFonts() const;
    QVariantMap buildFontPreset(int pixelSize, int weight) const;
    void rebuildThemeCategories();
    QString normalizeThemeName(const QString& themeName) const;
    bool isDarkTheme() const;

    QString m_theme;
    QMap<QString, QVariantMap> m_categories;
};

#endif // TOOLQMLTHEMEPROVIDER_H
