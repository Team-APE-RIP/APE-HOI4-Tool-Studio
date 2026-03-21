#include "ToolDescriptorParser.h"

#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>

namespace {
bool parseLine(const QString& line, QString& key, QString& value) {
    QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (trimmed.startsWith("#") || trimmed.startsWith("//")) {
        return false;
    }

    static const QRegularExpression pattern("^([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*\"(.*)\"\\s*$");
    const QRegularExpressionMatch match = pattern.match(trimmed);
    if (!match.hasMatch()) {
        return false;
    }

    key = match.captured(1).trimmed();
    value = match.captured(2);
    return !key.isEmpty();
}

bool isCommentOrEmpty(const QString& line) {
    const QString trimmed = line.trimmed();
    return trimmed.isEmpty() || trimmed.startsWith("#") || trimmed.startsWith("//");
}

QJsonArray parseDependenciesBlock(const QStringList& lines, int& index) {
    QJsonArray dependencies;

    for (++index; index < lines.size(); ++index) {
        QString line = lines[index];
        line.replace("\r", "");
        const QString trimmed = line.trimmed();

        if (isCommentOrEmpty(trimmed)) {
            continue;
        }

        if (trimmed == "}") {
            break;
        }

        static const QRegularExpression dependencyPattern("^\"([^\"]+)\"\\s*$");
        const QRegularExpressionMatch match = dependencyPattern.match(trimmed);
        if (match.hasMatch()) {
            dependencies.append(match.captured(1).trimmed());
        }
    }

    return dependencies;
}
}

namespace ToolDescriptorParser {

bool parseDescriptorFile(const QString& filePath, QJsonObject& outMetaData, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QString("Failed to open descriptor file: %1").arg(filePath);
        }
        return false;
    }

    QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
    file.close();

    QString name;
    QString version;
    QString supportedVersion;
    QString author;
    QJsonArray dependencies;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        line.replace("\r", "");
        const QString trimmed = line.trimmed();

        if (isCommentOrEmpty(trimmed)) {
            continue;
        }

        if (trimmed == "dependencies={") {
            dependencies = parseDependenciesBlock(lines, i);
            continue;
        }

        QString key;
        QString value;
        if (!parseLine(line, key, value)) {
            continue;
        }

        if (key == "name") {
            name = value;
        } else if (key == "version") {
            version = value;
        } else if (key == "supported_version") {
            supportedVersion = value;
        } else if (key == "author") {
            author = value;
        }
    }

    if (name.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("Descriptor missing name: %1").arg(filePath);
        }
        return false;
    }

    outMetaData = QJsonObject{
        {"id", name},
        {"version", version},
        {"compatibleVersion", supportedVersion},
        {"author", author},
        {"dependencies", dependencies}
    };

    return true;
}

QStringList extractDependencies(const QJsonObject& metaData) {
    QStringList dependencies;
    const QJsonArray array = metaData.value("dependencies").toArray();
    for (const QJsonValue& value : array) {
        const QString dependency = value.toString().trimmed();
        if (!dependency.isEmpty()) {
            dependencies.append(dependency);
        }
    }
    return dependencies;
}

}