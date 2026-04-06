//-------------------------------------------------------------------------------------
// FileManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QFutureWatcher>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <atomic>
#include "RecursiveFileSystemWatcher.h"

enum class FileSource : quint8 {
    Game = 0,
    Dlc = 1,
    Mod = 2
};

struct FileDetails {
    QString absPath;
    FileSource source = FileSource::Game;
    
    QString sourceString() const {
        switch (source) {
        case FileSource::Game:
            return "Game";
        case FileSource::Dlc:
            return "DLC";
        case FileSource::Mod:
            return "Mod";
        }
        return "Game";
    }
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["absPath"] = absPath;
        obj["source"] = sourceString();
        return obj;
    }
    
    static FileDetails fromJson(const QJsonObject& obj) {
        FileDetails fd;
        fd.absPath = obj["absPath"].toString();
        
        const QString sourceValue = obj["source"].toString();
        if (sourceValue == "Mod") {
            fd.source = FileSource::Mod;
        } else if (sourceValue == "DLC") {
            fd.source = FileSource::Dlc;
        } else {
            fd.source = FileSource::Game;
        }
        
        return fd;
    }
};

class FileManager : public QObject {
    Q_OBJECT

public:
    static FileManager& instance();

    void startScanning();
    void stopScanning();

    QMap<QString, FileDetails> getEffectiveFiles() const;
    QStringList getReplacePaths() const;
    int getFileCount() const;
    bool isScanning() const { return m_isScanning; }

    QJsonObject toJson() const;
    static void fromJson(const QJsonObject& obj, QMap<QString, FileDetails>& files, QStringList& replacePaths);
    void setFromJson(const QJsonObject& obj);

signals:
    void scanStarted();
    void scanFinished();
    void fileChanged(const QString& path);

private slots:
    void onFileChanged(const QString& path);
    void onDebounceTimerTimeout();
    void onScanFinished();

private:
    FileManager();

public:
    struct CompactFileRecord {
        QString absPath;
        FileSource source = FileSource::Game;
        qint64 lastModifiedMs = 0;
    };

    struct DlcCacheManifest {
        QString zipPath;
        qint64 zipSize = 0;
        qint64 zipLastModifiedMs = 0;
        int extractedFileCount = 0;

        QJsonObject toJson() const;
        static DlcCacheManifest fromJson(const QJsonObject& obj);
    };

    struct RootDescriptor {
        QString rootPath;
        bool isMod = false;
        bool isDlc = false;
        int rootId = -1;
    };

    struct ScanContext {
        QString gamePath;
        QString modPath;
        QStringList ignoreDirs;
        QStringList rootPaths;
        QSet<QString> replacePaths;
        QSet<QString> normalizedReplacePaths;
        const std::atomic_bool* stopRequested = nullptr;
        QThreadPool* scanThreadPool = nullptr;
        QThreadPool* extractThreadPool = nullptr;
    };

    struct ScanResult {
        QHash<QString, CompactFileRecord> files;
        QHash<QString, qint64> fileTimes;
        QSet<QString> replacePaths;
        QStringList watchedPaths;
        QStringList rootPaths;
    };

    struct PersistentCachePayload {
        QString gamePath;
        QString modPath;
        qint64 gamePathLastModifiedMs = 0;
        qint64 modPathLastModifiedMs = 0;
        qint64 modDescriptorLastModifiedMs = 0;
        QStringList rootPaths;
        QSet<QString> replacePaths;
        ScanResult scanResult;

        QJsonObject toJson() const;
        static PersistentCachePayload fromJson(const QJsonObject& obj);
    };

    static ScanResult doScan(const QString& gamePath,
                             const QString& modPath,
                             const QStringList& ignoreDirs,
                             const std::atomic_bool* stopRequested);

    static void scanGameDirectory(const ScanContext& context, ScanResult& result);
    static void scanModDirectory(const ScanContext& context, ScanResult& result);
    static void scanDlcDirectory(const ScanContext& context, ScanResult& result);

    static ScanResult scanDirectoryRecursive(const ScanContext& context,
                                            const RootDescriptor& root,
                                            const QString& currentPath);

    static bool processFile(const ScanContext& context,
                            const RootDescriptor& root,
                            const QString& absPath,
                            const QString& relPath,
                            qint64 lastModifiedMs,
                            ScanResult& result);

    static bool extractZip(const QString& zipPath, const QString& destPath);
    static bool ensureZipExtracted(const QString& zipPath,
                                   const QString& destPath,
                                   const std::atomic_bool* stopRequested);
    static bool isExtractedCacheReusable(const QString& zipPath,
                                         const QString& destPath,
                                         const std::atomic_bool* stopRequested);
    static int countFilesRecursively(const QString& rootPath, const std::atomic_bool* stopRequested);

    static QString getDlcCacheRootPath();
    static QString buildDlcCachePath(const QString& zipPath);
    static QString buildDlcManifestPath(const QString& destPath);
    static QString getPersistentCacheFilePath(const QString& gamePath, const QString& modPath);

    static bool loadDlcManifest(const QString& manifestPath, DlcCacheManifest& manifest);
    static bool saveDlcManifest(const QString& manifestPath, const DlcCacheManifest& manifest);
    static bool loadPersistentCache(const QString& cacheFilePath, PersistentCachePayload& payload);
    static bool savePersistentCache(const QString& cacheFilePath, const PersistentCachePayload& payload);
    static bool isPersistentCacheValid(const PersistentCachePayload& payload,
                                       const QString& gamePath,
                                       const QString& modPath);
    static PersistentCachePayload buildPersistentCachePayload(const ScanContext& context,
                                                              const QString& modDescriptorPath,
                                                              const ScanResult& result);

    static QString findPrimaryModDescriptorPath(const QString& modPath);
    static qint64 getPathLastModifiedMs(const QString& path);
    static QString normalizePathForComparison(QString path);
    static QString normalizeDlcPath(const QString& path);
    static QSet<QString> buildNormalizedReplacePathSet(const QSet<QString>& replacePaths);
    static void mergeScanResult(ScanResult& target, ScanResult&& source);
    static void convertScanResultToPublicData(const ScanResult& scanResult,
                                              QMap<QString, FileDetails>& files,
                                              QMap<QString, qint64>& fileTimes);
    static bool isIgnoredFile(const QString& absPath, const QString& relPath, bool isDlc);

private:
    QMap<QString, FileDetails> m_files;
    QMap<QString, qint64> m_fileTimes;
    QSet<QString> m_replacePaths;
    QStringList m_ignoreDirs;

    RecursiveFileSystemWatcher* m_watcher = nullptr;
    QTimer* m_debounceTimer = nullptr;
    QFutureWatcher<ScanResult>* m_futureWatcher = nullptr;
    mutable QMutex m_mutex;

    bool m_isScanning = false;
    std::atomic_bool m_stopRequested = false;
};

#endif // FILEMANAGER_H