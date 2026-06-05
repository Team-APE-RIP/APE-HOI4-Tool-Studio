//-------------------------------------------------------------------------------------
// main.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "MainWindow.h"
#include "ApiRequests.h"
#include "SetupDialog.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "AuthManager.h"
#include "LoginDialog.h"
#include "HttpClient.h"
#include "FileAssociationManager.h"
#include "ExternalPackageManager.h"
#include "SingleInstanceManager.h"
#include "RuntimeContextConfigurator.h"
#include "StartupSplashScreen.h"
#include "PackageRegistry.h"
#include "Logger.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSysInfo>

#ifdef Q_OS_WIN
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

QString qtRuntimeCacheRootPath() {
    return QDir::cleanPath(QDir::temp().filePath(QStringLiteral("APE-HOI4-Tool-Studio/cache")));
}

QString qtPipelineCacheFilePath(const QString& cacheRootPath) {
    const QString buildAbi = QSysInfo::buildAbi().trimmed().isEmpty()
        ? QStringLiteral("default")
        : QSysInfo::buildAbi().trimmed();
    return QDir(cacheRootPath).filePath(QStringLiteral("qtpipelinecache-%1").arg(buildAbi));
}

QByteArray nativeUtf8Path(const QString& path) {
    return QDir::toNativeSeparators(path).toUtf8();
}

void configureQtRuntimeCachePaths() {
    const QString cacheRootPath = qtRuntimeCacheRootPath();
    QDir().mkpath(cacheRootPath);

    qputenv("QML_DISK_CACHE_PATH", nativeUtf8Path(cacheRootPath));

    const QString pipelineCacheFilePath = qtPipelineCacheFilePath(cacheRootPath);
    qputenv("QSG_RHI_PIPELINE_CACHE_LOAD", nativeUtf8Path(pipelineCacheFilePath));
    qputenv("QSG_RHI_PIPELINE_CACHE_SAVE", nativeUtf8Path(pipelineCacheFilePath));
}

} // namespace

int main(int argc, char* argv[]) {
    configureQtRuntimeCachePaths();

    QApplication a(argc, argv);

    // Set application metadata
    a.setApplicationName("APE HOI4 Tool Studio");
    a.setOrganizationName("Team-APE-RIP");
    a.setApplicationVersion("1.0.0");

    const QStringList args = a.arguments();
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

    StartupSplashScreen splash;
    splash.showSplash();
    splash.setProgress(0.08, StartupSplashStage::CheckingInstance);

    QString ipcError;
    singleInstanceManager.startListening(&ipcError);

    if (!pendingRequest.isValid()) {
        pendingRequest = ExternalPackageManager::takePendingRestartRequest();
    }

    // Normal mode: run main application
    splash.setProgress(0.16, StartupSplashStage::PreparingRuntime);
    ConfigManager& config = ConfigManager::instance();

    // Load language before the remaining startup work so splash status text can
    // follow the same dynamic localisation path as the rest of the application.
    splash.setProgress(0.22, StartupSplashStage::LoadingLanguage);
    LocalizationManager::instance().loadLanguage(config.getLanguage());
    splash.applyLocalizedStrings();

    splash.setProgress(0.30, StartupSplashStage::ConfiguringRuntime);
    configureToolRuntimeContext();
    configurePluginRuntimeContext();

    splash.setProgress(0.36, StartupSplashStage::OrganizingPackages);
    QString packageSyncError;
    if (!PackageRegistry::synchronizeInstalledPackages(QCoreApplication::applicationDirPath(), &packageSyncError)) {
        Logger::instance().logWarning("Startup", "Package registry synchronization failed: " + packageSyncError);
    }

    splash.setProgress(0.40, StartupSplashStage::RefreshingSecurityBundle);
    HttpClient::instance().refreshCaBundleInBackground(
        ApiRequests::caBundleMetadataUrl(),
        ApiRequests::caBundleDownloadUrl()
    );

    // Initialize AuthManager (loads saved credentials and auto-logs in if available)
    splash.setProgress(0.50, StartupSplashStage::PreparingAccountSession);
    AuthManager::instance().init();

    splash.setProgress(0.60, StartupSplashStage::RegisteringFileAssociations);
    QString associationError;
    FileAssociationManager::registerFileAssociations(&associationError);

    // Clean update cache if exists
    splash.setProgress(0.70, StartupSplashStage::CleaningCaches);
    const QString updateCacheDir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/APE-HOI4-Tool-Studio/update_cache";
    if (QDir(updateCacheDir).exists()) {
        QDir(updateCacheDir).removeRecursively();
    }

    const QString tempDir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + "/APE-HOI4-Tool-Studio/setup_cache";

    // Clean up setup cache exe if exists
    const QString setupCacheExe = tempDir + "/Setup.exe";
    if (QFile::exists(setupCacheExe)) {
        QFile::remove(setupCacheExe);
    }

    splash.setProgress(0.82, StartupSplashStage::BuildingMainWindow);
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
    splash.setProgress(0.96, StartupSplashStage::OpeningMainWindow);
    splash.finishWithMainWindow(&w);

    return a.exec();
}
