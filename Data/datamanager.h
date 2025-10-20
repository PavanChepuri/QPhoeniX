#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QObject>
#include <QHash>
#include <QMap>
#include <QVector>
#include <QString>
#include <QDate>
#include <QDateTime>
#include <QJsonArray>

// Project data structures
#include "Data/DataStructures/instrumentdata.h"
#include "Data/DataStructures/candle.h"
#include "Data/DataStructures/instrumentanalytics.h"

// Market calendar (for prev trading day etc.)
#include "Utils/marketcalendar.h"

class DataManager : public QObject
{
    Q_OBJECT

public:
    // Singleton
    static DataManager* instance();

    // Basic accessors
    InstrumentData getInstrument(const QString &instrumentToken) const;
    QHash<QString, InstrumentData> getAllInstruments() const;
    QVector<CandleData> getStoredHistoricalData(const QString &instrumentToken, const QString &interval) const;
    InstrumentAnalytics getInstrumentAnalytics(const QString &instrumentToken) const;

    // --- Option expiry helpers (read-only utilities) ---
    // Pick the earliest expiry >= fromDate (i.e., "weekly" by convention).
    QDate nearestWeeklyExpiry(const QString& underlying,
                              const QDate& fromDate = QDate::currentDate()) const;

    // Pick the last expiry within the same month as fromDate (monthly). If none in that month,
    // pick the latest available overall (future-proof).
    QDate monthlyExpiryInSameMonth(const QString& underlying,
                                   const QDate& fromDate = QDate::currentDate()) const;

    // All options (CE/PE) for a specific underlying & expiry.
    QVector<InstrumentData> optionsForUnderlyingAndExpiry(const QString& underlying,
                                                          const QDate& expiry) const;

    // Returns the instrumentToken (QString) of the current-month future for the given underlying
    // ("NIFTY" or "BANKNIFTY"). Returns empty QString if not found.
    QString currentMonthFutureToken(const QString& underlying) const;

signals:
    void instrumentDataUpdated(const QString &instrumentToken);
    void allInstrumentsDataUpdated();
    void fetchHistoricalDataRequested(const QString &instrumentToken,
                                      const QString &interval,
                                      const QString &fromDate,
                                      const QString &toDate);
    void errorOccurred(const QString& context, const QString& message);

public slots:
    // Input slots
    void onInstrumentsFetched(const QString &filePath);
    void onHistoricalDataReceived(const QString &instrumentToken,
                                  const QString &interval,
                                  const QJsonArray &candles);

    // Actions
    void loadInstrumentsFromFile(const QString &filename);
    void requestHistoricalData(const QString &instrumentToken, const QString &interval);

private:
    explicit DataManager(QObject *parent = nullptr);
    ~DataManager();

    // --- State ---
    static DataManager* m_instance;
    QHash<QString, InstrumentData> m_instruments; // includes indices + filtered NFO
    QMap<QString, QMap<QString, QVector<CandleData>>> m_historicalDataMap; // token -> interval -> candles
    QMap<QString, InstrumentAnalytics> m_instrumentAnalyticsMap;            // token -> analytics

    // --- Helpers: file parse / persist ---
    InstrumentData parseInstrumentCSVLine(const QString &line);
    void saveParsedInstrumentsToFile();

    // --- Storage & analytics ---
    void storeHistoricalData(const QString &instrumentToken,
                             const QString &interval,
                             const QVector<CandleData> &data);

    void calculateDailyAnalytics(const QString &instrumentToken);
    void calculate5MinAnalytics(const QString &instrumentToken);
    void calculatePreviousDayVWAPStats(const QString &instrumentToken);

    // --- Math helpers ---
    double calculateEMA(const QVector<double>& prices, int period) const;
    void   calculateSwingHighLow(const QVector<CandleData>& dailyCandles,
                               int period, double& outHigh, double& outLow) const;
    double calculateMean(const QVector<double>& values) const;
    double calculateStdDev(const QVector<double>& values) const;

    // no copy/move
    DataManager(const DataManager&) = delete;
    DataManager& operator=(const DataManager&) = delete;
    DataManager(DataManager&&) = delete;
    DataManager& operator=(DataManager&&) = delete;
};

#endif // DATAMANAGER_H
