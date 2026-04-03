//-------------------------------------------------------------------------------------
// SslConfig.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef SSLCONFIG_H
#define SSLCONFIG_H

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslError>

namespace SslConfig {
    QSslConfiguration pinnedConfiguration();
    void applyPinnedConfiguration(QNetworkRequest& request);
    void handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors);
}

#endif // SSLCONFIG_H
