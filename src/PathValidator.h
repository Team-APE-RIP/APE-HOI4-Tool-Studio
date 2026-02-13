#ifndef PATHVALIDATOR_H
#define PATHVALIDATOR_H

#include <QObject>
#include <QString>
#include <QTimer>

class PathValidator : public QObject {
    Q_OBJECT

public:
    static PathValidator& instance();

    // Returns error message key, or empty if valid
    QString validateGamePath(const QString& path);
    QString validateModPath(const QString& path);
    
    void startMonitoring();
    void stopMonitoring();

signals:
    void pathInvalid(const QString& titleKey, const QString& msgKey);

private:
    PathValidator();
    QTimer* m_timer;
    
    void checkPaths();
};

#endif // PATHVALIDATOR_H
