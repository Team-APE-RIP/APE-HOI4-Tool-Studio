//-------------------------------------------------------------------------------------
// ToolHostMain.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "ToolHostMode.h"
#include <QCoreApplication>

// Standalone tool host executable entry point
// This is a non-GUI process that hosts tool worker processes
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    // Set application metadata
    app.setApplicationName("APE HOI4 Tool Studio - Tool Host");
    app.setOrganizationName("Team APE-RIP");
    app.setApplicationVersion("1.0.0");
    // Note: QCoreApplication doesn't have setQuitOnLastWindowClosed (it's a QGuiApplication method)
    // This is a non-GUI process, so it doesn't need that setting
    
    // Parse arguments: <server_name> <tool_dll_path> [tool_name] [--log-file <path>]
    const QStringList args = app.arguments();
    if (args.size() < 3) {
        return 1;
    }
    
    const QString serverName = args[1];
    const QString toolPath = args[2];
    const QString toolName = args.size() >= 4 ? args[3] : "Tool";
    QString logFilePath;
    
    const int logFileIndex = args.indexOf("--log-file");
    if (logFileIndex != -1 && logFileIndex + 1 < args.size()) {
        logFilePath = args[logFileIndex + 1];
    }
    
    return runToolHostMode(serverName, toolPath, toolName, logFilePath);
}
