//-------------------------------------------------------------------------------------
// SingleInstanceManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "SingleInstanceManager.h"

#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr wchar_t kInstanceMutexName[] = L"Global\\Team-APE-RIP.APEHOI4ToolStudio.Singleton";
constexpr const char* kInstanceServerName = "Team-APE-RIP.APEHOI4ToolStudio.LocalServer";
}

SingleInstanceManager::SingleInstanceManager(QObject* parent)
    : QObject(parent) {
}

SingleInstanceManager::~SingleInstanceManager() {
#ifdef Q_OS_WIN
    if (m_mutexHandle) {
        CloseHandle(reinterpret_cast<HANDLE>(m_mutexHandle));
        m_mutexHandle = nullptr;
    }
#endif

    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

bool SingleInstanceManager::acquirePrimaryInstance() {
#ifdef Q_OS_WIN
    HANDLE mutexHandle = CreateMutexW(nullptr, FALSE, kInstanceMutexName);
    if (!mutexHandle) {
        return false;
    }

    m_mutexHandle = mutexHandle;
    const DWORD lastError = GetLastError();
    return lastError != ERROR_ALREADY_EXISTS;
#else
    return true;
#endif
}

bool SingleInstanceManager::startListening(QString* errorMessage) {
    if (m_server) {
        return true;
    }

    QLocalServer::removeServer(serverName());

    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstanceManager::handleNewConnection);

    if (!m_server->listen(serverName())) {
        if (errorMessage) {
            *errorMessage = m_server->errorString();
        }
        m_server->deleteLater();
        m_server = nullptr;
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool SingleInstanceManager::forwardArgumentsToPrimary(const QStringList& arguments, QString* errorMessage) {
    QLocalSocket socket;
    socket.connectToServer(serverName());

    if (!socket.waitForConnected(1500)) {
        if (errorMessage) {
            *errorMessage = socket.errorString();
        }
        return false;
    }

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out << arguments;
    socket.write(payload);

    if (!socket.waitForBytesWritten(1500)) {
        if (errorMessage) {
            *errorMessage = socket.errorString();
        }
        socket.disconnectFromServer();
        return false;
    }

    socket.flush();
    socket.disconnectFromServer();

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void SingleInstanceManager::handleNewConnection() {
    if (!m_server) {
        return;
    }

    QLocalSocket* socket = m_server->nextPendingConnection();
    if (!socket) {
        return;
    }

    connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
        QDataStream in(socket);
        QStringList arguments;
        in >> arguments;
        emit argumentsReceived(arguments);
        socket->disconnectFromServer();
        socket->deleteLater();
    });

    connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
}

QString SingleInstanceManager::serverName() const {
    return QString::fromUtf8(kInstanceServerName);
}