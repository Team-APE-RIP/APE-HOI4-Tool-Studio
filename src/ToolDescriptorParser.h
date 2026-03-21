#ifndef TOOLDESCRIPTORPARSER_H
#define TOOLDESCRIPTORPARSER_H

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace ToolDescriptorParser {
bool parseDescriptorFile(const QString& filePath, QJsonObject& outMetaData, QString* errorMessage = nullptr);
QStringList extractDependencies(const QJsonObject& metaData);
}

#endif // TOOLDESCRIPTORPARSER_H