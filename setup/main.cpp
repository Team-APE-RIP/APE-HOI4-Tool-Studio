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
