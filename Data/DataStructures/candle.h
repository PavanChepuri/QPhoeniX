#ifndef CANDLE_H
#define CANDLE_H

#include <QDateTime>
#include <QtGlobal> // For qlonglong

// Defines a single historical data candle (OHLCV)
struct CandleData {
    QDateTime timestamp; // Timestamp for the start of the candle interval
    double open = 0.0;   // Opening price for the interval
    double high = 0.0;   // Highest price during the interval
    double low = 0.0;    // Lowest price during the interval
    double close = 0.0;  // Closing price for the interval
    qlonglong volume = 0; // *** ADDED: Volume traded during the interval ***
};

// Comparison operators for sorting/uniqueness based on timestamp
inline bool operator<(const CandleData& lhs, const CandleData& rhs) {
    // Handle invalid timestamps if necessary, placing them consistently (e.g., at the end)
    if (!lhs.timestamp.isValid()) return false;
    if (!rhs.timestamp.isValid()) return true;
    return lhs.timestamp < rhs.timestamp;
}
inline bool operator==(const CandleData& lhs, const CandleData& rhs) {
    // Consider candles equal if timestamp is the same
    // Handle invalid timestamps carefully if they can occur after parsing
    if (!lhs.timestamp.isValid() || !rhs.timestamp.isValid()) {
        return (!lhs.timestamp.isValid() && !rhs.timestamp.isValid());
    }
    return lhs.timestamp == rhs.timestamp;
}

#endif // CANDLE_H
