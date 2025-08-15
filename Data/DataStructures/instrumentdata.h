#ifndef INSTRUMENTDATA_H
#define INSTRUMENTDATA_H

#include <QString>
#include <QDate>

// Structure to store instrument data parsed from the CSV file.
struct InstrumentData {
    QString instrumentToken;
    QString exchangeToken;
    QString tradingSymbol;
    QString name;
    double lastPrice;
    QString expiry;        // Expiry date as a string (from CSV)
    QDate expiryDate;      // Parsed expiry date as QDate object
    double strike;
    double tickSize;
    int lotSize;
    QString instrumentType;
    QString segment;
    QString exchange;
};

#endif // INSTRUMENTDATA_H
