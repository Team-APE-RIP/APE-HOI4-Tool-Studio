//-------------------------------------------------------------------------------------
// Update.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "Update.h"

#include "AuthManager.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QStandardPaths>
#include <QTextStream>
#include <QtGlobal>

#include <algorithm>
#include <limits>
#include <utility>

namespace {
QString formatBytesToMb(qint64 bytes) {
    const double mb = bytes / 1024.0 / 1024.0;
    return QString::number(mb, 'f', 2);
}

QString formatDuration(qint64 seconds) {
    if (seconds < 0) {
        return QString("--:--");
    }

    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    const qint64 secs = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}
}

Update::Update(QWidget* parent)
    : QWidget(parent) {
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
}

void Update::setupUi() {
    m_container = new QWidget(this);
    m_container->setObjectName("UpdateContainer");
    m_container->setFixedSize(400, 300);

    QVBoxLayout* layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(30, 25, 30, 25);
    layout->setSpacing(15);

    m_checkingWidget = new QWidget(m_container);
    QVBoxLayout* checkingLayout = new QVBoxLayout(m_checkingWidget);
    checkingLayout->setContentsMargins(0, 0, 0, 0);
    checkingLayout->setSpacing(15);
    checkingLayout->setAlignment(Qt::AlignCenter);

    m_iconLabel = new QLabel(m_checkingWidget);
    m_iconLabel->setPixmap(QPixmap(":/app.ico"));
    m_iconLabel->setAlignment(Qt::AlignCenter);
    checkingLayout->addWidget(m_iconLabel);

    m_checkingMessageLabel = new QLabel(m_checkingWidget);
    m_checkingMessageLabel->setObjectName("UpdateCheckingMessage");
    m_checkingMessageLabel->setAlignment(Qt::AlignCenter);
    m_checkingMessageLabel->setWordWrap(true);
    checkingLayout->addWidget(m_checkingMessageLabel);

    m_checkingProgressBar = new QProgressBar(m_checkingWidget);
    m_checkingProgressBar->setObjectName("UpdateCheckingProgressBar");
    m_checkingProgressBar->setTextVisible(false);
    m_checkingProgressBar->setFixedHeight(6);
    m_checkingProgressBar->setRange(0, 0);
    checkingLayout->addWidget(m_checkingProgressBar);

    layout->addWidget(m_checkingWidget);
    m_checkingWidget->hide();

    m_titleLabel = new QLabel(m_container);
    m_titleLabel->setObjectName("UpdateTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);

    m_versionLabel = new QLabel(m_container);
    m_versionLabel->setObjectName("UpdateVersion");
    m_versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_versionLabel);

    m_changelogLabel = new QTextBrowser(m_container);
    m_changelogLabel->setObjectName("UpdateChangelog");
    m_changelogLabel->setReadOnly(true);
    m_changelogLabel->setOpenExternalLinks(true);
    m_changelogLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_changelogLabel);

    m_bottomStack = new QStackedWidget(m_container);
    m_bottomStack->setFixedHeight(50);

    QWidget* btnPage = new QWidget();
    QHBoxLayout* btnLayout = new QHBoxLayout(btnPage);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    m_updateBtn = new QPushButton(btnPage);
    m_updateBtn->setObjectName("UpdateBtn");
    m_updateBtn->setCursor(Qt::PointingHandCursor);
    m_updateBtn->setFixedHeight(32);
    connect(m_updateBtn, &QPushButton::clicked, this, &Update::onUpdateClicked);
    btnLayout->addWidget(m_updateBtn);

    m_bottomStack->addWidget(btnPage);

    QWidget* progressPage = new QWidget();
    QVBoxLayout* progressLayout = new QVBoxLayout(progressPage);
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

    m_progressDetailLabel = new QLabel(progressPage);
    m_progressDetailLabel->setObjectName("UpdateProgressDetail");
    m_progressDetailLabel->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(m_progressDetailLabel);

    m_bottomStack->addWidget(progressPage);
    layout->addWidget(m_bottomStack);
}

void Update::updateTheme() {
    const bool isDark = ConfigManager::instance().isCurrentThemeDark();

    const QString containerBg = isDark ? "#2C2C2E" : "#FFFFFF";
    const QString textColor = isDark ? "#FFFFFF" : "#1D1D1F";
    const QString secondaryTextColor = isDark ? "#8E8E93" : "#86868B";
    const QString borderColor = isDark ? "#3A3A3C" : "#D2D2D7";
    const QString progressBg = isDark ? "#3A3A3C" : "#E5E5EA";
    const QString progressChunk = "#007AFF";
    const QString primaryBtnBg = "#007AFF";
    const QString primaryBtnHoverBg = "#0062CC";

    m_container->setStyleSheet(QString(
        "QWidget#UpdateContainer {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
    ).arg(containerBg, borderColor));

    m_titleLabel->setStyleSheet(QString("color: %1;").arg(textColor));
    m_versionLabel->setStyleSheet(QString("color: %1; font-weight: 500;").arg(primaryBtnBg));
    m_changelogLabel->setStyleSheet(QString(
        "QTextBrowser#UpdateChangelog {"
        "  color: %1;"
        "  border: none;"
        "  background: transparent;"
        "}"
    ).arg(secondaryTextColor));
    m_progressTextLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(secondaryTextColor));
    m_progressDetailLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(secondaryTextColor));

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

    m_checkingMessageLabel->setStyleSheet(QString(
        "QLabel#UpdateCheckingMessage {"
        "  color: %1;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
    ).arg(textColor));

    m_checkingProgressBar->setStyleSheet(QString(
        "QProgressBar#UpdateCheckingProgressBar {"
        "  background-color: %1;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        "QProgressBar#UpdateCheckingProgressBar::chunk {"
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

    m_titleLabel->setText(LocalizationManager::instance().getString("Update", "new_version_title"));
    m_updateBtn->setText(LocalizationManager::instance().getString("Update", "update_now"));
}

void Update::checkForUpdates() {
    fetchManifest();
}

void Update::showCheckingOverlay() {
    m_isCheckingPhase = true;

    m_titleLabel->hide();
    m_versionLabel->hide();
    m_changelogLabel->hide();
    m_bottomStack->hide();

    m_checkingWidget->show();
    m_checkingMessageLabel->setText(LocalizationManager::instance().getString("Update", "checking_for_updates"));

    m_container->setFixedSize(350, 400);

    if (parentWidget()) {
        raise();
        updatePosition();
    }
    show();
}

void Update::fetchManifest() {
    HttpRequestOptions options = HttpClient::createGet(QUrl(AuthManager::getApiBaseUrl() + "/api/v1/update/manifest"));
    HttpClient::addOrReplaceHeader(options, "User-Agent", "APE-HOI4-Tool-Studio-Updater");

    const QString token = AuthManager::instance().getToken();
    if (!token.isEmpty()) {
        HttpClient::addOrReplaceHeader(options, "Authorization", QString("Bearer %1").arg(token).toUtf8());
    }

    HttpClient::addOrReplaceHeader(options, "X-Update-Channel", AuthManager::instance().getChannel().toUtf8());
    HttpClient::addOrReplaceHeader(options, "X-Current-Version", QString(APP_VERSION).toUtf8());
    HttpClient::addOrReplaceHeader(options, "X-Lang", LocalizationManager::instance().currentLang().toUtf8());

    options.category = HttpRequestCategory::Manifest;
    options.timeoutMs = 12000;
    options.connectTimeoutMs = 3500;
    options.maxRetries = 2;
    options.retryOnHttp5xx = true;
    options.retryOnTimeout = true;
    options.retryBackoffMs = 350;
    options.httpVersionPolicy = HttpVersionPolicy::PreferHttp2;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;

    HttpClient::instance().send(options, this, [this](const HttpResponse& response) {
        onManifestReceived(response);
    });
}

void Update::onManifestReceived(const HttpResponse& response) {
    if (!response.success) {
        qDebug() << "Update check failed:" << response.errorMessage;

        if (m_isCheckingPhase) {
            m_checkingMessageLabel->setText(LocalizationManager::instance().getString("Update", "check_failed_retrying"));

            if (!m_retryTimer) {
                m_retryTimer = new QTimer(this);
                m_retryTimer->setSingleShot(true);
                connect(m_retryTimer, &QTimer::timeout, this, &Update::fetchManifest);
            }
            m_retryTimer->start(5000);
        }
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(response.body);
    if (!doc.isObject()) {
        if (m_isCheckingPhase) {
            m_checkingMessageLabel->setText(LocalizationManager::instance().getString("Update", "check_failed_retrying"));
            if (!m_retryTimer) {
                m_retryTimer = new QTimer(this);
                m_retryTimer->setSingleShot(true);
                connect(m_retryTimer, &QTimer::timeout, this, &Update::fetchManifest);
            }
            m_retryTimer->start(5000);
        }
        return;
    }

    processManifest(doc.object());
}

int Update::determineMaxParallelDownloads(const QJsonObject& manifest) const {
    int parallel = manifest.value("max_parallel_downloads").toInt(0);

    const QJsonObject parallelObject = manifest.value("parallel_download").toObject();
    if (parallel <= 0 && !parallelObject.isEmpty()) {
        parallel = parallelObject.value("max_parallel").toInt(0);
    }

    const QJsonObject transportHints = manifest.value("transport_hints").toObject();
    if (parallel <= 0 && !transportHints.isEmpty()) {
        parallel = transportHints.value("max_parallel").toInt(0);
    }

    if (parallel <= 0) {
        parallel = 4;
    }

    return qBound(1, parallel, 8);
}

void Update::processManifest(const QJsonObject& manifest) {
    const QString latestVersion = manifest["version"].toString();
    const QString changelog = manifest["combined_changelog"].toString();

    const QString currentVersion = APP_VERSION;
    if (latestVersion != currentVersion) {
        m_latestVersion = latestVersion;
        m_manifestFiles.clear();

        const QJsonArray filesArray = manifest["files"].toArray();
        for (const QJsonValue& val : filesArray) {
            const QJsonObject fileObj = val.toObject();
            UpdateFile uf;
            uf.path = fileObj["path"].toString();
            uf.hash = fileObj["hash"].toString();
            uf.url = fileObj["url"].toString();
            uf.preferredUrl = fileObj["preferred_url"].toString();
            uf.directUrl = fileObj["direct_url"].toString();
            uf.size = fileObj["size"].toVariant().toLongLong();
            uf.priority = fileObj["priority"].toInt(0);
            m_manifestFiles.append(uf);
        }

        m_maxParallelDownloads = determineMaxParallelDownloads(manifest);

        const bool wasChecking = m_isCheckingPhase;
        m_isCheckingPhase = false;

        showUpdateDialog(latestVersion, changelog);

        if (wasChecking) {
            emit updateCheckCompleted(true);
        }
    } else {
        if (m_isCheckingPhase) {
            m_isCheckingPhase = false;
            hide();
            emit updateCheckCompleted(false);
        }
    }
}

void Update::showUpdateDialog(const QString& version, const QString& changelog) {
    m_checkingWidget->hide();

    m_titleLabel->show();
    m_versionLabel->show();
    m_changelogLabel->show();
    m_bottomStack->show();
    m_bottomStack->setCurrentIndex(0);
    m_progressBar->setRange(0, 100);
    m_container->setFixedSize(400, 300);

    m_versionLabel->setText(QString("v%1 -> v%2").arg(APP_VERSION, version));
    m_changelogLabel->setMarkdown(changelog);

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
    m_progressDetailLabel->clear();

    startUpdateProcess();
}

QString Update::calculateFileHash(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        return QString(hash.result().toHex());
    }
    return QString();
}

QString Update::resolveDownloadUrl(const UpdateFile& file) const {
    const QString candidate = !file.preferredUrl.isEmpty()
        ? file.preferredUrl
        : (!file.directUrl.isEmpty() ? file.directUrl : file.url);

    if (candidate.startsWith("http://", Qt::CaseInsensitive) || candidate.startsWith("https://", Qt::CaseInsensitive)) {
        return candidate;
    }

    return AuthManager::getApiBaseUrl() + candidate;
}

void Update::queueDownloadTask(const UpdateFile& file, const QString& targetPath, bool isUpdater) {
    UpdateDownloadTask task;
    task.id = ++m_downloadTaskSequence;
    task.file = file;
    task.targetPath = targetPath;
    task.isUpdater = isUpdater;
    task.bytesReceived = 0;
    task.bytesTotal = file.size;

    m_pendingDownloads.append(task);
}

void Update::startUpdateProcess() {
    m_tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/update_cache";
    QDir().mkpath(m_tempDir);

    m_pendingDownloads.clear();
    m_activeDownloads.clear();
    m_downloadTaskSequence = 0;
    m_failedFilesCount = 0;

    m_appDir = QCoreApplication::applicationDirPath();

    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "checking_local_files"));
    m_progressDetailLabel->clear();
    QApplication::processEvents();

    m_totalFilesToDownload = 0;
    m_downloadedFilesCount = 0;
    m_totalBytesToDownload = 0;
    m_downloadedBytesTotal = 0;
    m_hasTotalSize = true;

    for (const UpdateFile& uf : std::as_const(m_manifestFiles)) {
        const QString localPath = QDir(m_appDir).filePath(uf.path);
        const QString localHash = calculateFileHash(localPath);

        if (localHash == uf.hash) {
            continue;
        }

        const bool isUpdater = QFileInfo(uf.path).fileName().compare("Updater.exe", Qt::CaseInsensitive) == 0;
        const QString targetPath = isUpdater
            ? QDir(m_appDir).filePath(uf.path)
            : QDir(m_tempDir).filePath(uf.path);

        queueDownloadTask(uf, targetPath, isUpdater);

        m_totalFilesToDownload++;
        if (uf.size <= 0) {
            m_hasTotalSize = false;
        } else if (m_hasTotalSize) {
            m_totalBytesToDownload += uf.size;
        }
    }

    if (m_totalFilesToDownload == 0) {
        m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "up_to_date"));
        QTimer::singleShot(1500, this, &Update::hide);
        return;
    }

    std::sort(m_pendingDownloads.begin(), m_pendingDownloads.end(), [](const UpdateDownloadTask& left, const UpdateDownloadTask& right) {
        if (left.isUpdater != right.isUpdater) {
            return left.isUpdater;
        }
        if (left.file.priority != right.file.priority) {
            return left.file.priority > right.file.priority;
        }

        const qint64 leftSize = left.file.size > 0 ? left.file.size : std::numeric_limits<qint64>::max();
        const qint64 rightSize = right.file.size > 0 ? right.file.size : std::numeric_limits<qint64>::max();
        if (leftSize != rightSize) {
            return leftSize < rightSize;
        }

        return left.file.path < right.file.path;
    });

    m_downloadTimer.invalidate();
    m_downloadTimer.start();

    startQueuedDownloads();
}

void Update::startQueuedDownloads() {
    while (m_activeDownloads.size() < m_maxParallelDownloads && !m_pendingDownloads.isEmpty()) {
        const UpdateDownloadTask task = m_pendingDownloads.takeFirst();
        startDownloadTask(task);
    }

    if (m_activeDownloads.isEmpty() && m_pendingDownloads.isEmpty()) {
        if (m_failedFilesCount > 0) {
            handleUpdateFailure();
        } else {
            finishUpdate();
        }
    }
}

void Update::startDownloadTask(const UpdateDownloadTask& task) {
    QFileInfo fi(task.targetPath);
    QDir().mkpath(fi.absolutePath());

    m_activeDownloads.insert(task.id, task);
    updateAggregateProgress();

    const QString resolvedUrl = resolveDownloadUrl(task.file);
    const bool isLargeDownload = task.file.size > 8 * 1024 * 1024;
    const bool isVeryLargeDownload = task.file.size > 64 * 1024 * 1024;

    HttpRequestOptions options = HttpClient::createGet(QUrl(resolvedUrl));
    HttpClient::addOrReplaceHeader(options, "User-Agent", "APE-HOI4-Tool-Studio-Updater");
    HttpClient::addOrReplaceHeader(options, "Accept", "*/*");
    HttpClient::addOrReplaceHeader(options, "Cache-Control", "no-cache");
    options.category = isLargeDownload ? HttpRequestCategory::LargeDownload : HttpRequestCategory::UpdateDownload;
    options.timeoutMs = isVeryLargeDownload ? 420000 : (isLargeDownload ? 240000 : 120000);
    options.connectTimeoutMs = 5000;
    options.lowSpeedLimitBytesPerSecond = isVeryLargeDownload ? 512 : 1024;
    options.lowSpeedTimeSeconds = isVeryLargeDownload ? 45 : 25;
    options.maxRetries = isVeryLargeDownload ? 3 : 2;
    options.retryOnHttp5xx = true;
    options.retryOnTimeout = true;
    options.retryBackoffMs = isVeryLargeDownload ? 1200 : 700;
    options.httpVersionPolicy = HttpVersionPolicy::ForceHttp11;
    options.allowHttp11Fallback = true;
    options.allowIpv4Fallback = true;
    options.ipResolvePolicy = HttpIpResolvePolicy::PreferIpv4;
    options.enableConnectionReuse = true;
    options.enableDnsCache = true;
    options.enableTcpKeepAlive = true;

    HttpClient::instance().downloadToFile(
        options,
        task.targetPath,
        this,
        [this, taskId = task.id](const HttpResponse& response) {
            onTaskFinished(taskId, response);
        },
        [this, taskId = task.id](qint64 bytesReceived, qint64 bytesTotal) {
            onTaskProgress(taskId, bytesReceived, bytesTotal);
        }
    );
}

void Update::onTaskProgress(int taskId, qint64 bytesReceived, qint64 bytesTotal) {
    auto it = m_activeDownloads.find(taskId);
    if (it == m_activeDownloads.end()) {
        return;
    }

    it->bytesReceived = qMax<qint64>(0, bytesReceived);
    if (bytesTotal > 0) {
        it->bytesTotal = bytesTotal;
    }

    updateAggregateProgress();
}

void Update::onTaskFinished(int taskId, const HttpResponse& response) {
    auto it = m_activeDownloads.find(taskId);
    if (it == m_activeDownloads.end()) {
        startQueuedDownloads();
        return;
    }

    UpdateDownloadTask task = it.value();
    m_activeDownloads.erase(it);

    if (response.success) {
        qint64 completedBytes = task.bytesTotal;
        if (completedBytes <= 0) {
            completedBytes = QFileInfo(task.targetPath).size();
        }

        if (completedBytes > 0) {
            m_downloadedBytesTotal += completedBytes;
        }

        m_downloadedFilesCount++;
    } else {
        m_failedFilesCount++;
        qDebug() << "Download failed:" << task.file.path << response.errorMessage << response.diagnostics;
    }

    updateAggregateProgress();
    startQueuedDownloads();
}

void Update::updateAggregateProgress() {
    qint64 activeBytes = 0;
    double activeFileProgress = 0.0;
    QString displayFileName;

    for (auto it = m_activeDownloads.cbegin(); it != m_activeDownloads.cend(); ++it) {
        activeBytes += it->bytesReceived;
        if (it->bytesTotal > 0) {
            activeFileProgress += static_cast<double>(it->bytesReceived) / static_cast<double>(it->bytesTotal);
        }
        if (displayFileName.isEmpty()) {
            displayFileName = QFileInfo(it->file.path).fileName();
        }
    }

    if (displayFileName.isEmpty() && !m_pendingDownloads.isEmpty()) {
        displayFileName = QFileInfo(m_pendingDownloads.first().file.path).fileName();
    }

    if (displayFileName.isEmpty()) {
        displayFileName = m_latestVersion;
    }

    const int completedForDisplay = qMin(m_totalFilesToDownload, m_downloadedFilesCount + 1);
    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "downloading_file")
        .arg(displayFileName)
        .arg(completedForDisplay)
        .arg(m_totalFilesToDownload));

    const qint64 totalSoFar = m_downloadedBytesTotal + activeBytes;
    if (m_hasTotalSize && m_totalBytesToDownload > 0) {
        const int overallPercent = static_cast<int>(
            (static_cast<double>(totalSoFar) / static_cast<double>(m_totalBytesToDownload)) * 100.0
        );
        m_progressBar->setValue(qBound(0, overallPercent, 100));
    } else if (m_totalFilesToDownload > 0) {
        const double completedFiles = static_cast<double>(m_downloadedFilesCount) + activeFileProgress;
        const int overallPercent = static_cast<int>((completedFiles / static_cast<double>(m_totalFilesToDownload)) * 100.0);
        m_progressBar->setValue(qBound(0, overallPercent, 100));
    }

    const double elapsedSeconds = m_downloadTimer.isValid() ? (m_downloadTimer.elapsed() / 1000.0) : 0.0;
    const double speedBytes = (elapsedSeconds > 0.0) ? (totalSoFar / elapsedSeconds) : 0.0;
    const double speedMb = speedBytes / 1024.0 / 1024.0;
    const QString speedText = QString::number(speedMb, 'f', 2);

    if (m_hasTotalSize && m_totalBytesToDownload > 0) {
        const qint64 remainingBytes = qMax<qint64>(0, m_totalBytesToDownload - totalSoFar);
        const qint64 etaSeconds = (speedBytes > 0.0) ? static_cast<qint64>(remainingBytes / speedBytes) : -1;
        m_progressDetailLabel->setText(LocalizationManager::instance().getString("Update", "download_detail_with_total")
            .arg(formatBytesToMb(totalSoFar))
            .arg(formatBytesToMb(m_totalBytesToDownload))
            .arg(speedText)
            .arg(formatDuration(etaSeconds)));
    } else {
        m_progressDetailLabel->setText(LocalizationManager::instance().getString("Update", "download_detail")
            .arg(formatBytesToMb(totalSoFar))
            .arg(speedText));
    }
}

void Update::handleUpdateFailure() {
    m_bottomStack->setCurrentIndex(0);
    m_updateBtn->setText(LocalizationManager::instance().getString("Update", "update_now"));
    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "check_failed_retrying"));
}

void Update::finishUpdate() {
    m_progressBar->setValue(100);
    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "download_complete"));

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString updaterPath = QDir(appDir).filePath("Updater.exe");

    if (!QFile::exists(updaterPath)) {
        qDebug() << "Updater.exe not found at:" << updaterPath;
        m_bottomStack->setCurrentIndex(0);
        m_updateBtn->setText(LocalizationManager::instance().getString("Update", "update_now"));
        return;
    }

    const QString manifestListPath = QDir(m_tempDir).filePath("manifest_files.txt");
    QFile manifestListFile(manifestListPath);
    if (manifestListFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&manifestListFile);
        for (const UpdateFile& uf : std::as_const(m_manifestFiles)) {
            stream << uf.path << "\n";
        }
        manifestListFile.close();
    } else {
        qDebug() << "Failed to write manifest_files.txt";
    }

    QStringList args;
    args << appDir
         << m_tempDir
         << QString::number(QCoreApplication::applicationPid());

    const bool updaterStarted = QProcess::startDetached(updaterPath, args);
    if (!updaterStarted) {
        qDebug() << "Failed to start Updater.exe:" << updaterPath << args;
        m_bottomStack->setCurrentIndex(0);
        m_updateBtn->setText(LocalizationManager::instance().getString("Update", "update_now"));
        return;
    }

    m_progressTextLabel->setText(LocalizationManager::instance().getString("Update", "download_complete"));
    m_progressDetailLabel->setText("Closing main application...");
    QApplication::processEvents();

    emit updateShutdownRequested();
}

void Update::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    const QRectF r = rect();
    const qreal radius = 9;

    path.addRoundedRect(r, radius, radius);
    painter.fillPath(path, QColor(0, 0, 0, 120));
}

bool Update::eventFilter(QObject* obj, QEvent* event) {
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