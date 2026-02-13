#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

class Logger {
public:
    static Logger& instance();

    void logClick(const QString& context);
    void logError(const QString& context, const QString& message);
    void logWarning(const QString& context, const QString& message); // Added
    void logInfo(const QString& context, const QString& message);
    void openLogDirectory();

private:
    Logger();
    ~Logger();
    void write(const QString& type, const QString& context, const QString& message);
    QString getLogFilePath() const;

    QFile m_logFile;
    QTextStream m_stream;
};

#endif // LOGGER_H
