#include "FileManagerTool.h"
#include "FileTreeWidget.h"
#include <QIcon>
#include <QFile>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QPluginLoader>
#include <QTextStream>
#include <QStringConverter>

QString FileManagerTool::name() const {
    return m_localizedNames.value(m_currentLang, "File Manager");
}

QString FileManagerTool::description() const {
    return m_localizedDescs.value(m_currentLang, "Browse and manage game files with mod priority.");
}

void FileManagerTool::setMetaData(const QJsonObject& metaData) {
    m_id = metaData.value("id").toString();
    m_version = metaData.value("version").toString();
    m_compatibleVersion = metaData.value("compatibleVersion").toString();
    m_author = metaData.value("author").toString();
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
    m_treeWidget = new FileTreeWidget(parent);
    return m_treeWidget;
}

void FileManagerTool::applyTheme() {
    if (m_treeWidget) {
        m_treeWidget->updateTheme();
    }
}

void FileManagerTool::loadLanguage(const QString& lang) {
    const QString normalizedLang = normalizeLanguageCode(lang);
    const QString rootPath = localisationRootPath();

    m_currentLang = normalizedLang;
    m_localizedStrings.clear();

    const QMap<QString, QString> englishStrings = parseSimpleYamlFile(rootPath + "/en_US/strings.yml");
    for (auto it = englishStrings.constBegin(); it != englishStrings.constEnd(); ++it) {
        m_localizedStrings.insert(it.key(), it.value());
    }

    if (normalizedLang != "en_US") {
        const QMap<QString, QString> selectedStrings =
            parseSimpleYamlFile(rootPath + "/" + normalizedLang + "/strings.yml");
        for (auto it = selectedStrings.constBegin(); it != selectedStrings.constEnd(); ++it) {
            m_localizedStrings.insert(it.key(), it.value());
        }
    }

    m_localizedNames["en_US"] = englishStrings.value("Name", "File Manager");
    m_localizedDescs["en_US"] = englishStrings.value("Description", "Browse and manage game files with mod priority.");

    if (normalizedLang != "en_US") {
        m_localizedNames[normalizedLang] = m_localizedStrings.value("Name", m_localizedNames.value("en_US"));
        m_localizedDescs[normalizedLang] = m_localizedStrings.value("Description", m_localizedDescs.value("en_US"));
    }
}

QString FileManagerTool::normalizeLanguageCode(const QString& lang) const {
    const QString normalized = lang.trimmed();
    const QString rootPath = localisationRootPath();
    QDir rootDir(rootPath);
    
    if (!rootDir.exists()) {
        return "en_US";
    }

    const QStringList languageDirectories = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    
    // Exact match
    if (languageDirectories.contains(normalized)) {
        return normalized;
    }
    
    // Scan meta.htsl for text match
    for (const QString& dirName : languageDirectories) {
        const QString metaPath = rootDir.filePath(dirName + "/meta.htsl");
        QFile file(metaPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            while (!stream.atEnd()) {
                QString line = stream.readLine().trimmed();
                if (line.startsWith("text=") || line.startsWith("text =")) {
                    QString text = line.mid(line.indexOf('=') + 1).trimmed();
                    if (text.startsWith('"') && text.endsWith('"')) {
                        text = text.mid(1, text.length() - 2);
                    }
                    if (text == normalized) {
                        return dirName;
                    }
                }
            }
        }
    }

    return "en_US";
}

QString FileManagerTool::localisationRootPath() const {
    QDir appDir(QCoreApplication::applicationDirPath());

    if (QDir(appDir.filePath("tools/FileManagerTool/localisation")).exists()) {
        return appDir.filePath("tools/FileManagerTool/localisation");
    }
    if (QDir(appDir.filePath("../tools/FileManagerTool/localisation")).exists()) {
        return appDir.filePath("../tools/FileManagerTool/localisation");
    }
    return appDir.filePath("tools/FileManagerTool/localisation");
}

QMap<QString, QString> FileManagerTool::parseSimpleYamlFile(const QString& filePath) const {
    QMap<QString, QString> parsed;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return parsed;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#endif

    bool inLanguageBlock = false;
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.trimmed().isEmpty()) {
            continue;
        }

        if (!line.startsWith(' ') && !line.startsWith('\t')) {
            if (line.trimmed().startsWith("l_") && line.trimmed().endsWith(':')) {
                inLanguageBlock = true;
            }
            continue;
        }

        if (!inLanguageBlock) {
            continue;
        }

        const QString trimmed = line.trimmed();
        if (trimmed.startsWith('#')) {
            continue;
        }

        const int colonIndex = trimmed.indexOf(':');
        if (colonIndex <= 0) {
            continue;
        }

        const QString key = trimmed.left(colonIndex).trimmed();
        QString value = trimmed.mid(colonIndex + 1).trimmed();

        if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\''))) {
            value = value.mid(1, value.length() - 2);
        }

        value.replace("\\n", "\n");
        parsed.insert(key, value);
    }

    return parsed;
}