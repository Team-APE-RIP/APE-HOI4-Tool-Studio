//-------------------------------------------------------------------------------------
// PathValidator.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
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
    QString validateDocPath(const QString& path);
    
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
