//-------------------------------------------------------------------------------------
// SingleInstanceManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef SINGLEINSTANCEMANAGER_H
#define SINGLEINSTANCEMANAGER_H

#include <QObject>
#include <QStringList>

class QLocalServer;
class QLocalSocket;

class SingleInstanceManager : public QObject {
    Q_OBJECT

public:
    explicit SingleInstanceManager(QObject* parent = nullptr);
    ~SingleInstanceManager() override;

    bool acquirePrimaryInstance();
    bool startListening(QString* errorMessage = nullptr);
    bool forwardArgumentsToPrimary(const QStringList& arguments, QString* errorMessage = nullptr);

signals:
    void argumentsReceived(const QStringList& arguments);

private:
    void handleNewConnection();
    QString serverName() const;

private:
    void* m_mutexHandle = nullptr;
    QLocalServer* m_server = nullptr;
};

#endif // SINGLEINSTANCEMANAGER_H