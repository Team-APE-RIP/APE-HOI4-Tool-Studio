#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

class Logger {
public:
    static Logger& instance();

    void logClick(const QString& context);
    void logError(const QString& context, const QString& message);
    void logWarning(const QString& context, const QString& message);
    void logInfo(const QString& context, const QString& message);
    void openLogDirectory();
    void cleanOldLogs();
    
    // Set log file path (for subprocess to use same log as main process)
    void setLogFilePath(const QString& path);
    QString logFilePath() const;

private:
    Logger();
    ~Logger();
    void write(const QString& type, const QString& context, const QString& message);
    void initLogFile();

    QFile m_logFile;
    QTextStream m_stream;
    QString m_logFilePath;
    QMutex m_mutex;
    bool m_initialized;
};

#endif // LOGGER_H
