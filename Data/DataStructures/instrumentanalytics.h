#ifndef INSTRUMENTANALYTICS_H
#define INSTRUMENTANALYTICS_H

#include <QDateTime> // Include necessary header for QDateTime
#include <QString>   // Usually good practice if used indirectly

// Holds various calculated analytical values for a specific instrument
struct InstrumentAnalytics {

    // --- Volatility Metrics (from historical daily data) ---
    double avgVolatility = 0.0;        // Average historical volatility (mean of means approach)
    double minPeriodVolatility = 0.0;  // Minimum volatility from calculated periods (e.g., 3,5,..21)
    double maxPeriodVolatility = 0.0;  // Maximum volatility from calculated periods
    bool volatilityCalculated = false; // Status flag

    // --- Daily Range Bands (Based on Previous Day's Close) ---
    double prevDayClose = 0.0;         // The closing price used for calculation
    double rangeUpperBand_PC = 0.0;    // Upper band = Ceil(PrevClose + (PrevClose * AvgVol * GoldenRatio))
    double rangeLowerBand_PC = 0.0;    // Lower band = Floor(PrevClose - (PrevClose * AvgVol * GoldenRatio))
    bool rangeBands_PC_Calculated = false; // Status flag

    // --- Daily Range Bands (Based on Today's Open - Placeholder) ---
    double todayOpen = 0.0;            // Placeholder for today's actual open price
    double rangeUpperBand_TO = 0.0;    // Placeholder for upper band based on TodayOpen
    double rangeLowerBand_TO = 0.0;    // Placeholder for lower band based on TodayOpen
    bool rangeBands_TO_Calculated = false; // Status flag (false until implemented)

    // --- Exponential Moving Averages ---
    double ema21_Daily = 0.0;          // EMA(21) based on daily closing prices
    bool ema21_Daily_Calculated = false; // Status flag
    double ema21_5Min = 0.0;           // EMA(21) based on 5-minute closing prices
    bool ema21_5Min_Calculated = false; // Status flag

    // --- Swing High/Low (Based on Daily Data) ---
    double high_7D = 0.0;              // Highest high over the last 7 daily candles
    double low_7D = 0.0;               // Lowest low over the last 7 daily candles
    bool swing_7D_Calculated = false;  // Status flag
    double high_21D = 0.0;             // Highest high over the last 21 daily candles
    double low_21D = 0.0;              // Lowest low over the last 21 daily candles
    bool swing_21D_Calculated = false; // Status flag

    // *** ADDED for Previous Day VWAP Stats ***
    double prevDayVWAP_High = 0.0;            ///< Highest VWAP value reached during the previous trading day.
    double prevDayVWAP_Low = 0.0;             ///< Lowest VWAP value reached during the previous trading day.
    double prevDayVWAP_Close = 0.0;           ///< VWAP value at the close of the previous trading day.
    bool prevDayVWAP_Stats_Calculated = false; ///< Flag indicating if prev day VWAP H/L/C were calculated.

    // --- Meta Data ---
    QDateTime lastCalculationTime;     // Timestamp when these analytics were last updated
};

#endif // INSTRUMENTANALYTICS_H
