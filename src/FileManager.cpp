#include "FileManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include <QStandardPaths>
#include <QProcess>

FileManager& FileManager::instance() {
    static FileManager instance;
    return instance;
}

FileManager::FileManager() {
    m_ignoreDirs = {
        "assets", "browser", "cef", "country_metadata", "crash_reporter",
        "dlc_metadata", "documentation", "EmptySteamDepot", "integrated_dlc"
    };
    // Removed "country_metadata" from ignore list as per new rule, but wait, 
    // user said "retain 00_country_metadata.txt, other country_metadata/ files still block".
    // So we should NOT ignore the folder entirely, but filter inside.
    m_ignoreDirs.removeAll("country_metadata");
    
    m_watcher = new RecursiveFileSystemWatcher(this);
    connect(m_watcher, &RecursiveFileSystemWatcher::fileChanged, this, &FileManager::onFileChanged);
    
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(2000); // 2 seconds debounce
    connect(m_debounceTimer, &QTimer::timeout, this, &FileManager::onDebounceTimerTimeout);
    
    m_futureWatcher = new QFutureWatcher<ScanResult>(this);
    connect(m_futureWatcher, &QFutureWatcher<ScanResult>::finished, this, &FileManager::onScanFinished);
}

void FileManager::startScanning() {
    if (m_isScanning) return;
    
    // Trigger scan immediately
    onDebounceTimerTimeout();
}

void FileManager::stopScanning() {
    m_debounceTimer->stop();
    if (m_futureWatcher->isRunning()) {
        m_futureWatcher->waitForFinished();
    }
    m_watcher->removeAllPaths();
}

void FileManager::onFileChanged(const QString& path) {
    // Logger::instance().logInfo("FileManager", "File changed: " + path);
    m_debounceTimer->start();
}

void FileManager::onDebounceTimerTimeout() {
    if (m_isScanning) {
        // If already scanning, reschedule
        m_debounceTimer->start();
        return;
    }
    
    m_isScanning = true;
    emit scanStarted();
    
    ConfigManager& config = ConfigManager::instance();
    QString gamePath = config.getGamePath();
    QString modPath = config.getModPath();
    QStringList ignoreDirs = m_ignoreDirs;
    
    // Run scan in background thread
    QFuture<ScanResult> future = QtConcurrent::run([gamePath, modPath, ignoreDirs]() {
        return doScan(gamePath, modPath, ignoreDirs);
    });
    m_futureWatcher->setFuture(future);
}

void FileManager::onScanFinished() {
    ScanResult result = m_futureWatcher->result();
    
    {
        QMutexLocker locker(&m_mutex);
        
        // Compare with old files to log changes
        for (auto it = result.fileTimes.begin(); it != result.fileTimes.end(); ++it) {
            const QString& path = it.key();
            const QDateTime& time = it.value();
            
            if (!m_fileTimes.contains(path)) {
                Logger::instance().logInfo("FileManager", "File added: " + path);
            } else if (m_fileTimes[path] != time) {
                Logger::instance().logInfo("FileManager", "File modified: " + path);
            }
        }
        
        for (auto it = m_fileTimes.begin(); it != m_fileTimes.end(); ++it) {
            if (!result.fileTimes.contains(it.key())) {
                Logger::instance().logInfo("FileManager", "File removed: " + it.key());
            }
        }

        m_files = result.files;
        m_fileTimes = result.fileTimes;
        m_replacePaths = result.replacePaths;
        
        m_watcher->removeAllPaths();
        for (const QString& path : result.watchedPaths) {
            m_watcher->addPath(path);
        }
        
        m_isScanning = false;
        Logger::instance().logInfo("FileManager", QString("Scan finished. Total files: %1").arg(m_files.size()));
    } // Unlock mutex before emitting signal to avoid deadlock
    
    Logger::instance().logInfo("FileManager", "Emitting scanFinished signal");
    emit scanFinished();
    Logger::instance().logInfo("FileManager", "scanFinished signal emitted");
}

FileManager::ScanResult FileManager::doScan(const QString& gamePath, const QString& modPath, const QStringList& ignoreDirs) {
    ScanResult result;
    
    if (gamePath.isEmpty() || modPath.isEmpty()) return result;

    clearCache();

    // 1. Parse .mod file
    QDir modDirObj(modPath);
    QStringList modFiles = modDirObj.entryList(QStringList() << "*.mod", QDir::Files);
    if (!modFiles.isEmpty()) {
        QFile file(modDirObj.filePath(modFiles.first()));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.startsWith("replace_path")) {
                    int start = line.indexOf('"');
                    int end = line.lastIndexOf('"');
                    if (start != -1 && end > start) {
                        QString path = line.mid(start + 1, end - start - 1);
                        result.replacePaths.insert(path);
                    }
                }
            }
            file.close();
        }
    }

    // 2. Scan Game Directory
    scanGameDirectory(gamePath, ignoreDirs, result);

    // 3. Scan DLCs
    scanDlcDirectory(gamePath + "/dlc", ignoreDirs, result);

    // 4. Scan Mod Directory
    scanModDirectory(modPath, ignoreDirs, result);
    
    // Add root paths to watcher
    result.watchedPaths.append(gamePath);
    result.watchedPaths.append(modPath);

    return result;
}

void FileManager::scanGameDirectory(const QString& gamePath, const QStringList& ignoreDirs, ScanResult& result) {
    QDir dir(gamePath);
    if (!dir.exists()) return;

    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subDir : subDirs) {
        if (subDir == "dlc") continue; // Handled separately
        if (ignoreDirs.contains(subDir)) continue;
        
        // Rule 3: Block folders with _assets in name at root level
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

        scanDirectoryRecursive(gamePath, subDir, false, false, ignoreDirs, result.replacePaths, result);
    }
}

void FileManager::scanModDirectory(const QString& modPath, const QStringList& ignoreDirs, ScanResult& result) {
    QDir dir(modPath);
    if (!dir.exists()) return;

    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subDir : subDirs) {
        if (ignoreDirs.contains(subDir)) continue;
        
        // Rule 3: Block folders with _assets in name at root level
        if (subDir.contains("_assets", Qt::CaseInsensitive)) continue;

        scanDirectoryRecursive(modPath, subDir, true, false, ignoreDirs, result.replacePaths, result);
    }
}

void FileManager::scanDlcDirectory(const QString& dlcPath, const QStringList& ignoreDirs, ScanResult& result) {
    QDir dir(dlcPath);
    if (!dir.exists()) return;

    QStringList entries = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        QString fullPath = dir.filePath(entry);
        QFileInfo info(fullPath);

        if (info.isDir()) {
            // Normal DLC folder
            scanDirectoryRecursive(fullPath, "", false, true, ignoreDirs, result.replacePaths, result);
        } else if (info.isFile() && entry.endsWith(".zip", Qt::CaseInsensitive)) {
            // Zip DLC
            QString cachePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/dlc_cache/" + info.baseName();
            extractZip(fullPath, cachePath);
            scanDirectoryRecursive(cachePath, "", false, true, ignoreDirs, result.replacePaths, result);
        }
    }
}

void FileManager::scanDirectoryRecursive(const QString& rootPath, const QString& currentPath, 
                                       bool isMod, bool isDlc, 
                                       const QStringList& ignoreDirs, 
                                       const QSet<QString>& replacePaths,
                                       ScanResult& result) {
    QDir dir(rootPath + (currentPath.isEmpty() ? "" : "/" + currentPath));
    if (!dir.exists()) return;

    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QFileInfo& entry : entries) {
        QString relPath = currentPath.isEmpty() ? entry.fileName() : currentPath + "/" + entry.fileName();
        
        if (entry.isDir()) {
            scanDirectoryRecursive(rootPath, relPath, isMod, isDlc, ignoreDirs, replacePaths, result);
        } else {
            // Check if it's a zip file inside a DLC directory (recursively found)
            if (isDlc && entry.fileName().endsWith(".zip", Qt::CaseInsensitive)) {
                QString cachePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/dlc_cache/" + entry.baseName();
                extractZip(entry.absoluteFilePath(), cachePath);
                scanDirectoryRecursive(cachePath, currentPath, isMod, isDlc, ignoreDirs, replacePaths, result);
            } else {
                processFile(entry.absoluteFilePath(), relPath, isMod, isDlc, result);
            }
        }
    }
}

void FileManager::processFile(const QString& absPath, const QString& relPath, bool isMod, bool isDlc, ScanResult& result) {
    // 1. Normalize path (remove dlc prefix if needed)
    QString normalizedRelPath = relPath;
    if (isDlc) {
        normalizedRelPath = normalizeDlcPath(relPath);
    }

    // 2. Check ignore rules
    if (isIgnoredFile(absPath, normalizedRelPath, isDlc)) return;

    // 3. Check replace_path
    if (!isMod) {
        for (const QString& replaced : result.replacePaths) {
            QString parentFolder = QFileInfo(normalizedRelPath).path();
            
            // Normalize separators
            QString p = parentFolder.replace('\\', '/');
            QString r = replaced;
            r.replace('\\', '/'); // Fix: modify copy
            
            if (p == r) {
                return; // Blocked
            }
        }
    }

    // 4. Add to result (overwrite if exists)
    FileDetails details;
    details.absPath = absPath;
    details.source = isMod ? "Mod" : (isDlc ? "DLC" : "Game");
    
    result.files.insert(normalizedRelPath, details);
    result.fileTimes.insert(absPath, QFileInfo(absPath).lastModified());
}

QString FileManager::normalizeDlcPath(const QString& path) {
    QFileInfo info(path);
    QString dirPath = info.path();
    QString fileName = info.fileName();
    
    if (dirPath == ".") return fileName; // No directory part
    
    QStringList parts = dirPath.split('/');
    QStringList cleanParts;
    
    for (const QString& part : parts) {
        if (part.startsWith("dlc", Qt::CaseInsensitive)) continue;
        cleanParts.append(part);
    }
    
    QString cleanDir = cleanParts.join('/');
    if (cleanDir.isEmpty()) return fileName;
    return cleanDir + "/" + fileName;
}

bool FileManager::isIgnoredFile(const QString& absPath, const QString& relPath, bool isDlc) {
    QFileInfo info(absPath);
    QString fileName = info.fileName();
    QString suffix = info.suffix().toLower();

    // Rule 1: Block .pdf
    if (suffix == "pdf") return true;

    // Rule 3: Block .dlc and thumbnail.png
    if (suffix == "dlc") return true;
    if (fileName.compare("thumbnail.png", Qt::CaseInsensitive) == 0) return true;

    // Rule 4: Audio files only in music/sound/soundtrack
    if (suffix == "mp3" || suffix == "ogg") {
        if (!relPath.contains("music/", Qt::CaseInsensitive) &&
            !relPath.contains("sound/", Qt::CaseInsensitive) &&
            !relPath.contains("soundtrack/", Qt::CaseInsensitive)) {
            return true;
        }
    }

    // Rule 6 & 7: Specific DLC wallpapers
    QString normalizedAbsPath = absPath;
    normalizedAbsPath.replace('\\', '/');
    
    if (normalizedAbsPath.contains("/dlc028_la_resistance/Wallpaper", Qt::CaseInsensitive)) return true;
    if (normalizedAbsPath.contains("/dlc014_wallpaper", Qt::CaseInsensitive)) return true;
    if (normalizedAbsPath.contains("/dlc024_man_the_guns_wallpaper", Qt::CaseInsensitive)) return true;

    // New Rule 1: Block files in MP3 or Wallpaper folders inside DLC
    if (isDlc) {
        if (normalizedAbsPath.contains("/MP3/", Qt::CaseInsensitive) || 
            normalizedAbsPath.contains("/Wallpaper/", Qt::CaseInsensitive)) {
            return true;
        }
    }

    // New Rule 2: Block country_metadata files except 00_country_metadata.txt
    if (relPath.startsWith("country_metadata/", Qt::CaseInsensitive)) {
        if (fileName.compare("00_country_metadata.txt", Qt::CaseInsensitive) != 0) {
            return true;
        }
    }

    return false;
}

void FileManager::extractZip(const QString& zipPath, const QString& destPath) {
    QDir dir(destPath);
    if (dir.exists()) return; 
    
    dir.mkpath(".");
    
    // Use PowerShell to extract
    QString cmd = QString("powershell -command \"Expand-Archive -Path '%1' -DestinationPath '%2' -Force\"").arg(zipPath, destPath);
    QProcess::execute(cmd);
}

void FileManager::clearCache() {
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/dlc_cache";
    QDir dir(cachePath);
    if (dir.exists()) {
        dir.removeRecursively();
    }
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
    
    // Serialize files
    QJsonObject filesObj;
    for (auto it = m_files.begin(); it != m_files.end(); ++it) {
        filesObj[it.key()] = it.value().toJson();
    }
    obj["files"] = filesObj;
    
    // Serialize replace paths
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
    
    // Deserialize files
    QJsonObject filesObj = obj["files"].toObject();
    for (auto it = filesObj.begin(); it != filesObj.end(); ++it) {
        files[it.key()] = FileDetails::fromJson(it.value().toObject());
    }
    
    // Deserialize replace paths
    QJsonArray replacePathsArr = obj["replacePaths"].toArray();
    for (const QJsonValue& val : replacePathsArr) {
        replacePaths.append(val.toString());
    }
}

void FileManager::setFromJson(const QJsonObject& obj) {
    QMutexLocker locker(&m_mutex);
    
    m_files.clear();
    m_replacePaths.clear();
    
    // Deserialize files
    QJsonObject filesObj = obj["files"].toObject();
    for (auto it = filesObj.begin(); it != filesObj.end(); ++it) {
        m_files[it.key()] = FileDetails::fromJson(it.value().toObject());
    }
    
    // Deserialize replace paths
    QJsonArray replacePathsArr = obj["replacePaths"].toArray();
    for (const QJsonValue& val : replacePathsArr) {
        m_replacePaths.insert(val.toString());
    }
    
    Logger::instance().logInfo("FileManager", QString("Loaded %1 files from IPC data").arg(m_files.size()));
}
