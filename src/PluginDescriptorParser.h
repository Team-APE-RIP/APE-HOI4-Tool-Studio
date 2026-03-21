#ifndef PLUGINDESCRIPTORPARSER_H
#define PLUGINDESCRIPTORPARSER_H

#include <QString>
#include <QStringList>

struct PluginInfo {
    QString id;
    QString name;
    QString version;
    QString compatibleVersion;
    QString author;
    QString directoryPath;
    QString libraryPath;
    QString descriptorPath;
    QString licensePath;

    bool isValid() const {
        return !id.isEmpty() && !name.isEmpty() && !directoryPath.isEmpty() && !descriptorPath.isEmpty();
    }
};

namespace PluginDescriptorParser {
bool parseDescriptorFile(const QString& filePath, PluginInfo& outInfo, QString* errorMessage = nullptr);
}

#endif // PLUGINDESCRIPTORPARSER_H