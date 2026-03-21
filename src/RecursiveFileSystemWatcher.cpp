#include "RecursiveFileSystemWatcher.h"
#include <QDebug>

WatcherThread::WatcherThread(const QString& path, QObject* parent)
    : QThread(parent), m_path(path), m_running(true) {
    m_hDir = CreateFileW(
        (LPCWSTR)path.utf16(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );
}

WatcherThread::~WatcherThread() {
    stop();
}

void WatcherThread::stop() {
    m_running = false;

    if (m_hDir != INVALID_HANDLE_VALUE) {
        CancelIoEx(m_hDir, NULL);
        CloseHandle(m_hDir);
        m_hDir = INVALID_HANDLE_VALUE;
    }

    if (isRunning()) {
        wait();
    }
}

void WatcherThread::run() {
    if (m_hDir == INVALID_HANDLE_VALUE) return;

    char buffer[4096];
    DWORD bytesReturned = 0;
    
    while (m_running) {
        if (ReadDirectoryChangesW(
            m_hDir,
            &buffer,
            sizeof(buffer),
            TRUE, // Recursive
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            NULL,
            NULL
        )) {
            if (bytesReturned > 0) {
                // Parse changes if needed, but for now just signal that something changed
                // FILE_NOTIFY_INFORMATION* info = (FILE_NOTIFY_INFORMATION*)buffer;
                // ...
                emit changed(m_path);
            }
        } else {
            // Error or cancelled
            break;
        }
    }
}

RecursiveFileSystemWatcher::RecursiveFileSystemWatcher(QObject *parent) : QObject(parent) {}

RecursiveFileSystemWatcher::~RecursiveFileSystemWatcher() {
    removeAllPaths();
}

void RecursiveFileSystemWatcher::addPath(const QString& path) {
    WatcherThread* thread = new WatcherThread(path, this);
    connect(thread, &WatcherThread::changed, this, &RecursiveFileSystemWatcher::fileChanged);
    m_threads.push_back(thread);
    thread->start();
}

void RecursiveFileSystemWatcher::removeAllPaths() {
    for (auto thread : m_threads) {
        thread->stop();
        delete thread;
    }
    m_threads.clear();
}