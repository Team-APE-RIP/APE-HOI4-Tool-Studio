//-------------------------------------------------------------------------------------
// ToolUiContainer.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolUiContainer.h"
#include "Logger.h"

#include <QVBoxLayout>
#include <QEvent>
#include <QShowEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QSizePolicy>

namespace {
const char* kLogContext = "ToolUiContainer";
constexpr bool kVerboseToolUiContainerLogging = false;
}

ToolUiContainer::ToolUiContainer(QWidget* parent)
    : QWidget(parent)
{
    if (kVerboseToolUiContainerLogging) {
        Logger::instance().logInfo(kLogContext, "Creating ToolUiContainer with attribute isolation");
    }
    
    // CRITICAL: Explicitly disable problematic attributes
    // These attributes cause mouse event penetration on Windows
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    
    // Keep input isolation while allowing the parent acrylic material to show through.
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Setup layout
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    setLayout(m_layout);
    
    if (kVerboseToolUiContainerLogging) {
        Logger::instance().logInfo(kLogContext, QString("ToolUiContainer created: ptr=%1, parent=%2")
            .arg(reinterpret_cast<quintptr>(this), 0, 16)
            .arg(reinterpret_cast<quintptr>(parent), 0, 16));
    }
}

ToolUiContainer::~ToolUiContainer() {
    if (kVerboseToolUiContainerLogging) {
        Logger::instance().logInfo(kLogContext, "Destroying ToolUiContainer");
    }
}

void ToolUiContainer::setToolWidget(QWidget* toolWidget) {
    if (kVerboseToolUiContainerLogging) {
        Logger::instance().logInfo(kLogContext, QString("setToolWidget called: old=%1, new=%2")
            .arg(reinterpret_cast<quintptr>(m_toolWidget), 0, 16)
            .arg(reinterpret_cast<quintptr>(toolWidget), 0, 16));
    }
    
    // Remove old widget if exists
    if (m_toolWidget) {
        m_layout->removeWidget(m_toolWidget);
        // Don't delete - caller manages lifecycle
        m_toolWidget = nullptr;
    }
    
    // Add new widget
    if (toolWidget) {
        m_toolWidget = toolWidget;
        m_toolWidget->setParent(this);
        m_layout->addWidget(m_toolWidget);
        
        // CRITICAL: Ensure tool widget also has proper attributes
        m_toolWidget->setAttribute(Qt::WA_TranslucentBackground, false);
        m_toolWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        m_toolWidget->setEnabled(true);
        
        if (kVerboseToolUiContainerLogging) {
            Logger::instance().logInfo(kLogContext, QString("Tool widget configured: enabled=%1, transparentForMouse=%2")
                .arg(m_toolWidget->isEnabled())
                .arg(m_toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents)));
        }
    }
}

void ToolUiContainer::clearToolWidget() {
    if (kVerboseToolUiContainerLogging) {
        Logger::instance().logInfo(kLogContext, "clearToolWidget called");
    }
    
    if (m_toolWidget) {
        m_layout->removeWidget(m_toolWidget);
        m_toolWidget = nullptr;
    }
}

void ToolUiContainer::enforceAttributeIsolation() {
    // CRITICAL: Re-enforce attribute isolation
    // Parent widget changes or Qt internal operations might reset these
    bool wasTranslucent = testAttribute(Qt::WA_TranslucentBackground);
    bool wasTransparent = testAttribute(Qt::WA_TransparentForMouseEvents);
    
    if (wasTranslucent || wasTransparent) {
        Logger::instance().logWarning(kLogContext, 
            QString("Attributes were reset! translucent=%1, transparent=%2 - re-enforcing isolation")
            .arg(wasTranslucent)
            .arg(wasTransparent));
        
        setAttribute(Qt::WA_TranslucentBackground, false);
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
    }
    
    // Also enforce on tool widget
    if (m_toolWidget) {
        bool toolWasTranslucent = m_toolWidget->testAttribute(Qt::WA_TranslucentBackground);
        bool toolWasTransparent = m_toolWidget->testAttribute(Qt::WA_TransparentForMouseEvents);
        
        if (toolWasTranslucent || toolWasTransparent) {
            Logger::instance().logWarning(kLogContext, 
                QString("Tool widget attributes were reset! translucent=%1, transparent=%2 - re-enforcing")
                .arg(toolWasTranslucent)
                .arg(toolWasTransparent));
            
            m_toolWidget->setAttribute(Qt::WA_TranslucentBackground, false);
            m_toolWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        }
    }
}

bool ToolUiContainer::event(QEvent* event) {
    // Intercept events that might change attributes.
    if (event->type() == QEvent::ParentChange ||
        event->type() == QEvent::Polish ||
        event->type() == QEvent::PolishRequest) {

        if (kVerboseToolUiContainerLogging) {
            Logger::instance().logInfo(kLogContext, QString("Intercepted event type=%1, enforcing attribute isolation")
                .arg(event->type()));
        }

        const bool result = QWidget::event(event);
        enforceAttributeIsolation();
        return result;
    }

    const bool result = QWidget::event(event);
    if (event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonRelease
        || event->type() == QEvent::MouseButtonDblClick
        || event->type() == QEvent::MouseMove
        || event->type() == QEvent::Wheel) {
        event->accept();
    }
    return result;
}

void ToolUiContainer::mousePressEvent(QMouseEvent* event) {
    if (event) {
        if (kVerboseToolUiContainerLogging) {
            Logger::instance().logInfo(kLogContext, QStringLiteral("Absorbing unhandled mouse press at tool container boundary"));
        }
        event->accept();
    }
}

void ToolUiContainer::mouseReleaseEvent(QMouseEvent* event) {
    if (event) {
        event->accept();
    }
}

void ToolUiContainer::mouseMoveEvent(QMouseEvent* event) {
    if (event) {
        event->accept();
    }
}

void ToolUiContainer::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event) {
        event->accept();
    }
}

void ToolUiContainer::wheelEvent(QWheelEvent* event) {
    if (event) {
        event->accept();
    }
}

void ToolUiContainer::showEvent(QShowEvent* event) {
    if (kVerboseToolUiContainerLogging) {
        Logger::instance().logInfo(kLogContext, "showEvent - enforcing attribute isolation");
    }
    
    // Enforce attributes before showing
    enforceAttributeIsolation();
    
    QWidget::showEvent(event);
    
    // Enforce again after showing (Qt might have changed them)
    enforceAttributeIsolation();
    
    if (kVerboseToolUiContainerLogging) {
        Logger::instance().logInfo(kLogContext, QString("Container shown: visible=%1, enabled=%2, geometry=%3x%4")
            .arg(isVisible())
            .arg(isEnabled())
            .arg(width())
            .arg(height()));
    }
}

void ToolUiContainer::resizeEvent(QResizeEvent* event) {
    // CRITICAL: Enforce attribute isolation on resize
    // Parent window's setMask() in resizeEvent might affect child widget attributes
    enforceAttributeIsolation();
    
    QWidget::resizeEvent(event);
    
    // Enforce again after resize
    enforceAttributeIsolation();
}
