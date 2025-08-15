#ifndef MARKETCALENDAR_H
#define MARKETCALENDAR_H

#include <QObject>
#include <QDate>
#include <QTime>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager> // For fetching holidays
#include <QNetworkReply>        // For handling the reply

class MarketCalendar : public QObject
{
    Q_OBJECT

public:
    static MarketCalendar* instance();

    bool isTradingDay(const QDate &date) const;
    bool isTradingTime(const QTime &time) const;
    QTime getTradingStartTime() const;
    QTime getTradingEndTime() const;

    void loadHolidays(); // Fetch and parse holiday data
    QDate getThursdayForThisWeek(const QDate &date) const;
    QDate getLastThursdayOfMonth(int year, int month) const;
    QDate getPreviousTradingDay(const QDate &currentDate) const;

signals:
    void holidaysUpdated();

private slots:
    void onHolidaysFetched(QNetworkReply *reply); //Corrected slot declaration

private:
    explicit MarketCalendar(QObject *parent = nullptr);
    ~MarketCalendar();

    static MarketCalendar* m_instance;

    //QList<QDate> m_holidays; // Store holidays
    QMap<QDate, QString> m_holidays;  // Use a QMap.  Date -> Holiday Name
    QTime m_tradingStartTime;
    QTime m_tradingEndTime;
    QNetworkAccessManager *m_networkManager; //For network requests
    void handleNetworkReplyError(QNetworkReply *reply, const QString &endpoint);
};

#endif // MARKETCALENDAR_H
