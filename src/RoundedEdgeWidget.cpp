//-------------------------------------------------------------------------------------
// RoundedEdgeWidget.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "RoundedEdgeWidget.h"
#include <QPainter>
#include <QPainterPath>

RoundedEdgeWidget::RoundedEdgeWidget(Edge edge, QWidget* parent)
    : QWidget(parent)
    , m_edge(edge)
    , m_backgroundColor(Qt::white)
{
    // This widget is transparent and only draws rounded corners
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);  // Let mouse events pass through
    setFixedWidth(RADIUS);
}

void RoundedEdgeWidget::setBackgroundColor(const QColor& color) {
    m_backgroundColor = color;
    update();
}

void RoundedEdgeWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw the background color with rounded corners
    QPainterPath path;
    
    if (m_edge == LeftEdge) {
        // Draw left edge with rounded top-left and bottom-left corners
        path.moveTo(RADIUS, 0);
        path.lineTo(RADIUS, height());
        path.lineTo(0, height());
        path.arcTo(0, height() - RADIUS * 2, RADIUS * 2, RADIUS * 2, 270, -90);
        path.lineTo(0, RADIUS);
        path.arcTo(0, 0, RADIUS * 2, RADIUS * 2, 180, -90);
        path.closeSubpath();
    } else {
        // Draw right edge with rounded top-right and bottom-right corners
        path.moveTo(0, 0);
        path.lineTo(0, height());
        path.lineTo(RADIUS, height());
        path.arcTo(RADIUS - RADIUS * 2, height() - RADIUS * 2, RADIUS * 2, RADIUS * 2, 270, 90);
        path.lineTo(RADIUS, RADIUS);
        path.arcTo(RADIUS - RADIUS * 2, 0, RADIUS * 2, RADIUS * 2, 0, 90);
        path.closeSubpath();
    }
    
    painter.fillPath(path, m_backgroundColor);
}
