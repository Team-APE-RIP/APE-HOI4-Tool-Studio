#include "PluginDescriptorParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace {
bool parseLine(const QString& line, QString& key, QString& value) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith("#") || trimmed.startsWith("//")) {
        return false;
    }

    static const QRegularExpression pattern("^([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*\"(.*)\"\\s*$");
    const QRegularExpressionMatch match = pattern.match(trimmed);
    if (!match.hasMatch()) {
        return false;
    }

    key = match.captured(1).trimmed();
    value = match.captured(2).trimmed();
    return !key.isEmpty();
}
}

namespace PluginDescriptorParser {

bool parseDescriptorFile(const QString& filePath, PluginInfo& outInfo, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QString("Failed to open plugin descriptor file: %1").arg(filePath);
        }
        return false;
    }

    QString id;
    QString name;
    QString version;
    QString supportedVersion;
    QString author;

    const QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
    file.close();

    for (QString line : lines) {
        line.replace("\r", "");

        QString key;
        QString value;
        if (!parseLine(line, key, value)) {
            continue;
        }

        if (key == "id") {
            id = value;
        } else if (key == "name") {
            name = value;
        } else if (key == "version") {
            version = value;
        } else if (key == "supported_version") {
            supportedVersion = value;
        } else if (key == "author") {
            author = value;
        }
    }

    if (id.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("Plugin descriptor missing id: %1").arg(filePath);
        }
        return false;
    }

    if (name.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QString("Plugin descriptor missing name: %1").arg(filePath);
        }
        return false;
    }

    const QFileInfo descriptorInfo(filePath);
    const QString directoryPath = descriptorInfo.dir().absolutePath();
    const QString licensePath = descriptorInfo.dir().filePath("LICENSE");

    outInfo.id = id;
    outInfo.name = name;
    outInfo.version = version;
    outInfo.compatibleVersion = supportedVersion;
    outInfo.author = author;
    outInfo.directoryPath = directoryPath;
    outInfo.descriptorPath = descriptorInfo.absoluteFilePath();
    outInfo.licensePath = QFile::exists(licensePath) ? licensePath : QString();

    return true;
}

}