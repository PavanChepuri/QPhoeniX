// datamanager.cpp
#include "datamanager.h"
#include "Utils/marketcalendar.h" // Assuming path is correct

// *** Includes for moved structs ***
#include "Data/DataStructures/candle.h"           // Include definition
#include "Data/DataStructures/instrumentanalytics.h"  // Include definition
// --- End Includes ---

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QStringView>
#include <QtMath>
#include <algorithm>
#include <limits> // Required for numeric_limits
#include <numeric>
#include <QJsonValue> // Ensure QJsonValue is included
#include <QVariant> // Include QVariant for conversion


// Static member initialization.
DataManager* DataManager::m_instance = nullptr;

// --- Internal Helper Function (Static - Internal Linkage) ---
// Calculates standard deviation, used internally by volatility funcs
static double calculateStdDevInternal(const QVector<double>& values) {
    int count = values.size();
    if (count < 2) { return 0.0; }
    double sum = std::accumulate(values.constBegin(), values.constEnd(), 0.0);
    if (count == 0) return 0.0;
    double mean = sum / count;
    double sumSqDev = 0.0;
    for (double val : values) { double deviation = val - mean; sumSqDev += deviation * deviation; }
    if (count < 2) return 0.0; // Should be caught earlier, but safety
    double variance = sumSqDev / (count - 1);
    if (variance < 0) return 0.0;
    double stdDev = qSqrt(variance);
    return qIsNaN(stdDev) ? 0.0 : stdDev;
}

// Calculates volatility (stdev of log returns), used internally
static double calculateLogReturnVolatilityInternal(const QVector<double>& closingPrices) {
    if (closingPrices.size() < 2) { return 0.0; }
    QVector<double> logReturns; logReturns.reserve(closingPrices.size() - 1);
    for (int i = 1; i < closingPrices.size(); ++i) {
        double p_i = closingPrices.at(i); double p_prev = closingPrices.at(i-1);
        if (p_i > std::numeric_limits<double>::epsilon() && p_prev > std::numeric_limits<double>::epsilon()) {
            logReturns.append(qLn(p_i / p_prev));
        } else { qWarning() << "Cannot calculate log return with zero/near-zero price at index" << i; return std::numeric_limits<double>::quiet_NaN(); }
    }
    if (logReturns.isEmpty()) { return 0.0; }
    double stdDev = calculateStdDevInternal(logReturns);
    return qIsNaN(stdDev) ? std::numeric_limits<double>::quiet_NaN() : stdDev;
}

// Calculates historical volatility for a specific lookback, used internally
static double calculateHistoricalVolatility(const QVector<double>& closingPrices, int lookback) {
    if (closingPrices.size() < lookback + 1 || lookback < 1) { return 0.0; }
    QVector<double> recentCloses = closingPrices.sliced(closingPrices.size() - (lookback + 1));
    return calculateLogReturnVolatilityInternal(recentCloses);
}
// --- End Internal Helpers ---


// --- Public Member Function Implementations ---
DataManager* DataManager::instance() { if (!m_instance) { m_instance = new DataManager(); } return m_instance; }

DataManager::DataManager(QObject *parent) : QObject(parent) {
    // Constructor implementation (hardcoding indices)
    InstrumentData nifty50; nifty50.instrumentToken = "256265"; nifty50.exchangeToken = "1001"; nifty50.tradingSymbol = "NIFTY 50"; nifty50.name = "NIFTY 50"; nifty50.segment = "INDICES"; nifty50.exchange = "NSE"; nifty50.instrumentType = "INDEX"; nifty50.tickSize = 0.05; nifty50.lotSize = 1; nifty50.lastPrice = 0.0; nifty50.strike = 0.0; nifty50.expiry = "";
    m_instruments.insert(nifty50.instrumentToken, nifty50);
    InstrumentData niftyBank; niftyBank.instrumentToken = "260105"; niftyBank.exchangeToken = "1016"; niftyBank.tradingSymbol = "NIFTY BANK"; niftyBank.name = "NIFTY BANK"; niftyBank.segment = "INDICES"; niftyBank.exchange = "NSE"; niftyBank.instrumentType = "INDEX"; niftyBank.tickSize = 0.05; niftyBank.lotSize = 1; niftyBank.lastPrice = 0.0; niftyBank.strike = 0.0; niftyBank.expiry = "";
    m_instruments.insert(niftyBank.instrumentToken, niftyBank);
    qInfo() << "DataManager initialized. Added NIFTY 50 and NIFTY BANK indices.";
}

DataManager::~DataManager() { qInfo() << "DataManager destroyed."; }

InstrumentData DataManager::getInstrument(const QString &instrumentToken) const { return m_instruments.value(instrumentToken, InstrumentData()); }

QHash<QString, InstrumentData> DataManager::getAllInstruments() const { return m_instruments; }

void DataManager::loadInstrumentsFromFile(const QString &filename) {
    // Implementation with filtering logic and emit allInstrumentsDataUpdated
    qDebug() << "DataManager::loadInstrumentsFromFile called with:" << filename;
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { qWarning() << "Error opening instruments file:" << filename << "Error:" << file.errorString(); emit errorOccurred("loadInstrumentsFromFile", "Could not open file: " + file.errorString()); return; }
    QTextStream in(&file); in.setEncoding(QStringConverter::Utf8); QString headerLine = in.readLine();
    if(headerLine.isEmpty() || !headerLine.contains("instrument_token")){ qWarning() << "Instruments file has invalid header or is empty:" << filename; file.close(); emit errorOccurred("loadInstrumentsFromFile", "Invalid header or empty file."); return; }
    auto it = m_instruments.begin(); while (it != m_instruments.end()) { if (it.value().segment != "INDICES") { it = m_instruments.erase(it); } else { ++it; } } qDebug() << "Cleared previous non-index instruments.";
    QDate currentThursday = MarketCalendar::instance()->getThursdayForThisWeek(QDate::currentDate()); QDate lastThursdayOfMonth = MarketCalendar::instance()->getLastThursdayOfMonth(QDate::currentDate().year(), QDate::currentDate().month());
    qDebug() << "Filtering options/futures using Current Thursday:" << currentThursday.toString(Qt::ISODate) << "and Last Thursday of Month:" << lastThursdayOfMonth.toString(Qt::ISODate);
    int linesRead = 0; int instrumentsAdded = 0;
    while (!in.atEnd()) {
        QString line = in.readLine(); linesRead++; InstrumentData instrument = parseInstrumentCSVLine(line);
        if (instrument.instrumentToken.isEmpty() || instrument.segment.isEmpty() || instrument.segment == "INDICES") { continue; }
        bool shouldAdd = false; /* Filtering Logic */
        if (instrument.segment == "NFO-OPT") { if (instrument.expiryDate.isValid()){ if (instrument.name == "NIFTY" && (instrument.expiryDate == currentThursday || instrument.expiryDate == lastThursdayOfMonth)) { shouldAdd = true; } else if (instrument.name == "BANKNIFTY" && instrument.expiryDate == lastThursdayOfMonth) { shouldAdd = true; } } }
        else if (instrument.segment == "NFO-FUT" && (instrument.name == "NIFTY" || instrument.name == "BANKNIFTY")) { if (instrument.expiryDate.isValid() && instrument.expiryDate == lastThursdayOfMonth) { shouldAdd = true; } }
        if (shouldAdd) { m_instruments.insert(instrument.instrumentToken, instrument); instrumentsAdded++; }
    }
    file.close(); qInfo() << "Finished loading instruments from" << filename << "- Read" << linesRead << "lines, Added" << instrumentsAdded << "NFO instruments. Total:" << m_instruments.count();
    saveParsedInstrumentsToFile(); qDebug() << "Emitting allInstrumentsDataUpdated signal."; emit allInstrumentsDataUpdated();
}

void DataManager::onInstrumentsFetched(const QString &filePath) {
    qDebug() << "DataManager::onInstrumentsFetched signal received with file path:" << filePath;
    if (!filePath.isEmpty() && QFile::exists(filePath)) { loadInstrumentsFromFile(filePath); }
    else { qWarning() << "DataManager::onInstrumentsFetched received empty or non-existent file path:" << filePath; emit errorOccurred("onInstrumentsFetched", "Received invalid file path for instruments."); }
}

InstrumentData DataManager::parseInstrumentCSVLine(const QString &line) {
    // Implementation uses InstrumentData, no direct use of CandleData/Analytics
    InstrumentData instrument; QStringView lineView(line); QList<QStringView> values = lineView.split(',');
    if (values.size() >= 12) {
        instrument.instrumentToken = values.at(0).trimmed().toString(); instrument.exchangeToken = values.at(1).trimmed().toString(); instrument.tradingSymbol = values.at(2).trimmed().toString();
        QStringView nameView = values.at(3).trimmed(); if (nameView.startsWith('"') && nameView.endsWith('"') && nameView.length() >= 2) { instrument.name = nameView.mid(1, nameView.length() - 2).toString(); } else { instrument.name = nameView.toString(); }
        instrument.lastPrice = values.at(4).toDouble(); instrument.expiry = values.at(5).trimmed().toString(); instrument.strike = values.at(6).toDouble(); instrument.tickSize = values.at(7).toDouble(); instrument.lotSize = values.at(8).toInt(); instrument.instrumentType = values.at(9).trimmed().toString(); instrument.segment = values.at(10).trimmed().toString(); instrument.exchange = values.at(11).trimmed().toString();
        if (!instrument.expiry.isEmpty() && instrument.expiry != "NA" && (instrument.segment == "NFO-FUT" || instrument.segment == "NFO-OPT")) { instrument.expiryDate = QDate::fromString(instrument.expiry, Qt::ISODate); if (!instrument.expiryDate.isValid()) { qWarning() << "Invalid expiry date format:" << instrument.expiry << "for" << instrument.tradingSymbol; } }
    } else if (!line.isEmpty()){ qWarning() << "Invalid CSV line format:" << line; }
    return instrument;
}

void DataManager::saveParsedInstrumentsToFile() {
    QString currentDate = QDate::currentDate().toString("yyyyMMdd"); QString filename = QString("parsed_instruments_%1.csv").arg(currentDate);
    QFile file(filename); if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) { qWarning() << "Error opening file for writing parsed instruments:" << filename << "Error:" << file.errorString(); return; }
    QTextStream out(&file); out.setEncoding(QStringConverter::Utf8); out << "Instrument Token,Exchange Token,Trading Symbol,Name,Last Price,Expiry,Strike,Tick Size,Lot Size,Instrument Type,Segment,Exchange,Expiry Date\n";
    for (const InstrumentData &instrument : m_instruments.values()) { out << instrument.instrumentToken << "," << instrument.exchangeToken << "," << instrument.tradingSymbol << "," << "\"" << instrument.name << "\"," << instrument.lastPrice << "," << instrument.expiry << "," << instrument.strike << "," << instrument.tickSize << "," << instrument.lotSize << "," << instrument.instrumentType << "," << instrument.segment << "," << instrument.exchange << "," << instrument.expiryDate.toString(Qt::ISODate) << "\n"; }
    file.close(); qInfo() << "Filtered instruments data saved to:" << filename;
}

void DataManager::requestHistoricalData(const QString &instrumentToken, const QString &interval) {
    qDebug() << "DataManager::requestHistoricalData - Token:" << instrumentToken << "Interval:" << interval;
    QDateTime now = QDateTime::currentDateTime(); QDate today = now.date(); QDate fromDateCalc;
    const QTime marketOpenTime(9, 15, 0); const QTime marketCloseTime(15, 30, 0);
    if (interval.compare("day", Qt::CaseInsensitive) == 0) { fromDateCalc = today.addDays(-120); qDebug() << "Requesting ~4 months daily data..."; } // Increased lookback
    else if (interval.compare("5minute", Qt::CaseInsensitive) == 0) { fromDateCalc = today.addDays(-7); }
    else { qWarning() << "Invalid interval requested:" << interval; emit errorOccurred("requestHistoricalData", "Invalid interval: " + interval); return; }
    QDateTime fromDateTime(fromDateCalc, marketOpenTime); QDateTime toDateTime(today, marketCloseTime);
    QString fromDateStr = fromDateTime.toString("yyyy-MM-dd+HH:mm:ss"); QString toDateStr = toDateTime.toString("yyyy-MM-dd+HH:mm:ss");
    qDebug() << "Requesting historical data From:" << fromDateStr << "To:" << toDateStr;
    emit fetchHistoricalDataRequested(instrumentToken, interval, fromDateStr, toDateStr);
}

/**
 * @brief Slot to handle incoming raw historical data (now including volume).
 */
void DataManager::onHistoricalDataReceived(const QString &instrumentToken, const QString &interval, const QJsonArray &candles) {
    qDebug() << "DataManager::onHistoricalDataReceived - ENTER - Token:" << instrumentToken << "Interval:" << interval << "Candle Count:" << candles.size();
    if(candles.isEmpty()){ qWarning() << "Received empty candle array for" << instrumentToken << interval; return; }

    QVector<CandleData> candleDataVector;
    candleDataVector.reserve(candles.size());

    for (const QJsonValue &candleValue : candles) {
        if (!candleValue.isArray()) { continue; }
        QJsonArray candle = candleValue.toArray();
        // Need at least 6 elements: timestamp, o, h, l, c, volume
        if (candle.size() < 6) {
            qWarning() << "Skipping candle with insufficient data points (<6) for" << instrumentToken << "Size:" << candle.size();
            continue;
        }

        CandleData data;

        // Parse Timestamp
        data.timestamp = QDateTime::fromString(candle[0].toString(), Qt::ISODateWithMs);
        if (!data.timestamp.isValid()) { data.timestamp = QDateTime::fromString(candle[0].toString(), Qt::ISODate); }
        if (!data.timestamp.isValid()) { qWarning() << "Skipping candle with invalid timestamp:" << candle[0].toString(); continue; }

        // Parse OHLC
        bool ohlcOk = candle[1].isDouble() && candle[2].isDouble() && candle[3].isDouble() && candle[4].isDouble();
        if (!ohlcOk) { qWarning() << "Skipping candle with non-double OHLC at" << data.timestamp.toString(); continue; }
        data.open = candle[1].toDouble();
        data.high = candle[2].toDouble();
        data.low = candle[3].toDouble();
        data.close = candle[4].toDouble();

        // *** CORRECTED: Parse Volume ***
        bool volumeOk = false;
        // Use QVariant::toLongLong() which handles potential string or number representations
        qlonglong volume = candle[5].toVariant().toLongLong(&volumeOk); // Index 5 is volume
        if (!volumeOk) {
            qWarning() << "Skipping candle with invalid volume value:" << candle[5].toVariant() << "at" << data.timestamp.toString();
            continue; // Skip if volume isn't a valid integer/long long
        }
        data.volume = volume;
        // --- End Volume Parsing ---

        candleDataVector.append(data);
    }

    qDebug() << "DataManager::onHistoricalDataReceived - PARSED CANDLES - Count:" << candleDataVector.size();
    if (!candleDataVector.isEmpty()) {
        qDebug() << "DataManager::onHistoricalDataReceived - CALLING storeHistoricalData";
        storeHistoricalData(instrumentToken, interval, candleDataVector);
    } else { /* ... warning ... */ }
    qDebug() << "DataManager::onHistoricalDataReceived - EXIT - Token:" << instrumentToken << "Interval:" << interval;
}

/**
 * @brief Stores new historical data, sorts, removes duplicates, triggers analytics.
 */
void DataManager::storeHistoricalData(const QString &instrumentToken, const QString &interval, const QVector<CandleData> &newData) {
    if (newData.isEmpty()) { return; }
    qDebug() << "DataManager::storeHistoricalData - ENTER - Token:" << instrumentToken << "Interval:" << interval << "New Data size:" << newData.size();

    QVector<CandleData>& targetVector = m_historicalDataMap[instrumentToken][interval];
    targetVector.append(newData);
    qDebug() << "DataManager::storeHistoricalData - Appended data. Current Size:" << targetVector.size();

    // Sort and remove duplicates
    std::sort(targetVector.begin(), targetVector.end());
    auto last = std::unique(targetVector.begin(), targetVector.end());
    targetVector.erase(last, targetVector.end());
    qDebug() << "DataManager::storeHistoricalData - After Sort/Unique - Size:" << targetVector.size();

    // Trigger calculations based on interval
    if (interval.compare("day", Qt::CaseInsensitive) == 0)
    {
        calculateDailyAnalytics(instrumentToken);
    }
    else if (interval.compare("5minute", Qt::CaseInsensitive) == 0)
    {
        calculate5MinAnalytics(instrumentToken);
        // *** MODIFIED: Trigger Previous Day VWAP Stats calculation for Futures ***
        InstrumentData instrument = getInstrument(instrumentToken); // Check segment
        qDebug() << " -> Checking segment for VWAP trigger. Token:" << instrumentToken << "Segment:" << instrument.segment << "Name:" << instrument.name;
        // -> Checking segment for VWAP trigger. Token: "14625282" Segment: "NFO-FUT" Name: "BANKNIFTY"

        if (instrument.segment == "NFO-FUT")
        {
            calculatePreviousDayVWAPStats(instrumentToken); // Call new function
        }
    }

    emit instrumentDataUpdated(instrumentToken);
    qDebug() << "DataManager::storeHistoricalData - EXIT - Token:" << instrumentToken << "Interval:" << interval;
}


// Gets stored historical data. Uses CandleData.
QVector<CandleData> DataManager::getStoredHistoricalData(const QString &instrumentToken, const QString &interval) const {
    return m_historicalDataMap.value(instrumentToken).value(interval, QVector<CandleData>());
}

// Gets computed analytics data. Uses InstrumentAnalytics.
InstrumentAnalytics DataManager::getInstrumentAnalytics(const QString &instrumentToken) const { return m_instrumentAnalyticsMap.value(instrumentToken, InstrumentAnalytics()); }


// --- Private Helper Method Implementations ---

// Calculate Mean
double DataManager::calculateMean(const QVector<double>& values) const {
    if (values.isEmpty()) { return 0.0; }
    double sum = std::accumulate(values.constBegin(), values.constEnd(), 0.0);
    return (values.size() > 0) ? (sum / values.size()) : 0.0;
}

// Calculate StdDev
double DataManager::calculateStdDev(const QVector<double>& values) const {
    int count = values.size();
    if (count < 2) { return 0.0; }
    double mean = this->calculateMean(values);
    double sumSqDev = 0.0;
    for (double val : values) { double deviation = val - mean; sumSqDev += deviation * deviation; }
    double variance = sumSqDev / (count - 1);
    if (variance < 0) return 0.0;
    double stdDev = qSqrt(variance);
    return qIsNaN(stdDev) ? 0.0 : stdDev;
}


// Calculate EMA
double DataManager::calculateEMA(const QVector<double>& prices, int period) const {
    if (period <= 0 || prices.size() < period) { return 0.0; }
    double k = 2.0 / (period + 1.0);
    double ema = 0.0;
    double initialSum = std::accumulate(prices.begin(), prices.begin() + period, 0.0);
    ema = (period > 0) ? (initialSum / period) : 0.0; // Initial SMA
    for (int i = period; i < prices.size(); ++i) { ema = (prices[i] * k) + (ema * (1.0 - k)); }
    return (qIsNaN(ema) || qIsInf(ema)) ? 0.0 : ema; // Return 0 on NaN/Inf
}

// Calculate Swing High/Low
void DataManager::calculateSwingHighLow(const QVector<CandleData>& dailyCandles, int period, double& outHigh, double& outLow) const {
    outHigh = 0.0; outLow = std::numeric_limits<double>::max();
    if (period <= 0 || dailyCandles.isEmpty()) return;
    int startIndex = qMax(0, dailyCandles.size() - period);
    bool validDataFound = false;
    for (int i = startIndex; i < dailyCandles.size(); ++i) {
        const auto& candle = dailyCandles.at(i);
        // Check high >= low > epsilon
        if (candle.high >= candle.low && candle.low > std::numeric_limits<double>::epsilon()) {
            if (!validDataFound) { outHigh = candle.high; outLow = candle.low; validDataFound = true; }
            else { outHigh = qMax(outHigh, candle.high); outLow = qMin(outLow, candle.low); }
        } else { qWarning() << "Skipping invalid H/L candle in Swing:" << candle.timestamp << "H:" << candle.high << "L:" << candle.low; }
    }
    if (!validDataFound) { outHigh = 0.0; outLow = 0.0; } // Reset if no valid data
}

// Calculate all analytics derived from DAILY data
void DataManager::calculateDailyAnalytics(const QString &instrumentToken) {
    qDebug() << "--- ENTERING calculateDailyAnalytics for:" << instrumentToken << "---";
    if (!m_historicalDataMap.contains(instrumentToken) || !m_historicalDataMap.value(instrumentToken).contains("day")) { qWarning() << "No daily historical data for" << instrumentToken; m_instrumentAnalyticsMap.remove(instrumentToken); return; }
    const QVector<CandleData>& dailyCandles = m_historicalDataMap.value(instrumentToken).value("day");
    int numCandles = dailyCandles.size();
    QString instrumentName = getInstrument(instrumentToken).tradingSymbol; if(instrumentName.isEmpty()) instrumentName = instrumentToken; // Use Name for logging
    qDebug() << "Calculating daily analytics for" << instrumentName << "using" << numCandles << "candles.";

    InstrumentAnalytics analytics; // Create new struct
    analytics.lastCalculationTime = QDateTime::currentDateTime();

    const int minCandlesForVol = 22; const int minCandlesForEma = 21;
    const int minCandlesForSwing21 = 21; const int minCandlesForSwing7 = 7;

    if (numCandles < 1) { qWarning() << "No daily candles for" << instrumentName; m_instrumentAnalyticsMap[instrumentToken] = analytics; return; }

    analytics.prevDayClose = dailyCandles.last().close;
    QVector<double> closingPrices; closingPrices.reserve(numCandles); for (const CandleData& c : dailyCandles) { closingPrices.append(c.close); }

    // --- Volatility ---
    if (numCandles >= minCandlesForVol) {
        QList<int> lookbackPeriods = {3, 5, 8, 13, 21}; QVector<double> periodVols; periodVols.reserve(lookbackPeriods.size()); bool volError = false;
        for (int period : lookbackPeriods) { double vol = calculateHistoricalVolatility(closingPrices, period); if (qIsNaN(vol)) { volError = true; break; } periodVols.append(qMax(0.0, vol)); }
        if (!volError && periodVols.size() == lookbackPeriods.size()) {
            analytics.minPeriodVolatility = periodVols.isEmpty() ? 0.0 : *std::min_element(periodVols.constBegin(), periodVols.constEnd());
            analytics.maxPeriodVolatility = periodVols.isEmpty() ? 0.0 : *std::max_element(periodVols.constBegin(), periodVols.constEnd());
            double sMean=calculateMean(periodVols); double pVol=1.0; bool has0=false; for(double v:periodVols){if(qAbs(v)<1e-12){has0=true;pVol=0.0;break;}pVol*=v;} double gMean=has0?0.0:qPow(qAbs(pVol),1.0/periodVols.size()); if(pVol<0&&!has0)gMean=0.0;
            double sRecip=0.0; bool hInv=false; for(double v:periodVols){if(qAbs(v)<1e-12){hInv=true;break;}sRecip+=(1.0/v);} double hMean=(hInv||qAbs(sRecip)<1e-12)?0.0:periodVols.size()/sRecip;
            analytics.avgVolatility = calculateMean({sMean, gMean, hMean}); analytics.volatilityCalculated = true;
        } else { analytics.volatilityCalculated = false; qWarning() << "Volatility calc error for" << instrumentName; }
    } else { analytics.volatilityCalculated = false; qWarning() << "Insuff. data for volatility for" << instrumentName; }

    // --- Range Bands (PrevClose) ---
    if (analytics.volatilityCalculated && analytics.prevDayClose > 0) {
        const double GOLDEN_RATIO = 1.618034; double effVol = analytics.prevDayClose * analytics.avgVolatility; double delta = effVol * GOLDEN_RATIO;
        analytics.rangeUpperBand_PC = qCeil(analytics.prevDayClose + delta); // Ceil for Upper
        analytics.rangeLowerBand_PC = qFloor(analytics.prevDayClose - delta); // Floor for Lower
        analytics.rangeBands_PC_Calculated = true;
    } else { analytics.rangeBands_PC_Calculated = false; }
    analytics.rangeBands_TO_Calculated = false; analytics.todayOpen = 0.0; analytics.rangeUpperBand_TO = 0.0; analytics.rangeLowerBand_TO = 0.0;

    // --- Swing High/Low ---
    if (numCandles >= minCandlesForSwing7) { calculateSwingHighLow(dailyCandles, 7, analytics.high_7D, analytics.low_7D); analytics.swing_7D_Calculated = (analytics.high_7D > 0 || analytics.low_7D < std::numeric_limits<double>::max()); } else { analytics.swing_7D_Calculated = false; }
    if (numCandles >= minCandlesForSwing21) { calculateSwingHighLow(dailyCandles, 21, analytics.high_21D, analytics.low_21D); analytics.swing_21D_Calculated = (analytics.high_21D > 0 || analytics.low_21D < std::numeric_limits<double>::max()); } else { analytics.swing_21D_Calculated = false; }

    // --- Daily EMA(21) ---
    if (numCandles >= minCandlesForEma) { analytics.ema21_Daily = calculateEMA(closingPrices, 21); analytics.ema21_Daily_Calculated = !qIsNaN(analytics.ema21_Daily) && analytics.ema21_Daily != 0.0; }
    else { analytics.ema21_Daily_Calculated = false; qWarning() << "Insuff. data for Daily EMA(21) for" << instrumentName; }

    // --- Store Results & Log ---
    m_instrumentAnalyticsMap[instrumentToken] = analytics; // Store analytics object in map
    qInfo().noquote() << QString("=== Daily Analytics Updated: %1 (%2) ===").arg(instrumentName).arg(instrumentToken);
    if(analytics.volatilityCalculated) qInfo().noquote() << QString("  Volatility (Avg/Min/Max): %1 / %2 / %3").arg(analytics.avgVolatility, 0, 'g', 5).arg(analytics.minPeriodVolatility, 0, 'g', 5).arg(analytics.maxPeriodVolatility, 0, 'g', 5);
    if(analytics.rangeBands_PC_Calculated) qInfo().noquote() << QString("  Range (PrevCl=%1): Lower=%2 Upper=%3").arg(analytics.prevDayClose, 0, 'f', 2).arg(analytics.rangeLowerBand_PC, 0, 'f', 2).arg(analytics.rangeUpperBand_PC, 0, 'f', 2);
    if(analytics.swing_7D_Calculated) qInfo().noquote() << QString("  Swing 7D (L/H): %1 / %2").arg(analytics.low_7D, 0, 'f', 2).arg(analytics.high_7D, 0, 'f', 2);
    if(analytics.swing_21D_Calculated) qInfo().noquote() << QString("  Swing 21D (L/H): %1 / %2").arg(analytics.low_21D, 0, 'f', 2).arg(analytics.high_21D, 0, 'f', 2);
    if(analytics.ema21_Daily_Calculated) qInfo().noquote() << QString("  Daily EMA(21): %1").arg(analytics.ema21_Daily, 0, 'f', 2);
    qInfo() << "==================================================";
}

// Calculate analytics derived from 5MIN data
void DataManager::calculate5MinAnalytics(const QString &instrumentToken) {
    qDebug() << "--- ENTERING calculate5MinAnalytics for:" << instrumentToken << "---";
    if (!m_historicalDataMap.contains(instrumentToken) || !m_historicalDataMap.value(instrumentToken).contains("5minute")) { qWarning() << "No 5-min data for" << instrumentToken; return; }
    const QVector<CandleData>& fiveMinCandles = m_historicalDataMap.value(instrumentToken).value("5minute");
    int numCandles = fiveMinCandles.size();
    QString instrumentName = getInstrument(instrumentToken).tradingSymbol; if(instrumentName.isEmpty()) instrumentName = instrumentToken;
    qDebug() << "Calculating 5min analytics for" << instrumentName << "using" << numCandles << "candles.";

    InstrumentAnalytics analytics = m_instrumentAnalyticsMap.value(instrumentToken); // Get existing or default
    analytics.lastCalculationTime = QDateTime::currentDateTime();
    const int minCandlesForEma = 21;
    if (numCandles >= minCandlesForEma) {
        QVector<double> fiveMinCloses; fiveMinCloses.reserve(numCandles); for(const auto& c : fiveMinCandles) { fiveMinCloses.append(c.close); }
        analytics.ema21_5Min = calculateEMA(fiveMinCloses, 21);
        analytics.ema21_5Min_Calculated = !qIsNaN(analytics.ema21_5Min) && analytics.ema21_5Min != 0.0;
    } else { analytics.ema21_5Min_Calculated = false; qWarning() << "Insuff. data (" << numCandles << "/" << minCandlesForEma << ") for 5min EMA(21) for" << instrumentName; }
    m_instrumentAnalyticsMap[instrumentToken] = analytics; // Store updated struct
    if(analytics.ema21_5Min_Calculated) { qInfo().noquote() << QString(">>> 5-Min Analytics Updated for: %1 (%2) | EMA(21): %3").arg(instrumentName).arg(instrumentToken).arg(analytics.ema21_5Min, 0, 'f', 2); }
    else { qInfo() << ">>> 5-Min Analytics Update attempted for:" << instrumentName << "(" << instrumentToken << ") - EMA Failed"; }
}

// *** ADDED/REVISED: Function to calculate Previous Day's VWAP H/L/C from 5min data ***
/**
 * @brief Calculates VWAP stats (High, Low, Close) for the previous trading day.
 * Uses the stored 5-minute candle data (including volume). Stores results in InstrumentAnalytics.
 * @param instrumentToken The token of the instrument (should be NFO-FUT).
 */
void DataManager::calculatePreviousDayVWAPStats(const QString &instrumentToken) {
    qDebug() << "--- ENTERING calculatePreviousDayVWAPStats for:" << instrumentToken << "---";

    // Get the 5-minute data
    if (!m_historicalDataMap.contains(instrumentToken) || !m_historicalDataMap.value(instrumentToken).contains("5minute")) {
        qWarning() << "No 5-minute historical data found for" << instrumentToken << "to calculate Prev Day VWAP.";
        return;
    }
    const QVector<CandleData>& fiveMinCandles = m_historicalDataMap.value(instrumentToken).value("5minute");
    if (fiveMinCandles.isEmpty()) {
        qWarning() << "5-minute candle vector is empty for" << instrumentToken;
        return;
    }

    // Determine the previous trading day's date
    MarketCalendar* calendar = MarketCalendar::instance();
    if (!calendar) { /* ... handle error ... */ return; }
    QDate today = QDate::currentDate();
    QDate prevTradingDay = calendar->getPreviousTradingDay(today); // Assumes MarketCalendar has this helper
    if (!prevTradingDay.isValid()) { /* ... handle error ... */ return; }
    qDebug() << "Calculating VWAP Stats for previous trading day:" << prevTradingDay.toString(Qt::ISODate);

    // Iterate through candles for the previous day ONLY and calculate stats
    double cumulativePriceVolume = 0.0;
    qlonglong cumulativeVolume = 0;
    double highVWAP = 0.0; // Initialize High VWAP to 0
    double lowVWAP = std::numeric_limits<double>::max(); // Initialize Low VWAP to max double
    double closingVWAP = 0.0; // Will hold the last calculated VWAP
    bool dataFoundForPrevDay = false;
    bool vwapCalculatedAtLeastOnce = false;

    for (const CandleData &candle : fiveMinCandles) {
        // Filter for previous trading day
        if (candle.timestamp.date() != prevTradingDay) {
            // If we were processing the previous day and now passed it, break (assuming sorted data)
            if (dataFoundForPrevDay) break;
            // Otherwise, continue until we find the start of the previous day
            continue;
        }

        // Skip candles with invalid data or zero volume (as they don't contribute to VWAP)
        if (!candle.timestamp.isValid() || candle.high < candle.low || candle.low < 0 || candle.close < 0 || candle.volume <= 0) {
            qWarning() << "Skipping invalid candle in VWAP calculation:" << candle.timestamp << "H:" << candle.high << "L:" << candle.low << "C:" << candle.close << "V:" << candle.volume;
            continue;
        }

        dataFoundForPrevDay = true; // Mark that we found relevant data

        // Calculate Typical Price for this candle
        double typicalPrice = (candle.high + candle.low + candle.close) / 3.0;

        // Update cumulative sums
        cumulativePriceVolume += (typicalPrice * candle.volume);
        cumulativeVolume += candle.volume;

        // Calculate VWAP up to this candle's end
        if (cumulativeVolume > 0) {
            double currentVWAP = cumulativePriceVolume / cumulativeVolume;
            closingVWAP = currentVWAP; // Update closing VWAP with the latest value

            // Update High and Low VWAP
            if (!vwapCalculatedAtLeastOnce) {
                // Initialize high/low with the first valid VWAP
                highVWAP = currentVWAP;
                lowVWAP = currentVWAP;
                vwapCalculatedAtLeastOnce = true;
            } else {
                highVWAP = qMax(highVWAP, currentVWAP);
                lowVWAP = qMin(lowVWAP, currentVWAP);
            }
        }
    }

    // Store the results in InstrumentAnalytics
    InstrumentAnalytics analytics = m_instrumentAnalyticsMap.value(instrumentToken); // Get existing or default

    if (dataFoundForPrevDay && vwapCalculatedAtLeastOnce) {
        analytics.prevDayVWAP_High = (qIsNaN(highVWAP) || qIsInf(highVWAP)) ? 0.0 : highVWAP;
        analytics.prevDayVWAP_Low = (qIsNaN(lowVWAP) || qIsInf(lowVWAP) || lowVWAP == std::numeric_limits<double>::max()) ? 0.0 : lowVWAP; // Handle initial state if no valid calc
        analytics.prevDayVWAP_Close = (qIsNaN(closingVWAP) || qIsInf(closingVWAP)) ? 0.0 : closingVWAP;
        analytics.prevDayVWAP_Stats_Calculated = true;
    } else {
        qWarning() << "No valid 5-min data found for previous trading day" << prevTradingDay.toString(Qt::ISODate) << "or cumulative volume was zero for" << instrumentToken << "to calculate VWAP stats.";
        analytics.prevDayVWAP_High = 0.0;
        analytics.prevDayVWAP_Low = 0.0;
        analytics.prevDayVWAP_Close = 0.0;
        analytics.prevDayVWAP_Stats_Calculated = false;
    }

    analytics.lastCalculationTime = QDateTime::currentDateTime(); // Update timestamp
    m_instrumentAnalyticsMap[instrumentToken] = analytics; // Put updated analytics back in map

    // Log the result
    QString instrumentName = getInstrument(instrumentToken).tradingSymbol; if(instrumentName.isEmpty()) instrumentName = instrumentToken;
    if(analytics.prevDayVWAP_Stats_Calculated) {
        qInfo().noquote() << QString(">>> Prev Day VWAP Stats Calculated for: %1 (%2) | H: %3 L: %4 C: %5")
        .arg(instrumentName)
            .arg(instrumentToken)
            .arg(analytics.prevDayVWAP_High, 0, 'f', 2)
            .arg(analytics.prevDayVWAP_Low, 0, 'f', 2)
            .arg(analytics.prevDayVWAP_Close, 0, 'f', 2);
    } else {
        qInfo() << ">>> Prev Day VWAP Stats Calculation FAILED for:" << instrumentName << "(" << instrumentToken << ")";
    }
    qDebug() << "--- EXITING calculatePreviousDayVWAPStats for:" << instrumentToken << "---";
}
// --- End Added VWAP Calculation ---
