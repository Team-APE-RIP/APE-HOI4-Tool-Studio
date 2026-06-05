//-------------------------------------------------------------------------------------
// PackageRegistry.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef PACKAGEREGISTRY_H
#define PACKAGEREGISTRY_H

#include <QList>
#include <QString>

enum class PackageKind {
    Tool,   // PackageKind::Tool registry entries live under Tools.
    Plugin  // PackageKind::Plugin registry entries live under Plugins.
};

struct RegisteredPackage {
    QString id;
    QString name;
    QString version;
    QString directoryPath;
    QString descriptorPath;
    bool official = false;

    bool isValid() const {
        return !id.trimmed().isEmpty()
            && !name.trimmed().isEmpty()
            && !directoryPath.trimmed().isEmpty()
            && !descriptorPath.trimmed().isEmpty();
    }
};

class PackageRegistry {
public:
    static constexpr const char* RegistryRoot =
        "HKEY_CURRENT_USER\\Software\\Team-APE-RIP\\APE-HOI4-Tool-Studio";

    static bool synchronizeInstalledPackages(const QString& applicationDirPath,
                                             QString* errorMessage = nullptr);
    static QList<RegisteredPackage> registeredPackages(PackageKind kind);
    static RegisteredPackage registeredPackage(PackageKind kind, const QString& id);
    static bool registerPackage(PackageKind kind,
                                const RegisteredPackage& package,
                                QString* errorMessage = nullptr);
    static bool unregisterPackage(PackageKind kind,
                                  const QString& id,
                                  QString* errorMessage = nullptr);
    static bool removeInstalledPackage(PackageKind kind,
                                       const QString& id,
                                       QString* errorMessage = nullptr);
    static bool isPackageOfficial(PackageKind kind, const QString& id);
    static bool cleanUnregisteredPackageDirectories(PackageKind kind,
                                                   const QString& applicationDirPath,
                                                   QString* errorMessage = nullptr);

    static QString packageRootPath(PackageKind kind, const QString& applicationDirPath);
    static QString descriptorFileName(PackageKind kind);
    static bool isValidPackageId(PackageKind kind, const QString& id);
};

#endif // PACKAGEREGISTRY_H
