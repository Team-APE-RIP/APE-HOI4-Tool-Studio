#include "SetupDialog.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "PathValidator.h"
#include "CustomMessageBox.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QStyle>
#include <QSettings>
#include <QLineEdit>

SetupDialog::SetupDialog(QWidget *parent) : QDialog(parent), m_isDarkMode(false), m_dragging(false) {
    // Use saved theme from ConfigManager, fallback to system detection if System theme
    ConfigManager::Theme theme = ConfigManager::instance().getTheme();
    if (theme == ConfigManager::Theme::System) {
        m_isDarkMode = detectSystemDarkMode();
    } else {
        m_isDarkMode = (theme == ConfigManager::Theme::Dark);
    }
    
    // Remove system title bar, make frameless window
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setWindowIcon(QIcon(":/app.ico"));
    
    setupUi();
    
    // Load existing config
    ConfigManager& config = ConfigManager::instance();
    if (!config.getLanguage().isEmpty()) {
        m_languageCombo->setCurrentText(config.getLanguage());
    }
    if (!config.getGamePath().isEmpty()) {
        m_gamePathEdit->setText(config.getGamePath());
    }
    if (!config.getModPath().isEmpty()) {
        m_modPathEdit->setText(config.getModPath());
    }
    
    // Connect real-time save signals
    connect(m_gamePathEdit, &QLineEdit::textChanged, this, &SetupDialog::onGamePathChanged);
    connect(m_modPathEdit, &QLineEdit::textChanged, this, &SetupDialog::onModPathChanged);
    
    updateTexts();
    applyTheme();
    
    setMinimumSize(500, 580);
    resize(500, 580);
}

bool SetupDialog::detectSystemDarkMode() {
    // Use Windows Registry to detect system theme
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 
                       QSettings::NativeFormat);
    // AppsUseLightTheme: 0 = dark, 1 = light
    return settings.value("AppsUseLightTheme", 1).toInt() == 0;
}

void SetupDialog::applyTheme() {
    QString bg, text, border, inputBg, btnBg, btnHoverBg, browseBtnBg, browseBtnHoverBg, browseBtnText;
    
    if (m_isDarkMode) {
        bg = "#2C2C2E";
        text = "#FFFFFF";
        border = "#3A3A3C";
        inputBg = "#3A3A3C";
        btnBg = "#0A84FF";
        btnHoverBg = "#0070E0";
        browseBtnBg = "#3A3A3C";
        browseBtnHoverBg = "#4A4A4C";
        browseBtnText = "#0A84FF";
    } else {
        bg = "#F5F5F7";
        text = "#1D1D1F";
        border = "#D2D2D7";
        inputBg = "#FFFFFF";
        btnBg = "#007AFF";
        btnHoverBg = "#0062CC";
        browseBtnBg = "#E5E5EA";
        browseBtnHoverBg = "#D1D1D6";
        browseBtnText = "#007AFF";
    }
    
    m_centralWidget->setStyleSheet(QString(R"(
        QWidget#CentralWidget {
            background-color: %1;
            border: 1px solid %3;
            border-radius: 10px;
        }
        QLabel {
            color: %2;
            font-size: 14px;
            background: transparent;
            border: none;
        }
        QLabel#TitleLabel {
            font-size: 22px;
            font-weight: bold;
        }
        QLineEdit {
            border: 1px solid %3;
            border-radius: 6px;
            padding: 8px;
            background-color: %4;
            color: %2;
            selection-background-color: #007AFF;
        }
        QPushButton#ConfirmButton {
            background-color: %5;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 30px;
            font-weight: 500;
            font-size: 14px;
        }
        QPushButton#ConfirmButton:hover {
            background-color: %6;
        }
        QPushButton#ConfirmButton:pressed {
            background-color: #004999;
        }
        QPushButton#BrowseButton {
            background-color: %7;
            color: %9;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 500;
        }
        QPushButton#BrowseButton:hover {
            background-color: %8;
        }
        QComboBox {
            border: 1px solid %3;
            border-radius: 6px;
            padding: 6px 12px;
            background-color: %4;
            color: %2;
            min-width: 70px;
        }
        QComboBox::drop-down {
            border: none;
            background: transparent;
            width: 0px;
        }
        QComboBox::down-arrow {
            width: 0;
            height: 0;
        }
        QComboBox QAbstractItemView {
            background-color: %4;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            selection-background-color: #007AFF;
            selection-color: white;
        }
    )").arg(bg, text, border, inputBg, btnBg, btnHoverBg, browseBtnBg, browseBtnHoverBg, browseBtnText));
}

void SetupDialog::setupUi() {
    // Main layout for the dialog (transparent background)
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);
    dialogLayout->setContentsMargins(0, 0, 0, 0);
    
    // Central widget with rounded corners and border
    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("CentralWidget");
    dialogLayout->addWidget(m_centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setSpacing(12);
    // Top margin reduced by 12px for window controls positioning
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Top bar: Window controls (left) + Language selector (right)
    QHBoxLayout *topBarLayout = new QHBoxLayout();
    topBarLayout->setSpacing(0);
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    
    // Mac-style window control buttons in a fixed-width container
    QWidget *controlContainer = new QWidget(this);
    controlContainer->setFixedWidth(60);
    controlContainer->setStyleSheet("background: transparent;");
    QHBoxLayout *controlLayout = new QHBoxLayout(controlContainer);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(8);
    
    auto createControlBtn = [](const QString &color, const QString &hoverColor) -> QPushButton* {
        QPushButton *btn = new QPushButton();
        btn->setFixedSize(12, 12);
        btn->setStyleSheet(QString(
            "QPushButton { background-color: %1; border-radius: 6px; border: none; }"
            "QPushButton:hover { background-color: %2; }"
        ).arg(color, hoverColor));
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };
    
    QPushButton *closeBtn = createControlBtn("#FF5F57", "#FF3B30");
    QPushButton *minBtn = createControlBtn("#FFBD2E", "#FFAD1F");
    QPushButton *maxBtn = createControlBtn("#28C940", "#24B538");
    
    connect(closeBtn, &QPushButton::clicked, this, &SetupDialog::closeWindow);
    connect(minBtn, &QPushButton::clicked, this, &QDialog::showMinimized);
    // Max button does nothing for dialog, but keep for visual consistency
    
    controlLayout->addWidget(closeBtn);
    controlLayout->addWidget(minBtn);
    controlLayout->addWidget(maxBtn);
    controlLayout->addStretch();
    
    topBarLayout->addWidget(controlContainer);
    topBarLayout->addStretch();
    
    // Language selector - whole box is clickable
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItems({"English", "简体中文", "繁體中文"});
    m_languageCombo->setCursor(Qt::PointingHandCursor);
    connect(m_languageCombo, &QComboBox::currentTextChanged, this, &SetupDialog::onLanguageChanged);
    topBarLayout->addWidget(m_languageCombo);
    
    mainLayout->addLayout(topBarLayout);

    // Title
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setObjectName("TitleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // App Icon (centered, 256x256)
    QLabel *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon(":/app.ico").pixmap(256, 256));
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(256, 256);
    
    QHBoxLayout *iconLayout = new QHBoxLayout();
    iconLayout->addStretch();
    iconLayout->addWidget(iconLabel);
    iconLayout->addStretch();
    mainLayout->addLayout(iconLayout);

    // Game Path
    QVBoxLayout *gameLayout = new QVBoxLayout();
    gameLayout->setSpacing(6);
    QLabel *gameLabel = new QLabel(this);
    gameLabel->setObjectName("GameLabel");
    gameLayout->addWidget(gameLabel);
    
    QHBoxLayout *gameInputLayout = new QHBoxLayout();
    gameInputLayout->setSpacing(8);
    m_gamePathEdit = new QLineEdit(this);
    QPushButton *browseGameBtn = new QPushButton(this);
    browseGameBtn->setObjectName("BrowseButton");
    browseGameBtn->setCursor(Qt::PointingHandCursor);
    connect(browseGameBtn, &QPushButton::clicked, this, &SetupDialog::browseGamePath);
    
    gameInputLayout->addWidget(m_gamePathEdit);
    gameInputLayout->addWidget(browseGameBtn);
    gameLayout->addLayout(gameInputLayout);
    mainLayout->addLayout(gameLayout);

    // Mod Path
    QVBoxLayout *modLayout = new QVBoxLayout();
    modLayout->setSpacing(6);
    QLabel *modLabel = new QLabel(this);
    modLabel->setObjectName("ModLabel");
    modLayout->addWidget(modLabel);
    
    QHBoxLayout *modInputLayout = new QHBoxLayout();
    modInputLayout->setSpacing(8);
    m_modPathEdit = new QLineEdit(this);
    QPushButton *browseModBtn = new QPushButton(this);
    browseModBtn->setObjectName("BrowseButton");
    browseModBtn->setCursor(Qt::PointingHandCursor);
    connect(browseModBtn, &QPushButton::clicked, this, &SetupDialog::browseModPath);
    
    modInputLayout->addWidget(m_modPathEdit);
    modInputLayout->addWidget(browseModBtn);
    modLayout->addLayout(modInputLayout);
    mainLayout->addLayout(modLayout);

    mainLayout->addSpacing(10);

    // Confirm Button
    QPushButton *confirmBtn = new QPushButton(this);
    confirmBtn->setObjectName("ConfirmButton");
    confirmBtn->setCursor(Qt::PointingHandCursor);
    connect(confirmBtn, &QPushButton::clicked, this, &SetupDialog::validateAndAccept);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(confirmBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);
}

void SetupDialog::updateTexts() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    setWindowTitle(loc.getString("SetupDialog", "WindowTitle"));
    
    QLabel *titleLabel = findChild<QLabel*>("TitleLabel");
    if(titleLabel) titleLabel->setText(loc.getString("SetupDialog", "TitleLabel"));
    
    QLabel *gameLabel = findChild<QLabel*>("GameLabel");
    if(gameLabel) gameLabel->setText(loc.getString("SetupDialog", "GameLabel"));
    
    QLabel *modLabel = findChild<QLabel*>("ModLabel");
    if(modLabel) modLabel->setText(loc.getString("SetupDialog", "ModLabel"));
    
    m_gamePathEdit->setPlaceholderText(loc.getString("SetupDialog", "GamePlaceholder"));
    m_modPathEdit->setPlaceholderText(loc.getString("SetupDialog", "ModPlaceholder"));
    
    QPushButton *confirmBtn = findChild<QPushButton*>("ConfirmButton");
    if(confirmBtn) confirmBtn->setText(loc.getString("SetupDialog", "ConfirmButton"));
    
    QList<QPushButton*> browseBtns = findChildren<QPushButton*>("BrowseButton");
    for(auto btn : browseBtns) btn->setText(loc.getString("SetupDialog", "BrowseButton"));
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

void SetupDialog::onLanguageChanged(const QString &lang) {
    LocalizationManager::instance().loadLanguage(lang);
    ConfigManager::instance().setLanguage(lang);
    Logger::instance().logInfo("SetupDialog", "Language changed to: " + lang);
    updateTexts();
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

void SetupDialog::validateAndAccept() {
    LocalizationManager& loc = LocalizationManager::instance();
    
    // Check if paths are empty
    if (m_gamePathEdit->text().isEmpty() || m_modPathEdit->text().isEmpty()) {
        CustomMessageBox::information(this, 
            loc.getString("SetupDialog", "ErrorTitle"), 
            loc.getString("SetupDialog", "ErrorMsg"));
        Logger::instance().logError("SetupDialog", "Validation failed: Empty paths");
        return;
    }
    
    // Validate game path
    QString gameError = PathValidator::instance().validateGamePath(m_gamePathEdit->text());
    if (!gameError.isEmpty()) {
        CustomMessageBox::information(this, 
            loc.getString("Error", "GamePathInvalid"), 
            loc.getString("Error", gameError));
        Logger::instance().logError("SetupDialog", "Game path validation failed: " + gameError);
        return;
    }
    
    // Validate mod path
    QString modError = PathValidator::instance().validateModPath(m_modPathEdit->text());
    if (!modError.isEmpty()) {
        CustomMessageBox::information(this, 
            loc.getString("Error", "ModPathInvalid"), 
            loc.getString("Error", modError));
        Logger::instance().logError("SetupDialog", "Mod path validation failed: " + modError);
        return;
    }
    
    Logger::instance().logClick("SetupConfirm");
    accept();
}

void SetupDialog::closeWindow() {
    reject();
}

QString SetupDialog::getGamePath() const {
    return m_gamePathEdit->text();
}

QString SetupDialog::getModPath() const {
    return m_modPathEdit->text();
}

QString SetupDialog::getLanguage() const {
    return m_languageCombo->currentText();
}

// Mouse event handlers for window dragging
void SetupDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void SetupDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton && m_dragging) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void SetupDialog::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}
