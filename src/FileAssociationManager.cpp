//-------------------------------------------------------------------------------------
// FileAssociationManager.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "FileAssociationManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

bool FileAssociationManager::registerFileAssociations(QString* errorMessage) {
#ifndef Q_OS_WIN
    Q_UNUSED(errorMessage);
    return true;
#else
    const QString executablePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

    if (!registerSingleAssociation(".apehts",
                                   "APEHOI4ToolStudio.apehts",
                                   "APE HOI4 Tool",
                                   executablePath,
                                   errorMessage)) {
        return false;
    }

    if (!registerSingleAssociation(".htsplugin",
                                   "APEHOI4ToolStudio.htsplugin",
                                   "APE HOI4 Tool Studio Plugin",
                                   executablePath,
                                   errorMessage)) {
        return false;
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
#endif
}

bool FileAssociationManager::registerSingleAssociation(const QString& extension,
                                                       const QString& progId,
                                                       const QString& fileTypeName,
                                                       const QString& executablePath,
                                                       QString* errorMessage) {
#ifndef Q_OS_WIN
    Q_UNUSED(extension);
    Q_UNUSED(progId);
    Q_UNUSED(fileTypeName);
    Q_UNUSED(executablePath);
    Q_UNUSED(errorMessage);
    return true;
#else
    const QString classesRoot = "HKEY_CURRENT_USER\\Software\\Classes\\";
    const QString iconValue = executablePath + ",0";
    const QString commandValue = QString("\"%1\" \"%2\"").arg(executablePath, "%1");

    {
        QSettings extensionSettings(classesRoot + extension, QSettings::NativeFormat);
        extensionSettings.setValue(".", progId);
    }

    {
        QSettings progIdSettings(classesRoot + progId, QSettings::NativeFormat);
        progIdSettings.setValue(".", fileTypeName);
    }

    {
        QSettings iconSettings(classesRoot + progId + "\\DefaultIcon", QSettings::NativeFormat);
        iconSettings.setValue(".", iconValue);
    }

    {
        QSettings commandSettings(classesRoot + progId + "\\shell\\open\\command", QSettings::NativeFormat);
        commandSettings.setValue(".", commandValue);
    }

    {
        QSettings openWithProgIdsSettings(classesRoot + extension + "\\OpenWithProgids", QSettings::NativeFormat);
        openWithProgIdsSettings.setValue(progId, QString());
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
#endif
}