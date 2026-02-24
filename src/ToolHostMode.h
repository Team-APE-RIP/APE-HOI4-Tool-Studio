#ifndef TOOLHOSTMODE_H
#define TOOLHOSTMODE_H

#include <QString>

// Run the application in tool host mode (as a subprocess for a specific tool)
// toolName is used to set the application name for display in task manager
// logFilePath is the path to the log file (to merge logs with main process)
// Returns the exit code
int runToolHostMode(const QString& serverName, const QString& toolPath, const QString& toolName = "Tool", const QString& logFilePath = QString());

#endif // TOOLHOSTMODE_H
