//-------------------------------------------------------------------------------------
// SetupDialog.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "SetupDialog.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "PathValidator.h"
#include "CustomMessageBox.h"
#include "Logger.h"
#include "OverlayAcrylicMaterial.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QFileDialog>
#include <QScreen>
#include <QApplication>

SetupDialog::SetupDialog(QWidget *parent) 
    : QWidget(parent), m_isDarkMode(ConfigManager::instance().isCurrentThemeDark()) {
    
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    OverlayAcrylicMaterial::installLiveRefresh(this);
    
    hide();
    
    setupUi();
    updateTheme();
    updateTexts();
    
    // Load existing config
    ConfigManager& config = ConfigManager::instance();
    if (!config.getModPath().isEmpty()) {
        m_modPathEdit->setText(config.getModPath());
    }
    
    // Connect real-time save signals
    connect(m_modPathEdit, &QLineEdit::textChanged, this, &SetupDialog::onModPathChanged);
    
    if (parent) {
        parent->installEventFilter(this);
    }
}

void SetupDialog::setupUi() {
    // Container for content (centered)
    m_container = new OverlayAcrylicPanel(this);
    m_container->setObjectName("SetupContainer");
    m_container->setFixedSize(500, 500);
    
    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(40, 20, 40, 20);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignCenter);
    
    // Title (moved to top)
    m_titleLabel = new QLabel(m_container);
    m_titleLabel->setObjectName("SetupTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel);
    
    layout->addSpacing(10);
    
    // App Icon (centered, 256x256)
    m_iconLabel = new QLabel(m_container);
    m_iconLabel->setPixmap(QIcon(":/app.ico").pixmap(256, 256));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setFixedSize(256, 256);
    layout->addWidget(m_iconLabel);
    layout->setAlignment(m_iconLabel, Qt::AlignHCenter);
    
    layout->addSpacing(10);
    
    // Mod Path
    QVBoxLayout *modLayout = new QVBoxLayout();
    modLayout->setSpacing(8);
    
    m_modLabel = new QLabel(m_container);
    m_modLabel->setObjectName("ModLabel");
    modLayout->addWidget(m_modLabel);
    
    QHBoxLayout *modInputLayout = new QHBoxLayout();
    modInputLayout->setSpacing(8);
    
    m_modPathEdit = new QLineEdit(m_container);
    m_modPathEdit->setObjectName("ModPathEdit");
    m_modPathEdit->setPlaceholderText("Select your Mod folder...");
    
    m_browseModBtn = new OverlayAcrylicButton(OverlayAcrylicButton::Role::Secondary, m_container);
    m_browseModBtn->setObjectName("BrowseButton");
    m_browseModBtn->setCursor(Qt::PointingHandCursor);
    m_browseModBtn->setFixedWidth(80);
    connect(m_browseModBtn, &QPushButton::clicked, this, &SetupDialog::browseModPath);
    
    modInputLayout->addWidget(m_modPathEdit);
    modInputLayout->addWidget(m_browseModBtn);
    modLayout->addLayout(modInputLayout);
    layout->addLayout(modLayout);
    
    layout->addStretch();
    
    // Confirm Button
    m_confirmBtn = new OverlayAcrylicButton(OverlayAcrylicButton::Role::Accent, m_container);
    m_confirmBtn->setObjectName("ConfirmButton");
    m_confirmBtn->setCursor(Qt::PointingHandCursor);
    m_confirmBtn->setFixedHeight(45);
    connect(m_confirmBtn, &QPushButton::clicked, this, &SetupDialog::validateAndAccept);
    layout->addWidget(m_confirmBtn);
}

void SetupDialog::updateTheme() {
    m_isDarkMode = ConfigManager::instance().isCurrentThemeDark();
    
    QString textColor = m_isDarkMode ? "#FFFFFF" : "#1D1D1F";
    QString borderColor = m_isDarkMode ? "#3A3A3C" : "#D2D2D7";
    QString inputBg = m_isDarkMode ? "rgba(58, 58, 60, 0.54)" : "rgba(255, 255, 255, 0.50)";
    QString accentGlass = OverlayAcrylicMaterial::accentGlassBrush(m_isDarkMode);
    
    m_container->setStyleSheet(QString(
        "QWidget#SetupContainer {"
        "  background-color: transparent;"
        "  border: none;"
        "}"
    ));
    
    m_titleLabel->setStyleSheet(QString(
        "QLabel#SetupTitle {"
        "  color: %1;"
        "  font-size: 20px;"
        "  font-weight: bold;"
        "}"
    ).arg(textColor));
    
    m_modLabel->setStyleSheet(QString(
        "QLabel#ModLabel {"
        "  color: %1;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
    ).arg(textColor));
    
    m_modPathEdit->setStyleSheet(QString(
        "QLineEdit#ModPathEdit {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  padding: 10px 12px;"
        "  background-color: %2;"
        "  color: %3;"
        "  selection-background-color: %4;"
        "  selection-color: #FFFFFF;"
        "}"
    ).arg(borderColor, inputBg, textColor, accentGlass));
    
    m_browseModBtn->setStyleSheet(QString(
        "QPushButton#BrowseButton {"
        "  background-color: transparent;"
        "  color: %1;"
        "  border: none;"
        "  padding: 10px 16px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#BrowseButton:hover {"
        "  background-color: transparent;"
        "}"
    ).arg(textColor));
    
    m_confirmBtn->setStyleSheet(QString(
        "QPushButton#ConfirmButton {"
        "  background-color: transparent;"
        "  color: white;"
        "  border: none;"
        "  padding: 12px 30px;"
        "  font-weight: 600;"
        "  font-size: 15px;"
        "}"
        "QPushButton#ConfirmButton:hover {"
        "  background-color: transparent;"
        "}"
        "QPushButton#ConfirmButton:pressed {"
        "  background-color: transparent;"
        "}"
    ));
}

void SetupDialog::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    m_titleLabel->setText(loc.getString("SetupDialog", "TitleLabel"));
    m_modLabel->setText(loc.getString("SetupDialog", "ModLabel"));
    
    m_modPathEdit->setPlaceholderText(loc.getString("SetupDialog", "ModPlaceholder"));
    
    m_confirmBtn->setText(loc.getString("SetupDialog", "ConfirmButton"));
    m_browseModBtn->setText(loc.getString("SetupDialog", "BrowseButton"));
}

void SetupDialog::showOverlay() {
    updateTheme();
    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
    activateWindow();
    setFocus();
}

void SetupDialog::hideOverlay() {
    hide();
}

void SetupDialog::updatePosition() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_container->move(
            (width() - m_container->width()) / 2,
            (height() - m_container->height()) / 2
        );
    }
}

void SetupDialog::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    OverlayAcrylicMaterial::paintOverlayBackdrop(
        painter,
        this,
        QRectF(rect()),
        9.0,
        ConfigManager::instance().isCurrentThemeDark(),
        120);
}

bool SetupDialog::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QWidget::eventFilter(obj, event);
}

void SetupDialog::browseModPath() {
    LocalizationManager& loc = LocalizationManager::instance();
    QString dir = QFileDialog::getExistingDirectory(this, 
        loc.getString("SetupDialog", "SelectModDir"),
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!dir.isEmpty()) {
        m_modPathEdit->setText(dir);
        Logger::instance().logClick("SetupBrowseModPath");
    }
}

void SetupDialog::onModPathChanged(const QString &path) {
    if (!path.isEmpty()) {
        ConfigManager::instance().setModPath(path);
        Logger::instance().logInfo("SetupDialog", "Mod path saved: " + path);
    }
}

void SetupDialog::validateAndAccept() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    // Check if paths are empty
    if (m_modPathEdit->text().isEmpty()) {
        CustomMessageBox::information(nullptr, 
            loc.getString("SetupDialog", "ErrorTitle"), 
            loc.getString("SetupDialog", "ErrorMsg"));
        Logger::instance().logError("SetupDialog", "Validation failed: Empty mod path");
        return;
    }
    
    // Validate game path
    const QString resolvedGamePath = PathValidator::instance().ensureGamePathDiscovered();
    if (resolvedGamePath.isEmpty()) {
        CustomMessageBox::information(nullptr,
            loc.getString("MainWindow", "GameStartupFailedTitle"),
            loc.getString("MainWindow", "GameStartupFailedMsg"));
        Logger::instance().logError("SetupDialog", "Game path discovery failed during setup validation.");
        return;
    }

    QString gameError = PathValidator::instance().validateGamePath(ConfigManager::instance().getGamePath());
    if (!gameError.isEmpty()) {
        CustomMessageBox::information(nullptr, 
            loc.getString("Error", "GamePathInvalid"), 
            loc.getString("Error", gameError));
        Logger::instance().logError("SetupDialog", "Game path validation failed: " + gameError);
        return;
    }
    
    // Validate mod path
    QString modError = PathValidator::instance().validateModPath(m_modPathEdit->text());
    if (!modError.isEmpty()) {
        CustomMessageBox::information(nullptr, 
            loc.getString("Error", "ModPathInvalid"), 
            loc.getString("Error", modError));
        Logger::instance().logError("SetupDialog", "Mod path validation failed: " + modError);
        return;
    }
    
    const QString docError = PathValidator::instance().validateDocPath(ConfigManager::instance().getDocPath());
    if (!docError.isEmpty()) {
        CustomMessageBox::information(nullptr,
            loc.getString("Error", "DocPathInvalid"),
            loc.getString("Error", docError));
        Logger::instance().logError("SetupDialog", "Doc path validation failed: " + docError);
        return;
    }
    
    Logger::instance().logClick("SetupConfirm");
    
    // Save final config
    ConfigManager& config = ConfigManager::instance();
    config.setModPath(m_modPathEdit->text());
    
    emit setupCompleted();
    hideOverlay();
}

QString SetupDialog::getGamePath() const {
    return ConfigManager::instance().getGamePath();
}

QString SetupDialog::getModPath() const {
    return m_modPathEdit->text();
}

bool SetupDialog::isConfigValid() {
    ConfigManager& config = ConfigManager::instance();
    
    // Check if game path exists and is valid
    QString gamePath = config.getGamePath();
    if (gamePath.isEmpty()) {
        return false;
    }
    
    QString gameError = PathValidator::instance().validateGamePath(gamePath);
    if (!gameError.isEmpty()) {
        return false;
    }
    
    QString docError = PathValidator::instance().validateDocPath(ConfigManager::instance().getDocPath());
    if (!docError.isEmpty()) {
        return false;
    }
    
    // Check if mod path exists and is valid
    QString modPath = config.getModPath();
    if (modPath.isEmpty()) {
        return false;
    }
    
    QString modError = PathValidator::instance().validateModPath(modPath);
    if (!modError.isEmpty()) {
        return false;
    }
    
    return true;
}
