//-------------------------------------------------------------------------------------
// RoundedEdgeWidget.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef ROUNDEDEDGEWIDGET_H
#define ROUNDEDEDGEWIDGET_H

#include <QWidget>

class RoundedEdgeWidget : public QWidget {
    Q_OBJECT

public:
    enum Edge {
        LeftEdge,
        RightEdge
    };

    explicit RoundedEdgeWidget(Edge edge, QWidget* parent = nullptr);

    void setBackgroundColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Edge m_edge;
    QColor m_backgroundColor;
    static constexpr int RADIUS = 10;
};

#endif // ROUNDEDEDGEWIDGET_H
