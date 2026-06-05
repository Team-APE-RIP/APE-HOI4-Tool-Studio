//-------------------------------------------------------------------------------------
// ToolQmlHostComponents.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolQmlHostComponents.h"

#include "ToolQmlThemeProvider.h"

#include <QJSEngine>
#include <QQmlEngine>
#include <QUrl>
#include <QtQml>

namespace {
ToolQmlThemeProvider* g_sharedThemeProvider = nullptr;
bool g_typesRegistered = false;

QObject* createThemeProviderSingleton(QQmlEngine* engine, QJSEngine* scriptEngine) {
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    return g_sharedThemeProvider;
}

void registerVisualType(const char* typeName, const char* resourceUrl) {
    qmlRegisterType(QUrl(QString::fromUtf8(resourceUrl)), "APE.ToolHost", 1, 0, typeName);
}
}

void ToolQmlHostComponents::registerTypes() {
    if (g_typesRegistered) {
        return;
    }

    registerVisualType("TopBar", "qrc:/toolhost/TopBar.qml");
    registerVisualType("StatusBar", "qrc:/toolhost/StatusBar.qml");
    registerVisualType("LoadingOverlay", "qrc:/toolhost/LoadingOverlay.qml");
    registerVisualType("MacScrollBar", "qrc:/toolhost/MacScrollBar.qml");
    registerVisualType("FormTextField", "qrc:/toolhost/FormTextField.qml");
    registerVisualType("FormSpinBox", "qrc:/toolhost/FormSpinBox.qml");
    registerVisualType("FormCheckBox", "qrc:/toolhost/FormCheckBox.qml");
    registerVisualType("ThemedToolTip", "qrc:/toolhost/ThemedToolTip.qml");
    qmlRegisterSingletonType<ToolQmlThemeProvider>(
        "APE.ToolHost",
        1,
        0,
        "HostTheme",
        createThemeProviderSingleton
    );

    g_typesRegistered = true;
}

QObject* ToolQmlHostComponents::createThemeSingleton(QQmlEngine* engine, QJSEngine* scriptEngine) {
    return createThemeProviderSingleton(engine, scriptEngine);
}

void ToolQmlHostComponents::setSharedThemeProvider(ToolQmlThemeProvider* themeProvider) {
    g_sharedThemeProvider = themeProvider;
}

ToolQmlThemeProvider* ToolQmlHostComponents::sharedThemeProvider() {
    return g_sharedThemeProvider;
}
