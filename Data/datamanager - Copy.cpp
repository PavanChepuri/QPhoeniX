#include "datamanager.h"
#include "Utils/marketcalendar.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QDate>
#include <QRegularExpression>


// Static member initialization.
DataManager* DataManager::m_instance = nullptr;

// Returns the singleton instance of DataManager.
DataManager* DataManager::instance()
{
    if (!m_instance) {
        m_instance = new DataManager();
    }
    return m_instance;
}

void DataManager::setSampleInstrumentsFilePath(const QString &filepath)
{
    m_sampleInstrumentsFilePath = filepath;
}

// Private constructor for DataManager.
DataManager::DataManager(QObject *parent) : QObject(parent)
{
    // Hardcode the data for NIFTY 50 and NIFTY BANK indices
    InstrumentData nifty50;
    nifty50.instrumentToken = "256265";
    nifty50.exchangeToken = "1001";
    nifty50.tradingSymbol = "NIFTY 50";
    nifty50.name = "NIFTY 50";
    nifty50.segment = "INDICES";
    nifty50.exchange = "NSE";
    nifty50.instrumentType = "EQ";
    nifty50.tickSize = 0.05;
    nifty50.lotSize = 1;
    nifty50.lastPrice = 0.0;
    nifty50.strike = 0.0;
    nifty50.expiry = "";
    m_instruments.insert(nifty50.instrumentToken, nifty50);

    InstrumentData niftyBank;
    niftyBank.instrumentToken = "260105";
    niftyBank.exchangeToken = "1016";
    niftyBank.tradingSymbol = "NIFTY BANK";
    niftyBank.name = "NIFTY BANK";
    niftyBank.segment = "INDICES";
    niftyBank.exchange = "NSE";
    niftyBank.instrumentType = "EQ"; //Equity
    niftyBank.tickSize = 0.05;
    niftyBank.lotSize = 1;     //Default
    niftyBank.lastPrice = 0.0;
    niftyBank.strike = 0.0;
    niftyBank.expiry = "";     //Default
    m_instruments.insert(niftyBank.instrumentToken, niftyBank);
}

// Destructor for DataManager.
DataManager::~DataManager()
{
}

// Retrieves the InstrumentData for a given instrument token.
InstrumentData DataManager::getInstrument(const QString &instrumentToken)
{
    if (m_instruments.contains(instrumentToken)) {
        return m_instruments.value(instrumentToken);
    }

    return InstrumentData(); // Return an empty object if not found
}

// Returns all instruments.
QHash<QString, InstrumentData> DataManager::getAllInstruments()
{
    return m_instruments;
}


// Loads instruments from a CSV file.
void DataManager::loadInstrumentsFromFile(const QString &filename)
{
    qDebug() << "DataManager::loadInstrumentsFromFile called with:" << filename;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Error opening instruments file:" << filename;
        return;
    }

    QTextStream in(&file);
    QString headerLine = in.readLine(); // Read and discard the header line
    QStringList headers = headerLine.split(','); // Assuming comma as delimiter

    // Check if header has expected number of columns
    if (headers.size() < 12) {
        qDebug() << "Invalid CSV header format.";
        file.close();
        return;
    }

    // No need to clear existing instruments as they are already hardcoded

    QDate currentThursday = MarketCalendar::instance()->getThursdayForThisWeek(QDate::currentDate());
    QDate lastThursdayOfMonth = MarketCalendar::instance()->getLastThursdayOfMonth(QDate::currentDate().year(), QDate::currentDate().month());
    qDebug() << "Current Thursday:" << currentThursday;
    qDebug() << "Last Thursday of Month:" << lastThursdayOfMonth;

    while (!in.atEnd()) {
        QString line = in.readLine();
        InstrumentData instrument = parseInstrumentCSVLine(line);

        // Check if this instrument should be added based on the current logic
        if (!instrument.instrumentToken.isEmpty()) {
            // Skip the INDICES. They are already added in the constructor.
            if (instrument.segment == "INDICES")
                continue;

            // For NFO-OPT segment
            if (instrument.segment == "NFO-OPT")
            {
                // NIFTY options: current week's Thursday OR last Thursday of the month
                if (instrument.name == "NIFTY" && (instrument.expiryDate == currentThursday || instrument.expiryDate == lastThursdayOfMonth))
                {
                    qDebug() << "Adding NIFTY Option:" << instrument.tradingSymbol;
                    m_instruments.insert(instrument.instrumentToken, instrument);
                }
                // BANKNIFTY options: Only last Thursday of the month
                else if (instrument.name == "BANKNIFTY" && instrument.expiryDate == lastThursdayOfMonth)
                {
                    qDebug() << "Adding BANKNIFTY Option:" << instrument.tradingSymbol;
                    m_instruments.insert(instrument.instrumentToken, instrument);
                }
            }
            // For NFO-FUT segment
            else if (instrument.segment == "NFO-FUT")
            {
                // Check if the expiry date is the last Thursday of the current month
                // and it is specifically a NIFTY or BANKNIFTY future.
                if ((instrument.name == "NIFTY" || instrument.name == "BANKNIFTY")
                    && instrument.expiryDate == lastThursdayOfMonth)
                {
                    qDebug() << "Adding " << instrument.name << " Future:" << instrument.tradingSymbol;
                    m_instruments.insert(instrument.instrumentToken, instrument);
                }
            }
        }
    }

    file.close();

    // Save the filtered instruments to a new CSV file for debugging
    saveParsedInstrumentsToFile();

    emit allInstrumentsDataUpdated();
}

// Slot to handle the instrumentsFetched signal from KiteConnectAPI.
void DataManager::onInstrumentsFetched(const QString &filePath)
{
    qDebug() << "Instruments fetched signal received with file path:" << filePath;
    if (!filePath.isEmpty()) {
        loadInstrumentsFromFile(filePath);
    }
}

// Parses a single line of CSV data into an InstrumentData object.
InstrumentData DataManager::parseInstrumentCSVLine(const QString &line)
{
    InstrumentData instrument;
    QStringList values = line.split(',');

    if (values.size() >= 12) {
        instrument.instrumentToken = values.at(0).trimmed();
        instrument.exchangeToken = values.at(1).trimmed();
        instrument.tradingSymbol = values.at(2).trimmed();
        instrument.name = values.at(3).trimmed().remove('"'); //Remove extra quotes
        instrument.lastPrice = values.at(4).toDouble();
        instrument.expiry = values.at(5).trimmed();  //Keep expiry as string
        instrument.strike = values.at(6).toDouble();
        instrument.tickSize = values.at(7).toDouble();
        instrument.lotSize = values.at(8).toInt();
        instrument.instrumentType = values.at(9).trimmed();
        instrument.segment = values.at(10).trimmed();
        instrument.exchange = values.at(11).trimmed();

        // Use direct date parsing from the expiry field
        instrument.expiryDate = QDate::fromString(instrument.expiry, "yyyy-MM-dd");
        if (!instrument.expiryDate.isValid()) {
            qDebug() << "Invalid date format in CSV:" << instrument.expiry;
        }
    } else {
        qDebug() << "Invalid CSV line format:" << line;
    }

    return instrument;
}

// Parses a date string from the instrument symbol.  -- NO LONGER USED
QDate DataManager::parseDateFromSymbol(const QString & /*symbol*/)
{
    return QDate();
}

// save the parsed instrument data to a file for debugging
void DataManager::saveParsedInstrumentsToFile()
{
    QString currentDate = QDate::currentDate().toString("yyyyMMdd");
    QString filename = QString("parsed_instruments_%1.csv").arg(currentDate);
    QFile file(filename);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Error opening file for writing:" << filename;
        return;
    }

    QTextStream out(&file);

    // Write headers
    out << "Instrument Token,Exchange Token,Trading Symbol,Name,Last Price,Expiry,Strike,Tick Size,Lot Size,Instrument Type,Segment,Exchange,Expiry Date\n";

    // Write data for each instrument
    for (const InstrumentData &instrument : m_instruments) {
        out << instrument.instrumentToken << ","
            << instrument.exchangeToken << ","
            << instrument.tradingSymbol << ","
            << "\"" << instrument.name << "\"," // Enclose name in double quotes
            << instrument.lastPrice << ","
            << instrument.expiry << ","
            << instrument.strike << ","
            << instrument.tickSize << ","
            << instrument.lotSize << ","
            << instrument.instrumentType << ","
            << instrument.segment << ","
            << instrument.exchange << ","
            << instrument.expiryDate.toString("yyyy-MM-dd") << "\n"; // Format QDate to string
    }

    file.close();
    qDebug() << "Parsed instruments data saved to:" << filename;
}
