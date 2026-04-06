//-------------------------------------------------------------------------------------
// FileManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "FileManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

namespace {
constexpr int kExpectedDlcCacheFileCount = 368;
constexpr int kPersistentCacheVersion = 2;

QString sourceToString(FileSource source) {
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

FileSource sourceFromString(const QString& value) {
    if (value == "Mod") return FileSource::Mod;
    if (value == "DLC") return FileSource::Dlc;
    return FileSource::Game;
}

QJsonObject compactRecordToJson(const FileManager::CompactFileRecord& record) {
    QJsonObject obj;
    obj["absPath"] = record.absPath;
    obj["source"] = sourceToString(record.source);
    obj["lastModifiedMs"] = QString::number(record.lastModifiedMs);
    return obj;
}

FileManager::CompactFileRecord compactRecordFromJson(const QJsonObject& obj) {
    FileManager::CompactFileRecord record;
    record.absPath = obj["absPath"].toString();
    record.source = sourceFromString(obj["source"].toString());
    record.lastModifiedMs = obj["lastModifiedMs"].toString().toLongLong();
    return record;
}

qint64 computeFileSignatureMs(const QFileInfo& info) {
    return info.lastModified().toMSecsSinceEpoch();
}

void sanitizeScanResult(FileManager::ScanResult& result) {
    static const QString kInternalManifestName = ".ape_dlc_manifest.json";

    result.files.remove(kInternalManifestName);

    QStringList staleFileTimeKeys;
    for (auto it = result.fileTimes.begin(); it != result.fileTimes.end(); ++it) {
        const QString normalizedPath = QDir::cleanPath(it.key()).replace('\\', '/');
        if (normalizedPath.endsWith("/" + kInternalManifestName, Qt::CaseInsensitive) ||
            normalizedPath.compare(kInternalManifestName, Qt::CaseInsensitive) == 0) {
            staleFileTimeKeys.append(it.key());
        }
    }

    for (const QString& key : staleFileTimeKeys) {
        result.fileTimes.remove(key);
    }
}

void cleanupLegacyDlcCacheIfNeeded() {
    const QString cacheRootPath =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/dlc_cache";
    QDir cacheRoot(cacheRootPath);
    if (!cacheRoot.exists()) {
        return;
    }

    const QFileInfoList entries = cacheRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        const QString manifestPath = QDir(entry.absoluteFilePath()).filePath(".ape_dlc_manifest.json");
        if (!QFileInfo::exists(manifestPath)) {
            Logger::instance().logInfo(
                "FileManager",
                "Legacy DLC cache detected without manifest, clearing entire dlc_cache directory"
            );
            cacheRoot.removeRecursively();
            return;
        }
    }
}
}

FileManager& FileManager::instance() {
    static FileManager instance;
    return instance;
}

QJsonObject FileManager::DlcCacheManifest::toJson() const {
    QJsonObject obj;
    obj["zipPath"] = zipPath;
    obj["zipSize"] = QString::number(zipSize);
    obj["zipLastModifiedMs"] = QString::number(zipLastModifiedMs);
    obj["extractedFileCount"] = extractedFileCount;
    return obj;
}

FileManager::DlcCacheManifest FileManager::DlcCacheManifest::fromJson(const QJsonObject& obj) {
    DlcCacheManifest manifest;
    manifest.zipPath = obj["zipPath"].toString();
    manifest.zipSize = obj["zipSize"].toString().toLongLong();
    manifest.zipLastModifiedMs = obj["zipLastModifiedMs"].toString().toLongLong();
    manifest.extractedFileCount = obj["extractedFileCount"].toInt();
    return manifest;
}

QJsonObject FileManager::PersistentCachePayload::toJson() const {
    QJsonObject obj;
    obj["version"] = kPersistentCacheVersion;
    obj["gamePath"] = gamePath;
    obj["modPath"] = modPath;
    obj["gamePathLastModifiedMs"] = QString::number(gamePathLastModifiedMs);
    obj["modPathLastModifiedMs"] = QString::number(modPathLastModifiedMs);
    obj["modDescriptorLastModifiedMs"] = QString::number(modDescriptorLastModifiedMs);

    QJsonArray rootPathsArray;
    for (const QString& rootPath : rootPaths) {
        rootPathsArray.append(rootPath);
    }
    obj["rootPaths"] = rootPathsArray;

    QJsonArray replacePathsArray;
    for (const QString& replacePath : replacePaths) {
        replacePathsArray.append(replacePath);
    }
    obj["replacePaths"] = replacePathsArray;

    QJsonObject filesObject;
    for (auto it = scanResult.files.begin(); it != scanResult.files.end(); ++it) {
        filesObject[it.key()] = compactRecordToJson(it.value());
    }
    obj["files"] = filesObject;

    QJsonObject fileTimesObject;
    for (auto it = scanResult.fileTimes.begin(); it != scanResult.fileTimes.end(); ++it) {
        fileTimesObject[it.key()] = QString::number(it.value());
    }
    obj["fileTimes"] = fileTimesObject;

    QJsonArray watchedPathsArray;
    for (const QString& watchedPath : scanResult.watchedPaths) {
        watchedPathsArray.append(watchedPath);
    }
    obj["watchedPaths"] = watchedPathsArray;

    QJsonArray scanRootPathsArray;
    for (const QString& rootPath : scanResult.rootPaths) {
        scanRootPathsArray.append(rootPath);
    }
    obj["scanRootPaths"] = scanRootPathsArray;

    return obj;
}

FileManager::PersistentCachePayload FileManager::PersistentCachePayload::fromJson(const QJsonObject& obj) {
    PersistentCachePayload payload;
    payload.gamePath = obj["gamePath"].toString();
    payload.modPath = obj["modPath"].toString();
    payload.gamePathLastModifiedMs = obj["gamePathLastModifiedMs"].toString().toLongLong();
    payload.modPathLastModifiedMs = obj["modPathLastModifiedMs"].toString().toLongLong();
    payload.modDescriptorLastModifiedMs = obj["modDescriptorLastModifiedMs"].toString().toLongLong();

    const QJsonArray rootPathsArray = obj["rootPaths"].toArray();
    for (const QJsonValue& value : rootPathsArray) {
        payload.rootPaths.append(value.toString());
    }

    const QJsonArray replacePathsArray = obj["replacePaths"].toArray();
    for (const QJsonValue& value : replacePathsArray) {
        payload.replacePaths.insert(value.toString());
    }

    const QJsonObject filesObject = obj["files"].toObject();
    for (auto it = filesObject.begin(); it != filesObject.end(); ++it) {
        payload.scanResult.files.insert(it.key(), compactRecordFromJson(it.value().toObject()));
    }

    const QJsonObject fileTimesObject = obj["fileTimes"].toObject();
    for (auto it = fileTimesObject.begin(); it != fileTimesObject.end(); ++it) {
        payload.scanResult.fileTimes.insert(it.key(), it.value().toString().toLongLong());
    }

    const QJsonArray watchedPathsArray = obj["watchedPaths"].toArray();
    for (const QJsonValue& value : watchedPathsArray) {
        payload.scanResult.watchedPaths.append(value.toString());
    }

    const QJsonArray scanRootPathsArray = obj["scanRootPaths"].toArray();
    for (const QJsonValue& value : scanRootPathsArray) {
        payload.scanResult.rootPaths.append(value.toString());
    }

    payload.scanResult.replacePaths = payload.replacePaths;
    return payload;
}

FileManager::FileManager() {
    m_ignoreDirs = {
        "assets", "browser", "cef", "country_metadata", "crash_reporter",
        "dlc_metadata", "documentation", "EmptySteamDepot", "integrated_dlc"
    };
    m_ignoreDirs.removeAll("country_metadata");

    m_watcher = new RecursiveFileSystemWatcher(this);
    connect(m_watcher, &RecursiveFileSystemWatcher::fileChanged, this, &FileManager::onFileChanged);

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(50);
    connect(m_debounceTimer, &QTimer::timeout, this, &FileManager::onDebounceTimerTimeout);

    m_futureWatcher = new QFutureWatcher<ScanResult>(this);
    connect(m_futureWatcher, &QFutureWatcher<ScanResult>::finished, this, &FileManager::onScanFinished);
}

void FileManager::startScanning() {
    if (m_isScanning) return;

    m_stopRequested = false;
    onDebounceTimerTimeout();
}

void FileManager::stopScanning() {
    m_stopRequested = true;
    if (m_debounceTimer) {
        m_debounceTimer->stop();
    }
    if (m_watcher) {
        m_watcher->removeAllPaths();
    }

    if (m_futureWatcher && m_futureWatcher->isRunning()) {
        m_futureWatcher->waitForFinished();
    }

    m_isScanning = false;
}

void FileManager::onFileChanged(const QString& path) {
    Q_UNUSED(path);
    if (m_debounceTimer) {
        m_debounceTimer->start();
    }
}

void FileManager::onDebounceTimerTimeout() {
    if (m_isScanning) {
        m_debounceTimer->start();
        return;
    }

    m_isScanning = true;
    emit scanStarted();

    ConfigManager& config = ConfigManager::instance();
    const QString gamePath = config.getGamePath();
    const QString modPath = config.getModPath();
    const QStringList ignoreDirs = m_ignoreDirs;

    QFuture<ScanResult> future = QtConcurrent::run([gamePath, modPath, ignoreDirs, this]() {
        return doScan(gamePath, modPath, ignoreDirs, &m_stopRequested);
    });
    m_futureWatcher->setFuture(future);
}

void FileManager::onScanFinished() {
    const ScanResult result = m_futureWatcher->result();

    if (m_stopRequested) {
        m_isScanning = false;
        Logger::instance().logInfo("FileManager", "Scan finished during shutdown, skipping watcher refresh");
        return;
    }

    QMap<QString, FileDetails> publicFiles;
    QMap<QString, qint64> publicFileTimes;
    convertScanResultToPublicData(result, publicFiles, publicFileTimes);

    {
        QMutexLocker locker(&m_mutex);

        for (auto it = publicFileTimes.begin(); it != publicFileTimes.end(); ++it) {
            const QString& path = it.key();
            const qint64 time = it.value();

            if (!m_fileTimes.contains(path)) {
                Logger::instance().logInfo("FileManager", "File added: " + path);
            } else if (m_fileTimes[path] != time) {
                Logger::instance().logInfo("FileManager", "File modified: " + path);
            }
        }

        for (auto it = m_fileTimes.begin(); it != m_fileTimes.end(); ++it) {
            if (!publicFileTimes.contains(it.key())) {
                Logger::instance().logInfo("FileManager", "File removed: " + it.key());
            }
        }

        m_files = publicFiles;
        m_fileTimes = publicFileTimes;
        m_replacePaths = result.replacePaths;

        m_watcher->removeAllPaths();
        for (const QString& path : result.watchedPaths) {
            m_watcher->addPath(path);
        }

        m_isScanning = false;
        Logger::instance().logInfo("FileManager", QString("Scan finished. Total files: %1").arg(m_files.size()));
    }

    emit scanFinished();
}

FileManager::ScanResult FileManager::doScan(const QString& gamePath,
                                            const QString& modPath,
                                            const QStringList& ignoreDirs,
                                            const std::atomic_bool* stopRequested) {
    ScanResult result;
    if (gamePath.isEmpty() || modPath.isEmpty()) return result;
    if (stopRequested && stopRequested->load()) return result;

    ScanContext context;
    context.gamePath = QDir::cleanPath(gamePath);
    context.modPath = QDir::cleanPath(modPath);
    context.ignoreDirs = ignoreDirs;
    context.stopRequested = stopRequested;

    QThreadPool scanThreadPool;
    QThreadPool extractThreadPool;
    const int idealThreadCount = qMax(2, QThread::idealThreadCount());
    scanThreadPool.setMaxThreadCount(qMax(2, idealThreadCount - 1));
    extractThreadPool.setMaxThreadCount(qMin(3, qMax(1, idealThreadCount / 2)));
    context.scanThreadPool = &scanThreadPool;
    context.extractThreadPool = &extractThreadPool;

    const QString modDescriptorPath = findPrimaryModDescriptorPath(context.modPath);
    if (!modDescriptorPath.isEmpty()) {
        QFile file(modDescriptorPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                const QString line = in.readLine().trimmed();
                if (line.startsWith("replace_path")) {
                    const int start = line.indexOf('"');
                    const int end = line.lastIndexOf('"');
                    if (start != -1 && end > start) {
                        context.replacePaths.insert(line.mid(start + 1, end - start - 1));
                    }
                }
            }
            file.close();
        }
    }

    context.normalizedReplacePaths = buildNormalizedReplacePathSet(context.replacePaths);

    cleanupLegacyDlcCacheIfNeeded();

    const QString persistentCachePath = getPersistentCacheFilePath(context.gamePath, context.modPath);
    PersistentCachePayload persistentPayload;
    if (loadPersistentCache(persistentCachePath, persistentPayload) &&
        isPersistentCacheValid(persistentPayload, context.gamePath, context.modPath)) {
        sanitizeScanResult(persistentPayload.scanResult);
        Logger::instance().logInfo("FileManager", "Loaded file index from persistent cache");
        return persistentPayload.scanResult;
    }

    result.replacePaths = context.replacePaths;

    scanGameDirectory(context, result);
    if (stopRequested && stopRequested->load()) return result;

    ScanResult dlcResult;
    dlcResult.replacePaths = context.replacePaths;
    scanDlcDirectory(context, dlcResult);
    mergeScanResult(result, std::move(dlcResult));
    if (stopRequested && stopRequested->load()) return result;

    ScanResult modResult;
    modResult.replacePaths = context.replacePaths;
    scanModDirectory(context, modResult);
    mergeScanResult(result, std::move(modResult));
    if (stopRequested && stopRequested->load()) return result;

    if (!result.watchedPaths.contains(context.gamePath)) {
        result.watchedPaths.append(context.gamePath);
    }
    if (!result.watchedPaths.contains(context.modPath)) {
        result.watchedPaths.append(context.modPath);
    }

    sanitizeScanResult(result);

    const PersistentCachePayload payload = buildPersistentCachePayload(context, modDescriptorPath, result);
    savePersistentCache(persistentCachePath, payload);

    return result;
}

void FileManager::scanGameDirectory(const ScanContext& context, ScanResult& result) {
    QDir dir(context.gamePath);
    if (!dir.exists()) return;
    if (context.stopRequested && context.stopRequested->load()) return;

    const QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QList<QFuture<ScanResult>> futures;
    futures.reserve(subDirs.size());

    for (const QString& subDir : subDirs) {
        if (context.stopRequested && context.stopRequested->load()) return;
        if (subDir == "dlc") continue;
        if (context.ignoreDirs.contains(subDir)) continue;
        if (subDir.contains("_assets", Qt::CaseInsensitive)) continue;

        if (subDir.contains("pdx", Qt::CaseInsensitive) ||
            subDir.contains("steam", Qt::CaseInsensitive) ||
            subDir.contains("cline", Qt::CaseInsensitive) ||
            subDir.contains("git", Qt::CaseInsensitive) ||
            subDir.contains("wiki", Qt::CaseInsensitive) ||
            subDir.contains("tools", Qt::CaseInsensitive) ||
            subDir.contains("test", Qt::CaseInsensitive) ||
            subDir.contains("script", Qt::CaseInsensitive)) {
            continue;
        }

        RootDescriptor root;
        root.rootPath = context.gamePath;
        root.isMod = false;
        root.isDlc = false;
        root.rootId = 0;

        futures.append(QtConcurrent::run(context.scanThreadPool, [context, root, subDir]() {
            return scanDirectoryRecursive(context, root, subDir);
        }));
    }

    for (QFuture<ScanResult>& future : futures) {
        future.waitForFinished();
        if (context.stopRequested && context.stopRequested->load()) return;
        mergeScanResult(result, future.result());
    }
}

void FileManager::scanModDirectory(const ScanContext& context, ScanResult& result) {
    QDir dir(context.modPath);
    if (!dir.exists()) return;
    if (context.stopRequested && context.stopRequested->load()) return;

    const QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QList<QFuture<ScanResult>> futures;
    futures.reserve(subDirs.size());

    for (const QString& subDir : subDirs) {
        if (context.stopRequested && context.stopRequested->load()) return;
        if (context.ignoreDirs.contains(subDir)) continue;
        if (subDir.contains("_assets", Qt::CaseInsensitive)) continue;

        RootDescriptor root;
        root.rootPath = context.modPath;
        root.isMod = true;
        root.isDlc = false;
        root.rootId = 1;

        futures.append(QtConcurrent::run(context.scanThreadPool, [context, root, subDir]() {
            return scanDirectoryRecursive(context, root, subDir);
        }));
    }

    for (QFuture<ScanResult>& future : futures) {
        future.waitForFinished();
        if (context.stopRequested && context.stopRequested->load()) return;
        mergeScanResult(result, future.result());
    }
}

void FileManager::scanDlcDirectory(const ScanContext& context, ScanResult& result) {
    const QString dlcPath = context.gamePath + "/dlc";
    QDir dir(dlcPath);
    if (!dir.exists()) return;
    if (context.stopRequested && context.stopRequested->load()) return;

    const QStringList entries = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    QList<QFuture<ScanResult>> futures;
    futures.reserve(entries.size());

    int dynamicRootId = 2;
    for (const QString& entry : entries) {
        if (context.stopRequested && context.stopRequested->load()) return;

        const QString fullPath = dir.filePath(entry);
        const QFileInfo info(fullPath);

        if (info.isDir()) {
            RootDescriptor root;
            root.rootPath = fullPath;
            root.isMod = false;
            root.isDlc = true;
            root.rootId = dynamicRootId++;

            futures.append(QtConcurrent::run(context.scanThreadPool, [context, root]() {
                return scanDirectoryRecursive(context, root, QString());
            }));
        } else if (info.isFile() && entry.endsWith(".zip", Qt::CaseInsensitive)) {
            const int rootId = dynamicRootId++;
            futures.append(QtConcurrent::run(context.extractThreadPool, [context, fullPath, rootId]() {
                ScanResult zipResult;
                const QString cachePath = buildDlcCachePath(fullPath);
                if (!ensureZipExtracted(fullPath, cachePath, context.stopRequested)) {
                    return zipResult;
                }
                if (context.stopRequested && context.stopRequested->load()) return zipResult;

                RootDescriptor root;
                root.rootPath = cachePath;
                root.isMod = false;
                root.isDlc = true;
                root.rootId = rootId;
                return scanDirectoryRecursive(context, root, QString());
            }));
        }
    }

    for (QFuture<ScanResult>& future : futures) {
        future.waitForFinished();
        if (context.stopRequested && context.stopRequested->load()) return;
        mergeScanResult(result, future.result());
    }
}

FileManager::ScanResult FileManager::scanDirectoryRecursive(const ScanContext& context,
                                                            const RootDescriptor& root,
                                                            const QString& currentPath) {
    ScanResult result;
    const QString normalizedRootPath = QDir::cleanPath(root.rootPath);
    if (!QDir(normalizedRootPath).exists()) return result;
    if (context.stopRequested && context.stopRequested->load()) return result;

    QVector<QString> pendingDirs;
    pendingDirs.reserve(64);
    pendingDirs.append(currentPath);

    if (!result.rootPaths.contains(normalizedRootPath)) {
        result.rootPaths.append(normalizedRootPath);
    }

    while (!pendingDirs.isEmpty()) {
        if (context.stopRequested && context.stopRequested->load()) return result;

        const QString relativeDir = pendingDirs.back();
        pendingDirs.pop_back();

        const QString absoluteDirPath = relativeDir.isEmpty()
            ? normalizedRootPath
            : QDir(normalizedRootPath).filePath(relativeDir);

        QDir dir(absoluteDirPath);
        if (!dir.exists()) continue;

        if (!result.watchedPaths.contains(absoluteDirPath)) {
            result.watchedPaths.append(absoluteDirPath);
        }

        const QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::DirsFirst | QDir::Name | QDir::IgnoreCase
        );

        for (const QFileInfo& entry : entries) {
            if (context.stopRequested && context.stopRequested->load()) return result;

            const QString relPath = relativeDir.isEmpty()
                ? entry.fileName()
                : relativeDir + "/" + entry.fileName();

            if (entry.isDir()) {
                pendingDirs.append(relPath);
                continue;
            }

            if (root.isDlc && entry.fileName().endsWith(".zip", Qt::CaseInsensitive)) {
                const QString cachePath = buildDlcCachePath(entry.absoluteFilePath());
                if (!ensureZipExtracted(entry.absoluteFilePath(), cachePath, context.stopRequested)) {
                    continue;
                }
                if (context.stopRequested && context.stopRequested->load()) return result;

                RootDescriptor nestedRoot = root;
                nestedRoot.rootPath = cachePath;
                ScanResult nestedResult = scanDirectoryRecursive(context, nestedRoot, currentPath);
                mergeScanResult(result, std::move(nestedResult));
                continue;
            }

            processFile(
                context,
                root,
                entry.absoluteFilePath(),
                relPath,
                computeFileSignatureMs(entry),
                result
            );
        }
    }

    return result;
}

bool FileManager::processFile(const ScanContext& context,
                              const RootDescriptor& root,
                              const QString& absPath,
                              const QString& relPath,
                              qint64 lastModifiedMs,
                              ScanResult& result) {
    QString normalizedRelPath = relPath;
    if (root.isDlc) {
        normalizedRelPath = normalizeDlcPath(relPath);
    }

    if (isIgnoredFile(absPath, normalizedRelPath, root.isDlc)) {
        return false;
    }

    if (!root.isMod) {
        const QString parentFolder = normalizePathForComparison(QFileInfo(normalizedRelPath).path());
        if (context.normalizedReplacePaths.contains(parentFolder)) {
            return false;
        }
    }

    CompactFileRecord record;
    record.absPath = absPath;
    record.source = root.isMod ? FileSource::Mod : (root.isDlc ? FileSource::Dlc : FileSource::Game);
    record.lastModifiedMs = lastModifiedMs;

    result.files.insert(normalizedRelPath, record);
    result.fileTimes.insert(absPath, lastModifiedMs);
    return true;
}

bool FileManager::extractZip(const QString& zipPath, const QString& destPath) {
    QDir().mkpath(destPath);

    const QString program = "powershell";
    QStringList arguments;
    arguments << "-NoProfile"
              << "-NonInteractive"
              << "-Command"
              << QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                     .arg(QDir::toNativeSeparators(zipPath), QDir::toNativeSeparators(destPath));

    return QProcess::execute(program, arguments) == 0;
}

bool FileManager::ensureZipExtracted(const QString& zipPath,
                                     const QString& destPath,
                                     const std::atomic_bool* stopRequested) {
    if (stopRequested && stopRequested->load()) return false;

    if (isExtractedCacheReusable(zipPath, destPath, stopRequested)) {
        return true;
    }

    QDir cacheDir(destPath);
    if (cacheDir.exists()) {
        cacheDir.removeRecursively();
    }

    if (stopRequested && stopRequested->load()) return false;
    if (!extractZip(zipPath, destPath)) return false;
    if (stopRequested && stopRequested->load()) return false;

    const QFileInfo zipInfo(zipPath);
    DlcCacheManifest manifest;
    manifest.zipPath = QDir::cleanPath(zipPath);
    manifest.zipSize = zipInfo.size();
    manifest.zipLastModifiedMs = computeFileSignatureMs(zipInfo);
    manifest.extractedFileCount = countFilesRecursively(destPath, stopRequested);

    return saveDlcManifest(buildDlcManifestPath(destPath), manifest);
}

bool FileManager::isExtractedCacheReusable(const QString& zipPath,
                                           const QString& destPath,
                                           const std::atomic_bool* stopRequested) {
    QDir dir(destPath);
    if (!dir.exists()) return false;

    DlcCacheManifest manifest;
    if (!loadDlcManifest(buildDlcManifestPath(destPath), manifest)) {
        return false;
    }

    const QFileInfo zipInfo(zipPath);
    if (!zipInfo.exists()) return false;
    if (manifest.zipPath != QDir::cleanPath(zipPath)) return false;
    if (manifest.zipSize != zipInfo.size()) return false;
    if (manifest.zipLastModifiedMs != computeFileSignatureMs(zipInfo)) return false;
    if (manifest.extractedFileCount != kExpectedDlcCacheFileCount) return false;

    return countFilesRecursively(destPath, stopRequested) == kExpectedDlcCacheFileCount;
}

int FileManager::countFilesRecursively(const QString& rootPath, const std::atomic_bool* stopRequested) {
    if (!QDir(rootPath).exists()) return 0;

    int count = 0;
    QDirIterator it(rootPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (stopRequested && stopRequested->load()) return count;
        it.next();
        ++count;
    }
    return count;
}

QString FileManager::getDlcCacheRootPath() {
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/dlc_cache";
}

QString FileManager::buildDlcCachePath(const QString& zipPath) {
    const QByteArray hash = QCryptographicHash::hash(
        QDir::cleanPath(zipPath).toUtf8(),
        QCryptographicHash::Sha1
    ).toHex();

    return getDlcCacheRootPath() + "/" + QFileInfo(zipPath).baseName() + "_" + QString::fromLatin1(hash.left(12));
}

QString FileManager::buildDlcManifestPath(const QString& destPath) {
    return QDir::cleanPath(destPath) + "/.ape_dlc_manifest.json";
}

QString FileManager::getPersistentCacheFilePath(const QString& gamePath, const QString& modPath) {
    const QString key = QDir::cleanPath(gamePath) + "|" + QDir::cleanPath(modPath);
    const QString hashStr = QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex());
    
    const QString oldCacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/file_index";
    const QString newCacheRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/cache/file_index";
    
    const QString oldFilePath = oldCacheRoot + "/" + hashStr + ".json";
    const QString newFilePath = newCacheRoot + "/" + hashStr + ".json";
    
    QDir().mkpath(newCacheRoot);
    
    if (QFile::exists(oldFilePath)) {
        if (QFile::exists(newFilePath)) {
            QFile::remove(newFilePath);
        }
        QFile::rename(oldFilePath, newFilePath);
    }
    
    return newFilePath;
}

bool FileManager::loadDlcManifest(const QString& manifestPath, DlcCacheManifest& manifest) {
    QFile file(manifestPath);
    if (!file.exists()) return false;
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) return false;

    manifest = DlcCacheManifest::fromJson(document.object());
    return true;
}

bool FileManager::saveDlcManifest(const QString& manifestPath, const DlcCacheManifest& manifest) {
    QSaveFile file(manifestPath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    const QJsonDocument document(manifest.toJson());
    if (file.write(document.toJson(QJsonDocument::Compact)) < 0) {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

bool FileManager::loadPersistentCache(const QString& cacheFilePath, PersistentCachePayload& payload) {
    QFile file(cacheFilePath);
    if (!file.exists()) return false;
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!document.isObject()) return false;

    const QJsonObject obj = document.object();
    if (obj["version"].toInt() != kPersistentCacheVersion) return false;

    payload = PersistentCachePayload::fromJson(obj);
    return true;
}

bool FileManager::savePersistentCache(const QString& cacheFilePath, const PersistentCachePayload& payload) {
    QSaveFile file(cacheFilePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    const QJsonDocument document(payload.toJson());
    if (file.write(document.toJson(QJsonDocument::Compact)) < 0) {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

bool FileManager::isPersistentCacheValid(const PersistentCachePayload& payload,
                                         const QString& gamePath,
                                         const QString& modPath) {
    if (payload.gamePath != QDir::cleanPath(gamePath)) return false;
    if (payload.modPath != QDir::cleanPath(modPath)) return false;
    if (payload.gamePathLastModifiedMs != getPathLastModifiedMs(gamePath)) return false;
    if (payload.modPathLastModifiedMs != getPathLastModifiedMs(modPath)) return false;

    const QString modDescriptorPath = findPrimaryModDescriptorPath(modPath);
    const qint64 descriptorLastModifiedMs = modDescriptorPath.isEmpty()
        ? 0
        : getPathLastModifiedMs(modDescriptorPath);

    if (payload.modDescriptorLastModifiedMs != descriptorLastModifiedMs) return false;
    if (payload.scanResult.files.isEmpty()) return false;

    return true;
}

FileManager::PersistentCachePayload FileManager::buildPersistentCachePayload(const ScanContext& context,
                                                                             const QString& modDescriptorPath,
                                                                             const ScanResult& result) {
    PersistentCachePayload payload;
    payload.gamePath = context.gamePath;
    payload.modPath = context.modPath;
    payload.gamePathLastModifiedMs = getPathLastModifiedMs(context.gamePath);
    payload.modPathLastModifiedMs = getPathLastModifiedMs(context.modPath);
    payload.modDescriptorLastModifiedMs = modDescriptorPath.isEmpty() ? 0 : getPathLastModifiedMs(modDescriptorPath);
    payload.rootPaths = result.rootPaths;
    payload.replacePaths = result.replacePaths;
    payload.scanResult = result;
    return payload;
}

QString FileManager::findPrimaryModDescriptorPath(const QString& modPath) {
    QDir modDir(modPath);
    const QStringList modFiles = modDir.entryList(QStringList() << "*.mod", QDir::Files);
    if (modFiles.isEmpty()) {
        return QString();
    }
    return modDir.filePath(modFiles.first());
}

qint64 FileManager::getPathLastModifiedMs(const QString& path) {
    const QFileInfo info(path);
    if (!info.exists()) return 0;
    return computeFileSignatureMs(info);
}

QString FileManager::normalizePathForComparison(QString path) {
    path.replace('\\', '/');
    path = QDir::cleanPath(path);
    if (path == ".") return QString();
    while (path.startsWith('/')) {
        path.remove(0, 1);
    }
    while (path.endsWith('/')) {
        path.chop(1);
    }
    return path;
}

QString FileManager::normalizeDlcPath(const QString& path) {
    const QFileInfo info(path);
    const QString dirPath = info.path();
    const QString fileName = info.fileName();

    if (dirPath == ".") return fileName;

    const QStringList parts = dirPath.split('/', Qt::SkipEmptyParts);
    QStringList cleanParts;

    for (const QString& part : parts) {
        if (part.startsWith("dlc", Qt::CaseInsensitive)) continue;
        cleanParts.append(part);
    }

    const QString cleanDir = cleanParts.join('/');
    if (cleanDir.isEmpty()) return fileName;
    return cleanDir + "/" + fileName;
}

QSet<QString> FileManager::buildNormalizedReplacePathSet(const QSet<QString>& replacePaths) {
    QSet<QString> normalized;
    normalized.reserve(replacePaths.size());
    for (const QString& path : replacePaths) {
        normalized.insert(normalizePathForComparison(path));
    }
    return normalized;
}

void FileManager::mergeScanResult(ScanResult& target, ScanResult&& source) {
    for (auto it = source.files.begin(); it != source.files.end(); ++it) {
        target.files.insert(it.key(), it.value());
    }

    for (auto it = source.fileTimes.begin(); it != source.fileTimes.end(); ++it) {
        target.fileTimes.insert(it.key(), it.value());
    }

    for (const QString& path : source.replacePaths) {
        target.replacePaths.insert(path);
    }

    for (const QString& watchedPath : source.watchedPaths) {
        if (!target.watchedPaths.contains(watchedPath)) {
            target.watchedPaths.append(watchedPath);
        }
    }

    for (const QString& rootPath : source.rootPaths) {
        if (!target.rootPaths.contains(rootPath)) {
            target.rootPaths.append(rootPath);
        }
    }
}

void FileManager::convertScanResultToPublicData(const ScanResult& scanResult,
                                                QMap<QString, FileDetails>& files,
                                                QMap<QString, qint64>& fileTimes) {
    files.clear();
    fileTimes.clear();

    for (auto it = scanResult.files.begin(); it != scanResult.files.end(); ++it) {
        FileDetails details;
        details.absPath = it.value().absPath;
        details.source = it.value().source;
        files.insert(it.key(), details);
    }

    for (auto it = scanResult.fileTimes.begin(); it != scanResult.fileTimes.end(); ++it) {
        fileTimes.insert(it.key(), it.value());
    }
}

bool FileManager::isIgnoredFile(const QString& absPath, const QString& relPath, bool isDlc) {
    const QFileInfo info(absPath);
    const QString fileName = info.fileName();
    const QString suffix = info.suffix().toLower();

    if (suffix == "pdf" || suffix == "md" || suffix == "dlc") return true;
    if (fileName.compare("thumbnail.png", Qt::CaseInsensitive) == 0) return true;
    if (fileName.compare(".ape_dlc_manifest.json", Qt::CaseInsensitive) == 0) return true;

    if (suffix == "mp3" || suffix == "ogg") {
        if (!relPath.contains("music/", Qt::CaseInsensitive) &&
            !relPath.contains("sound/", Qt::CaseInsensitive) &&
            !relPath.contains("soundtrack/", Qt::CaseInsensitive)) {
            return true;
        }
    }

    QString normalizedAbsPath = absPath;
    normalizedAbsPath.replace('\\', '/');

    if (normalizedAbsPath.contains("/dlc028_la_resistance/Wallpaper", Qt::CaseInsensitive)) return true;
    if (normalizedAbsPath.contains("/dlc014_wallpaper", Qt::CaseInsensitive)) return true;
    if (normalizedAbsPath.contains("/dlc024_man_the_guns_wallpaper", Qt::CaseInsensitive)) return true;

    if (isDlc) {
        if (normalizedAbsPath.contains("/MP3/", Qt::CaseInsensitive) ||
            normalizedAbsPath.contains("/Wallpaper/", Qt::CaseInsensitive)) {
            return true;
        }
    }

    if (relPath.startsWith("country_metadata/", Qt::CaseInsensitive) &&
        fileName.compare("00_country_metadata.txt", Qt::CaseInsensitive) != 0) {
        return true;
    }

    return false;
}

QMap<QString, FileDetails> FileManager::getEffectiveFiles() const {
    QMutexLocker locker(&m_mutex);
    return m_files;
}

QStringList FileManager::getReplacePaths() const {
    QMutexLocker locker(&m_mutex);
    return m_replacePaths.values();
}

int FileManager::getFileCount() const {
    QMutexLocker locker(&m_mutex);
    return m_files.size();
}

QJsonObject FileManager::toJson() const {
    QMutexLocker locker(&m_mutex);

    QJsonObject obj;
    QJsonObject filesObj;
    for (auto it = m_files.begin(); it != m_files.end(); ++it) {
        filesObj[it.key()] = it.value().toJson();
    }
    obj["files"] = filesObj;

    QJsonArray replacePathsArr;
    for (const QString& path : m_replacePaths) {
        replacePathsArr.append(path);
    }
    obj["replacePaths"] = replacePathsArr;

    return obj;
}

void FileManager::fromJson(const QJsonObject& obj, QMap<QString, FileDetails>& files, QStringList& replacePaths) {
    files.clear();
    replacePaths.clear();

    const QJsonObject filesObj = obj["files"].toObject();
    for (auto it = filesObj.begin(); it != filesObj.end(); ++it) {
        files[it.key()] = FileDetails::fromJson(it.value().toObject());
    }

    const QJsonArray replacePathsArr = obj["replacePaths"].toArray();
    for (const QJsonValue& val : replacePathsArr) {
        replacePaths.append(val.toString());
    }
}

void FileManager::setFromJson(const QJsonObject& obj) {
    QMutexLocker locker(&m_mutex);

    m_files.clear();
    m_replacePaths.clear();
    m_fileTimes.clear();

    const QJsonObject filesObj = obj["files"].toObject();
    for (auto it = filesObj.begin(); it != filesObj.end(); ++it) {
        const FileDetails details = FileDetails::fromJson(it.value().toObject());
        m_files[it.key()] = details;
        if (!details.absPath.isEmpty()) {
            m_fileTimes[details.absPath] = getPathLastModifiedMs(details.absPath);
        }
    }

    const QJsonArray replacePathsArr = obj["replacePaths"].toArray();
    for (const QJsonValue& val : replacePathsArr) {
        m_replacePaths.insert(val.toString());
    }

    Logger::instance().logInfo("FileManager", QString("Loaded %1 files from IPC data").arg(m_files.size()));
}