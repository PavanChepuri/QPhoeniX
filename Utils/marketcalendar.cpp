#include "marketcalendar.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDate>
#include <QTime>
#include <QDebug>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
//Make sure this path is correct
#include "Utils/configurationmanager.h"

MarketCalendar* MarketCalendar::m_instance = nullptr;

MarketCalendar* MarketCalendar::instance()
{
    if (!m_instance) {
        m_instance = new MarketCalendar();
    }
    return m_instance;
}

MarketCalendar::MarketCalendar(QObject *parent) : QObject(parent), m_networkManager(new QNetworkAccessManager(this))
{
    // Initialize with default trading hours (example: 9:15 AM to 3:30 PM IST)
    m_tradingStartTime = QTime(9, 15);
    m_tradingEndTime = QTime(15, 30);
}

MarketCalendar::~MarketCalendar()
{
    delete m_networkManager;
}

bool MarketCalendar::isTradingDay(const QDate &date) const
{
    // Check if the day is a weekend (Saturday or Sunday)
    if (date.dayOfWeek() == Qt::Saturday || date.dayOfWeek() == Qt::Sunday) {
        return false;
    }

    // Check if the date exists as a key in the QMap.  If it's in the map,
    // it's a holiday.
    return !m_holidays.contains(date);
}

bool MarketCalendar::isTradingTime(const QTime &time) const
{
    return time >= m_tradingStartTime && time <= m_tradingEndTime;
}

QTime MarketCalendar::getTradingStartTime() const
{
    return m_tradingStartTime;
}

QTime MarketCalendar::getTradingEndTime() const
{
    return m_tradingEndTime;
}

void MarketCalendar::handleNetworkReplyError(QNetworkReply *reply, const QString &endpoint)
{
    if (!reply) return;

    qDebug() << "Network error occurred: " << endpoint << reply->errorString();

    if (reply && reply->error() != QNetworkReply::NoError) {
        qDebug() << "Endpoint:" << endpoint << "Error String:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    if (reply && reply->bytesAvailable() == 0) {
        qDebug() << endpoint << ": No data received from the server.";
        reply->deleteLater();
        return;
    }
}


void MarketCalendar::loadHolidays()
{
    // Load from cache first (using ConfigurationManager).
    QJsonArray cachedHolidays = ConfigurationManager::instance()->getHolidays();
    if (!cachedHolidays.isEmpty()) {
        m_holidays.clear();
        for (const QJsonValue &value : cachedHolidays) {
            QJsonObject holidayObj = value.toObject();
            // Use "date" as the key, and expect "yyyy-MM-dd" format after extraction
            QString dateString = holidayObj["date"].toString();
            QDate holidayDate = QDate::fromString(dateString, Qt::ISODate);
            if (!holidayDate.isValid()) {
                qDebug() << "Invalid date format in cached holidays:" << dateString;
                continue;
            }
            // Store the date and title in the map.
            m_holidays[holidayDate] = holidayObj["title"].toString();
        }
        emit holidaysUpdated();
        return; // Return after loading from cache.
    }

    // If not found in cache (or cache is invalid), fetch from URL.
    QString url = "https://zerodha.com/marketintel/holiday-calendar/?format=json";  // Correct URL.
    QNetworkRequest request;
    request.setUrl(QUrl(url));

    // Use a QNetworkAccessManager to send a get request.
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onHolidaysFetched(reply);
    });
}

// Slot to handle the finished signal and process holiday data
void MarketCalendar::onHolidaysFetched(QNetworkReply *reply)
{
    if (!reply) return;


    if (reply->error() != QNetworkReply::NoError) {
        handleNetworkReplyError(reply, "loadHolidays"); // Use our error handler
        reply->deleteLater();
        return; // Important: return on error
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater(); // Clean up the reply object.

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (!jsonDoc.isArray()) {
        qDebug() << "Invalid JSON format for holidays. Expected an array.";
        return; // Important: return on error
    }

    QJsonArray jsonArray = jsonDoc.array();
    m_holidays.clear();  // Clear any existing holidays.
    QJsonArray newHolidayCache; // To store for caching.

    for (const QJsonValue &value : jsonArray) {
        QJsonObject holidayObj = value.toObject();
        if(holidayObj.isEmpty()) continue;

        // Check for the existence of the "holiday_properties" and "trading" key,
        // and ensure that "trading" exists in the list of properties
        if (!holidayObj.contains("holiday_properties") ) {
            qDebug() << "Missing 'holiday_properties' in holiday data:" << holidayObj;
            continue; // Skip this entry
        }

        QJsonValue val = holidayObj.value("holiday_properties");
        QStringList holiday_props;
        if(val.isArray()){
            for(const QJsonValue& propVal : val.toArray())
            {
                if(propVal.isString())
                    holiday_props.append(propVal.toString());
            }
        }
        else if(val.isObject()){
            for(const QString& key : val.toObject().keys())
            {
                holiday_props.append(key);
            }
        }

        bool isTradingHoliday = false;
        for(int i = 0; i< holiday_props.count(); i++)
        {
            if(holiday_props[i] == "trading" || holiday_props[i] == "nse" || holiday_props[i] == "bse")
            {
                isTradingHoliday = true;
                break;
            }
        }


        if (isTradingHoliday)
        {
            QString dateString = holidayObj["date"].toString();  // Get date as string
            QDateTime holidayDateTime = QDateTime::fromString(dateString, "yyyy-MM-dd hh:mm:ss");
            if (!holidayDateTime.isValid()) {
                qDebug() << "Invalid date format:" << dateString;
                continue; // Skip this entry
            }
            // Store the date and title.
            m_holidays[holidayDateTime.date()] = holidayObj["title"].toString(); // Use QMap
            newHolidayCache.append(holidayObj);  // Add to cache (including time), even if we don't use it.
        }
    }
    // Cache the fetched holidays using ConfigurationManager.
    ConfigurationManager::instance()->setHolidays(newHolidayCache);

    emit holidaysUpdated();
}


// Returns the Thursday of the current week, accounting for holidays
QDate MarketCalendar::getThursdayForThisWeek(const QDate &date) const
{
    QDate thursday = date;
    int daysToAdd = 0;

    switch (date.dayOfWeek()) {
    case Qt::Monday: daysToAdd = 3; break;   // Monday, so add 3 days to get to Thursday
    case Qt::Tuesday: daysToAdd = 2; break;  // Tuesday, so add 2 days to get to Thursday
    case Qt::Wednesday: daysToAdd = 1; break;// Wednesday, so add 1 day to get to Thursday
    case Qt::Thursday: daysToAdd = 0; break; // Already Thursday
    case Qt::Friday: daysToAdd = -1; break;  // Already past, so go back to Thursday
    case Qt::Saturday: daysToAdd = -2; break; // Already past, so go back to Thursday
    case Qt::Sunday: daysToAdd = -3; break;  // Already past, so go back to Thursday

    }

    thursday = thursday.addDays(daysToAdd);

    // Check if calculated thursday is holiday, if so, go back one day until a trading day is found
    while (!isTradingDay(thursday)) {
        thursday = thursday.addDays(-1);
    }
    return thursday;
}

// Returns the last Thursday of the specified month, accounting for holidays
QDate MarketCalendar::getLastThursdayOfMonth(int year, int month) const
{
    // Start with the last day of the month
    QDate date(year, month, 1);
    date = date.addMonths(1).addDays(-1); // Last day of the given month

    // Go back to the last Thursday
    int daysToSubtract = (date.dayOfWeek() - Qt::Thursday + 7) % 7; // Calculate days to subtract.  Correct.
    QDate lastThursday = date.addDays(-daysToSubtract);

    // Check if last thursday is holiday, if so, go back one day until a trading day is found
    while (!isTradingDay(lastThursday)) {
        lastThursday = lastThursday.addDays(-1);
    }

    return lastThursday;
}

// *** ADDED: Implementation for getPreviousTradingDay ***
/**
 * @brief Finds the date of the most recent trading day before the given date.
 * @param currentDate The date to start searching backwards from.
 * @return The date of the previous trading day, or an invalid QDate if none found within limits.
 */
QDate MarketCalendar::getPreviousTradingDay(const QDate &currentDate) const
{
    QDate dateToCheck = currentDate.addDays(-1); // Start checking from the day before
    int daysChecked = 0;
    const int maxDaysToCheck = 30; // Safety limit to prevent infinite loops

    while (daysChecked < maxDaysToCheck) {
        if (isTradingDay(dateToCheck)) {
            return dateToCheck; // Found a valid trading day
        }
        dateToCheck = dateToCheck.addDays(-1); // Go back one more day
        daysChecked++;
    }

    qWarning() << "Could not find a previous trading day within the last" << maxDaysToCheck << "days from" << currentDate.toString();
    return QDate(); // Return invalid date if not found within limit
}
// --- End Added Implementation ---
