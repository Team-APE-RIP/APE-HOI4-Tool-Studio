#include "SetupDialog.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QStyle>

SetupDialog::SetupDialog(QWidget *parent) : QDialog(parent) {
    setupUi();
    
    // Load existing config
    ConfigManager& config = ConfigManager::instance();
    if (!config.getLanguage().isEmpty()) {
        m_languageCombo->setCurrentText(config.getLanguage());
    }
    if (!config.getGamePath().isEmpty()) {
        m_gamePathEdit->setText(config.getGamePath());
    }
    
    updateTexts();
    
    // Apple-like styling
    setStyleSheet(R"(
        QDialog {
            background-color: #F5F5F7;
            font-family: "Segoe UI", "Helvetica Neue", Arial, sans-serif;
        }
        QLabel {
            color: #1D1D1F;
            font-size: 14px;
        }
        QLineEdit {
            border: 1px solid #D2D2D7;
            border-radius: 6px;
            padding: 8px;
            background-color: white;
            selection-background-color: #007AFF;
        }
        QPushButton {
            background-color: #007AFF;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #0062CC;
        }
        QPushButton:pressed {
            background-color: #004999;
        }
        QPushButton#BrowseButton {
            background-color: #E5E5EA;
            color: #007AFF;
        }
        QPushButton#BrowseButton:hover {
            background-color: #D1D1D6;
        }
        QComboBox {
            border: 1px solid #D2D2D7;
            border-radius: 6px;
            padding: 6px;
            background-color: white;
        }
    )");
}

void SetupDialog::setupUi() {
    setWindowTitle("HOI4 Character Studio - Setup");
    setMinimumWidth(500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Language Selection (Top Right)
    QHBoxLayout *langLayout = new QHBoxLayout();
    langLayout->addStretch();
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItems({"English", "简体中文", "繁體中文"});
    connect(m_languageCombo, &QComboBox::currentTextChanged, this, &SetupDialog::onLanguageChanged);
    langLayout->addWidget(m_languageCombo);
    mainLayout->addLayout(langLayout);

    // Title
    QLabel *titleLabel = new QLabel("Welcome to HOI4 Character Studio", this);
    titleLabel->setObjectName("TitleLabel");
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold; margin-bottom: 10px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Game Path
    QVBoxLayout *gameLayout = new QVBoxLayout();
    QLabel *gameLabel = new QLabel("HOI4 Game Directory:", this);
    gameLabel->setObjectName("GameLabel");
    gameLayout->addWidget(gameLabel);
    
    QHBoxLayout *gameInputLayout = new QHBoxLayout();
    m_gamePathEdit = new QLineEdit(this);
    m_gamePathEdit->setPlaceholderText("Select Hearts of Iron IV installation folder...");
    QPushButton *browseGameBtn = new QPushButton("Browse", this);
    browseGameBtn->setObjectName("BrowseButton");
    connect(browseGameBtn, &QPushButton::clicked, this, &SetupDialog::browseGamePath);
    
    gameInputLayout->addWidget(m_gamePathEdit);
    gameInputLayout->addWidget(browseGameBtn);
    gameLayout->addLayout(gameInputLayout);
    mainLayout->addLayout(gameLayout);

    // Mod Path
    QVBoxLayout *modLayout = new QVBoxLayout();
    QLabel *modLabel = new QLabel("Mod Directory:", this);
    modLabel->setObjectName("ModLabel");
    modLayout->addWidget(modLabel);
    
    QHBoxLayout *modInputLayout = new QHBoxLayout();
    m_modPathEdit = new QLineEdit(this);
    m_modPathEdit->setPlaceholderText("Select your mod folder...");
    QPushButton *browseModBtn = new QPushButton("Browse", this);
    browseModBtn->setObjectName("BrowseButton");
    connect(browseModBtn, &QPushButton::clicked, this, &SetupDialog::browseModPath);
    
    modInputLayout->addWidget(m_modPathEdit);
    modInputLayout->addWidget(browseModBtn);
    modLayout->addLayout(modInputLayout);
    mainLayout->addLayout(modLayout);

    mainLayout->addStretch();

    // Confirm Button
    QPushButton *confirmBtn = new QPushButton("Start Studio", this);
    confirmBtn->setObjectName("ConfirmButton");
    confirmBtn->setCursor(Qt::PointingHandCursor);
    connect(confirmBtn, &QPushButton::clicked, this, &SetupDialog::validateAndAccept);
    mainLayout->addWidget(confirmBtn, 0, Qt::AlignCenter);
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
    Logger::instance().logInfo("SetupDialog", "Language changed to: " + lang);
    updateTexts();
}

void SetupDialog::validateAndAccept() {
    if (m_gamePathEdit->text().isEmpty() || m_modPathEdit->text().isEmpty()) {
        LocalizationManager& loc = LocalizationManager::instance();
        QMessageBox::warning(this, 
            loc.getString("SetupDialog", "ErrorTitle"), 
            loc.getString("SetupDialog", "ErrorMsg"));
        Logger::instance().logError("SetupDialog", "Validation failed: Empty paths");
        return;
    }
    Logger::instance().logClick("SetupConfirm");
    accept();
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
