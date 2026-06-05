//-------------------------------------------------------------------------------------
// OverlayControlStyle.h -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#ifndef OVERLAYCONTROLSTYLE_H
#define OVERLAYCONTROLSTYLE_H

#include <QString>

class QWidget;

namespace OverlayControlStyle {
    QString pageStyleSheet(bool isDark);
    QString linkButtonStyle(bool isDark);
    void polishFormControl(QWidget *control);
}

#endif // OVERLAYCONTROLSTYLE_H
