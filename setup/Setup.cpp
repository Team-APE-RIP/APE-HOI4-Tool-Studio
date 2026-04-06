//-------------------------------------------------------------------------------------
// Setup.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "Setup.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QTimer>

namespace {
const char* kRegistryOrganization = "Team-APE-RIP";
const char* kRegistryApplication = "APE-HOI4-Tool-Studio";
const char* kConfigLanguageKey = "Config/Language";
const char* kPathInstallPathKey = "Path/InstallPath";
const char* kPathAutoSetupKey = "Path/AutoSetup";
const QString kDefaultLanguageCode = QStringLiteral("en_US");
const QString kSetupLocalisationRoot = QStringLiteral(":/localisation");
const QString kDefaultInstallPath = QStringLiteral("D:/APE HOI4 Tool Studio");

QString unquoteValue(QString value) {
    value = value.trimmed();
    if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
        value = value.mid(1, value.size() - 2);
    }
    value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    return value;
}

QString readJsonString(const QJsonObject& object, const QString& key) {
    return object.value(key).toString().trimmed();
}

// --- SetupMessageBox Implementation ---
class SetupMessageBox : public QDialog {
public:
    enum Type { Information, Question, Critical };

    SetupMessageBox(QWidget* parent, const QString& title, const QString& message, Type type, bool isDark)
        : QDialog(parent), m_result(QMessageBox::No) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowModality(Qt::WindowModal);

        m_isDark = isDark;
        QString text = isDark ? "#FFFFFF" : "#1D1D1F";

        setStyleSheet(QString(R"(
            QLabel { color: %1; }
            QPushButton {
                background-color: #007AFF; color: white; border: none; border-radius: 6px; padding: 8px 16px; font-weight: bold;
            }
            QPushButton:hover { background-color: #0062CC; }
            QPushButton#CancelBtn {
                background-color: %2; color: %1; border: 1px solid %3;
            }
            QPushButton#CancelBtn:hover { background-color: %4; }
        )").arg(text, isDark ? "#3A3A3C" : "#F5F5F7", isDark ? "#48484A" : "#D2D2D7", isDark ? "#48484A" : "#E5E5EA"));

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(20);

        QLabel* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
        layout->addWidget(titleLabel);

        QLabel* msgLabel = new QLabel(message);
        msgLabel->setWordWrap(true);
        msgLabel->setStyleSheet("font-size: 14px;");
        layout->addWidget(msgLabel);

        QHBoxLayout* btnLayout = new QHBoxLayout();
        btnLayout->addStretch();

        if (type == Question) {
            QPushButton* cancelBtn = new QPushButton("Cancel");
            cancelBtn->setObjectName("CancelBtn");
            connect(cancelBtn, &QPushButton::clicked, [this]() {
                m_result = QMessageBox::No;
                reject();
            });
            btnLayout->addWidget(cancelBtn);

            QPushButton* yesBtn = new QPushButton("Yes");
            connect(yesBtn, &QPushButton::clicked, [this]() {
                m_result = QMessageBox::Yes;
                accept();
            });
            btnLayout->addWidget(yesBtn);
        } else {
            QPushButton* okBtn = new QPushButton("OK");
            connect(okBtn, &QPushButton::clicked, [this]() {
                m_result = QMessageBox::Ok;
                accept();
            });
            btnLayout->addWidget(okBtn);
        }

        layout->addLayout(btnLayout);
    }

    QMessageBox::StandardButton result() const { return m_result; }

protected:
    void paintEvent(QPaintEvent* event) override {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QColor bg = m_isDark ? QColor("#2C2C2E") : QColor("#FFFFFF");
        QColor border = m_isDark ? QColor("#3A3A3C") : QColor("#D2D2D7");

        QPainterPath path;
        path.addRoundedRect(rect(), 10, 10);

        painter.fillPath(path, bg);
        painter.setPen(QPen(border, 1));
        painter.drawPath(path);
    }

private:
    QMessageBox::StandardButton m_result;
    bool m_isDark;
};

static bool copyDirectory(const QString& srcPath, const QString& dstPath, bool overwrite) {
    QDir srcDir(srcPath);
    if (!srcDir.exists()) return false;

    QDir dstDir(dstPath);
    if (!dstDir.exists()) {
        dstDir.mkpath(".");
    }

    bool success = true;
    QFileInfoList fileInfoList = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : fileInfoList) {
        QString srcFilePath = fileInfo.filePath();
        QString dstFilePath = dstDir.filePath(fileInfo.fileName());

        if (fileInfo.isDir()) {
            success = copyDirectory(srcFilePath, dstFilePath, overwrite) && success;
        } else {
            if (QFile::exists(dstFilePath)) {
                if (overwrite) {
                    QFile::remove(dstFilePath);
                } else {
                    continue;
                }
            }
            success = QFile::copy(srcFilePath, dstFilePath) && success;
        }
    }
    return success;
}

static void showCustomMessageBox(QWidget* parent, const QString& title, const QString& message, SetupMessageBox::Type type, bool isDark) {
    SetupMessageBox box(parent, title, message, type, isDark);
    box.adjustSize();
    if (parent) {
        QPoint parentCenter = parent->mapToGlobal(parent->rect().center());
        box.move(parentCenter.x() - box.width() / 2, parentCenter.y() - box.height() / 2);
    }
    box.raise();
    box.activateWindow();
    box.exec();
}
} // namespace

Setup::Setup(QWidget* parent)
    : QDialog(parent), currentLang(kDefaultLanguageCode), m_isDarkMode(false), m_dragging(false), m_isAutoSetup(false) {
    m_isDarkMode = detectSystemDarkMode();

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setWindowIcon(QIcon(":/app.ico"));

    migrateLegacySettings();
    setupUi();
    populateLanguageCombo();

    const QString initialLang = normalizeLanguageCode(QSettings(kRegistryOrganization, kRegistryApplication).value(kConfigLanguageKey, kDefaultLanguageCode).toString());

    langCombo->blockSignals(true);
    langCombo->setCurrentText(displayTextForLanguage(initialLang));
    langCombo->blockSignals(false);

    loadLanguage(initialLang);
    applyTheme();

    setMinimumSize(500, 580);
    resize(500, 580);
}

Setup::~Setup() {}

QMap<QString, QString> Setup::parseMetaFile(const QString& path) const {
    QMap<QString, QString> result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        const int separatorIndex = line.indexOf('=');
        if (separatorIndex <= 0) {
            continue;
        }

        const QString key = line.left(separatorIndex).trimmed();
        const QString value = unquoteValue(line.mid(separatorIndex + 1));
        result.insert(key, value);
    }

    return result;
}

QMap<QString, QString> Setup::parseSimpleYamlFile(const QString& path) const {
    QMap<QString, QString> result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }

    QTextStream stream(&file);
    bool insideRoot = false;

    while (!stream.atEnd()) {
        const QString rawLine = stream.readLine();
        const QString trimmed = rawLine.trimmed();

        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            continue;
        }

        if (!insideRoot) {
            if (trimmed.startsWith("l_") && trimmed.endsWith(':')) {
                insideRoot = true;
            }
            continue;
        }

        if (!rawLine.startsWith(' ') && !rawLine.startsWith('\t')) {
            continue;
        }

        const int separatorIndex = trimmed.indexOf(':');
        if (separatorIndex <= 0) {
            continue;
        }

        const QString key = trimmed.left(separatorIndex).trimmed();
        const QString value = unquoteValue(trimmed.mid(separatorIndex + 1));
        result.insert(key, value);
    }

    return result;
}

QString Setup::normalizeLanguageCode(const QString& value) const {
    const QString trimmed = value.trimmed();
    if (trimmed == "English" || trimmed == "en_US") return "en_US";
    if (trimmed == "简体中文" || trimmed == "zh_CN") return "zh_CN";
    if (trimmed == "繁體中文" || trimmed == "zh_TW") return "zh_TW";
    if (m_languageTextByCode.contains(trimmed)) return trimmed;
    for (auto it = m_languageTextByCode.begin(); it != m_languageTextByCode.end(); ++it) {
        if (it.value() == trimmed) return it.key();
    }
    return kDefaultLanguageCode;
}

QString Setup::displayTextForLanguage(const QString& langCode) const {
    const QString normalized = normalizeLanguageCode(langCode);
    return m_languageTextByCode.value(normalized, m_languageTextByCode.value(kDefaultLanguageCode, QStringLiteral("English")));
}

QString Setup::localizedValue(const QString& key, const QString& fallback) const {
    return currentLoc.value(key, fallback);
}

void Setup::populateLanguageCombo() {
    m_languageTextByCode.clear();

    QDir rootDir(kSetupLocalisationRoot);
    const QStringList languageDirectories = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString& directoryName : languageDirectories) {
        const QMap<QString, QString> meta = parseMetaFile(rootDir.filePath(directoryName + "/meta.htsl"));
        const QString langCode = meta.value("lang", directoryName).trimmed();
        const QString displayText = meta.value("text", langCode).trimmed();
        m_languageTextByCode.insert(langCode, displayText);
    }

    if (!m_languageTextByCode.contains(kDefaultLanguageCode)) {
        m_languageTextByCode.insert(kDefaultLanguageCode, QStringLiteral("English"));
    }

    langCombo->clear();
    for (auto it = m_languageTextByCode.begin(); it != m_languageTextByCode.end(); ++it) {
        langCombo->addItem(it.value(), it.key());
    }
}

void Setup::migrateLegacySettings() {
    QSettings settings(kRegistryOrganization, kRegistryApplication);

    const QString tempRoot =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
    const QString setupCacheRoot = tempRoot + "/setup_cache";
    const QString configPath = tempRoot + "/config.json";
    const QString pathPath = tempRoot + "/path.json";
    const QString tempLanguagePath = setupCacheRoot + "/temp_lang.json";

    QFile configFile(configPath);
    if (configFile.exists() && configFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(configFile.readAll());
        const QJsonObject object = document.object();
        if (!settings.contains(kConfigLanguageKey) && object.contains("language")) {
            settings.setValue(kConfigLanguageKey, normalizeLanguageCode(readJsonString(object, "language")));
        }
        configFile.close();
        QFile::remove(configPath);
    }

    QFile pathFile(pathPath);
    if (pathFile.exists() && pathFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(pathFile.readAll());
        const QJsonObject object = document.object();
        if (!settings.contains(kPathInstallPathKey) && object.contains("path")) {
            settings.setValue(kPathInstallPathKey, readJsonString(object, "path"));
        }
        if (!settings.contains(kPathAutoSetupKey) && object.contains("auto")) {
            settings.setValue(kPathAutoSetupKey, readJsonString(object, "auto") == "1");
        }
        pathFile.close();
        QFile::remove(pathPath);
    }

    QFile tempLanguageFile(tempLanguagePath);
    if (tempLanguageFile.exists() && tempLanguageFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(tempLanguageFile.readAll());
        const QJsonObject object = document.object();
        if (!settings.contains(kConfigLanguageKey) && object.contains("language")) {
            settings.setValue(kConfigLanguageKey, normalizeLanguageCode(readJsonString(object, "language")));
        }
        tempLanguageFile.close();
        QFile::remove(tempLanguagePath);
    }

    if (!settings.contains(kConfigLanguageKey)) {
        settings.setValue(kConfigLanguageKey, QString(kDefaultLanguageCode));
    }
}

QString Setup::currentInstallPath() const {
    QSettings settings(kRegistryOrganization, kRegistryApplication);
    return settings.value(kPathInstallPathKey, kDefaultInstallPath).toString().trimmed();
}

bool Setup::currentAutoSetupFlag() const {
    QSettings settings(kRegistryOrganization, kRegistryApplication);
    return settings.value(kPathAutoSetupKey, false).toBool();
}

bool Setup::detectSystemDarkMode() {
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                       QSettings::NativeFormat);
    return settings.value("AppsUseLightTheme", 1).toInt() == 0;
}

void Setup::applyTheme() {
    QString bg, text, border, inputBg, btnBg, btnHoverBg, browseBtnBg, browseBtnHoverBg, browseBtnText;
    QString itemHover, comboIndicator;

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
        itemHover = "#3A3A3C";
        comboIndicator = "#FFFFFF";
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
        itemHover = "rgba(0, 0, 0, 0.05)";
        comboIndicator = "#1D1D1F";
    }

    QString styleSheet = QString(R"(
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
            padding: 4px;
            outline: none;
        }
        QComboBox QAbstractItemView::item {
            padding: 6px 12px;
            border-left: 3px solid transparent;
            color: %2;
        }
        QComboBox QAbstractItemView::item:hover {
            background-color: %10;
            border-left: 3px solid %11;
            color: %2;
        }
        QProgressBar {
            border: 1px solid %3;
            border-radius: 6px;
            text-align: center;
            background-color: %4;
            color: %2;
        }
        QProgressBar::chunk {
            background-color: %5;
            border-radius: 5px;
        }
    )").arg(bg, text, border, inputBg, btnBg, btnHoverBg, browseBtnBg, browseBtnHoverBg, browseBtnText);

    styleSheet = styleSheet.replace("%10", itemHover).replace("%11", comboIndicator);
    m_centralWidget->setStyleSheet(styleSheet);
}

void Setup::setupUi() {
    QVBoxLayout* dialogLayout = new QVBoxLayout(this);
    dialogLayout->setContentsMargins(0, 0, 0, 0);

    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("CentralWidget");
    dialogLayout->addWidget(m_centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QHBoxLayout* topBarLayout = new QHBoxLayout();
    topBarLayout->setSpacing(0);
    topBarLayout->setContentsMargins(0, 0, 0, 0);

    QWidget* controlContainer = new QWidget(this);
    controlContainer->setFixedWidth(60);
    controlContainer->setStyleSheet("background: transparent;");
    QHBoxLayout* controlLayout = new QHBoxLayout(controlContainer);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(8);

    auto createControlBtn = [](const QString& color, const QString& hoverColor) -> QPushButton* {
        QPushButton* btn = new QPushButton();
        btn->setFixedSize(12, 12);
        btn->setStyleSheet(QString(
            "QPushButton { background-color: %1; border-radius: 6px; border: none; }"
            "QPushButton:hover { background-color: %2; }"
        ).arg(color, hoverColor));
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    QPushButton* closeBtn = createControlBtn("#FF5F57", "#FF3B30");
    QPushButton* minBtn = createControlBtn("#FFBD2E", "#FFAD1F");
    QPushButton* maxBtn = createControlBtn("#28C940", "#24B538");

    connect(closeBtn, &QPushButton::clicked, this, &Setup::closeWindow);
    connect(minBtn, &QPushButton::clicked, this, &QDialog::showMinimized);

    controlLayout->addWidget(closeBtn);
    controlLayout->addWidget(minBtn);
    controlLayout->addWidget(maxBtn);
    controlLayout->addStretch();

    topBarLayout->addWidget(controlContainer);
    topBarLayout->addStretch();

    langCombo = new QComboBox(this);
    langCombo->setCursor(Qt::PointingHandCursor);
    connect(langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Setup::changeLanguage);
    topBarLayout->addWidget(langCombo);

    mainLayout->addLayout(topBarLayout);

    titleLabel = new QLabel(this);
    titleLabel->setObjectName("TitleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QLabel* iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon(":/app.ico").pixmap(256, 256));
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setFixedSize(256, 256);

    QHBoxLayout* iconLayout = new QHBoxLayout();
    iconLayout->addStretch();
    iconLayout->addWidget(iconLabel);
    iconLayout->addStretch();
    mainLayout->addLayout(iconLayout);

    mainLayout->addStretch();

    QVBoxLayout* pathLayout = new QVBoxLayout();
    pathLayout->setSpacing(6);
    pathLabel = new QLabel(this);
    pathLayout->addWidget(pathLabel);

    QHBoxLayout* pathInputLayout = new QHBoxLayout();
    pathInputLayout->setSpacing(8);
    pathEdit = new QLineEdit(this);

    m_isAutoSetup = currentAutoSetupFlag();
    const QString installPath = currentInstallPath();
    pathEdit->setText(installPath.isEmpty() ? kDefaultInstallPath : installPath);

    browseBtn = new QPushButton(this);
    browseBtn->setObjectName("BrowseButton");
    browseBtn->setCursor(Qt::PointingHandCursor);
    connect(browseBtn, &QPushButton::clicked, this, &Setup::browseDirectory);

    pathInputLayout->addWidget(pathEdit);
    pathInputLayout->addWidget(browseBtn);
    pathLayout->addLayout(pathInputLayout);
    mainLayout->addLayout(pathLayout);

    mainLayout->addSpacing(10);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);
    progressBar->hide();
    mainLayout->addWidget(progressBar);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    installBtn = new QPushButton(this);
    installBtn->setObjectName("ConfirmButton");
    installBtn->setCursor(Qt::PointingHandCursor);
    connect(installBtn, &QPushButton::clicked, this, &Setup::startInstall);

    btnLayout->addWidget(installBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    if (m_isAutoSetup) {
        pathLabel->hide();
        pathEdit->hide();
        browseBtn->hide();
        installBtn->hide();
        QTimer::singleShot(100, this, &Setup::startInstall);
    }
}

void Setup::loadLanguage(const QString& langCode) {
    currentLang = normalizeLanguageCode(langCode);

    currentLoc = parseSimpleYamlFile(QString("%1/%2/strings.yml").arg(kSetupLocalisationRoot, currentLang));
    if (currentLoc.isEmpty() && currentLang != kDefaultLanguageCode) {
        currentLoc = parseSimpleYamlFile(QString("%1/%2/strings.yml").arg(kSetupLocalisationRoot, kDefaultLanguageCode));
        currentLang = kDefaultLanguageCode;
    }

    setWindowTitle(localizedValue("window_title", "APE HOI4 Tool Studio - Setup"));
    titleLabel->setText(localizedValue("title", "Install APE HOI4 Tool Studio"));
    pathLabel->setText(localizedValue("path_label", "Installation Path:"));
    browseBtn->setText(localizedValue("browse_btn", "Browse..."));
    installBtn->setText(localizedValue("install_btn", "Install"));
}

void Setup::changeLanguage(int index) {
    const QString langCode = langCombo->itemData(index).toString();
    loadLanguage(langCode);
    saveTempLanguage();
}

void Setup::browseDirectory() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        localizedValue("select_dir_title", "Select Installation Directory"),
        pathEdit->text()
    );

    if (!dir.isEmpty()) {
        QDir d(dir);
        if (d.dirName() != "APE HOI4 Tool Studio") {
            dir = QDir::cleanPath(d.filePath("APE HOI4 Tool Studio"));
        } else {
            dir = QDir::cleanPath(dir);
        }
        pathEdit->setText(dir);
    }
}

void Setup::startInstall() {
    QString targetPath = pathEdit->text().trimmed();
    if (targetPath.isEmpty()) {
        showCustomMessageBox(
            this,
            localizedValue("error_title", "Error"),
            localizedValue("error_empty_path", "Installation path cannot be empty."),
            SetupMessageBox::Critical,
            m_isDarkMode
        );
        return;
    }

    {
        QSettings settings(kRegistryOrganization, kRegistryApplication);
        settings.setValue(kPathInstallPathKey, targetPath);
        settings.setValue(kPathAutoSetupKey, m_isAutoSetup);
        settings.setValue(kConfigLanguageKey, currentLang);
    }

    QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "APEHOI4ToolStudio.exe");

    pathLabel->hide();
    pathEdit->hide();
    browseBtn->hide();
    installBtn->hide();

    QDir dir(targetPath);
    QString oldToolsPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/setup_cache/old_tools";

    if (dir.exists()) {
        QDir toolsDir(dir.filePath("tools"));
        if (toolsDir.exists()) {
            QDir oldToolsDir(oldToolsPath);
            if (oldToolsDir.exists()) {
                oldToolsDir.removeRecursively();
            }
            copyDirectory(toolsDir.absolutePath(), oldToolsPath, true);
        }
        dir.removeRecursively();
    }

    if (!dir.mkpath(".")) {
        showCustomMessageBox(
            this,
            localizedValue("error_title", "Error"),
            localizedValue("error_create_dir", "Failed to create installation directory."),
            SetupMessageBox::Critical,
            m_isDarkMode
        );
        return;
    }

    langCombo->setEnabled(false);

    progressBar->show();
    progressBar->setValue(10);

    QTimer::singleShot(100, this, [this, targetPath, oldToolsPath]() {
        if (extractPayload(targetPath)) {
            QDir oldToolsDir(oldToolsPath);
            if (oldToolsDir.exists()) {
                QDir dir(targetPath);
                QString newToolsPath = dir.filePath("tools");
                copyDirectory(oldToolsPath, newToolsPath, false);
                oldToolsDir.removeRecursively();
            }

            progressBar->setValue(100);

            const QString successTitle = m_isAutoSetup
                ? localizedValue("update_success_title", "Update Success")
                : localizedValue("success_title", "Success");
            const QString successMsg = m_isAutoSetup
                ? localizedValue("update_success_msg", "Update completed successfully!")
                : localizedValue("success_msg", "Installation completed successfully!");

            showCustomMessageBox(this, successTitle, successMsg, SetupMessageBox::Information, m_isDarkMode);

            QProcess::startDetached(targetPath + "/APEHOI4ToolStudio.exe", QStringList());
            accept();
        } else {
            const QString errorTitle = m_isAutoSetup
                ? localizedValue("update_error_title", "Update Error")
                : localizedValue("error_title", "Error");
            const QString errorMsg = m_isAutoSetup
                ? localizedValue("update_error_msg", "Failed to extract files. Update aborted.")
                : localizedValue("error_extract", "Failed to extract files. Installation aborted.");

            showCustomMessageBox(this, errorTitle, errorMsg, SetupMessageBox::Critical, m_isDarkMode);

            if (!m_isAutoSetup) {
                pathLabel->show();
                pathEdit->show();
                browseBtn->show();
                installBtn->show();
                installBtn->setEnabled(true);
                browseBtn->setEnabled(true);
                pathEdit->setEnabled(true);
            }
            progressBar->hide();
        }
    });
}

void Setup::saveTempLanguage() {
    QSettings settings(kRegistryOrganization, kRegistryApplication);
    settings.setValue(kConfigLanguageKey, currentLang);
}

bool Setup::extractPayload(const QString& targetDir) {
    progressBar->setValue(20);

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/setup_cache";
    QDir().mkpath(tempDir);

    QString tempArchive = tempDir + "/payload.7z";
    QString temp7zExe = tempDir + "/7z.exe";
    QString temp7zDll = tempDir + "/7z.dll";

    auto extractResource = [](const QString& resPath, const QString& outPath) -> bool {
        QFile resFile(resPath);
        if (!resFile.exists() || !resFile.open(QIODevice::ReadOnly)) return false;

        QFile outFile(outPath);
        if (!outFile.open(QIODevice::WriteOnly)) return false;

        outFile.write(resFile.readAll());
        outFile.close();
        resFile.close();
        return true;
    };

    if (!extractResource(":/data/7z.exe", temp7zExe)) return false;
    if (!extractResource(":/data/7z.dll", temp7zDll)) return false;

    progressBar->setValue(30);

    if (!extractResource(":/data/payload.7z", tempArchive)) return false;

    progressBar->setValue(50);

    QProcess process;
    QStringList args;
    args << "x" << tempArchive << "-y" << QString("-o%1").arg(targetDir);

    process.start(temp7zExe, args);
    process.waitForFinished(-1);

    progressBar->setValue(90);

    QFile::remove(tempArchive);
    QFile::remove(temp7zExe);
    QFile::remove(temp7zDll);

    return process.exitCode() == 0;
}

void Setup::closeWindow() {
    reject();
}

void Setup::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void Setup::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && m_dragging) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void Setup::mouseReleaseEvent(QMouseEvent* event) {
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}