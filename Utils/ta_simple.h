#ifndef TA_SIMPLE_H
#define TA_SIMPLE_H

#include <QVector>
#include <QDateTime>
#include <QtMath>
#include <limits>

namespace TA {

// --- Common helpers ---
inline double NaN() { return std::numeric_limits<double>::quiet_NaN(); }

QVector<double> sma(const QVector<double>& v, int period, int warmup = 0);
QVector<double> ema(const QVector<double>& v, int period, int warmup = 0);
QVector<double> stddev(const QVector<double>& v, int period, int warmup = 0);

struct BBands {
    QVector<double> mid;
    QVector<double> upper;
    QVector<double> lower;
};
BBands bollinger(const QVector<double>& close, int period = 20, double stdevMult = 2.0, int warmup = 0);

struct Stoch {
    QVector<double> k;      // slow %K
    QVector<double> d;      // slow %D
    QVector<double> fastK;  // fast %K (before smoothing)
};
Stoch stochastics(const QVector<double>& high,
                  const QVector<double>& low,
                  const QVector<double>& close,
                  int kPeriod = 14, int kSmoothing = 3, int dPeriod = 3, int warmup = 0);

// Intraday VWAP, resets when ts.date() changes.
QVector<double> vwap(const QVector<double>& high,
                     const QVector<double>& low,
                     const QVector<double>& close,
                     const QVector<double>& volume,
                     const QVector<QDateTime>& ts);

// Pivot point sets (Classic, Fibonacci, Camarilla) from previous day H/L/C
struct Pivots {
    double P;
    double R1, R2, R3, R4, R5;
    double S1, S2, S3, S4, S5;
};
Pivots pivotsClassic(double prevHigh, double prevLow, double prevClose);
Pivots pivotsFibonacci(double prevHigh, double prevLow, double prevClose);
Pivots pivotsCamarilla(double prevHigh, double prevLow, double prevClose);

// Utility: slice a QVector safely
QVector<double> slice(const QVector<double>& v, int start, int end);

} // namespace TA

#endif // TA_SIMPLE_H
