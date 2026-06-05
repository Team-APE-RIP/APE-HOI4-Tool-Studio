//-------------------------------------------------------------------------------------
// ScreenAcrylicBackdrop.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef SCREENACRYLICBACKDROP_H
#define SCREENACRYLICBACKDROP_H

#include <QColor>
#include <QImage>
#include <QTimer>
#include <QWidget>

class QHideEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;

class ScreenAcrylicBackdrop : public QWidget {
    Q_OBJECT

public:
    explicit ScreenAcrylicBackdrop(QWidget *parent = nullptr);
    ~ScreenAcrylicBackdrop() override;

    void setDarkMode(bool isDark);
    void setChromeColors(const QColor& tint, const QColor& border);
    void setChromeCornerRadius(qreal radius);
    Q_INVOKABLE QImage acrylicImageForGlobalRect(const QRect& globalRect, const QSize& targetSize) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refreshBackdrop();
    QImage captureScreenGrid(int gridWidth, int gridHeight) const;
    QImage processAcrylicGrid(const QImage& source) const;
    QImage weightedBlur(const QImage& source, int passes) const;
    QSize gridSizeForCurrentWindow() const;
    void applyCaptureExclusion();
    void restoreCaptureExclusion();

    QTimer m_refreshTimer;
    QImage m_acrylicGrid;
    QColor m_tintColor;
    QColor m_borderColor;
    qreal m_chromeCornerRadius = 10.0;
    quint32 m_previousDisplayAffinity = 0;
    quintptr m_captureWindowId = 0;
    bool m_hadDisplayAffinity = false;
    bool m_captureExclusionActive = false;
    bool m_captureExclusionUnavailable = false;
    bool m_isDark = false;
};

#endif // SCREENACRYLICBACKDROP_H
