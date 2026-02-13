#ifndef RECURSIVEFILESYSTEMWATCHER_H
#define RECURSIVEFILESYSTEMWATCHER_H

#include <QObject>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <vector>
#include <windows.h>

class WatcherThread : public QThread {
    Q_OBJECT
public:
    WatcherThread(const QString& path, QObject* parent = nullptr);
    ~WatcherThread();

    void stop();

signals:
    void changed(const QString& path);

protected:
    void run() override;

private:
    QString m_path;
    HANDLE m_hDir;
    bool m_running;
};

class RecursiveFileSystemWatcher : public QObject {
    Q_OBJECT

public:
    explicit RecursiveFileSystemWatcher(QObject *parent = nullptr);
    ~RecursiveFileSystemWatcher();

    void addPath(const QString& path);
    void removeAllPaths();

signals:
    void fileChanged(const QString& path);

private:
    std::vector<WatcherThread*> m_threads;
};

#endif // RECURSIVEFILESYSTEMWATCHER_H
