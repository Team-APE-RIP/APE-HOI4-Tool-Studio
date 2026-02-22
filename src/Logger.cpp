#include "Logger.h"
#include "ConfigManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfoList>
#include <algorithm>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    // Clean old logs before creating a new one
    cleanOldLogs();

    QString logDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/logs";
    
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString fileName = QString("log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    m_logFile.setFileName(dir.filePath(fileName));
    
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_stream.setDevice(&m_logFile);
        m_stream.setEncoding(QStringConverter::Utf8);
    } else {
        qDebug() << "Failed to open log file:" << m_logFile.fileName();
    }
}

Logger::~Logger() {
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void Logger::write(const QString& type, const QString& context, const QString& message) {
    if (!m_logFile.isOpen()) return;

    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logEntry = QString("[%1] [%2] [%3] %4").arg(time, type, context, message);
    
    m_stream << logEntry << "\n";
    m_stream.flush();
    
    // Also print to debug console
    qDebug() << qPrintable(logEntry);
}

void Logger::logClick(const QString& context) {
    write("CLICK", context, "User clicked");
}

void Logger::logError(const QString& context, const QString& message) {
    write("ERROR", context, message);
}

void Logger::logWarning(const QString& context, const QString& message) {
    write("WARNING", context, message);
}

void Logger::logInfo(const QString& context, const QString& message) {
    write("INFO", context, message);
}

void Logger::openLogDirectory() {
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/logs";
    QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
}

void Logger::cleanOldLogs() {
    int maxFiles = ConfigManager::instance().getMaxLogFiles();
    if (maxFiles <= 0) return;

    QString logDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/logs";
    QDir dir(logDir);
    if (!dir.exists()) return;

    QFileInfoList files = dir.entryInfoList(QStringList() << "log_*.txt", QDir::Files, QDir::Time);
    
    // Files are sorted by time (newest first due to QDir::Time default if not specified otherwise, 
    // but actually it's usually newest first in entryInfoList if we want latest)
    // Let's be explicit.
    std::sort(files.begin(), files.end(), [](const QFileInfo& a, const QFileInfo& b) {
        return a.lastModified() > b.lastModified(); // Newest first
    });

    if (files.size() > maxFiles) {
        for (int i = maxFiles; i < files.size(); ++i) {
            QFile::remove(files[i].absoluteFilePath());
        }
    }
}
