#include "FileManagerTool.h"
#include "FileTreeWidget.h"
#include <QIcon>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QPluginLoader>

QString FileManagerTool::name() const {
    return m_localizedNames.value(m_currentLang, "File Manager");
}

QString FileManagerTool::description() const {
    return m_localizedDescs.value(m_currentLang, "Browse and manage game files with mod priority.");
}

QIcon FileManagerTool::icon() const {
    // Determine path to cover.png
    // It should be in the same directory as the plugin DLL
    // We can use QPluginLoader logic or just assume standard path
    
    // Try to find the tools directory (same logic as loadLanguage)
    QDir appDir(QCoreApplication::applicationDirPath());
    if (appDir.dirName() == "bin" || appDir.dirName() == "debug" || appDir.dirName() == "release") {
        // Handle dev environment
    }
    
    QString toolsPath;
    if (appDir.exists("tools")) {
        toolsPath = appDir.filePath("tools");
    } else if (QDir(appDir.filePath("../tools")).exists()) {
        toolsPath = appDir.filePath("../tools");
    }
    
    if (!toolsPath.isEmpty()) {
        QString coverPath = toolsPath + "/FileManagerTool/cover.png";
        if (QFile::exists(coverPath)) {
            return QIcon(coverPath);
        }
    }
    
    // Fallback
    return QIcon::fromTheme("folder"); 
}

void FileManagerTool::initialize() {
    // Load initial language or default
    loadLanguage("en_US");
}

QWidget* FileManagerTool::createWidget(QWidget* parent) {
    return new FileTreeWidget(parent);
}

void FileManagerTool::loadLanguage(const QString& lang) {
    m_currentLang = lang;
    
    QDir appDir(QCoreApplication::applicationDirPath());
    QString toolsPath;
    if (appDir.exists("tools")) {
        toolsPath = appDir.filePath("tools");
    } else if (QDir(appDir.filePath("../tools")).exists()) {
        toolsPath = appDir.filePath("../tools");
    } else {
        qDebug() << "FileManagerTool: Tools directory not found.";
        return;
    }
    
    QString locPath = toolsPath + "/FileManagerTool/localization";
    QString langFile = lang;
    if (lang == "简体中文") langFile = "zh_CN";
    else if (lang == "繁體中文") langFile = "zh_TW";
    else if (lang == "English") langFile = "en_US";
    
    QString filePath = locPath + "/" + langFile + ".json";
    
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        
        if (obj.contains("Name")) {
            m_localizedNames[lang] = obj["Name"].toString();
        }
        if (obj.contains("Description")) {
            m_localizedDescs[lang] = obj["Description"].toString();
        }
    } else {
        qDebug() << "FileManagerTool: Failed to load localization file:" << filePath;
    }
}
