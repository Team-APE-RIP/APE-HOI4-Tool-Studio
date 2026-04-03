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
    
    hide();
    
    setupUi();
    updateTheme();
    updateTexts();
    
    // Load existing config
    ConfigManager& config = ConfigManager::instance();
    if (!config.getGamePath().isEmpty()) {
        m_gamePathEdit->setText(config.getGamePath());
    }
    if (!config.getDocPath().isEmpty()) {
        m_docPathEdit->setText(config.getDocPath());
    }
    if (!config.getModPath().isEmpty()) {
        m_modPathEdit->setText(config.getModPath());
    }
    
    // Connect real-time save signals
    connect(m_gamePathEdit, &QLineEdit::textChanged, this, &SetupDialog::onGamePathChanged);
    connect(m_docPathEdit, &QLineEdit::textChanged, this, &SetupDialog::onDocPathChanged);
    connect(m_modPathEdit, &QLineEdit::textChanged, this, &SetupDialog::onModPathChanged);
    
    if (parent) {
        parent->installEventFilter(this);
    }
}

void SetupDialog::setupUi() {
    // Container for content (centered)
    m_container = new QWidget(this);
    m_container->setObjectName("SetupContainer");
    m_container->setFixedSize(500, 630);
    
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
    
    // Game Path
    QVBoxLayout *gameLayout = new QVBoxLayout();
    gameLayout->setSpacing(8);
    
    m_gameLabel = new QLabel(m_container);
    m_gameLabel->setObjectName("GameLabel");
    gameLayout->addWidget(m_gameLabel);
    
    QHBoxLayout *gameInputLayout = new QHBoxLayout();
    gameInputLayout->setSpacing(8);
    
    m_gamePathEdit = new QLineEdit(m_container);
    m_gamePathEdit->setObjectName("GamePathEdit");
    m_gamePathEdit->setPlaceholderText("Select HOI4 installation folder...");
    
    m_browseGameBtn = new QPushButton(m_container);
    m_browseGameBtn->setObjectName("BrowseButton");
    m_browseGameBtn->setCursor(Qt::PointingHandCursor);
    m_browseGameBtn->setFixedWidth(80);
    connect(m_browseGameBtn, &QPushButton::clicked, this, &SetupDialog::browseGamePath);
    
    gameInputLayout->addWidget(m_gamePathEdit);
    gameInputLayout->addWidget(m_browseGameBtn);
    gameLayout->addLayout(gameInputLayout);
    layout->addLayout(gameLayout);
    
    // Doc Path
    QVBoxLayout *docLayout = new QVBoxLayout();
    docLayout->setSpacing(8);
    
    m_docLabel = new QLabel(m_container);
    m_docLabel->setObjectName("DocLabel");
    docLayout->addWidget(m_docLabel);
    
    QHBoxLayout *docInputLayout = new QHBoxLayout();
    docInputLayout->setSpacing(8);
    
    m_docPathEdit = new QLineEdit(m_container);
    m_docPathEdit->setObjectName("DocPathEdit");
    m_docPathEdit->setPlaceholderText("Select your Hearts of Iron IV documents folder...");
    
    m_browseDocBtn = new QPushButton(m_container);
    m_browseDocBtn->setObjectName("BrowseButton");
    m_browseDocBtn->setCursor(Qt::PointingHandCursor);
    m_browseDocBtn->setFixedWidth(80);
    connect(m_browseDocBtn, &QPushButton::clicked, this, &SetupDialog::browseDocPath);
    
    docInputLayout->addWidget(m_docPathEdit);
    docInputLayout->addWidget(m_browseDocBtn);
    docLayout->addLayout(docInputLayout);
    layout->addLayout(docLayout);
    
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
    
    m_browseModBtn = new QPushButton(m_container);
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
    m_confirmBtn = new QPushButton(m_container);
    m_confirmBtn->setObjectName("ConfirmButton");
    m_confirmBtn->setCursor(Qt::PointingHandCursor);
    m_confirmBtn->setFixedHeight(45);
    connect(m_confirmBtn, &QPushButton::clicked, this, &SetupDialog::validateAndAccept);
    layout->addWidget(m_confirmBtn);
}

void SetupDialog::updateTheme() {
    m_isDarkMode = ConfigManager::instance().isCurrentThemeDark();
    
    QString containerBg = m_isDarkMode ? "#2C2C2E" : "#FFFFFF";
    QString textColor = m_isDarkMode ? "#FFFFFF" : "#1D1D1F";
    QString borderColor = m_isDarkMode ? "#3A3A3C" : "#D2D2D7";
    QString inputBg = m_isDarkMode ? "#3A3A3C" : "#FFFFFF";
    QString btnBg = m_isDarkMode ? "#0A84FF" : "#007AFF";
    QString btnHoverBg = m_isDarkMode ? "#0070E0" : "#0062CC";
    QString browseBtnBg = m_isDarkMode ? "#3A3A3C" : "#E5E5EA";
    QString browseBtnHoverBg = m_isDarkMode ? "#4A4A4C" : "#D1D1D6";
    QString browseBtnText = m_isDarkMode ? "#0A84FF" : "#007AFF";
    QString itemHover = m_isDarkMode ? "#3A3A3C" : "rgba(0, 0, 0, 0.05)";
    QString comboIndicator = m_isDarkMode ? "#FFFFFF" : "#1D1D1F";
    
    m_container->setStyleSheet(QString(
        "QWidget#SetupContainer {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(containerBg, borderColor));
    
    m_titleLabel->setStyleSheet(QString(
        "QLabel#SetupTitle {"
        "  color: %1;"
        "  font-size: 20px;"
        "  font-weight: bold;"
        "}"
    ).arg(textColor));
    
    m_gameLabel->setStyleSheet(QString(
        "QLabel#GameLabel {"
        "  color: %1;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
    ).arg(textColor));
    
    m_docLabel->setStyleSheet(QString(
        "QLabel#DocLabel {"
        "  color: %1;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
    ).arg(textColor));
    
    m_modLabel->setStyleSheet(QString(
        "QLabel#ModLabel {"
        "  color: %1;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
    ).arg(textColor));
    
    m_gamePathEdit->setStyleSheet(QString(
        "QLineEdit#GamePathEdit {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  padding: 10px 12px;"
        "  background-color: %2;"
        "  color: %3;"
        "  selection-background-color: #007AFF;"
        "}"
    ).arg(borderColor, inputBg, textColor));
    
    m_docPathEdit->setStyleSheet(QString(
        "QLineEdit#DocPathEdit {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  padding: 10px 12px;"
        "  background-color: %2;"
        "  color: %3;"
        "  selection-background-color: #007AFF;"
        "}"
    ).arg(borderColor, inputBg, textColor));
    
    m_modPathEdit->setStyleSheet(QString(
        "QLineEdit#ModPathEdit {"
        "  border: 1px solid %1;"
        "  border-radius: 6px;"
        "  padding: 10px 12px;"
        "  background-color: %2;"
        "  color: %3;"
        "  selection-background-color: #007AFF;"
        "}"
    ).arg(borderColor, inputBg, textColor));
    
    m_browseGameBtn->setStyleSheet(QString(
        "QPushButton#BrowseButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: 6px;"
        "  padding: 10px 16px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#BrowseButton:hover {"
        "  background-color: %3;"
        "}"
    ).arg(browseBtnBg, browseBtnText, browseBtnHoverBg));
    
    m_browseDocBtn->setStyleSheet(m_browseGameBtn->styleSheet());
    m_browseModBtn->setStyleSheet(m_browseGameBtn->styleSheet());
    
    m_confirmBtn->setStyleSheet(QString(
        "QPushButton#ConfirmButton {"
        "  background-color: %1;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 12px 30px;"
        "  font-weight: 600;"
        "  font-size: 15px;"
        "}"
        "QPushButton#ConfirmButton:hover {"
        "  background-color: %2;"
        "}"
        "QPushButton#ConfirmButton:pressed {"
        "  background-color: #004999;"
        "}"
    ).arg(btnBg, btnHoverBg));
}

void SetupDialog::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    m_titleLabel->setText(loc.getString("SetupDialog", "TitleLabel"));
    m_gameLabel->setText(loc.getString("SetupDialog", "GameLabel"));
    m_docLabel->setText(loc.getString("SetupDialog", "DocLabel"));
    m_modLabel->setText(loc.getString("SetupDialog", "ModLabel"));
    
    m_gamePathEdit->setPlaceholderText(loc.getString("SetupDialog", "GamePlaceholder"));
    m_docPathEdit->setPlaceholderText(loc.getString("SetupDialog", "DocPlaceholder"));
    m_modPathEdit->setPlaceholderText(loc.getString("SetupDialog", "ModPlaceholder"));
    
    m_confirmBtn->setText(loc.getString("SetupDialog", "ConfirmButton"));
    m_browseGameBtn->setText(loc.getString("SetupDialog", "BrowseButton"));
    m_browseDocBtn->setText(loc.getString("SetupDialog", "BrowseButton"));
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
    
    // Semi-transparent background with rounded corners
    QPainterPath path;
    QRectF r = rect();
    qreal radius = 9;
    
    path.addRoundedRect(r, radius, radius);
    
    painter.fillPath(path, QColor(0, 0, 0, 120));
}

bool SetupDialog::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QWidget::eventFilter(obj, event);
}

void SetupDialog::browseGamePath() {
    LocalizationManager& loc = LocalizationManager::instance();
    QString dir = QFileDialog::getExistingDirectory(this, 
        loc.getString("SetupDialog", "SelectGameDir"),
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!dir.isEmpty()) {
        m_gamePathEdit->setText(dir);
        Logger::instance().logClick("SetupBrowseGamePath");
    }
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

void SetupDialog::browseDocPath() {
    LocalizationManager& loc = LocalizationManager::instance();
    QString dir = QFileDialog::getExistingDirectory(this, 
        loc.getString("SetupDialog", "SelectDocDir"),
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!dir.isEmpty()) {
        m_docPathEdit->setText(dir);
        Logger::instance().logClick("SetupBrowseDocPath");
    }
}

void SetupDialog::onGamePathChanged(const QString &path) {
    if (!path.isEmpty()) {
        ConfigManager::instance().setGamePath(path);
        Logger::instance().logInfo("SetupDialog", "Game path saved: " + path);
    }
}

void SetupDialog::onModPathChanged(const QString &path) {
    if (!path.isEmpty()) {
        ConfigManager::instance().setModPath(path);
        Logger::instance().logInfo("SetupDialog", "Mod path saved: " + path);
    }
}

void SetupDialog::onDocPathChanged(const QString &path) {
    if (!path.isEmpty()) {
        ConfigManager::instance().setDocPath(path);
        Logger::instance().logInfo("SetupDialog", "Doc path saved: " + path);
    }
}

void SetupDialog::validateAndAccept() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    // Check if paths are empty
    if (m_gamePathEdit->text().isEmpty() || m_modPathEdit->text().isEmpty() || m_docPathEdit->text().isEmpty()) {
        CustomMessageBox::information(nullptr, 
            loc.getString("SetupDialog", "ErrorTitle"), 
            loc.getString("SetupDialog", "ErrorMsg"));
        Logger::instance().logError("SetupDialog", "Validation failed: Empty paths");
        return;
    }
    
    // Validate game path
    QString gameError = PathValidator::instance().validateGamePath(m_gamePathEdit->text());
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
    
    // Validate doc path if provided
    QString docPath = m_docPathEdit->text();
    if (!docPath.isEmpty()) {
        QString docError = PathValidator::instance().validateDocPath(docPath);
        if (!docError.isEmpty()) {
            CustomMessageBox::information(nullptr, 
                loc.getString("Error", "DocPathInvalid"), 
                loc.getString("Error", docError));
            Logger::instance().logError("SetupDialog", "Doc path validation failed: " + docError);
            return;
        }
    }
    
    Logger::instance().logClick("SetupConfirm");
    
    // Save final config
    ConfigManager& config = ConfigManager::instance();
    config.setGamePath(m_gamePathEdit->text());
    config.setModPath(m_modPathEdit->text());
    if (!docPath.isEmpty()) {
        config.setDocPath(docPath);
    }
    
    emit setupCompleted();
    hideOverlay();
}

QString SetupDialog::getGamePath() const {
    return m_gamePathEdit->text();
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
    
    // Check if doc path exists and is valid (if provided)
    QString docPath = config.getDocPath();
    if (docPath.isEmpty()) {
        return false;
    }
    
    QString docError = PathValidator::instance().validateDocPath(docPath);
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
