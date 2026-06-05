//-------------------------------------------------------------------------------------
// ToolQmlHostComponents.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef TOOLQMLHOSTCOMPONENTS_H
#define TOOLQMLHOSTCOMPONENTS_H

class QObject;
class QQmlEngine;
class QJSEngine;
class ToolQmlThemeProvider;

class ToolQmlHostComponents {
public:
    static void registerTypes();
    static QObject* createThemeSingleton(QQmlEngine* engine, QJSEngine* scriptEngine);
    static void setSharedThemeProvider(ToolQmlThemeProvider* themeProvider);
    static ToolQmlThemeProvider* sharedThemeProvider();
};

#endif // TOOLQMLHOSTCOMPONENTS_H
