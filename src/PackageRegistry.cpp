//-------------------------------------------------------------------------------------
// PackageRegistry.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "PackageRegistry.h"

#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStringList>

#include <algorithm>

namespace {
QString kindGroupName(PackageKind kind) {
    return kind == PackageKind::Tool ? QStringLiteral("Tools") : QStringLiteral("Plugins");
}

QString kindLogName(PackageKind kind) {
    return kind == PackageKind::Tool ? QStringLiteral("tool") : QStringLiteral("plugin");
}

QSettings registrySettings() {
    return QSettings(QString::fromLatin1(PackageRegistry::RegistryRoot), QSettings::NativeFormat);
}

QString normalizeId(const QString& id) {
    return id.trimmed();
}

QString normalizePath(const QString& path) {
    if (path.trimmed().isEmpty()) {
        return QString();
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString descriptorValue(const QString& descriptorPath, const QString& keyName) {
    QFile file(descriptorPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    static const QRegularExpression pattern(QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*\"(.*)\"\\s*$"));
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    for (QString line : lines) {
        line.replace(QStringLiteral("\r"), QString());
        const QRegularExpressionMatch match = pattern.match(line.trimmed());
        if (match.hasMatch() && match.captured(1) == keyName) {
            return match.captured(2).trimmed();
        }
    }
    return QString();
}

RegisteredPackage readPackageFromSettings(PackageKind kind, const QString& id) {
    RegisteredPackage package;
    const QString normalizedId = normalizeId(id);
    if (!PackageRegistry::isValidPackageId(kind, normalizedId)) {
        return package;
    }

    QSettings settings = registrySettings();
    settings.beginGroup(kindGroupName(kind));
    settings.beginGroup(normalizedId);
    package.id = settings.value(QStringLiteral("id"), normalizedId).toString().trimmed();
    package.name = settings.value(QStringLiteral("name")).toString().trimmed();
    package.version = settings.value(QStringLiteral("version")).toString().trimmed();
    package.directoryPath = normalizePath(settings.value(QStringLiteral("directoryPath")).toString().trimmed());
    package.descriptorPath = normalizePath(settings.value(QStringLiteral("descriptorPath")).toString().trimmed());
    package.official = settings.value(QStringLiteral("official"), false).toBool();
    settings.endGroup();
    settings.endGroup();
    return package;
}

bool removeRegisteredPackageDirectory(PackageKind kind,
                                      const RegisteredPackage& package,
                                      QString* errorMessage) {
    const QFileInfo directoryInfo(package.directoryPath);
    if (!directoryInfo.exists()) {
        return true;
    }

    QDir directory(package.directoryPath);
    if (!directory.exists()) {
        return true;
    }

    const QFileInfo rootInfo(directoryInfo.dir().absolutePath());
    const QString rootPath = normalizePath(rootInfo.absoluteFilePath());
    const QString directoryPath = normalizePath(directoryInfo.absoluteFilePath());
    if (directoryInfo.fileName() != package.id
        || !directoryPath.startsWith(rootPath + QLatin1Char('/'), Qt::CaseInsensitive)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Refusing to remove package directory outside its package root: %1")
                                .arg(QDir::toNativeSeparators(directoryPath));
        }
        return false;
    }

    if (directory.removeRecursively()) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("Failed to remove package directory: %1")
                            .arg(QDir::toNativeSeparators(directoryPath));
    }
    return false;
}

QList<RegisteredPackage> officialPackagesFromResource(PackageKind requestedKind,
                                                       const QString& applicationDirPath) {
    QList<RegisteredPackage> packages;
    QFile file(QStringLiteral(":/package_registry.proto"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Logger::instance().logWarning("PackageRegistry", "Official package registry proto is missing.");
        return packages;
    }

    const QString rootPath = PackageRegistry::packageRootPath(requestedKind, applicationDirPath);
    const QString descriptorName = PackageRegistry::descriptorFileName(requestedKind);
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    const QRegularExpression entryPattern(QStringLiteral("^\\s*(tool|plugin)\\s*:\\s*\"([0-9]{8})\\|([^\"]+)\"\\s*$"));

    for (const QString& line : lines) {
        const QRegularExpressionMatch match = entryPattern.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        const PackageKind kind = match.captured(1) == QStringLiteral("tool")
            ? PackageKind::Tool
            : PackageKind::Plugin;
        if (kind != requestedKind) {
            continue;
        }

        RegisteredPackage package;
        package.id = match.captured(2).trimmed();
        package.name = match.captured(3).trimmed();
        package.official = true;
        package.directoryPath = normalizePath(QDir(rootPath).filePath(package.id));
        package.descriptorPath = normalizePath(QDir(package.directoryPath).filePath(descriptorName));
        package.version = descriptorValue(package.descriptorPath, QStringLiteral("version"));

        if (PackageRegistry::isValidPackageId(kind, package.id) && !package.name.isEmpty()) {
            packages.append(package);
        }
    }

    return packages;
}

bool synchronizeOfficialPackages(PackageKind kind,
                                 const QString& applicationDirPath,
                                 QString* errorMessage) {
    const QList<RegisteredPackage> packages = officialPackagesFromResource(kind, applicationDirPath);
    for (const RegisteredPackage& package : packages) {
        if (!PackageRegistry::registerPackage(kind, package, errorMessage)) {
            return false;
        }
    }
    return true;
}

bool pruneRegistryEntries(PackageKind kind, QString* errorMessage) {
    QSettings settings = registrySettings();
    settings.beginGroup(kindGroupName(kind));
    const QStringList ids = settings.childGroups();
    settings.endGroup();

    for (const QString& id : ids) {
        const RegisteredPackage package = readPackageFromSettings(kind, id);
        if (!PackageRegistry::isValidPackageId(kind, id)
            || !package.isValid()
            || (!package.official && !QFile::exists(package.descriptorPath))) {
            if (!PackageRegistry::unregisterPackage(kind, id, errorMessage)) {
                return false;
            }
        }
    }
    return true;
}
}

bool PackageRegistry::synchronizeInstalledPackages(const QString& applicationDirPath,
                                                   QString* errorMessage) {
    QDir().mkpath(packageRootPath(PackageKind::Tool, applicationDirPath));
    QDir().mkpath(packageRootPath(PackageKind::Plugin, applicationDirPath));

    if (!synchronizeOfficialPackages(PackageKind::Tool, applicationDirPath, errorMessage)
        || !synchronizeOfficialPackages(PackageKind::Plugin, applicationDirPath, errorMessage)
        || !pruneRegistryEntries(PackageKind::Tool, errorMessage)
        || !pruneRegistryEntries(PackageKind::Plugin, errorMessage)
        || !cleanUnregisteredPackageDirectories(PackageKind::Tool, applicationDirPath, errorMessage)
        || !cleanUnregisteredPackageDirectories(PackageKind::Plugin, applicationDirPath, errorMessage)) {
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QList<RegisteredPackage> PackageRegistry::registeredPackages(PackageKind kind) {
    QList<RegisteredPackage> packages;
    QSettings settings = registrySettings();
    settings.beginGroup(kindGroupName(kind));
    const QStringList ids = settings.childGroups();
    settings.endGroup();

    for (const QString& id : ids) {
        const RegisteredPackage package = readPackageFromSettings(kind, id);
        if (package.isValid() && isValidPackageId(kind, package.id)) {
            packages.append(package);
        }
    }

    std::sort(packages.begin(), packages.end(), [](const RegisteredPackage& left, const RegisteredPackage& right) {
        bool leftOk = false;
        bool rightOk = false;
        const int leftId = left.id.toInt(&leftOk);
        const int rightId = right.id.toInt(&rightOk);
        if (leftOk && rightOk) {
            return leftId < rightId;
        }
        return left.id < right.id;
    });

    return packages;
}

RegisteredPackage PackageRegistry::registeredPackage(PackageKind kind, const QString& id) {
    return readPackageFromSettings(kind, id);
}

bool PackageRegistry::registerPackage(PackageKind kind,
                                      const RegisteredPackage& package,
                                      QString* errorMessage) {
    RegisteredPackage normalized = package;
    normalized.id = normalizeId(normalized.id);
    normalized.name = normalized.name.trimmed();
    normalized.version = normalized.version.trimmed();
    normalized.directoryPath = normalizePath(normalized.directoryPath);
    normalized.descriptorPath = normalizePath(normalized.descriptorPath);

    if (!isValidPackageId(kind, normalized.id) || normalized.name.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid %1 registry entry: %2")
                                .arg(kindLogName(kind), normalized.id);
        }
        return false;
    }

    QSettings settings = registrySettings();
    settings.beginGroup(kindGroupName(kind));
    settings.beginGroup(normalized.id);
    settings.setValue(QStringLiteral("id"), normalized.id);
    settings.setValue(QStringLiteral("name"), normalized.name);
    settings.setValue(QStringLiteral("version"), normalized.version);
    settings.setValue(QStringLiteral("directoryPath"), normalized.directoryPath);
    settings.setValue(QStringLiteral("descriptorPath"), normalized.descriptorPath);
    settings.setValue(QStringLiteral("official"), normalized.official);
    settings.endGroup();
    settings.endGroup();
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write %1 registry entry: %2")
                                .arg(kindLogName(kind), normalized.id);
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool PackageRegistry::unregisterPackage(PackageKind kind,
                                        const QString& id,
                                        QString* errorMessage) {
    const QString normalizedId = normalizeId(id);
    QSettings settings = registrySettings();
    settings.beginGroup(kindGroupName(kind));
    settings.remove(normalizedId);
    settings.endGroup();
    settings.sync();

    if (settings.status() != QSettings::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to remove %1 registry entry: %2")
                                .arg(kindLogName(kind), normalizedId);
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool PackageRegistry::removeInstalledPackage(PackageKind kind,
                                             const QString& id,
                                             QString* errorMessage) {
    const RegisteredPackage package = registeredPackage(kind, id);
    if (!package.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Package is not registered: %1").arg(id);
        }
        return false;
    }

    if (package.official) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Official packages cannot be removed: %1").arg(package.name);
        }
        return false;
    }

    if (!removeRegisteredPackageDirectory(kind, package, errorMessage)) {
        return false;
    }

    return unregisterPackage(kind, package.id, errorMessage);
}

bool PackageRegistry::isPackageOfficial(PackageKind kind, const QString& id) {
    return registeredPackage(kind, id).official;
}

bool PackageRegistry::cleanUnregisteredPackageDirectories(PackageKind kind,
                                                          const QString& applicationDirPath,
                                                          QString* errorMessage) {
    const QString rootPath = packageRootPath(kind, applicationDirPath);
    QDir root(rootPath);
    if (!root.exists()) {
        return true;
    }

    QSet<QString> registeredIds;
    for (const RegisteredPackage& package : registeredPackages(kind)) {
        registeredIds.insert(package.id);
    }

    const QFileInfo rootInfo(root.absolutePath());
    const QString normalizedRootPath = normalizePath(rootInfo.absoluteFilePath());
    const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        const QString folderName = entry.fileName();
        const QString directoryPath = normalizePath(entry.absoluteFilePath());
        const bool shouldRemove = !isValidPackageId(kind, folderName) || !registeredIds.contains(folderName);
        if (!shouldRemove) {
            continue;
        }

        if (!directoryPath.startsWith(normalizedRootPath + QLatin1Char('/'), Qt::CaseInsensitive)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Refusing to clean package directory outside root: %1")
                                    .arg(QDir::toNativeSeparators(directoryPath));
            }
            return false;
        }

        QDir directory(directoryPath);
        if (!directory.removeRecursively()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Failed to clean unregistered %1 directory: %2")
                                    .arg(kindLogName(kind), QDir::toNativeSeparators(directoryPath));
            }
            return false;
        }

        Logger::instance().logInfo(
            "PackageRegistry",
            QString("Removed unregistered %1 directory: %2")
                .arg(kindLogName(kind), QDir::toNativeSeparators(directoryPath))
        );
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QString PackageRegistry::packageRootPath(PackageKind kind, const QString& applicationDirPath) {
    const QString child = kind == PackageKind::Tool ? QStringLiteral("tools") : QStringLiteral("plugins");
    return normalizePath(QDir(applicationDirPath).filePath(child));
}

QString PackageRegistry::descriptorFileName(PackageKind kind) {
    return kind == PackageKind::Tool
        ? QStringLiteral("descriptor.apehts")
        : QStringLiteral("descriptor.htsplugin");
}

bool PackageRegistry::isValidPackageId(PackageKind kind, const QString& id) {
    const QString normalizedId = normalizeId(id);
    static const QRegularExpression toolPattern(QStringLiteral("^1[0-9]{7}$"));
    static const QRegularExpression pluginPattern(QStringLiteral("^[0-9]{8}$"));
    return (kind == PackageKind::Tool ? toolPattern : pluginPattern).match(normalizedId).hasMatch();
}
