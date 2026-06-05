//-------------------------------------------------------------------------------------
// GamePathDiscovery.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include "GamePathDiscovery.h"
#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QWaitCondition>

#include <atomic>
#include <thread>
#include <vector>

namespace {
const char* kSteamPathRegistryValue = "HKCU\\Software\\Valve\\Steam\\SteamPath";
const char* kSteamInstallPathRegistryValue = "HKLM\\SOFTWARE\\WOW6432Node\\Valve\\Steam\\InstallPath";

QString cleanExistingPath(const QString& path) {
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    if (normalized.isEmpty()) {
        return QString();
    }

    return QDir::cleanPath(normalized);
}

QString unescapeVdfString(QString value) {
    value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return cleanExistingPath(value);
}

void addUniquePath(QStringList& paths, QSet<QString>& seen, const QString& path) {
    const QString cleaned = cleanExistingPath(path);
    if (cleaned.isEmpty()) {
        return;
    }

    const QString key = cleaned.toCaseFolded();
    if (seen.contains(key)) {
        return;
    }

    seen.insert(key);
    paths.append(cleaned);
}

QString readRegistryPath(const QString& valuePath) {
#ifdef Q_OS_WIN
    const int valueSeparator = valuePath.lastIndexOf(QChar('\\'));
    if (valueSeparator <= 0 || valueSeparator + 1 >= valuePath.size()) {
        return QString();
    }

    QSettings settings(valuePath.left(valueSeparator), QSettings::NativeFormat);
    return settings.value(valuePath.mid(valueSeparator + 1)).toString().trimmed();
#else
    Q_UNUSED(valuePath);
    return QString();
#endif
}

QString readTextFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    return QString::fromUtf8(file.readAll());
}

QString parseVdfValue(const QString& content, const QString& key) {
    const QRegularExpression pattern(
        QStringLiteral("\"%1\"\\s+\"([^\"]*)\"").arg(QRegularExpression::escape(key)));
    const QRegularExpressionMatch match = pattern.match(content);
    if (!match.hasMatch()) {
        return QString();
    }

    return unescapeVdfString(match.captured(1));
}

QStringList steamRootCandidates() {
    QStringList roots;
    QSet<QString> seen;

    addUniquePath(roots, seen, readRegistryPath(QString::fromLatin1(kSteamPathRegistryValue)));
    addUniquePath(roots, seen, readRegistryPath(QString::fromLatin1(kSteamInstallPathRegistryValue)));
    addUniquePath(roots, seen, QStringLiteral("C:/Program Files (x86)/Steam"));

    return roots;
}

QStringList steamLibraryFolders(const QString& steamRoot) {
    QStringList libraries;
    QSet<QString> seen;
    addUniquePath(libraries, seen, steamRoot);

    const QString libraryFile = QDir(steamRoot).filePath(QStringLiteral("steamapps/libraryfolders.vdf"));
    const QString content = readTextFile(libraryFile);
    if (content.isEmpty()) {
        return libraries;
    }

    const QRegularExpression pathPattern(QStringLiteral("\"path\"\\s+\"([^\"]+)\""));
    QRegularExpressionMatchIterator pathMatches = pathPattern.globalMatch(content);
    while (pathMatches.hasNext()) {
        addUniquePath(libraries, seen, unescapeVdfString(pathMatches.next().captured(1)));
    }

    const QRegularExpression oldLibraryPattern(QStringLiteral("\"\\d+\"\\s+\"([^\"]*[\\\\/][^\"]*)\""));
    QRegularExpressionMatchIterator oldMatches = oldLibraryPattern.globalMatch(content);
    while (oldMatches.hasNext()) {
        addUniquePath(libraries, seen, unescapeVdfString(oldMatches.next().captured(1)));
    }

    return libraries;
}

QString findFromSteam() {
    for (const QString& steamRoot : steamRootCandidates()) {
        if (!QDir(steamRoot).exists()) {
            continue;
        }

        for (const QString& library : steamLibraryFolders(steamRoot)) {
            const QString manifestPath = QDir(library).filePath(QStringLiteral("steamapps/appmanifest_394360.acf"));
            const QString manifest = readTextFile(manifestPath);
            if (manifest.isEmpty()) {
                continue;
            }

            const QString installDir = parseVdfValue(manifest, QStringLiteral("installdir"));
            if (installDir.isEmpty()) {
                continue;
            }

            const QString candidate = QDir(QDir(library).filePath(QStringLiteral("steamapps/common"))).filePath(installDir);
            if (GamePathDiscovery::isValidGamePath(candidate)) {
                Logger::instance().logInfo("GamePathDiscovery", "Found HOI4 through Steam manifest: " + candidate);
                return QDir::cleanPath(candidate);
            }
        }
    }

    return QString();
}

QStringList scanRoots() {
    QStringList roots;
    QSet<QString> seen;
    const QFileInfoList drives = QDir::drives();
    for (const QFileInfo& drive : drives) {
        addUniquePath(roots, seen, drive.absoluteFilePath());
    }

    return roots;
}

int scanThreadCount() {
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    const int detectedThreads = hardwareThreads == 0 ? 8 : static_cast<int>(hardwareThreads);
    return qBound(4, detectedThreads * 2, 64);
}

QString findByFolderScan() {
    QStringList pending = scanRoots();
    if (pending.isEmpty()) {
        return QString();
    }

    QMutex mutex;
    QWaitCondition pendingChanged;
    std::atomic_bool shouldStop(false);
    QString result;
    bool exhausted = false;
    int activeWorkers = 0;

    auto worker = [&]() {
        while (true) {
            QString currentPath;
            {
                QMutexLocker locker(&mutex);
                while (!shouldStop.load(std::memory_order_relaxed) && pending.isEmpty() && !exhausted) {
                    if (activeWorkers == 0) {
                        exhausted = true;
                        pendingChanged.wakeAll();
                        break;
                    }
                    pendingChanged.wait(&mutex);
                }

                if (shouldStop.load(std::memory_order_relaxed) || exhausted) {
                    return;
                }

                currentPath = pending.takeLast();
                ++activeWorkers;
            }

            QString foundPath;
            QStringList childDirs;
            const QFileInfoList entries = QDir(currentPath).entryInfoList(
                QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System | QDir::NoSymLinks,
                QDir::Name);

            for (const QFileInfo& entry : entries) {
                if (shouldStop.load(std::memory_order_relaxed)) {
                    break;
                }

                const QString childPath = QDir::cleanPath(entry.absoluteFilePath());
                if (entry.fileName() == QStringLiteral("Hearts of Iron IV")) {
                    if (GamePathDiscovery::isValidGamePath(childPath)) {
                        foundPath = childPath;
                        break;
                    }
                    continue;
                }

                if (entry.isDir()) {
                    childDirs.append(childPath);
                }
            }

            {
                QMutexLocker locker(&mutex);
                --activeWorkers;

                if (!foundPath.isEmpty()) {
                    result = foundPath;
                    shouldStop.store(true, std::memory_order_relaxed);
                    pendingChanged.wakeAll();
                    return;
                }

                if (!childDirs.isEmpty() && !shouldStop.load(std::memory_order_relaxed)) {
                    pending.append(childDirs);
                    pendingChanged.wakeAll();
                }

                if (pending.isEmpty() && activeWorkers == 0) {
                    exhausted = true;
                    pendingChanged.wakeAll();
                }
            }
        }
    };

    const int workerCount = scanThreadCount();
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(workerCount));
    for (int i = 0; i < workerCount; ++i) {
        workers.emplace_back(worker);
    }

    for (std::thread& thread : workers) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    if (!result.isEmpty()) {
        Logger::instance().logInfo("GamePathDiscovery", "Found HOI4 through folder scan: " + result);
    }
    return result;
}
}

QStringList GamePathDiscovery::requiredEntries() {
    return {
        QStringLiteral("hoi4.exe"),
        QStringLiteral("common"),
        QStringLiteral("history"),
        QStringLiteral("events"),
        QStringLiteral("localisation"),
        QStringLiteral("map")
    };
}

bool GamePathDiscovery::isValidGamePath(const QString& path) {
    const QString cleaned = cleanExistingPath(path);
    if (cleaned.isEmpty()) {
        return false;
    }

    QDir dir(cleaned);
    if (!dir.exists()) {
        return false;
    }

    if (!QFileInfo(dir.filePath(QStringLiteral("hoi4.exe"))).isFile()) {
        return false;
    }

    for (const QString& relativeDir : {
             QStringLiteral("common"),
             QStringLiteral("history"),
             QStringLiteral("events"),
             QStringLiteral("localisation"),
             QStringLiteral("map")
         }) {
        if (!QFileInfo(dir.filePath(relativeDir)).isDir()) {
            return false;
        }
    }

    return true;
}

QString GamePathDiscovery::findGamePath() {
    const QString steamPath = findFromSteam();
    if (!steamPath.isEmpty()) {
        return steamPath;
    }

    Logger::instance().logInfo("GamePathDiscovery", "Steam discovery did not find HOI4, starting folder scan.");
    const QString scannedPath = findByFolderScan();
    if (scannedPath.isEmpty()) {
        Logger::instance().logWarning("GamePathDiscovery", "HOI4 game directory was not found.");
    }
    return scannedPath;
}
