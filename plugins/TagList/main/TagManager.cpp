#include "TagManager.h"
#include "../../../src/FileManager.h"
#include "../../../src/Logger.h"
#include "../../../src/PluginRuntimeContext.h"

#include <QRegularExpression>

TagManager::TagManager() {}

TagManager& TagManager::instance() {
    static TagManager instance;
    return instance;
}

QMap<QString, QString> TagManager::getTags() const {
    QMutexLocker locker(&m_mutex);
    return m_tags;
}

QJsonObject TagManager::toJson() const {
    QMutexLocker locker(&m_mutex);
    QJsonObject obj;
    for (auto it = m_tags.begin(); it != m_tags.end(); ++it) {
        obj[it.key()] = it.value();
    }
    return obj;
}

void TagManager::setFromJson(const QJsonObject& obj) {
    QMutexLocker locker(&m_mutex);
    m_tags.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m_tags[it.key()] = it.value().toString();
    }
    Logger::instance().logInfo("TagListPlugin", QString("Loaded %1 tags from JSON data").arg(m_tags.size()));
}

void TagManager::scanTags() {
    Logger::instance().logInfo("TagListPlugin", "Scanning country tags...");

    QMap<QString, QString> newTags;
    const QMap<QString, FileDetails> allFiles = FileManager::instance().getEffectiveFiles();

    for (auto it = allFiles.begin(); it != allFiles.end(); ++it) {
        QString normalizedRelPath = it.key();
        normalizedRelPath.replace("\\", "/");

        if (!normalizedRelPath.startsWith("common/country_tags/") || !normalizedRelPath.endsWith(".txt")) {
            continue;
        }

        const PluginRuntimeContext::TextReadResult readResult =
            PluginRuntimeContext::instance().readEffectiveTextFile(normalizedRelPath);

        if (!readResult.success) {
            Logger::instance().logError(
                "TagListPlugin",
                QString("Failed to read effective tag file %1: %2").arg(normalizedRelPath, readResult.errorMessage)
            );
            continue;
        }

        parseFileContent(normalizedRelPath, readResult.content, newTags);
    }

    {
        QMutexLocker locker(&m_mutex);
        m_tags = newTags;
    }

    Logger::instance().logInfo("TagListPlugin", QString("Found %1 country tags.").arg(newTags.size()));
    emit tagsUpdated();
}

QString TagManager::removeComments(const QString& content) {
    QString result;
    result.reserve(content.size());

    bool inQuote = false;
    bool inComment = false;

    for (int i = 0; i < content.size(); ++i) {
        const QChar c = content[i];

        if (inComment) {
            if (c == '\n') {
                inComment = false;
                result.append(c);
            }
        } else {
            if (c == '"') {
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

void TagManager::parseFileContent(const QString& relativePath, const QString& content, QMap<QString, QString>& tags) {
    const QString cleanContent = removeComments(content);

    static QRegularExpression dynamicRe("dynamic_tags\\s*=\\s*yes", QRegularExpression::CaseInsensitiveOption);
    if (dynamicRe.match(cleanContent).hasMatch()) {
        Logger::instance().logInfo("TagListPlugin", "Skipping dynamic tags file: " + relativePath);
        return;
    }

    static QRegularExpression tagRe("([A-Z0-9]{3})\\s*=\\s*\"([^\"]+)\"");
    QRegularExpressionMatchIterator it = tagRe.globalMatch(cleanContent);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString tag = match.captured(1);
        const QString path = match.captured(2);

        if (!tags.contains(tag)) {
            tags.insert(tag, path);
        } else {
            Logger::instance().logWarning("TagListPlugin", "Duplicate tag definition found: " + tag + " in " + relativePath);
        }
    }
}