#include "datamanager.h"
#include "Utils/marketcalendar.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QStringView>
#include <QtMath>
#include <algorithm>
#include <limits>
#include <numeric>
#include <QJsonValue>
#include <QVariant>
#include <QSet>
#include <QStringConverter>
#include "Utils/ta_simple.h"

// ---------- static ----------
DataManager* DataManager::m_instance = nullptr;
static double calculateStdDevInternal(const QVector<double>& values) {
    const int n = values.size();
    if (n < 2) return 0.0;
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / n;
    double ssd = 0.0;
    for (double v : values) { const double d = v - mean; ssd += d * d; }
    const double var = ssd / (n - 1);
    const double sd  = var > 0 ? qSqrt(var) : 0.0;
    return qIsNaN(sd) ? 0.0 : sd;
}
static double calculateLogReturnVolatilityInternal(const QVector<double>& closes) {
    if (closes.size() < 2) return 0.0;
    QVector<double> lr; lr.reserve(closes.size() - 1);
    for (int i = 1; i < closes.size(); ++i) {
        const double p = closes[i], pp = closes[i-1];
        if (p <= std::numeric_limits<double>::epsilon() ||
            pp <= std::numeric_limits<double>::epsilon()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        lr.append(qLn(p/pp));
    }
    if (lr.isEmpty()) return 0.0;
    const double sd = calculateStdDevInternal(lr);
    return qIsNaN(sd) ? std::numeric_limits<double>::quiet_NaN() : sd;
}
static double calculateHistoricalVolatility(const QVector<double>& closes, int lookback) {
    if (lookback < 1 || closes.size() < lookback + 1) return 0.0;
    const auto recent = closes.sliced(closes.size() - (lookback + 1));
    return calculateLogReturnVolatilityInternal(recent);
}

// ---------- singleton ----------
DataManager* DataManager::instance() {
    if (!m_instance) m_instance = new DataManager();
    return m_instance;
}

DataManager::DataManager(QObject *parent)
    : QObject(parent)
{
    // Seed the two indices so the UI has them immediately
    InstrumentData nifty50;
    nifty50.instrumentToken = "256265";
    nifty50.exchangeToken   = "1001";
    nifty50.tradingSymbol   = "NIFTY 50";
    nifty50.name            = "NIFTY 50";
    nifty50.segment         = "INDICES";
    nifty50.exchange        = "NSE";
    nifty50.instrumentType  = "INDEX";
    nifty50.tickSize        = 0.05;
    nifty50.lotSize         = 1;
    m_instruments.insert(nifty50.instrumentToken, nifty50);

    InstrumentData banknifty;
    banknifty.instrumentToken = "260105";
    banknifty.exchangeToken   = "1016";
    banknifty.tradingSymbol   = "NIFTY BANK";
    banknifty.name            = "NIFTY BANK";
    banknifty.segment         = "INDICES";
    banknifty.exchange        = "NSE";
    banknifty.instrumentType  = "INDEX";
    banknifty.tickSize        = 0.05;
    banknifty.lotSize         = 1;
    m_instruments.insert(banknifty.instrumentToken, banknifty);

    qInfo() << "DataManager initialized. Added NIFTY 50 and NIFTY BANK indices.";
}

DataManager::~DataManager() {
    qInfo() << "DataManager destroyed.";
}

// ---------- basic accessors ----------
InstrumentData DataManager::getInstrument(const QString &instrumentToken) const {
    return m_instruments.value(instrumentToken, InstrumentData());
}
QHash<QString, InstrumentData> DataManager::getAllInstruments() const {
    return m_instruments;
}
QVector<CandleData> DataManager::getStoredHistoricalData(const QString &instrumentToken,
                                                         const QString &interval) const {
    return m_historicalDataMap.value(instrumentToken).value(interval, {});
}
InstrumentAnalytics DataManager::getInstrumentAnalytics(const QString &instrumentToken) const {
    return m_instrumentAnalyticsMap.value(instrumentToken, InstrumentAnalytics());
}

// ---------- expiry helpers (public, read-only) ----------
QDate DataManager::nearestWeeklyExpiry(const QString& underlying, const QDate& fromDate) const {
    QDate best;
    for (const auto& inst : m_instruments) {
        if (inst.segment != "NFO-OPT") continue;
        if (inst.name.compare(underlying, Qt::CaseInsensitive) != 0) continue;
        if (!inst.expiryDate.isValid() || inst.expiryDate < fromDate) continue;
        if (!best.isValid() || inst.expiryDate < best) best = inst.expiryDate;
    }
    return best; // may be invalid
}

QDate DataManager::monthlyExpiryInSameMonth(const QString& underlying, const QDate& fromDate) const {
    QSet<QDate> expiries;
    for (const auto& inst : m_instruments) {
        if (inst.segment != "NFO-OPT") continue;
        if (inst.name.compare(underlying, Qt::CaseInsensitive) != 0) continue;
        if (!inst.expiryDate.isValid() || inst.expiryDate < fromDate) continue;
        expiries.insert(inst.expiryDate);
    }
    if (expiries.isEmpty()) return QDate();

    const int fromYm = fromDate.year() * 100 + fromDate.month();
    int targetYm = -1;
    for (const auto& d : expiries) {
        const int ym = d.year() * 100 + d.month();
        if (ym < fromYm) continue;
        if (targetYm == -1 || ym < targetYm) targetYm = ym;
    }
    if (targetYm == -1) {
        // nothing this/next month; return latest available overall
        QDate latest;
        for (const auto& d : expiries) if (!latest.isValid() || d > latest) latest = d;
        return latest;
    }

    QDate monthly; // last expiry of that month
    for (const auto& d : expiries) {
        const int ym = d.year() * 100 + d.month();
        if (ym == targetYm && (!monthly.isValid() || d > monthly)) monthly = d;
    }
    return monthly;
}

QVector<InstrumentData> DataManager::optionsForUnderlyingAndExpiry(const QString& underlying,
                                                                   const QDate& expiry) const {
    QVector<InstrumentData> out;
    if (!expiry.isValid()) return out;
    out.reserve(512);
    for (const auto& inst : m_instruments) {
        if (inst.segment != "NFO-OPT") continue;
        if (inst.name.compare(underlying, Qt::CaseInsensitive) != 0) continue;
        if (!inst.expiryDate.isValid() || inst.expiryDate != expiry) continue;
        out.push_back(inst);
    }
    return out;
}

QString DataManager::currentMonthFutureToken(const QString& underlying) const
{
    const QDate today = QDate::currentDate();
    const int y = today.year();
    const int m = today.month();

    QString bestToken;   // empty means "not found yet"
    QDate bestExpiry;    // we’ll keep the earliest valid expiry within the same month

    // Exact-name match to avoid things like "NIFTYNXT50"
    auto exactUnderlying = [&](const InstrumentData& ins) -> bool {
        return ins.name.compare(underlying, Qt::CaseInsensitive) == 0;
    };

    for (const auto& ins : m_instruments) {
        if (ins.segment != "NFO-FUT")             continue;
        if (ins.instrumentType != "FUT")          continue;
        if (!ins.expiryDate.isValid())            continue;
        if (!exactUnderlying(ins))                continue;
        if (ins.expiryDate.year() != y)           continue;
        if (ins.expiryDate.month() != m)          continue;

        // Keep the earliest expiry within the current month (normally the standard monthly)
        if (!bestExpiry.isValid() || ins.expiryDate < bestExpiry) {
            bestExpiry = ins.expiryDate;
            bestToken  = ins.instrumentToken;
        }
    }

    return bestToken; // empty if not found
}

// ---------- instruments load path ----------
void DataManager::loadInstrumentsFromFile(const QString &filename)
{
    qDebug() << "DataManager::loadInstrumentsFromFile:" << filename;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open instruments file:" << filename;
        emit errorOccurred("loadInstrumentsFromFile", "Cannot open " + filename);
        return;
    }

    // 1) Clear previous NFO rows (keep indices)
    {
        int removed = 0;
        for (auto it = m_instruments.begin(); it != m_instruments.end(); ) {
            const auto &inst = it.value();
            if (inst.segment.startsWith("NFO")) { it = m_instruments.erase(it); ++removed; }
            else { ++it; }
        }
        qDebug() << "Cleared previous NFO instruments:" << removed;
    }

    QTextStream in(&file);
    QString header = in.readLine(); Q_UNUSED(header);
    int linesRead = 0, parsed = 0;

    // accept only NIFTY/BANKNIFTY derivatives (avoid NIFTY NEXT 50 etc.)
    auto isNiftyOrBank = [](const InstrumentData &d) -> QString {
        const QString base = d.name.trimmed().toUpper();
        if (base == "NIFTY")     return "NIFTY";
        if (base == "BANKNIFTY") return "BANKNIFTY";
        return {};
    };

    QVector<InstrumentData> candidates; candidates.reserve(200000);

    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.isEmpty()) continue;
        ++linesRead;

        InstrumentData d = parseInstrumentCSVLine(line);
        if (d.segment.isEmpty()) continue;
        if (!d.expiry.isEmpty() && !d.expiryDate.isValid() &&
            (d.segment == "NFO-OPT" || d.segment == "NFO-FUT")) {
            continue; // strict on bad expiry rows
        }

        const bool isDeriv = (d.segment == "NFO-OPT" || d.segment == "NFO-FUT");
        if (isDeriv) {
            const QString u = isNiftyOrBank(d);
            if (!u.isEmpty()) {
                candidates.push_back(d);
                ++parsed;
            }
        } else {
            // keep indices as-is (already present), do nothing here
        }
    }
    file.close();

    qDebug().noquote() << QString("Finished reading %1 lines, parsed %2 NFO candidates.")
                              .arg(linesRead).arg(parsed);

    if (candidates.isEmpty()) {
        qWarning() << "No NIFTY/BANKNIFTY NFO rows found. Aborting.";
        emit allInstrumentsDataUpdated();
        return;
    }

    // 2) Insert candidates temporarily to compute expiries dynamically
    for (const auto &d : candidates) m_instruments.insert(d.instrumentToken, d);

    const QDate today = QDate::currentDate();
    const QDate niftyWeekly  = nearestWeeklyExpiry("NIFTY", today);
    const QDate niftyMonthly = monthlyExpiryInSameMonth("NIFTY", today);
    const QDate bankWeekly   = nearestWeeklyExpiry("BANKNIFTY", today);
    const QDate bankMonthly  = monthlyExpiryInSameMonth("BANKNIFTY", today);

    qInfo().noquote() <<
        QString("Dynamic expiries -> NIFTY [weekly=%1, monthly=%2], "
                "BANKNIFTY [weekly=%3, monthly=%4]")
            .arg(niftyWeekly.toString(Qt::ISODate))
            .arg(niftyMonthly.toString(Qt::ISODate))
            .arg(bankWeekly.toString(Qt::ISODate))
            .arg(bankMonthly.toString(Qt::ISODate));

    // 3) Prune everything else (keep: NIFTY/BANKNIFTY OPT for weekly or monthly, FUT only monthly)
    auto shouldKeep = [&](const InstrumentData &d) -> bool {
        const QString base = isNiftyOrBank(d);
        if (base.isEmpty()) return false;

        if (d.segment == "NFO-OPT") {
            if (!d.expiryDate.isValid()) return false;
            if (base == "NIFTY")     return (d.expiryDate == niftyWeekly) || (d.expiryDate == niftyMonthly);
            if (base == "BANKNIFTY") return (d.expiryDate == bankWeekly)  || (d.expiryDate == bankMonthly);
            return false;
        }
        if (d.segment == "NFO-FUT") {
            if (!d.expiryDate.isValid()) return false;
            if (base == "NIFTY")     return d.expiryDate == niftyMonthly;
            if (base == "BANKNIFTY") return d.expiryDate == bankMonthly;
            return false;
        }
        return false;
    };

    const int before = m_instruments.size();
    for (auto it = m_instruments.begin(); it != m_instruments.end(); ) {
        const InstrumentData &d = it.value();
        if (d.segment.startsWith("NFO") && !shouldKeep(d)) {
            it = m_instruments.erase(it);
        } else {
            ++it;
        }
    }
    const int after = m_instruments.size();
    qInfo().noquote() << QString("Pruned instruments: before=%1, after=%2, removed=%3")
                             .arg(before).arg(after).arg(before - after);

    saveParsedInstrumentsToFile();
    emit allInstrumentsDataUpdated();
}

void DataManager::onInstrumentsFetched(const QString &filePath) {
    qDebug() << "onInstrumentsFetched:" << filePath;
    if (!filePath.isEmpty() && QFile::exists(filePath)) {
        loadInstrumentsFromFile(filePath);
    } else {
        emit errorOccurred("onInstrumentsFetched", "Invalid instruments file path.");
    }
}

// ---------- CSV parsing & persist ----------
InstrumentData DataManager::parseInstrumentCSVLine(const QString &line) {
    InstrumentData d;
    QStringView v(line);
    const auto parts = v.split(',');
    if (parts.size() < 12) {
        if (!line.isEmpty()) qWarning() << "Invalid CSV line:" << line;
        return d;
    }

    d.instrumentToken = parts.at(0).trimmed().toString();
    d.exchangeToken   = parts.at(1).trimmed().toString();
    d.tradingSymbol   = parts.at(2).trimmed().toString();

    // name may be quoted
    {
        QStringView nv = parts.at(3).trimmed();
        if (nv.startsWith('"') && nv.endsWith('"') && nv.size() >= 2)
            d.name = nv.mid(1, nv.size()-2).toString();
        else
            d.name = nv.toString();
    }

    d.lastPrice      = parts.at(4).toDouble();
    d.expiry         = parts.at(5).trimmed().toString();
    d.strike         = parts.at(6).toDouble();
    d.tickSize       = parts.at(7).toDouble();
    d.lotSize        = parts.at(8).toInt();
    d.instrumentType = parts.at(9).trimmed().toString();
    d.segment        = parts.at(10).trimmed().toString();
    d.exchange       = parts.at(11).trimmed().toString();

    if (!d.expiry.isEmpty() && d.expiry != "NA" &&
        (d.segment == "NFO-FUT" || d.segment == "NFO-OPT")) {
        d.expiryDate = QDate::fromString(d.expiry, Qt::ISODate);
        if (!d.expiryDate.isValid()) {
            qWarning() << "Invalid expiry date:" << d.expiry << "for" << d.tradingSymbol;
        }
    }
    return d;
}

void DataManager::saveParsedInstrumentsToFile() {
    const QString stamp = QDate::currentDate().toString("yyyyMMdd");
    const QString file  = QString("parsed_instruments_%1.csv").arg(stamp);

    QFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Cannot write parsed instruments:" << file << f.errorString();
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    out << "Instrument Token,Exchange Token,Trading Symbol,Name,Last Price,Expiry,Strike,"
           "Tick Size,Lot Size,Instrument Type,Segment,Exchange,Expiry Date\n";

    for (const auto &d : m_instruments) {
        out << d.instrumentToken << ','
            << d.exchangeToken   << ','
            << d.tradingSymbol   << ",\""
            << d.name            << "\","
            << d.lastPrice       << ','
            << d.expiry          << ','
            << d.strike          << ','
            << d.tickSize        << ','
            << d.lotSize         << ','
            << d.instrumentType  << ','
            << d.segment         << ','
            << d.exchange        << ','
            << d.expiryDate.toString(Qt::ISODate)
            << '\n';
    }
    f.close();
    qInfo() << "Filtered instruments data saved to:" << file;
}

// ---------- historical data path ----------
void DataManager::requestHistoricalData(const QString &instrumentToken,
                                        const QString &interval)
{
    qDebug() << "requestHistoricalData:" << instrumentToken << interval;

    const QDate today = QDate::currentDate();
    const QTime tOpen(9, 15, 0);
    const QTime tClose(15, 30, 0);

    QDate from;
    if (interval.compare("day", Qt::CaseInsensitive) == 0)
    {
        // from = today.addDays(-120);
        // qDebug() << "Requesting ~4 months daily data...";

        // daily needs ~250 bars → ~400 calendar days
        from = today.addDays(-400);
        qDebug() << "Requesting ~18 months daily data...";

    }
    else if (interval.compare("5minute", Qt::CaseInsensitive) == 0)
    {
        from = today.addDays(-7);
    }
    else
    {
        emit errorOccurred("requestHistoricalData", "Invalid interval: " + interval);
        return;
    }

    const QString fromStr = QDateTime(from,  tOpen ).toString("yyyy-MM-dd+HH:mm:ss");
    const QString toStr   = QDateTime(today, tClose).toString("yyyy-MM-dd+HH:mm:ss");

    qDebug() << "Historical from:" << fromStr << "to:" << toStr;
    emit fetchHistoricalDataRequested(instrumentToken, interval, fromStr, toStr);
}

void DataManager::onHistoricalDataReceived(const QString &instrumentToken,
                                           const QString &interval,
                                           const QJsonArray &candles)
{
    qDebug() << "onHistoricalDataReceived:" << instrumentToken << interval
             << "count:" << candles.size();
    if (candles.isEmpty()) return;

    QVector<CandleData> vec; vec.reserve(candles.size());
    for (const QJsonValue &v : candles) {
        if (!v.isArray()) continue;
        const QJsonArray c = v.toArray();
        if (c.size() < 6) continue; // ts,o,h,l,c,v

        CandleData d;
        d.timestamp = QDateTime::fromString(c[0].toString(), Qt::ISODateWithMs);
        if (!d.timestamp.isValid())
            d.timestamp = QDateTime::fromString(c[0].toString(), Qt::ISODate);
        if (!d.timestamp.isValid()) continue;

        if (!c[1].isDouble() || !c[2].isDouble() || !c[3].isDouble() || !c[4].isDouble())
            continue;

        d.open  = c[1].toDouble();
        d.high  = c[2].toDouble();
        d.low   = c[3].toDouble();
        d.close = c[4].toDouble();

        bool volOk = false;
        d.volume = c[5].toVariant().toLongLong(&volOk);
        if (!volOk) continue;

        vec.append(d);
    }

    if (!vec.isEmpty()) {
        storeHistoricalData(instrumentToken, interval, vec);
    }
}

// ---------- storage & analytics ----------
void DataManager::storeHistoricalData(const QString &instrumentToken,
                                      const QString &interval,
                                      const QVector<CandleData> &newData)
{
    if (newData.isEmpty()) return;

    QVector<CandleData> &dst = m_historicalDataMap[instrumentToken][interval];
    dst += newData;

    // sort by timestamp & dedup on timestamp
    std::sort(dst.begin(), dst.end(),
              [](const CandleData& a, const CandleData& b){ return a.timestamp < b.timestamp; });
    auto last = std::unique(dst.begin(), dst.end(),
                            [](const CandleData& a, const CandleData& b){ return a.timestamp == b.timestamp; });
    dst.erase(last, dst.end());

    if (interval.compare("day", Qt::CaseInsensitive) == 0) {
        calculateDailyAnalytics(instrumentToken);
    } else if (interval.compare("5minute", Qt::CaseInsensitive) == 0) {
        calculate5MinAnalytics(instrumentToken);

        // For futures only, compute previous-day VWAP stats
        const auto inst = getInstrument(instrumentToken);
        if (inst.segment == "NFO-FUT") {
            calculatePreviousDayVWAPStats(instrumentToken);
        }
    }

    emit instrumentDataUpdated(instrumentToken);
}

double DataManager::calculateMean(const QVector<double>& v) const {
    if (v.isEmpty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}
double DataManager::calculateStdDev(const QVector<double>& v) const {
    return calculateStdDevInternal(v);
}
double DataManager::calculateEMA(const QVector<double>& prices, int period) const {
    if (period <= 0 || prices.size() < period) return 0.0;
    const double k = 2.0 / (period + 1.0);
    double ema = std::accumulate(prices.begin(), prices.begin() + period, 0.0) / period;
    for (int i = period; i < prices.size(); ++i)
        ema = prices[i] * k + ema * (1.0 - k);
    return (qIsNaN(ema) || qIsInf(ema)) ? 0.0 : ema;
}
void DataManager::calculateSwingHighLow(const QVector<CandleData>& daily,
                                        int period, double& outHigh, double& outLow) const {
    outHigh = 0.0;
    outLow  = std::numeric_limits<double>::max();
    if (period <= 0 || daily.isEmpty()) return;

    const int start = qMax(0, daily.size() - period);
    bool inited = false;
    for (int i = start; i < daily.size(); ++i) {
        const auto& c = daily[i];
        if (c.low <= 0 || c.high < c.low) continue;
        if (!inited) { outHigh = c.high; outLow = c.low; inited = true; }
        else { outHigh = qMax(outHigh, c.high); outLow = qMin(outLow, c.low); }
    }
    if (!inited) { outHigh = 0.0; outLow = 0.0; }
}

void DataManager::calculateDailyAnalytics(const QString &instrumentToken) {
    if (!m_historicalDataMap.contains(instrumentToken) ||
        !m_historicalDataMap.value(instrumentToken).contains("day")) {
        m_instrumentAnalyticsMap.remove(instrumentToken);
        return;
    }
    const auto& daily = m_historicalDataMap[instrumentToken]["day"];
    const int n = daily.size();
    const QString name = getInstrument(instrumentToken).tradingSymbol;

    InstrumentAnalytics a;
    a.lastCalculationTime = QDateTime::currentDateTime();
    if (n < 1) { m_instrumentAnalyticsMap[instrumentToken] = a; return; }

    a.prevDayClose = daily.last().close;

    QVector<double> closes; closes.reserve(n);
    for (const auto& c : daily) closes.append(c.close);

    if (n >= 22) {
        const QList<int> looks = {3,5,8,13,21};
        QVector<double> vols; vols.reserve(looks.size());
        bool ok = true;
        for (int L : looks) {
            const double v = calculateHistoricalVolatility(closes, L);
            if (qIsNaN(v)) { ok = false; break; }
            vols.append(qMax(0.0, v));
        }
        if (ok && vols.size() == looks.size()) {
            const double s = calculateMean(vols);
            // geometric mean (guard zeros)
            double prod = 1.0; bool has0=false;
            for (double v : vols) { if (qFuzzyIsNull(v)) { has0=true; break; } prod *= v; }
            const double g = has0 ? 0.0 : qPow(prod, 1.0/vols.size());
            // harmonic mean (guard zeros)
            double inv = 0.0; bool bad=false;
            for (double v : vols) { if (qFuzzyIsNull(v)) { bad=true; break; } inv += 1.0/v; }
            const double h = (bad || qFuzzyIsNull(inv)) ? 0.0 : (vols.size()/inv);

            a.avgVolatility = calculateMean({s,g,h});
            a.minPeriodVolatility = *std::min_element(vols.begin(), vols.end());
            a.maxPeriodVolatility = *std::max_element(vols.begin(), vols.end());
            a.volatilityCalculated = true;
        }
    }

    if (a.volatilityCalculated && a.prevDayClose > 0) {
        const double phi  = 1.618034;
        const double eff  = a.prevDayClose * a.avgVolatility;
        const double delt = eff * phi;
        a.rangeUpperBand_PC = qCeil(a.prevDayClose + delt);
        a.rangeLowerBand_PC = qFloor(a.prevDayClose - delt);
        a.rangeBands_PC_Calculated = true;
    }

    double hi7=0, lo7=0, hi21=0, lo21=0;
    if (n >= 7)  { calculateSwingHighLow(daily, 7,  hi7,  lo7);  a.high_7D = hi7;  a.low_7D = lo7;  a.swing_7D_Calculated  = (hi7>0 || lo7>0); }
    if (n >= 21) { calculateSwingHighLow(daily, 21, hi21, lo21); a.high_21D = hi21; a.low_21D = lo21; a.swing_21D_Calculated = (hi21>0||lo21>0); }

    if (n >= 21) {
        a.ema21_Daily = calculateEMA(closes, 21);
        a.ema21_Daily_Calculated = !qIsNaN(a.ema21_Daily) && a.ema21_Daily != 0.0;
    }

    m_instrumentAnalyticsMap[instrumentToken] = a;


    const int warmup = qMax(5*21, 200);
    const int effWarmup = qMin(warmup, closes.size());
    qDebug() << ">>> Daily closes =" << closes.size() << "warmup(eff)=" << effWarmup;
    auto ema21Daily = TA::ema(closes, 21, effWarmup);

    double ema21DailyLast = ema21Daily.isEmpty() ? qQNaN() : ema21Daily.last();
    qDebug() << ">>> Daily Indicators: EMA(21)=" << ema21DailyLast;

    if (!daily.isEmpty()) {
        const auto& pd = daily.last(); // most recent completed daily bar
        const double H = pd.high, L = pd.low, C = pd.close;
        const double range = H - L;


        // --- Classic ---
        const double P  = (H + L + C) / 3.0;
        const double R1 = 2*P - L;
        const double S1 = 2*P - H;
        const double R2 = P + range;
        const double S2 = P - range;
        const double R3 = H + 2*(P - L);
        const double S3 = L - 2*(H - P);

        qDebug() << ">>> Daily Pivots (Classic):"
                 << "P=" << P << "R1=" << R1 << "R2=" << R2 << "R3=" << R3
                 << "S1=" << S1 << "S2=" << S2 << "S3=" << S3;

        // --- Fibonacci (R1..R3 / S1..S3) ---
        const double R1F = P + 0.382*range;
        const double R2F = P + 0.618*range;
        const double R3F = P + 1.000*range;
        const double S1F = P - 0.382*range;
        const double S2F = P - 0.618*range;
        const double S3F = P - 1.000*range;

        qDebug() << ">>> Daily Pivots (Fibo):"
                 << "R1=" << R1F << "R2=" << R2F << "R3=" << R3F
                 << "S1=" << S1F << "S2=" << S2F << "S3=" << S3F;

        // --- Camarilla (H3/H4/L3/L4 core levels; we can extend to H1..H8 later) ---
        const double H3 = C + (range * 1.1 / 3.0);
        const double H4 = C + (range * 1.1 / 2.0);
        const double L3 = C - (range * 1.1 / 3.0);
        const double L4 = C - (range * 1.1 / 2.0);

        qDebug() << ">>> Daily Pivots (Camarilla):"
                 << "H3=" << H3 << "H4=" << H4 << "L3=" << L3 << "L4=" << L4;
    } else {
        qDebug() << ">>> Daily Pivots: insufficient bars";
    }

    // friendly console summary
    qInfo().noquote() << QString("=== Daily Analytics Updated: %1 (%2) ===")
                             .arg(name.isEmpty() ? instrumentToken : name)
                             .arg(instrumentToken);
    if (a.volatilityCalculated)
        qInfo().noquote() << QString("  Volatility (Avg/Min/Max): %1 / %2 / %3")
                                 .arg(a.avgVolatility, 0, 'g', 5)
                                 .arg(a.minPeriodVolatility, 0, 'g', 5)
                                 .arg(a.maxPeriodVolatility, 0, 'g', 5);
    if (a.rangeBands_PC_Calculated)
        qInfo().noquote() << QString("  Range (PrevCl=%1): L=%2 U=%3")
                                 .arg(a.prevDayClose, 0, 'f', 2)
                                 .arg(a.rangeLowerBand_PC, 0, 'f', 2)
                                 .arg(a.rangeUpperBand_PC, 0, 'f', 2);
    if (a.swing_7D_Calculated)
        qInfo().noquote() << QString("  Swing 7D (L/H): %1 / %2")
                                 .arg(a.low_7D, 0, 'f', 2).arg(a.high_7D, 0, 'f', 2);
    if (a.swing_21D_Calculated)
        qInfo().noquote() << QString("  Swing 21D (L/H): %1 / %2")
                                 .arg(a.low_21D, 0, 'f', 2).arg(a.high_21D, 0, 'f', 2);
    if (a.ema21_Daily_Calculated)
        qInfo().noquote() << QString("  Daily EMA(21): %1").arg(a.ema21_Daily, 0, 'f', 2);
    qInfo() << "==================================================";
}

void DataManager::calculate5MinAnalytics(const QString &instrumentToken) {
    if (!m_historicalDataMap.contains(instrumentToken) ||
        !m_historicalDataMap.value(instrumentToken).contains("5minute")) {
        return;
    }
    const auto& five = m_historicalDataMap[instrumentToken]["5minute"];
    const int n = five.size();
    QString name = getInstrument(instrumentToken).tradingSymbol;
    if (name.isEmpty()) name = instrumentToken;

    auto a = m_instrumentAnalyticsMap.value(instrumentToken);
    a.lastCalculationTime = QDateTime::currentDateTime();

    if (n >= 21) {
        QVector<double> closes; closes.reserve(n);
        for (const auto& c : five) closes.append(c.close);
        a.ema21_5Min = calculateEMA(closes, 21);
        a.ema21_5Min_Calculated = !qIsNaN(a.ema21_5Min) && a.ema21_5Min != 0.0;

        // Warmup policy you approved: max(5×period, 200)
        const int warmup = qMax(5*21, 200);
        const int effWarmup = qMin(warmup, closes.size());
        qDebug() << ">>> 5min closes =" << closes.size() << "warmup(eff)=" << effWarmup;
        auto ema21Series = TA::ema(closes, 21, effWarmup);

        double ema21Last = ema21Series.isEmpty() ? qQNaN() : ema21Series.last();
        qDebug() << ">>> 5-Min Indicators: EMA(21)=" << ema21Last;


        const int bbWarmup = qMax(5*21, 200);
        auto bb = TA::bollinger(closes, 21, 2.0, qMin(bbWarmup, closes.size()));
        if (!bb.upper.isEmpty() && !bb.mid.isEmpty() && !bb.lower.isEmpty()) {
            qDebug() << ">>> 5-Min BB(21,2):"
                     << "U=" << bb.upper.last()
                     << "M=" << bb.mid.last()
                     << "L=" << bb.lower.last();
        } else {
            qDebug() << ">>> 5-Min BB(20,2): insufficient bars";
        }

        // Build highs/lows once from the same 5-min candles you used for 'closes'
        QVector<double> highs; highs.reserve(n);
        QVector<double> lows;  lows.reserve(n);
        for (const auto& c : five) { highs.push_back(c.high); lows.push_back(c.low); }

        const int stWarmup = qMax(5*14, 200);
        auto st = TA::stochastics(highs, lows, closes,
                                  /*kPeriod*/14, /*kSmooth*/3, /*dPeriod*/3,
                                  qMin(stWarmup, closes.size()));
        if (!st.k.isEmpty() && !st.d.isEmpty()) {
            qDebug() << ">>> 5-Min Stoch(14,3,3):"
                     << "%K=" << st.k.last()
                     << "%D=" << st.d.last();
        } else {
            qDebug() << ">>> 5-Min Stoch: insufficient bars";
        }

    } else {
        a.ema21_5Min_Calculated = false;
    }

    m_instrumentAnalyticsMap[instrumentToken] = a;

    if (a.ema21_5Min_Calculated) {
        qInfo().noquote() << QString(">>> 5-Min Analytics: %1 (%2) | EMA(21): %3")
        .arg(name).arg(instrumentToken).arg(a.ema21_5Min, 0, 'f', 2);
    }
}

void DataManager::calculatePreviousDayVWAPStats(const QString &instrumentToken) {
    // Need 5-min data
    if (!m_historicalDataMap.contains(instrumentToken) ||
        !m_historicalDataMap.value(instrumentToken).contains("5minute")) {
        return;
    }
    const auto& five = m_historicalDataMap[instrumentToken]["5minute"];
    if (five.isEmpty()) return;

    auto* cal = MarketCalendar::instance();
    if (!cal) return;
    const QDate prevDay = cal->getPreviousTradingDay(QDate::currentDate());
    if (!prevDay.isValid()) return;

    double pv = 0.0;
    qlonglong vol = 0;
    double vwapHigh = 0.0;
    double vwapLow  = std::numeric_limits<double>::max();
    double vwapClose = 0.0;
    bool any = false;

    for (const auto& c : five) {
        if (c.timestamp.date() != prevDay) {
            if (any && c.timestamp.date() > prevDay) break;
            continue;
        }
        if (!c.timestamp.isValid() || c.volume <= 0 ||
            c.high < c.low || c.low < 0 || c.close < 0) continue;

        any = true;
        const double tp = (c.high + c.low + c.close) / 3.0;
        pv  += tp * c.volume;
        vol += c.volume;
        if (vol > 0) {
            const double vwap = pv / vol;
            vwapClose = vwap;
            if (!qIsNaN(vwap) && !qIsInf(vwap)) {
                vwapHigh = qMax(vwapHigh, vwap);
                vwapLow  = qMin(vwapLow,  vwap);
            }
        }
    }

    auto a = m_instrumentAnalyticsMap.value(instrumentToken);
    if (any && vol > 0) {
        a.prevDayVWAP_High  = vwapHigh;
        a.prevDayVWAP_Low   = (vwapLow == std::numeric_limits<double>::max()) ? 0.0 : vwapLow;
        a.prevDayVWAP_Close = vwapClose;
        a.prevDayVWAP_Stats_Calculated = true;

        QString name = getInstrument(instrumentToken).tradingSymbol;
        if (name.isEmpty()) name = instrumentToken;
        qInfo().noquote() << QString(">>> PrevDay VWAP: %1 (%2) | H:%3 L:%4 C:%5")
                                 .arg(name).arg(instrumentToken)
                                 .arg(a.prevDayVWAP_High,  0, 'f', 2)
                                 .arg(a.prevDayVWAP_Low,   0, 'f', 2)
                                 .arg(a.prevDayVWAP_Close, 0, 'f', 2);
    } else {
        a.prevDayVWAP_High = a.prevDayVWAP_Low = a.prevDayVWAP_Close = 0.0;
        a.prevDayVWAP_Stats_Calculated = false;
    }
    a.lastCalculationTime = QDateTime::currentDateTime();
    m_instrumentAnalyticsMap[instrumentToken] = a;
}
