//-------------------------------------------------------------------------------------
// main.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include <QCoreApplication>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QThread>
#include <QDebug>
#include <QSet>
#include <QTextStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <algorithm>
#include <iostream>
#include <functional>
#include <string>
#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace {
QString g_logFilePath;
#ifdef Q_OS_WIN
volatile LONG g_restartLaunchInProgress = 0;
volatile LONG g_restartExceptionLogCount = 0;
volatile LONG g_showRestartFailureDialogOnUnhandledException = 0;
#endif
}

void logMessage(const QString& msg) {
    const QString line = QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")), msg);
    qDebug() << line;
    std::cout << line.toStdString() << std::endl;

    if (!g_logFilePath.isEmpty()) {
        QFile logFile(g_logFilePath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&logFile);
            stream.setEncoding(QStringConverter::Utf8);
            stream << line << "\n";
        }
    }
}

void initializeUpdaterLogFile() {
    const QString logDir = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("APE-HOI4-Tool-Studio/updater_logs"));
    QDir().mkpath(logDir);

    g_logFilePath = QDir(logDir).filePath(QStringLiteral("updater_%1_%2.txt")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")))
        .arg(QCoreApplication::applicationPid()));

    logMessage("Updater log file: " + g_logFilePath);
}

#ifdef Q_OS_WIN
QString hexValue(quintptr value) {
    return QStringLiteral("0x") + QString::number(value, 16);
}

QString moduleInfoForAddress(quintptr address) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) {
        return QStringLiteral("module=<snapshot_failed error=%1>").arg(GetLastError());
    }

    MODULEENTRY32 entry;
    entry.dwSize = sizeof(MODULEENTRY32);

    QString result = QStringLiteral("module=<not_found>");
    if (Module32First(snapshot, &entry)) {
        do {
            const quintptr base = reinterpret_cast<quintptr>(entry.modBaseAddr);
            const quintptr end = base + static_cast<quintptr>(entry.modBaseSize);
            if (address >= base && address < end) {
                result = QStringLiteral("module=%1 base=%2 size=%3 offset=%4")
                    .arg(QString::fromWCharArray(entry.szExePath))
                    .arg(hexValue(base))
                    .arg(entry.modBaseSize)
                    .arg(hexValue(address - base));
                break;
            }
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return result;
}

QString memoryInfoForAddress(quintptr address) {
    MEMORY_BASIC_INFORMATION memoryInfo = {};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &memoryInfo, sizeof(memoryInfo)) == 0) {
        return QStringLiteral("memory=<query_failed error=%1>").arg(GetLastError());
    }

    return QStringLiteral("memory_base=%1 allocation_base=%2 region_size=%3 state=0x%4 protect=0x%5 type=0x%6")
        .arg(hexValue(reinterpret_cast<quintptr>(memoryInfo.BaseAddress)))
        .arg(hexValue(reinterpret_cast<quintptr>(memoryInfo.AllocationBase)))
        .arg(static_cast<qulonglong>(memoryInfo.RegionSize))
        .arg(QString::number(memoryInfo.State, 16))
        .arg(QString::number(memoryInfo.Protect, 16))
        .arg(QString::number(memoryInfo.Type, 16));
}

QString exceptionParameters(const EXCEPTION_RECORD* exceptionRecord) {
    if (!exceptionRecord || exceptionRecord->NumberParameters == 0) {
        return QStringLiteral("<none>");
    }

    QStringList parameters;
    const DWORD count = qMin<DWORD>(exceptionRecord->NumberParameters, EXCEPTION_MAXIMUM_PARAMETERS);
    for (DWORD index = 0; index < count; ++index) {
        parameters.append(hexValue(static_cast<quintptr>(exceptionRecord->ExceptionInformation[index])));
    }
    return parameters.join(QStringLiteral(","));
}

void logExceptionDetails(const QString& prefix, EXCEPTION_POINTERS* exceptionInfo) {
    const DWORD exceptionCode = exceptionInfo && exceptionInfo->ExceptionRecord
        ? exceptionInfo->ExceptionRecord->ExceptionCode
        : 0;
    const quintptr exceptionAddress = exceptionInfo && exceptionInfo->ExceptionRecord
        ? reinterpret_cast<quintptr>(exceptionInfo->ExceptionRecord->ExceptionAddress)
        : 0;
    const DWORD exceptionFlags = exceptionInfo && exceptionInfo->ExceptionRecord
        ? exceptionInfo->ExceptionRecord->ExceptionFlags
        : 0;
    const QString parameters = exceptionParameters(exceptionInfo ? exceptionInfo->ExceptionRecord : nullptr);
    QString contextText = QStringLiteral("context=<none>");
#if defined(_M_X64) || defined(__x86_64__)
    if (exceptionInfo && exceptionInfo->ContextRecord) {
        contextText = QStringLiteral("rip=%1 rsp=%2 rbp=%3")
            .arg(hexValue(static_cast<quintptr>(exceptionInfo->ContextRecord->Rip)))
            .arg(hexValue(static_cast<quintptr>(exceptionInfo->ContextRecord->Rsp)))
            .arg(hexValue(static_cast<quintptr>(exceptionInfo->ContextRecord->Rbp)));
    }
#endif

    logMessage(QString("%1: code=0x%2 flags=0x%3 address=%4 thread=%5 params=%6 %7 %8 %9")
        .arg(prefix)
        .arg(QString::number(exceptionCode, 16))
        .arg(QString::number(exceptionFlags, 16))
        .arg(hexValue(exceptionAddress))
        .arg(GetCurrentThreadId())
        .arg(parameters)
        .arg(moduleInfoForAddress(exceptionAddress))
        .arg(memoryInfoForAddress(exceptionAddress))
        .arg(contextText));

    if (exceptionCode == EXCEPTION_ACCESS_VIOLATION
        && exceptionInfo
        && exceptionInfo->ExceptionRecord
        && exceptionInfo->ExceptionRecord->NumberParameters >= 2) {
        const quintptr accessMode = static_cast<quintptr>(exceptionInfo->ExceptionRecord->ExceptionInformation[0]);
        const quintptr accessedAddress = static_cast<quintptr>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]);
        logMessage(QString("%1 access violation detail: mode=%2 accessed_address=%3 %4 %5")
            .arg(prefix)
            .arg(accessMode == 0 ? QStringLiteral("read") : accessMode == 1 ? QStringLiteral("write") : accessMode == 8 ? QStringLiteral("execute") : hexValue(accessMode))
            .arg(hexValue(accessedAddress))
            .arg(moduleInfoForAddress(accessedAddress))
            .arg(memoryInfoForAddress(accessedAddress)));
    }
}

LONG WINAPI logVectoredException(EXCEPTION_POINTERS* exceptionInfo) {
    if (InterlockedCompareExchange(&g_restartLaunchInProgress, 0, 0) == 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const LONG count = InterlockedIncrement(&g_restartExceptionLogCount);
    if (count <= 8) {
        logExceptionDetails(QStringLiteral("Vectored exception during restart #%1").arg(count), exceptionInfo);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI logUnhandledException(EXCEPTION_POINTERS* exceptionInfo) {
    const DWORD exceptionCode = exceptionInfo && exceptionInfo->ExceptionRecord
        ? exceptionInfo->ExceptionRecord->ExceptionCode
        : 1;
    logExceptionDetails(QStringLiteral("Unhandled exception"), exceptionInfo);
    if (InterlockedCompareExchange(&g_showRestartFailureDialogOnUnhandledException, 0, 0) != 0) {
        MessageBoxW(
            nullptr,
            L"Failed to restart the app after the update.",
            L"APE HOI4 Tool Studio Updater",
            MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
    }
    TerminateProcess(GetCurrentProcess(), exceptionCode == 0 ? 1 : exceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

void installNativeCrashLogging() {
    AddVectoredExceptionHandler(1, logVectoredException);
    SetUnhandledExceptionFilter(logUnhandledException);
}

void logLoadedModulesSnapshot(const QString& label) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) {
        logMessage(QString("%1: module snapshot failed error=%2").arg(label).arg(GetLastError()));
        return;
    }

    MODULEENTRY32 entry;
    entry.dwSize = sizeof(MODULEENTRY32);

    int moduleCount = 0;
    if (Module32First(snapshot, &entry)) {
        do {
            ++moduleCount;
            logMessage(QString("%1 module[%2]: name=%3 base=%4 size=%5 path=%6")
                .arg(label)
                .arg(moduleCount)
                .arg(QString::fromWCharArray(entry.szModule))
                .arg(hexValue(reinterpret_cast<quintptr>(entry.modBaseAddr)))
                .arg(entry.modBaseSize)
                .arg(QString::fromWCharArray(entry.szExePath)));
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    logMessage(QString("%1: module_count=%2").arg(label).arg(moduleCount));
}
#else
void installNativeCrashLogging() {}
#endif

QString restartModeFromArguments(const QStringList& args, int startIndex) {
    const QString prefix = QStringLiteral("--restart-mode=");
    for (int i = startIndex; i < args.size(); ++i) {
        const QString arg = args[i].trimmed();
        if (arg.startsWith(prefix)) {
            return arg.mid(prefix.size()).trimmed().toLower();
        }
        if (arg == QStringLiteral("--restart-mode") && i + 1 < args.size()) {
            return args[i + 1].trimmed().toLower();
        }
    }

#ifdef Q_OS_WIN
    return QStringLiteral("commandline");
#else
    return QStringLiteral("qprocess");
#endif
}

bool hasArgument(const QStringList& args, const QString& name) {
    return std::any_of(args.cbegin(), args.cend(), [&](const QString& arg) {
        return arg == name;
    });
}

#ifdef Q_OS_WIN
QString quoteWindowsCommandLineArgument(const QString& argument);
void showRestartFailureDialogAndExit();
int terminateProcessesByName(const QString& processName);
#endif

bool maybeRelaunchUpdaterForIsolation(const QStringList& args) {
#ifdef Q_OS_WIN
    if (args.size() < 3
        || hasArgument(args, QStringLiteral("--reexec-child"))
        || hasArgument(args, QStringLiteral("--debug-restart-only"))) {
        return false;
    }

    constexpr int maxAttempts = 3;
    constexpr DWORD childTimeoutMs = 30000;
    const QString updaterPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString updaterWorkingDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    QStringList childArgs = args.mid(1);
    childArgs.append(QStringLiteral("--reexec-child"));

    const std::wstring currentDirectory = updaterWorkingDir.toStdWString();

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        QStringList commandArguments;
        commandArguments << updaterPath;
        commandArguments.append(childArgs);

        QStringList quotedArguments;
        quotedArguments.reserve(commandArguments.size());
        for (const QString& argument : commandArguments) {
            quotedArguments.append(quoteWindowsCommandLineArgument(argument));
        }

        const std::wstring commandLine = quotedArguments.join(QLatin1Char(' ')).toStdWString();
        std::vector<wchar_t> commandLineBuffer(commandLine.begin(), commandLine.end());
        commandLineBuffer.push_back(L'\0');

        STARTUPINFOW startupInfo = {};
        startupInfo.cb = sizeof(startupInfo);

        PROCESS_INFORMATION processInfo = {};
        logMessage(QString("Updater self-relaunch isolation attempt %1/%2 requested.")
            .arg(attempt)
            .arg(maxAttempts));
        logMessage(QString("Updater self-relaunch isolation command prepared: chars=%1 use_application_name=false")
            .arg(commandLineBuffer.size()));

        SetLastError(ERROR_SUCCESS);
        InterlockedExchange(&g_showRestartFailureDialogOnUnhandledException, 1);
        const BOOL created = CreateProcessW(
            nullptr,
            commandLineBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP,
            nullptr,
            currentDirectory.c_str(),
            &startupInfo,
            &processInfo);
        const DWORD createError = GetLastError();
        InterlockedExchange(&g_showRestartFailureDialogOnUnhandledException, 0);
        if (!created) {
            logMessage(QString("Updater self-relaunch isolation failed to start child process: error=%1")
                .arg(createError));
        } else {
            logMessage(QString("Updater self-relaunch isolation started child process. pid=%1 working_dir=%2")
                .arg(processInfo.dwProcessId)
                .arg(updaterWorkingDir));
            CloseHandle(processInfo.hThread);

            const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, childTimeoutMs);
            if (waitResult == WAIT_OBJECT_0) {
                DWORD exitCode = 1;
                const BOOL exitCodeAvailable = GetExitCodeProcess(processInfo.hProcess, &exitCode);
                CloseHandle(processInfo.hProcess);

                const bool success = exitCodeAvailable && exitCode == 0;
                logMessage(QString("Updater self-relaunch isolation child finished: success=%1 exit_code_available=%2 exit_code=%3 exit_code_hex=0x%4")
                    .arg(success ? "true" : "false")
                    .arg(exitCodeAvailable ? "true" : "false")
                    .arg(exitCode)
                    .arg(QString::number(exitCode, 16)));
                if (success) {
                    return true;
                }
            } else {
                logMessage(QString("Updater self-relaunch isolation child did not finish: wait_result=%1; terminating child.")
                    .arg(waitResult));
                TerminateProcess(processInfo.hProcess, 1);
                WaitForSingleObject(processInfo.hProcess, 3000);
                CloseHandle(processInfo.hProcess);
            }
        }

        const int terminatedMainProcesses = terminateProcessesByName(QStringLiteral("APEHOI4ToolStudio.exe"));
        if (terminatedMainProcesses > 0) {
            logMessage(QString("Updater self-relaunch retry cleanup terminated %1 main process(es).")
                .arg(terminatedMainProcesses));
        }

        if (attempt < maxAttempts) {
            logMessage(QString("Updater self-relaunch isolation attempt %1/%2 failed; retrying in 1000 ms.")
                .arg(attempt)
                .arg(maxAttempts));
            QThread::msleep(1000);
        }
    }

    showRestartFailureDialogAndExit();
#else
    Q_UNUSED(args);
#endif
    return false;
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

int terminateProcessesByName(const QString& processName) {
    int terminated = 0;
    const QList<DWORD> processIds = processIdsByName(processName);
    for (DWORD processId : processIds) {
        HANDLE processHandle = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, processId);
        if (!processHandle) {
            continue;
        }

        if (TerminateProcess(processHandle, 1)) {
            WaitForSingleObject(processHandle, 3000);
            ++terminated;
        }
        CloseHandle(processHandle);
    }
    return terminated;
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

bool restartMainApplication(const QString& mainAppPath, const QString& workingDir, const QString& restartMode) {
    logMessage("Restart program: " + mainAppPath);
    logMessage("Restart working directory: " + workingDir);
    logMessage("Restart mode: " + restartMode);

#ifdef Q_OS_WIN
    const QFileInfo mainAppInfo(mainAppPath);
    logMessage(QString("Restart file state: exists=%1 size=%2 last_modified=%3")
        .arg(mainAppInfo.exists() ? "true" : "false")
        .arg(mainAppInfo.exists() ? mainAppInfo.size() : -1)
        .arg(mainAppInfo.exists()
            ? mainAppInfo.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
            : QStringLiteral("")));
    logMessage("Restart preparing native launch strings...");

    const QString nativeMainAppPath = QDir::toNativeSeparators(mainAppPath);
    const QString nativeWorkingDir = QDir::toNativeSeparators(workingDir);
    logMessage("Restart native program: " + nativeMainAppPath);
    logMessage("Restart native working directory: " + nativeWorkingDir);

    const std::wstring applicationName = nativeMainAppPath.toStdWString();
    const std::wstring commandLine = L"\"" + nativeMainAppPath.toStdWString() + L"\"";
    const std::wstring currentDirectory = nativeWorkingDir.toStdWString();
    logMessage(QString("Restart native strings prepared: application_chars=%1 command_chars=%2 cwd_chars=%3 cwd_exists=%4")
        .arg(applicationName.size())
        .arg(commandLine.size())
        .arg(currentDirectory.size())
        .arg(QDir(workingDir).exists() ? "true" : "false"));

    std::vector<wchar_t> commandLineBuffer(commandLine.begin(), commandLine.end());
    commandLineBuffer.push_back(L'\0');
    logMessage(QString("Restart command line buffer prepared: chars=%1").arg(commandLineBuffer.size()));

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_SHOWNORMAL;
    logMessage("Restart startup info prepared.");

    PROCESS_INFORMATION processInfo = {};
    QElapsedTimer launchTimer;
    launchTimer.start();
    logLoadedModulesSnapshot(QStringLiteral("Restart pre-CreateProcessW modules"));
    InterlockedExchange(&g_restartExceptionLogCount, 0);
    InterlockedExchange(&g_restartLaunchInProgress, 1);
    SetLastError(ERROR_SUCCESS);
    const bool useApplicationName = restartMode != QStringLiteral("commandline");
    const DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT
        | (restartMode == QStringLiteral("no-new-group") ? 0 : CREATE_NEW_PROCESS_GROUP)
        | (restartMode == QStringLiteral("suspended") ? CREATE_SUSPENDED : 0);
    logMessage(QString("Restart CreateProcessW parameters: use_application_name=%1 creation_flags=0x%2")
        .arg(useApplicationName ? "true" : "false")
        .arg(QString::number(creationFlags, 16)));
    logMessage("Restart calling CreateProcessW...");
    const BOOL created = CreateProcessW(
        useApplicationName ? applicationName.c_str() : nullptr,
        commandLineBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        creationFlags,
        nullptr,
        currentDirectory.c_str(),
        &startupInfo,
        &processInfo);
    const DWORD lastErrorAfterCreateProcess = GetLastError();
    InterlockedExchange(&g_restartLaunchInProgress, 0);

    logMessage(QString("Restart CreateProcessW returned: elapsed_ms=%1 raw_result=%2")
        .arg(launchTimer.elapsed())
        .arg(created ? "true" : "false"));

    const DWORD createError = created ? ERROR_SUCCESS : lastErrorAfterCreateProcess;
    logMessage(QString("Restart CreateProcessW last_error=%1").arg(lastErrorAfterCreateProcess));
    logMessage(QString("Restart launch result: success=%1")
        .arg(created ? "true" : "false"));
    if (created) {
        logMessage(QString("Restart launch PID: %1").arg(processInfo.dwProcessId));
        if (restartMode == QStringLiteral("suspended")) {
            const DWORD resumeResult = ResumeThread(processInfo.hThread);
            logMessage(QString("Restart suspended thread resume result: %1").arg(resumeResult));
        }
        DWORD childExitCode = 0;
        const BOOL exitCodeAvailable = GetExitCodeProcess(processInfo.hProcess, &childExitCode);
        const DWORD childWaitState = WaitForSingleObject(processInfo.hProcess, 0);
        logMessage(QString("Restart child initial state: process_id=%1 thread_id=%2 wait0=%3 exit_code_available=%4 exit_code=%5")
            .arg(processInfo.dwProcessId)
            .arg(processInfo.dwThreadId)
            .arg(childWaitState)
            .arg(exitCodeAvailable ? "true" : "false")
            .arg(childExitCode));
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return true;
    }
    logMessage(QString("Restart launch error: %1").arg(createError));
    return false;
#else
    qint64 pid = 0;
    Q_UNUSED(restartMode);
    const bool started = QProcess::startDetached(mainAppPath, QStringList(), workingDir, &pid);
    logMessage(QString("Restart launch result: success=%1")
        .arg(started ? "true" : "false"));
    if (started) {
        logMessage(QString("Restart launch PID: %1").arg(pid));
    }
    return started;
#endif
}

#ifdef Q_OS_WIN
QString quoteWindowsCommandLineArgument(const QString& argument) {
    if (!argument.isEmpty()
        && !argument.contains(QLatin1Char(' '))
        && !argument.contains(QLatin1Char('\t'))
        && !argument.contains(QLatin1Char('"'))) {
        return argument;
    }

    QString quoted = QStringLiteral("\"");
    int backslashCount = 0;
    for (const QChar ch : argument) {
        if (ch == QLatin1Char('\\')) {
            ++backslashCount;
        } else if (ch == QLatin1Char('"')) {
            quoted += QString(backslashCount * 2 + 1, QLatin1Char('\\'));
            quoted += ch;
            backslashCount = 0;
        } else {
            quoted += QString(backslashCount, QLatin1Char('\\'));
            quoted += ch;
            backslashCount = 0;
        }
    }
    quoted += QString(backslashCount * 2, QLatin1Char('\\'));
    quoted += QLatin1Char('"');
    return quoted;
}
#endif

void showRestartFailureDialogAndExit() {
    logMessage("Restart failed after all retry attempts.");
#ifdef Q_OS_WIN
    MessageBoxW(
        nullptr,
        L"Failed to restart the app after the update.",
        L"APE HOI4 Tool Studio Updater",
        MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
    logMessage("Terminating updater after restart failure.");
    ExitProcess(1);
#endif
}

bool runRestartAttemptChild(const QString& targetDir, const QString& restartMode, int attempt, int maxAttempts) {
#ifdef Q_OS_WIN
    const QString updaterPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString updaterWorkingDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QStringList commandArguments = {
        updaterPath,
        QStringLiteral("--restart-attempt-child"),
        targetDir,
        QStringLiteral("--restart-mode"),
        restartMode
    };

    QStringList quotedArguments;
    quotedArguments.reserve(commandArguments.size());
    for (const QString& argument : commandArguments) {
        quotedArguments.append(quoteWindowsCommandLineArgument(argument));
    }

    const std::wstring commandLine = quotedArguments.join(QLatin1Char(' ')).toStdWString();
    const std::wstring currentDirectory = updaterWorkingDir.toStdWString();

    std::vector<wchar_t> commandLineBuffer(commandLine.begin(), commandLine.end());
    commandLineBuffer.push_back(L'\0');

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo = {};
    logMessage(QString("Restart attempt %1/%2 starting.").arg(attempt).arg(maxAttempts));
    logMessage(QString("Restart attempt %1/%2 native child command prepared: chars=%3 cwd=%4")
        .arg(attempt)
        .arg(maxAttempts)
        .arg(commandLineBuffer.size())
        .arg(updaterWorkingDir));
    logMessage(QString("Restart attempt %1/%2 native child CreateProcessW parameters: use_application_name=false")
        .arg(attempt)
        .arg(maxAttempts));

    SetLastError(ERROR_SUCCESS);
    const BOOL created = CreateProcessW(
        nullptr,
        commandLineBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP,
        nullptr,
        currentDirectory.c_str(),
        &startupInfo,
        &processInfo);
    const DWORD createError = GetLastError();

    if (!created) {
        logMessage(QString("Restart attempt %1/%2 failed to start child updater: error=%3")
            .arg(attempt)
            .arg(maxAttempts)
            .arg(createError));
        return false;
    }

    logMessage(QString("Restart attempt %1/%2 child updater PID: %3")
        .arg(attempt)
        .arg(maxAttempts)
        .arg(processInfo.dwProcessId));
    CloseHandle(processInfo.hThread);

    const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 3000);
    if (waitResult == WAIT_TIMEOUT) {
        logMessage(QString("Restart attempt %1/%2 timed out; terminating child updater.")
            .arg(attempt)
            .arg(maxAttempts));
        TerminateProcess(processInfo.hProcess, 1);
        WaitForSingleObject(processInfo.hProcess, 3000);
        CloseHandle(processInfo.hProcess);
        return false;
    }

    if (waitResult == WAIT_FAILED) {
        const DWORD waitError = GetLastError();
        logMessage(QString("Restart attempt %1/%2 wait failed: error=%3")
            .arg(attempt)
            .arg(maxAttempts)
            .arg(waitError));
        TerminateProcess(processInfo.hProcess, 1);
        CloseHandle(processInfo.hProcess);
        return false;
    }

    DWORD exitCode = 1;
    const BOOL exitCodeAvailable = GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hProcess);

    const bool success = exitCodeAvailable && exitCode == 0;
    logMessage(QString("Restart attempt %1/%2 finished: success=%3 wait_result=%4 exit_code_available=%5 exit_code=%6 exit_code_hex=0x%7")
        .arg(attempt)
        .arg(maxAttempts)
        .arg(success ? "true" : "false")
        .arg(waitResult)
        .arg(exitCodeAvailable ? "true" : "false")
        .arg(exitCode)
        .arg(QString::number(exitCode, 16)));
    return success;
#else
    QProcess restartProcess;
    restartProcess.setProgram(QCoreApplication::applicationFilePath());
    restartProcess.setWorkingDirectory(QCoreApplication::applicationDirPath());
    restartProcess.setProcessChannelMode(QProcess::ForwardedChannels);

    QStringList restartArgs;
    restartArgs << QStringLiteral("--restart-attempt-child")
                << targetDir
                << QStringLiteral("--restart-mode")
                << restartMode;
    restartProcess.setArguments(restartArgs);

    logMessage(QString("Restart attempt %1/%2 starting.").arg(attempt).arg(maxAttempts));
    restartProcess.start();
    if (!restartProcess.waitForStarted(3000)) {
        logMessage(QString("Restart attempt %1/%2 failed to start child updater: %3")
            .arg(attempt)
            .arg(maxAttempts)
            .arg(restartProcess.errorString()));
        return false;
    }

    logMessage(QString("Restart attempt %1/%2 child updater PID: %3")
        .arg(attempt)
        .arg(maxAttempts)
        .arg(restartProcess.processId()));

    if (!restartProcess.waitForFinished(3000)) {
        logMessage(QString("Restart attempt %1/%2 timed out; killing child updater.")
            .arg(attempt)
            .arg(maxAttempts));
        restartProcess.kill();
        restartProcess.waitForFinished(3000);
        return false;
    }

    const bool success = restartProcess.exitStatus() == QProcess::NormalExit
        && restartProcess.exitCode() == 0;
    logMessage(QString("Restart attempt %1/%2 finished: success=%3 exit_status=%4 exit_code=%5")
        .arg(attempt)
        .arg(maxAttempts)
        .arg(success ? "true" : "false")
        .arg(restartProcess.exitStatus() == QProcess::NormalExit ? "normal" : "crash")
        .arg(restartProcess.exitCode()));
    return success;
#endif
}

bool restartMainApplicationWithRetries(const QString& targetDir, const QString& restartMode) {
    constexpr int maxAttempts = 3;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (runRestartAttemptChild(targetDir, restartMode, attempt, maxAttempts)) {
            return true;
        }

        if (attempt < maxAttempts) {
            logMessage(QString("Restart attempt %1/%2 failed; retrying in 1000 ms.")
                .arg(attempt)
                .arg(maxAttempts));
            QThread::msleep(1000);
        }
    }

    showRestartFailureDialogAndExit();
    return false;
}

struct FileRetryResult {
    bool success = false;
    int attempts = 0;
    qint64 elapsedMs = 0;
    QString errorMessage;
};

FileRetryResult removeFileWithRetry(const QString& filePath, int attempts = 30, int delayMs = 100) {
    FileRetryResult result;
    QElapsedTimer timer;
    timer.start();

    if (!QFile::exists(filePath)) {
        result.success = true;
        result.elapsedMs = timer.elapsed();
        return result;
    }

    for (int i = 0; i < attempts; ++i) {
        result.attempts = i + 1;
        QFile file(filePath);
        if (file.remove()) {
            result.success = true;
            result.elapsedMs = timer.elapsed();
            return result;
        }

        result.errorMessage = file.errorString();

        QThread::msleep(static_cast<unsigned long>(delayMs));
    }

    result.elapsedMs = timer.elapsed();
    return result;
}

FileRetryResult copyFileWithRetry(const QString& sourcePath, const QString& destinationPath, int attempts = 30, int delayMs = 100) {
    FileRetryResult result;
    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < attempts; ++i) {
        result.attempts = i + 1;
        QFile sourceFile(sourcePath);
        if (sourceFile.copy(destinationPath)) {
            result.success = true;
            result.elapsedMs = timer.elapsed();
            return result;
        }

        result.errorMessage = sourceFile.errorString();

        QThread::msleep(static_cast<unsigned long>(delayMs));
    }

    result.elapsedMs = timer.elapsed();
    return result;
}

// Recursively collect all file paths relative to baseDir.
QStringList getAllLocalFiles(const QString& baseDir, const QString& relativeDir = QString()) {
    QStringList result;
    QDir dir(relativeDir.isEmpty() ? baseDir : QDir(baseDir).filePath(relativeDir));
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        const QString relativePath = relativeDir.isEmpty()
            ? info.fileName()
            : relativeDir + QStringLiteral("/") + info.fileName();

        if (info.isDir()) {
            result.append(getAllLocalFiles(baseDir, relativePath));
        } else {
            result.append(relativePath);
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
    initializeUpdaterLogFile();
    installNativeCrashLogging();
    QElapsedTimer totalTimer;
    totalTimer.start();

    QStringList args = app.arguments();
    if (args.size() >= 3 && args[1] == QStringLiteral("--restart-attempt-child")) {
        const QString targetDir = QDir::cleanPath(args[2]);
        const QString mainAppPath = QDir(targetDir).filePath(QStringLiteral("APEHOI4ToolStudio.exe"));
        const QString restartMode = restartModeFromArguments(args, 3);
        logMessage("Updater restart attempt child mode.");
        logMessage("Target Dir: " + targetDir);
        if (!QFile::exists(mainAppPath)) {
            logMessage("Main application not found at: " + mainAppPath);
            return 1;
        }
        const bool restarted = restartMainApplication(mainAppPath, targetDir, restartMode);
        logMessage(QString("Updater finished in %1 ms.").arg(totalTimer.elapsed()));
        return restarted ? 0 : 1;
    }

    if (args.size() >= 3 && args[1] == QStringLiteral("--debug-restart-only")) {
        const QString targetDir = QDir::cleanPath(args[2]);
        const QString mainAppPath = QDir(targetDir).filePath(QStringLiteral("APEHOI4ToolStudio.exe"));
        const QString restartMode = restartModeFromArguments(args, 3);
        logMessage("Updater debug restart-only mode.");
        logMessage("Target Dir: " + targetDir);
        if (!QFile::exists(mainAppPath)) {
            logMessage("Main application not found at: " + mainAppPath);
            return 1;
        }
        const bool restarted = restartMainApplication(mainAppPath, targetDir, restartMode);
        logMessage(QString("Updater finished in %1 ms.").arg(totalTimer.elapsed()));
        return restarted ? 0 : 1;
    }

    if (args.size() < 3) {
        logMessage("Usage: updater.exe <target_dir> <temp_dir> [main_pid] [--restart-mode commandline|appname|no-new-group|suspended]");
        logMessage("Debug: updater.exe --debug-restart-only <target_dir> [--restart-mode commandline|appname|no-new-group|suspended]");
        return 1;
    }

    QString targetDir = args[1];
    QString tempDir = args[2];
    const QString restartMode = restartModeFromArguments(args, 3);
    qint64 mainPid = 0;
    if (args.size() >= 4) {
        bool ok = false;
        mainPid = args[3].toLongLong(&ok);
        if (!ok) {
            mainPid = 0;
        }
    }

    logMessage("Updater started.");
    logMessage("Updater PID: " + QString::number(QCoreApplication::applicationPid()));
    logMessage("Target Dir: " + targetDir);
    logMessage("Temp Dir: " + tempDir);

    const QString mainAppPath = QDir(targetDir).filePath("APEHOI4ToolStudio.exe");

    logMessage("Waiting for main application to exit...");
    QElapsedTimer stageTimer;
    stageTimer.start();

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

    logMessage(QString("Main application shutdown wait completed in %1 ms.").arg(stageTimer.elapsed()));
    stageTimer.restart();

    if (maybeRelaunchUpdaterForIsolation(args)) {
        logMessage(QString("Updater finished in %1 ms after self-relaunch handoff.").arg(totalTimer.elapsed()));
        return 0;
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
        stageTimer.restart();

        // Read manifest file list
        QSet<QString> manifestFiles;
        QSet<QString> officialToolDirs; // e.g. "tools/LogManagerTool"
        QSet<QString> officialPluginDirs; // e.g. "plugins/Lumorpha"

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

        QStringList managedCleanupDirs;
        for (const QString& toolDir : std::as_const(officialToolDirs)) {
            managedCleanupDirs.append(toolDir);
        }
        for (const QString& pluginDir : std::as_const(officialPluginDirs)) {
            managedCleanupDirs.append(pluginDir);
        }
        managedCleanupDirs.removeDuplicates();
        managedCleanupDirs.sort();

        logMessage(QString("Cleanup scope contains %1 managed tool/plugin directories.")
            .arg(managedCleanupDirs.size()));

        // Scan only updater-managed tool/plugin directories. The update manifest is
        // not a complete install-directory whitelist because protected builds keep
        // runtime resources in the install folder that are not part of update payloads.
        QStringList localFiles;
        for (const QString& cleanupDir : std::as_const(managedCleanupDirs)) {
            if (QDir(QDir(targetDir).filePath(cleanupDir)).exists()) {
                localFiles.append(getAllLocalFiles(targetDir, cleanupDir));
            }
        }
        localFiles.removeDuplicates();
        int removedCount = 0;
        int failedRemovalCount = 0;
        QStringList failedRemovalSamples;

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
                failedRemovalCount++;
                if (failedRemovalSamples.size() < 20) {
                    failedRemovalSamples.append(localFile);
                }
            }
        }

        if (failedRemovalCount > 0) {
            logMessage(QString("Cleanup skipped %1 obsolete file(s) that could not be removed. Samples: %2")
                .arg(failedRemovalCount)
                .arg(failedRemovalSamples.join(QStringLiteral(", "))));
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
            if (managedCleanupDirs.contains(relDir)) {
                return;
            }

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

        for (const QString& cleanupDir : std::as_const(managedCleanupDirs)) {
            const QString cleanupPath = QDir(targetDir).filePath(cleanupDir);
            if (QDir(cleanupPath).exists()) {
                removeEmptyDirs(cleanupPath, targetDir);
            }
        }

        logMessage(QString("Cleanup complete. Removed %1 obsolete files in %2 ms.")
            .arg(removedCount)
            .arg(stageTimer.elapsed()));
    } else {
        logMessage("No manifest_files.txt found, skipping cleanup (legacy update mode).");
    }

    logMessage("Starting file copy...");
    stageTimer.restart();

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
                    logMessage("Skipped protected update payload file: " + fileInfo.fileName());
                    continue;
                }
                if (QFile::exists(dstFilePath)) {
                    const FileRetryResult removeResult = removeFileWithRetry(dstFilePath);
                    logMessage(QString("Remove result: success=%1 attempts=%2 elapsed_ms=%3 path=%4 error=%5")
                        .arg(removeResult.success ? "true" : "false")
                        .arg(removeResult.attempts)
                        .arg(removeResult.elapsedMs)
                        .arg(dstFilePath)
                        .arg(removeResult.errorMessage));

                    if (!removeResult.success) {
                        logMessage("Failed to remove existing file after retries: " + dstFilePath);
                        success = false;
                        continue;
                    }
                }
                const FileRetryResult copyResult = copyFileWithRetry(srcFilePath, dstFilePath);
                logMessage(QString("Copy result: success=%1 attempts=%2 elapsed_ms=%3 bytes=%4 src=%5 dst=%6 error=%7")
                    .arg(copyResult.success ? "true" : "false")
                    .arg(copyResult.attempts)
                    .arg(copyResult.elapsedMs)
                    .arg(fileInfo.size())
                    .arg(srcFilePath)
                    .arg(dstFilePath)
                    .arg(copyResult.errorMessage));

                if (!copyResult.success) {
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
        logMessage(QString("File copy completed successfully in %1 ms.").arg(stageTimer.elapsed()));
        stageTimer.restart();

        // Restart main application
        if (QFile::exists(mainAppPath)) {
            logMessage("Restarting main application...");
            const bool restarted = restartMainApplicationWithRetries(targetDir, restartMode);
            logMessage(QString("Restart stage elapsed_ms=%1").arg(stageTimer.elapsed()));
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

    logMessage(QString("Updater finished in %1 ms.").arg(totalTimer.elapsed()));
    return 0;
}
