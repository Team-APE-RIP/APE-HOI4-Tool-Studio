#include "Setup.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QApplication>
#include <QThread>
#include <QSettings>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>

// --- SetupMessageBox Implementation ---
class SetupMessageBox : public QDialog {
public:
    enum Type { Information, Question, Critical };

    SetupMessageBox(QWidget *parent, const QString &title, const QString &message, Type type, bool isDark)
        : QDialog(parent), m_result(QMessageBox::No)
    {
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

        if (type == Question) {
            QPushButton *cancelBtn = new QPushButton("Cancel"); // In a real app, localize this
            cancelBtn->setObjectName("CancelBtn");
            connect(cancelBtn, &QPushButton::clicked, [this](){ 
                m_result = QMessageBox::No; 
                reject(); 
            });
            btnLayout->addWidget(cancelBtn);

            QPushButton *yesBtn = new QPushButton("Yes");
            connect(yesBtn, &QPushButton::clicked, [this](){ 
                m_result = QMessageBox::Yes; 
                accept(); 
            });
            btnLayout->addWidget(yesBtn);
        } else {
            QPushButton *okBtn = new QPushButton("OK");
            connect(okBtn, &QPushButton::clicked, [this](){ 
                m_result = QMessageBox::Ok; 
                accept(); 
            });
            btnLayout->addWidget(okBtn);
        }
        
        layout->addLayout(btnLayout);
    }

    QMessageBox::StandardButton result() const { return m_result; }

protected:
    void paintEvent(QPaintEvent *event) override {
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

// Helper function to copy directory
static bool copyDirectory(const QString &srcPath, const QString &dstPath, bool overwrite) {
    QDir srcDir(srcPath);
    if (!srcDir.exists()) return false;

    QDir dstDir(dstPath);
    if (!dstDir.exists()) {
        dstDir.mkpath(".");
    }

    bool success = true;
    QFileInfoList fileInfoList = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fileInfo : fileInfoList) {
        QString srcFilePath = fileInfo.filePath();
        QString dstFilePath = dstDir.filePath(fileInfo.fileName());

        if (fileInfo.isDir()) {
            success = copyDirectory(srcFilePath, dstFilePath, overwrite) && success;
        } else {
            if (QFile::exists(dstFilePath)) {
                if (overwrite) {
                    QFile::remove(dstFilePath);
                } else {
                    continue; // Skip if it exists and we shouldn't overwrite
                }
            }
            success = QFile::copy(srcFilePath, dstFilePath) && success;
        }
    }
    return success;
}

// Helper function to show the custom message box
static void showCustomMessageBox(QWidget *parent, const QString &title, const QString &message, SetupMessageBox::Type type, bool isDark) {
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
// ------------------------------------------

Setup::Setup(QWidget *parent) : QDialog(parent), currentLang("English"), m_isDarkMode(false), m_dragging(false), m_isAutoSetup(false) {
    m_isDarkMode = detectSystemDarkMode();
    
    // Remove system title bar, make frameless window
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setWindowIcon(QIcon(":/app.ico"));
    
    setupUi();

    // Determine initial language
    QString initialLang = "English";
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/setup_cache";
    QString tempLangFile = tempDir + "/temp_lang.json";
    
    // config.json is stored in %TEMP%/APE-HOI4-Tool-Studio/config.json
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
    QString configFile = configDir + "/config.json";

    bool useConfigLang = false;
    QString configLang;

    // Check config.json first
    QFile cFile(configFile);
    if (cFile.exists() && cFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(cFile.readAll());
        QJsonObject obj = doc.object();
        if (obj.contains("language")) {
            configLang = obj["language"].toString();
            if (configLang == "English" || configLang == "简体中文" || configLang == "繁體中文") {
                initialLang = configLang;
                useConfigLang = true;
            }
        }
        cFile.close();
    }

    // If config.json doesn't have it, check temp_lang.json
    if (!useConfigLang) {
        QFile tFile(tempLangFile);
        if (tFile.exists() && tFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(tFile.readAll());
            QJsonObject obj = doc.object();
            if (obj.contains("language")) {
                QString tempLang = obj["language"].toString();
                if (tempLang == "English" || tempLang == "简体中文" || tempLang == "繁體中文") {
                    initialLang = tempLang;
                }
            }
            tFile.close();
        }
    }

    // Set combo box without triggering changeLanguage yet
    langCombo->blockSignals(true);
    langCombo->setCurrentText(initialLang);
    langCombo->blockSignals(false);

    loadLanguage(initialLang);
    saveTempLanguage(); // Ensure temp file is created/updated with initial language

    applyTheme();
    
    setMinimumSize(500, 580);
    resize(500, 580);
}

Setup::~Setup() {}

bool Setup::detectSystemDarkMode() {
    // Use Windows Registry to detect system theme
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 
                       QSettings::NativeFormat);
    // AppsUseLightTheme: 0 = dark, 1 = light
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
        QPushButton#CancelButton {
            background-color: transparent;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 10px 30px;
            font-weight: 500;
            font-size: 14px;
        }
        QPushButton#CancelButton:hover {
            background-color: %7;
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
    // Main layout for the dialog (transparent background)
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);
    dialogLayout->setContentsMargins(0, 0, 0, 0);
    
    // Central widget with rounded corners and border
    m_centralWidget = new QWidget(this);
    m_centralWidget->setObjectName("CentralWidget");
    dialogLayout->addWidget(m_centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setSpacing(12);
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
    
    connect(closeBtn, &QPushButton::clicked, this, &Setup::closeWindow);
    connect(minBtn, &QPushButton::clicked, this, &QDialog::showMinimized);
    
    controlLayout->addWidget(closeBtn);
    controlLayout->addWidget(minBtn);
    controlLayout->addWidget(maxBtn);
    controlLayout->addStretch();
    
    topBarLayout->addWidget(controlContainer);
    topBarLayout->addStretch();
    
    // Language selector
    langCombo = new QComboBox(this);
    langCombo->addItem("English");
    langCombo->addItem("简体中文");
    langCombo->addItem("繁體中文");
    langCombo->setCursor(Qt::PointingHandCursor);
    connect(langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Setup::changeLanguage);
    topBarLayout->addWidget(langCombo);
    
    mainLayout->addLayout(topBarLayout);

    // Title
    titleLabel = new QLabel(this);
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

    // Add stretch to push the top elements up and keep them fixed
    mainLayout->addStretch();

    // Installation Path
    QVBoxLayout *pathLayout = new QVBoxLayout();
    pathLayout->setSpacing(6);
    pathLabel = new QLabel(this);
    pathLayout->addWidget(pathLabel);
    
    QHBoxLayout *pathInputLayout = new QHBoxLayout();
    pathInputLayout->setSpacing(8);
    pathEdit = new QLineEdit(this);
    
    // 默认安装路径改为 D:/APE HOI4 Tool Studio，如果有历史路径则优先使用
    QString defaultPath = "D:/APE HOI4 Tool Studio";
    QString pathJsonFile = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/path.json";
    QFile pFile(pathJsonFile);
    if (pFile.exists() && pFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(pFile.readAll());
        QJsonObject obj = doc.object();
        if (obj.contains("path")) {
            QString historyPath = obj["path"].toString();
            if (!historyPath.isEmpty()) {
                defaultPath = historyPath;
            }
        }
        if (obj.contains("auto") && obj["auto"].toString() == "1") {
            m_isAutoSetup = true;
        }
        pFile.close();
    }
    pathEdit->setText(defaultPath);
    
    browseBtn = new QPushButton(this);
    browseBtn->setObjectName("BrowseButton");
    browseBtn->setCursor(Qt::PointingHandCursor);
    connect(browseBtn, &QPushButton::clicked, this, &Setup::browseDirectory);
    
    pathInputLayout->addWidget(pathEdit);
    pathInputLayout->addWidget(browseBtn);
    pathLayout->addLayout(pathInputLayout);
    mainLayout->addLayout(pathLayout);

    mainLayout->addSpacing(10);

    // Progress Bar
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false); // 隐藏进度条上的文字
    progressBar->hide();
    mainLayout->addWidget(progressBar);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
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
    currentLang = langCode;
    
    QString folderName;
    if (langCode == "English") folderName = "en_US";
    else if (langCode == "简体中文") folderName = "zh_CN";
    else if (langCode == "繁體中文") folderName = "zh_TW";
    else folderName = "en_US"; // Default

    QFile file(QString(":/localization/%1.json").arg(folderName));
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        currentLoc = doc.object();
        file.close();
    } else {
        // Fallback to English if file not found
        QFile fallbackFile(":/localization/en_US.json");
        if (fallbackFile.open(QIODevice::ReadOnly)) {
            QByteArray data = fallbackFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            currentLoc = doc.object();
            fallbackFile.close();
        }
    }

    setWindowTitle(currentLoc["window_title"].toString("APE HOI4 Tool Studio - Setup"));
    titleLabel->setText(currentLoc["title"].toString("Install APE HOI4 Tool Studio"));
    pathLabel->setText(currentLoc["path_label"].toString("Installation Path:"));
    browseBtn->setText(currentLoc["browse_btn"].toString("Browse..."));
    installBtn->setText(currentLoc["install_btn"].toString("Install"));
}

void Setup::changeLanguage(int index) {
    QString langCode = langCombo->itemText(index);
    loadLanguage(langCode);
    saveTempLanguage(); // Real-time save
}

void Setup::browseDirectory() {
    QString dir = QFileDialog::getExistingDirectory(this, 
        currentLoc["select_dir_title"].toString("Select Installation Directory"),
        pathEdit->text());
        
    if (!dir.isEmpty()) {
        QDir d(dir);
        if (d.dirName() != "APE HOI4 Tool Studio") {
            // 使用 QDir::cleanPath 和 QDir::filePath 安全地拼接路径，避免双斜杠
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
        showCustomMessageBox(this, currentLoc["error_title"].toString("Error"), 
            currentLoc["error_empty_path"].toString("Installation path cannot be empty."), SetupMessageBox::Critical, m_isDarkMode);
        return;
    }

    // Kill any running instance of APEHOI4ToolStudio.exe before installation
    QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "APEHOI4ToolStudio.exe");

    // Hide UI elements during installation
    pathLabel->hide();
    pathEdit->hide();
    browseBtn->hide();
    installBtn->hide();

    QDir dir(targetPath);
    QString oldToolsPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/setup_cache/old_tools";
    
    if (dir.exists()) {
        // Backup tools
        QDir toolsDir(dir.filePath("tools"));
        if (toolsDir.exists()) {
            QDir oldToolsDir(oldToolsPath);
            if (oldToolsDir.exists()) {
                oldToolsDir.removeRecursively();
            }
            copyDirectory(toolsDir.absolutePath(), oldToolsPath, true);
        }
        
        // 尝试清空目录
        dir.removeRecursively();
    }
    
    if (!dir.mkpath(".")) {
        showCustomMessageBox(this, currentLoc["error_title"].toString("Error"), 
            currentLoc["error_create_dir"].toString("Failed to create installation directory."), SetupMessageBox::Critical, m_isDarkMode);
        return;
    }

    langCombo->setEnabled(false);
    
    progressBar->show();
    progressBar->setValue(10);

    // 提取文件
    QTimer::singleShot(100, this, [this, targetPath, oldToolsPath]() {
        if (extractPayload(targetPath)) {
            // Restore tools
            QDir oldToolsDir(oldToolsPath);
            if (oldToolsDir.exists()) {
                QDir dir(targetPath);
                QString newToolsPath = dir.filePath("tools");
                copyDirectory(oldToolsPath, newToolsPath, false); // Do not overwrite new tools
                oldToolsDir.removeRecursively(); // Clean up
            }
            
            progressBar->setValue(100);
            
            QString successTitle = currentLoc["success_title"].toString("Success");
            QString successMsg = currentLoc["success_msg"].toString("Installation completed successfully!");
            
            if (m_isAutoSetup) {
                if (currentLang == "简体中文") {
                    successTitle = "更新成功";
                    successMsg = "更新已成功完成！";
                } else if (currentLang == "繁體中文") {
                    successTitle = "更新成功";
                    successMsg = "更新已成功完成！";
                } else {
                    successTitle = "Update Success";
                    successMsg = "Update completed successfully!";
                }
            }
            
            showCustomMessageBox(this, successTitle, successMsg, SetupMessageBox::Information, m_isDarkMode);
            
            // 启动程序
            QProcess::startDetached(targetPath + "/APEHOI4ToolStudio.exe", QStringList());
            accept();
        } else {
            QString errorTitle = currentLoc["error_title"].toString("Error");
            QString errorMsg = currentLoc["error_extract"].toString("Failed to extract files. Installation aborted.");
            
            if (m_isAutoSetup) {
                if (currentLang == "简体中文") {
                    errorTitle = "更新失败";
                    errorMsg = "提取文件失败。更新已中止。";
                } else if (currentLang == "繁體中文") {
                    errorTitle = "更新失敗";
                    errorMsg = "提取文件失敗。更新已中止。";
                } else {
                    errorTitle = "Update Error";
                    errorMsg = "Failed to extract files. Update aborted.";
                }
            }
            
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
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/setup_cache";
    QDir().mkpath(tempDir);
    
    QFile file(tempDir + "/temp_lang.json");
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        obj["language"] = currentLang;
        QJsonDocument doc(obj);
        file.write(doc.toJson());
        file.close();
    }
}

bool Setup::extractPayload(const QString& targetDir) {
    progressBar->setValue(20);
    
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/setup_cache";
    QDir().mkpath(tempDir);
    
    QString tempArchive = tempDir + "/payload.7z";
    QString temp7zExe = tempDir + "/7z.exe";
    QString temp7zDll = tempDir + "/7z.dll";
    
    // 1. 释放 7z.exe 和 7z.dll
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
    
    // 2. 释放 payload.7z
    if (!extractResource(":/data/payload.7z", tempArchive)) return false;
    
    progressBar->setValue(50);
    
    // 3. 使用 7z.exe 解压
    QProcess process;
    QStringList args;
    // x: eXtract with full paths
    // -y: assume Yes on all queries
    // -o: set Output directory
    args << "x" << tempArchive << "-y" << QString("-o%1").arg(targetDir);
         
    process.start(temp7zExe, args);
    process.waitForFinished(-1); // 等待解压完成
    
    progressBar->setValue(90);
    
    // 4. 清理临时文件
    QFile::remove(tempArchive);
    QFile::remove(temp7zExe);
    QFile::remove(temp7zDll);
    
    return process.exitCode() == 0;
}

void Setup::closeWindow() {
    reject();
}

// Mouse event handlers for window dragging
void Setup::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void Setup::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton && m_dragging) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void Setup::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}
