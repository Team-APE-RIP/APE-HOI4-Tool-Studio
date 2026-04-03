//-------------------------------------------------------------------------------------
// FileAssociationManager.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef FILEASSOCIATIONMANAGER_H
#define FILEASSOCIATIONMANAGER_H

#include <QString>

class FileAssociationManager {
public:
    static bool registerFileAssociations(QString* errorMessage = nullptr);

private:
    static bool registerSingleAssociation(const QString& extension,
                                          const QString& progId,
                                          const QString& fileTypeName,
                                          const QString& executablePath,
                                          QString* errorMessage);
};

#endif // FILEASSOCIATIONMANAGER_H