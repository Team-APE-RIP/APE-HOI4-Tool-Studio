//-------------------------------------------------------------------------------------
// SslConfig.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "SslConfig.h"
#include <QDebug>

namespace SslConfig {

QSslConfiguration pinnedConfiguration() {
    return QSslConfiguration::defaultConfiguration();
}

void applyPinnedConfiguration(QNetworkRequest& request) {
    request.setSslConfiguration(pinnedConfiguration());
}

void handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors) {
    Q_UNUSED(reply);

    if (errors.isEmpty()) {
        return;
    }

    qDebug() << "SslConfig: SSL errors reported by Qt:";
    for (const QSslError& err : errors) {
        qDebug() << "  -" << err.errorString();
    }

    qDebug() << "SslConfig: Using system default TLS trust chain. SSL errors are not ignored.";
}

} // namespace SslConfig