//-------------------------------------------------------------------------------------
// OverlayAcrylicMaterial.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef OVERLAYACRYLICMATERIAL_H
#define OVERLAYACRYLICMATERIAL_H

#include <QImage>
#include <QPushButton>
#include <QWidget>

class QPainter;

namespace OverlayAcrylicMaterial {
    QImage materialImage(QWidget* target, const QRect& localRect, bool isDark, const QSize& targetSize = QSize());
    void paintOverlayBackdrop(QPainter& painter, QWidget* target, const QRectF& rect, qreal radius, bool isDark, int dimAlpha);
    void paintPanel(QPainter& painter, QWidget* target, const QRectF& rect, qreal radius, bool isDark, bool elevated);
    QString accentGlassBrush(bool isDark);
    QString accentGlassHoverBrush(bool isDark);
    void installLiveRefresh(QWidget* target, int intervalMs = 33);
}

class OverlayAcrylicPanel : public QWidget {
public:
    explicit OverlayAcrylicPanel(QWidget* parent = nullptr);

    void setAcrylicRadius(qreal radius);
    void setElevatedAcrylic(bool elevated);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    qreal m_radius = 12.0;
    bool m_elevated = true;
};

class OverlayAcrylicButton : public QPushButton {
public:
    enum class Role {
        Accent,
        Secondary,
        Danger
    };

    explicit OverlayAcrylicButton(Role role, QWidget* parent = nullptr);

    void setAcrylicRole(Role role);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Role m_role = Role::Accent;
};

#endif // OVERLAYACRYLICMATERIAL_H
