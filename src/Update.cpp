#include "Update.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>
#include <QDebug>

Update::Update(QWidget *parent)
    : QWidget(parent), m_networkManager(new QNetworkAccessManager(this)), m_downloadReply(nullptr), m_downloadFile(nullptr)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setupUi();
    updateTheme();
    
    hide();
    
    if (parent) {
        parent->installEventFilter(this);
    }
}

Update::~Update() {
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
    }
}

void Update::setupUi() {
    m_container = new QWidget(this);
    m_container->setObjectName("UpdateContainer");
    m_container->setFixedSize(400, 300);
    
    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(15);
    
    // Title
    m_titleLabel = new QLabel(m_container);
    m_titleLabel->setObjectName("UpdateTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);
    
    // Version
    m_versionLabel = new QLabel(m_container);
    m_versionLabel->setObjectName("UpdateVersion");
    m_versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_versionLabel);
    
    // Changelog
    m_changelogLabel = new QLabel(m_container);
    m_changelogLabel->setObjectName("UpdateChangelog");
    m_changelogLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_changelogLabel->setWordWrap(true);
    m_changelogLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_changelogLabel);
    
    // Bottom Stack (Button vs Progress)
    m_bottomStack = new QStackedWidget(m_container);
    m_bottomStack->setFixedHeight(50); // Fixed height to prevent jumping
    
    // Page 0: Update Button
    QWidget *btnPage = new QWidget();
    QHBoxLayout *btnLayout = new QHBoxLayout(btnPage);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    
    m_updateBtn = new QPushButton(btnPage);
    m_updateBtn->setObjectName("UpdateBtn");
    m_updateBtn->setCursor(Qt::PointingHandCursor);
    m_updateBtn->setFixedHeight(32);
    connect(m_updateBtn, &QPushButton::clicked, this, &Update::onUpdateClicked);
    btnLayout->addWidget(m_updateBtn);
    
    m_bottomStack->addWidget(btnPage);
    
    // Page 1: Progress Bar and Text
    QWidget *progressPage = new QWidget();
    QVBoxLayout *progressLayout = new QVBoxLayout(progressPage);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(5);
    
    m_progressBar = new QProgressBar(progressPage);
    m_progressBar->setObjectName("UpdateProgressBar");
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setRange(0, 100);
    progressLayout->addWidget(m_progressBar);
    
    m_progressTextLabel = new QLabel(progressPage);
    m_progressTextLabel->setObjectName("UpdateProgressText");
    m_progressTextLabel->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(m_progressTextLabel);
    
    m_bottomStack->addWidget(progressPage);
    
    layout->addWidget(m_bottomStack);
}

void Update::updateTheme() {
    bool isDark = ConfigManager::instance().isCurrentThemeDark();
    
    QString containerBg = isDark ? "#2C2C2E" : "#FFFFFF";
    QString textColor = isDark ? "#FFFFFF" : "#1D1D1F";
    QString secondaryTextColor = isDark ? "#8E8E93" : "#86868B";
    QString borderColor = isDark ? "#3A3A3C" : "#D2D2D7";
    QString progressBg = isDark ? "#3A3A3C" : "#E5E5EA";
    QString progressChunk = "#007AFF";
    QString btnBg = isDark ? "#3A3A3C" : "#F2F2F7";
    QString btnHoverBg = isDark ? "#48484A" : "#E5E5EA";
    QString primaryBtnBg = "#007AFF";
    QString primaryBtnHoverBg = "#0062CC";
    
    m_container->setStyleSheet(QString(
        "QWidget#UpdateContainer {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(containerBg, borderColor));
    
    m_titleLabel->setStyleSheet(QString("color: %1;").arg(textColor));
    m_versionLabel->setStyleSheet(QString("color: %1; font-weight: 500;").arg(primaryBtnBg));
    m_changelogLabel->setStyleSheet(QString("color: %1;").arg(secondaryTextColor));
    m_progressTextLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(secondaryTextColor));
    
    m_progressBar->setStyleSheet(QString(
        "QProgressBar#UpdateProgressBar {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        "QProgressBar#UpdateProgressBar::chunk {"
        "  background-color: %2;"
        "  border-radius: 3px;"
        "}"
    ).arg(progressBg, progressChunk));
    
    m_updateBtn->setStyleSheet(QString(
        "QPushButton#UpdateBtn {"
        "  background-color: %1;"
        "  color: #FFFFFF;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#UpdateBtn:hover {"
        "  background-color: %2;"
        "}"
    ).arg(primaryBtnBg, primaryBtnHoverBg));

    // Update texts
    m_titleLabel->setText(LocalizationManager::instance().getString("Update", "new_version_title"));
    m_updateBtn->setText(LocalizationManager::instance().getString("Update", "update_now"));
}

void Update::checkForUpdates() {
    QNetworkRequest request(QUrl("https://api.github.com/repos/Team-APE-RIP/APE-HOI4-Tool-Studio/releases/latest"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "APE-HOI4-Tool-Studio-Updater");
    
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onCheckFinished(reply);
    });
}

void Update::onCheckFinished(QNetworkReply *reply) {
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Update check failed:" << reply->errorString();
        return;
    }
    
    QByteArray response = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) return;
    
    QJsonObject obj = doc.object();
    QString tagName = obj["tag_name"].toString();
    QString body = obj["body"].toString();
    
    // Clean tag name (e.g., "v1.1.0" -> "1.1.0")
    if (tagName.startsWith("v", Qt::CaseInsensitive)) {
        tagName = tagName.mid(1);
    }
    
    // Compare versions
    QString currentVersion = APP_VERSION;
    if (tagName > currentVersion) {
        // Find download URL for Setup.exe
        QString downloadUrl;
        QJsonArray assets = obj["assets"].toArray();
        for (const QJsonValue &val : assets) {
            QJsonObject asset = val.toObject();
            if (asset["name"].toString() == "Setup.exe") {
                downloadUrl = asset["browser_download_url"].toString();
                break;
            }
        }
        
        if (!downloadUrl.isEmpty()) {
            showUpdateDialog(tagName, body, downloadUrl);
        }
    }
}

void Update::showUpdateDialog(const QString& version, const QString& changelog, const QString& downloadUrl) {
    m_downloadUrl = downloadUrl;
    
    m_versionLabel->setText(QString("v%1 -> v%2").arg(APP_VERSION, version));
    m_changelogLabel->setText(changelog);
    
    updateTheme();
    
    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void Update::onUpdateClicked() {
    m_bottomStack->setCurrentIndex(1);
    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "starting_download"));
    
    startDownload();
}

void Update::startDownload() {
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/setup_cache";
    QDir().mkpath(tempDir);
    m_tempFilePath = tempDir + "/Setup.exe";
    
    m_downloadFile = new QFile(m_tempFilePath);
    if (!m_downloadFile->open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open temp file for download:" << m_tempFilePath;
        hide();
        return;
    }
    
    QNetworkRequest request((QUrl(m_downloadUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "APE-HOI4-Tool-Studio-Updater");
    
    // Handle redirects
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    m_downloadReply = m_networkManager->get(request);
    
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &Update::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::readyRead, this, &Update::onDownloadReadyRead);
    connect(m_downloadReply, &QNetworkReply::finished, this, &Update::onDownloadFinished);
}

void Update::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        int percent = (bytesReceived * 100) / bytesTotal;
        m_progressBar->setValue(percent);
        
        double mbReceived = bytesReceived / 1024.0 / 1024.0;
        double mbTotal = bytesTotal / 1024.0 / 1024.0;
        
        m_progressTextLabel->setText(QString("%1% - %2 MB / %3 MB")
            .arg(percent)
            .arg(mbReceived, 0, 'f', 2)
            .arg(mbTotal, 0, 'f', 2));
    }
}

void Update::onDownloadReadyRead() {
    if (m_downloadFile) {
        m_downloadFile->write(m_downloadReply->readAll());
    }
}

void Update::onDownloadFinished() {
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
    
    if (m_downloadReply->error() == QNetworkReply::NoError) {
        m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "download_complete"));
        
        // Update path.json to trigger auto-update mode in Setup.exe
        QString pathJsonDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
        QFile pathFile(pathJsonDir + "/path.json");
        if (pathFile.open(QIODevice::ReadWrite)) {
            QByteArray data = pathFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            
            obj["auto"] = "1";
            
            pathFile.resize(0);
            pathFile.write(QJsonDocument(obj).toJson());
            pathFile.close();
        }
        
        // Start Setup.exe
        QProcess::startDetached(m_tempFilePath, QStringList());
        
        // Quit application
        QApplication::quit();
    } else {
        qDebug() << "Download failed:" << m_downloadReply->errorString();
        m_bottomStack->setCurrentIndex(0);
        m_updateBtn->setText(LocalizationManager::instance().getString("Update", "download_failed") + " - Retry");
    }
    
    m_downloadReply->deleteLater();
    m_downloadReply = nullptr;
}

void Update::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QPainterPath path;
    QRectF r = rect();
    qreal radius = 10;
    
    path.addRoundedRect(r, radius, radius);
    
    painter.fillPath(path, QColor(0, 0, 0, 120));
}

bool Update::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updatePosition();
    }
    return QWidget::eventFilter(obj, event);
}

void Update::updatePosition() {
    if (parentWidget()) {
        setGeometry(parentWidget()->rect());
        m_container->move(
            (width() - m_container->width()) / 2,
            (height() - m_container->height()) / 2
        );
    }
}
