//-------------------------------------------------------------------------------------
// GamePathDiscovery.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef GAMEPATHDISCOVERY_H
#define GAMEPATHDISCOVERY_H

#include <QString>
#include <QStringList>

class GamePathDiscovery {
public:
    static QString findGamePath();
    static bool isValidGamePath(const QString& path);
    static QStringList requiredEntries();
};

#endif // GAMEPATHDISCOVERY_H
