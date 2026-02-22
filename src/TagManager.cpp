#include "TagManager.h"
#include "FileManager.h"
#include "Logger.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

TagManager::TagManager() {
    connect(&FileManager::instance(), &FileManager::scanFinished, this, &TagManager::onFileScanFinished);
}

TagManager& TagManager::instance() {
    static TagManager instance;
    return instance;
}

void TagManager::onFileScanFinished() {
    scanTags();
}

QMap<QString, QString> TagManager::getTags() const {
    QMutexLocker locker(&m_mutex);
    return m_tags;
}

void TagManager::scanTags() {
    Logger::instance().logInfo("TagManager", "Scanning country tags...");
    
    QMap<QString, QString> newTags;
    
    // Get all files in common/country_tags
    // FileManager returns effective files (mod overrides game)
    QMap<QString, FileDetails> allFiles = FileManager::instance().getEffectiveFiles();
    
    for (auto it = allFiles.begin(); it != allFiles.end(); ++it) {
        QString relPath = it.key();
        // Normalize separators just in case
        QString normalizedRelPath = relPath.replace("\\", "/");
        
        if (normalizedRelPath.startsWith("common/country_tags/") && normalizedRelPath.endsWith(".txt")) {
            parseFile(it.value().absPath, newTags);
        }
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_tags = newTags;
    }
    
    Logger::instance().logInfo("TagManager", QString("Found %1 country tags.").arg(newTags.size()));
    emit tagsUpdated();
}

QString TagManager::removeComments(const QString& content) {
    QString result;
    result.reserve(content.size());
    
    bool inQuote = false;
    bool inComment = false;
    
    for (int i = 0; i < content.size(); ++i) {
        QChar c = content[i];
        
        if (inComment) {
            if (c == '\n') {
                inComment = false;
                result.append(c);
            }
            // Skip comment content
        } else {
            if (c == '"') {
                // Handle escaped quotes if necessary, though Paradox script rarely uses them
                // Simple toggle for now
                inQuote = !inQuote;
                result.append(c);
            } else if (c == '#' && !inQuote) {
                inComment = true;
            } else {
                result.append(c);
            }
        }
    }
    return result;
}

void TagManager::parseFile(const QString& filePath, QMap<QString, QString>& tags) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::instance().logError("TagManager", "Failed to open file: " + filePath);
        return;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    QString cleanContent = removeComments(content);
    
    // Check for dynamic_tags = yes
    // Regex: dynamic_tags\s*=\s*yes
    // We use MultilineOption to handle cases where it might span lines (though \s matches newlines anyway)
    static QRegularExpression dynamicRe("dynamic_tags\\s*=\\s*yes", QRegularExpression::CaseInsensitiveOption);
    if (dynamicRe.match(cleanContent).hasMatch()) {
        Logger::instance().logInfo("TagManager", "Skipping dynamic tags file: " + filePath);
        return;
    }
    
    // Parse TAG = "path"
    // Regex: ([A-Z0-9]{3})\s*=\s*"([^"]+)"
    // Captures: 1=TAG, 2=PATH
    static QRegularExpression tagRe("([A-Z0-9]{3})\\s*=\\s*\"([^\"]+)\"");
    
    QRegularExpressionMatchIterator i = tagRe.globalMatch(cleanContent);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString tag = match.captured(1);
        QString path = match.captured(2);
        
        if (!tags.contains(tag)) {
            tags.insert(tag, path);
        } else {
            // If a tag is redefined, we log a warning but keep the first one found (arbitrary choice unless we sort files)
            // Since FileManager doesn't guarantee order of files in the map, this is non-deterministic if duplicates exist across files.
            // But usually tags are unique.
            Logger::instance().logWarning("TagManager", "Duplicate tag definition found: " + tag + " in " + filePath);
        }
    }
}
