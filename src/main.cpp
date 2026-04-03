//-------------------------------------------------------------------------------------
// main.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "MainWindow.h"
#include "SetupDialog.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "ToolHostMode.h"
#include "AuthManager.h"
#include "LoginDialog.h"
#include "HttpClient.h"
#include "FileAssociationManager.h"
#include "ExternalPackageManager.h"
#include "SingleInstanceManager.h"
#include "RuntimeContextConfigurator.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    // Set application metadata
    a.setApplicationName("APE HOI4 Tool Studio");
    a.setOrganizationName("Team APE-RIP");
    a.setApplicationVersion("1.0.0");

    // Check for tool host mode: --tool-host <server_name> <tool_dll_path> [tool_name] [--log-file <path>]
    const QStringList args = a.arguments();
    if (args.size() >= 4 && args[1] == "--tool-host") {
        const QString toolName = args.size() >= 5 ? args[4] : "Tool";
        QString logFilePath;

        const int logFileIndex = args.indexOf("--log-file");
        if (logFileIndex != -1 && logFileIndex + 1 < args.size()) {
            logFilePath = args[logFileIndex + 1];
        }

        a.setApplicationName(toolName);
        a.setQuitOnLastWindowClosed(false);
        return runToolHostMode(args[2], args[3], toolName, logFilePath);
    }

    ExternalPackageManager::PendingRequest pendingRequest;
    if (args.size() >= 2) {
        const QString possiblePath = args[1].trimmed();
        pendingRequest = ExternalPackageManager::createPendingRequestFromPath(possiblePath);
    }

    SingleInstanceManager singleInstanceManager;
    const bool isPrimaryInstance = singleInstanceManager.acquirePrimaryInstance();
    if (!isPrimaryInstance) {
        QString forwardError;
        singleInstanceManager.forwardArgumentsToPrimary(args, &forwardError);
        return 0;
    }

    QString ipcError;
    singleInstanceManager.startListening(&ipcError);

    if (!pendingRequest.isValid()) {
        pendingRequest = ExternalPackageManager::takePendingRestartRequest();
    }

    // Normal mode: run main application
    ConfigManager& config = ConfigManager::instance();
    configureToolRuntimeContext();
    configurePluginRuntimeContext();

    HttpClient::instance().refreshCaBundleInBackground(
        QUrl(AuthManager::buildApiUrl("/api/v1/security/ca-bundle/meta")),
        QUrl(AuthManager::buildApiUrl("/api/v1/security/ca-bundle"))
    );

    // Initialize AuthManager (loads saved credentials and auto-logs in if available)
    AuthManager::instance().init();

    QString associationError;
    FileAssociationManager::registerFileAssociations(&associationError);

    // Write current application path to path.json for Setup.exe
    QString appDir = QDir::cleanPath(QCoreApplication::applicationDirPath());
    const QString pathJsonDir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
    QDir().mkpath(pathJsonDir);

    QFile pathFile(pathJsonDir + "/path.json");
    if (pathFile.open(QIODevice::WriteOnly)) {
        QJsonObject pathObj;
        pathObj["path"] = appDir;
        pathObj["auto"] = "0";
        const QJsonDocument pathDoc(pathObj);
        pathFile.write(pathDoc.toJson());
        pathFile.close();
    }

    // Clean update cache if exists
    const QString updateCacheDir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/APE-HOI4-Tool-Studio/update_cache";
    if (QDir(updateCacheDir).exists()) {
        QDir(updateCacheDir).removeRecursively();
    }

    // Check for setup cache language
    const QString tempDir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/APE-HOI4-Tool-Studio/setup_cache";

    // Clean up setup cache exe if exists
    const QString setupCacheExe = tempDir + "/Setup.exe";
    if (QFile::exists(setupCacheExe)) {
        QFile::remove(setupCacheExe);
    }

    const QString tempLangFile = tempDir + "/temp_lang.json";
    QFile tFile(tempLangFile);
    if (tFile.exists() && tFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(tFile.readAll());
        const QJsonObject obj = doc.object();
        if (obj.contains("language")) {
            const QString tempLang = obj["language"].toString();
            if (tempLang == "English" || tempLang == "简体中文" || tempLang == "繁體中文") {
                config.setLanguage(tempLang);
            }
        }
        tFile.close();
        QFile::remove(tempLangFile);
    }

    // Load language immediately
    LocalizationManager::instance().loadLanguage(config.getLanguage());

    MainWindow w(pendingRequest);
    QObject::connect(
        &singleInstanceManager,
        &SingleInstanceManager::argumentsReceived,
        &w,
        [&w](const QStringList& incomingArgs) {
            if (incomingArgs.size() < 2) {
                return;
            }

            const ExternalPackageManager::PendingRequest incomingRequest =
                ExternalPackageManager::createPendingRequestFromPath(incomingArgs[1].trimmed());
            if (!incomingRequest.isValid()) {
                return;
            }

            w.handleExternalRequest(incomingRequest);
        }
    );

    w.show();

    return a.exec();
}