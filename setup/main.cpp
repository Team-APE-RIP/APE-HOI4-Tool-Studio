//-------------------------------------------------------------------------------------
// main.cpp -- Part of APE HOI4 Tool Studio
//
// Copyright (C) 2026 Team APE:RIP. All rights reserved.
// Licensed under the Team APE:RIP Source Code License Agreement.
//
// https://github.com/Team-APE-RIP/APE-HOI4-Tool-Studio/
//-------------------------------------------------------------------------------------
#include <QApplication>
#include "Setup.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 使用系统默认主题
    app.setStyle("windowsvista"); // 强制使用 Windows 默认风格
    
    Setup setup;
    setup.show();
    
    return app.exec();
}
