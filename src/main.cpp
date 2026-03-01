#include "MainWindow.h"
#include "SetupDialog.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include "ToolHostMode.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // Set application metadata
    a.setApplicationName("APE HOI4 Tool Studio");
    a.setOrganizationName("Team APE-RIP");
    a.setApplicationVersion("1.0.0");
    
    // Check for tool host mode: --tool-host <server_name> <tool_dll_path> [tool_name] [--log-file <path>]
    QStringList args = a.arguments();
    if (args.size() >= 4 && args[1] == "--tool-host") {
        // Run in tool host mode (as subprocess for a tool)
        QString toolName = args.size() >= 5 ? args[4] : "Tool";
        QString logFilePath;
        
        // Parse optional --log-file argument
        int logFileIndex = args.indexOf("--log-file");
        if (logFileIndex != -1 && logFileIndex + 1 < args.size()) {
            logFilePath = args[logFileIndex + 1];
        }
        
        a.setApplicationName(toolName);
        a.setQuitOnLastWindowClosed(false);
        return runToolHostMode(args[2], args[3], toolName, logFilePath);
    }
    
    // Normal mode: run main application
    ConfigManager& config = ConfigManager::instance();
    
    // Write current application path to path.json for Setup.exe
    QString appDir = QCoreApplication::applicationDirPath();
    appDir = QDir::cleanPath(appDir);
    QString pathJsonDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio";
    QDir().mkpath(pathJsonDir);
    QFile pathFile(pathJsonDir + "/path.json");
    if (pathFile.open(QIODevice::WriteOnly)) {
        QJsonObject pathObj;
        pathObj["path"] = appDir;
        pathObj["auto"] = "0";
        QJsonDocument pathDoc(pathObj);
        pathFile.write(pathDoc.toJson());
        pathFile.close();
    }
    
    // Check for setup cache language
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/APE-HOI4-Tool-Studio/setup_cache";
    QString tempLangFile = tempDir + "/temp_lang.json";
    QFile tFile(tempLangFile);
    if (tFile.exists() && tFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(tFile.readAll());
        QJsonObject obj = doc.object();
        if (obj.contains("language")) {
            QString tempLang = obj["language"].toString();
            if (tempLang == "English" || tempLang == "简体中文" || tempLang == "繁體中文") {
                config.setLanguage(tempLang);
            }
        }
        tFile.close();
        // Remove the temp language file after reading
        QFile::remove(tempLangFile);
    }

    // Load language immediately
    LocalizationManager::instance().loadLanguage(config.getLanguage());

    // If game path is not set OR mod path is not set, show setup
    if (config.isFirstRun() || !config.hasModSelected()) {
        SetupDialog setup;
        if (setup.exec() == QDialog::Accepted) {
            config.setGamePath(setup.getGamePath());
            config.setModPath(setup.getModPath());
            config.setLanguage(setup.getLanguage());
        } else {
            return 0; // User cancelled setup
        }
    }

    MainWindow w;
    w.show();

    return a.exec();
}
