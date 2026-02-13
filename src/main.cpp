#include "MainWindow.h"
#include "SetupDialog.h"
#include "ConfigManager.h"
#include "LocalizationManager.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // Set application metadata
    a.setApplicationName("APE HOI4 Tool Studio");
    a.setOrganizationName("Team APE-RIP");
    a.setApplicationVersion("1.0.0");
    
    // Check if first run or no mod selected
    ConfigManager& config = ConfigManager::instance();
    
    // Load language immediately
    LocalizationManager::instance().loadLanguage(config.getLanguage());

    // If game path is not set OR mod path is not set, show setup
    if (config.isFirstRun() || !config.hasModSelected()) {
        SetupDialog setup;
        if (setup.exec() == QDialog::Accepted) {
            config.setGamePath(setup.getGamePath());
            config.setModPath(setup.getModPath());
            config.setLanguage(setup.getLanguage());
        } else {
            return 0; // User cancelled setup
        }
    }

    MainWindow w;
    w.show();

    return a.exec();
}
