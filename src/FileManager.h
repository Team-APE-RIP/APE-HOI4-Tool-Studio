#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QTimer>
#include <QMutex>
#include <QFuture>
#include <QFutureWatcher>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include "RecursiveFileSystemWatcher.h"

struct FileDetails {
    QString absPath;
    QString source; // "Game", "Mod", "DLC"
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["absPath"] = absPath;
        obj["source"] = source;
        return obj;
    }
    
    static FileDetails fromJson(const QJsonObject& obj) {
        FileDetails fd;
        fd.absPath = obj["absPath"].toString();
        fd.source = obj["source"].toString();
        return fd;
    }
};

class FileManager : public QObject {
    Q_OBJECT

public:
    static FileManager& instance();

    void startScanning(); // Starts initial scan and monitoring
    void stopScanning();

    QMap<QString, FileDetails> getEffectiveFiles() const;
    QStringList getReplacePaths() const;
    int getFileCount() const;
    bool isScanning() const { return m_isScanning; }
    
    // Serialization for IPC
    QJsonObject toJson() const;
    static void fromJson(const QJsonObject& obj, QMap<QString, FileDetails>& files, QStringList& replacePaths);
    void setFromJson(const QJsonObject& obj); // For ToolHost to initialize from IPC data

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
    
    struct ScanResult {
        QMap<QString, FileDetails> files; // Rel -> Details
        QMap<QString, QDateTime> fileTimes; // Abs -> Time
        QSet<QString> replacePaths;
        QStringList watchedPaths;
    };
    
    static ScanResult doScan(const QString& gamePath, const QString& modPath, const QStringList& ignoreDirs);
    
    static void scanGameDirectory(const QString& gamePath, const QStringList& ignoreDirs, ScanResult& result);
    static void scanModDirectory(const QString& modPath, const QStringList& ignoreDirs, ScanResult& result);
    static void scanDlcDirectory(const QString& dlcPath, const QStringList& ignoreDirs, ScanResult& result);
    
    static void scanDirectoryRecursive(const QString& rootPath, const QString& currentPath, 
                                     bool isMod, bool isDlc, 
                                     const QStringList& ignoreDirs, 
                                     const QSet<QString>& replacePaths,
                                     ScanResult& result);
    
    static void processFile(const QString& absPath, const QString& relPath, bool isMod, bool isDlc, ScanResult& result);
    static void extractZip(const QString& zipPath, const QString& destPath);
    static void clearCache();
    
    static QString normalizeDlcPath(const QString& path);
    static bool isIgnoredFile(const QString& fileName, const QString& relPath, bool isDlc); // Updated signature

    QMap<QString, FileDetails> m_files;
    QMap<QString, QDateTime> m_fileTimes; // Cache for comparison
    QSet<QString> m_replacePaths;
    QStringList m_ignoreDirs;

    RecursiveFileSystemWatcher* m_watcher;
    QTimer* m_debounceTimer;
    QFutureWatcher<ScanResult>* m_futureWatcher;
    mutable QMutex m_mutex;
    
    bool m_isScanning = false;
};

#endif // FILEMANAGER_H
