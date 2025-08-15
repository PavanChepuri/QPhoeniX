#include <QApplication>
#include "UI/mainwindow.h"
#include "Network/kiteconnectapi.h"
#include "Data/datamanager.h"
#include "Utils/configurationmanager.h"
#include "Utils/marketcalendar.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Initialize ConfigurationManager first
    ConfigurationManager::instance()->loadConfiguration("Config/config.json");

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
