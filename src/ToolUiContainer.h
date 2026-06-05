//-------------------------------------------------------------------------------------
// ToolUiContainer.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLUICONTAINER_H
#define TOOLUICONTAINER_H

#include <QWidget>

class QMouseEvent;
class QVBoxLayout;
class QWheelEvent;

/**
 * @brief Attribute-isolating container for tool UI
 *
 * This class solves the UI penetration problem by explicitly blocking
 * accidental translucent/input-transparent attributes on embedded tools.
 *
 * MainWindow must never enable Qt::WA_TranslucentBackground. The acrylic
 * material is drawn inside an opaque main window, while tool widgets are kept
 * visually transparent enough for that material to show through.
 *
 * Solution: This container explicitly disables the problematic attributes
 * and ensures they cannot be re-enabled by parent widget changes.
 */
class ToolUiContainer : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a new Tool UI Container
     * @param parent The parent widget (typically MainWindow's content area)
     */
    explicit ToolUiContainer(QWidget* parent = nullptr);
    ~ToolUiContainer() override;

    /**
     * @brief Set the tool widget to display
     * @param toolWidget The widget containing the tool UI (takes ownership)
     */
    void setToolWidget(QWidget* toolWidget);

    /**
     * @brief Get the current tool widget
     * @return The tool widget or nullptr if none set
     */
    QWidget* toolWidget() const { return m_toolWidget; }

    /**
     * @brief Clear the current tool widget
     */
    void clearToolWidget();

protected:
    // Override to enforce attribute isolation and stop unhandled tool input from reaching MainWindow.
    bool event(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void enforceAttributeIsolation();

    QVBoxLayout* m_layout = nullptr;
    QWidget* m_toolWidget = nullptr;
};

#endif // TOOLUICONTAINER_H
