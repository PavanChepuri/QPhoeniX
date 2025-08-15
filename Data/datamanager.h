// datamanager.h
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

// *** MODIFIED: Include data structures from separate files ***
#include "Data/DataStructures/instrumentdata.h"       // Assuming path is correct
#include "Data/DataStructures/candle.h"               // <-- Include (was candle.h)
#include "Data/DataStructures/instrumentanalytics.h"  // <-- Include new header
// --- End Modification ---

#include "Utils/marketcalendar.h"                     // Assuming path is correct


class DataManager : public QObject
{
    Q_OBJECT

public:
    // Singleton Access
    static DataManager* instance();

    // Public Data Accessors
    InstrumentData getInstrument(const QString &instrumentToken) const;
    QHash<QString, InstrumentData> getAllInstruments() const;
    // Uses CandleData from included header
    QVector<CandleData> getStoredHistoricalData(const QString &instrumentToken, const QString &interval) const;
    // Uses InstrumentAnalytics from included header
    InstrumentAnalytics getInstrumentAnalytics(const QString &instrumentToken) const;


signals:
    void instrumentDataUpdated(const QString &instrumentToken); // Data for token (any interval) changed
    void allInstrumentsDataUpdated(); // Instrument list loaded/parsed
    void fetchHistoricalDataRequested(const QString &instrumentToken, const QString &interval, const QString &fromDate, const QString &toDate);
    void errorOccurred(const QString& context, const QString& message);


public slots:
    // Data Loading Slots
    void onInstrumentsFetched(const QString &filePath);
    void onHistoricalDataReceived(const QString &instrumentToken, const QString &interval, const QJsonArray &candles);

    // Action Slots
    void loadInstrumentsFromFile(const QString &filename);
    void requestHistoricalData(const QString &instrumentToken, const QString &interval);


private:
    // Private Constructor/Destructor
    explicit DataManager(QObject *parent = nullptr);
    ~DataManager();

    // --- Private Member Variables ---
    static DataManager* m_instance;
    QHash<QString, InstrumentData> m_instruments;
    // Uses CandleData from included header
    QMap<QString, QMap<QString, QVector<CandleData>>> m_historicalDataMap;
    // Uses InstrumentAnalytics from included header
    QMap<QString, InstrumentAnalytics> m_instrumentAnalyticsMap;


    // --- Private Helper Methods ---
    // Instrument Loading
    InstrumentData parseInstrumentCSVLine(const QString &line);
    void saveParsedInstrumentsToFile();
    // Historical Data Storage (Uses CandleData)
    void storeHistoricalData(const QString &instrumentToken, const QString &interval, const QVector<CandleData> &data);

    // Analytics Calculations (Uses CandleData and InstrumentAnalytics)
    void calculateDailyAnalytics(const QString &instrumentToken); // Calculates Vol, Bands(PC), Swings, EMA(Daily)
    void calculate5MinAnalytics(const QString &instrumentToken);  // Calculates EMA(5Min)
    void calculatePreviousDayVWAPStats(const QString &instrumentToken);
    double calculateEMA(const QVector<double>& prices, int period) const;
    void calculateSwingHighLow(const QVector<CandleData>& dailyCandles, int period, double& outHigh, double& outLow) const;
    double calculateMean(const QVector<double>& values) const;   // Declaration reinstated
    double calculateStdDev(const QVector<double>& values) const;  // Declaration reinstated


    // Prevent copying
    DataManager(const DataManager&) = delete;
    DataManager& operator=(const DataManager&) = delete;
    DataManager(DataManager&&) = delete;
    DataManager& operator=(DataManager&&) = delete;
};

#endif // DATAMANAGER_H
