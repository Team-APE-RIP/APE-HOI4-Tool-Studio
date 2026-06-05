//-------------------------------------------------------------------------------------
// CustomMessageBox.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "CustomMessageBox.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "OverlayAcrylicMaterial.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QPainter>

CustomMessageBox::CustomMessageBox(QWidget *parent, const QString &title, const QString &message, Type type)
    : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowModality(Qt::WindowModal);
    OverlayAcrylicMaterial::installLiveRefresh(this);
    setupUi(title, message, type);
    
    // Theme will be handled in paintEvent and setStyleSheet for children
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    QString text = isDark ? "#FFFFFF" : "#1D1D1F";
    
    setStyleSheet(QString(R"(
        QLabel { color: %1; }
        QPushButton {
            background-color: transparent; color: white; border: none; padding: 8px 16px; font-weight: bold;
        }
        QPushButton:hover { background-color: transparent; }
        QPushButton#CancelBtn {
            background-color: transparent; color: %1; border: none;
        }
        QPushButton#CancelBtn:hover { background-color: transparent; }
    )").arg(text, isDark ? "#3A3A3C" : "#F5F5F7", isDark ? "#48484A" : "#D2D2D7", isDark ? "#48484A" : "#E5E5EA"));
}

void CustomMessageBox::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    OverlayAcrylicMaterial::paintPanel(
        painter,
        this,
        QRectF(rect()),
        10.0,
        ConfigManager::instance().isCurrentThemeDark(),
        true);
}

void CustomMessageBox::setupUi(const QString &title, const QString &message, Type type) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(20);

    QLabel *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    layout->addWidget(titleLabel);

    QLabel *msgLabel = new QLabel(message);
    msgLabel->setWordWrap(true);
    msgLabel->setStyleSheet("font-size: 14px;");
    layout->addWidget(msgLabel);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    LocalizationManager& loc = LocalizationManager::instance();

    if (type == Question) {
        QPushButton *cancelBtn = new OverlayAcrylicButton(OverlayAcrylicButton::Role::Secondary, this);
        cancelBtn->setText(loc.getString("Common", "Cancel"));
        cancelBtn->setObjectName("CancelBtn");
        cancelBtn->setCursor(Qt::PointingHandCursor);
        cancelBtn->setMinimumHeight(32);
        connect(cancelBtn, &QPushButton::clicked, [this](){ 
            m_result = QMessageBox::No; 
            reject(); 
        });
        btnLayout->addWidget(cancelBtn);

        QPushButton *yesBtn = new OverlayAcrylicButton(OverlayAcrylicButton::Role::Accent, this);
        yesBtn->setText(loc.getString("Common", "Yes"));
        yesBtn->setCursor(Qt::PointingHandCursor);
        yesBtn->setMinimumHeight(32);
        connect(yesBtn, &QPushButton::clicked, [this](){ 
            m_result = QMessageBox::Yes; 
            accept(); 
        });
        btnLayout->addWidget(yesBtn);
    } else {
        QPushButton *okBtn = new OverlayAcrylicButton(OverlayAcrylicButton::Role::Accent, this);
        okBtn->setText(loc.getString("Common", "OK"));
        okBtn->setCursor(Qt::PointingHandCursor);
        okBtn->setMinimumHeight(32);
        connect(okBtn, &QPushButton::clicked, [this](){ 
            m_result = QMessageBox::Ok; 
            accept(); 
        });
        btnLayout->addWidget(okBtn);
    }
    
    layout->addLayout(btnLayout);
}

void CustomMessageBox::information(QWidget *parent, const QString &title, const QString &message) {
    CustomMessageBox box(parent, title, message, Information);
    box.adjustSize();
    // Center on parent or screen
    if (parent) {
        QPoint parentCenter = parent->mapToGlobal(parent->rect().center());
        box.move(parentCenter.x() - box.width() / 2, parentCenter.y() - box.height() / 2);
    }
    // Ensure dialog is on top
    box.raise();
    box.activateWindow();
    box.exec();
}

QMessageBox::StandardButton CustomMessageBox::question(QWidget *parent, const QString &title, const QString &message) {
    CustomMessageBox box(parent, title, message, Question);
    box.adjustSize();
    // Center on parent or screen
    if (parent) {
        QPoint parentCenter = parent->mapToGlobal(parent->rect().center());
        box.move(parentCenter.x() - box.width() / 2, parentCenter.y() - box.height() / 2);
    }
    // Ensure dialog is on top
    box.raise();
    box.activateWindow();
    box.exec();
    return box.m_result;
}
