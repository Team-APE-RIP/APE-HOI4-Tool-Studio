#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QDebug>
#include <QSet>
#include <QTextStream>
#include <iostream>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

void logMessage(const QString& msg) {
    qDebug() << msg;
    std::cout << msg.toStdString() << std::endl;
}

#ifdef Q_OS_WIN
bool isProcessRunningByName(const QString& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    const QString targetName = processName.toLower();
    bool found = false;

    if (Process32First(snapshot, &entry)) {
        do {
            const QString exeName = QString::fromWCharArray(entry.szExeFile).toLower();
            if (exeName == targetName) {
                found = true;
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
}

bool waitForProcessNameToDisappear(const QString& processName, int timeoutMs, int pollIntervalMs = 100) {
    const DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs);
    while (GetTickCount() < deadline) {
        if (!isProcessRunningByName(processName)) {
            return true;
        }
        Sleep(static_cast<DWORD>(pollIntervalMs));
    }
    return !isProcessRunningByName(processName);
}

BOOL CALLBACK collectWindowsForProcessCallback(HWND hwnd, LPARAM lParam) {
    if (!IsWindow(hwnd)) {
        return TRUE;
    }

    if (GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0) {
        return TRUE;
    }

    auto* payload = reinterpret_cast<QPair<DWORD, QList<HWND>>*>(lParam);
    if (!payload) {
        return TRUE;
    }

    if (payload->first != processId) {
        return TRUE;
    }

    payload->second.append(hwnd);
    return TRUE;
}

QList<HWND> windowsForProcessId(DWORD processId) {
    QPair<DWORD, QList<HWND>> payload(processId, QList<HWND>());
    EnumWindows(collectWindowsForProcessCallback, reinterpret_cast<LPARAM>(&payload));
    return payload.second;
}

QList<DWORD> processIdsByName(const QString& processName) {
    QList<DWORD> result;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return result;
    }

    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    const QString targetName = processName.toLower();

    if (Process32First(snapshot, &entry)) {
        do {
            const QString exeName = QString::fromWCharArray(entry.szExeFile).toLower();
            if (exeName == targetName) {
                result.append(entry.th32ProcessID);
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return result;
}

bool requestProcessCloseByPid(DWORD processId) {
    const QList<HWND> windows = windowsForProcessId(processId);
    if (windows.isEmpty()) {
        return false;
    }

    bool posted = false;
    for (HWND hwnd : windows) {
        if (PostMessage(hwnd, WM_CLOSE, 0, 0)) {
            posted = true;
        }
    }

    return posted;
}

int requestCloseByProcessName(const QString& processName) {
    int requested = 0;
    const QList<DWORD> processIds = processIdsByName(processName);
    for (DWORD processId : processIds) {
        if (requestProcessCloseByPid(processId)) {
            ++requested;
        }
    }
    return requested;
}

bool waitForProcessExitByPid(qint64 pid, int gracefulWaitMs, int forceWaitMs) {
    if (pid <= 0) {
        return true;
    }

    HANDLE processHandle = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!processHandle) {
        return true;
    }

    const bool closeRequested = requestProcessCloseByPid(static_cast<DWORD>(pid));
    if (closeRequested) {
        logMessage(QString("Sent WM_CLOSE to process %1").arg(pid));
    } else {
        logMessage(QString("No top-level window found for process %1, falling back to wait/terminate").arg(pid));
    }

    DWORD waitResult = WaitForSingleObject(processHandle, static_cast<DWORD>(gracefulWaitMs));
    if (waitResult == WAIT_OBJECT_0) {
        CloseHandle(processHandle);
        return true;
    }

    logMessage(QString("Process %1 still running, forcing termination...").arg(pid));
    TerminateProcess(processHandle, 1);

    waitResult = WaitForSingleObject(processHandle, static_cast<DWORD>(forceWaitMs));
    CloseHandle(processHandle);

    return waitResult == WAIT_OBJECT_0;
}
#endif

bool removeFileWithRetry(const QString& filePath, int attempts = 30, int delayMs = 100) {
    if (!QFile::exists(filePath)) {
        return true;
    }

    for (int i = 0; i < attempts; ++i) {
        if (QFile::remove(filePath)) {
            return true;
        }

        QThread::msleep(static_cast<unsigned long>(delayMs));
    }

    return false;
}

bool copyFileWithRetry(const QString& sourcePath, const QString& destinationPath, int attempts = 30, int delayMs = 100) {
    for (int i = 0; i < attempts; ++i) {
        if (QFile::copy(sourcePath, destinationPath)) {
            return true;
        }

        QThread::msleep(static_cast<unsigned long>(delayMs));
    }

    return false;
}

// Recursively collect all file paths relative to baseDir
QStringList getAllLocalFiles(const QString& baseDir) {
    QStringList result;
    QDir dir(baseDir);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        if (info.isDir()) {
            QStringList sub = getAllLocalFiles(info.filePath());
            result.append(sub);
        } else {
            QString rel = QDir(baseDir).relativeFilePath(info.filePath());
            result.append(rel.replace('\\', '/'));
        }
    }
    return result;
}

// Extract the tool directory name from a path like "tools/SomeTool/..."
// Returns empty string if path is not under tools/
QString extractToolName(const QString& relativePath) {
    if (!relativePath.startsWith("tools/")) {
        return QString();
    }
    int secondSlash = relativePath.indexOf('/', 6); // after "tools/"
    if (secondSlash < 0) {
        // File directly in tools/ (not in a subdirectory) - treat as non-tool file
        return QString();
    }
    return relativePath.mid(0, secondSlash); // e.g. "tools/SomeTool"
}

// Extract the plugin directory name from a path like "plugins/SomePlugin/..."
// Returns empty string if path is not under plugins/
QString extractPluginName(const QString& relativePath) {
    if (!relativePath.startsWith("plugins/")) {
        return QString();
    }
    int secondSlash = relativePath.indexOf('/', 8); // after "plugins/"
    if (secondSlash < 0) {
        // File directly in plugins/ (not in a subdirectory) - treat as non-plugin file
        return QString();
    }
    return relativePath.mid(0, secondSlash); // e.g. "plugins/SomePlugin"
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QStringList args = app.arguments();
    if (args.size() < 3) {
        logMessage("Usage: updater.exe <target_dir> <temp_dir> [main_pid]");
        return 1;
    }

    QString targetDir = args[1];
    QString tempDir = args[2];
    qint64 mainPid = 0;
    if (args.size() >= 4) {
        bool ok = false;
        mainPid = args[3].toLongLong(&ok);
        if (!ok) {
            mainPid = 0;
        }
    }

    logMessage("Updater started.");
    logMessage("Target Dir: " + targetDir);
    logMessage("Temp Dir: " + tempDir);

    logMessage("Waiting for main application to exit...");

#ifdef Q_OS_WIN
    bool mainStopped = false;

    if (mainPid > 0) {
        logMessage(QString("Using PID-aware shutdown path for process %1").arg(mainPid));
        mainStopped = waitForProcessExitByPid(mainPid, 3000, 3000);
    } else {
        logMessage("No PID provided by launcher, entering compatibility mode for older versions.");
        const int closeRequests = requestCloseByProcessName("APEHOI4ToolStudio.exe");
        logMessage(QString("Sent WM_CLOSE to %1 matching top-level window(s).").arg(closeRequests));
        mainStopped = waitForProcessNameToDisappear("APEHOI4ToolStudio.exe", 5000);
    }

    if (!mainStopped) {
        logMessage("Graceful shutdown did not complete, running fallback force cleanup...");
        QProcess::execute("taskkill", QStringList() << "/F" << "/IM" << "APEHOI4ToolStudio.exe");
    }

    const bool allInstancesStopped = waitForProcessNameToDisappear("APEHOI4ToolStudio.exe", 5000);

    if (!allInstancesStopped) {
        logMessage("Error: APEHOI4ToolStudio.exe is still running, aborting update copy phase.");
        return 1;
    } else {
        logMessage("Confirmed that no old APEHOI4ToolStudio.exe processes remain.");
    }

    Sleep(800);
#else
    QThread::sleep(2);
#endif

    QDir tDir(tempDir);
    if (!tDir.exists()) {
        logMessage("Temp directory does not exist. Nothing to update.");
        return 1;
    }

    // Prepare target dir
    QDir target(targetDir);
    if (!target.exists()) {
        target.mkpath(".");
    }

    // --- Cleanup obsolete files (third-party tools protection) ---
    QString manifestListPath = QDir(tempDir).filePath("manifest_files.txt");
    if (QFile::exists(manifestListPath)) {
        logMessage("Reading manifest file list for cleanup...");

        // Read manifest file list
        QSet<QString> manifestFiles;
        QSet<QString> officialToolDirs; // e.g. "tools/LogManagerTool"
        QSet<QString> officialPluginDirs; // e.g. "plugins/DirectXTex"

        QFile manifestFile(manifestListPath);
        if (manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&manifestFile);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (!line.isEmpty()) {
                    manifestFiles.insert(line);
                    // Track official tool directories
                    QString toolName = extractToolName(line);
                    if (!toolName.isEmpty()) {
                        officialToolDirs.insert(toolName);
                    }

                    // Track official plugin directories
                    QString pluginName = extractPluginName(line);
                    if (!pluginName.isEmpty()) {
                        officialPluginDirs.insert(pluginName);
                    }
                }
            }
            manifestFile.close();
        }

        logMessage(QString("Manifest contains %1 files, %2 official tool directories, %3 official plugin directories.")
            .arg(manifestFiles.size())
            .arg(officialToolDirs.size())
            .arg(officialPluginDirs.size()));

        // Files/dirs that should never be deleted
        QSet<QString> protectedFiles;
        protectedFiles.insert("Updater.exe");
        protectedFiles.insert("config.json");

        // Scan local directory and remove obsolete files
        QStringList localFiles = getAllLocalFiles(targetDir);
        int removedCount = 0;

        for (const QString& localFile : localFiles) {
            // Skip protected files
            if (protectedFiles.contains(localFile)) {
                continue;
            }

            // Skip if file is in the manifest (it's a current official file)
            if (manifestFiles.contains(localFile)) {
                continue;
            }

            // Check if this file is under tools/ directory
            QString toolName = extractToolName(localFile);
            if (!toolName.isEmpty()) {
                // This file is inside a tool directory
                if (!officialToolDirs.contains(toolName)) {
                    // This tool directory is NOT in the manifest at all
                    // -> it's a third-party tool, skip it
                    continue;
                }
                // This tool IS an official tool but this specific file
                // is no longer in the manifest -> delete it
            }

            // Check if this file is under plugins/ directory
            QString pluginName = extractPluginName(localFile);
            if (!pluginName.isEmpty()) {
                // This file is inside a plugin directory
                if (!officialPluginDirs.contains(pluginName)) {
                    // This plugin directory is NOT in the manifest at all
                    // -> it's a third-party plugin, skip it
                    continue;
                }
                // This plugin IS an official plugin but this specific file
                // is no longer in the manifest -> delete it
            }

            // Delete the obsolete file
            QString fullPath = QDir(targetDir).filePath(localFile);
            if (QFile::remove(fullPath)) {
                logMessage("Removed obsolete file: " + localFile);
                removedCount++;
            } else {
                logMessage("Failed to remove obsolete file: " + localFile);
            }
        }

        // Clean up empty directories (but not tool directories that belong to third-party tools)
        // We do a second pass to remove empty dirs
        std::function<void(const QString&, const QString&)> removeEmptyDirs = [&](const QString& dirPath, const QString& basePath) {
            QDir dir(dirPath);
            QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo& entry : entries) {
                removeEmptyDirs(entry.filePath(), basePath);
            }

            // Don't remove the root target directory itself
            if (dirPath == basePath) return;

            QString relDir = QDir(basePath).relativeFilePath(dirPath).replace('\\', '/');

            // Protect third-party tool directories
            if (relDir.startsWith("tools/")) {
                QString toolName = relDir;
                int slash = relDir.indexOf('/', 6);
                if (slash > 0) {
                    toolName = relDir.mid(0, slash);
                }
                if (!officialToolDirs.contains(toolName)) {
                    // Third-party tool directory, don't remove even if empty
                    return;
                }
            }

            // Protect third-party plugin directories
            if (relDir.startsWith("plugins/")) {
                QString pluginName = relDir;
                int slash = relDir.indexOf('/', 8);
                if (slash > 0) {
                    pluginName = relDir.mid(0, slash);
                }
                if (!officialPluginDirs.contains(pluginName)) {
                    // Third-party plugin directory, don't remove even if empty
                    return;
                }
            }

            // Remove if empty
            if (dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                if (QDir().rmdir(dirPath)) {
                    logMessage("Removed empty directory: " + relDir);
                }
            }
        };

        removeEmptyDirs(targetDir, targetDir);

        logMessage(QString("Cleanup complete. Removed %1 obsolete files.").arg(removedCount));
    } else {
        logMessage("No manifest_files.txt found, skipping cleanup (legacy update mode).");
    }

    logMessage("Starting file copy...");

    // Recursive copy function
    std::function<bool(const QString&, const QString&)> copyRecursively = [&](const QString& srcPath, const QString& dstPath) -> bool {
        QDir srcDir(srcPath);
        if (!srcDir.exists()) return false;

        QDir dstDir(dstPath);
        if (!dstDir.exists()) {
            dstDir.mkpath(".");
        }

        bool success = true;
        QFileInfoList fileInfoList = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fileInfo : fileInfoList) {
            QString srcFilePath = fileInfo.filePath();
            QString dstFilePath = dstDir.filePath(fileInfo.fileName());

            if (fileInfo.isDir()) {
                success = copyRecursively(srcFilePath, dstFilePath) && success;
            } else {
                // Skip manifest_files.txt and Updater.exe
                if (fileInfo.fileName() == "manifest_files.txt" || fileInfo.fileName() == "Updater.exe") {
                    continue;
                }
                if (QFile::exists(dstFilePath)) {
                    if (!removeFileWithRetry(dstFilePath)) {
                        logMessage("Failed to remove existing file after retries: " + dstFilePath);
                        success = false;
                        continue;
                    }
                }
                if (!copyFileWithRetry(srcFilePath, dstFilePath)) {
                    logMessage("Failed to copy file after retries: " + srcFilePath + " to " + dstFilePath);
                    success = false;
                } else {
                    logMessage("Copied: " + fileInfo.fileName());
                }
            }
        }
        return success;
    };

    if (copyRecursively(tempDir, targetDir)) {
        logMessage("File copy completed successfully.");

        // Restart main application
        QString mainAppPath = QDir(targetDir).filePath("APEHOI4ToolStudio.exe");
        if (QFile::exists(mainAppPath)) {
            logMessage("Restarting main application...");
            const bool restarted = QProcess::startDetached(mainAppPath, QStringList());
            if (!restarted) {
                logMessage("Failed to restart main application.");
                return 1;
            }
        } else {
            logMessage("Main application not found at: " + mainAppPath);
        }
    } else {
        logMessage("Update failed during file copy.");
        return 1;
    }

    return 0;
}