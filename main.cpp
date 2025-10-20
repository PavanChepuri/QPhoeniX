#include <QApplication>
#include <QDir>
#include <QTimer>
#include "UI/mainwindow.h"
#include "Network/kiteconnectapi.h"
#include "Data/datamanager.h"
#include "Utils/configurationmanager.h"
#include "Utils/marketcalendar.h"

// --- path helpers (Windows-friendly, no schema change) ---
static QString qp_cfgRoot() {
    // Prefer the current working dir (Qt Creator Run settings)
    QDir wd(QDir::currentPath());
    if (wd.exists("Config"))
        return wd.absoluteFilePath("Config");

    // Next to the executable (shadow builds)
    QDir exe(QCoreApplication::applicationDirPath());
    if (exe.exists("Config"))
        return exe.absoluteFilePath("Config");

    // Fallback: create in working dir if nothing exists yet
    QString fallback = QDir::currentPath() + "/Config";
    QDir().mkpath(fallback);
    return fallback;
}

static QString qp_cfgFile(const QString& name) {
    return qp_cfgRoot() + "/" + name;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Initialize ConfigurationManager first
    const QString configPath = qp_cfgFile("config.json");
    ConfigurationManager::instance()->loadConfiguration(configPath);

    // Initialize MarketCalendar and load holidays
    MarketCalendar::instance()->loadHolidays();

    // Get the API key from the configuration
    QString apiKey = ConfigurationManager::instance()->getApiKey();
    if (apiKey.isEmpty()) {
        qDebug() << "API key is missing in the configuration file.";
        return -1; // Or handle this error appropriately
    }

    // Create KiteConnectAPI instance with the API key
    KiteConnectAPI kiteAPI(apiKey);

    // Create and show the main window
    MainWindow w;
    w.setKiteConnectAPI(&kiteAPI);
    w.show();

    // Initialize DataManager
    DataManager::instance();

    return a.exec();
}
