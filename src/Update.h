//-------------------------------------------------------------------------------------
// Update.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef UPDATE_H
#define UPDATE_H

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "HttpClient.h"

struct UpdateFile {
    QString path;
    QString hash;
    QString url;
    QString preferredUrl;
    QString directUrl;
    qint64 size = -1;
    int priority = 0;
};

struct UpdateDownloadTask {
    int id = 0;
    UpdateFile file;
    QString targetPath;
    qint64 bytesReceived = 0;
    qint64 bytesTotal = 0;
    bool isUpdater = false;
};

class Update : public QWidget {
    Q_OBJECT

public:
    explicit Update(QWidget* parent = nullptr);
    ~Update();

    void checkForUpdates();
    void showCheckingOverlay();
    void updateTheme();

signals:
    void updateCheckCompleted(bool hasUpdate);
    void updateShutdownRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void onUpdateClicked();

    void updatePosition();
    void setupUi();
    void showUpdateDialog(const QString& version, const QString& changelog);
    void startUpdateProcess();

    void fetchManifest();
    void onManifestReceived(const HttpResponse& response);
    void processManifest(const QJsonObject& manifest);

    QString calculateFileHash(const QString& filePath);
    QString resolveDownloadUrl(const UpdateFile& file) const;
    int determineMaxParallelDownloads(const QJsonObject& manifest) const;

    void queueDownloadTask(const UpdateFile& file, const QString& targetPath, bool isUpdater);
    void startQueuedDownloads();
    void startDownloadTask(const UpdateDownloadTask& task);
    void onTaskProgress(int taskId, qint64 bytesReceived, qint64 bytesTotal);
    void onTaskFinished(int taskId, const HttpResponse& response);
    void updateAggregateProgress();
    void finishUpdate();
    void handleUpdateFailure();

    QWidget* m_container;

    QWidget* m_checkingWidget;
    QLabel* m_iconLabel;
    QLabel* m_checkingMessageLabel;
    QProgressBar* m_checkingProgressBar;

    QLabel* m_titleLabel;
    QLabel* m_versionLabel;
    QTextBrowser* m_changelogLabel;
    QProgressBar* m_progressBar;
    QLabel* m_progressTextLabel;
    QLabel* m_progressDetailLabel;
    QPushButton* m_updateBtn;
    QStackedWidget* m_bottomStack;

    QString m_tempDir;
    QList<UpdateFile> m_manifestFiles;
    QList<UpdateDownloadTask> m_pendingDownloads;
    QHash<int, UpdateDownloadTask> m_activeDownloads;

    QString m_appDir;
    QString m_latestVersion;

    int m_totalFilesToDownload = 0;
    int m_downloadedFilesCount = 0;
    int m_failedFilesCount = 0;
    int m_downloadTaskSequence = 0;
    int m_maxParallelDownloads = 4;

    qint64 m_totalBytesToDownload = 0;
    qint64 m_downloadedBytesTotal = 0;
    QElapsedTimer m_downloadTimer;
    bool m_hasTotalSize = false;

    bool m_isCheckingPhase = false;
    QTimer* m_retryTimer = nullptr;
};

#endif // UPDATE_H